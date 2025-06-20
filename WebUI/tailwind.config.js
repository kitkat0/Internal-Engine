/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'ce-bg': '#1e1e1e',
        'ce-bg-light': '#2d2d30',
        'ce-border': '#3e3e42',
        'ce-text': '#cccccc',
        'ce-accent': '#007acc',
        'ce-hover': '#094771',
        'ce-success': '#4ec9b0',
        'ce-error': '#f48771',
      }
    },
  },
  plugins: [],
}