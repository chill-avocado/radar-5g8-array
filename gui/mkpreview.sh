#!/bin/sh
# radar-gui.html is the artifact body. claude.ai wraps it in a doctype/head/body
# skeleton at publish time; this rebuilds the same wrapper as index.html so the
# local copy renders identically (and with the right character set).
cd "$(dirname "$0")"
{
  printf '%s' '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>:root{color-scheme:light}body{margin:0;padding:0;font:14px -apple-system,BlinkMacSystemFont,sans-serif;background:#faf9f5;color:#141413}img{max-width:100%}</style></head><body>'
  cat radar-gui.html
  printf '%s' '</body></html>'
} > index.html
echo "index.html $(wc -c < index.html) bytes"
