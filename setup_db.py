import sqlite3
import os

DB_PATH = os.environ.get("DB_PATH", "platform.db")

def setup():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    # We only drop if explicitly requested, otherwise use IF NOT EXISTS
    # For this version, let's keep it safe.
    
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY,
        username TEXT UNIQUE,
        email TEXT UNIQUE,
        password TEXT,
        reset_token TEXT
    )
    """)

    cursor.execute("""
    CREATE TABLE IF NOT EXISTS devices (
        hardware_token TEXT PRIMARY KEY,
        user_id INTEGER,
        device_name TEXT,
        status TEXT,
        is_locked INTEGER DEFAULT 0,
        FOREIGN KEY(user_id) REFERENCES users(id)
    )
    """)

    cursor.execute("""
    CREATE TABLE IF NOT EXISTS history (
        id INTEGER PRIMARY KEY,
        hardware_token TEXT,
        download_link TEXT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )
    """)

    # Only insert admin if users table is empty
    cursor.execute("SELECT COUNT(*) FROM users")
    if cursor.fetchone()[0] == 0:
        cursor.execute(
            "INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
            ("admin", "admin@test.com", "secret123"),
        )
        print("Default admin account created: admin / secret123")

    conn.commit()
    conn.close()
    print(f"Database setup complete at {DB_PATH}")


if __name__ == "__main__":
    setup()
