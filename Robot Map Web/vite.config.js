import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      "/pos": {
        target: "http://10.10.10.55",
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/pos/, ""),
      },
      "/robot": {
        target: "http://10.10.10.50",
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/robot/, ""),
      },
    },
  },
});
