import { defineConfig } from 'vite';
import commonjs from '@rollup/plugin-commonjs';

export default defineConfig({
	plugins: [commonjs({ include: /plugin\/src\// })],
	server: { port: 1420, strictPort: true },
	clearScreen: false,
	build: { target: 'es2022' }
});
