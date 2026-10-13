'use strict';

const vscode = require('vscode');
const { indentationEdits } = require('./indentation');
const { codeOnly } = require('./lexical');
const { unmatchedBrackets } = require('./brackets');
const languageId = 'cie-pseudocode';

function formatLines(document, startLine, endLine, keepBlankIndent = false) {
	const lines = document.getText().split(/\r?\n/);
	return indentationEdits(lines, startLine, endLine, keepBlankIndent).map(edit =>
		vscode.TextEdit.replace(new vscode.Range(edit.line, 0, edit.line, edit.length), edit.text));
}

function activate(context) {
	const invalidBracket = vscode.window.createTextEditorDecorationType({
		dark: { color: '#F44747' }
	});
	function refreshBrackets(document) {
		const rangesByDocument = new Map();
		for (const editor of vscode.window.visibleTextEditors) {
			if (document && editor.document !== document) continue;
			if (editor.document.languageId !== languageId) {
				editor.setDecorations(invalidBracket, []);
				continue;
			}
			if (!rangesByDocument.has(editor.document)) {
				const ranges = unmatchedBrackets(editor.document.getText()).map(offset =>
					new vscode.Range(editor.document.positionAt(offset), editor.document.positionAt(offset + 1)));
				rangesByDocument.set(editor.document, ranges);
			}
			editor.setDecorations(invalidBracket, rangesByDocument.get(editor.document));
		}
	}
	context.subscriptions.push(
		invalidBracket,
		vscode.workspace.onDidChangeTextDocument(event => refreshBrackets(event.document)),
		vscode.window.onDidChangeVisibleTextEditors(() => refreshBrackets()),
		vscode.languages.registerDocumentRangeFormattingEditProvider(languageId, {
			provideDocumentRangeFormattingEdits(document, range) {
				const endLine = range.end.character === 0 && range.end.line > range.start.line
					? range.end.line - 1 : range.end.line;
				return formatLines(document, range.start.line, endLine);
			}
		}),
		vscode.languages.registerOnTypeFormattingEditProvider(languageId, {
			provideOnTypeFormattingEdits(document, position, char) {
				if (char !== '\n' && !/^\s*(?:ELSE|ENDIF|ENDWHILE|UNTIL|NEXT|ENDFUNCTION|ENDPROCEDURE|ENDTYPE)\b/.test(
					codeOnly(document.lineAt(position.line).text))) return [];
				return formatLines(document, position.line, position.line, char === '\n');
			}
		}, '\n', 'E', 'F', 'L', 'T', 'N')
	);
	refreshBrackets();
}

module.exports = { activate };
