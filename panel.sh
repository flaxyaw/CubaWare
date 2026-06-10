#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER="$ROOT/panel/server"
CLIENT="$ROOT/panel/client"

# first-run: install deps and init DB if not set up yet
if [[ ! -d "$SERVER/node_modules" ]] || [[ ! -d "$CLIENT/node_modules" ]] || [[ ! -f "$SERVER/src/database/cubaware.sqlite" ]]; then
    echo "[panel] First run — running setup..."
    bash "$ROOT/panel/setup.sh"
fi

# optional: open libvirt firewall ports if running VMs on Fedora/RHEL
if command -v firewall-cmd &>/dev/null && systemctl is-active --quiet firewalld 2>/dev/null; then
    if ! firewall-cmd --zone=libvirt --query-port=3000/tcp --quiet 2>/dev/null; then
        echo "[panel] Tip: allow VM access:"
        echo "  sudo firewall-cmd --zone=libvirt --add-port=3000/tcp --add-port=3001/tcp --permanent && sudo firewall-cmd --reload"
    fi
fi

cd "$SERVER"
DB_FILE_NAME=./src/database/cubaware.sqlite bun run dev &
SERVER_PID=$!

sleep 4

cd "$CLIENT"
npm run dev &
CLIENT_PID=$!

trap "kill $SERVER_PID $CLIENT_PID 2>/dev/null" INT TERM EXIT
wait
