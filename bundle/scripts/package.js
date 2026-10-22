import { execFileSync } from 'node:child_process';
import { copyFileSync, mkdirSync, readdirSync, readFileSync, rmSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));

try {
	const repository = path.resolve(root, '..');
	const compilerPaths = readFileSync(path.join(repository, 'cmake/CompilerPaths.txt'), 'utf8').split('\n').map(line => line.trim()).filter(Boolean);
	const revision = execFileSync('git', ['-C', repository, 'log', '-1', '--format=%ct:%h', '--', ...compilerPaths, 'bundle', 'plugin'], { encoding: 'utf8' }).trim();
	const match = /^(\d+):([0-9a-f]+)$/.exec(revision);
	if (!match) throw new Error('Cannot read the last Bundle commit for the package name.');
	const timestamp = new Date(Number(match[1]) * 1000).toISOString();
	const date = timestamp.slice(0, 10).replace(/-/g, '');
	const time = timestamp.slice(11, 19).replace(/:/g, '');
	console.log(`Bundle version: ${date}-${time}-${match[2]}`);
	const platform = {
		win32: { architecture: 'x64', name: 'Windows-x86_64', extension: '.exe' },
		darwin: { architecture: 'arm64', name: 'Darwin-arm64', extension: '.tar.xz' }
	}[process.platform];
	if (!platform || process.arch !== platform.architecture) {
		throw new Error('Release packages are built on Windows x64 and macOS arm64 only. Use tauri build --no-bundle for Linux.');
	}
	execFileSync(process.execPath, [path.join(root, 'node_modules/@tauri-apps/cli/tauri.js'), 'build',
		...(process.platform === 'win32' ? ['--no-bundle'] : ['--bundles', 'app'])], {
		cwd: root, stdio: 'inherit',
		env: { ...process.env, CPSC_BUNDLE_TIME: `${date}-${time}`, CPSC_BUNDLE_COMMIT: match[2] }
	});
	const artifacts = path.join(root, 'artifacts');
	rmSync(artifacts, { recursive: true, force: true });
	mkdirSync(artifacts, { recursive: true });
	const name = `cpsc-bundle-${date}-${time}-${match[2]}-${platform.name}${platform.extension}`;
	if (process.platform === 'win32') {
		copyFileSync(path.join(root, 'src-tauri/target/release/cpsc-bundle.exe'), path.join(artifacts, name));
	} else {
		const built = path.join(root, 'src-tauri/target/release/bundle/macos');
		const apps = readdirSync(built).filter(name => name.endsWith('.app'));
		if (apps.length !== 1) throw new Error(`Expected one .app, found ${apps.length}.`);
		execFileSync('tar', ['-cJf', path.join(artifacts, name), '-C', built, apps[0]], { stdio: 'inherit' });
	}
	console.log(`Packaged: ${name}`);
} catch (error) {
	console.error(`Cannot package Bundle: ${error.message}`);
	process.exitCode = 1;
}
