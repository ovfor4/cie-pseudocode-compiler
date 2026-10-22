// Startup check for a newer nightly build. It only reports; there is no auto-update.
// Release builds get CPSC_BUNDLE_TIME and CPSC_BUNDLE_COMMIT at build time; without
// them (dev, tests) nothing runs. Every failure is silent.
use serde::Deserialize;
use std::time::Duration;
use tauri::AppHandle;
use tauri_plugin_dialog::{DialogExt, MessageDialogButtons, MessageDialogKind};
use tauri_plugin_opener::OpenerExt;

const LOCAL_TIME: Option<&str> = option_env!("CPSC_BUNDLE_TIME");
const LOCAL_COMMIT: Option<&str> = option_env!("CPSC_BUNDLE_COMMIT");
const VERSION_URL: &str = match option_env!("CPSC_BUNDLE_VERSION_URL") {
    Some(url) => url,
    None => "https://github.com/ovfor4/cie-pseudocode-compiler/releases/download/nightly-build/version.json",
};
const RELEASE_PAGE: &str = "https://github.com/ovfor4/cie-pseudocode-compiler/releases/tag/nightly-build";

#[cfg(windows)]
const ASSET: &str = "cpsc-bundle-latest-Windows-x86_64.exe";
#[cfg(target_os = "macos")]
const ASSET: &str = "cpsc-bundle-latest-Darwin-arm64.tar.xz";
#[cfg(not(any(windows, target_os = "macos")))]
const ASSET: &str = "";

#[derive(Deserialize)]
struct Product {
    time: String,
    commit: String,
}

#[derive(Deserialize)]
struct Versions {
    bundle: Product,
}

fn latest(json: &str) -> Option<Product> {
    serde_json::from_str::<Versions>(json).ok().map(|v| v.bundle)
}

// Times look like YYYYMMDD-HHMMSS, so a string comparison orders them.
fn valid(time: &str) -> bool {
    let b = time.as_bytes();
    b.len() == 15 && b[8] == b'-' && b.iter().enumerate().all(|(i, c)| i == 8 || c.is_ascii_digit())
}

fn newer(remote_time: &str, local_time: &str) -> bool {
    valid(remote_time) && valid(local_time) && remote_time > local_time
}

fn fetch() -> Option<Product> {
    let agent: ureq::Agent = ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(10)))
        .build()
        .into();
    let body = agent.get(VERSION_URL).call().ok()?.body_mut().read_to_string().ok()?;
    latest(&body)
}

pub fn check(app: AppHandle) {
    let (Some(time), Some(commit)) = (LOCAL_TIME, LOCAL_COMMIT) else {
        return;
    };
    std::thread::spawn(move || {
        let Some(remote) = fetch() else { return };
        if !newer(&remote.time, time) {
            return;
        }
        let download = if ASSET.is_empty() {
            String::new()
        } else {
            format!("Download {ASSET} from:\n")
        };
        let message = format!(
            "A newer CPSC Bundle is available.\n\nInstalled: {time} ({commit})\nLatest:    {} ({})\n\n{download}{RELEASE_PAGE}",
            remote.time, remote.commit
        );
        let opener = app.clone();
        app.dialog()
            .message(message)
            .title("Update available")
            .kind(MessageDialogKind::Info)
            .buttons(MessageDialogButtons::OkCancelCustom(
                "Open Release Page".into(),
                "Later".into(),
            ))
            .show(move |open| {
                if open {
                    let _ = opener.opener().open_url(RELEASE_PAGE, None::<&str>);
                }
            });
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn compares_times() {
        assert!(newer("20261022-044114", "20261021-140654"));
        assert!(!newer("20261022-044114", "20261022-044114"));
        assert!(!newer("20261021-140654", "20261022-044114"));
        assert!(!newer("garbage", "20261022-044114"));
        assert!(!newer("20261022-044114", "2026102-0441144"));
        assert!(!newer("20261022x044114", "20200101-000000"));
    }

    #[test]
    fn parses_version_json() {
        let json = r#"{
            "compiler": { "time": "20261022-044114", "commit": "620a154" },
            "plugin":   { "time": "20261021-140654", "commit": "899c2af" },
            "bundle":   { "time": "20261022-044114", "commit": "620a154" }
        }"#;
        let bundle = latest(json).unwrap();
        assert_eq!((bundle.time.as_str(), bundle.commit.as_str()), ("20261022-044114", "620a154"));
    }

    #[test]
    fn rejects_garbage_json() {
        assert!(latest("not json").is_none());
        assert!(latest("{}").is_none());
    }
}
