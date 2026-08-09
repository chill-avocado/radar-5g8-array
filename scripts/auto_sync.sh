#!/bin/bash
# Auto-sync background daemon for radar-5g8-array
# Rules:
# 1. Local changes automatically commit & push to GitHub.
# 2. Remote additions (e.g. from a cloud agent) are pulled locally.
# 3. Protection: Remote changes CANNOT overwrite or delete existing local files.

export PATH="/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin:$PATH"
REPO_DIR="/Users/lucasnaylor/radar-5g8-array"
cd "$REPO_DIR" || exit 1

while true; do
  # Step 1: Local -> Remote (Push local work)
  if [ -n "$(git status --porcelain)" ]; then
    git add -A
    git commit -m "Auto-update: $(date '+%Y-%m-%d %H:%M:%S')" --quiet
    git push origin main --quiet 2>&1
  else
    # Step 2: Remote -> Local (Safe additions-only sync)
    git fetch origin main --quiet 2>&1
    
    # Find files added on remote relative to local HEAD
    new_remote_files=$(git diff --name-only --diff-filter=A HEAD..origin/main 2>/dev/null)
    
    if [ -n "$new_remote_files" ]; then
      pulled=0
      for file in $new_remote_files; do
        # Only pull if file does NOT exist locally (never overwrite existing local files)
        if [ ! -e "$file" ]; then
          git checkout origin/main -- "$file" 2>/dev/null
          pulled=1
        fi
      done
      if [ $pulled -eq 1 ] && [ -n "$(git status --porcelain)" ]; then
        git add -A
        git commit -m "Auto-pull remote additions: $(date '+%Y-%m-%d %H:%M:%S')" --quiet
      fi
    fi
  fi
  sleep 5
done
