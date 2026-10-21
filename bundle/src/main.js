import { invoke } from '@tauri-apps/api/core';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { open, save, message } from '@tauri-apps/plugin-dialog';
import { createEditor } from './editor.js';
import './style.css';

const status = document.querySelector('#status');
const filename = document.querySelector('#filename');
const window = getCurrentWindow();
const filters = [{ name: 'Pseudocode', extensions: ['cps'] }];
let path = null;
let savedText = '';
let busy = false;
let closing = false;
let editor;

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
async function action(callback) {
	if (busy) return;
	busy = true;
	try { await callback(); } catch (error) { report(error); } finally { busy = false; }
}

try {
	editor = await createEditor(document.querySelector('#editor'));
	editor.onDidChangeModelContent(refreshTitle);
	document.querySelector('#save').onclick = () => action(saveFile);
	document.querySelector('#new').onclick = () => action(async () => {
		if (!await canReplace()) return;
		path = null;
		savedText = '';
		editor.setValue('');
		refreshTitle();
		status.textContent = 'Ready';
	});
	document.querySelector('#open').onclick = () => action(async () => {
		if (!await canReplace()) return;
		const target = await open({ filters, multiple: false });
		if (!target) return;
		const text = await invoke('read_file', { path: target });
		path = target;
		savedText = text;
		editor.setValue(text);
		refreshTitle();
		status.textContent = 'Ready';
	});
	document.addEventListener('keydown', event => {
		if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') {
			event.preventDefault();
			action(saveFile);
		}
	});
	await window.onCloseRequested(async event => {
		if (closing) return;
		event.preventDefault();
		await action(async () => {
			if (!await canReplace()) return;
			closing = true;
			await window.close();
		});
	});
	editor.focus();
} catch (error) { report(error); }
