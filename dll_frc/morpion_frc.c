#include <windows.h>
#include <stdio.h>
#include <string.h>

//gcc -shared -o morpion_frc.dll morpion_frc.c -lgdi32 -luser32

// Export de la fonction avec le nom demandé
__declspec(dllexport) const char* check_winner(char* arg) {
    // Le chemin du fichier table_frc.txt est supposé dans le même dossier que l'exécutable.
    // On récupère le chemin de l'exécutable puis on concatène table_frc.txt

    static char result[4];  // pour retourner "0", "1", "2" ou "-1" en string

    char exePath[MAX_PATH];
    char dirPath[MAX_PATH];
    char filePath[MAX_PATH];
    FILE *f = NULL;

    // Récupérer le chemin complet de l'exe
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        strcpy(result, "-1");
        return result;
    }

    // Extraire le dossier (tout avant le dernier \)
    char *lastSlash = strrchr(exePath, '\\');
    if (!lastSlash) {
        strcpy(result, "-1");
        return result;
    }

    size_t dirLen = lastSlash - exePath + 1;
    strncpy(dirPath, exePath, dirLen);
    dirPath[dirLen] = '\0';

    // Construire le chemin complet vers table_frc.txt
    snprintf(filePath, MAX_PATH, "%stable_frc.txt", dirPath);

    // Ouvrir le fichier
    f = fopen(filePath, "r");
    if (!f) {
        strcpy(result, "-1");
        return result;
    }

    // Lire ligne par ligne pour trouver grille=[[...]]
    char line[1024];
    char *start, *end;
    char grilleStr[256] = {0};
    int found = 0;

    char key[100];
    snprintf(key, sizeof(key), "%s=[[", arg);

    while (fgets(line, sizeof(line), f)) {
        start = strstr(line, key);
        if (start) {
            start += strlen(key);  // début des valeurs
            end = strchr(start, ']');
            if (!end) {
                end = strchr(start, ']');  // recheck just in case
            }
            if (end) {
                size_t len = end - start;
                if (len > sizeof(grilleStr)-1)
                    len = sizeof(grilleStr)-1;
                strncpy(grilleStr, start, len);
                grilleStr[len] = '\0';
                found = 1;
                break;
            }
        }
    }
    fclose(f);

    if (!found) {
        strcpy(result, "-1");
        return result;
    }

    // grilleStr contient quelque chose comme : X;O;X;O;X;O;X;O;X
    // On doit parser dans un tableau de 9 cases
    char *tokens[9];
    int count = 0;
    char *p = strtok(grilleStr, ";");
    while (p && count < 9) {
        tokens[count++] = p;
        p = strtok(NULL, ";");
    }
    if (count != 9) {
        strcpy(result, "-1");
        return result;
    }

    // Fonction lambda-like pour vérifier victoire
    // return 1 si X gagne, 2 si O gagne, 0 sinon
    int checkLine(int a, int b, int c) {
        if (strcmp(tokens[a], "X") == 0 &&
            strcmp(tokens[b], "X") == 0 &&
            strcmp(tokens[c], "X") == 0)
            return 1;
        if (strcmp(tokens[a], "O") == 0 &&
            strcmp(tokens[b], "O") == 0 &&
            strcmp(tokens[c], "O") == 0)
            return 2;
        return 0;
    }

    int res = 0;
    int lines[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8}, // lignes
        {0,3,6}, {1,4,7}, {2,5,8}, // colonnes
        {0,4,8}, {2,4,6}           // diagonales
    };

    for (int i=0; i<8; i++) {
        res = checkLine(lines[i][0], lines[i][1], lines[i][2]);
        if (res != 0)
            break;
    }

    // Retourner le résultat en string
    snprintf(result, sizeof(result), "%d", res);
    return result;
}
