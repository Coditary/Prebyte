package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"sync"
	"text/template"
	"time"
)

type benchCase struct {
	name       string
	iterations int
	renderCold func() (string, error)
	renderWarmExecute func() (string, error)
	expected   string
}

type includeData struct {
	Name    string
	Enabled bool
}

type controlFlowMember struct {
	Name  string
	Admin bool
}

type controlFlowData struct {
	Members  []controlFlowMember
	Archived bool
}

type batchVariable struct {
	Greeting string
	Name     string
}

type manifestCase struct {
	Name       string `json:"name"`
	Iterations int    `json:"iterations"`
}

type manifestGroup struct {
	Cases []manifestCase `json:"cases"`
}

type manifest struct {
	Render manifestGroup `json:"render"`
	Batch  manifestGroup `json:"batch"`
}

func loadManifest(root string) (manifest, error) {
	path := filepath.Join(root, "tools", "benchmark_compare", "manifest.json")
	data, err := os.ReadFile(path)
	if err != nil {
		return manifest{}, err
	}
	var loaded manifest
	if err := json.Unmarshal(data, &loaded); err != nil {
		return manifest{}, err
	}
	return loaded, nil
}

func iterationsFor(cases []manifestCase, name string) int {
	for _, benchCase := range cases {
		if benchCase.Name == name {
			return benchCase.Iterations
		}
	}
	panic(fmt.Sprintf("manifest case not found: %s", name))
}

func benchmarkBatchCase(render func() (string, error), expected string, iterations int, entriesPerIteration int) (float64, error) {
	samples := make([]float64, 0, 5)
	for run := 0; run < 5; run++ {
		var output string
		start := time.Now()
		for iteration := 0; iteration < iterations; iteration++ {
			rendered, err := render()
			if err != nil {
				return 0, err
			}
			output = rendered
		}
		elapsed := time.Since(start)
		if output != expected {
			return 0, fmt.Errorf("unexpected batch output: got=%q expected=%q", output, expected)
		}
		totalRenders := float64(iterations * entriesPerIteration)
		samples = append(samples, float64(elapsed.Nanoseconds())/totalRenders/1000.0)
	}
	sort.Float64s(samples)
	return samples[len(samples)/2], nil
}

func benchmarkThreads() int {
	threads := runtime.NumCPU()
	if threads < 2 {
		return 2
	}
	return threads
}

func benchmarkCase(render func() (string, error), b benchCase) (float64, error) {
	samples := make([]float64, 0, 5)
	for run := 0; run < 5; run++ {
		var output string
		start := time.Now()
		for iteration := 0; iteration < b.iterations; iteration++ {
			rendered, err := render()
			if err != nil {
				return 0, err
			}
			output = rendered
		}
		elapsed := time.Since(start)
		if output != b.expected {
			return 0, fmt.Errorf("unexpected output for %s: got=%q expected=%q", b.name, output, b.expected)
		}
		samples = append(samples, float64(elapsed.Nanoseconds())/float64(b.iterations)/1000.0)
	}
	sort.Float64s(samples)
	return samples[len(samples)/2], nil
}

func benchmarkCaseParallel(render func() (string, error), b benchCase, threads int) (float64, error) {
	samples := make([]float64, 0, 5)
	for run := 0; run < 5; run++ {
		var (
			output string
			outputMu sync.Mutex
			firstErr error
			errMu sync.Mutex
		)
		start := time.Now()
		var wg sync.WaitGroup
		iterationsPerThread := b.iterations / threads
		remainder := b.iterations % threads
		for thread := 0; thread < threads; thread++ {
			iterations := iterationsPerThread
			if thread == 0 {
				iterations += remainder
			}
			wg.Add(1)
			go func() {
				defer wg.Done()
				var localOutput string
				for iteration := 0; iteration < iterations; iteration++ {
					rendered, err := render()
					if err != nil {
						errMu.Lock()
						if firstErr == nil {
							firstErr = err
						}
						errMu.Unlock()
						return
					}
					localOutput = rendered
				}
				outputMu.Lock()
				output = localOutput
				outputMu.Unlock()
			}()
		}
		wg.Wait()
		if firstErr != nil {
			return 0, firstErr
		}
		elapsed := time.Since(start)
		if output != b.expected {
			return 0, fmt.Errorf("unexpected output for %s: got=%q expected=%q", b.name, output, b.expected)
		}
		samples = append(samples, float64(elapsed.Nanoseconds())/float64(b.iterations)/1000.0)
	}
	sort.Float64s(samples)
	return samples[len(samples)/2], nil
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: bench_go <root>")
		os.Exit(1)
	}
	root := os.Args[1]
	manifest, err := loadManifest(root)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	includeDir := filepath.Join(root, "tools", "benchmark_compare", "cases", "gotemplate", "include_if")
	warmSimple, err := template.New("simple").Parse("Hello {{ .Name }}")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	warmConditional, err := template.New("conditional").Parse("{{ if .Enabled }}Enabled{{ else }}Disabled{{ end }}")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	warmControlFlow, err := template.New("control-flow").Parse("{{ if .Members }}{{ range .Members }}{{ if .Admin }}*{{ else }}-{{ end }}{{ .Name }};{{ end }}{{ else if .Archived }}archived{{ else }}empty{{ end }}")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	warmInclude, err := template.ParseFiles(filepath.Join(includeDir, "main.txt"), filepath.Join(includeDir, "header.txt"))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	controlFlowValue := controlFlowData{Members: []controlFlowMember{{Name: "Ada", Admin: true}, {Name: "Grace", Admin: false}}, Archived: false}
	cases := []benchCase{
		{
			name:       "simple-variable",
			iterations: iterationsFor(manifest.Render.Cases, "simple-variable"),
			renderCold: func() (string, error) {
				tmpl, err := template.New("simple").Parse("Hello {{ .Name }}")
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.Execute(&output, struct{ Name string }{Name: "Ada"})
				return output.String(), err
			},
			renderWarmExecute: func() (string, error) {
				var output bytes.Buffer
				err := warmSimple.Execute(&output, struct{ Name string }{Name: "Ada"})
				return output.String(), err
			},
			expected: "Hello Ada",
		},
		{
			name:       "conditional",
			iterations: iterationsFor(manifest.Render.Cases, "conditional"),
			renderCold: func() (string, error) {
				tmpl, err := template.New("conditional").Parse("{{ if .Enabled }}Enabled{{ else }}Disabled{{ end }}")
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.Execute(&output, struct{ Enabled bool }{Enabled: true})
				return output.String(), err
			},
			renderWarmExecute: func() (string, error) {
				var output bytes.Buffer
				err := warmConditional.Execute(&output, struct{ Enabled bool }{Enabled: true})
				return output.String(), err
			},
			expected: "Enabled",
		},
		{
			name:       "include-if",
			iterations: iterationsFor(manifest.Render.Cases, "include-if"),
			renderCold: func() (string, error) {
				tmpl, err := template.ParseFiles(filepath.Join(includeDir, "main.txt"), filepath.Join(includeDir, "header.txt"))
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.ExecuteTemplate(&output, "main.txt", includeData{Name: "Ada", Enabled: true})
				return output.String(), err
			},
			renderWarmExecute: func() (string, error) {
				var output bytes.Buffer
				err := warmInclude.ExecuteTemplate(&output, "main.txt", includeData{Name: "Ada", Enabled: true})
				return output.String(), err
			},
			expected: "Header for Ada\n\nEnabled\nFooter\n",
		},
		{
			name:       "control-flow",
			iterations: iterationsFor(manifest.Render.Cases, "control-flow"),
			renderCold: func() (string, error) {
				tmpl, err := template.New("control-flow").Parse("{{ if .Members }}{{ range .Members }}{{ if .Admin }}*{{ else }}-{{ end }}{{ .Name }};{{ end }}{{ else if .Archived }}archived{{ else }}empty{{ end }}")
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.Execute(&output, controlFlowValue)
				return output.String(), err
			},
			renderWarmExecute: func() (string, error) {
				var output bytes.Buffer
				err := warmControlFlow.Execute(&output, controlFlowValue)
				return output.String(), err
			},
			expected: "*Ada;-Grace;",
		},
	}

	threads := benchmarkThreads()
	for _, bench := range cases {
		micros, err := benchmarkCase(bench.renderCold, bench)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		fmt.Printf("cold:%s\t%.6f\n", bench.name, micros)

		micros, err = benchmarkCase(bench.renderWarmExecute, bench)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		fmt.Printf("warm-execute:%s\t%.6f\n", bench.name, micros)
		fmt.Printf("warm-memoized:%s\t%.6f\n", bench.name, micros)

		micros, err = benchmarkCaseParallel(bench.renderCold, bench, threads)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		fmt.Printf("mt-cold:%s\t%.6f\n", bench.name, micros)

		micros, err = benchmarkCaseParallel(bench.renderWarmExecute, bench, threads)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		fmt.Printf("mt-warm-execute:%s\t%.6f\n", bench.name, micros)
		fmt.Printf("mt-warm-memoized:%s\t%.6f\n", bench.name, micros)
	}

	const batchSource = "{{ .Greeting }} {{ .Name }}!\n"
	batchEntries := []batchVariable{
		{Greeting: "Hello", Name: "Ada"},
		{Greeting: "Hello", Name: "Grace"},
		{Greeting: "Hello", Name: "Linus"},
		{Greeting: "Hello", Name: "Alan"},
		{Greeting: "Hello", Name: "Katherine"},
		{Greeting: "Hello", Name: "Dennis"},
		{Greeting: "Hello", Name: "Margaret"},
		{Greeting: "Hello", Name: "Ken"},
	}
	var batchExpected bytes.Buffer
	for _, entry := range batchEntries {
		fmt.Fprintf(&batchExpected, "%s %s!\n", entry.Greeting, entry.Name)
	}
	warmBatch, err := template.New("batch").Parse(batchSource)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	batchIterations := iterationsFor(manifest.Batch.Cases, "batch-variable")
	renderSequentialWarm := func() (string, error) {
		var output bytes.Buffer
		for _, entry := range batchEntries {
			if err := warmBatch.Execute(&output, entry); err != nil {
				return "", err
			}
		}
		return output.String(), nil
	}
	renderSequentialCold := func() (string, error) {
		var output bytes.Buffer
		for _, entry := range batchEntries {
			tmpl, err := template.New("batch").Parse(batchSource)
			if err != nil {
				return "", err
			}
			if err := tmpl.Execute(&output, entry); err != nil {
				return "", err
			}
		}
		return output.String(), nil
	}

	micros, err := benchmarkBatchCase(renderSequentialWarm, batchExpected.String(), batchIterations, len(batchEntries))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("sequential-warm:batch-variable\t%.6f\n", micros)

	micros, err = benchmarkBatchCase(renderSequentialCold, batchExpected.String(), batchIterations, len(batchEntries))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("sequential-cold:batch-variable\t%.6f\n", micros)
}
