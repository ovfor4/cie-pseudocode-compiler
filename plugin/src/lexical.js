'use strict';

// Preserve UTF-16 offsets and newlines, masking CIE quotes and // comments.
// Backslash escapes protect the next character; literals end at each newline.
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
			result.push(' ');
			if (char === '\\' && i + 1 < text.length && !/[\r\n]/.test(text[i + 1])) {
				result.push(' ');
				i++;
			} else if (char === quote) quote = null;
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
