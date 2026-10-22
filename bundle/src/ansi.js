// Splits terminal text into runs styled by SGR escape sequences, keeping the
// style and any sequence cut off at a chunk boundary for the next chunk.
// Handles reset, bold and the 16 foreground colors; drops other sequences.
const names = ['Black', 'Red', 'Green', 'Yellow', 'Blue', 'Magenta', 'Cyan', 'White'];
const sequence = /\x1b\[([0-?]*)[ -/]*([@-~])/g;
const unfinished = /\x1b(\[[0-?]*[ -/]*)?$/;

export function createAnsiParser() {
	let bold = false;
	let color = null;
	let rest = '';
	return text => {
		text = rest + text;
		const cut = text.match(unfinished);
		rest = cut ? cut[0] : '';
		if (cut) text = text.slice(0, cut.index);
		const runs = [];
		let start = 0;
		for (const match of text.matchAll(sequence)) {
			if (match.index > start) runs.push({ text: text.slice(start, match.index), bold, color });
			start = match.index + match[0].length;
			if (match[2] !== 'm') continue;
			for (const parameter of match[1].split(';')) {
				const code = Number(parameter || 0);
				if (code === 0) { bold = false; color = null; }
				else if (code === 1) bold = true;
				else if (code === 22) bold = false;
				else if (code === 39) color = null;
				else if (code >= 30 && code <= 37) color = `terminal.ansi${names[code - 30]}`;
				else if (code >= 90 && code <= 97) color = `terminal.ansiBright${names[code - 90]}`;
			}
		}
		if (start < text.length) runs.push({ text: text.slice(start), bold, color });
		return runs;
	};
}
