#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER="$ROOT/server"
CLIENT="$ROOT/client"

if [[ ! -f "$SERVER/.env" ]]; then
    echo "[setup] Generating .env..."
    JWT_SECRET=$(openssl rand -hex 32)
    API_KEY=$(openssl rand -hex 20)
    ZIP_PASS=$(openssl rand -hex 16)
    CLIENT_DIR="$(cd "$ROOT/.." && pwd)/client/CubaClient/CubaClient"
    cat > "$SERVER/.env" <<EOF
PORT=3001
DB_FILE_NAME=./src/database/cubaware.sqlite
NEXT_PUBLIC_API_URL=http://localhost:3001
JWT_SECRET=$JWT_SECRET
CUBACLIENT_API_KEY=$API_KEY
CUBACLIENT_ZIP_PASS=$ZIP_PASS
CLIENT_DIR=$CLIENT_DIR
EOF
    echo "[setup] .env written."
    echo "[setup]   API key:  $API_KEY"
    echo "[setup]   ZIP pass: $ZIP_PASS"
fi

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
