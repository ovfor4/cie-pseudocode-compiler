import { execFileSync } from 'node:child_process';
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { gzipSync } from 'node:zlib';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const target = execFileSync('rustc', ['--print', 'host-tuple'], { encoding: 'utf8' }).trim();
const extension = process.platform === 'win32' ? '.exe' : '';
const source = path.resolve(process.argv[2] || path.join(root, '..', 'build', `cpsc${extension}`));
const destination = path.join(root, 'src-tauri', 'binaries', process.platform === 'win32' ? 'cpsc.exe.gz' : `cpsc-${target}${extension}`);
mkdirSync(path.dirname(destination), { recursive: true });
if (process.platform === 'win32') {
	writeFileSync(destination, gzipSync(readFileSync(source)));
} else {
	copyFileSync(source, destination);
}
console.log(`Bundled compiler: ${source}`);
