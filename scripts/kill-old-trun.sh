#!/usr/bin/env bash
# Kill any existing Tauri/Trun/Nuxt instances before starting a new one.
# Usage: ./scripts/kill-old-trun.sh

set -euo pipefail

echo "🔍 Looking for existing trun/Tauri/Nuxt processes..."

# Kill Tauri CLI processes (node with "tauri dev")
tauri_pids=$(lsof -ti :3000 2>/dev/null || true)
if [ -n "$tauri_pids" ]; then
  echo "🚫 Killing Tauri dev server PIDs: $(echo $tauri_pids | tr ' ' ', ')"
  echo "$tauri_pids" | xargs kill -9 2>/dev/null || true
fi

# Kill any leftover Tauri app processes
app_pids=$(pgrep -f "target/debug/app" 2>/dev/null || true)
if [ -n "$app_pids" ]; then
  echo "🚫 Killing Tauri app PIDs: $(echo $app_pids | tr ' ' ', ')"
  echo "$app_pids" | xargs kill -9 2>/dev/null || true
fi

# Kill any Tauri CLI processes by name
cli_pids=$(pgrep -f "node.*tauri dev" 2>/dev/null || true)
if [ -n "$cli_pids" ]; then
  echo "🚫 Killing Tauri CLI PIDs: $(echo $cli_pids | tr ' ' ', ')"
  echo "$cli_pids" | xargs kill -9 2>/dev/null || true
fi

# Wait for port to clear
echo "⏳ Waiting for port 3000 to clear..."
for i in $(seq 1 10); do
  if ! lsof -ti :3000 > /dev/null 2>&1; then
    echo "✅ Port 3000 is free"
    break
  fi
  sleep 1
done

echo "✅ Cleanup complete. Ready to start fresh."
