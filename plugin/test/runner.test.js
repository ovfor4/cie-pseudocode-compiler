'use strict';

const assert = require('node:assert/strict');
const { test } = require('node:test');
const { spawnSync } = require('node:child_process');
const { buildRunCommand, registerRunner } = require('../src/runner');

test('commands quote paths and only run after changing directory', () => {
	assert.equal(buildRunCommand('/tools/my cpsc', '/work/my file.cps', '/work/my project', 'posix'),
		"cd '/work/my project' && '/tools/my cpsc' '/work/my file.cps' -r");
	assert.equal(buildRunCommand('C:\\My Tools\\cpsc.exe', 'D:\\My Files\\test.cps', 'D:\\My Files', 'cmd'),
		'cd /d "D:\\My Files" && "C:\\My Tools\\cpsc.exe" "D:\\My Files\\test.cps" -r');
	assert.equal(buildRunCommand('C:\\My Tools\\cpsc.exe', "D:\\Pat's Files\\test.cps", "D:\\Pat's Files", 'powershell'),
		"Set-Location -LiteralPath 'D:\\Pat''s Files'; if ($?) { & 'C:\\My Tools\\cpsc.exe' 'D:\\Pat''s Files\\test.cps' -r }");
	assert.throws(() => buildRunCommand('cpsc\nother', 'test.cps', '.', 'posix'), /line breaks/);
	if (process.platform !== 'win32') {
		const file = "/tmp/Pat's file $(echo unexpected) `echo unexpected`.cps";
		const result = spawnSync('/bin/sh', ['-c', buildRunCommand('/usr/bin/printf', file, '/tmp', 'posix')], { encoding: 'utf8' });
		assert.equal(result.status, 0);
		assert.equal(result.stdout, file);
		const failed = spawnSync('/bin/sh', ['-c', buildRunCommand('/usr/bin/printf', file, '/missing-cie-run-directory', 'posix')], { encoding: 'utf8' });
		assert.notEqual(failed.status, 0);
		assert.equal(failed.stdout, '');
	}
});

function harness(platform = 'linux') {
	let run, close;
	const calls = { saved: 0, terminals: [], errors: [], settings: [] };
	const document = {
		languageId: 'cie-pseudocode', isUntitled: false,
		uri: { scheme: 'file', fsPath: '/project/sub/my file.cps' },
		async save() { calls.saved++; return true; }
	};
	const vscode = {
		env: { shell: 'C:\\Windows\\System32\\cmd.exe' },
		commands: { registerCommand(id, callback) { assert.equal(id, 'cie-pseudocode.runFile'); run = callback; return { dispose() {} }; } },
		workspace: {
			async openTextDocument() { return document; },
			getWorkspaceFolder() { return { uri: { fsPath: '/project' } }; },
			getConfiguration(section, uri) {
				calls.settings.push({ section, uri });
				return { get(key, fallback) { return section === 'cie-pseudocode' ? '/tools/my cpsc' : fallback; } };
			}
		},
		window: {
			activeTextEditor: { document },
			async showTextDocument(doc) { this.activeTextEditor = { document: doc }; },
			showErrorMessage(message) { calls.errors.push(message); },
			onDidCloseTerminal(callback) { close = callback; return { dispose() {} }; },
			createTerminal(name) {
				const terminal = { name, sent: [], show(focus) { this.preserveFocus = focus; }, sendText(text) { this.sent.push(text); }, dispose() {} };
				calls.terminals.push(terminal);
				return terminal;
			}
		}
	};
	registerRunner(vscode, { subscriptions: [] }, platform);
	return { vscode, document, calls, run: uri => run(uri), close: terminal => close(terminal) };
}

test('save before running the whole file, scope settings, reuse and recreate the terminal', async () => {
	const h = harness();
	await h.run(h.document.uri);
	assert.equal(h.calls.saved, 1);
	assert.equal(h.calls.terminals[0].preserveFocus, false);
	assert.equal(h.calls.terminals[0].sent[0], "cd '/project' && '/tools/my cpsc' '/project/sub/my file.cps' -r");
	assert.equal(h.calls.settings[0].uri, h.document.uri);
	h.close({});
	await h.run();
	assert.equal(h.calls.terminals.length, 1);
	h.close(h.calls.terminals[0]);
	h.vscode.workspace.getWorkspaceFolder = () => undefined;
	await h.run();
	assert.equal(h.calls.terminals.length, 2);
	assert.match(h.calls.terminals[1].sent[0], /^cd '\/project\/sub'/);
	assert.deepEqual(h.calls.errors, []);
});

test('save cancellation and failure do not create a terminal; untitled files use the saved URI', async () => {
	const h = harness();
	h.document.isUntitled = true;
	h.document.uri = { scheme: 'untitled', fsPath: 'Untitled-1' };
	h.document.save = async () => false;
	await h.run();
	assert.equal(h.calls.terminals.length, 0);
	h.document.save = async () => { throw new Error('Save failed'); };
	await h.run();
	assert.match(h.calls.errors[0], /Save failed/);
	assert.equal(h.calls.terminals.length, 0);
	h.document.save = async () => {
		h.vscode.window.activeTextEditor = { document: { ...h.document, isUntitled: false,
			uri: { scheme: 'file', fsPath: '/project/new file.cps' } } };
		return true;
	};
	await h.run();
	assert.match(h.calls.terminals[0].sent[0], /'\/project\/new file.cps' -r$/);
});

test('Windows recognizes custom pwsh and cmd profiles and retains the reused terminal shell', async () => {
	const h = harness('win32');
	let profile = 'My Shell';
	h.vscode.workspace.getConfiguration = section => ({ get(key, fallback) {
		if (section === 'cie-pseudocode') return 'cpsc';
		if (key === 'integrated.defaultProfile.windows') return profile;
		if (key === 'integrated.profiles.windows') return { 'My Shell': { path: 'C:\\Program Files\\PowerShell\\7\\pwsh.exe' } };
		return fallback;
	} });
	await h.run();
	assert.match(h.calls.terminals[0].sent[0], /^Set-Location.*& 'cpsc'/);
	profile = 'Command Prompt';
	await h.run();
	assert.match(h.calls.terminals[0].sent[1], /^Set-Location/);
	h.close(h.calls.terminals[0]);
	await h.run();
	assert.match(h.calls.terminals[1].sent[0], /^cd \/d .*&& "cpsc"/);
});
