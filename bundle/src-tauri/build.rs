fn main() {
    #[cfg(windows)]
    println!("cargo:rerun-if-changed=binaries/cpsc.exe.gz");
    tauri_build::build()
}
