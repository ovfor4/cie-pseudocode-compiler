'use strict';

const assert = require('node:assert/strict');
const { test, before } = require('node:test');
const fs = require('node:fs');
const path = require('node:path');
const { Registry } = require('vscode-textmate');
const { loadWASM, OnigScanner, OnigString } = require('vscode-oniguruma');

let grammar;
before(async () => {
	const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
	await loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
	const registry = new Registry({
		onigLib: Promise.resolve({
			createOnigScanner: patterns => new OnigScanner(patterns),
			createOnigString: text => new OnigString(text)
		}),
		loadGrammar: async () => JSON.parse(fs.readFileSync(
			path.join(__dirname, '../syntaxes/cie-pseudocode.tmLanguage.json'), 'utf8'))
	});
	grammar = await registry.loadGrammar('source.cie-pseudocode');
});

function scopesAt(text, word) {
	const offset = text.indexOf(word);
	return grammar.tokenizeLine(text).tokens.find(
		token => token.startIndex <= offset && token.endIndex > offset).scopes;
}

test('C++ style scopes distinguish keywords, types, literals, operators and builtins', () => {
	for (const [text, word, scope] of [
		['IF Ready THEN', 'IF', 'keyword.control'],
		['DECLARE X : INTEGER', 'DECLARE', 'storage.modifier'],
		['DECLARE X : INTEGER', 'INTEGER', 'storage.type'],
		['X <- TRUE', 'TRUE', 'constant.language'],
		['X <- 12.5', '12.5', 'constant.numeric'],
		['X <- 2 DIV 1', 'DIV', 'keyword.operator.word'],
		['X <- 1', '<-', 'keyword.operator'],
		['OUTPUT "text"', 'text', 'string.quoted.double'],
		["X <- 'A'", 'A', 'string.quoted.single']
	]) assert.ok(scopesAt(text, word).some(s => s.startsWith(scope)), text);
	for (const name of ['LENGTH', 'MID', 'RIGHT', 'LEFT', 'LCASE', 'UCASE',
		'ASC', 'CHR', 'IS_NUM', 'NUM_TO_STR', 'STR_TO_NUM', 'EOF']) {
		assert.ok(scopesAt(`${name}(X)`, name).includes('entity.name.function.cie-pseudocode'));
	}
});

test('comments, strings, lowercase and keyword prefixes cannot become keywords', () => {
	assert.ok(scopesAt('// IF [(', 'IF').some(s => s.startsWith('comment.')));
	assert.ok(scopesAt('OUTPUT "IF // ["', 'IF').some(s => s.startsWith('string.')));
	for (const text of ['if X then', 'IFValue <- 1', 'CASE OF X', 'CLASS Example']) {
		assert.ok(!scopesAt(text, text.split(' ')[0]).some(
			s => /^(keyword|storage)\./.test(s)), text);
	}
	// CIE has neither backslash escapes nor multiline string literals.
	let state = grammar.tokenizeLine('OUTPUT "unfinished').ruleStack;
	assert.ok(grammar.tokenizeLine('IF X THEN', state).tokens[0].scopes.includes(
		'keyword.control.cie-pseudocode'));
	assert.ok(scopesAt('OUTPUT "x\\" + 1', '+').includes('keyword.operator.cie-pseudocode'));
});
