// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import tailwindcss from '@tailwindcss/vite'
import { compression } from 'vite-plugin-compression2'

export default defineConfig(({ mode }) => {
  const isGithub = mode === 'github'

  return {
    plugins: [
      svelte(),
      tailwindcss(),
      ...(!isGithub
        ? [
            // gzip assets at build time
            compression({
              algorithms: ['gzip'],
              deleteOriginalAssets: true,
            }),
          ]
        : []),
    ],

    base: isGithub ? `/${process.env.GITHUB_REPOSITORY?.split('/')[1]}/` : '/',

    build: {
      outDir: 'dist',
      ...(isGithub
        ? {}
        : {
            // Put all files at the same place
            assetsDir: '',
            // Inline all assets (for single file)
            assetsInlineLimit: 100000000,
            // Remove hash in file names
            rollupOptions: {
              output: {
                entryFileNames: `[name].js`,
                assetFileNames: `[name].[ext]`,
              },
            },
            // sourcemap: true, // This generates .map files, needs to be gzipped manually
          }),
    },
    resolve: {
      alias: {
        src: '/src',
        $lib: '/$lib',
      },
    },
  }
})
