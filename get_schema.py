import sqlite3
import os

DB_PATH = 'toca.db'

def get_schema():
    if not os.path.exists(DB_PATH):
        print(f"Error: {DB_PATH} not found.")
        return

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    try:
        cursor.execute("SELECT name, sql FROM sqlite_master WHERE type='table'")
        tables = cursor.fetchall()
        for t in tables:
            print(f"Table: {t['name']}")
            print(f"{t['sql']}\n")
            
            # Also get indexes for completeness
            cursor.execute(f"SELECT name, sql FROM sqlite_master WHERE type='index' AND tbl_name='{t['name']}'")
            indexes = cursor.fetchall()
            for idx in indexes:
                if idx['sql']:
                    print(f"Index: {idx['name']}\n{idx['sql']}\n")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        conn.close()

if __name__ == '__main__':
    get_schema()
