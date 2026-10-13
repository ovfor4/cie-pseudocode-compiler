'use strict';

const vscode = require('vscode');
const { indentationEdits } = require('./indentation');
const { codeOnly } = require('./lexical');
const languageId = 'cie-pseudocode';

function formatLines(document, startLine, endLine, keepBlankIndent = false) {
	const lines = document.getText().split(/\r?\n/);
	return indentationEdits(lines, startLine, endLine, keepBlankIndent).map(edit =>
		vscode.TextEdit.replace(new vscode.Range(edit.line, 0, edit.line, edit.length), edit.text));
}

function activate(context) {
	context.subscriptions.push(
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
}

module.exports = { activate };
