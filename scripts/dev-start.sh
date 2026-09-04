#!/usr/bin/env bash
# CRITICAL: Kill ALL trun/Nuxt/Tauri processes before starting.
# Multiple Nuxt instances serve stale content — you MUST kill everything first.
#
# Usage (from project root):
#   ./scripts/dev-start.sh       # Kill ALL old instances
#   cd frontend && npx nuxi dev --port 3000  # Then start fresh
#   cd src-tauri && tauri dev    # Then start Tauri

set -euo pipefail

echo "🔍 Killing ALL trun/Nuxt/Tauri processes..."

KILLED=()

# 1. Kill EVERYTHING on port 3000 — force kill, no mercy
PORT3000_PIDS=$(lsof -ti :3000 2>/dev/null || true)
if [ -n "$PORT3000_PIDS" ]; then
  echo "🚫 Port 3000 PIDs: $(echo $PORT3000_PIDS | tr '\n' ', ' | sed 's/,$//')"
  kill -9 $PORT3000_PIDS 2>/dev/null || true
  KILLED+=($PORT3000_PIDS)
fi

# 2. Kill Tauri app binary
APP_PIDS=$(pgrep -f "target/debug/trun" 2>/dev/null || true)
if [ -n "$APP_PIDS" ]; then
  echo "🚫 Tauri app PIDs: $(echo $APP_PIDS | tr '\n' ', ' | sed 's/,$//')"
  kill -9 $APP_PIDS 2>/dev/null || true
  KILLED+=($APP_PIDS)
fi

# 3. Kill Tauri CLI
TAURI_CLI_PIDS=$(pgrep -f "tauri dev" 2>/dev/null | grep -v "nuxt" | grep -v "PhpStorm" || true)
if [ -n "$TAURI_CLI_PIDS" ]; then
  echo "🚫 Tauri CLI PIDs: $(echo $TAURI_CLI_PIDS | tr '\n' ', ' | sed 's/,$//')"
  kill -9 $TAURI_CLI_PIDS 2>/dev/null || true
  KILLED+=($TAURI_CLI_PIDS)
fi

# 4. Kill ALL nuxt processes (aggressive)
NUXT_PIDS=$(pgrep -f "nuxt" 2>/dev/null | grep -v "PhpStorm" | grep -v "^$$" || true)
if [ -n "$NUXT_PIDS" ]; then
  echo "🚫 Nuxt PIDs: $(echo $NUXT_PIDS | tr '\n' ', ' | sed 's/,$//')"
  kill -9 $NUXT_PIDS 2>/dev/null || true
  KILLED+=($NUXT_PIDS)
fi

# 5. Kill trun-related node processes
PROJECT_NODE_PIDS=$(pgrep -f "trun" 2>/dev/null | grep -v "PhpStorm" | grep -v "^$$" || true)
if [ -n "$PROJECT_NODE_PIDS" ]; then
  echo "🚫 Trun node PIDs: $(echo $PROJECT_NODE_PIDS | tr '\n' ', ' | sed 's/,$//')"
  kill -9 $PROJECT_NODE_PIDS 2>/dev/null || true
  KILLED+=($PROJECT_NODE_PIDS)
fi

if [ ${#KILLED[@]} -eq 0 ]; then
  echo "✅ No existing processes found."
else
  echo "📊 Killed ${#KILLED[@]} process(es)"
fi

# Double-check: kill anything STILL on port 3000
if lsof -ti :3000 > /dev/null 2>&1; then
  echo "⚠️  Port 3000 still occupied — nuking everything..."
  lsof -ti :3000 2>/dev/null | xargs kill -9 2>/dev/null || true
fi

# Wait for port to be FREE
echo "⏳ Waiting for port 3000 to be free..."
for i in $(seq 1 20); do
  if ! lsof -ti :3000 > /dev/null 2>&1; then
    echo "✅ Port 3000 is FREE"
    break
  fi
  sleep 1
done

if lsof -ti :3000 > /dev/null 2>&1; then
  echo "⚠️  WARNING: Port 3000 still occupied after 20s"
  lsof -i :3000 2>&1
  exit 1
fi
