import * as monaco from 'monaco-editor/editor/editor.api.js';
import 'monaco-editor/editor/browser/coreCommands.js';
import 'monaco-editor/editor/contrib/clipboard/browser/clipboard.js';
import 'monaco-editor/editor/contrib/contextmenu/browser/contextmenu.js';
import 'monaco-editor/editor/contrib/format/browser/formatActions.js';
import 'monaco-editor/editor/contrib/bracketMatching/browser/bracketMatching.js';
import 'monaco-editor/editor/contrib/linesOperations/browser/linesOperations.js';
import 'monaco-editor/editor/contrib/wordOperations/browser/wordOperations.js';
import 'monaco-editor/editor/contrib/find/browser/findController.js';
import EditorWorker from 'monaco-editor/editor/editor.worker.js?worker';
import textmate from 'vscode-textmate';
import oniguruma from 'vscode-oniguruma';
import wasmUrl from 'vscode-oniguruma/release/onig.wasm?url';
import { parse } from 'jsonc-parser';
import grammar from '../../plugin/syntaxes/cie-pseudocode.tmLanguage.json';
import configuration from '../../plugin/language-configuration.json';
import indentation from '../../plugin/src/indentation.js';
import brackets from '../../plugin/src/brackets.js';
import darkPlusSource from '../vendor/vscode/dark_plus.json?raw';
import darkSource from '../vendor/vscode/dark_vs.json?raw';

const languageId = 'cie-pseudocode';
const { indentationEdits, onTypeCharacters, shouldFormatOnType, selectionEndLine } = indentation;
const dark = parse(darkSource);
const plus = parse(darkPlusSource);
export const theme = { ...dark, ...plus, tokenColors: [...dark.tokenColors, ...plus.tokenColors] };

class TokenState {
	constructor(stack) { this.stack = stack; }
	clone() { return new TokenState(this.stack); }
	equals(other) { return other instanceof TokenState && (this.stack === other.stack || this.stack.equals(other.stack)); }
}

export async function createEditor(element) {
	globalThis.MonacoEnvironment = { getWorker: () => new EditorWorker() };
	const { Registry, INITIAL } = textmate;
	const { loadWASM, OnigScanner, OnigString } = oniguruma;
	await loadWASM(await (await fetch(wasmUrl)).arrayBuffer());
	const registry = new Registry({
		theme: { settings: theme.tokenColors },
		onigLib: Promise.resolve({ createOnigScanner: patterns => new OnigScanner(patterns), createOnigString: text => new OnigString(text) }),
		loadGrammar: async scope => scope === grammar.scopeName ? grammar : null
	});
	monaco.languages.register({ id: languageId, extensions: ['.cps'] });
	monaco.languages.setLanguageConfiguration(languageId, { ...configuration, wordPattern: new RegExp(configuration.wordPattern) });
	const tmGrammar = await registry.loadGrammarWithConfiguration(grammar.scopeName, monaco.languages.getEncodedLanguageId(languageId), { balancedBracketSelectors: ['*'] });
	monaco.editor.defineTheme('cpsc-dark-plus', { base: 'vs-dark', inherit: false, rules: [], colors: dark.colors });
	monaco.editor.setTheme('cpsc-dark-plus');
	monaco.languages.setColorMap(registry.getColorMap());
	monaco.languages.setTokensProvider(languageId, {
		getInitialState: () => new TokenState(INITIAL),
		tokenizeEncoded(line, state) {
			const result = tmGrammar.tokenizeLine2(line, state.stack);
			return { tokens: result.tokens, endState: new TokenState(result.ruleStack) };
		}
	});
	function edits(model, start, end, keepBlank = false) {
		return indentationEdits(model.getLinesContent(), start, end, keepBlank).map(edit => ({
			range: new monaco.Range(edit.line + 1, 1, edit.line + 1, edit.length + 1), text: edit.text
		}));
	}
	monaco.languages.registerDocumentRangeFormattingEditProvider(languageId, {
		provideDocumentRangeFormattingEdits(model, range) {
			return edits(model, range.startLineNumber - 1, selectionEndLine(range.startLineNumber - 1, range.endLineNumber - 1, range.endColumn - 1));
		}
	});
	monaco.languages.registerOnTypeFormattingEditProvider(languageId, {
		autoFormatTriggerCharacters: onTypeCharacters,
		provideOnTypeFormattingEdits(model, position, character) {
			return shouldFormatOnType(model.getLineContent(position.lineNumber), character)
				? edits(model, position.lineNumber - 1, position.lineNumber - 1, character === '\n') : [];
		}
	});
	const editor = monaco.editor.create(element, {
		language: languageId, theme: 'cpsc-dark-plus', automaticLayout: true,
		insertSpaces: false, tabSize: 4, detectIndentation: false, autoIndent: 'full', formatOnType: true,
		bracketPairColorization: { enabled: true, independentColorPoolPerBracketType: false },
		minimap: { enabled: false }, smoothScrolling: false, cursorSmoothCaretAnimation: 'off',
		scrollBeyondLastLine: false, fontSize: 14, accessibilitySupport: 'auto'
	});
	const decorations = editor.createDecorationsCollection();
	editor.onDidChangeModelContent(() => {
		const model = editor.getModel();
		decorations.set(brackets.unmatchedBrackets(model.getValue()).map(offset => {
			const start = model.getPositionAt(offset);
			return { range: new monaco.Range(start.lineNumber, start.column, start.lineNumber, start.column + 1), options: { inlineClassName: 'invalid-bracket' } };
		}));
	});
	return editor;
}
