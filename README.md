# Ocrib: Remote Hardware Download Proxy

Ocrib is a streamlined, global platform that allows you to trigger file downloads on any of your devices from anywhere in the world. Simply log in, link your device with a one-time PIN, and paste a link. Your device handles the rest.

## 🚀 Key Features

- **Zero-Config Pairing:** Link new devices in seconds using a simple 6-digit PIN.
- **Global Reach:** Host the broker in the cloud and access your devices from anywhere.
- **Efficient Agent:** Lightweight C-based agent with minimal footprint and high performance.
- **Docker Ready:** Deploy the entire server stack with a single command.
- **Modern UI:** intuitive dashboard for managing devices and tracking history.

---

## 🛠️ Getting Started (Cloud / Server)

If you want to host the Ocrib platform itself:

### Option A: Using Docker (Recommended)
1. Install [Docker](https://docs.docker.com/get-docker/) and [Docker Compose](https://docs.docker.com/compose/install/).
2. Run the platform:
   ```bash
   make docker-up
   ```
3. Access your dashboard at `http://your-server-ip:5000`.

### Option B: Running Locally
1. Ensure you have Python 3.x installed.
2. Initialize the database:
   ```bash
   make setup
   ```
3. Start the broker:
   ```bash
   make broker
   ```

---

## 📡 Setting Up a Receiver (The Agent)

On the device where you want to receive files:

1. Download the source or the `install.sh` script.
2. Run the installer:
   ```bash
   ./install.sh
   ```
3. Start the agent by pointing it to your server:
   ```bash
   ./remote_agent <your-server-ip-or-domain>
   ```
4. Note the **6-digit PIN** displayed in the terminal.
5. Log into the web dashboard and enter this PIN under "Link New Device".

---

## 📖 Architecture

- **Broker (Python):** The central hub. It serves the web dashboard and maintains persistent TCP connections with all active agents.
- **Agent (C):** A lightweight daemon that sits on the receiver device. It waits for instructions and uses `libcurl` to stream files directly to disk.
- **Database (SQLite):** Stores user accounts, device tokens, and download history.

## 🔒 Security

- **Authentication:** Protected user accounts and device pairing tokens.
- **Encryption:** (Recommendation) Use a reverse proxy like Nginx with SSL/TLS (Certbot) to encrypt all traffic to the dashboard and between agents.

## 📜 License
This project is provided as-is for personal and professional remote file management.
