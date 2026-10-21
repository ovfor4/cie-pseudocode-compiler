import { defineConfig } from 'vite';
import commonjs from '@rollup/plugin-commonjs';
import { readFileSync } from 'node:fs';
import configuration from './src-tauri/tauri.conf.json';

export default defineConfig({
	plugins: [commonjs({ include: /plugin\/src\// }), {
		name: 'bundled-notices',
		generateBundle() {
			// Frontend assets are embedded in the portable Windows executable too.
			for (const [source, fileName] of Object.entries(configuration.bundle.resources)) {
				this.emitFile({ type: 'asset', fileName, source: readFileSync(new URL(`./src-tauri/${source}`, import.meta.url)) });
			}
		}
	}],
	server: { port: 1420, strictPort: true },
	clearScreen: false,
	build: { target: 'es2022' }
});
