use askama::Template;
use serde::Deserialize;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

#[derive(Deserialize)]
struct ManifestCase {
    name: String,
    iterations: usize,
}

#[derive(Deserialize)]
struct ManifestGroup {
    cases: Vec<ManifestCase>,
}

#[derive(Deserialize)]
struct Manifest {
    render: ManifestGroup,
    batch: ManifestGroup,
}

fn load_manifest(root: &str) -> Result<Manifest, String> {
    let path = PathBuf::from(root)
        .join("tools")
        .join("benchmark_compare")
        .join("manifest.json");
    let data = fs::read_to_string(path).map_err(|error| error.to_string())?;
    serde_json::from_str(&data).map_err(|error| error.to_string())
}

fn iterations_for(cases: &[ManifestCase], name: &str) -> usize {
    cases
        .iter()
        .find(|case| case.name == name)
        .map(|case| case.iterations)
        .unwrap_or_else(|| panic!("manifest case not found: {name}"))
}

fn benchmark_threads() -> usize {
    thread::available_parallelism()
        .map(|count| count.get())
        .unwrap_or(2)
        .max(2)
}

fn median(mut samples: Vec<f64>) -> f64 {
    samples.sort_by(|left, right| left.partial_cmp(right).unwrap());
    samples[samples.len() / 2]
}

fn benchmark_case<F>(
    name: &str,
    iterations: usize,
    expected: &str,
    render: &F,
) -> Result<f64, String>
where
    F: Fn() -> Result<String, askama::Error>,
{
    let mut samples = Vec::with_capacity(5);
    for _ in 0..5 {
        let mut output = String::new();
        let start = Instant::now();
        for _ in 0..iterations {
            output = render().map_err(|error| error.to_string())?;
        }
        let elapsed = start.elapsed();
        if output != expected {
            return Err(format!(
                "unexpected output for {name}: got={output:?} expected={expected:?}"
            ));
        }
        samples.push(elapsed.as_nanos() as f64 / iterations as f64 / 1000.0);
    }
    Ok(median(samples))
}

fn benchmark_case_parallel<F>(
    name: &str,
    iterations: usize,
    expected: &str,
    threads: usize,
    render: &F,
) -> Result<f64, String>
where
    F: Fn() -> Result<String, askama::Error> + Sync,
{
    let mut samples = Vec::with_capacity(5);
    for _ in 0..5 {
        let output = Arc::new(Mutex::new(String::new()));
        let error = Arc::new(Mutex::new(None::<String>));
        let start = Instant::now();
        thread::scope(|scope| {
            let iterations_per_thread = iterations / threads;
            let remainder = iterations % threads;
            for thread_index in 0..threads {
                let mut thread_iterations = iterations_per_thread;
                if thread_index == 0 {
                    thread_iterations += remainder;
                }
                let output = Arc::clone(&output);
                let error = Arc::clone(&error);
                scope.spawn(move || {
                    let mut local_output = String::new();
                    for _ in 0..thread_iterations {
                        match render() {
                            Ok(rendered) => local_output = rendered,
                            Err(render_error) => {
                                let mut guard = error.lock().unwrap();
                                if guard.is_none() {
                                    *guard = Some(render_error.to_string());
                                }
                                return;
                            }
                        }
                    }
                    let mut guard = output.lock().unwrap();
                    *guard = local_output;
                });
            }
        });
        if let Some(message) = error.lock().unwrap().take() {
            return Err(message);
        }
        let elapsed = start.elapsed();
        let final_output = output.lock().unwrap().clone();
        if final_output != expected {
            return Err(format!(
                "unexpected output for {name}: got={final_output:?} expected={expected:?}"
            ));
        }
        samples.push(elapsed.as_nanos() as f64 / iterations as f64 / 1000.0);
    }
    Ok(median(samples))
}

fn benchmark_batch_case<F>(
    name: &str,
    mode: &str,
    iterations: usize,
    entries_per_iteration: usize,
    expected: &str,
    render: &F,
) -> Result<(), String>
where
    F: Fn() -> Result<String, askama::Error>,
{
    let mut samples = Vec::with_capacity(5);
    for _ in 0..5 {
        let mut output = String::new();
        let start = Instant::now();
        for _ in 0..iterations {
            output = render().map_err(|error| error.to_string())?;
        }
        let elapsed = start.elapsed();
        if output != expected {
            return Err(format!(
                "unexpected batch output for {name}: got={output:?} expected={expected:?}"
            ));
        }
        let total_renders = iterations as f64 * entries_per_iteration as f64;
        samples.push(elapsed.as_nanos() as f64 / total_renders / 1000.0);
    }
    let micros = median(samples);
    println!("{mode}:{name}\t{micros:.6}");
    Ok(())
}

fn run_case<C, W>(
    name: &str,
    iterations: usize,
    expected: &str,
    render_cold: &C,
    render_warm: &W,
) -> Result<(), String>
where
    C: Fn() -> Result<String, askama::Error> + Sync,
    W: Fn() -> Result<String, askama::Error> + Sync,
{
    let micros = benchmark_case(name, iterations, expected, render_cold)?;
    println!("cold:{name}\t{micros:.6}");

    let micros = benchmark_case(name, iterations, expected, render_warm)?;
    println!("warm-execute:{name}\t{micros:.6}");
    println!("warm-memoized:{name}\t{micros:.6}");

    let threads = benchmark_threads();
    let micros = benchmark_case_parallel(name, iterations, expected, threads, render_cold)?;
    println!("mt-cold:{name}\t{micros:.6}");

    let micros = benchmark_case_parallel(name, iterations, expected, threads, render_warm)?;
    println!("mt-warm-execute:{name}\t{micros:.6}");
    println!("mt-warm-memoized:{name}\t{micros:.6}");
    Ok(())
}

#[derive(Template)]
#[template(source = "Hello {{ name }}", ext = "txt")]
struct SimpleTemplate<'a> {
    name: &'a str,
}

#[derive(Template)]
#[template(
    source = "{% if enabled %}Enabled{% else %}Disabled{% endif %}",
    ext = "txt"
)]
struct ConditionalTemplate {
    enabled: bool,
}

#[derive(Template)]
#[template(path = "include_if/main.txt")]
struct IncludeIfTemplate {
    name: String,
    enabled: bool,
}

#[derive(Clone)]
struct ControlFlowMember {
    name: String,
    admin: bool,
}

#[derive(Template)]
#[template(path = "control_flow.txt")]
struct ControlFlowTemplate {
    members: Vec<ControlFlowMember>,
    is_archived: bool,
}

#[derive(Template)]
#[template(source = "{{ greeting }} {{ name }}!\n", ext = "txt")]
struct BatchTemplate<'a> {
    greeting: &'a str,
    name: &'a str,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: bench_askama <root>");
        std::process::exit(1);
    }
    let root = &args[1];
    let manifest = match load_manifest(root) {
        Ok(manifest) => manifest,
        Err(error) => {
            eprintln!("{error}");
            std::process::exit(1);
        }
    };

    let control_flow_members = vec![
        ControlFlowMember {
            name: "Ada".to_string(),
            admin: true,
        },
        ControlFlowMember {
            name: "Grace".to_string(),
            admin: false,
        },
    ];

    let warm_simple = SimpleTemplate { name: "Ada" };
    if let Err(error) = run_case(
        "simple-variable",
        iterations_for(&manifest.render.cases, "simple-variable"),
        "Hello Ada",
        &|| SimpleTemplate { name: "Ada" }.render(),
        &|| warm_simple.render(),
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }

    let warm_conditional = ConditionalTemplate { enabled: true };
    if let Err(error) = run_case(
        "conditional",
        iterations_for(&manifest.render.cases, "conditional"),
        "Enabled",
        &|| ConditionalTemplate { enabled: true }.render(),
        &|| warm_conditional.render(),
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }

    let warm_include = IncludeIfTemplate {
        name: "Ada".to_string(),
        enabled: true,
    };
    if let Err(error) = run_case(
        "include-if",
        iterations_for(&manifest.render.cases, "include-if"),
        "Header for Ada\n\nEnabled\nFooter\n",
        &|| {
            IncludeIfTemplate {
                name: "Ada".to_string(),
                enabled: true,
            }
            .render()
        },
        &|| warm_include.render(),
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }

    let warm_control_flow = ControlFlowTemplate {
        members: control_flow_members.clone(),
        is_archived: false,
    };
    if let Err(error) = run_case(
        "control-flow",
        iterations_for(&manifest.render.cases, "control-flow"),
        "*Ada;-Grace;",
        &|| {
            ControlFlowTemplate {
                members: control_flow_members.clone(),
                is_archived: false,
            }
            .render()
        },
        &|| warm_control_flow.render(),
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }

    let batch_entries = [
        ("Hello", "Ada"),
        ("Hello", "Grace"),
        ("Hello", "Linus"),
        ("Hello", "Alan"),
        ("Hello", "Katherine"),
        ("Hello", "Dennis"),
        ("Hello", "Margaret"),
        ("Hello", "Ken"),
    ];
    let batch_expected = batch_entries
        .iter()
        .map(|(greeting, name)| format!("{greeting} {name}!\n"))
        .collect::<String>();
    let batch_iterations = iterations_for(&manifest.batch.cases, "batch-variable");

    if let Err(error) = benchmark_batch_case(
        "batch-variable",
        "sequential-warm",
        batch_iterations,
        batch_entries.len(),
        &batch_expected,
        &|| {
            let mut output = String::new();
            for (greeting, name) in batch_entries {
                output.push_str(&BatchTemplate { greeting, name }.render()?);
            }
            Ok(output)
        },
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }

    if let Err(error) = benchmark_batch_case(
        "batch-variable",
        "sequential-cold",
        batch_iterations,
        batch_entries.len(),
        &batch_expected,
        &|| {
            let mut output = String::new();
            for (greeting, name) in batch_entries {
                output.push_str(&BatchTemplate { greeting, name }.render()?);
            }
            Ok(output)
        },
    ) {
        eprintln!("{error}");
        std::process::exit(1);
    }
}
