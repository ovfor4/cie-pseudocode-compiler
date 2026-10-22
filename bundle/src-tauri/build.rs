fn main() {
    #[cfg(windows)]
    println!("cargo:rerun-if-changed=binaries/cpsc.exe.gz");
    for name in ["CPSC_BUNDLE_TIME", "CPSC_BUNDLE_COMMIT", "CPSC_BUNDLE_VERSION_URL"] {
        println!("cargo:rerun-if-env-changed={name}");
    }
    tauri_build::build()
}
