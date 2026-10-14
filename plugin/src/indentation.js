'use strict';

const { codeOnly } = require('./lexical');
const closers = {
	ENDIF: 'IF', ENDWHILE: 'WHILE', UNTIL: 'REPEAT', NEXT: 'FOR',
	ENDFUNCTION: 'FUNCTION', ENDPROCEDURE: 'PROCEDURE', ENDTYPE: 'TYPE', ENDTRY: 'TRY'
};

function lineDepths(lines) {
	const stack = [];
	const depth = () => stack.filter(block => block.active).length;
	return lines.map(line => {
		const code = codeOnly(line).trim();
		const keyword = /^([A-Za-z][A-Za-z0-9_]*)\b/.exec(code)?.[1];
		const branch = keyword === 'ELSE' ? 'IF' : keyword === 'CATCH' ? 'TRY' : null;
		if (closers[keyword] || branch) {
			const kind = branch || closers[keyword];
			const index = stack.findLastIndex(block => block.kind === kind);
			if (index >= 0) {
				stack.length = index;
				const current = depth();
				if (branch) stack.push({ kind, active: true });
				return current;
			}
			// Incomplete code should still format without negative indentation.
			return depth();
		}

		const current = depth();
		if (keyword === 'THEN' || keyword === 'DO') {
			const top = stack[stack.length - 1];
			if (top?.kind === (keyword === 'THEN' ? 'IF' : 'WHILE')) top.active = true;
		} else if (keyword === 'IF' || keyword === 'WHILE') {
			stack.push({ kind: keyword, active: new RegExp(
				keyword === 'IF' ? '\\bTHEN\\b' : '\\bDO\\b').test(code) });
		} else if (['FOR', 'REPEAT', 'FUNCTION', 'PROCEDURE', 'TRY'].includes(keyword)) {
			stack.push({ kind: keyword, active: true });
		} else if (/^TYPE\s+[A-Za-z][A-Za-z0-9_]*$/.test(code)) {
			stack.push({ kind: 'TYPE', active: true });
		}
		return current;
	});
}

function indentationEdits(lines, startLine, endLine, keepBlankIndent = false) {
	const depths = lineDepths(lines.slice(0, endLine + 1));
	const edits = [];
	for (let line = startLine; line <= endLine; line++) {
		const oldIndent = /^[\t ]*/.exec(lines[line])[0];
		const blank = lines[line].trim() === '';
		const newIndent = blank && !keepBlankIndent ? '' : '\t'.repeat(depths[line]);
		if (oldIndent !== newIndent) {
			edits.push({ line, length: oldIndent.length, text: newIndent });
		}
	}
	return edits;
}

module.exports = { lineDepths, indentationEdits };
