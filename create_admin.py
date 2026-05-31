import sqlite3
import os

DB_PATH = os.environ.get("DB_PATH", "platform.db")

def create_test_admin():
    db = sqlite3.connect(DB_PATH)
    cursor = db.cursor()

    try:
        cursor.execute(
            "INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
            ("testadmin", "test@example.com", "testpass123"),
        )
        db.commit()
        print("Test admin account created.")
        print("Username: testadmin")
        print("Password: testpass123")
    except sqlite3.IntegrityError:
        print("Error: Account 'testadmin' already exists.")

    db.close()


if __name__ == "__main__":
    create_test_admin()
