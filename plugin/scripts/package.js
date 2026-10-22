'use strict';

const { execFileSync, spawnSync } = require('node:child_process');
const path = require('node:path');
const pluginDir = path.resolve(__dirname, '..');

try {
	const revision = execFileSync('git', ['log', '-1', '--format=%ct:%h', '--', '.', '../.github'], {
		cwd: pluginDir,
		encoding: 'utf8',
		stdio: ['ignore', 'pipe', 'pipe']
	}).trim();
	const match = /^(\d+):([0-9a-f]+)$/.exec(revision);
	if (!match) throw new Error('Cannot read the last plugin commit for the package version.');
	const timestamp = new Date(Number(match[1]) * 1000).toISOString();
	const date = timestamp.slice(0, 10).replace(/-/g, '');
	const time = timestamp.slice(11, 19).replace(/:/g, '');
	const commit = match[2];
	const version = `${date}.${Number(time)}.0+${commit}`;
	const filename = `cpsc-plugin-${date}-${time}-${commit}.vsix`;
	const result = spawnSync(process.execPath, [
		require.resolve('@vscode/vsce/vsce'), 'package', version,
		'--no-update-package-json', '--no-dependencies', '--out', filename
	], { cwd: pluginDir, stdio: 'inherit' });
	if (result.error) throw result.error;
	process.exitCode = result.status ?? 1;
} catch (error) {
	console.error(`Cannot package plugin: ${error.message}`);
	process.exitCode = 1;
}
