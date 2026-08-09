#!/bin/bash
# Auto-sync background daemon for radar-5g8-array
# Detects local file changes, stages, commits, and pushes to GitHub automatically.

export PATH="/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin:$PATH"
REPO_DIR="/Users/lucasnaylor/radar-5g8-array"
cd "$REPO_DIR" || exit 1

while true; do
  if [ -n "$(git status --porcelain)" ]; then
    git add -A
    git commit -m "Auto-update: $(date '+%Y-%m-%d %H:%M:%S')" --quiet
    git push origin main --quiet 2>&1
  fi
  sleep 5
done
