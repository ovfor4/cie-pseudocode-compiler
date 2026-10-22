import { invoke, Channel } from '@tauri-apps/api/core';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { open, save, message } from '@tauri-apps/plugin-dialog';
import { KeyCode, KeyMod } from 'monaco-editor/editor/editor.api.js';
import { StandardKeyboardEvent } from 'monaco-editor/base/browser/keyboardEvent.js';
import { createEditor, themeVariable } from './editor.js';
import { createAnsiParser } from './ansi.js';
import './style.css';

const status = document.querySelector('#status');
const filename = document.querySelector('#filename');
const input = document.querySelector('#input');
const output = document.querySelector('#output');
const runButton = document.querySelector('#run');
const window = getCurrentWindow();
const filters = [{ name: 'Pseudocode', extensions: ['cps'] }];
let path = null;
let savedText = '';
let busy = false;
let closing = false;
let editor;
let running = false;

function setRunning(value) {
	running = value;
	runButton.textContent = value ? 'Stop' : 'Run';
	for (const id of ['new', 'open', 'save']) document.getElementById(id).disabled = value;
	input.disabled = value;
	editor.updateOptions({ readOnly: value });
}

async function runFile() {
	if (!await saveFile()) return;
	setRunning(true);
	output.textContent = '';
	status.textContent = 'Running';
	const decoders = { stdout: new TextDecoder(), stderr: new TextDecoder() };
	// Only compiler diagnostics on stderr are colored; program output stays verbatim.
	const diagnostics = createAnsiParser();
	const pending = document.createDocumentFragment();
	let frame = null;
	function flush() {
		frame = null;
		if (!pending.hasChildNodes()) return;
		output.append(pending);
		output.scrollTop = output.scrollHeight;
	}
	function append(stream, text) {
		for (const run of stream === 'stderr' ? diagnostics(text) : [{ text }]) {
			if (!run.text) continue;
			if (!run.bold && !run.color) {
				if (pending.lastChild?.nodeType === Node.TEXT_NODE) pending.lastChild.appendData(run.text);
				else pending.append(run.text);
				continue;
			}
			const span = document.createElement('span');
			if (run.bold) span.style.fontWeight = 'bold';
			if (run.color) span.style.color = `var(${themeVariable(run.color)})`;
			span.textContent = run.text;
			pending.append(span);
		}
		if (frame === null) frame = requestAnimationFrame(flush);
	}
	let finish;
	const finished = new Promise(resolve => { finish = resolve; });
	const channel = new Channel(event => {
		if (event.event === 'data') {
			append(event.stream, decoders[event.stream].decode(Uint8Array.from(event.bytes), { stream: true }));
		} else if (event.event === 'finished') {
			for (const [stream, decoder] of Object.entries(decoders)) append(stream, decoder.decode());
			if (frame !== null) cancelAnimationFrame(frame);
			flush();
			status.textContent = event.stopped ? 'Stopped' : `Exit code: ${event.code ?? 'terminated'}`;
			finish();
		}
	});
	try {
		await invoke('run_file', { path, input: input.value, output: channel });
		await finished;
	} finally {
		if (frame !== null) cancelAnimationFrame(frame);
		flush();
		setRunning(false);
	}
}

function dirty() { return editor.getValue() !== savedText; }
function refreshTitle() {
	const name = path ? path.split(/[\\/]/).pop() : 'Untitled.cps';
	filename.textContent = `${dirty() ? '* ' : ''}${name}`;
	filename.title = path || name;
	window.setTitle(`${dirty() ? '* ' : ''}${name} — CPSC Bundle`);
}
function report(error) { status.textContent = String(error); }
async function saveFile() {
	const target = path || await save({ filters, defaultPath: 'Untitled.cps' });
	if (!target) return false;
	const text = editor.getValue();
	await invoke('write_file', { path: target, text });
	path = target;
	savedText = text;
	refreshTitle();
	status.textContent = 'Saved';
	return true;
}
async function canReplace() {
	if (!dirty()) return true;
	const answer = await message('Save changes?', {
		title: 'CPSC Bundle', kind: 'warning',
		buttons: { yes: 'Save', no: 'Discard', cancel: 'Cancel' }
	});
	if (answer === 'Cancel') return false;
	return answer === 'Discard' || await saveFile();
}
async function newFile() {
	if (!await canReplace()) return;
	path = null;
	savedText = '';
	editor.setValue('');
	refreshTitle();
	status.textContent = 'Ready';
}
async function openFile() {
	if (!await canReplace()) return;
	const target = await open({ filters, multiple: false });
	if (!target) return;
	const text = await invoke('read_file', { path: target });
	path = target;
	savedText = text;
	editor.setValue(text);
	refreshTitle();
	status.textContent = 'Ready';
}
async function action(callback) {
	if (busy || running) return;
	busy = true;
	try { await callback(); } catch (error) { report(error); } finally { busy = false; }
}
function editorAction(id) { editor.focus(); editor.getAction(id).run(); }
// Keyed by the menu item ids in src-tauri/src/menu.rs.
const commands = {
	new: () => action(newFile),
	// Each window is an independent native OS window, even while this one runs.
	new_window: () => invoke('new_window').catch(report),
	open: () => action(openFile),
	save: () => action(saveFile),
	find: () => editorAction('actions.find'),
	comment: () => editorAction('editor.action.commentLine')
};
// CtrlCmd is Cmd on macOS and Ctrl elsewhere, matching Monaco's own keybindings and the menu.
const shortcuts = [
	[KeyMod.CtrlCmd | KeyCode.KeyN, 'new'],
	[KeyMod.CtrlCmd | KeyMod.Shift | KeyCode.KeyN, 'new_window'],
	[KeyMod.CtrlCmd | KeyCode.KeyO, 'open'],
	[KeyMod.CtrlCmd | KeyCode.KeyS, 'save'],
	[KeyMod.CtrlCmd | KeyCode.KeyF, 'find'],
	[KeyMod.CtrlCmd | KeyCode.Slash, 'comment']
];

try {
	editor = await createEditor(document.querySelector('#editor'));
	editor.onDidChangeModelContent(refreshTitle);
	runButton.onclick = () => {
		if (running) {
			invoke('stop_run').catch(report);
		} else {
			action(runFile);
		}
	};
	document.querySelector('#save').onclick = () => action(saveFile);
	document.querySelector('#new').onclick = () => action(newFile);
	document.querySelector('#open').onclick = () => action(openFile);
	document.addEventListener('keydown', event => {
		// Monaco already handled its own keybindings, such as Find inside the editor.
		if (event.defaultPrevented) return;
		const key = new StandardKeyboardEvent(event);
		const shortcut = shortcuts.find(([binding]) => key.equals(binding));
		if (!shortcut) return;
		event.preventDefault();
		commands[shortcut[1]]();
	});
	await window.listen('menu', event => commands[event.payload]());
	await window.onCloseRequested(async event => {
		if (closing) return;
		event.preventDefault();
		if (running) {
			try { await invoke('stop_run'); } catch (error) { report(error); return; }
			// The command drains pipes before completing; let runFile release its UI lock.
			busy = false;
			setRunning(false);
		}
		await action(async () => {
			if (!await canReplace()) return;
			closing = true;
			await window.close();
		});
	});
	editor.focus();
} catch (error) { report(error); }
