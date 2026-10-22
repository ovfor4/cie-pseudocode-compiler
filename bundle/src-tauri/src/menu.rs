use tauri::{
    menu::{Menu, MenuBuilder, MenuItem, SubmenuBuilder},
    AppHandle, Runtime,
};

// One menu for all platforms: the macOS menu bar, or the top of each window elsewhere.
// Custom item ids name the frontend commands that the same shortcuts run.
pub fn build<R: Runtime>(app: &AppHandle<R>) -> tauri::Result<Menu<R>> {
    let item = |id: &str, text: &str, accelerator: &str| {
        MenuItem::with_id(app, id, text, true, Some(accelerator))
    };
    let file = SubmenuBuilder::new(app, "File")
        .item(&item("new", "New", "CmdOrCtrl+N")?)
        .item(&item("new_window", "New Window", "CmdOrCtrl+Shift+N")?)
        .item(&item("open", "Open…", "CmdOrCtrl+O")?)
        .item(&item("save", "Save", "CmdOrCtrl+S")?)
        .separator()
        .close_window()
        .build()?;
    // macOS needs the predefined edit items for clipboard and undo keys in the webview.
    let edit = SubmenuBuilder::new(app, "Edit")
        .undo()
        .redo()
        .separator()
        .cut()
        .copy()
        .paste()
        .select_all()
        .separator()
        .item(&item("find", "Find", "CmdOrCtrl+F")?)
        .item(&item("comment", "Toggle Line Comment", "CmdOrCtrl+/")?)
        .build()?;
    // Windows share the menu but run independently, so the label covers both states.
    let run = SubmenuBuilder::new(app, "Run")
        .item(&item("run", "Run/Stop", "F5")?)
        .build()?;
    let window = SubmenuBuilder::new(app, "Window")
        .minimize()
        .maximize()
        .build()?;
    let menu = MenuBuilder::new(app);
    // macOS shows the first submenu as the application menu. Quit skips unsaved-changes
    // prompts, so other platforms close windows instead.
    #[cfg(target_os = "macos")]
    let menu = menu.item(
        &SubmenuBuilder::new(app, "CPSC Bundle")
            .about(None)
            .separator()
            .services()
            .separator()
            .hide()
            .hide_others()
            .show_all()
            .separator()
            .quit()
            .build()?,
    );
    menu.items(&[&file, &edit, &run, &window]).build()
}
