'use strict';

// Preserve UTF-16 offsets and newlines, masking CIE quotes and // comments.
// Unlike C++, CIE has no escapes or multiline literals.
function codeOnly(text) {
	let quote = null;
	let comment = false;
	const result = [];
	for (let i = 0; i < text.length; i++) {
		const char = text[i];
		if (char === '\n' || char === '\r') {
			quote = null;
			comment = false;
			result.push(char);
		} else if (comment) {
			result.push(' ');
		} else if (quote) {
			if (char === quote) quote = null;
			result.push(' ');
		} else if (char === '"' || char === "'") {
			quote = char;
			result.push(' ');
		} else if (char === '/' && text[i + 1] === '/') {
			comment = true;
			result.push(' ');
		} else {
			result.push(char);
		}
	}
	return result.join('');
}

module.exports = { codeOnly };
