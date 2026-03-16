from database import get_db
import os

def init_wallpaper():
    db = get_db()
    cursor = db.cursor()
    
    # Check if table exists (should be created by init_db already)
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='configuracoes_globais'")
    if not cursor.fetchone():
        print("Table configuracoes_globais missing!")
        return

    # Check if already has a wallpaper
    cursor.execute("SELECT valor FROM configuracoes_globais WHERE chave='active_wallpaper'")
    if not cursor.fetchone():
        # Set a default one from static/images if available
        image_dir = os.path.join(os.path.dirname(__file__), 'static', 'images')
        default_wallpaper = '/static/images/logo4.jpg' # Fallback
        
        if os.path.exists(image_dir):
            files = [f for f in os.listdir(image_dir) if f.endswith('.webp') or f.endswith('.jpg')]
            if files:
                default_wallpaper = f"/static/images/{files[0]}"
        
        cursor.execute("INSERT INTO configuracoes_globais (chave, valor) VALUES (?, ?)", ('active_wallpaper', default_wallpaper))
        db.commit()
        print(f"Default wallpaper set to: {default_wallpaper}")
    else:
        print("Wallpaper already set.")
    
    db.close()

if __name__ == '__main__':
    init_wallpaper()
