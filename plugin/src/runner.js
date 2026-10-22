'use strict';

const path = require('node:path');

function quote(value, shell) {
	if (/[\r\n\0]/.test(value)) throw new Error('Run paths must not contain line breaks or NUL characters.');
	if (shell === 'cmd') return `"${value}"`;
	// PowerShell also ends single-quoted strings at the typographic quotes U+2018 to U+201B.
	if (shell === 'powershell') return "'" + value.replace(/['\u2018-\u201B]/g, m => m + m) + "'";
	return "'" + value.replace(/'/g, "'\\''") + "'";
}

function buildRunCommand(compiler, file, cwd, shell) {
	const run = `${shell === 'powershell' ? '& ' : ''}${quote(compiler, shell)} ${quote(file, shell)} -r`;
	if (shell === 'powershell') return `Set-Location -LiteralPath ${quote(cwd, shell)}; if ($?) { ${run} }`;
	return `cd ${shell === 'cmd' ? '/d ' : ''}${quote(cwd, shell)} && ${run}`;
}

// Adapted from Code Runner's isPowershellOnWindows; also recognize pwsh and named profiles.
function terminalShell(vscode, platform) {
	if (platform !== 'win32') return 'posix';
	const config = vscode.workspace.getConfiguration('terminal');
	const profileName = config.get('integrated.defaultProfile.windows');
	const profile = config.get('integrated.profiles.windows', {})[profileName];
	const profilePath = Array.isArray(profile?.path) ? profile.path[0] : profile?.path;
	const shell = profilePath || profile?.source || profileName || vscode.env.shell;
	if (/(?:powershell|pwsh)/i.test(shell)) return 'powershell';
	if (/(?:^|[\\/])cmd(?:\.exe)?$|^Command Prompt$/i.test(shell)) return 'cmd';
	throw new Error('Choose PowerShell or Command Prompt as the default Windows terminal profile.');
}

function registerRunner(vscode, context, platform = process.platform) {
	let terminal = null;
	let shell;
	let preparing = false;
	const command = vscode.commands.registerCommand('cie-pseudocode.runFile', async uri => {
		if (preparing) return;
		preparing = true;
		try {
			let document = uri ? await vscode.workspace.openTextDocument(uri) : vscode.window.activeTextEditor?.document;
			if (!document || document.languageId !== 'cie-pseudocode') {
				vscode.window.showErrorMessage('Open a CIE pseudocode file to run.');
				return;
			}
			const wasUntitled = document.isUntitled;
			if (wasUntitled) await vscode.window.showTextDocument(document);
			if (!(await document.save())) return;
			// Saving an untitled document replaces it with a new file-backed document.
			if (wasUntitled) document = vscode.window.activeTextEditor?.document;
			if (!document || document.isUntitled || document.uri.scheme !== 'file') {
				vscode.window.showErrorMessage('Save the pseudocode to a local file before running.');
				return;
			}
			const cwd = vscode.workspace.getWorkspaceFolder(document.uri)?.uri.fsPath || path.dirname(document.uri.fsPath);
			const compiler = vscode.workspace.getConfiguration('cie-pseudocode', document.uri).get('compilerPath', 'cpsc');
			if (!compiler.trim()) throw new Error('Set CIE Pseudocode: Compiler Path to cpsc or its full path.');
			const runShell = terminal ? shell : terminalShell(vscode, platform);
			const text = buildRunCommand(compiler, document.uri.fsPath, cwd, runShell);
			// Adapted from Code Runner's executeCommandInTerminal (see ThirdPartyNotices.md).
			if (terminal === null) {
				terminal = vscode.window.createTerminal('CIE Pseudocode');
				shell = runShell;
			}
			terminal.show(false);
			terminal.sendText(text);
		} catch (error) {
			vscode.window.showErrorMessage(`Could not run pseudocode: ${error.message}`);
		} finally {
			preparing = false;
		}
	});
	context.subscriptions.push(
		command,
		vscode.window.onDidCloseTerminal(closed => {
			if (closed === terminal) terminal = null;
		}),
		{ dispose() { terminal?.dispose(); } }
	);
}

module.exports = { buildRunCommand, registerRunner };
