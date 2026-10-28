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

function scopesAt(text, word, state) {
	const offset = text.indexOf(word);
	return grammar.tokenizeLine(text, state).tokens.find(
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
		'ASC', 'CHR', 'IS_NUM', 'NUM_TO_STR', 'STR_TO_NUM', 'EOF', 'MALLOC', 'FREE', 'REALLOC']) {
		assert.ok(scopesAt(`${name}(X)`, name).includes('entity.name.function.cie-pseudocode'));
	}
});

test('function names include underscores and explicit nested template arguments', () => {
	for (const text of ['CALL _print(1)', 'FUNCTION _value() RETURNS INTEGER', 'FUNCTION _value RETURNS INTEGER',
		'OUTPUT _value<Box<Pair<INTEGER, REAL>>>(X)', 'OUTPUT _value <Box<Types...>> (X)']) {
		const name = text.includes('_print') ? '_print' : '_value';
		assert.ok(scopesAt(text, name).includes('entity.name.function.cie-pseudocode'), text);
	}
	const text = 'OUTPUT _value<Box<Pair<INTEGER, REAL>>>(X)';
	for (const name of ['Box', 'Pair']) assert.ok(scopesAt(text, name).includes('entity.name.type.cpp'));
	for (const name of ['INTEGER', 'REAL']) assert.ok(scopesAt(text, name).includes('storage.type.cie-pseudocode'));
	assert.ok(scopesAt(text, '<').includes('punctuation.section.angle-brackets.begin.template.call.cpp'));
	assert.ok(scopesAt(text, '>').includes('punctuation.section.angle-brackets.end.template.call.cpp'));
	for (const expression of ['A < B AND C > D', 'A < B + C > (D)', 'A <= B', 'A <> B']) {
		assert.ok(scopesAt(expression, '<').includes('keyword.operator.cie-pseudocode'), expression);
		assert.ok(!scopesAt(expression, 'B').includes('entity.name.type.cpp'), expression);
	}
	const configuration = JSON.parse(fs.readFileSync(path.join(__dirname, '../language-configuration.json')));
	assert.equal('_value'.match(new RegExp(configuration.wordPattern))[0], '_value');
});

test('default return modifiers preserve builtin and nested result type scopes', () => {
	for (const [text, names] of [
		['OVERLOAD FUNCTION Foo(X : STRING) RETURNS DEFAULT REAL', ['REAL']],
		['PUBLIC OVERLOAD FUNCTION Foo() RETURNS DEFAULT Box<Pair<Widget, REAL>>', ['Box', 'Pair', 'Widget', 'REAL']]
	]) {
		assert.ok(scopesAt(text, 'DEFAULT').includes('storage.modifier.cpp'));
		assert.ok(!scopesAt(text, 'DEFAULT').includes('entity.name.type.cpp'));
		for (const name of names) {
			assert.ok(scopesAt(text, name).some(s => /^(entity\.name\.type|storage\.type)\./.test(s)), `${text}: ${name}`);
		}
	}
	assert.ok(!scopesAt('OUTPUT "DEFAULT"', 'DEFAULT').includes('storage.modifier.cpp'));
	assert.ok(!scopesAt('// DEFAULT', 'DEFAULT').includes('storage.modifier.cpp'));
	assert.ok(!scopesAt('default()', 'default').includes('storage.modifier.cpp'));
});

test('types are recognized in declarations, signatures, arrays, aliases, constructors and templates', () => {
	for (const [text, names] of [
		['TYPE Student', ['Student']],
		['CLASS Student', ['Student']],
		['TYPE Native = C_TYPE("unsigned int")', ['Native']],
		['TYPE NodePointer = ^Node', ['NodePointer', 'Node']],
		['DECLARE Value : Widget', ['Widget']],
		['DECLARE Values : ARRAY[Low:High, 1:Limit] OF Box<Widget>', ['Box', 'Widget']],
		['PROCEDURE Work(BYREF Value : Widget, Rest : ARRAY OF Node)', ['Widget', 'Node']],
		['FUNCTION Work(Value : Widget) RETURNS Result', ['Widget', 'Result']],
		['FUNCTION Work() RETURNS void', ['void']],
		['EXTERN "C" EXTERN_FUNCTION print(Native, RawPointer, ...) RETURNS NativeResult', ['Native', 'RawPointer', 'NativeResult']],
		['Value <- NEW Box<Widget>()', ['Box', 'Widget']],
		['TEMPLATE <T, Rest...>', ['T', 'Rest']]
	]) for (const name of names) {
		assert.ok(scopesAt(text, name).some(s => /^(entity\.name\.type|storage\.type)\./.test(s)), `${text}: ${name}`);
	}
	for (const [text, word] of [
		['CASE OF Selector', 'Selector'], ['Label : OUTPUT Value', 'OUTPUT'],
		['DECLARE Values : ARRAY[Low:High, Indices[Start]:Limit] OF Widget', 'High'],
		['DECLARE Values : ARRAY[Low:High, Indices[Start]:Limit] OF Widget', 'Limit'],
		['OUTPUT (Widget)Value', 'Widget'], ['OUTPUT Value.Field', 'Field'],
		['// DECLARE Value : Widget', 'Widget'], ['OUTPUT "DECLARE Value : Widget"', 'Widget']
	]) assert.ok(!scopesAt(text, word).includes('entity.name.type.cpp'), `${text}: ${word}`);
	let state = grammar.tokenizeLine('PROCEDURE Work(').ruleStack;
	assert.ok(scopesAt('Value : Widget,', 'Widget', state).includes('entity.name.type.cpp'));
	state = grammar.tokenizeLine('Value : Widget,', state).ruleStack;
	state = grammar.tokenizeLine('Other : ARRAY[Low:High] OF Node)', state).ruleStack;
	assert.ok(!scopesAt('Label : OUTPUT Value', 'OUTPUT', state).includes('entity.name.type.cpp'));
});

test('class fields have type scopes and this is confined to method code', () => {
	let state = grammar.tokenizeLine('CLASS Counter').ruleStack;
	for (const text of ['PRIVATE Value : Widget', 'PUBLIC Other : Widget', 'Field : Widget']) {
		assert.ok(scopesAt(text, 'Widget', state).includes('entity.name.type.cpp'));
		state = grammar.tokenizeLine(text, state).ruleStack;
	}
	assert.ok(!scopesAt('Field : this', 'this', state).includes('variable.language.this.cpp'));
	state = grammar.tokenizeLine('PUBLIC PROCEDURE Update(Other : Widget)', state).ruleStack;
	for (const word of ['this', 'This', 'THIS']) {
		assert.ok(scopesAt(`${word}.Value <- Other`, word, state).includes('variable.language.this.cpp'));
	}
	for (const text of ['thisValue', '_this', 'this_1', '// this', 'OUTPUT "this"']) {
		assert.ok(!scopesAt(text, 'this', state).includes('variable.language.this.cpp'), text);
	}
	assert.ok(!scopesAt('Label : OUTPUT Value', 'OUTPUT', state).includes('entity.name.type.cpp'));
	assert.ok(scopesAt('DECLARE Copy : Widget(this.Value)', 'this', state).includes('variable.language.this.cpp'));
	state = grammar.tokenizeLine('ENDPROCEDURE', state).ruleStack;
	assert.ok(!scopesAt('Field : this', 'this', state).includes('variable.language.this.cpp'));
	state = grammar.tokenizeLine('ENDCLASS', state).ruleStack;
	assert.ok(!scopesAt('this <- 1', 'this', state).includes('variable.language.this.cpp'));
	state = grammar.tokenizeLine('FUNCTION Outside() RETURNS INTEGER', state).ruleStack;
	assert.ok(!scopesAt('this <- 1', 'this', state).includes('variable.language.this.cpp'));
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

test('preprocessor directives reuse C++ scopes and shield switch names from ordinary keywords', () => {
	for (const [word, scope] of [
		['PP_DEFINE', 'keyword.control.directive.define.cpp'],
		['PP_UNDEF', 'keyword.control.directive.undef.cpp'],
		['PP_IFDEF', 'keyword.control.directive.conditional.cpp'],
		['PP_IFNDEF', 'keyword.control.directive.conditional.cpp'],
		['PP_ENDIF', 'keyword.control.directive.conditional.cpp']
	]) {
		assert.ok(scopesAt(`\t${word} __LIB // comment`, word).includes(scope));
		for (const text of [word.toLowerCase(), `${word}Value`, `_${word}`, `X ${word}`, `// ${word}`, `OUTPUT "${word}"`]) {
			assert.ok(!scopesAt(text, word.toLowerCase() === text ? text : word).includes(scope), text);
		}
	}
	for (const word of ['__LIB', 'FUNCTION', 'INTEGER']) {
		assert.ok(scopesAt(`PP_DEFINE ${word}`, word).includes('entity.name.function.preprocessor.cpp'));
	}
	assert.ok(scopesAt('PP_IFDEF Flag // PP_ENDIF', 'PP_ENDIF').some(s => s.startsWith('comment.')));
	assert.ok(!scopesAt('#LIB', 'LIB').some(s => s.startsWith('keyword.control.directive.')));
});

test('ellipses use the C++ variadic scope', () => {
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
