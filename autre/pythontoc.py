import subprocess
import os
import threading
import time

def read_output(proc):
    """ Fonction pour lire les lignes de sortie du programme C en continu """
    while True:
        output = proc.stdout.readline()
        if output == '' and proc.poll() is not None:
            break  # Sortir si le processus est terminé
        if output:
            print(f"Réponse de C : {output.strip()}")

boucle = 0
while boucle == 0:
    recup = input("Entrer un programme en C : ")
    if recup == "exit":
        print("Fermeture")
        exit()
    # Lancer l'exécutable C
    if os.path.exists(recup):
        proc = subprocess.Popen([recup], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        # Démarrer un thread pour lire les sorties
        threading.Thread(target=read_output, args=(proc,), daemon=True).start()
        boucle = 1
    else:
        print("Le programme n'existe pas")

# Envoyer des commandes mathématiques à C
print("Entrez une commande (ex: 2 + 3 ou 'info' ou 'exit' pour quitter) : ")
while True:
    command = input("")

    # Envoyer la commande au programme C
    proc.stdin.write(command + '\n')
    proc.stdin.flush()

    if command == "exit":
        break

    # Optionnel: inclure un délai pour les réponses
    time.sleep(0.3)  # Délai d'une seconde (ajustez si nécessaire)

# Fermer les pipes
proc.stdin.close()
proc.stdout.close()
proc.stderr.close()
proc.wait()