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
		['DECLARE X : C_INT', 'C_INT', 'storage.type'],
		['DECLARE X : C_CHAR', 'C_CHAR', 'storage.type'],
		['DECLARE X : C_STRING', 'C_STRING', 'storage.type'],
		['OUTPUT (C_DOUBLE)X', 'C_DOUBLE', 'storage.type'],
		['TYPE Native = C_TYPE("int")', 'C_TYPE', 'storage.type'],
		['X <- TRUE', 'TRUE', 'constant.language'],
		['X <- 12.5', '12.5', 'constant.numeric'],
		['X <- 2 DIV 1', 'DIV', 'keyword.operator.word'],
		['X <- 1', '<-', 'keyword.operator.assignment.cpp'],
		['OUTPUT "text"', 'text', 'string.quoted.double'],
		["X <- 'A'", 'A', 'string.quoted.single']
	]) assert.ok(scopesAt(text, word).some(s => s.startsWith(scope)), text);
	for (const name of ['LENGTH', 'MID', 'RIGHT', 'LEFT', 'LCASE', 'UCASE',
		'ASC', 'CHR', 'IS_NUM', 'NUM_TO_STR', 'STR_TO_NUM', 'EOF']) {
		assert.ok(scopesAt(`${name}(X)`, name).includes('entity.name.function.cie-pseudocode'));
	}
});

test('both assignment characters share the C++ assignment scope', () => {
	const text = 'X <- 1';
	assert.deepEqual(scopesAt(text, '<-'), scopesAt(text, '-'));
	assert.ok(!scopesAt('X < 1', '<').includes('keyword.operator.assignment.cpp'));
	assert.ok(!scopesAt('X - 1', '-').includes('keyword.operator.assignment.cpp'));
});

test('supported keywords reuse C++ scopes without matching identifiers or quoted text', () => {
	for (const [words, scope] of [
		[['CASE', 'ENDCASE'], 'keyword.control.switch.cpp'],
		[['OTHERWISE'], 'keyword.control.default.cpp'],
		[['CONSTANT'], 'storage.modifier.const.cpp'],
		[['CLASS', 'ENDCLASS'], 'storage.type.class.cpp'],
		[['PUBLIC'], 'storage.type.modifier.access.control.public.cpp'],
		[['PRIVATE'], 'storage.type.modifier.access.control.private.cpp'],
		[['TEMPLATE'], 'storage.type.template.cpp'],
		[['OPERATOR'], 'keyword.other.operator.overload.cpp'],
		[['OVERLOAD', 'EXTERN_FUNCTION', 'EXTERN_PROCEDURE'], 'storage.modifier.cpp'],
		[['EXTERN'], 'storage.modifier.specifier.extern.cpp'],
		[['INCLUDE_OBJ'], 'keyword.control.directive.include.cpp']
	]) for (const word of words) {
		assert.ok(scopesAt(word, word).includes(scope), word);
		for (const text of [word.toLowerCase(), `${word}Value`, `_${word}`, `${word}_1`]) {
			assert.ok(!scopesAt(text, text).includes(scope), text);
		}
		assert.ok(scopesAt(`// ${word}`, word).some(s => s.startsWith('comment.')));
		assert.ok(scopesAt(`OUTPUT "${word}"`, word).some(s => s.startsWith('string.')));
	}
});

test('library directives require their own line and ellipses use the C++ variadic scope', () => {
	assert.ok(scopesAt('\t#LIB  ', '#').includes('punctuation.definition.directive.cpp'));
	assert.ok(scopesAt('\t#LIB  ', 'LIB').includes('keyword.control.directive.pragma.cpp'));
	for (const text of ['#lib', '#LIBRARY', '#LIB // comment', 'X #LIB', '// #LIB', 'OUTPUT "#LIB"']) {
		assert.ok(!scopesAt(text, '#')
			.includes('punctuation.definition.directive.cpp'), text);
	}
	for (const text of ['TEMPLATE <Types...>', 'EXTERN "C" EXTERN_PROCEDURE print(C_INT, ...)', 'CALL print(Values...)']) {
		assert.ok(scopesAt(text, '...').includes('punctuation.vararg-ellipses.cpp'), text);
	}
	assert.ok(!scopesAt('Value.Field', '.').includes('punctuation.vararg-ellipses.cpp'));
});

test('exception keywords reuse C++ scopes and ignore non-keyword occurrences', () => {
	for (const [word, scope] of [
		['TRY', 'keyword.control.try.cpp'],
		['CATCH', 'keyword.control.catch.cpp'],
		['THROW', 'keyword.control.throw.cpp'],
		['ENDTRY', 'keyword.control.try.cpp']
	]) {
		assert.ok(scopesAt(word, word).includes(scope));
		assert.ok(scopesAt(`// ${word}`, word).some(s => s.startsWith('comment.')));
		assert.ok(scopesAt(`OUTPUT "${word}"`, word).some(s => s.startsWith('string.')));
		for (const text of [word.toLowerCase(), `${word}Value`, `My${word}`, `${word}_1`]) {
			assert.ok(!scopesAt(text, text).some(s => s.startsWith('keyword.')), text);
		}
	}
});

test('comments, strings, lowercase and keyword prefixes cannot become keywords', () => {
	assert.ok(scopesAt('// IF [(', 'IF').some(s => s.startsWith('comment.')));
	assert.ok(scopesAt('OUTPUT "IF // ["', 'IF').some(s => s.startsWith('string.')));
	assert.ok(scopesAt('TYPE Native = C_TYPE("C_INT")', 'C_INT').some(s => s.startsWith('string.')));
	assert.ok(scopesAt('// C_TYPE("int")', 'C_TYPE').some(s => s.startsWith('comment.')));
	for (const text of ['c_int', 'C_INTValue', 'MyC_INT', 'C_TYPE_1']) {
		assert.ok(!scopesAt(text, text).some(s => s.startsWith('storage.')), text);
	}
	for (const text of ['if X then', 'IFValue <- 1', 'SET OF X', 'DEFINE X', 'RANDOM']) {
		assert.ok(!scopesAt(text, text.split(' ')[0]).some(
			s => /^(keyword|storage)\./.test(s)), text);
	}
	// An unfinished literal must not carry its string scope onto the next line.
	let state = grammar.tokenizeLine('OUTPUT "unfinished').ruleStack;
	assert.ok(grammar.tokenizeLine('IF X THEN', state).tokens[0].scopes.includes(
		'keyword.control.cie-pseudocode'));
});

test('literal escapes protect quotes and reuse C++ escape and suffix scopes', () => {
	for (const escape of [String.raw`\'`, String.raw`\"`, String.raw`\\`, String.raw`\?`,
		...['a', 'b', 'f', 'n', 'r', 't', 'v', '0', '101', 'x4142', 'u0041', 'U0001F600']
			.map(value => `\\${value}`)]) {
		const text = `OUTPUT "${escape} IF // [" + 1`;
		assert.ok(scopesAt(text, escape).includes('constant.character.escape'), escape);
		assert.ok(scopesAt(text, 'IF').includes('string.quoted.double.cie-pseudocode'), escape);
		assert.ok(scopesAt(text, '+').includes('keyword.operator.cie-pseudocode'), escape);
	}
	assert.ok(scopesAt(String.raw`OUTPUT '\'' + 1`, '+').includes('keyword.operator.cie-pseudocode'));
	assert.ok(scopesAt(String.raw`OUTPUT '\x41'`, String.raw`\x41`).includes('constant.character.escape'));
	assert.ok(scopesAt('"text"C', 'C').includes('keyword.other.suffix.literal.user-defined.string.cpp'));
	for (const text of ['"text" C', '"text"c', "'x'C"]) {
		assert.ok(!scopesAt(text, text.at(-1)).includes('keyword.other.suffix.literal.user-defined.string.cpp'));
	}
	for (const text of ['OUTPUT "unfinished', 'OUTPUT "unfinished\\', "OUTPUT '''"]) {
		const state = grammar.tokenizeLine(text).ruleStack;
		assert.ok(grammar.tokenizeLine('IF Ready THEN', state).tokens[0].scopes.includes('keyword.control.cie-pseudocode'));
	}
});
