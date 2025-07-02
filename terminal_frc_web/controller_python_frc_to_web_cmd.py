import subprocess
import threading
import websockets
import asyncio
import sys
import os
import time

exe_path = "../fr-simplecode0.3.exe"
html_path= "frc_web_cmd.html"


accept_start_html=True

if os.path.exists("path_exe_web_cmd.txt"):
    with open("path_exe_web_cmd.txt", "r", encoding="utf-8") as f:
        line = f.readline().strip()
        if line:  # si pas vide
            exe_path = line
            if not os.path.exists(exe_path):
                print(f"Erreur: le chemin spécifié dans path_exe_web_cmd.txt est invalide: {exe_path}")
                time.sleep(3)
                sys.exit(1)

print(f"Chemin utilisé pour l'exe: {exe_path} (créer un fichier path_exe_web_cmd.txt permet de changer le chemin de l'exe)")
if not os.path.exists(exe_path):
    print(f"Erreur: le chemin prédéfini ne fonctionne pas")
    time.sleep(3)
    sys.exit(0)
    
if not os.path.exists(html_path):
    print(f"la page web prédéfini: {html_path} n'est pas disponible\nchoisi un nouveau fichier html")
    html_true=False
    while html_true==False:
        html_path=str(input("chemin du fichier html ('no' pour refusez):"))
        if html_path=="no" or html_path=="n" or html_path=="non":
            accept_start_html=False
            break
        if os.path.exists(html_path):
            html_true=True
        else:
            print(f"chemin incorrect: {html_path}")
  
if accept_start_html==True:
    # Ouvre la page HTML dans le navigateur (cmd start)
    try:
        subprocess.Popen(['cmd', '/c', 'start', html_path])
    except Exception as e:
        print(f"Impossible d'ouvrir la page: {e}")


print("INFO app de ce controller:\nattention a la vitesse de demande de l'application,\nun enchainement de commandes très rapide peut tronquer/répéter plusieurs fois/ralentir les messages !")

# Fonction qui gère chaque client WebSocket
async def handle_client(websocket):
    print("Client connecté")

    # Lance le programme (remplace par le tien)
    process = subprocess.Popen(
        [exe_path],  # <- programme frc en sous dossier (v0.3)
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    # Fonction pour lire stdout et envoyer au client
    def read_stdout():
        for line in process.stdout:
            asyncio.run(websocket.send(f"STDOUT: {line}"))

    # Fonction pour lire stderr et envoyer au client
    def read_stderr():
        for line in process.stderr:
            asyncio.run(websocket.send(f"STDERR: {line}"))

    # Démarre les threads pour stdout et stderr
    threading.Thread(target=read_stdout, daemon=True).start()
    threading.Thread(target=read_stderr, daemon=True).start()

    try:
        # Boucle pour recevoir les messages du client et les écrire dans stdin
        async for message in websocket:
            print(f"Reçu du client: {message}")
            process.stdin.write(message + "\n")
            process.stdin.flush()
    except websockets.exceptions.ConnectionClosed:
        print("Client déconnecté")

async def main():
    # Démarre le serveur WebSocket
    async with websockets.serve(handle_client, "localhost", 8765):
        print("Serveur WebSocket démarré sur ws://localhost:8765")
        await asyncio.Future()  # run forever


asyncio.run(main())