package main

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"text/template"
	"time"
)

type benchCase struct {
	name       string
	iterations int
	render     func() (string, error)
	expected   string
}

type includeData struct {
	Name    string
	Enabled bool
}

func benchmarkCase(b benchCase) (float64, error) {
	samples := make([]float64, 0, 5)
	for run := 0; run < 5; run++ {
		var output string
		start := time.Now()
		for iteration := 0; iteration < b.iterations; iteration++ {
			rendered, err := b.render()
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

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: bench_go <root>")
		os.Exit(1)
	}
	root := os.Args[1]
	includeDir := filepath.Join(root, "tools", "benchmark_compare", "cases", "gotemplate", "include_if")
	cases := []benchCase{
		{
			name:       "simple-variable",
			iterations: 20000,
			render: func() (string, error) {
				tmpl, err := template.New("simple").Parse("Hello {{ .Name }}")
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.Execute(&output, struct{ Name string }{Name: "Ada"})
				return output.String(), err
			},
			expected: "Hello Ada",
		},
		{
			name:       "conditional",
			iterations: 20000,
			render: func() (string, error) {
				tmpl, err := template.New("conditional").Parse("{{ if .Enabled }}Enabled{{ else }}Disabled{{ end }}")
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.Execute(&output, struct{ Enabled bool }{Enabled: true})
				return output.String(), err
			},
			expected: "Enabled",
		},
		{
			name:       "include-if",
			iterations: 5000,
			render: func() (string, error) {
				tmpl, err := template.ParseFiles(filepath.Join(includeDir, "main.txt"), filepath.Join(includeDir, "header.txt"))
				if err != nil {
					return "", err
				}
				var output bytes.Buffer
				err = tmpl.ExecuteTemplate(&output, "main.txt", includeData{Name: "Ada", Enabled: true})
				return output.String(), err
			},
			expected: "Header for Ada\n\nEnabled\nFooter\n",
		},
	}

	for _, bench := range cases {
		micros, err := benchmarkCase(bench)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		fmt.Printf("%s\t%.6f\n", bench.name, micros)
	}
}
