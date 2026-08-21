#!/bin/sh
# Wrap the artifact body in the same skeleton claude.ai adds at publish time,
# so what is tested locally is what ships.
cd "$(dirname "$0")"
{
  printf '%s' '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>:root{color-scheme:light}body{margin:0;padding:0;font:14px -apple-system,BlinkMacSystemFont,sans-serif;background:#faf9f5;color:#141413}img{max-width:100%}</style></head><body>'
  cat radar-gui.html
  printf '%s' '</body></html>'
} > preview.html
echo "preview.html $(wc -c < preview.html) bytes"
