use serde::Serialize;
use std::{
    path::PathBuf,
    process::Stdio,
    sync::{Arc, Mutex},
};
use tokio::{
    io::{AsyncRead, AsyncReadExt, AsyncWriteExt},
    process::Command,
    sync::watch,
};

#[derive(Clone, Default)]
pub struct Runner(Arc<Mutex<Option<Control>>>);

struct Control {
    stop: watch::Sender<bool>,
    done: watch::Receiver<bool>,
}

#[derive(Serialize)]
#[serde(tag = "event", rename_all = "camelCase")]
pub enum OutputEvent {
    Data { stream: String, bytes: Vec<u8> },
    Finished { code: Option<i32>, stopped: bool },
}

pub struct RunResult {
    pub code: Option<i32>,
    pub stopped: bool,
}

impl Runner {
    pub async fn run(
        &self,
        executable: PathBuf,
        source: PathBuf,
        input: String,
        output: impl Fn(&str, Vec<u8>) + Send + Sync,
    ) -> Result<RunResult, String> {
        let (stop, stop_rx) = watch::channel(false);
        let (done_tx, done) = watch::channel(false);
        {
            let mut active = self.0.lock().unwrap();
            if active.is_some() {
                return Err("A program is already running.".into());
            }
            *active = Some(Control { stop, done });
        }
        let result = run_process(executable, source, input, stop_rx, output).await;
        self.0.lock().unwrap().take();
        let _ = done_tx.send(true);
        result
    }

    pub async fn stop(&self) {
        let control = self
            .0
            .lock()
            .unwrap()
            .as_ref()
            .map(|run| (run.stop.clone(), run.done.clone()));
        if let Some((stop, mut done)) = control {
            let _ = stop.send(true);
            while !*done.borrow_and_update() {
                if done.changed().await.is_err() {
                    break;
                }
            }
        }
    }
}

async fn read_output(
    mut pipe: impl AsyncRead + Unpin,
    stream: &str,
    output: &(impl Fn(&str, Vec<u8>) + Send + Sync),
) -> Result<(), String> {
    let mut buffer = [0; 8192];
    loop {
        let count = pipe
            .read(&mut buffer)
            .await
            .map_err(|error| error.to_string())?;
        if count == 0 {
            return Ok(());
        }
        output(stream, buffer[..count].to_vec());
    }
}

async fn run_process(
    executable: PathBuf,
    source: PathBuf,
    input: String,
    mut stop: watch::Receiver<bool>,
    output: impl Fn(&str, Vec<u8>) + Send + Sync,
) -> Result<RunResult, String> {
    let mut command = Command::new(executable);
    command
        .arg(&source)
        .arg("-r")
        // Output renders stderr colors; auto would only see a pipe.
        .arg("-fdiagnostics-color=always")
        .current_dir(source.parent().ok_or("Invalid source path.")?)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .kill_on_drop(true);
    #[cfg(windows)]
    command.creation_flags(0x08000000); // CREATE_NO_WINDOW
    let mut child = command
        .spawn()
        .map_err(|error| format!("Cannot start compiler: {error}"))?;
    let mut stdin = child.stdin.take().unwrap();
    let stdout = child.stdout.take().unwrap();
    let stderr = child.stderr.take().unwrap();
    let write_input = async move {
        let result = stdin.write_all(input.as_bytes()).await;
        drop(stdin); // EOF must reach programs even when Input is empty.
        match result {
            Err(error) if error.kind() != std::io::ErrorKind::BrokenPipe => Err(error.to_string()),
            _ => Ok(()), // Programs may finish without consuming all input.
        }
    };
    let wait = async {
        tokio::select! {
            status = child.wait() => status.map(|status| RunResult { code: status.code(), stopped: false }),
            _ = stop.changed() => {
                child.kill().await.map_err(|error| error.to_string())?;
                child.wait().await.map(|status| RunResult { code: status.code(), stopped: true })
            }
        }.map_err(|error| error.to_string())
    };
    let (result, input_result, stdout_result, stderr_result) = tokio::join!(
        wait,
        write_input,
        read_output(stdout, "stdout", &output),
        read_output(stderr, "stderr", &output)
    );
    let result = result?;
    input_result?;
    stdout_result?;
    stderr_result?;
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use tokio::time::{timeout, Duration};

    struct Source(PathBuf);
    impl Source {
        fn new(text: &str) -> Self {
            static NEXT: AtomicUsize = AtomicUsize::new(0);
            let directory = std::env::temp_dir().join(format!(
                "cpsc bundle test {} {}",
                std::process::id(),
                NEXT.fetch_add(1, Ordering::Relaxed)
            ));
            std::fs::create_dir_all(&directory).unwrap();
            let source = directory.join("program.cps");
            std::fs::write(&source, text).unwrap();
            Self(source)
        }
    }
    impl Drop for Source {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(self.0.parent().unwrap());
        }
    }
    fn compiler() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(if cfg!(windows) {
            "../../build/cpsc.exe"
        } else {
            "../../build/cpsc"
        })
    }

    #[tokio::test]
    async fn prefilled_input_empty_eof_and_diagnostics() {
        let runner = Runner::default();
        for (program, input, expected, code) in [
            (
                "PP_DEFINE Enabled\nPP_IFDEF Enabled\nOUTPUT 1\nPP_ENDIF\nPP_UNDEF Enabled\nPP_IFNDEF Enabled\nOUTPUT 2\nPP_ENDIF\n",
                "",
                "1\n2\n",
                0,
            ),
            (
                "DECLARE N : INTEGER\nINPUT N\nOUTPUT N\nOUTPUT \"\\U0001F600\"\n",
                "42\n",
                "42\n😀\n",
                0,
            ),
            (
                "DECLARE S : STRING\nINPUT S\nOUTPUT \"done\"\n",
                "",
                "INPUT reached the end of input",
                1,
            ),
            (
                "DECLARE S : STRING\nINPUT S\nOUTPUT \"done\"\n",
                "\n",
                "done\n",
                0,
            ),
            (
                "OUTPUT Missing\n",
                "",
                "\u{1b}[01;31m\u{1b}[Kerror: \u{1b}[m\u{1b}[K",
                1,
            ),
            (
                "OUTPUT 1 DIV 0\n",
                "",
                "Unhandled exception. Program terminated.",
                1,
            ),
        ] {
            let source = Source::new(program);
            let bytes = Mutex::new(Vec::new());
            let result = timeout(
                Duration::from_secs(30),
                runner.run(compiler(), source.0.clone(), input.into(), |_, chunk| {
                    bytes.lock().unwrap().extend(chunk)
                }),
            )
            .await
            .unwrap()
            .unwrap();
            assert_eq!(result.code, Some(code));
            let text = String::from_utf8(bytes.into_inner().unwrap())
                .unwrap()
                .replace("\r\n", "\n");
            if code == 0 {
                assert_eq!(text, expected);
            } else {
                assert!(text.contains(expected), "{text}");
            }
        }
    }

    #[tokio::test]
    async fn output_arrives_before_exit_and_stop_allows_restart() {
        let runner = Runner::default();
        let source = Source::new("OUTPUT \"ready\"\nWHILE TRUE\nENDWHILE\n");
        let (send, mut receive) = tokio::sync::mpsc::unbounded_channel();
        let running = runner.clone();
        let path = source.0.clone();
        let task = tokio::spawn(async move {
            running
                .run(compiler(), path, String::new(), |_, bytes| {
                    let _ = send.send(bytes);
                })
                .await
        });
        let mut output = Vec::new();
        timeout(Duration::from_secs(30), async {
            while !output.ends_with(b"ready\n") && !output.ends_with(b"ready\r\n") {
                output.extend(receive.recv().await.expect("output closed before ready"));
            }
        })
        .await
        .expect("stdout was buffered until exit");
        assert!(runner
            .run(compiler(), source.0.clone(), String::new(), |_, _| {})
            .await
            .is_err());
        timeout(Duration::from_secs(5), runner.stop())
            .await
            .expect("stop hung");
        assert!(task.await.unwrap().unwrap().stopped);
        let source = Source::new("OUTPUT \"again\"\n");
        assert_eq!(
            runner
                .run(compiler(), source.0.clone(), String::new(), |_, _| {})
                .await
                .unwrap()
                .code,
            Some(0)
        );
    }
}
