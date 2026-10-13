# CIE Pseudocode

用于本项目 `.cps` 文件的 VS Code 插件。需要 VS Code 1.85 或更新版本，仅适配深色主题，语法颜色跟随当前主题，参考 C++ 的配色分类。

## 安装

在 VS Code 的扩展面板点击右上角 `…` → **Install from VSIX…（从 VSIX 安装）**，选择 `cie-pseudocode-0.1.0.vsix`。

也可以在 VSIX 所在目录执行：

```sh
code --install-extension cie-pseudocode-0.1.0.vsix
```

打开 `.cps` 文件后，右下角语言模式应显示 **CIE Pseudocode**。建议使用 **Dark+** 或其他深色主题。

## 使用

- **自动缩进**：默认每层一个真实 Tab，显示宽度为 4。块开始后回车自动缩进，输入完整的 `ELSE`、`ENDIF`、`NEXT` 等关键字时自动回退。
- **整理选区**：选中代码，按 macOS 的 **⌘K，然后 ⌘F**；Windows/Linux 为 **Ctrl+K，然后 Ctrl+F**。也可以右键 → **Format Selection（格式化选区）**。只整理选中行的行首缩进，使用前文确定层级，保留其他文本和换行格式。空行整理后为空。
- **括号**：圆括号和方括号按嵌套层级上色；落单左括号、落单右括号及类型错配立即变红，补齐后恢复。字符串、字符和注释中的括号不参与检查。
- **高亮**：关键字、类型、布尔值、运算符、字符串、字符、数字、注释和 12 个内置函数使用对应的 C++ 风格配色分类。

缩进支持 IF/ELSE、WHILE、REPEAT、FOR、FUNCTION、PROCEDURE 和记录 TYPE；`THEN`、`DO` 可以单独成行。枚举和指针的单行 `TYPE ... = ...` 不增加缩进。

关键字必须大写；赋值使用 `<-`；注释使用 `//`；字符串及字符没有反斜杠转义。仅覆盖当前编译器已实现的语法，暂不支持 PDF 中的 CASE、类、SET、DATE 等未实现语法。

示例在 [examples/highlighting.cps](examples/highlighting.cps)。这是编辑辅助插件，编译与运行仍使用项目的 `cpsc` 命令。

如果用户或工作区设置覆盖了插件默认值，可在 VS Code 设置中确认：

```json
"[cie-pseudocode]": {
  "editor.insertSpaces": false,
  "editor.tabSize": 4,
  "editor.detectIndentation": false,
  "editor.autoIndent": "full",
  "editor.formatOnType": true,
  "editor.defaultFormatter": "ovfor4.cie-pseudocode",
  "editor.bracketPairColorization.enabled": true
}
```

## 开发与打包

在 `plugin/` 目录下执行，开发依赖需要 Node.js 22 或更新版本：

```sh
npm ci --ignore-scripts
npm run check
npm test
npm run package
```

打包命令会先检查语法并运行简单测试，然后生成 `cie-pseudocode-0.1.0.vsix`。插件运行时没有 npm 依赖，不需要安装编译器或 cpptools。VSIX、依赖和测试产物不提交 Git。

语言配置、注释/字符串规则及配色 scope 改造自 [VS Code 内置 C++ 扩展](https://github.com/microsoft/vscode/tree/main/extensions/cpp)，provider 注册方式参考 [cpptools](https://github.com/microsoft/vscode-cpptools)。本仓库的参考克隆位于 `reference/vscode-cpptools/`，原始 C++ 配置及 grammar 位于 `reference/vscode-cpp/`，均不纳入插件包。
