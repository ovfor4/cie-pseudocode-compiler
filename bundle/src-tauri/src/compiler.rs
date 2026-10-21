use std::path::PathBuf;

// Keep startup failures available to the UI, including an unwritable working directory.
pub struct Compiler(pub Result<PathBuf, String>);

impl Compiler {
    pub fn load() -> Self {
        Self(executable().map_err(|error| format!("Cannot load compiler: {error}")))
    }
}

#[cfg(not(windows))]
fn executable() -> std::io::Result<PathBuf> {
    Ok(std::env::current_exe()?.with_file_name("cpsc"))
}

#[cfg(windows)]
fn executable() -> std::io::Result<PathBuf> {
    use std::{
        fs::{self, File},
        io,
    };

    let directory = std::env::current_dir()?;
    let destination = directory.join("cpsc.exe");
    let staged = directory.join(format!(".cpsc-{}.tmp", std::process::id()));
    let result = (|| {
        let mut compressed = flate2::read::GzDecoder::new(
            &include_bytes!(concat!(env!("CARGO_MANIFEST_DIR"), "/binaries/cpsc.exe.gz"))[..],
        );
        let mut file = File::create(&staged)?;
        io::copy(&mut compressed, &mut file)?;
        drop(file);
        fs::rename(&staged, &destination)?;
        Ok(destination)
    })();
    if result.is_err() {
        let _ = fs::remove_file(staged);
    }
    result
}
