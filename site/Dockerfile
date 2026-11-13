# Imagem do site para a Fly.io (tcc-autotune.fly.dev).
#
# O site é estático (HTML, CSS, JS e três mp3), então basta um nginx servindo
# arquivos. Copiamos só o que é página: o Dockerfile, o fly.toml e a config do
# nginx ficam fora da pasta servida, para ninguém baixar a infraestrutura.
#
# Por que Fly e não GitHub Pages: em 12/11/2026 a conta do GitHub estava com a
# cobrança bloqueada, e o Pages publica por GitHub Actions, que parou junto.
# Deploy:  cd site && fly deploy
FROM nginx:1.27-alpine

COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY index.html estilo.css idioma.js /usr/share/nginx/html/
COPY audio /usr/share/nginx/html/audio
