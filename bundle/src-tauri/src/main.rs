#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod compiler;
mod menu;
mod runner;
use compiler::Compiler;
use runner::{OutputEvent, Runner};
use std::{
    collections::HashMap,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Mutex,
    },
};
use tauri::{
    ipc::Channel, AppHandle, Emitter, Manager, State, WebviewWindow, WebviewWindowBuilder,
};

// Each window runs its own program, keyed by window label.
#[derive(Default)]
struct Runners(Mutex<HashMap<String, Runner>>);

impl Runners {
    fn get(&self, label: &str) -> Runner {
        self.0
            .lock()
            .unwrap()
            .entry(label.into())
            .or_default()
            .clone()
    }
}

#[tauri::command]
fn read_file(path: String) -> Result<String, String> {
    std::fs::read_to_string(path).map_err(|error| error.to_string())
}

#[tauri::command]
fn write_file(path: String, text: String) -> Result<(), String> {
    std::fs::write(path, text).map_err(|error| error.to_string())
}

#[tauri::command]
async fn run_file(
    path: String,
    input: String,
    output: Channel<OutputEvent>,
    window: WebviewWindow,
    runners: State<'_, Runners>,
    compiler: State<'_, Compiler>,
) -> Result<(), String> {
    let executable = compiler.inner().0.clone()?;
    let result = runners
        .get(window.label())
        .run(executable, path.into(), input, |stream, bytes| {
            let _ = output.send(OutputEvent::Data {
                stream: stream.into(),
                bytes,
            });
        })
        .await?;
    output
        .send(OutputEvent::Finished {
            code: result.code,
            stopped: result.stopped,
        })
        .map_err(|error| error.to_string())
}

#[tauri::command]
async fn stop_run(window: WebviewWindow, runners: State<'_, Runners>) -> Result<(), String> {
    runners.get(window.label()).stop().await;
    Ok(())
}

// Async because creating a window from a synchronous command deadlocks on Windows.
#[tauri::command]
async fn new_window(app: AppHandle) -> Result<(), String> {
    static NEXT: AtomicUsize = AtomicUsize::new(1);
    let mut config = app.config().app.windows[0].clone();
    config.label = format!("window-{}", NEXT.fetch_add(1, Ordering::Relaxed));
    WebviewWindowBuilder::from_config(&app, &config)
        .and_then(|builder| builder.build())
        .map(drop)
        .map_err(|error| error.to_string())
}

fn main() {
    let compiler = Compiler::load();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .menu(menu::build)
        // Menu items act on the focused window, like its keyboard shortcuts.
        .on_menu_event(|app, event| {
            let mut windows = app.webview_windows().into_values();
            if event.id() == "quit" {
                // Each window runs its unsaved-changes prompt; the app exits after the last.
                windows.for_each(|window| drop(window.close()));
            } else if let Some(window) = windows.find(|window| window.is_focused().unwrap_or(false))
            {
                let _ = window.emit_to(window.label(), "menu", event.id().as_ref());
            }
        })
        .manage(Runners::default())
        .manage(compiler)
        .invoke_handler(tauri::generate_handler![
            read_file, write_file, run_file, stop_run, new_window
        ])
        .build(tauri::generate_context!())
        .expect("Could not start CPSC Bundle")
        .run(|app, event| match event {
            // Also reached when macOS terminates without closing windows, such as Dock Quit.
            tauri::RunEvent::Exit => {
                let runners: Vec<Runner> = app
                    .state::<Runners>()
                    .0
                    .lock()
                    .unwrap()
                    .values()
                    .cloned()
                    .collect();
                tauri::async_runtime::block_on(async {
                    for runner in runners {
                        runner.stop().await;
                    }
                });
            }
            tauri::RunEvent::WindowEvent {
                label,
                event: tauri::WindowEvent::Destroyed,
                ..
            } => {
                let runner = app.state::<Runners>().0.lock().unwrap().remove(&label);
                if let Some(runner) = runner {
                    tauri::async_runtime::block_on(runner.stop());
                }
            }
            _ => {}
        });
}
