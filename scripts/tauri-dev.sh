#!/usr/bin/env bash
# Start Tauri dev after killing all old instances.
# Usage: ./scripts/tauri-dev.sh

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "======================================"
echo "  trun — Tauri Dev (clean start)"
echo "======================================"

# Kill old processes from project root
cd "$PROJECT_ROOT"
./scripts/dev-start.sh

# Start Nuxt dev in background
cd "$PROJECT_ROOT/frontend"
echo "🚀 Starting Nuxt dev server..."
npx nuxi dev --port 3000 &
NUXT_PID=$!

# Wait for Nuxt to be ready
echo "⏳ Waiting for Nuxt to be ready..."
for i in $(seq 1 30); do
  if lsof -ti :3000 > /dev/null 2>&1; then
    echo "✅ Nuxt is ready (PID: $NUXT_PID)"
    break
  fi
  sleep 1
done

# Start Tauri
cd "$PROJECT_ROOT/src-tauri"
echo "🚀 Starting Tauri dev..."
exec tauri dev
