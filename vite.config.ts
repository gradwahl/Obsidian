import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const projectRoot = fileURLToPath(new URL('.', import.meta.url));

export default defineConfig({
  plugins: [
    tailwindcss(),
    react(),
    {
      name: 'bundle-static-artwork',
      apply: 'build',
      buildStart() {
        // Include only versioned artwork, never local downloaded caches.
        const files = execFileSync('git', ['ls-files', '-z', '--', 'assets'], {
          cwd: projectRoot,
          encoding: 'utf8'
        }).split('\0').filter(Boolean);
        for (const fileName of files) {
          this.emitFile({
            type: 'asset',
            fileName,
            source: fs.readFileSync(path.join(projectRoot, fileName))
          });
        }
      }
    },
    {
      name: 'serve-covers-and-assets',
      configureServer(server) {
        server.middlewares.use((req, res, next) => {
          if (!req.url) return next();
          const cleanUrl = req.url.split('?')[0];

          // Handle .cover files
          if (cleanUrl.startsWith('/assets/covers/')) {
            const fileName = path.basename(cleanUrl);
            const filePath = path.join(process.cwd(), 'assets', 'covers', fileName);
            if (fs.existsSync(filePath)) {
              try {
                const buffer = fs.readFileSync(filePath);
                // Check magic bytes: \xff\xd8\xff is JPEG, \x89PNG is PNG
                const isPng = buffer[0] === 0x89 && buffer[1] === 0x50 && buffer[2] === 0x4e && buffer[3] === 0x47;
                res.setHeader('Content-Type', isPng ? 'image/png' : 'image/jpeg');
                res.setHeader('Cache-Control', 'public, max-age=86400');
                res.end(buffer);
                return;
              } catch (err) {
                console.error('Error serving cover:', err);
              }
            }
          }

          // Handle library artwork
          if (cleanUrl.startsWith('/assets/library/')) {
            const relPath = cleanUrl.replace(/^\/assets\/library\//, '');
            const filePath = path.join(process.cwd(), 'assets', 'library', relPath);
            if (fs.existsSync(filePath)) {
              try {
                const buffer = fs.readFileSync(filePath);
                res.setHeader('Content-Type', cleanUrl.endsWith('.json') ? 'application/json' : 'image/png');
                res.setHeader('Cache-Control', 'public, max-age=86400');
                res.end(buffer);
                return;
              } catch (err) {
                console.error('Error serving library asset:', err);
              }
            }
          }

          next();
        });
      }
    }
  ],
  server: {
    host: '0.0.0.0',
    port: 3000
  }
});
