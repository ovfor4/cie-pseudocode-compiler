'use strict';

const assert = require('node:assert/strict');
const { test } = require('node:test');
const { lineDepths, indentationEdits } = require('../src/indentation');

test('nested branches and each implemented block use one Tab per level', () => {
	const lines = [
		'PROCEDURE Demo()', 'FOR I <- 1 TO 3', 'IF X THEN',
		'WHILE Y DO', 'OUTPUT X', 'ENDWHILE', 'ELSE',
		'REPEAT', 'OUTPUT Y', 'UNTIL Done', 'ENDIF', 'NEXT I', 'ENDPROCEDURE',
		'FUNCTION Value() RETURNS INTEGER', 'RETURN 1', 'ENDFUNCTION',
		'TYPE Record', 'DECLARE X : INTEGER', 'ENDTYPE'
	];
	assert.deepEqual(lineDepths(lines), [0, 1, 2, 3, 4, 3, 2, 3, 4, 3, 2, 1, 0, 0, 1, 0, 0, 1, 0]);
});

test('split THEN/DO, single-line TYPE, comments and strings do not add extra levels', () => {
	const lines = [
		'TYPE Season = (Spring, Summer)', 'TYPE Ptr = ^INTEGER', 'IF Ready',
		'THEN // IF DO', 'WHILE Check("DO")', 'DO', 'OUTPUT "ENDIF // IF THEN"',
		"OUTPUT 'E' // ENDWHILE", 'ENDWHILE', 'ENDIF', 'IFValue <- 1', 'if X then'
	];
	assert.deepEqual(lineDepths(lines), [0, 0, 0, 0, 1, 1, 2, 2, 1, 0, 0, 0]);
});

test('range formatting reads earlier context, preserves text and is idempotent', () => {
	const lines = ['IF X THEN', '   WHILE Y DO', ' OUTPUT "x"  // keep spaces', ' ENDWHILE', 'ENDIF'];
	const edits = indentationEdits(lines, 2, 3);
	assert.deepEqual(edits, [
		{ line: 2, length: 1, text: '\t\t' }, { line: 3, length: 1, text: '\t' }
	]);
	for (const edit of edits) lines[edit.line] = edit.text + lines[edit.line].slice(edit.length);
	assert.equal(lines[2], '\t\tOUTPUT "x"  // keep spaces');
	assert.equal(lines[1], '   WHILE Y DO');
	assert.deepEqual(indentationEdits(lines, 2, 3), []);
	assert.deepEqual(indentationEdits(['ENDIF', 'UNTIL X', 'NEXT I'], 0, 2), []);
});

test('blank lines are empty when formatted and indented at the caret after Enter', () => {
	const lines = ['IF X THEN', '   '];
	assert.deepEqual(indentationEdits(lines, 1, 1), [{ line: 1, length: 3, text: '' }]);
	assert.deepEqual(indentationEdits(lines, 1, 1, true), [{ line: 1, length: 3, text: '\t' }]);
});
