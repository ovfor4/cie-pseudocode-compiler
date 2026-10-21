#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod compiler;
mod runner;
use compiler::Compiler;
use runner::{OutputEvent, Runner};
use tauri::{ipc::Channel, Manager, State};

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
    runner: State<'_, Runner>,
    compiler: State<'_, Compiler>,
) -> Result<(), String> {
    let executable = compiler.inner().0.clone()?;
    let result = runner
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
async fn stop_run(runner: State<'_, Runner>) -> Result<(), String> {
    runner.stop().await;
    Ok(())
}

fn main() {
    let compiler = Compiler::load();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(Runner::default())
        .manage(compiler)
        .invoke_handler(tauri::generate_handler![
            read_file, write_file, run_file, stop_run
        ])
        .build(tauri::generate_context!())
        .expect("Could not start CPSC Bundle")
        .run(|app, event| {
            if matches!(event, tauri::RunEvent::ExitRequested { .. }) {
                tauri::async_runtime::block_on(app.state::<Runner>().stop());
            }
        });
}
