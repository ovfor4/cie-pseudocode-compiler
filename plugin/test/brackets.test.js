'use strict';

const assert = require('node:assert/strict');
const { test } = require('node:test');
const { unmatchedBrackets } = require('../src/brackets');

test('mixed nesting matches across lines and repaired brackets stop being errors', () => {
	assert.deepEqual(unmatchedBrackets('A[(B + C)\n + D[1]]'), []);
	assert.deepEqual(unmatchedBrackets('A['), [1]);
	assert.deepEqual(unmatchedBrackets('A[1]'), []);
	assert.deepEqual(unmatchedBrackets('(\n)'), []);
});

test('unpaired opening, closing and crossed bracket types are all reported', () => {
	assert.deepEqual(unmatchedBrackets('(['), [0, 1]);
	assert.deepEqual(unmatchedBrackets(')]'), [0, 1]);
	assert.deepEqual(unmatchedBrackets('([)]'), [0, 1, 2, 3]);
	assert.deepEqual(unmatchedBrackets('(]'), [0, 1]);
});

test('comments and quotes are ignored with correct UTF-16 offsets after Unicode and CRLF', () => {
	assert.deepEqual(unmatchedBrackets('OUTPUT "[( //" // )]\nOUTPUT \'[\''), []);
	const unfinished = 'OUTPUT "unfinished [\r\nA[';
	assert.deepEqual(unmatchedBrackets(unfinished), [unfinished.length - 1]);
	const unicode = 'OUTPUT "你好😀"\r\nA[';
	assert.deepEqual(unmatchedBrackets(unicode), [unicode.length - 1]);
	// Backslashes do not escape CIE quotes.
	assert.deepEqual(unmatchedBrackets('"x\\"['), [4]);
});
