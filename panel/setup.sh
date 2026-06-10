#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER="$ROOT/server"
CLIENT="$ROOT/client"

echo "[setup] Installing server dependencies..."
cd "$SERVER" && bun install

echo "[setup] Installing client dependencies..."
cd "$CLIENT" && npm install

echo "[setup] Running database migrations..."
cd "$SERVER"
DB_FILE_NAME=./src/database/cubaware.sqlite bunx drizzle-kit migrate

echo "[setup] Seeding database..."
DB_FILE_NAME=./src/database/cubaware.sqlite bun run seed

echo "[setup] Done. Default login: admin / admin — change the password immediately."
