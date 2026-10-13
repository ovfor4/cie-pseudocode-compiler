'use strict';

const { codeOnly } = require('./lexical');
const pairs = { ')': '(', ']': '[' };

function unmatchedBrackets(text) {
	const code = codeOnly(text);
	const stack = [];
	const unmatched = [];
	for (let offset = 0; offset < code.length; offset++) {
		const char = code[offset];
		if (char === '(' || char === '[') {
			stack.push({ char, offset });
		} else if (pairs[char]) {
			const opening = stack.pop();
			if (!opening || opening.char !== pairs[char]) {
				if (opening) unmatched.push(opening.offset);
				unmatched.push(offset);
			}
		}
	}
	return unmatched.concat(stack.map(opening => opening.offset)).sort((a, b) => a - b);
}

module.exports = { unmatchedBrackets };
