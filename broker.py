import http.server
import socketserver
import socket
import threading
import sqlite3
import json
import urllib.parse
import uuid
import os
from enum import Enum

class DeviceStatus(str, Enum):
    """Explicit lifecycle state machine for remote agent device connectivity."""
    ONLINE = "online"
    OFFLINE = "offline"

# Configuration from environment variables
WEB_PORT = int(os.environ.get("WEB_PORT", 5000))
TCP_PORT = int(os.environ.get("TCP_PORT", 8080))
DB_PATH = os.environ.get("DB_PATH", "platform.db")
BIND_ADDR = os.environ.get("BIND_ADDR", "0.0.0.0")

active_connections = {}
pending_pairings = {}

class ThreadedTCPServer:
    def __init__(self, host, port):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((host, port))
        self.server.listen(5)

    def start(self):
        print(f"Device TCP Server active on {BIND_ADDR}:{TCP_PORT}")
        while True:
            try:
                conn, addr = self.server.accept()
                threading.Thread(target=self.handle_client, args=(conn,), daemon=True).start()
            except Exception as e:
                print(f"TCP Accept error: {e}")

    def handle_client(self, conn):
        token = ""
        pin = ""
        try:
            conn.settimeout(60) # 1 minute timeout for initial handshake
            data = conn.recv(1024).decode("utf-8").strip()

            if data.startswith("PIN:"):
                # Format: PIN:123456|NAME:hostname
                parts = data.split("|")
                pin = parts[0].split(":")[1]
                hostname = parts[1].split(":")[1] if len(parts) > 1 else "Unknown Device"
                
                pending_pairings[pin] = {"socket": conn, "hostname": hostname}
                print(f"Device waiting for PIN verification: {pin} ({hostname})")

                # Keep connection alive while waiting for pairing
                conn.settimeout(300) # 5 minutes for pairing
                while pin in pending_pairings:
                    heartbeat = conn.recv(1024)
                    if not heartbeat:
                        break
                return

            elif data.startswith("TOKEN:"):
                token = data.split(":")[1]
                db = sqlite3.connect(DB_PATH)
                cursor = db.cursor()
                cursor.execute(
                    "SELECT device_name FROM devices WHERE hardware_token=?", (token,)
                )
                if cursor.fetchone():
                    active_connections[token] = conn
                    cursor.execute(
                        "UPDATE devices SET status='online' WHERE hardware_token=?",
                        (token,),
                    )
                    db.commit()
                    db.close()

                    print(f"Device reconnected: {token}")
                    
                    conn.settimeout(None) # Infinite timeout for active connection
                    while True:
                        if not conn.recv(1024):
                            break
                else:
                    db.close()
                    conn.send("ERROR:INVALID_TOKEN".encode())

        except Exception as e:
            pass
        finally:
            if token in active_connections:
                del active_connections[token]
            if pin in pending_pairings:
                del pending_pairings[pin]
            
            try:
                db = sqlite3.connect(DB_PATH)
                if token:
                    db.execute(
                        "UPDATE devices SET status='offline' WHERE hardware_token=?",
                        (token,),
                    )
                    db.commit()
                db.close()
            except:
                pass
            conn.close()


class WebHandler(http.server.SimpleHTTPRequestHandler):
    def send_json(self, status, data):
        self.send_response(status)
        self.send_header("Content-type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*") # Allow global access
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        parsed_path = urllib.parse.urlparse(self.path)

        if parsed_path.path == "/":
            self.path = "/index.html"
            return http.server.SimpleHTTPRequestHandler.do_GET(self)

        if parsed_path.path == "/api/dashboard":
            query = urllib.parse.parse_qs(parsed_path.query)
            user_id = query.get("user_id", [None])[0]
            if not user_id:
                return self.send_json(400, {"error": "Missing user ID"})

            try:
                db = sqlite3.connect(DB_PATH)
                cursor = db.cursor()
                cursor.execute(
                    "SELECT hardware_token, device_name, status, is_locked FROM devices WHERE user_id=?",
                    (user_id,),
                )
                devices = [
                    {"token": r[0], "name": r[1], "status": r[2], "is_locked": bool(r[3])}
                    for r in cursor.fetchall()
                ]

                cursor.execute(
                    """
                    SELECT d.device_name, h.download_link, h.timestamp 
                    FROM history h
                    JOIN devices d ON h.hardware_token = d.hardware_token
                    WHERE d.user_id=? ORDER BY h.id DESC LIMIT 10
                """,
                    (user_id,),
                )
                history = [
                    {"name": r[0], "link": r[1], "time": r[2]}
                    for r in cursor.fetchall()
                ]
                db.close()
                return self.send_json(200, {"devices": devices, "history": history})
            except Exception as e:
                return self.send_json(500, {"error": f"Database error: {e}"})

        return http.server.SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length).decode("utf-8")
        if not post_data:
            return self.send_json(400, {"error": "Empty payload"})

        try:
            data = json.loads(post_data)
        except json.JSONDecodeError:
            return self.send_json(400, {"error": "Invalid JSON"})

        db = sqlite3.connect(DB_PATH)
        cursor = db.cursor()

        if self.path == "/api/login":
            cursor.execute(
                "SELECT id, username FROM users WHERE (username=? OR email=?) AND password=?",
                (data.get("login_id"), data.get("login_id"), data.get("password")),
            )
            user = cursor.fetchone()
            if user:
                self.send_json(200, {"id": user[0], "username": user[1]})
            else:
                self.send_json(401, {"error": "Invalid credentials"})

        elif self.path == "/api/register":
            try:
                cursor.execute(
                    "INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
                    (data.get("username"), data.get("email"), data.get("password")),
                )
                db.commit()
                self.send_json(200, {"message": "Account created"})
            except sqlite3.IntegrityError:
                self.send_json(400, {"error": "Username or Email already exists"})

        elif self.path == "/api/forgot":
            email = data.get("email")
            cursor.execute("SELECT id FROM users WHERE email=?", (email,))
            if cursor.fetchone():
                reset_token = str(uuid.uuid4())[:8] # Shorter token for ease of use
                cursor.execute(
                    "UPDATE users SET reset_token=? WHERE email=?", (reset_token, email)
                )
                db.commit()
                print(f"\n[SECURITY] Password reset token for {email}: {reset_token}\n")
            self.send_json(
                200, {"message": "If the email exists, a token was generated."}
            )

        elif self.path == "/api/reset":
            token = data.get("token")
            new_password = data.get("password")
            cursor.execute("SELECT id FROM users WHERE reset_token=?", (token,))
            if cursor.fetchone():
                cursor.execute(
                    "UPDATE users SET password=?, reset_token=NULL WHERE reset_token=?",
                    (new_password, token),
                )
                db.commit()
                self.send_json(200, {"message": "Password updated successfully"})
            else:
                self.send_json(400, {"error": "Invalid token"})

        elif self.path == "/api/pair":
            submitted_pin = data.get("token")
            if submitted_pin in pending_pairings:
                new_token = uuid.uuid4().hex
                device_info = pending_pairings[submitted_pin]
                
                # Use provided name or fall back to device hostname
                final_name = data.get("name")
                if not final_name or final_name.strip() == "":
                    final_name = device_info["hostname"]

                try:
                    cursor.execute(
                        "INSERT INTO devices (hardware_token, user_id, device_name, status) VALUES (?, ?, ?, 'online')",
                        (new_token, data.get("user_id"), final_name),
                    )
                    db.commit()
                    device_socket = device_info["socket"]
                    device_socket.send(f"VERIFIED:{new_token}".encode())
                    active_connections[new_token] = device_socket
                    del pending_pairings[submitted_pin]
                    self.send_json(200, {"message": "Device successfully linked."})
                except Exception as e:
                    self.send_json(500, {"error": f"Database error: {e}"})
            else:
                self.send_json(400, {"error": "Invalid PIN or device not waiting."})

        elif self.path == "/api/toggle_lock":
            token = data.get("token")
            try:
                cursor.execute("UPDATE devices SET is_locked = NOT is_locked WHERE hardware_token=?", (token,))
                db.commit()
                self.send_json(200, {"message": "Lock status updated"})
            except Exception as e:
                self.send_json(500, {"error": f"Database error: {e}"})

        elif self.path == "/api/download":
            tokens = data.get("tokens", [])
            # Support both single token (legacy) and array of tokens
            if not tokens and data.get("token"):
                tokens = [data.get("token")]
            
            link = data.get("link")
            results = []

            for token in tokens:
                # Check if device is locked
                cursor.execute("SELECT is_locked, device_name FROM devices WHERE hardware_token=?", (token,))
                row = cursor.fetchone()
                if not row:
                    results.append({"token": token, "status": "error", "message": "Device not found"})
                    continue
                
                if row[0]:
                    results.append({"token": token, "name": row[1], "status": "locked", "message": "Device is LOCKED"})
                    continue

                if token in active_connections:
                    try:
                        active_connections[token].send(link.encode())
                        cursor.execute(
                            "INSERT INTO history (hardware_token, download_link) VALUES (?, ?)",
                            (token, link),
                        )
                        db.commit()
                        results.append({"token": token, "name": row[1], "status": "success"})
                    except:
                        results.append({"token": token, "name": row[1], "status": "error", "message": "Socket error"})
                else:
                    results.append({"token": token, "name": row[1], "status": "offline"})

            self.send_json(200, {"results": results})

        db.close()


class ThreadedHTTPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass

def run_web_server():
    print(f"Web API active on {BIND_ADDR}:{WEB_PORT}")
    # Use ThreadedHTTPServer to handle multiple concurrent web requests
    server = ThreadedHTTPServer((BIND_ADDR, WEB_PORT), WebHandler)
    server.serve_forever()


if __name__ == "__main__":
    # Ensure database exists
    if not os.path.exists(DB_PATH):
        print("Database not found. Please run setup_db.py first.")
        # We could auto-run it, but user said "don't break anything", 
        # so let's stick to existing workflow but with better messaging.
    
    tcp_server = ThreadedTCPServer(BIND_ADDR, TCP_PORT)
    threading.Thread(target=tcp_server.start, daemon=True).start()
    run_web_server()

