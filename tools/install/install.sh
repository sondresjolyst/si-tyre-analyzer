#!/usr/bin/env sh
# SI Tyre Analyzer installer (macOS / Linux).
# Installs uv if missing, then installs the app from the latest GitHub release.
set -eu
REPO="sondresjolyst/si-tyre-analyzer"

if ! command -v uv >/dev/null 2>&1; then
  echo "Installing uv..."
  curl -LsSf https://astral.sh/uv/install.sh | sh
  export PATH="$HOME/.local/bin:$PATH"
fi

echo "Finding the latest release..."
api="https://api.github.com/repos/$REPO/releases"
wheel_url=$(curl -fsSL "$api" \
  | grep -o '"browser_download_url": *"[^"]*\.whl"' \
  | head -n1 \
  | sed -E 's/.*"(https[^"]*)".*/\1/')
if [ -z "$wheel_url" ]; then
  echo "No app release wheel found on GitHub." >&2
  exit 1
fi

tmp="$(mktemp -d)/$(basename "$wheel_url")"
echo "Downloading $(basename "$wheel_url")..."
curl -fsSL "$wheel_url" -o "$tmp"

echo "Installing..."
uv tool install --force "$tmp"

echo "Done. Launch it with:  si-tyre-analyzer"
