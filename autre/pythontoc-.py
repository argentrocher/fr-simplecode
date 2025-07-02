import subprocess

# Lancer l'exécutable C
proc = subprocess.Popen(['./mon_programme_c'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

# Envoyer des commandes mathématiques à C
while True:
    command = input("Entrez une commande (ex: 2 + 3 ou 'exit' pour quitter) : ")
    
    # Envoyer la commande au programme C
    proc.stdin.write(command + '\n')
    proc.stdin.flush()

    if command == "exit":
        break

    # Lire la réponse du programme C
    result = proc.stdout.readline()
    print(f"Réponse de C : {result.strip()}")

# Fermer les pipes
proc.stdin.close()
proc.stdout.close()
proc.stderr.close()
proc.wait()
