#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>

#include <winsock2.h> //pour un server tcp
#include <ws2tcpip.h> //pour un server tcp
#pragma comment(lib, "Ws2_32.lib") //pour un server tcp

#include <windows.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include <sapi.h> //pour la voix speak() et parle()


#define MAX_COMMAND_LEN 16384
#define MAX_LEN 1024

//fonction bruit d'erreur windows
void play_error_sound() {
    MessageBeep(MB_ICONHAND); // MB_ICONHAND correspond au son d'erreur
}

//fonction licence :
void licence() {
    // Obtenir le chemin absolu de l'exécutable
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        // Si l'obtention du chemin échoue, afficher une erreur
        printf("Erreur lors de l'obtention du chemin de l'exécutable.\n");
        fflush(stdout);
        return;
    }

    // Extraire le répertoire de l'exécutable
    char *last_backslash = strrchr(exe_path, '\\');
    if (last_backslash != NULL) {
        *last_backslash = '\0'; // Terminer la chaîne à la position du dernier backslash
    }

    // Construire le chemin complet du fichier de licence
    char licence_path[MAX_PATH];
    snprintf(licence_path, sizeof(licence_path), "%s\\licence_fr-simplecode.txt", exe_path);

    // Vérifier si le fichier existe déjà
    FILE *file = fopen(licence_path, "r");
    if (file) {
        // Si le fichier existe, on le ferme et on n'écrit rien
        fclose(file);
        //printf("Le fichier de licence existe déjà.\n");
    } else {
        // Si le fichier n'existe pas, on le crée et on y écrit la licence
        file = fopen(licence_path, "w");
        if (file == NULL) {
            // Si l'ouverture du fichier échoue, on affiche un message d'erreur
            printf("Erreur lors de la création du fichier de licence.\n");
            fflush(stdout);
            return;
        }

        // Texte de la licence
        const char *licence_text =
            "Licence du logiciel fr-simplecode.exe sur Windows (v0.2):\n"
            "---------------------------------------------------\n"
            "Ce logiciel est sous licence pour un usage sous contrainte. Vous n'êtes pas autorisé "
            "à modifier ou reproduire ce logiciel sans l'autorisation explicite de l'auteur argentropcher, assigné au compte Google argentropcher. "
            "L'utilisation commerciale est autorisée à condition de respecter "
            "les termes de cette licence et de ne pas enfreindre les droits de l'auteur.\n\n"
            "Avertissement :\n"
            "Ce logiciel est fourni tel quel, sans aucune garantie. L'auteur ne pourra être tenu responsable "
            "de tout dommage résultant de l'utilisation de ce logiciel sur votre appareil.\n";

        // Écrire la licence dans le fichier
        fprintf(file, "%s", licence_text);

        // Fermer le fichier
        fclose(file);
        //printf("Le fichier de licence a été créé avec succès.\n");
    }
}

//fonction pour remplacer time() dans la command
char *replace_actual_time(const char *str) {
    size_t new_size = strlen(str) + 1;  // Taille initiale de la chaîne
    char *result = malloc(new_size);    // Allocation mémoire
    if (!result) return NULL;

    char *pos, *start = (char *)str;
    result[0] = '\0'; // Initialise une chaîne vide

    while ((pos = strstr(start, "time(")) != NULL) {
        // Obtenir l'heure actuelle
        time_t rawtime;
        struct tm *timeinfo;
        char time_str[16] = ""; // Stockage pour hhmm.ss ou autres formats

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        size_t replace_length = 0; // Nombre de caractères à supprimer (ex: "time()")

        // Vérifier le type de time()
        if (strncmp(pos, "time(s)", 7) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d", timeinfo->tm_sec);
            replace_length = 7;  // Longueur de "time(s)"
        }
        else if (strncmp(pos, "time(m)", 7) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d", timeinfo->tm_min);
            replace_length = 7;  // Longueur de "time(m)"
        }
        else if (strncmp(pos, "time(h)", 7) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d", timeinfo->tm_hour);
            replace_length = 7;  // Longueur de "time(h)"
        }
        else if (strncmp(pos, "time(date)", 10) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d%02d,%02d",
                     timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900) % 100);
            replace_length = 10;
        }
        else if (strncmp(pos, "time(day)", 9) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d", timeinfo->tm_mday);
            replace_length = 9;
        }
        else if (strncmp(pos, "time(ms)", 8) == 0) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            snprintf(time_str, sizeof(time_str), "%03d", st.wMilliseconds); // 000–999
            replace_length = 8;
        }
        else if (strncmp(pos, "time()", 6) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d%02d.%02d",
                     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            replace_length = 6;  // Longueur de "time()"
        }
        else {
            // Si "time(" mal formé, on avance d'un caractère pour éviter une boucle infinie
            strncat(result, start, pos - start + 1);
            start = pos + 1;
            continue;
        }

        // Ajuster la mémoire pour le remplacement
        new_size += strlen(time_str) - replace_length;
        result = realloc(result, new_size);
        if (!result) return NULL;

        // Copier la partie avant "time(...)"
        strncat(result, start, pos - start);
        strcat(result, time_str); // Ajouter l'heure formatée

        // Avancer après "time(...)" complètement
        start = pos + replace_length;
    }

    // Ajouter le reste de la chaîne originale après la dernière occurrence
    strcat(result, start);

    return result; // Retourne la nouvelle chaîne allouée dynamiquement
}

//fonction pour remplacer num_day() par le numéro du jour actuelle entre 1 et 7
char* handle_num_day(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    // Obtenir le jour actuel (1 = Lundi, ..., 7 = Dimanche)
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int wday = t->tm_wday;
    int day_num = (wday == 0) ? 7 : wday; // dimanche = 0, donc on met 7

    // Convertir en chaîne
    char day_str[4];
    snprintf(day_str, sizeof(day_str), "%d", day_num);

    // Remplacer chaque "num_day()" par le chiffre
    char *src = input;
    while (1) {
        char *pos = strstr(src, "num_day()");
        if (!pos) {
            strcat(result, src);
            break;
        }

        // Copier la partie avant num_day()
        strncat(result, src, pos - src);
        strcat(result, day_str);

        src = pos + strlen("num_day()");
    }

    return result;
}

double last_result; //variable pour rep

int var_script_exit; //variable pour une sortie exit dans un script
int var_script_pause; //variable pour une sortie de boucle ou condition dans un script
int var_affiche_virgule; //variable pour afficher la virgule des nombres dans les print()/affiche()
int var_affiche_slash_n; //variable pour afficher le retour automatique à la fin d'un print()/affiche() ou non (0=oui 1=non)
int var_boucle_infini; //variable qui est à 1 lorsque une boucle infini est en court, sinon 0

bool var_sortir_entree; //variable pour savoir si il faut renvoié l'entré de l'utilisateur dans la sortie

int mode_script_output; //variable pour le type de sortie en script (1= tout les blocks, 0= uniquement les erreurs + les sorties voulu)

int var_in_module; //variable pour savoir si on peut utiliser [input] si on est en module (0 ou 1)

int socket_var=0; //var pour le socket actif ou non pour les print() et affiche() (attention ne gère pas les messages d'erreurs qui sont visibles en exécution local)
SOCKET client;

// Fonction pour évaluer une simple expression avec priorité des opérateurs (déclaration de la fonction)
double eval_expr(const char *expr, int *index);

// Fonction calcul expression pour la fonction affiche de script et les conditions if
double condition_script_order(char *command);

//fonction pour récupérer les 2 chiffres après la virgule
char *get_comma_only(const char *command);

void fonction_start_remove_newlines(char *str);

// fonction remplacer les virgules en points
void replace_comma_with_dot(char *str) {
    while (*str) {
        if (*str == ',') {
            *str = '.';
        }
        str++;
    }
}

// Fonction pour obtenir le répertoire de l'exécutable
char* get_executable_path() {
    static char path[MAX_PATH];
    GetModuleFileName(NULL, path, sizeof(path));
    char *last_backslash = strrchr(path, '\\');
    if (last_backslash) {
        *last_backslash = '\0'; // Terminer le chemin au dernier backslash
    }
    return path;
}

// Définition du fichier de résultat
char FILENAME[MAX_PATH];

void initialize_filename() {
    char *exec_path = get_executable_path();
    snprintf(FILENAME, sizeof(FILENAME), "%s\\result.txt", exec_path); // Met à jour le chemin complet
}

//debut traitement zone de variable
#define VAR_FILE "var.txt"

double handle_in_var(char *value) {
    FILE *file = fopen(VAR_FILE, "r");  // Ouvre var.txt en lecture
    if (file == NULL) {
        // Si le fichier n'existe pas, retourner 0.00
        printf("Erreur : Le fichier var.txt n'existe pas.\n");
        fflush(stdout);
        return 0.00;
    }

    char line[256];  // Buffer pour lire chaque ligne du fichier
    while (fgets(line, sizeof(line), file) != NULL) {
        // Enlever le saut de ligne à la fin de la ligne lue
        line[strcspn(line, "\n")] = '\0';

        // Vérifier si la ligne contient la valeur recherchée
        if (strstr(line, value) != NULL) {
            // Si la valeur est trouvée dans la ligne
            fclose(file);  // Fermer le fichier après la recherche
            return 1.00;
        }
    }

    fclose(file);  // Fermer le fichier si la valeur n'est pas trouvée
    return 0.00;  // Retourne 0.00 si la valeur n'a pas été trouvée
}

void replace_in_var(char *command) {
    char result[1024] = "";  // La chaîne pour stocker le résultat final
    char *cursor = command;
    char *start_in_var, *end_in_var;
    char temp[256];  // Pour stocker la valeur de remplacement

    // Parcours de toute la chaîne
    while ((start_in_var = strstr(cursor, "in_var(")) != NULL) {
        // Ajouter le texte avant 'in_var('
        strncat(result, cursor, start_in_var - cursor);

        // Trouver la parenthèse fermante
        end_in_var = strchr(start_in_var, ')');
        if (end_in_var != NULL) {
            // Extraire l'argument entre les parenthèses
            size_t length = end_in_var - start_in_var - 7;  // La longueur entre "in_var(" et ")"
            char value[length + 1];
            strncpy(value, start_in_var + 7, length);  // Copier l'argument
            value[length] = '\0';  // Terminer la chaîne

            // Obtenir la valeur retournée par handle_in_var
            double result_in_var = handle_in_var(value);

            // Convertir la valeur en chaîne de caractères
            snprintf(temp, sizeof(temp), "%.2f", result_in_var);
            replace_comma_with_dot(temp); //remplace la ',' du nombre par un '.'

            // Ajouter la valeur au résultat
            strncat(result, temp, sizeof(result) - strlen(result) - 1);

            // Déplacer le curseur après la parenthèse fermante
            cursor = end_in_var + 1;
        } else {
            // Si parenthèse fermante non trouvée, ajouter le reste du texte
            strncat(result, cursor, strlen(cursor));
            break;
        }
    }

    // Ajouter le reste du texte après le dernier in_var
    if (*cursor != '\0') {
        strncat(result, cursor, sizeof(result) - strlen(result) - 1);
    }

    // Copier le résultat final dans la variable command
    strcpy(command, result);
}

void handle_var_list() {
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir var.txt\n");
        return;
    }

    char line[MAX_LEN];
    char var_name[MAX_LEN];

    printf("Liste des variables dans le fichier var.txt :\n");
    fflush(stdout);

    // Lire chaque ligne du fichier
    while (fgets(line, sizeof(line), file)) {
        // Extraire le nom de la variable (avant le signe '=')
        if (sscanf(line, "%[^=]", var_name) == 1) {
            // Afficher le nom de la variable
            printf("%s\n", var_name);
            fflush(stdout);
        }
    }

    fclose(file);
}

void handle_var_suppr(const char *arg) {
    if (strcmp(arg, "tout") == 0 || strcmp(arg, "all") == 0) {
        // Supprimer complètement le fichier var.txt
        if (remove(VAR_FILE) == 0) {
            printf("Toutes les variables ont été supprimées.\n");
            fflush(stdout);
        } else {
            printf("Erreur : Impossible de supprimer le fichier %s.\n", VAR_FILE);
            fflush(stdout);
        }
        return;
    }

    // Si ce n'est pas "tout" ou "all", chercher et supprimer une seule variable
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir %s.\n", VAR_FILE);
        fflush(stdout);
        return;
    }

    FILE *tempFile = fopen("var_temp.txt", "w");
    if (!tempFile) {
        printf("Erreur : Impossible de créer un fichier temporaire.\n");
        fflush(stdout);
        fclose(file);
        return;
    }

    char line[MAX_LEN];
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        char var_name[MAX_LEN];
        // Extraire le nom de la variable avant '='
        if (sscanf(line, "%[^=]", var_name) == 1) {
            if (strcmp(var_name, arg) == 0) {
                found = 1; // Marquer que la variable a été trouvée et ignorée (donc supprimée)
                continue;
            }
        }
        fputs(line, tempFile); // Copier la ligne dans le fichier temporaire si ce n'est pas la variable à supprimer
    }

    fclose(file);
    fclose(tempFile);

    // Remplacer var.txt par le fichier temporaire si une suppression a eu lieu
    if (found) {
        remove(VAR_FILE);
        rename("var_temp.txt", VAR_FILE);
        remove("var_temp.txt");
        printf("La variable \"%s\" a été supprimée.\n", arg);
        fflush(stdout);
    } else {
        remove("var_temp.txt"); // Si aucune suppression n'a eu lieu, supprimer le fichier temporaire
        printf("Variable \"%s\" non trouvée, aucune suppression effectuée.\n", arg);
        fflush(stdout);
    }
}

void save_variable(const char *name, const char *value_str) {
    FILE *file = fopen("var.txt", "r");
    if (!file) {
        file = fopen("var.txt", "w");
        if (!file) {
            printf("Erreur : impossible d'ouvrir var.txt\n");
            fflush(stdout);
            return;
        }
    }

    // Lire toutes les variables existantes
    char line[100], temp[MAX_LEN] = "";
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        char var_name[50], var_value_str[50];

        if (sscanf(line, "%49[^=]=%49[^\n]", var_name, var_value_str) == 2) {
            // Si la variable existe déjà, on remplace sa valeur
            if (strcmp(var_name, name) == 0) {
                snprintf(line, sizeof(line), "%s=%s\n", name, value_str);
                found = 1;
            }
        }
        strcat(temp, line);
    }
    fclose(file);

    // Si la variable n'existe pas encore, on l'ajoute
    if (!found) {
        snprintf(line, sizeof(line), "%s=%s\n", name, value_str);
        strcat(temp, line);
    }

    // Écrire dans le fichier
    file = fopen("var.txt", "w");
    if (file) {
        fputs(temp, file);
        fclose(file);
    }
}



double get_variable(const char *name) {
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) return 0.00;  // Si le fichier n'existe pas, renvoyer 0.00

    char line[100], var_name[50];
    double value;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%49[^=]=%lf", var_name, &value) == 2) {
            if (strcmp(var_name, name) == 0) {
                fclose(file);
                return value;
            }
        }
    }

    fclose(file);
    return 0.00;  // Si la variable n'est pas trouvée
}

char* handle_variable(char *expr) {
    static char new_expr[MAX_LEN];
    char temp_expr[MAX_LEN];
    char *start = expr, *pos;
    char name[50];
    char valueforsave[1024];
    double value;

    new_expr[0] = '\0';  // Réinitialiser l'expression

    // Vérifier si c'est une affectation `var|nom|=valeur`
    if (sscanf(expr, "var|%49[^|]|=%49[^\n]", name, &valueforsave) == 2) {
        int index = 0;  // Position de départ pour l'analyse
        if (strstr(valueforsave, "var|") != NULL) {
            replace_comma_with_dot(valueforsave);
            strcpy(valueforsave, handle_variable(valueforsave));
            replace_comma_with_dot(valueforsave);
        }
        double result = eval_expr(valueforsave, &index);  // Évaluer l'expression
        char result_str[50];
        snprintf(result_str, sizeof(result_str), "%.2f", result);  // Convertir en chaîne
        save_variable(name, result_str);  // Sauvegarder sous forme de texte
        if (mode_script_output==1){
            printf("Variable mise à jour : %s = %s\n", name, result_str);
            fflush(stdout);
        }
        return "None";  // Indique qu'il ne faut pas exécuter l'expression
    }

    // Copier l'expression originale
    strcpy(temp_expr, expr);

    while ((pos = strstr(temp_expr, "var|")) != NULL) {
        // Copier le texte avant la variable
        strncat(new_expr, temp_expr, pos - temp_expr);

        // Trouver la fin du nom de variable
        char *end = strchr(pos + 4, '|');
        if (!end) break;  // Format incorrect, on arrête

        // Extraire le nom de la variable
        char var_name[50];
        strncpy(var_name, pos + 4, end - (pos + 4));
        var_name[end - (pos + 4)] = '\0';

        // Récupérer la valeur de la variable
        double value = get_variable(var_name);

        // Ajouter la valeur dans new_expr
        char value_str[50];
        snprintf(value_str, sizeof(value_str), "%.2f", value);
        strcat(new_expr, value_str);

        // Passer à la suite
        start = end + 1;
        strcpy(temp_expr, start);
    }

    // Ajouter le reste de l'expression
    strcat(new_expr, temp_expr);

    return new_expr;
}
//fin traitement zone de variable




// Fonction pour gérer les occurrences de random[nombre1, nombre2]
char* handle_random(char *expr, double last_result) {
    char *start = strstr(expr, "random[");

    while (start != NULL) {
        char *end = strchr(start, ']');

        if (end != NULL) {
            // Extraire la sous-expression entre les crochets
            int length = end - start - 7;  // Exclure "random[" et "]"
            if (length <= 0) {
                fprintf(stderr, "Erreur : Syntaxe incorrecte dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);  // Remplace par "0.00"
                return expr;
            }

            char range[length + 1];
            strncpy(range, start + 7, length);
            range[length] = '\0';  // Fin de chaîne

            // Vérifier si la virgule est présente
            char *comma = strchr(range, ',');
            if (!comma) {
                fprintf(stderr, "Erreur : Il manque une virgule dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // Variables pour stocker les deux valeurs
            char num1_str[50], num2_str[50];
            int num1, num2;

            // Extraire les deux parties
            if (sscanf(range, "%49[^,],%49s", num1_str, num2_str) != 2) {
                fprintf(stderr, "Erreur : Format incorrect dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // Vérifier si num1 est "rep", "var||" ou un nombre
            if (strcmp(num1_str, "rep") == 0) {
                num1 = (int)last_result;
            } else if (strncmp(num1_str, "var|", 4) == 0) {  // Vérifier si num1_str commence par "var||"
                // Remplacer "var||" par sa valeur via la fonction handle_variable
                strcpy(num1_str, handle_variable(num1_str));  // Appel de la fonction handle_variable
                num1 = atoi(num1_str);  // Convertir la chaîne modifiée en nombre entier
            } else if (isdigit(num1_str[0]) || num1_str[0] == '-') {
                num1 = atoi(num1_str);
            } else {
                fprintf(stderr, "Erreur : Valeur incorrecte dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // Vérifier si num2 est "rep", "var||" ou un nombre
            if (strcmp(num2_str, "rep") == 0) {
                num2 = (int)last_result;
            } else if (strncmp(num2_str, "var|", 4) == 0) {  // Vérifier si num2_str commence par "var||"
                // Remplacer "var||" par sa valeur via la fonction handle_variable
                strcpy(num2_str, handle_variable(num2_str));  // Appel de la fonction handle_variable
                num2 = atoi(num2_str);  // Convertir la chaîne modifiée en nombre entier
            } else if (isdigit(num2_str[0]) || num2_str[0] == '-') {
                num2 = atoi(num2_str);
            } else {
                fprintf(stderr, "Erreur : Valeur incorrecte dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // Vérifier l'ordre des bornes
            if (num1 > num2) {
                int temp = num1;
                num1 = num2;
                num2 = temp;
            }

            // Générer un nombre aléatoire entre num1 et num2
            int random_value = rand() % (num2 - num1 + 1) + num1;

            // Construire la nouvelle expression
            char result[MAX_LEN];
            snprintf(result, MAX_LEN, "%.*s%d%s", (int)(start - expr), expr, random_value, end + 1);

            // Remplacer dans expr
            strcpy(expr, result);
        } else {
            fprintf(stderr, "Erreur : Fermeture ']' manquante dans random[].\n");
            fflush(stdout);
            strncpy(start, "0.00", 4);
        }

        // Chercher une autre occurrence de "random["
        start = strstr(expr, "random[");
    }

    return expr;
}

// Fonction pour lire les nombres dans l'expression
double parse_number(const char *expr, int *index) {
    double num = 0;
    int sign = 1;

    // Vérifier s'il y a un signe "-" ou "+"
    if (expr[*index] == '-') {
        sign = -1;
        (*index)++;
    } else if (expr[*index] == '+') {
        (*index)++;
    }

    // Lire les chiffres du nombre
    while (isdigit(expr[*index])) {
        num = num * 10 + (expr[*index] - '0');
        (*index)++;
    }

    // Lire la partie décimale si présente
    if (expr[*index] == '.') {
        (*index)++;
        double fraction = 1;
        while (isdigit(expr[*index])) {
            fraction *= 0.1;
            num += (expr[*index] - '0') * fraction;
            (*index)++;
        }
    }

    return num * sign;  // Retourner le nombre avec le signe correct
}

// Fonction pour évaluer les termes séparés par * ou /
double parse_term(const char *expr, int *index) {
    double value = 0;
    int state = 0;

    // Lire un nombre ou une parenthèse
    if (expr[*index] == '(') {
        (*index)++;  // Passer la parenthèse ouvrante
        value = eval_expr(expr, index);  // Évaluer l'expression à l'intérieur des parenthèses
        if (expr[*index] == ')') {
            (*index)++;  // Passer la parenthèse fermante
        }
    } else {
        value = parse_number(expr, index);  // Si ce n'est pas une parenthèse, on lit un nombre
    }

    // Gérer les multiplicateurs et diviseurs en suivant les priorités
    while (expr[*index] == '*' || expr[*index] == '/') {
        char op = expr[*index];
        (*index)++;

        // Si l'opérateur suivant est *, diviser ou multiplier le terme précédent
        double next_value = 0;
        if (expr[*index] == '(') {
            (*index)++;  // Passer la parenthèse ouvrante
            next_value = eval_expr(expr, index);  // Récursivement évaluer l'expression dans les parenthèses
            if (expr[*index] == ')') {
                (*index)++;  // Passer la parenthèse fermante
            }
        } else {
            next_value = parse_number(expr, index);  // Lire un nombre normal
        }

        if (op == '*') {
            value *= next_value;
        } else if (op == '/') {
            if (next_value == 0) {
                printf("Erreur : division par zéro\n");
                fflush(stdout);
                state = 1;
                // Sortir en cas de division par zéro
            } else {
                value /= next_value;
            }
        }
    }

    if (state == 1) {
        value = 0.00;
    }

    return value;
}

// Fonction principale pour évaluer une expression
double eval_expr(const char *expr, int *index) {
    double value = parse_term(expr, index); // Évaluer le premier terme (gère la multiplication et la division)

    while (expr[*index] != '\0') {
        if (expr[*index] == '+' || expr[*index] == '-') {
            char op = expr[*index];
            (*index)++; // Passer à l'opérateur suivant

            double next_value = parse_term(expr, index); // Récupérer la valeur suivante

            // Appliquer l'opérateur à la valeur
            if (op == '+') {
                value += next_value;
            } else if (op == '-') {
                value -= next_value;
            }
        }
        // Gérer la multiplication et la division
        else if (expr[*index] == '*' || expr[*index] == '/') {
            char op = expr[*index];
            (*index)++; // Passer à l'opérateur suivant

            double next_value = parse_term(expr, index); // Récupérer la valeur suivante

            // Appliquer l'opérateur à la valeur
            if (op == '*') {
                value *= next_value;
            } else if (op == '/') {
                if (next_value == 0) {
                    printf("Erreur : division par zéro\n");
                    return 0;  // Retourner une valeur par défaut en cas d'erreur
                }
                value /= next_value;
            }
        }
        // Vérifier si une parenthèse ouvrante est présente
        else if (expr[*index] == '(') {
            (*index)++; // Passer la parenthèse ouvrante

            char prev_op = (expr[*index - 2] == '+' || expr[*index - 2] == '-') ? expr[*index - 2] : '+';
            double parenthesis_value = eval_expr(expr, index); // Appel récursif pour traiter l'expression à l'intérieur des parenthèses

            // Appliquer l'opérateur avant la parenthèse à la valeur de la parenthèse
            if (prev_op == '+') {
                value += parenthesis_value;
            } else if (prev_op == '-') {
                value -= parenthesis_value;
            }
        }
        // Vérifier si une parenthèse fermante est présente
        else if (expr[*index] == ')') {
            (*index)++; // Passer la parenthèse fermante
            break; // Sortir de la boucle une fois qu'on a traité la parenthèse
        }
        else {
            break; // Sortir de la boucle si un caractère invalide est trouvé
        }
    }

    return value;
}

// Fonction pour ajouter une multiplication implicite dans l'expression
void handle_implicit_multiplication(char *expr) {
    int len = strlen(expr);

    // Parcours de l'expression pour vérifier si un nombre est suivi immédiatement d'une parenthèse ouvrante
    for (int i = 0; i < len - 1; i++) {
        if (isdigit(expr[i]) && expr[i + 1] == '(') {
            // Insérer un '*' entre le nombre et la parenthèse
            memmove(&expr[i + 2], &expr[i + 1], len - i);  // Décaler les caractères
            expr[i + 1] = '*';  // Ajouter '*' entre le nombre et la parenthèse
            len++;  // Mettre à jour la longueur de l'expression après insertion
            i++;  // Passer à l'indice suivant pour éviter de traiter plusieurs fois la même paire
        }
    }
}

// Fonction principale pour évaluer une expression à partir d'une chaîne de caractères
double eval(char *expr) {
    handle_implicit_multiplication(expr);  // Appeler la fonction pour ajouter les multiplications implicites
    int index = 0;
    return eval_expr(expr, &index);  // Appeler eval_expr pour évaluer l'expression modifiée
}



// Fonction pour remplacer "rep" par la valeur de last_result dans la commande
char* replace_rep(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN];
    char *pos;

    // Convertir last_result en chaîne de caractères
    char result_str[32];
    snprintf(result_str, sizeof(result_str), "%.2lf", last_result);
    // Remplacer toutes les virgules par des points dans result_str (si la locale utilise une virgule)
    for (int i = 0; result_str[i] != '\0'; i++) {
        if (result_str[i] == ',') {
            result_str[i] = '.';  // Remplace la virgule par un point
        }
    }

    // Remplacer toutes les occurrences de "rep"
    while ((pos = strstr(command, "rep")) != NULL) {
        // Copier la partie avant "rep" dans le buffer
        int len_before = pos - command;
        strncpy(buffer, command, len_before);
        buffer[len_before] = '\0';

        // Ajouter la chaîne qui représente le dernier résultat
        strcat(buffer, result_str);

        // Ajouter le reste de la commande après "rep"
        strcat(buffer, pos + 3);

        // Copier la commande mise à jour dans "command"
        strcpy(command, buffer);
    }

    return command; // Retourner la commande modifiée
}

// Fonction pour sauvegarder le résultat dans un fichier
void save_result(double result) {
    FILE *file = fopen(FILENAME, "w");
    if (file == NULL) {
        perror("Erreur : impossible d'ouvrir le fichier pour écrire");
        fflush(stdout);
        return;
    }
    fprintf(file, "%.2lf\n", result);
    fclose(file);
    printf("Résultat %.2lf",result);
    fflush(stdout);
    printf(" sauvegardé dans %s\n", FILENAME);
    fflush(stdout);
}

// Fonction pour charger le résultat depuis un fichier
double load_result() {
    double result = 0.0;
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        perror("Erreur : impossible d'ouvrir le fichier pour lire");
        fflush(stdout);
        return result;
    }
    fscanf(file, "%lf", &result);
    fclose(file);
    printf("Résultat chargé depuis %s : %.s", FILENAME);
    fflush(stdout);
    printf("rep=%.2lf\n", result);
    fflush(stdout);
    return result;
}





double def_airetriangle(char* command, double last_result) {
    double base = 0, hauteur = 0;
    char* base_str;
    char* hauteur_str;

    // Vérifier si la commande commence par "triangle("
    if (strncmp(command, "triangle(", 9) != 0) {
        printf("Commande invalide\n");
        return -0.01;
    }

    // Isoler la partie entre "triangle(" et ")"
    char* params = strstr(command, "triangle(");
    if (params != NULL) {
        // Sauter "triangle("
        params += 9;

        // Trouver la partie avant le ";"
        base_str = strtok(params, ";");

        // Trouver la partie après le ";"
        hauteur_str = strtok(NULL, ")");

        // Vérification que les deux paramètres (base et hauteur) existent
        if (base_str == NULL || hauteur_str == NULL) {
            printf("Erreur : format incorrect, il faut 'triangle(base;hauteur)'\n");
            return -0.01;
        }

        // Si "rep" est trouvé, remplacer par last_result
        if (strcmp(base_str, "rep") == 0) {
            base = last_result;
        } else {
            base = atof(base_str); // Convertir la base en double
        }

        if (strcmp(hauteur_str, "rep") == 0) {
            hauteur = last_result;
        } else {
            hauteur = atof(hauteur_str); // Convertir la hauteur en double
        }

        // Calculer l'aire du triangle (base * hauteur) / 2
        double aire = (base * hauteur) / 2;
        return aire;
    } else {
        printf("Format de commande incorrect\n");
        return -0.01;
    }
}

double def_airecercle(char* command, double last_result) {
    double rayon = 0;
    char* rayon_str;

    // Vérifier si la commande commence par "cercle("
    if (strncmp(command, "cercle(", 7) != 0) {
        printf("Commande invalide \n");
        return -0.01;
    }

    // Isoler la partie entre "cercle(" et ")"
    char* params = strstr(command, "cercle(");
    if (params != NULL) {
        // Sauter "cercle("
        params += 7;

        // Trouver la partie avant le ")"
        rayon_str = strtok(params, ")");

        // Vérification que le rayon est bien fourni
        if (rayon_str == NULL) {
            printf("Erreur : format incorrect, il faut 'cercle(rayon)'\n");
            return -0.01;
        }

        // Si "rep" est trouvé, remplacer par last_result
        if (strcmp(rayon_str, "rep") == 0) {
            rayon = last_result;
        } else {
            rayon = atof(rayon_str);  // Convertir le rayon en double
        }

        // Calculer l'aire du cercle ( * rayon)
        double aire = M_PI * rayon * rayon;
        return aire;
    } else {
        printf("Format de commande incorrect\n");
        return -0.01;
    }
}


char* handle_cos(char *input) {
    static char result[MAX_COMMAND_LEN]; // buffer final
    result[0] = '\0';

    char *src = input;
    while (1) {
        char *start = strstr(src, "cos(");
        if (!start) {
            strcat(result, src); // pas d'autre cos, copier le reste
            break;
        }

        // Copier ce qui est avant cos(
        strncat(result, src, start - src);

        // Cherche la parenthèse fermante en tenant compte des ()
        int depth = 0;
        char *ptr = start + 4; // après "cos("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_cos()]"); // mauvaise syntaxe
            break;
        }

        // Extraire l'intérieur de cos()
        char expr[512] = {0};
        strncpy(expr, start + 4, ptr - (start + 4));
        expr[ptr - (start + 4)] = '\0';

        // Calculer l'expression
        double value = condition_script_order(expr);
        double cos_result = cos(value);

        // Convertir en chaîne
        char cos_str[64];
        snprintf(cos_str, sizeof(cos_str), "%.10f", cos_result);
        strcat(result, cos_str);

        // Continuer après la parenthèse fermante
        src = ptr + 1;
    }

    return result;
}

char* handle_arccos(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';
    char *src = input;
    while (1) {
        char *start = strstr(src, "arccos(");
        if (!start) {
            strcat(result, src);
            break;
        }

        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 7; // après "arccos("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_arccos()]");
            break;
        }

        char expr[512] = {0};
        strncpy(expr, start + 7, ptr - (start + 7));
        expr[ptr - (start + 7)] = '\0';

        double value = condition_script_order(expr);
        double arccos_result = acos(value); // en radians

        char arccos_str[64];
        snprintf(arccos_str, sizeof(arccos_str), "%.10f", arccos_result);
        strcat(result, arccos_str);
        //printf("Expression '%s' = %.10f\n", expr, value);
        src = ptr + 1;
    }

    return result;
}

char* handle_sin(char *input) {
    static char result[MAX_COMMAND_LEN]; // buffer final
    result[0] = '\0';

    char *src = input;
    while (1) {
        char *start = strstr(src, "sin(");
        if (!start) {
            strcat(result, src); // pas d'autre sin, copier le reste
            break;
        }

        // Copier ce qui est avant sin(
        strncat(result, src, start - src);

        // Cherche la parenthèse fermante en tenant compte des ()
        int depth = 0;
        char *ptr = start + 4; // après "sin("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_sin()]"); // mauvaise syntaxe
            break;
        }

        // Extraire l'intérieur de sin()
        char expr[512] = {0};
        strncpy(expr, start + 4, ptr - (start + 4));
        expr[ptr - (start + 4)] = '\0';

        // Calculer l'expression
        double value = condition_script_order(expr);
        double sin_result = sin(value);

        // Convertir en chaîne
        char sin_str[64];
        snprintf(sin_str, sizeof(sin_str), "%.10f", sin_result);
        strcat(result, sin_str);

        // Continuer après la parenthèse fermante
        src = ptr + 1;
    }

    return result;
}

char* handle_arcsin(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';
    char *src = input;
    while (1) {
        char *start = strstr(src, "arcsin(");
        if (!start) {
            strcat(result, src);
            break;
        }

        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 7; // après "arcsin("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_arcsin()]");
            break;
        }

        char expr[512] = {0};
        strncpy(expr, start + 7, ptr - (start + 7));
        expr[ptr - (start + 7)] = '\0';

        double value = condition_script_order(expr);
        double arcsin_result = asin(value); // en radians

        char arcsin_str[64];
        snprintf(arcsin_str, sizeof(arcsin_str), "%.10f", arcsin_result);
        strcat(result, arcsin_str);
        //printf("Expression '%s' = %.10f\n", expr, value);
        src = ptr + 1;
    }

    return result;
}

char* handle_tan(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    char *src = input;
    while (1) {
        char *start = strstr(src, "tan(");
        if (!start) {
            strcat(result, src);
            break;
        }

        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 4; // après "tan("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_tan()]");
            break;
        }

        char expr[512] = {0};
        strncpy(expr, start + 4, ptr - (start + 4));
        expr[ptr - (start + 4)] = '\0';

        double value = condition_script_order(expr);
        double tan_result = tan(value);

        char tan_str[64];
        snprintf(tan_str, sizeof(tan_str), "%.10f", tan_result);
        strcat(result, tan_str);
        src = ptr + 1;
    }

    return result;
}

char* handle_arctan(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    char *src = input;
    while (1) {
        char *start = strstr(src, "arctan(");
        if (!start) {
            strcat(result, src);
            break;
        }

        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 7; // après "arctan("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_arctan()]");
            break;
        }

        char expr[512] = {0};
        strncpy(expr, start + 7, ptr - (start + 7));
        expr[ptr - (start + 7)] = '\0';

        double value = condition_script_order(expr);
        double arctan_result = atan(value);

        char arctan_str[64];
        snprintf(arctan_str, sizeof(arctan_str), "%.10f", arctan_result);
        strcat(result, arctan_str);
        src = ptr + 1;
    }

    return result;
}

char* handle_log(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    char *src = input;
    while (1) {
        // Recherche de "log("
        char *start = strstr(src, "log(");
        if (!start) {
            strcat(result, src);
            break;
        }

        // Copier ce qui est avant log(
        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 4; // Après "log("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_log()]");
            break;
        }

        // Extraire l'intérieur de log()
        char expr[512] = {0};
        strncpy(expr, start + 4, ptr - (start + 4));
        expr[ptr - (start + 4)] = '\0';

        // Calculer l'expression
        double value = condition_script_order(expr);
        if (value <= 0) {
            strcat(result, "[ERREUR_log()]");
        } else {
            double log_result = log(value); // Logarithme népérien

            // Convertir en chaîne
            char log_str[64];
            snprintf(log_str, sizeof(log_str), "%.10f", log_result);
            strcat(result, log_str);
        }

        // Continuer après la parenthèse fermante
        src = ptr + 1;
    }

    return result;
}

char* handle_exp(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    char *src = input;
    while (1) {
        // Recherche de "exp("
        char *start = strstr(src, "exp(");
        if (!start) {
            strcat(result, src);
            break;
        }

        // Copier ce qui est avant exp(
        strncat(result, src, start - src);

        int depth = 0;
        char *ptr = start + 4; // Après "exp("
        while (*ptr) {
            if (*ptr == '(') depth++;
            else if (*ptr == ')') {
                if (depth == 0) break;
                depth--;
            }
            ptr++;
        }

        if (*ptr != ')') {
            strcat(result, "[ERREUR_exp()]");
            break;
        }

        // Extraire l'intérieur de exp()
        char expr[512] = {0};
        strncpy(expr, start + 4, ptr - (start + 4));
        expr[ptr - (start + 4)] = '\0';

        // Calculer l'expression
        double value = condition_script_order(expr);
        double exp_result = exp(value); // Calcul de l'exponentielle

        // Convertir en chaîne
        char exp_str[64];
        snprintf(exp_str, sizeof(exp_str), "%.10f", exp_result);
        strcat(result, exp_str);

        // Continuer après la parenthèse fermante
        src = ptr + 1;
    }

    return result;
}

//fonction modulo
char* handle_modulo(char *input) {
    static char result[MAX_COMMAND_LEN];
    result[0] = '\0';

    char *src = input;

    while (1) {
        char *start = strstr(src, "modulo(");
        if (!start) {
            strcat(result, src);  // Copie ce qui reste s'il n'y a plus de modulo
            break;
        }

        strncat(result, src, start - src);  // Copie avant le modulo(

        char *ptr = start + 7; // après "modulo("
        int depth = 0;
        int found_semicolon = 0;
        char *semicolon = NULL;
        char *end = ptr;

        // Trouve le bon ";" (pas dans des parenthèses)
        while (*end) {
            if (*end == '(') depth++;
            else if (*end == ')') {
                if (depth == 0) break;
                depth--;
            } else if (*end == ';' && depth == 0 && !found_semicolon) {
                semicolon = end;
                found_semicolon = 1;
            }
            end++;
        }

        if (!semicolon || *end != ')') {
            strcat(result, "[ERREUR_modulo()]");
            printf("Erreur : sur () ou ; dans modulo !\n");
            fflush(stdout);
            break;
        }

        // Extraire les deux expressions
        char expr1[512] = {0};
        char expr2[512] = {0};
        strncpy(expr1, ptr, semicolon - ptr);
        expr1[semicolon - ptr] = '\0';

        strncpy(expr2, semicolon + 1, end - (semicolon + 1));
        expr2[end - (semicolon + 1)] = '\0';

        // Vérifie que les parenthèses sont équilibrées
        int check_balance(const char *s) {
            int d = 0;
            while (*s) {
                if (*s == '(') d++;
                else if (*s == ')') d--;
                if (d < 0) return 0;
                s++;
            }
            return d == 0;
        }

        if (!check_balance(expr1) || !check_balance(expr2)) {
            strcat(result, "[ERREUR_modulo()]");
            printf("Erreur : sur () ou ; dans modulo !\n");
            fflush(stdout);
            break;
        }

        // Calcul des valeurs
        double v1 = condition_script_order(expr1);
        double v2 = condition_script_order(expr2);

        if (v2 == 0) {
            strcat(result, "[ERREUR_DIV0]");
            printf("Erreur : division par 0 dans modulo !\n");
            fflush(stdout);
        } else {
            double modulo = fmod(v1, v2);
            char mod_str[64];
            snprintf(mod_str, sizeof(mod_str), "%.10f", modulo);
            strcat(result, mod_str);
        }

        src = end + 1; // Poursuit après le modulo(...)
    }

    return result;
}

// Fonction pour gérer et évaluer toutes les occurrences de sqrt(...) dans la commande
char* handle_sqrt(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN]; // Pour stocker la commande modifiée
    char *start, *end; // Pointeurs pour trouver les parenthèses
    char inner_expr[MAX_COMMAND_LEN]; // Pour stocker l'expression interne

    // Initialiser le buffer avec la commande d'origine
    strcpy(buffer, command);

    // Chercher les occurrences de sqrt(...)
    while ((start = strstr(buffer, "sqrt(")) != NULL) {
        end = strchr(start, ')'); // Trouver la parenthèse fermante

        // Vérifier que nous avons trouvé une parenthèse fermante
        if (end == NULL) {
            break; // Pas de parenthèse fermante, sortir de la boucle
        }

        // Extraire l'expression à l'intérieur de sqrt(...)
        int length = end - (start + 5); // longueur de l'expression entre sqrt( et )
        strncpy(inner_expr, start + 5, length); // Copier l'expression interne
        inner_expr[length] = '\0'; // Terminer la chaîne

        // Remplacer "rep" par last_result dans l'expression interne
        replace_rep(inner_expr, last_result);

        //remplacer les time()
        strcpy(inner_expr,replace_actual_time(inner_expr));
        //remplacer les num_day()
        if (strstr(command,"num_day()")!=NULL){
            strcpy(command,handle_num_day(command));
        }
        //remplacer les random[,]
        handle_random(inner_expr,last_result);

        //fonction modulo
        if (strstr(command,"modulo(")!= NULL){
            strcpy(command,handle_modulo(command));
        }

        //pour fonction récupérer la virgule en entier
        if (strstr(inner_expr,"get_comma(")!= NULL || strstr(inner_expr,"prend_virgule(") != NULL){
            strcpy(inner_expr,get_comma_only(inner_expr));
        }

        //math best
        if (strstr(inner_expr,"arccos(")!= NULL){
            strcpy(inner_expr,handle_arccos(inner_expr));
        }
        if (strstr(inner_expr,"arcsin(")!= NULL){
            strcpy(inner_expr,handle_arcsin(inner_expr));
        }
        if (strstr(inner_expr,"arctan(")!= NULL){
            strcpy(inner_expr,handle_arctan(inner_expr));
        }
        if (strstr(inner_expr,"cos(")!= NULL){
            strcpy(inner_expr,handle_cos(inner_expr));
        }
        if (strstr(inner_expr,"sin(")!= NULL){
            strcpy(inner_expr,handle_sin(inner_expr));
        }
        if (strstr(inner_expr,"tan(")!= NULL){
            strcpy(inner_expr,handle_tan(inner_expr));
        }
        if (strstr(inner_expr,"log(")!= NULL){
            strcpy(inner_expr,handle_log(inner_expr));
        }
        if (strstr(inner_expr,"exp(")!= NULL){
            strcpy(inner_expr,handle_exp(inner_expr));
        }

        replace_comma_with_dot(inner_expr);

        // Évaluer l'expression interne
        double sqrt_value = eval(inner_expr);

        // Vérifier si le résultat est négatif
        if (sqrt_value < 0) {
            printf("Erreur : racine carrée d'un nombre négatif.\n");
            fflush(stdout);
            return command; // Retourner la commande originale si une erreur se produit
        }

        // Calculer la racine carrée
        double result = sqrt(sqrt_value);

        // Remplacer la première occurrence de sqrt(...) dans le buffer par son résultat
        char result_str[MAX_COMMAND_LEN];
        snprintf(result_str, sizeof(result_str), "%.2lf", result);

        // Construire la nouvelle chaîne sans sqrt
        char new_buffer[MAX_COMMAND_LEN];
        // Copier la partie avant sqrt(
        int prefix_length = start - buffer;
        strncpy(new_buffer, buffer, prefix_length);
        new_buffer[prefix_length] = '\0'; // Terminer la nouvelle chaîne

        // Ajouter le résultat
        strcat(new_buffer, result_str);

        // Ajouter le reste de la chaîne après )
        strcat(new_buffer, end + 1); // end + 1 pour passer la parenthèse fermante

        // Copier la nouvelle chaîne dans buffer
        strcpy(buffer, new_buffer);
    }

    return buffer; // Retourner la commande modifiée
}


// Fonction pour gérer et évaluer toutes les occurrences de sqrt(...) dans la commande
char* handle_cbrt(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN]; // Pour stocker la commande modifiée
    char *start, *end; // Pointeurs pour trouver les parenthèses
    char inner_expr[MAX_COMMAND_LEN]; // Pour stocker l'expression interne

    // Initialiser le buffer avec la commande d'origine
    strcpy(buffer, command);

    // Chercher les occurrences de sqrt(...)
    while ((start = strstr(buffer, "cbrt(")) != NULL) {
        end = strchr(start, ')'); // Trouver la parenthèse fermante

        // Vérifier que nous avons trouvé une parenthèse fermante
        if (end == NULL) {
            break; // Pas de parenthèse fermante, sortir de la boucle
        }

        // Extraire l'expression à l'intérieur de sqrt(...)
        int length = end - (start + 5); // longueur de l'expression entre sqrt( et )
        strncpy(inner_expr, start + 5, length); // Copier l'expression interne
        inner_expr[length] = '\0'; // Terminer la chaîne

        // Remplacer "rep" par last_result dans l'expression interne
        replace_rep(inner_expr, last_result);

        //remplacer les time()
        strcpy(inner_expr,replace_actual_time(inner_expr));
        //remplacer les num_day()
        if (strstr(command,"num_day()")!=NULL){
            strcpy(command,handle_num_day(command));
        }
        //remplacer les random[,]
        handle_random(inner_expr,last_result);

        //fonction modulo
        if (strstr(command,"modulo(")!= NULL){
            strcpy(command,handle_modulo(command));
        }

        //pour fonction récupérer la virgule en entier
        if (strstr(inner_expr,"get_comma(")!= NULL || strstr(inner_expr,"prend_virgule(") != NULL){
            strcpy(inner_expr,get_comma_only(inner_expr));
        }

        //math best
        if (strstr(inner_expr,"arccos(")!= NULL){
            strcpy(inner_expr,handle_arccos(inner_expr));
        }
        if (strstr(inner_expr,"arcsin(")!= NULL){
            strcpy(inner_expr,handle_arcsin(inner_expr));
        }
        if (strstr(inner_expr,"arctan(")!= NULL){
            strcpy(inner_expr,handle_arctan(inner_expr));
        }
        if (strstr(inner_expr,"cos(")!= NULL){
            strcpy(inner_expr,handle_cos(inner_expr));
        }
        if (strstr(inner_expr,"sin(")!= NULL){
            strcpy(inner_expr,handle_sin(inner_expr));
        }
        if (strstr(inner_expr,"tan(")!= NULL){
            strcpy(inner_expr,handle_tan(inner_expr));
        }
        if (strstr(inner_expr,"log(")!= NULL){
            strcpy(inner_expr,handle_log(inner_expr));
        }
        if (strstr(inner_expr,"exp(")!= NULL){
            strcpy(inner_expr,handle_exp(inner_expr));
        }

        replace_comma_with_dot(inner_expr);

        // Évaluer l'expression interne
        double sqrt_value = eval(inner_expr);

        // Calculer la racine cubique
        double result = cbrt(sqrt_value);

        // Remplacer la première occurrence de sqrt(...) dans le buffer par son résultat
        char result_str[MAX_COMMAND_LEN];
        snprintf(result_str, sizeof(result_str), "%.2lf", result);

        // Construire la nouvelle chaîne sans sqrt
        char new_buffer[MAX_COMMAND_LEN];
        // Copier la partie avant sqrt(
        int prefix_length = start - buffer;
        strncpy(new_buffer, buffer, prefix_length);
        new_buffer[prefix_length] = '\0'; // Terminer la nouvelle chaîne

        // Ajouter le résultat
        strcat(new_buffer, result_str);

        // Ajouter le reste de la chaîne après )
        strcat(new_buffer, end + 1); // end + 1 pour passer la parenthèse fermante

        // Copier la nouvelle chaîne dans buffer
        strcpy(buffer, new_buffer);
    }

    return buffer; // Retourner la commande modifiée
}

double local_value = 0.00; // Stocker la valeur de local


// Fonction pour définir la valeur de local(argument)
void set_local(const char* expr, double last_result) {
    char temp_expr[MAX_COMMAND_LEN];
    strcpy(temp_expr, expr); // Copier l'expression de l'argument
    // Remplacer "rep" dans l'expression par la valeur de last_result
    char* processed_expr = replace_rep(temp_expr, last_result);
    // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
    if (strstr(processed_expr, "sqrt(") != NULL) {
        //printf("%s\n",processed_expr);
        char* processed_command2="";
        // Gérer les expressions sqrt
        char* processed_command = handle_sqrt(processed_expr, last_result);
        // Afficher la commande après traitement de sqrt pour diagnostic
        ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
        // Remplacer les virgules par des points dans la commande traitée
        for (int i = 0; processed_command[i] != '\0'; i++) {
            if (processed_command[i] == ',') {
                processed_command[i] = '.'; // Remplacer la virgule par un point
            }
        }
        // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
        if (strstr(processed_expr, "cbrt(") != NULL) {
            // Gérer les expressions sqrt
            processed_command2 = handle_cbrt(processed_command, last_result);
            // Afficher la commande après traitement de sqrt pour diagnostic
            ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande traitée
            for (int i = 0; processed_command2[i] != '\0'; i++) {
                if (processed_command2[i] == ',') {
                    processed_command2[i] = '.'; // Remplacer la virgule par un point
                }
            }
        } else {
            processed_command2 = processed_command;
        }
        // Remplacer "rep" dans l'expression par la valeur de last_result
        char* processed_command3 = replace_rep(processed_command2, last_result);
        // Évaluer l'expression et renvoyer le résultat
        double result = eval(processed_command3);
        printf("Valeur de local définie à : %.2lf\n", result);
        fflush(stdout);  // Envoyer immédiatement le résultat à Python
        local_value = result;
        return;  // Continuer avec la prochaine itération de la boucle
    }

    // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
    if (strstr(processed_expr, "cbrt(") != NULL) {
        char* processed_command2="";
        // Gérer les expressions sqrt
        char* processed_command = handle_cbrt(processed_expr, last_result);
        // Afficher la commande après traitement de sqrt pour diagnostic
        ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
        // Remplacer les virgules par des points dans la commande traitée
        for (int i = 0; processed_command[i] != '\0'; i++) {
            if (processed_command[i] == ',') {
                processed_command[i] = '.'; // Remplacer la virgule par un point
            }
        }
        if (strstr(processed_expr, "sqrt(") != NULL) {
            // Gérer les expressions sqrt
            processed_command2 = handle_sqrt(processed_command, last_result);
            // Afficher la commande après traitement de sqrt pour diagnostic
            ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande traitée
            for (int i = 0; processed_command2[i] != '\0'; i++) {
                if (processed_command2[i] == ',') {
                    processed_command2[i] = '.'; // Remplacer la virgule par un point
                }
            }
        } else {
            processed_command2 = processed_command;
        }
        // Remplacer "rep" dans l'expression par la valeur de last_result
        char* processed_command3 = replace_rep(processed_command2, last_result);
        // Évaluer l'expression et renvoyer le résultat
        double result = eval(processed_command3);
        printf("Valeur de local définie à : %.2lf\n", result);
        fflush(stdout);  // Envoyer immédiatement le résultat à Python
        local_value = result;
        return;  // Continuer avec la prochaine itération de la boucle
    }



    // Évaluer l'expression
    local_value = eval(processed_expr);

    printf("Valeur de local définie à : %.2lf\n", local_value);
    fflush(stdout);
}

// Fonction pour récupérer la valeur de local() et la stocker dans last_result
double get_local() {
    printf("Valeur de local récupérée : %.2lf\n", local_value);
    fflush(stdout);
    return local_value;
}

// fonction pour lancer un .exe windows uniquement (cmd)
// Fonction pour exécuter le fichier spécifié
void execute(const char *file_path) {
    // Vérifier si le chemin est valide
    if (file_path == NULL || strlen(file_path) == 0) {
        printf("Erreur : chemin invalide.\n");
        return;
    }

    // Construire la commande à exécuter
    char command[2048];  // Assurez-vous que la taille est suffisante
    snprintf(command, sizeof(command), "cmd /c \"%s\"", file_path);

    // Utiliser system() pour exécuter le fichier
    int result = system(command);

    // Vérifier si l'exécution a réussi
    if (result == 0) {
        printf("Exécution réussie.\n");
        fflush(stdout);
    } else {
        printf("Erreur lors de l'exécution de la commande.\n");
        fflush(stdout);
    }
}

// Fonction pour extraire l'argument de la commande var="execute(chemin)"
void parse_and_execute(const char *var) {
    // Chercher "execute(" et ")" dans la chaîne
    const char *debut = strstr(var, "execute(");
    const char *fin = strchr(var, ')');

    if (debut != NULL && fin != NULL && fin > debut) {
        // Extraire le chemin entre les parenthèses
        debut += strlen("execute(");  // Déplacer le pointeur après "execute("

        int length = fin - debut;  // Calculer la longueur du chemin
        char file_path[length + 1];  // Allouer la mémoire pour le chemin

        strncpy(file_path, debut, length);  // Copier le chemin dans la variable
        file_path[length] = '\0';  // Ajouter le caractère de fin de chaîne

        // Appeler la fonction execute avec le chemin extrait
        execute(file_path);
    } else {
        printf("Erreur : format de commande invalide.\n");
        fflush(stdout);
    }
}
//fin fonction lancer un exe window uniquement (cmd)

//fonction pour récupérer que la virgule du double
char *get_comma_only(const char *command) {
    size_t size = strlen(command) + 1;
    char *result = malloc(size);
    if (!result) return NULL;
    result[0] = '\0';

    const char *start = command;
    const char *pos_get, *pos_prend,*pos;

    while (1) {
        pos_get = strstr(start, "get_comma(");
        pos_prend = strstr(start, "prend_virgule(");

        // Choisir le premier dans la chaîne
        if (pos_get && pos_prend) {
            pos = (pos_get < pos_prend) ? pos_get : pos_prend;
        } else if (pos_get) {
            pos = pos_get;
        } else if (pos_prend) {
            pos = pos_prend;
        } else {
            break; // aucun des deux n'est trouvé
        }

        // Ajoute ce qu'il y avait avant à result
        strncat(result, start, pos - start);

        // Détermine le type (longueur du préfixe)
        int prefix_len = (pos == pos_get) ? 10 : 14;

        // Cherche la parenthèse fermante
        const char *end = pos + prefix_len;
        int open_parens = 1; // On commence à 1 car on part de l'ouverture de "("

        // On parcourt la chaîne après la première parenthèse
        while (*end != '\0' && open_parens > 0) {
            if (*end == '(') {
                open_parens++; // Une nouvelle parenthèse ouvrante
            } else if (*end == ')') {
            open_parens--; // Une parenthèse fermante
            }
            end++; // Avancer dans la chaîne
        }
        end-=1;
        if (open_parens > 0) {
            printf("Erreur : parenthèses non équilibrées.\n");
            fflush(stdout);
            break;
        }
        size_t expr_len = end - (pos + prefix_len);
        char expr[expr_len + 1];
        strncpy(expr, pos + prefix_len, expr_len);
        expr[expr_len] = '\0';

        // Appel à ta fonction pour convertir l'expression
        double val = condition_script_order(expr);
        int decimal_part = (int)round((val - (int)val) * 100);  // récupère les 2 chiffres après la virgule

        // Corrige négatif éventuel dû aux arrondis flottants
        if (decimal_part < 0) decimal_part *= -1;

        //decimal_part -= 1;

        // Remplacer l'appel par le résultat des décimales
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", decimal_part);

        // Réallocation possible si on agrandit beaucoup
        size += strlen(buf);
        result = realloc(result, size);
        if (!result) return NULL;

        strcat(result, buf);

        // Continue après la parenthèse fermée
        start = end + 1;
    }

    // Ajoute le reste de la chaîne
    strcat(result, start);
    //printf(result);
    //printf("sortie \n");
    return result;
}

//fonction pour use_script(chemindufichier;input)[nomduscriptou0pourinit]
void use_script_launcher(const char *command) {
    // Extrait les parties entre () et []
    const char *start_args = strchr(command, '(');
    const char *end_args = strchr(command, ')');
    const char *start_block = strchr(command, ')[');
    const char *end_block = strrchr(command, ']');

    if (!start_args || !end_args) {
        printf("Erreur de syntaxe dans use_script()\n");
        fflush(stdout);
        return;
    }

    // Extraire chemin et paramètre
    char buffer[1024];
    strncpy(buffer, start_args + 1, end_args - start_args - 1);
    buffer[end_args - start_args - 1] = '\0';

    char *chemin = strtok(buffer, ";");
    char *parametre = strtok(NULL, ";");

    // Nettoyage des quotes
    if (chemin && chemin[0] == '\'') chemin++;
    if (parametre) {
    if (parametre && parametre[0] == '\'') parametre++;
    if (parametre && parametre[strlen(parametre) - 1] == '\'') parametre[strlen(parametre) - 1] = '\0';

    if (strncmp(parametre, "var|",4) == 0) {
            strcpy(parametre, handle_variable(parametre));  // Remplace les variables
            // Remplacement des virgules par des points
            char *ptr = parametre;
            while (*ptr) {  // Parcourt la chaîne caractère par caractère
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(parametre, "None") != NULL) {
                //printf("ici");
                printf("Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                fflush(stdout);
                strcpy(parametre, "0.00");
            }
    }
    }

    char nom_script[256] = {0};

    // Cas avec bloc []
    if (start_block && end_block && end_block > start_block) {
        strncpy(nom_script, start_block + 1, end_block - start_block - 1);
        nom_script[end_block - start_block - 1] = '\0';

        // Nettoyage quote s'il y a
        if (nom_script[0] == '\'') memmove(nom_script, nom_script + 1, strlen(nom_script));
        size_t len = strlen(nom_script);
        if (len > 0 && nom_script[len - 1] == '\'') nom_script[len - 1] = '\0';
    } else {
        strcpy(nom_script, "0");
    }

    // Nettoyage des quotes '
    if (chemin[0] == '\'') chemin++;
    if (parametre && parametre[0] == '\'') parametre++;
    if (parametre && parametre[strlen(parametre) - 1] == '\'') parametre[strlen(parametre) - 1] = '\0';
    if (nom_script[0] == '\'') {
        memmove(nom_script, nom_script + 1, strlen(nom_script));
    }
    if (nom_script[strlen(nom_script) - 1] == '\'') {
        nom_script[strlen(nom_script) - 1] = '\0';
    }

    // Écriture du paramètre dans le fichier
    if (parametre && strlen(parametre) > 0) {
        FILE *param_file = fopen("input_fr-simplecode.txt", "w");
        if (param_file) {
            fprintf(param_file, "%s\n", parametre);
            fclose(param_file);
        }
    }

    // Vérification extension
    if (!strstr(chemin, ".txt") && !strstr(chemin, ".frc")) {
        strcat(chemin, ".frc");
    }

    FILE *f = fopen(chemin, "r");
    if (!f) {
        printf("Impossible d’ouvrir le fichier : %s\n", chemin);
        fflush(stdout);
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *fichier_contenu = malloc(len + 1);
    fread(fichier_contenu, 1, len, f);
    fichier_contenu[len] = '\0';
    fclose(f);

    char *script_debut = NULL;
    if (strcmp(nom_script, "0") == 0) {
        script_debut = strstr(fichier_contenu, "#init[[[");
        if (!script_debut) {
            script_debut = strstr(fichier_contenu, "[[[");
        }
    } else {
        char cherche_balise[256];
        snprintf(cherche_balise, sizeof(cherche_balise), "#%s[[[", nom_script);
        script_debut = strstr(fichier_contenu, cherche_balise);
    }

    if (!script_debut) {
        printf("Script non trouvé dans le fichier\n");
        fflush(stdout);
        free(fichier_contenu);
        return;
    }

    char *bloc_debut = strstr(script_debut, "[[[");
    char *bloc_fin = strstr(bloc_debut, "]]]");

    if (!bloc_debut || !bloc_fin) {
        printf("Bloc script mal formé.\n");
        free(fichier_contenu);
        return;
    }

    bloc_fin += 3; // inclure les 3 crochets
    size_t taille_script = bloc_fin - bloc_debut;

    char *script = malloc(taille_script + 1);
    strncpy(script, bloc_debut, taille_script);
    script[taille_script] = '\0';

    fonction_start_remove_newlines(script);

    var_in_module=1;//activation de l'[input]

    if (strncmp(script, "[[[", 3) == 0) {
        var_script_exit = 0;
        var_script_pause = 0;
        mode_script_output = 0;
        var_affiche_virgule = 0;
        var_affiche_slash_n = 0;
        var_boucle_infini = 0;
        handle_script(script);
    }

    var_in_module=0;//désactivation de l'[input]
    free(fichier_contenu);
    free(script);
}

//fonction pour traiter les ""blabla"" dans les print() affiche() ou speak() parle()
char* traiter_chaine_guillemet(const char *input) {
    const char *ptr = input;
    char *result = malloc(2048);
    result[0] = '\0';

    char temp[1024];
    int temp_i = 0;
    bool in_quotes = false;

    while (*ptr) {
        if (*ptr == '"') {
            temp[temp_i] = '\0';
            if (in_quotes) {
                // On est à la fin d'une zone entre guillemets : copier tel quel
                strcat(result, temp);
            } else {
                // On est à la fin d'une zone hors guillemets : traiter
                if (strlen(temp) > 0) {
                    char formatted[64];
                    snprintf(formatted, sizeof(formatted), "%.2f", condition_script_order(temp));
                    strcat(result, formatted);
                }
            }
            temp_i = 0;
            in_quotes = !in_quotes;
        } else {
            if (temp_i < sizeof(temp) - 1) {
                temp[temp_i++] = *ptr;
            }
        }
        ptr++;
    }

    // Si la chaîne finit hors guillemets, traiter le reste
    if (!in_quotes && temp_i > 0) {
        temp[temp_i] = '\0';
        char formatted[64];
        snprintf(formatted, sizeof(formatted), "%.2f", condition_script_order(temp));
        strcat(result, formatted);
    }

    return result;
}

// Fonction qui regarde si la chaîne contient "speak()" ou "parle()"
// puis elle lit ce qui se trouve entre les parenthèses
void handle_speak(const char *input) {
    char command[MAX_COMMAND_LEN];
    char textToSpeak[MAX_COMMAND_LEN];

    // Vérifier si la chaîne commence par "speak(" ou "parle("
    if (strncmp(input, "speak(", 6) == 0 || strncmp(input, "parle(", 6) == 0) {
        // Extraire le texte entre les parenthèses
        const char *start = strchr(input, '(');
        const char *end = strchr(input, ')');

        if (start != NULL && end != NULL && end > start) {
            start++;  // Avancer pour ignorer le '('
            size_t length = end - start;
            strncpy(textToSpeak, start, length);
            textToSpeak[length] = '\0';  // Ajouter le terminateur de chaîne

            char *result = traiter_chaine_guillemet(textToSpeak);  // Appelle traiter_chaine_guillemet pour traiter "blabla"conditionscriptorder"blablabla"
            if (result != NULL) {
                strncpy(textToSpeak, result, MAX_COMMAND_LEN - 1);  // Copie dans textToSpeak
                textToSpeak[MAX_COMMAND_LEN - 1] = '\0';  // Assure le null-terminate
                free(result);  // Libère la mémoire allouée par traiter_chaine
            }

            if (strncmp(textToSpeak, "var|",4) == 0) { //historiquement l'appel sans traiter_chaine_guillemet (code update) peut être supprimer car inactif d'office
            strcpy(textToSpeak, handle_variable(textToSpeak));  // Remplace les variables
            // Remplacement des virgules par des points
            char *ptr = textToSpeak;
            while (*ptr) {  // Parcourt la chaîne caractère par caractère
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(textToSpeak, "None") != NULL) {
                //printf("ici");
                printf("Erreur : définir une variable dans un speak est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                fflush(stdout);
                strcpy(textToSpeak, "0.00");
            }
            }

            // Convertir le texte à dire en format wchar_t (UTF-16)
            wchar_t wideText[1024];
            MultiByteToWideChar(CP_UTF8, 0, textToSpeak, -1, wideText, 1024);

            ISpVoice *pVoice = NULL;

            // Initialiser COM
            if (FAILED(CoInitialize(NULL))) {
                printf("Erreur d'initialisation COM\n");
                return;
            }

            // Créer l'objet de voix SAPI
            if (SUCCEEDED(CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&pVoice))) {
                // Faire parler le texte
                pVoice->lpVtbl->Speak(pVoice, wideText, 0, NULL);
                // Libérer l'objet voix après utilisation
                pVoice->lpVtbl->Release(pVoice);
            } else {
                printf("Erreur de création de la voix.\n");
                fflush(stdout);
            }

            // Libérer COM
            CoUninitialize();
        }
    }
}

//partie script

//fonction pour pouvoir sauter des lignes grâce à [/n] dans un affiche
void replace_newline_tags_in_fonction_script_affiche(char *buffer) {
    char temp[MAX_COMMAND_LEN]; // Stockage temporaire
    int i = 0, j = 0;

    while (buffer[i] != '\0') {
        // Vérifie si on a trouvé `[/n]`
        if (strncmp(&buffer[i], "[/n]", 4) == 0) {
            temp[j++] = '\n'; // Ajoute un saut de ligne
            i += 4; // Saute `[/n]`
        } else {
            temp[j++] = buffer[i++]; // Copie le caractère normal
        }
    }

    temp[j] = '\0'; // Terminer la nouvelle chaîne
    strcpy(buffer, temp); // Remplace buffer par la nouvelle version
}

//Fonction affiche pour script
void fonction_script_affiche(char *text){
    char buffer[MAX_COMMAND_LEN];  // Stocke temporairement les parties de texte
    int index = 0;
    int in_quotes = 0; // Indique si on est à l'intérieur de guillemets

    char send_buf[MAX_COMMAND_LEN];
    int send_index = 0;

    //printf("Affichage : ");

    while (*text) {
        if (*text == '"') {
            if (in_quotes) {
                buffer[index] = '\0'; // Terminer la chaîne
                replace_newline_tags_in_fonction_script_affiche(buffer); //fonction pour pouvoir sauter des lignes grâce à [/n]
                if (socket_var == 1) {
                    send_index += snprintf(send_buf + send_index, sizeof(send_buf) - send_index, "%s", buffer);
                } else {
                    printf("%s", buffer);
                    fflush(stdout);
                }
            }
            in_quotes = !in_quotes; // Basculer l'état (entrée ou sortie de guillemets)
            index = 0; // Réinitialiser le buffer
        }
        else if (in_quotes) {
            buffer[index++] = *text; // Stocker le texte entre guillemets
        }
        else { // On est en dehors des guillemets
            if (isspace(*text)) {
                text++; // Ignorer les espaces
                continue;
            }
            // Lire une expression (jusqu'à un " ou un espace)
            while (*text && *text != '"' && !isspace(*text)) {
                buffer[index++] = *text++;
            }
            buffer[index] = '\0'; // Terminer la chaîne

            // Évaluer et afficher le résultat de l'expression
            if (index > 0) {

                if (var_affiche_virgule==0){
                    double val = condition_script_order(buffer);
                    if (socket_var == 1) {
                        send_index += snprintf(send_buf + send_index, sizeof(send_buf) - send_index, "%.2f", val);
                    } else {
                        printf("%.2f", val);
                        fflush(stdout);
                    }
                } else {
                    int var_not_comma = (int)condition_script_order(buffer);
                    if (socket_var == 1) {
                        send_index += snprintf(send_buf + send_index, sizeof(send_buf) - send_index, "%d", var_not_comma);
                    } else {
                        printf("%d", var_not_comma);
                        fflush(stdout);
                    }
                }

            }
            index = 0; // Réinitialiser le buffer
            continue; // Reprendre la boucle sans incrémenter text ici
        }
        text++;
    }
    if (var_affiche_slash_n==0){
        if (socket_var == 1) {
            send_index += snprintf(send_buf + send_index, sizeof(send_buf) - send_index, "\n");
        } else {
            printf("\n");
            fflush(stdout);
        }
    }
    // Envoi final via socket
    if (socket_var == 1 && send_index > 0) {
        send(client, send_buf, send_index, 0);
    }
}

void fonction_script_command_for_affiche(char *command) {
    char *start = NULL;

    if (strncmp(command, "affiche(", 8) == 0) {
        start = command + 8;  // Pointer juste après "affiche("
    } else if (strncmp(command, "print(", 6) == 0) {
        start = command + 6;  // Pointer juste après "print("
    }

    if (start != NULL) {
        // Trouver la position de la ")"
        char *end = strrchr(start, ')');
        if (end != NULL) {
            *end = '\0';  // Remplacer la ")" par un caractère nul
            fonction_script_affiche(start);  // Passer le contenu extrait à la fonction
        } else {
            printf("Erreur : Parenthèse de fermeture manquante sur affiche ou print.\n");
            fflush(stdout);
        }
    }
}

// Fonction qui exécute une commande
void execute_script_order(char *command) {
    if (strncmp(command, "###sortie:classique###",22)==0 || strncmp(command,"###sortie:normale###",20)==0 || strncmp(command,"###output:normal###",19)==0){
        mode_script_output=0;
        return;
    }
    if (mode_script_output==1){
        printf("%s\n", command); // Simulation d'une exécution
    }
    if (strncmp(command, "###sortie:tout###",17)==0 || strncmp(command,"###sortie:all###",16)==0 || strncmp(command,"###output:all###",16)==0){
        mode_script_output=1;
        return;
    }
    if (strncmp(command,"###sortie_affiche:pasdevirgule###",33)==0 || strncmp(command,"###output_print:notcomma###",27)==0){
        var_affiche_virgule=1;
        return;
    }
    if (strncmp(command,"###sortie_affiche:virgule###",28)==0 || strncmp(command,"###output_print:comma###",24)==0){
        var_affiche_virgule=0;
        return;
    }
    if (strncmp(command,"###sortie_affiche:[/n]###",25)==0 || strncmp(command,"###output_print:[/n]###",23)==0){
        var_affiche_slash_n=0;
        return;
    }
    if (strncmp(command,"###sortie_affiche:pasde[/n]###",30)==0 || strncmp(command,"###output_print:not[/n]###",26)==0){
        var_affiche_slash_n=1;
        return;
    }

    if (strncmp(command, "speak(", 6) == 0 || strncmp(command, "parle(", 6) == 0) {
        handle_speak(command);
    }

    //fonction pour exécuter un fichier de commande (script) depuis un autre script
    if (strncmp(command,"use_script(",11)==0){
        use_script_launcher(command);
        return;
    }

    if (strncmp(command,"error_sound()",13)==0 || strncmp(command,"son_erreur()",12)==0){
        play_error_sound();
        return;
    }

    if (strstr(command,"[input]")){
        if (var_in_module==1) {
            FILE *f = fopen("input_fr-simplecode.txt", "r");
            char input_value[64] = "0.00"; // Valeur par défaut

            if (f) {
                if (fgets(input_value, sizeof(input_value), f)) {
                    // Nettoyer les sauts de ligne
                    input_value[strcspn(input_value, "\r\n")] = '\0';
                    if (strlen(input_value) == 0) strcpy(input_value, "0.00");
                }
                fclose(f);
            }

            // Nouveau buffer pour remplacer toutes les occurrences
            char new_command[MAX_COMMAND_LEN] = {0};
            char *src = command;
            char *found;

            while ((found = strstr(src, "[input]"))) {
                // Copier la partie avant [input]
                strncat(new_command, src, found - src);
                strcat(new_command, input_value); // Ajouter la valeur

                src = found + 7; // Sauter "[input]"
            }

            // Ajouter la fin de la chaîne restante
            strcat(new_command, src);

            // Écraser la commande originale
            strcpy(command, new_command);
        } else {
            // Si var_in_module != 1, remplacer [input] par 0.00
            char new_command[MAX_COMMAND_LEN] = {0};
            char *src = command;
            char *found;

            while ((found = strstr(src, "[input]"))) {
            strncat(new_command, src, found - src);
            strcat(new_command, "0.00");
            src = found + 7;
            }

            strcat(new_command, src);
            strcpy(command, new_command);
        }
    }

    if (strncmp(command, "var_list()", 10) == 0) {
            handle_var_list();
            return;
        }
    if (strncmp(command, "var_suppr(", 10) == 0){
        char var_suppr_name[MAX_LEN];
        char *start_var_suppr = command + 10;
        char *end_var_suppr = strrchr(start_var_suppr, ')');
        if (end_var_suppr != NULL) {
            int length_var_suppr = end_var_suppr - start_var_suppr;
            strncpy(var_suppr_name, start_var_suppr, length_var_suppr);
            var_suppr_name[length_var_suppr] = '\0';
            handle_var_suppr(var_suppr_name);
        }
        return;
    }

    if (strncmp(command,"affiche(",8)==0 || strncmp(command,"print(",6)==0){
        fonction_script_command_for_affiche(command);
        return;
    }

    // Supprimer les espaces blancs et retours à la ligne de l'expression
    command[strcspn(command, "\n")] = 0;
    replace_in_var(command); //fonction pour remplacer les in_var()
    //remplacer les time()
    strcpy(command,replace_actual_time(command));
    //remplacer les num_day()
    if (strstr(command,"num_day()")!=NULL){
        strcpy(command,handle_num_day(command));
    }
    //remplacer les random[,]
    handle_random(command,last_result);

    //fonction modulo
    if (strstr(command,"modulo(")!= NULL){
        strcpy(command,handle_modulo(command));
    }

    //pour fonction récupérer la virgule en entier
    if (strstr(command,"get_comma(")!= NULL || strstr(command,"prend_virgule(") != NULL){
        strcpy(command,get_comma_only(command));
    }

    //math best
    if (strstr(command,"arccos(")!= NULL){
            strcpy(command,handle_arccos(command));
    }
    if (strstr(command,"arcsin(")!= NULL){
            strcpy(command,handle_arcsin(command));
    }
    if (strstr(command,"arctan(")!= NULL){
            strcpy(command,handle_arctan(command));
    }
    if (strstr(command,"cos(")!= NULL){
            strcpy(command,handle_cos(command));
    }
    if (strstr(command,"sin(")!= NULL){
            strcpy(command,handle_sin(command));
    }
    if (strstr(command,"tan(")!= NULL){
            strcpy(command,handle_tan(command));
    }
    if (strstr(command,"log(")!= NULL){
            strcpy(command,handle_log(command));
    }
    if (strstr(command,"exp(")!= NULL){
            strcpy(command,handle_exp(command));
    }

    replace_comma_with_dot(command);

    if (strstr(command, "var|") != NULL) {
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(command, replace_rep(command, last_result));

            //supprimé les var|| après le '=' si il y en a un en les traitant
            char* command_after_equal2 = strchr(command, '=');
            if (command_after_equal2 != NULL) {
                // Sauvegarder la partie avant `=`
                char prefix2[256] = {0};
                strncpy(prefix2, command, command_after_equal2 - command);
                prefix2[command_after_equal2 - command] = '\0';  // Terminer proprement

                // Passer après '='
                command_after_equal2++;

                if (strstr(command_after_equal2, "var|") != NULL) {
                        strcpy(command_after_equal2, handle_variable(command_after_equal2));
                        replace_comma_with_dot(command_after_equal2);
                }
                // Construire la nouvelle commande avec le résultat
                snprintf(command, sizeof(command)+1, "%s=%s", prefix2, command_after_equal2);
            }

            // Gérer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
            strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son résultat
            strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son résultat
            // Vérification des commandes pour les aires
            if (strstr(command, "triangle(") != NULL || strstr(command, "cercle(") != NULL) {
                // Trouver le signe '='
                char* command_after_equal = strchr(command, '=');

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer proprement

                    // Passer après '='
                    command_after_equal++;

                    double aire = 0.00;

                    //regarder si il y a un sous appel d'une variable dans triangle ou cercle ex: 'var|x|=cercle(var|y|)', remplace 'var|y|'
                    if (strstr(command_after_equal, "var|") != NULL) {
                        strcpy(command_after_equal, handle_variable(command_after_equal));
                        replace_comma_with_dot(command_after_equal);
                    }

                    // Vérifier si c'est un triangle ou un cercle
                    if (strstr(command_after_equal, "triangle(") != NULL) {
                        aire = def_airetriangle(command_after_equal, last_result);
                    } else if (strstr(command_after_equal, "cercle(") != NULL) {
                        aire = def_airecercle(command_after_equal, last_result);
                    }

                    // Vérification de la valeur retournée
                    if (aire == -0.01) {
                        aire = 0.00;
                    }

                    // Construire la nouvelle commande avec le résultat
                    snprintf(command, 256, "%s=%.2f", prefix, aire);
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouvé, assigner "0.00"
                }
            }
            handle_implicit_multiplication(command);
            //printf("vérif %s\n",command);
            replace_comma_with_dot(command);
            strcpy(command, handle_variable(command));  // Remplace les variables

            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la chaîne caractère par caractère
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(command, "None") != NULL) {
                return;
                }
    }
    if (strstr(command,"execute(") != NULL){
        parse_and_execute(command);
        return;
    }
}

double condition_script_order(char *command) {
    //printf("aa %s\n", command);
    //fonction pour remplacer les in_var()
    replace_in_var(command);
    // Remplacer "rep" dans l'expression par la valeur de last_result
    strcpy(command, replace_rep(command, last_result));
    handle_implicit_multiplication(command);
    //remplacer les time()
    strcpy(command,replace_actual_time(command));
    //remplacer les num_day()
    if (strstr(command,"num_day()")!=NULL){
        strcpy(command,handle_num_day(command));
    }
    //remplacer les random [,]
    handle_random(command,last_result);

    //fonction modulo
    if (strstr(command,"modulo(")!= NULL){
        strcpy(command,handle_modulo(command));
    }

    //pour fonction récupérer la virgule en entier
    if (strstr(command,"get_comma(")!= NULL || strstr(command,"prend_virgule(") != NULL){
        strcpy(command,get_comma_only(command));
    }

    //math best
    if (strstr(command,"arccos(")!= NULL){
            strcpy(command,handle_arccos(command));
    }
    if (strstr(command,"arcsin(")!= NULL){
            strcpy(command,handle_arcsin(command));
    }
    if (strstr(command,"arctan(")!= NULL){
            strcpy(command,handle_arctan(command));
    }
    if (strstr(command,"cos(")!= NULL){
            strcpy(command,handle_cos(command));
    }
    if (strstr(command,"sin(")!= NULL){
            strcpy(command,handle_sin(command));
    }
    if (strstr(command,"tan(")!= NULL){
            strcpy(command,handle_tan(command));
    }
    if (strstr(command,"log(")!= NULL){
            strcpy(command,handle_log(command));
    }
    if (strstr(command,"exp(")!= NULL){
            strcpy(command,handle_exp(command));
    }

    replace_comma_with_dot(command);

    if (strstr(command, "var|") != NULL) {
            strcpy(command, handle_variable(command));  // Remplace les variables
            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la chaîne caractère par caractère
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(command, "None") != NULL) {
                //printf("ici");
                printf("Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                fflush(stdout);
                return 0.00;
                }
        }
    if (strncmp(command, "triangle(",9) == 0) {
        double resultat = def_airetriangle(command, last_result);
        //printf("%.2lf\n",resultat);
        return resultat;
    }
    if (strncmp(command, "cercle(",7) == 0){
        double resultat = def_airecercle(command, last_result);
        //printf("%.2lf\n",resultat);
        return resultat;
    }
    handle_implicit_multiplication(command);

    // Gérer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
    strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son résultat
    strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son résultat
    replace_comma_with_dot(command);

    //printf("%s\n",command);
    double resultat = eval(command);
    return resultat;
}

// Fonction d'évaluation d'une condition
int evaluate_condition(double v1, char *op, double v2) {
    if (strcmp(op, "?=") == 0) return v1 == v2;
    if (strcmp(op, "!=") == 0) return v1 != v2;
    if (strcmp(op, "<=") == 0) return v1 <= v2;
    if (strcmp(op, ">=") == 0) return v1 >= v2;
    if (strcmp(op, "<") == 0) return v1 < v2;
    if (strcmp(op, ">") == 0) return v1 > v2;
    return 0;
}

void execute_script(char *expr) {
    //si il y a une boucle
    if (strncmp(expr, "repeat||",8)== 0 || strncmp(expr, "répète||",8)== 0 || strncmp(expr, "boucle||",8)==0){
        int repetitions = 0;
        int a_0_repetitions = 0;
        char *start_num, *end_num;
        char *text_boucle = malloc(MAX_COMMAND_LEN);
        char *text_after_second_pipe = malloc(MAX_COMMAND_LEN);
        // Trouver la première occurence de "||" après "repeat||" ou similaire
        start_num = strstr(expr, "||") + 2;  // Avancer après "||"

        // Trouver la deuxième occurence de "||"
        end_num = strstr(start_num, "||");

        if (start_num && end_num) {
            // Extraire le texte entre les deux "||" (nombre de répétitions)
            size_t length = end_num - start_num;
            char num_str[length + 1];
            strncpy(num_str, start_num, length);
            num_str[length] = '\0';  // Terminer la chaîne

            if (strncmp(num_str,"infini",6)==0 || strncmp(num_str,"endless",7)==0){
                char *first_pipe = strstr(expr, "||");
                if (first_pipe) {
                    // Trouver la deuxième occurrence de "||"
                    char *second_pipe = strstr(first_pipe + 2, "||");
                    if (second_pipe) {
                        // Extraire le texte après la deuxième "||"
                        text_after_second_pipe = second_pipe + 2;  // Commence juste après "||"
                        char *end_of_text = strchr(text_after_second_pipe, '\0');  // Fin de la chaîne
                        if (end_of_text) {
                            // Calculer la longueur du texte à extraire
                            size_t length = end_of_text - text_after_second_pipe;
                            text_boucle = malloc(length + 6 + 1);  // Allouer mémoire pour "[[[" + texte + "]]]" + '\0'

                            // Ajouter "[[[" au début
                            strcpy(text_boucle, "[[[");

                            // Copier le texte après la deuxième "||"
                            strncat(text_boucle, text_after_second_pipe, length);

                            // Ajouter "]]]" à la fin
                            strcat(text_boucle, "]]]");

                            var_boucle_infini=1;

                            //printf("texte_boucle%s\n",text_boucle); //aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

                            while (a_0_repetitions==0){
                                if (strstr(text_boucle,"[[[]]]") == NULL) {
                                    //if (var_script_pause>0){return;}
                                    handle_script(text_boucle);
                                } else {
                                    a_0_repetitions=1;
                                }
                                if (var_script_exit==1){
                                    break;
                                }
                                if (var_script_pause!=0){
                                    var_script_pause=var_script_pause-1;
                                    break;
                                }
                            }
                            var_boucle_infini=0;
                        }
                    }
                }
            } else {

                // Appeler la fonction condition_script_order pour obtenir un double
                double repetitions_double = condition_script_order(num_str);

                // Convertir le double en entier (en arrondissant)
                repetitions = (int)repetitions_double;

                char *first_pipe = strstr(expr, "||");
                if (first_pipe) {
                    // Trouver la deuxième occurrence de "||"
                    char *second_pipe = strstr(first_pipe + 2, "||");
                    if (second_pipe) {
                        // Extraire le texte après la deuxième "||"
                        text_after_second_pipe = second_pipe + 2;  // Commence juste après "||"
                        char *end_of_text = strchr(text_after_second_pipe, '\0');  // Fin de la chaîne
                        if (end_of_text) {
                            // Calculer la longueur du texte à extraire
                            size_t length = end_of_text - text_after_second_pipe;
                            text_boucle = malloc(length + 6 + 1);  // Allouer mémoire pour "[[[" + texte + "]]]" + '\0'

                            // Ajouter "[[[" au début
                            strcpy(text_boucle, "[[[");

                            // Copier le texte après la deuxième "||"
                            strncat(text_boucle, text_after_second_pipe, length);

                            // Ajouter "]]]" à la fin
                            strcat(text_boucle, "]]]");

                            //printf("texte_boucle%s\n",text_boucle); //aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

                            //printf("%s\n",text_boucle);
                            while (a_0_repetitions<repetitions){
                                a_0_repetitions=a_0_repetitions+1;
                                if (strstr(text_boucle,"[[[]]]") == NULL) {
                                    //if (var_script_pause>0){return;}
                                    handle_script(text_boucle);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (strncmp(expr, "if||", 4) != 0 && strncmp(expr, "si||", 4) != 0) {
        execute_script_order(expr); //si il n'y a pas de if ni de repeat
        return;
    }

    if (strncmp(expr, "if||", 4) != 0 && strncmp(expr, "si||", 4) != 0) {return;}

    //gestion des conditions if
    char expr_copy[1024];
    strcpy(expr_copy, expr);

    char *token = strstr(expr_copy, "||");
    //printf(token);

    char v1[256] = {0}, operateur[3] = {0}, v2[256] = {0};

    // Extraire les valeurs et l'opérateur

    char *p = token + 2; // Après le premier "||"
    char *egal = strchr(p, '{'); // '{' à remplacer le '=' car les '{' sont forcement précente si un autre block commence (ou un block dans un block)

    // Lire `valeur1`
    char *fin_token = strstr(p, "||?");
    if (!fin_token || (egal && egal < fin_token)) {
            fin_token = strstr(p, "||!");
            if (!fin_token || (egal && egal < fin_token)) {
                fin_token = strstr(p, "||>");
                if (!fin_token || (egal && egal < fin_token)) {
                    fin_token = strstr(p, "||<");
                    if (!fin_token || (egal && egal < fin_token)) { printf("Erreur : format incorrect dans une condition.\n"); fflush(stdout); return; }
            }
        }
    }
    strncpy(v1, p, fin_token - p);
    v1[fin_token - p] = '\0';

    // Lire `operateur`
    p = fin_token + 2; // Passer `||`
    fin_token = strstr(p, "||");
    if (!fin_token) { printf("Erreur : format incorrect dans une condition.\n"); fflush(stdout); return; }
    strncpy(operateur, p, fin_token - p);
    operateur[fin_token - p] = '\0';

    // Lire `valeur2`
    p = fin_token +2; // Passer `||`
    fin_token = strstr(p, "||[["); // Fin de la valeur 2 juste avant `[[`
    if (!fin_token) {
            fin_token = strstr(p, "||{");
            if (!fin_token){
                fin_token = strstr(p, "|| ");
                if (!fin_token) { printf("Erreur : format incorrect dans une condition.\n"); fflush(stdout); return; }
            }
    }
    strncpy(v2, p, fin_token - p);
    v2[fin_token - p] = '\0';
    double valeur1 = condition_script_order(v1);
    double valeur2 = condition_script_order(v2);

    // Lire [[N#M]]
    egal = strchr(p, '{'); //egal cherche l'acolade car c'est plus sur que '=', il y en a forcement si il y a un autre block
    token = strstr(p, "[[");
    int n_valid = 0;
    if (token && (!egal || token < egal)) { //condition pour ignorer les [[ d'une autre condition (à l'intérieur de la première) si il n'y en a pas sur la première en utilosant le repère avant ou après le '='
        sscanf(token, "[[%d]]", &n_valid);
    } else {
        //printf("%s\n",expr);
        // Sauvegarder la position initiale de expr
        const char *initial_expr = expr;
        while (*expr != '\0') {
            if (*expr == '{') {
                n_valid++;
            }
            expr++;
        }
        // Remettre expr à sa position initiale
        expr = initial_expr;
    }
    //printf("%d\n",n_valid);

    // Vérifier la condition
    int condition_reussie = evaluate_condition(valeur1, operateur, valeur2);

    char *start = strchr(expr, '{');
    if (!start) {
        printf("Erreur : Aucune instruction à exécuter dans une condition.\n");
        fflush(stdout);
        return;
    }

    char result[1024] = "[[[";
    int open_braces = 0, valid_count = 0, else_count = 0;
    int block_started = 0;
    int skipped_blocks = 0;
    int take_all_remaining = 0;  //  Ajouté : Permet de prendre tout après `N` blocs ignorés
    char *cursor = start;

    while (*cursor) {
        if (*cursor == '{') {
            if (open_braces == 0) {
                if (condition_reussie) {
                    if (valid_count < n_valid) {
                        block_started = 1;
                        valid_count++;
                    } else {
                        block_started = 0;
                    }
                } else {
                    if (skipped_blocks < n_valid) {
                        skipped_blocks++;
                        block_started = 0;
                    } else {
                        take_all_remaining = 1;  //  Active la prise de tous les blocs suivants
                        block_started = 1;
                    }
                }
            }
            open_braces++;
        }

        if (block_started || take_all_remaining) {
            strncat(result, cursor, 1);
        }

        if (*cursor == '}') {
            open_braces--;
            if (open_braces == 0) {
                block_started = 0;
            }
        }

        cursor++;
    }

    strcat(result, "]]]");

    if (strstr(result,"[[[]]]") == NULL) {
        handle_script(result);
    }
    }

void handle_script(char *expr) {
    if (strncmp(expr, "[[[", 3) != 0) {
        printf("Erreur : format de script invalide.\n");
        fflush(stdout);
        return;
    }

    //vérifier si il y a eut un exit dans un block précédent
    if (var_script_exit==1){
        return;
    }


    char *start = strstr(expr, "{");
    char *end_script = strstr(expr, "]]]");

    if (!start || !end_script) {
        printf("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
        fflush(stdout);
        if (socket_var == 1) {
            send(client, "Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n", strlen("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n"), 0); //erreur courante donc gérer par le socket aussi
        }
        return;
    }

    while (start && start < end_script) {
        if (*start != '{') {
            start++; // Sécurité, on avance jusqu'à la prochaine accolade
            continue;
        }

        char *cursor = start + 1; // Début du contenu après '{'
        int open_braces = 1;  // Compteur d'accolades ouvertes
        char order[MAX_COMMAND_LEN] = {0}; // Buffer pour stocker l'ordre
        int index = 0;

        while (*cursor && cursor < end_script) {
            if (*cursor == '{') {
                open_braces++;
            } else if (*cursor == '}') {
                open_braces--;
                if (open_braces == 0) {
                    break; // Fin de l'ordre trouvé
                }
            }

            order[index++] = *cursor;
            cursor++;
        }

        order[index] = '\0'; // Terminaison de chaîne correcte

        //vérifier si il y a exit dans un block à exécuter
        if (strncmp(order, "exit", 4) == 0){
            var_script_exit = 1;
            return;
        }

        if (strncmp(order, "stop_pause()",12) == 0 || strncmp(order,"stop_break()",12) == 0) {
            var_script_pause=0;
        }

        if (strncmp(order,"pause(",6) == 0 || strncmp(order,"break(",5) == 0){
            char *start_pause = strchr(order, '(');  // Trouver la première parenthèse ouvrante
            char *end_pause = strchr(order, ')');    // Trouver la parenthèse fermante

            if (start_pause != NULL && end_pause != NULL && end_pause > start_pause) {
                // Extraire la valeur entre les parenthèses
                size_t length_pause = end_pause - start_pause - 1;  // Longueur entre '(' et ')'
                char num_str_pause[length_pause + 1];         // Stockage de la valeur sous forme de chaîne
                strncpy(num_str_pause, start_pause + 1, length_pause);
                num_str_pause[length_pause] = '\0';  // Terminer la chaîne

                // Convertir la valeur en double
                if (var_script_pause==0){
                    double pause_value = condition_script_order(num_str_pause);
                    var_script_pause=(int)pause_value;
                    if (var_boucle_infini==1){
                        var_script_pause=var_script_pause+1; //si le mode est en boucle infini on ajoute 1 pour utiliser l'appel dans la boucle infini
                    }
                    //printf("var pause2: %d\n",var_script_pause);
                    var_script_pause=var_script_pause-1;
                    if (var_script_pause>=0){return;}
                    //printf("%.2f\n", pause_value);
                }
            } else {
                printf("Erreur : format de pause/break invalide.\n");
                fflush(stdout);
            }
        }

        //printf("var pause: %d\n",var_script_pause);

        //vérifier si on à fait une demande de sortir d'un nombre de boucle et condition
        if (var_script_pause>0){
            var_script_pause=var_script_pause-1;
            return;
        }

        // Exécuter l'ordre
        execute_script(order);

        // Chercher la prochaine accolade
        start = strstr(cursor + 1, "{");
    }
}
//fin partie script

//fonction pour supprimé les espaces avant et après les sauts de ligne dans la lecture d'un fichier .frc et/ou .txt (fonction --start:"chemin du fichier")
void fonction_start_remove_newlines(char *str) {
    char *src = str;
    char *dst = str;
    int skipping_space = 1;

    while (*src) {
        if (*src == '\n' || *src == '\r') {
            src++;  // ignore line breaks
            skipping_space = 1;
            continue;
        }

        if (isspace((unsigned char)*src)) {
            if (!skipping_space) {
                *dst++ = ' ';
                skipping_space = 1;
            }
        } else {
            *dst++ = *src;
            skipping_space = 0;
        }

        src++;
    }

    *dst = '\0';
}


int autorise=0;

//-lws2_32 pour server tcp gestion de script
//gcc fr-simplecode0.2.c icon_frc.o -o fr-simplecode0.2.exe -lole32 -luuid -lsapi -lws2_32
int main(int argc, char *argv[]) {
    //printf("Arguments reçus :\n");
    //for (int i = 0; i < argc; i++) {
        //printf("argv[%d]: %s\n", i, argv[i]);
    //}
    if ((argc == 2 && (strcmp(argv[1], "--version") == 0)) || (argc == 2 && (strcmp(argv[1], "--v") == 0))) {
        char *exec_path = get_executable_path();
        printf("VERSION : 0.2\nutilisation win64_bit, path : %s\n", exec_path);
        fflush(stdout);
        return 0;  // Quitter immédiatement après avoir affiché la version
    }

    licence(); //créer la licence de fr-simplecode

    if (argc == 2 && (strcmp(argv[1], "--licence") == 0)){
            // Récupérer le chemin de l'exécutable pour changer de répertoire
            char exe_path[MAX_PATH];
            if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
                printf("Erreur lors de l'obtention du chemin de l'exécutable.\n");
                fflush(stdout);
                return 1;
            }

            // Extraire le répertoire de l'exécutable
            char *last_backslash = strrchr(exe_path, '\\');
            if (last_backslash != NULL) {
                *last_backslash = '\0'; // Terminer la chaîne à la position du dernier backslash
            }

            // Construire la commande pour changer de répertoire et ouvrir le fichier
            char command[MAX_PATH + 50];  // Buffer pour la commande
            snprintf(command, sizeof(command), "cd %s && start licence_fr-simplecode.txt", exe_path);

            // Exécuter la commande
            int licence_var = system(command);
            if (licence_var==0){
                printf("fichier licence_fr-simplecode.txt ouvert\n");
            } else {
                printf("Erreur d'ouverture de licence_fr_simplecode.txt\n");
            }
            fflush(stdout);
            return 0;
    }


    char command[MAX_COMMAND_LEN];
    if (isatty(STDIN_FILENO)) {
        // Exécuté directement dans le terminal
        //system("chcp 850 > nul");  // Active latin-1
        //setlocale(LC_ALL, "fr_FR.ISO8859-1");  // Utilise Latin-1
        system("chcp 1252 > nul");
        setlocale(LC_ALL, "French_France.1252");
    } else {
        // Exécuté par un autre programme
        setlocale(LC_ALL, "fr_FR.UTF-8");
    }
    //system("chcp 65001 > nul");
    //setlocale(LC_ALL, "fr_FR.UTF-8");

    if (argc == 2 && (strncmp(argv[1], "--e:",4) == 0)){
        char *expression = argv[1] + 4; // Pointeur sur ce qui suit "--e:"
        double result = condition_script_order(expression);
        printf("Résultat : %.2f\n", result);
        fflush(stdout);
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "--info") == 0)){
        printf("info  sur -- : utilisez --version ou --v pour version, --licence pour licence,\n--e: pour faire un calcul directement en le collant au ':' (! pas d'espace sinon erreur)\net ne prend que des calculs, pas de script '[[[]]]' !\nPour random, utiliser --random (nombre entre 0 et 1000)\n--start:'cheminduficher' lance le premier [[[]]] du fichier frc (ou .txt) qu'il y est un #init ou non\n--server:... il faut entré un port après les : (nombre int) le serveur démarre et ne peut prendre qu'un client,\nutilisé exit pour quitter, il peut lire des scripts [[[]]] et des commandes basiques (! pas de var|| !)\n! les erreurs ne sont pas envoyé sur le socket, regardé le serveur !\n");
        fflush(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--random") == 0) {
        // Initialisation du générateur pseudo-aléatoire avec l'heure actuelle
        srand((unsigned int)time(NULL));
        // Génère un nombre entre 0 et 1000 inclus
        int aleatoire = rand() % 1001;
        // Affiche le résultat
        printf("Résultat : %d\n", aleatoire);
        return 0;
    }

    if (argc == 2 && strncmp(argv[1], "--server:", 9) == 0) {
        WSADATA wsa;
        SOCKET server;
        struct sockaddr_in server_addr, client_addr;
        int port = atoi(argv[1] + 9);
        char buffer[MAX_COMMAND_LEN];

        printf("Démarrage du serveur sur le port %d...\n", port);

        // Initialiser Winsock
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            printf("Échec de l'initialisation de Winsock.\n");
            return 1;
        }

        // Créer le socket
        server = socket(AF_INET, SOCK_STREAM, 0);
        if (server == INVALID_SOCKET) {
            printf("Erreur de socket.\n");
            WSACleanup();
            return 1;
        }

        // Préparer la structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        // Lier le socket
        if (bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            printf("Erreur de bind.\n");
            closesocket(server);
            WSACleanup();
            return 1;
        }

        listen(server, 1);
        int c = sizeof(struct sockaddr_in);
        printf("Serveur en attente de connexion...\n");

        client = accept(server, NULL,NULL ); //(struct sockaddr *)&client_addr, &c
        if (client == INVALID_SOCKET) {
            printf("Erreur accept.\n");
            closesocket(server);
            WSACleanup();
            return 1;
        }

        printf("Client connecté.\n");


        Sleep(500);

        int sent = send(client, "connecte au serveur frc\n", strlen("connecte au serveur frc\n"), 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            printf("Erreur lors du send : %d\n", err);
        } else {
            printf("Message envoyé (%d octets)\n", sent);
        }

        int sock_fd = _open_osfhandle((intptr_t)client, 0);
        if (sock_fd == -1) {
            printf("Erreur _open_osfhandle\n");
            closesocket(client);
            WSACleanup();
            return 1;
        }

        socket_var=1;

        //int saved_stdout = _dup(_fileno(stdout));
        //_dup2(sock_fd, _fileno(stdout));
        //setvbuf(stdout, NULL, _IONBF, 0);

        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);


            if (bytes <= 0) {
                int err = WSAGetLastError();
                printf("Client déconnecté ou erreur (recv = %d) erreur:%s.\n", bytes,err);
                break;
            }

            if (buffer[0] == ' ') {
                // Décale tout vers la gauche (écrase le premier caractère) si cela commence par espace
                memmove(buffer, buffer + 1, strlen(buffer));
            }

            printf("reçu : %s\n",buffer);

            buffer[bytes] = '\0';

            if (strncmp(buffer, "exit", 4) == 0) {
                printf("Fin du programme.\n");
                send(client, "Fin du programme fr-simplecode.\n(tout droits reserves a argentropcher, asigne au compte Google argentropcher,\nmodification strictement interdite selon l'article L.111-1 du Code de la propriete intellectuelle)\n", strlen("Fin du programme fr-simplecode.\n(tout droits reserves a argentropcher, asigne au compte Google argentropcher,\nmodification strictement interdite selon l'article L.111-1 du Code de la propriete intellectuelle)\n"), 0);
                break;
            }

            if (strncmp(buffer,"[[[",3)==0){
                //buffer[strcspn(buffer, "\n")] = 0;
                //printf("script\n");
                var_script_exit= 0;
                var_script_pause= 0;
                mode_script_output = 0;
                var_affiche_virgule = 0;
                var_affiche_slash_n = 0;
                var_boucle_infini = 0;
                var_in_module=0;
                handle_script(buffer);
                continue;
            } else {
                mode_script_output = 1;
            }

            if (strncmp(buffer,"error_sound()",13)==0 || strncmp(buffer,"son_erreur()",12)==0){
                play_error_sound();
                continue;
            }

            if (strncmp(buffer, "speak(", 6) == 0 || strncmp(buffer, "parle(", 6) == 0) {
                handle_speak(buffer);
                continue;
            }

            strcpy(buffer,replace_actual_time(buffer));
            //remplacer les num_day()
            if (strstr(buffer,"num_day()")!=NULL){
                strcpy(buffer,handle_num_day(buffer));
            }
            //remplacer les random[,]
            handle_random(buffer,last_result);

            //fonction modulo
            if (strstr(buffer,"modulo(")!= NULL){
                strcpy(buffer,handle_modulo(buffer));
            }

            //pour fonction récupérer la virgule en entier
            if (strstr(buffer,"get_comma(")!= NULL || strstr(buffer,"prend_virgule(") != NULL){
                strcpy(buffer,get_comma_only(buffer));
            }

            //math best
            if (strstr(buffer,"arccos(")){
                strcpy(buffer,handle_arccos(buffer));
            }
            if (strstr(buffer,"arcsin(")){
                strcpy(buffer,handle_arcsin(buffer));
            }
            if (strstr(buffer,"arctan(")!= NULL){
                strcpy(buffer,handle_arctan(buffer));
            }
            if (strstr(buffer,"cos(")!= NULL){
                strcpy(buffer,handle_cos(buffer));
            }
            if (strstr(buffer,"sin(")!= NULL){
                strcpy(buffer,handle_sin(buffer));
            }
            if (strstr(buffer,"tan(")!= NULL){
                strcpy(buffer,handle_tan(buffer));
            }
            if (strstr(buffer,"log(")!= NULL){
                strcpy(buffer,handle_log(buffer));
            }
            if (strstr(buffer,"exp(")!= NULL){
                strcpy(buffer,handle_exp(buffer));
            }

            char* modified_command = replace_rep(buffer, last_result);

            //remplacer les ',' par des '.'
            replace_comma_with_dot(modified_command);

            // Évaluer l'expression et renvoyer le résultat
            double result = eval(modified_command);
            printf("%.2lf\n", result);
            snprintf(buffer, sizeof(buffer), "%.2lf\n", result);  // Formate le double dans une chaîne
            send(client, buffer, strlen(buffer), 0);

            last_result=result;
        }

        //_dup2(saved_stdout, _fileno(stdout));
        //_close(saved_stdout);
        //_close(sock_fd);       // socket transformé en handle -> on close handle
        closesocket(client);   // socket original à fermer proprement
        closesocket(server);
        WSACleanup();

        socket_var=0;

        return 0;
    }

    if (argc >= 2 && strncmp(argv[1], "--start:", 8) == 0) {
        // 1. Construire le chemin complet
        char path[1024] = {0};
        strcat(path, argv[1] + 8); // commence après "--start:"

        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "--", 2) == 0) break; // stop si nouveau paramètre
            strcat(path, " ");
            strcat(path, argv[i]);
        }

        // 2. Vérifier si l'extension est présente
        if (!strstr(path, ".txt") && !strstr(path, ".frc")) {
            strcat(path, ".frc");
        }

        // 3. Lire le fichier
        FILE *f = fopen(path, "r");
        if (!f) {
            printf("Impossible d'ouvrir : %s\n", path);
            fflush(stdout);
            return 1;
        }

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        rewind(f);

        char *buffer = malloc(len + 1);
        fread(buffer, 1, len, f);
        buffer[len] = '\0';
        fclose(f);

        // 4. Trouver [[[ et ignorer le reste
        char *script_start = strstr(buffer, "[[[");
        if (!script_start) {
            printf("Aucune balise [[[ trouvée.\n");
            fflush(stdout);
            free(buffer);
            return 1;
        }

        // 5. Nettoyer le texte (sans [[[)
        char *clean_script = strdup(script_start);
        fonction_start_remove_newlines(clean_script);

        // 6. Lancer le script
        if (strncmp(clean_script, "[[[", 3) == 0) {
            var_script_exit = 0;
            var_script_pause = 0;
            mode_script_output = 0;
            var_affiche_virgule = 0;
            var_affiche_slash_n = 0;
            var_boucle_infini = 0;
            var_in_module=0;
            handle_script(clean_script);
        }

        free(buffer);
        free(clean_script);
        return 0;
    }

    printf("fr-simplecode on windows : (tapez 'info' pour les fonctions ou 'exit' pour quitter) \n");
    fflush(stdout);
    // Initialiser le chemin du fichier
    initialize_filename();

    last_result = 0.00;  // Stocke le dernier résultat
    var_script_exit = 0; //configure qu'il n'y a pas eut d'appel exit dans script (1 si il y a un appel)
    var_script_pause = 0; //configure le nombre de sortie de boucle (0 pas de sortie, autre, nombre de sortie de boucle ou condition)
    mode_script_output = 0; //sortie classique pour script
    var_affiche_virgule = 0; //sortie pour les print()/affiche() avec les nombres à virgule ou sans
    var_affiche_slash_n = 0; //sortie pour les print()/affiche() avec le retour à la ligne autoatique ou non
    var_boucle_infini = 0; //gestion des mode en boucle infini ou non (1 ou 0)
    var_sortir_entree=false; //gestion de l'affichage de l'entrée de l'utilisateur ou non
    var_in_module=0; //pour savoir si on est dans un module durant l'exécution du code pour pouvoir utilisez [input]

    srand(time(NULL));

    while (1) {
        // Lire une commande du pipe (entrée standard)
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;  // Si aucune commande, on sort de la boucle
        }


        // Vérifier si l'utilisateur a demandé d'arrêter
        if (strncmp(command, "exit", 4) == 0) {
            printf("Fin du programme fr-simplecode.\n(tout droits réservés à argentropcher, asigné au compte Google argentropcher,\nmodification strictement interdite selon l'article L.111-1 du Code de la propriété intellectuelle)\n");
            fflush(stdout);
            break;
        } else if (strncmp(command, "info", 4) == 0) {
            printf("INFO :\nLes calculs pris en compte sont basique: addition, soustraction, multiplication, division.\n(pour les virgules, utilisez '.')\nFonction :\n'rep' pour reprendre le résultat précédent\n");
            fflush(stdout);
            printf("'random[nombre1,nombre2]' renvoie un nombre aléatoire entre les 2 nombres entiers choisi\n'execute()' permet d'executer des commandes\n'get_comma(calcul)' et 'prend_virgule(calcul)' récupère les deux chiffres après la virgule en entier\n'var|'nomdelavariable'|' permet de créer/modifier des variables d'environnement (voir var.txt):\nutilisez 'var|nomdelavariable|=nombre/calcul/cercle()/triangle(;)/random[,]...' permet de définir la fonction\n'var|nomdelavariable|' la rappel du fichier si elle existe, (sinon valeur 0.00) pour l'utiliser dans un calcul\n+utiliser la fonction 'var_list()' pour obtenir la liste des variables de var.txt,\net 'var_suppr(all)' pour supprimé le fichier var.txt\nou 'var_suppr(nomdelavariable)' pour supprimé une variable du fichier var.txt\n'in_var(votrerecherche)' permet de savoir si il y a votrerecherche dans var.txt\nque se soit un nom de variable ou une valeur.\n");
            fflush(stdout);
            printf("'m' pour sauvgarder le résultat précédent\n'm+' pour importer le résultat sauvgarder et le stocker dans 'rep'\n'sqrt(argument)' pour la racine carrée\n'cbrt(argument)' pour la racine cubique\nLes fonctions exp(), log(), tan(), arctan(), sin(), arcsin(), cos(), arccos() sont également disponible\n(éviter de les mettre les uns dans les autres car cela peut provoquer des erreurs, utilisez surtout des variables).\n'modulo(calcul1;calcul2)' permet de faire un modulo (comme le pourcentage de python sauf que le séparateur est ';')\n");
            fflush(stdout);
            printf("'local(argument)' pour sauvgarder une variable dans la mémoire vive avec en argument un calcul\n'inlocal()' pour récupérer la valeur de 'local(argument)' et la mettre dans 'rep'\n");
            fflush(stdout);
            printf("'triangle(x;y)'pour calculer l'air d'un triangle de base=x et de hauteur=y\n'cercle(rayon)' pour calculer l'aire du cercle\n'fonction(argument)' pour plus d'info sur les fonctions\n'version' pour version\n'licence' pour licence\n'exit' pour quitter\n");
            fflush(stdout);
            printf("PARTIE CODE/PROGRAMME :\nLes programmes ont une structure de block imbriquée les un dans les autres.\nUn programme commence toujours par '[[[' et fini toujours par ']]]'.\nLes blocks à l'intérieur de cela commence et finissent toutjours par '{' et '}'.\nLes différents blocks :\n");
            fflush(stdout);
            printf("'{print(argument)}' ou '{affiche(argument)}' sont des blocks pour renvoié un résultat\n l'argument peut être un calcul ou  un texte entre '' double.\nPour mettre 2 calcul différents sans texte, s'éparer les par des '' double collé.\n");
            fflush(stdout);
            printf("'{var|nomdelavariable|=valeurendouble}' est la même chose que précédament et permet de définir une variable\nutiliser 'var|nomdelavariable|' pour la rappeler.\nLes fonctions 'var_list()', 'var_suppr(nomdelavariableoutout)'\net 'in_var(votrevaleurouvariablerechercher)' sont toujours disponible en les intégrants dans des '{}'.\n");
            fflush(stdout);
            printf("'{if||v1||condition||v2||accoladeimbriqué{}}' le if peut être remplacer par si, v1 et v2 sont des calculs, variable ...\ncondition peut être '?=' si c'est égal, '!=' si ce n'est pas égal, '<', '>', '<=' ou '>='.\nOn peut également rajouté en cas de condition fausse [[nombrede{}àsauterencasdeconditionfausse]]\nqui saute les premier {} si la condition est fausse.\n");
            fflush(stdout);
            printf("'{repeat||nombrederepetition||accoladeimbriqué{}...}', le mot 'repeat' peut être remplacer par 'boucle' ou par 'répète'\n nombrederépétition est le nombre de fois qu'il va exécuter la boucle,\nil peut aussi être 'infini' ou 'endless' pour fonctionné indéfiniment.\nPour arrêter une boucle, ont peut utiliser 'pause(nombred'arrêtdeboucle)' ou 'break(nombred'arrêtdeboucle)'\nqui permet de sortir des boucles\nsi la boucle est 'infini' ou 'endless', le nombred'arrêt à 1 suffi,\nsinon, il faut compter le nombre de répétition à arrêter\nSi vous ne voulez pas compter ou si la logique est trop compliquée,\nmettez une grande valeur et utilisez 'stop_break()' ou 'stop_pause()' à la sortie de la boucle.\n");
            fflush(stdout);
            printf("'{exit}' sort du programme complètement.\nFonction supplémentaire de l'affichage :\n");
            fflush(stdout);
            printf("'output_input(false)' et 'sortir_entrée(faux)' désactive le renvoie de l'entrée de l'utilisateur,\n'output_input(true)' et 'sortir_entrée(vrai)' l'active\n'{###sortie:classique###}', '{###sortie:normale###}' et '{###output:normal###}'désactive\nles messages script lors de l'execution,\n'{###sortie:tout###}', '{###sortie:all###}' et '{###output:all###}' active les messages script\nPOUR LES PRINT/AFFICHE :\n'{###sortie_affiche:pasdevirgule###}', '{###output_print:notcomma###}' n'affiche pas les virgules\ndes nombres dans les print() et affiche()\n'{###sortie_affiche:[/n]###}' et '{###output_print:[/n]###}' active le saut de ligne automatique dans la console\n'{###sortie_affiche:pasde[/n]###}' et '{###output_print:not[/n]###}' le désactive\nSi vous voulez sauter de ligne dans un print, écrivez entre '' double '[/n]'\n");
            fflush(stdout);
            printf("SON :\n'error_sound()' et 'son_erreur()' permet de jouer le son erreur de windows,\n'speak(arg)' et 'parle(arg)' disent arg (! pas de calcul, uniquement du texte,\nvar|nomdevariable| en premier fonctionne pour dire la valeur numérique)\n");
            fflush(stdout);
            continue;
        } else if (strncmp(command, "version", 7) == 0){
            char *exec_path = get_executable_path();
            printf("VERSION : 0.2\nutilisation win64_bit, path : %s\n", exec_path);
            fflush(stdout);
            continue;
        } else if (strncmp(command,"fonction(",9)==0){
            if (strncmp(command,"fonction()",10)==0 || strncmp(command,"fonction( )",10)==0 || strncmp(command,"fonction(fonction())",20)==0){
                printf("'fonction()' permet d'expliquer en détaille une fonction (écricre : 'fonction(la_fonction_choisie)'\n");
            } else if (strncmp(command,"fonction(m+)",12)==0){
               printf("'m+' permet de récupérer la valeur de 'm' enregistré dans result.txt\n");
            } else if (strncmp(command,"fonction(m)",11)==0) {
                printf("'m' permet de sauvgarder la valeur du résultat précédent dans une variable à long terme,\non peut récupérer sa valeur lors d'une autre exécution de calculatrice basique C\n");
            } else if (strncmp(command,"fonction(rep)",13)==0) {
                printf("'rep' récupère automatiquement le résultat de vôtre calcul précédent,\nvous pouvez l'utiliser directement dans un calcul (ex:'5/-rep+sqrt(rep*8)')\n");
            } else if (strncmp(command,"fonction(sqrt())",16)==0 || strncmp(command,"fonction(sqrt(argument))",24)==0) {
                printf("'sqrt(argument)' permet de calculer la racine carré d'un nombre,\nil s'utilise directement dans un calcul (ex:'50-sqrt(5+2*2)/rep')\n");
            } else if (strncmp(command,"fonction(cbrt())",16)==0 || strncmp(command,"fonction(cbrt(argument))",24)==0) {
                printf("'cbrt(argument)' permet de calculer la racine cubique d'un nombre,\nil s'utilise directement dans un calcul (ex:'cbrt(5+22)+7')\n");
            } else if (strncmp(command,"fonction(local())",17)==0 || strncmp(command,"fonction(local(argument))",25)==0) {
                printf("'local(argument)' permet de calculer des '+', '-', '*', '/' et des 'sqrt()' et 'cbrt()' dans les parenthèses,\nils sont sauvgardés dans une mémoire vive uniquement lors de l'utilisation de l'appli,\nmais ne prend pas d'argument hors des parenthèses (ex: 'local(5+cbrt(rep))')\n");
            } else if (strncmp(command,"fonction(inlocal())",19)==0) {
                printf("'inlocal()' permet de renvoyer la valeur stockée durant la fonction 'local(argument)'(voir : 'fonction(local())'),\nla valeur est renvoyer dans rep (ex: 'inlocal()')\n");
            } else if (strncmp(command,"fonction(triangle(x;y))",23)==0 || strncmp(command,"fonction(triangle())",20)==0) {
                printf("'triangle(x;y)' permet de calculer l'aire d'un triangle (base*hauteur/2),\nx et y ne doivent être que des nombres et non des calculs,\non ne peut pas mettre des arguments à l'extérieur (ex: 'triangle(3;rep)')\n");
            } else if (strncmp(command,"fonction(cercle())",18)==0 || strncmp(command,"fonction(cercle(rayon))",23)==0) {
                printf("'cercle(rayon)' permet de calculer l'aire d'un cercle (pi*rayon²),\nrayon doit être uniquement un nombre et non un calcul,\non ne peut pas mettre d'argument hors de la parenthèse (ex: 'cercle(3)')\n");
            } else if (strncmp(command,"fonction(var||)",15)==0 || strncmp(command,"fonction(var)",13)==0) {
                printf("'var||' permet de créer des variables qui sont stocké uniquement sous la forme de double,\npour la définir, on utilise = après son nom, et pour l'utiliser, il sufit d'écrire 'var|nomdelavariable|'.\nSi une erreur survient lors de sa définition ou si elle n'existe pas, ça valeur est automatiquement 0.00.\n");
            } else if (strncmp(command,"fonction(var_list())",20)==0 || strncmp(command,"fonction(var_list)",18)==0) {
                printf("'var_list()' permet de voir la liste des variables du fichier var.txt\n");
            }else if (strncmp(command,"fonction(random[])",18)==0 || strncmp(command,"fonction(random[nombre1,nombre2])",33)==0 || strncmp(command,"fonction(random)",16)==0){
                printf("fonction 'random[nombre1,nombre2]' renvoie un nombre entier aléatoire compris entre les 2 nombres entiers choisi par l'utilisateur\n");
            } else if (strncmp(command,"fonction(execute())",19)==0 || strncmp(command,"fonction(execute)",17)==0){
                printf("'execute()' permet de passer des commandes de type cmd depuis l'application, par exemple pour démarer un programme grâce à la fonction start du cmd ...\n");
            } else {
                printf("Erreur : 'fonction(argument)' argument inconnu,\ntapez 'info' pour connaître les différentes fonctions !\n");
            }
            fflush(stdout);
            continue;
        } else if (strncmp(command,"script_example",14)==0){
            printf("example : [[["
        "{var|seconde|=0}"
        "{var|seconde_debut|=time(s)}"
        "{var|minute_debut|=time(m)}"
        "{###output_print:notcomma###}"
        "{repeat||infini||"
            "{if||var|seconde|||<||time(s)||"
                "{print(\"Il est : \" time(h) \" h \" time(m) \" et \" time(s) \" secondes.\")}"
                "{var|seconde|=time(s)}"
            "}"
            "{if||time(s)||?=||0||"
                "{if||var|seconde|||?=||59||{var|seconde|=-1}}"
            "}"
            "{if||var|minute_debut|||!=||time(m)||"
                "{if||var|seconde_debut|||<=||time(s)||"
                    "{print(\"indice\")}"
                    "{break(1)}"
                "}"
            "}"
        "}"
        "{print(\"fin\")}"
        "]]]\n");
            fflush(stdout);
            continue;
        } else if (strncmp(command,"licence",7)==0){
            licence();
            // Récupérer le chemin de l'exécutable pour changer de répertoire
            char exe_path[MAX_PATH];
            if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
                printf("Erreur lors de l'obtention du chemin de l'exécutable.\n");
                fflush(stdout);
                continue;;
            }

            // Extraire le répertoire de l'exécutable
            char *last_backslash = strrchr(exe_path, '\\');
            if (last_backslash != NULL) {
                *last_backslash = '\0'; // Terminer la chaîne à la position du dernier backslash
            }

            // Construire la commande pour changer de répertoire et ouvrir le fichier
            char command[MAX_PATH + 50];  // Buffer pour la commande
            snprintf(command, sizeof(command), "cd %s && start licence_fr-simplecode.txt", exe_path);

            // Exécuter la commande
            int licence_var = system(command);
            if (licence_var==0){
                printf("fichier licence_fr-simplecode.txt ouvert\n");
            } else {
                printf("Erreur d'ouverture de licence_fr_simplecode.txt\n");
            }
            fflush(stdout);
            continue;
        }

        //zone pour réaficher la commande de l'utilisateur ou non
        if (strncmp(command,"output_input(false)",19)==0 || strncmp(command,"sortir_entrée(faux)",19)==0){
            var_sortir_entree=false;
            printf("l'affichage de l'entrée de l'utilisateur est désactivé\n");
            fflush(stdout);
            continue;
        }
        if (var_sortir_entree==true){
            printf("%s",command);
            fflush(stdout);
        }
        if (strncmp(command,"output_input(true)",18)==0 || strncmp(command,"sortir_entrée(vrai)",19)==0){
            var_sortir_entree=true;
            printf("l'affichage de l'entrée de l'utilisateur est activé\n");
            fflush(stdout);
            continue;
        }
        //fin zone pour réaficher la commande de l'utilisateur ou non

        if (strncmp(command, "var_list()", 10) == 0) {
            handle_var_list();
            continue;
        }
        if (strncmp(command, "var_suppr(", 10) == 0){
            char var_suppr_name[MAX_LEN];
            char *start_var_suppr = command + 10;
            char *end_var_suppr = strrchr(start_var_suppr, ')');
            if (end_var_suppr != NULL) {
                int length_var_suppr = end_var_suppr - start_var_suppr;
                strncpy(var_suppr_name, start_var_suppr, length_var_suppr);
                var_suppr_name[length_var_suppr] = '\0';
                handle_var_suppr(var_suppr_name);
            }
            continue;
        }

        if (strncmp(command,"in_var(",7) == 0){
            // Trouver la position de la parenthèse ouvrante et fermante
            char *start_in_var = strchr(command, '(');  // Trouve '('
            char *end_in_var = strchr(command, ')');    // Trouve ')'

            if (start_in_var != NULL && end_in_var != NULL && end_in_var > start_in_var) {
                // Extraire le contenu entre les parenthèses
                size_t length = end_in_var - start_in_var - 1;  // longueur entre '(' et ')'
                char value[length + 1];  // +1 pour le caractère null '\0'
                strncpy(value, start_in_var + 1, length);  // Copie tout sauf '(' et ')'
                value[length] = '\0';  // Ajouter le caractère de fin de chaîne

                // Passer la valeur extraite à handle_in_var()
                double result_in_var = handle_in_var(value);

                // Afficher le résultat
                printf("Le résultat de in_var est: %.2f (1.00 = recherche trouvé, 0.00 = pas trouvé dans le fichier)\n", result_in_var);
                fflush(stdout);
            } else {
                printf("Erreur : parenthèses mal formées.\n");
                fflush(stdout);
            }
            continue;
        }

        if (strncmp(command,"error_sound()",13)==0 || strncmp(command,"son_erreur()",12)==0){
            play_error_sound();
            continue;
        }

        if (strncmp(command, "speak(", 6) == 0 || strncmp(command, "parle(", 6) == 0) {
            handle_speak(command);
        }

        if (strncmp(command,"[[[",3)==0){
            //command[strcspn(command, "\n")] = 0;
            //printf("script\n");
            var_script_exit= 0;
            var_script_pause= 0;
            mode_script_output = 0;
            var_affiche_virgule = 0;
            var_affiche_slash_n = 0;
            var_boucle_infini = 0;
            var_in_module=0;
            handle_script(command);
            continue;
        } else {
            mode_script_output = 1;
        }

        // Supprimer les espaces blancs et retours à la ligne de l'expression
        command[strcspn(command, "\n")] = 0;
        //remplacer les time()
        strcpy(command,replace_actual_time(command));
        //remplacer les num_day()
        if (strstr(command,"num_day()")!=NULL){
            strcpy(command,handle_num_day(command));
        }
        //remplacer les random[,]
        handle_random(command,last_result);

        //fonction modulo
        if (strstr(command,"modulo(")!= NULL){
            strcpy(command,handle_modulo(command));
        }

        //pour fonction récupérer la virgule en entier
        if (strstr(command,"get_comma(")!= NULL || strstr(command,"prend_virgule(") != NULL){
            strcpy(command,get_comma_only(command));
        }

        //math best
        if (strstr(command,"arccos(")){
            strcpy(command,handle_arccos(command));
        }
        if (strstr(command,"arcsin(")){
            strcpy(command,handle_arcsin(command));
        }
        if (strstr(command,"arctan(")!= NULL){
            strcpy(command,handle_arctan(command));
        }
        if (strstr(command,"cos(")!= NULL){
            strcpy(command,handle_cos(command));
        }
        if (strstr(command,"sin(")!= NULL){
            strcpy(command,handle_sin(command));
        }
        if (strstr(command,"tan(")!= NULL){
            strcpy(command,handle_tan(command));
        }
        if (strstr(command,"log(")!= NULL){
            strcpy(command,handle_log(command));
        }
        if (strstr(command,"exp(")!= NULL){
            strcpy(command,handle_exp(command));
        }

        if (strstr(command, "var|") != NULL) {
            //remplacer les in_var()
            replace_in_var(command);
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(command, replace_rep(command, last_result));
            // Gérer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
            strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son résultat
            strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son résultat
            // Vérification des commandes pour les aires
            if (strstr(command, "triangle(") != NULL) {
                // Trouver le signe '=' dans la commande
                char* command_after_equal = strchr(command, '=');  // Trouver '='

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer la chaîne proprement

                    // Passer la commande après '=' à la fonction
                    command_after_equal++;  // Avancer d'un caractère pour ignorer '='
                    double aire_triangle = def_airetriangle(command_after_equal, last_result); // Appel de la fonction aire du triangle

                    if (aire_triangle != -0.01) {
                        snprintf(command, sizeof(command), "%s=%.2f", prefix, aire_triangle); // Convertir l'aire du triangle en chaîne de caractères
                    } else {
                        snprintf(command, sizeof(command), "%s=0.00", prefix); // Si l'aire est invalide, assigner "0.00"
                    }
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouvé, assigner "0.00"
                }
            } else if (strstr(command, "cercle(") != NULL) {
                // Trouver le signe '=' dans la commande
                char* command_after_equal = strchr(command, '=');  // Trouver '='

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer la chaîne proprement

                    // Passer la commande après '=' à la fonction
                    command_after_equal++;  // Avancer d'un caractère pour ignorer '='
                    double aire_cercle = def_airecercle(command_after_equal, last_result); // Appel de la fonction aire du cercle

                    if (aire_cercle != -0.01) {
                        snprintf(command, sizeof(command), "%s=%.2f", prefix, aire_cercle); // Convertir l'aire du cercle en chaîne de caractères
                    } else {
                        snprintf(command, sizeof(command), "%s=0.00", prefix); // Si l'aire est invalide, assigner "0.00"
                    }
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouvé, assigner "0.00"
                }
            }
            handle_implicit_multiplication(command);
            //printf("%s\n",command);
            replace_comma_with_dot(command);
            strcpy(command, handle_variable(command));  // Remplace les variables

            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la chaîne caractère par caractère
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(command, "None") != NULL) {
                continue;
                }
        }

        if (strstr(command,"execute(") != NULL){
            parse_and_execute(command);
            continue;
        }

        // Vérifier que l'expression n'est pas vide
        if (strlen(command) == 0) {
            printf("Commande vide, réessayez\n");
            fflush(stdout);
            continue;
        } else if (strcmp(command, "m") != 0) {
            autorise=0;
        }

        // Gestion des commandes spéciales
        if (strcmp(command, "m") == 0) {
            // Sauvegarder le dernier résultat dans un fichier
            if (last_result !=0.00){
                save_result(last_result);
                continue;
            } else {
                if (autorise==1){
                    autorise=0;
                    save_result(last_result);
                    continue;
                } else {
                    printf("êtes vous sur de vouloir sauvgarder une valeur null ? Si oui, refaite 'm', sinon faite une autre commande\n");
                    autorise=1;
                    continue;
                }
            }
        } else if (strcmp(command, "m+") == 0) {
            // Charger le résultat depuis un fichier et le placer dans last_result
            last_result = load_result();
            continue;
        }
        if (strncmp(command, "triangle(",9) == 0) {
            double airtriangle = def_airetriangle(command, last_result);
            printf("aire du triangle : %.2lf", airtriangle);
            printf(" m²\n");
            fflush(stdout);  // Envoyer immédiatement le résultat à Python
            if (airtriangle!=-0.01){
                last_result=airtriangle;
                }
            continue;
        } else if (strncmp(command, "cercle(", 7) == 0) {
            double aircercle = def_airecercle(command, last_result);
            printf("aire du cercle : %.2lf", aircercle);
            printf(" m²\n");
            fflush(stdout);  // Envoyer immédiatement le résultat à Python
            if (aircercle!=-0.01){
                last_result=aircercle;
                }
            continue;
        } else if (strncmp(command, "local(", 6) == 0) {
            // Extraire l'expression à l'intérieur de local()
            char* start_expr = command + 6;
            char* end_expr =  strrchr(start_expr, ')');
            if (end_expr) {
                *end_expr = '\0'; // Terminer l'expression à la parenthèse fermante
                set_local(start_expr, last_result);
            } else {
                printf("Erreur : parenthèse fermante manquante pour local().\n");
                fflush(stdout);
            }
            continue;
        } else if (strcmp(command, "inlocal()") == 0) {
            last_result = get_local();
            printf("rep = %.2lf\n", last_result);
            fflush(stdout);
            continue;
        }

        // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
        if (strstr(command, "sqrt(") != NULL) {
            char* processed_command2="";
            // Gérer les expressions sqrt
            char* processed_command = handle_sqrt(command, last_result);
            // Afficher la commande après traitement de sqrt pour diagnostic
            ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande traitée
            for (int i = 0; processed_command[i] != '\0'; i++) {
                if (processed_command[i] == ',') {
                    processed_command[i] = '.'; // Remplacer la virgule par un point
                }
            }
            // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
            if (strstr(command, "cbrt(") != NULL) {
                // Gérer les expressions sqrt
                processed_command2 = handle_cbrt(processed_command, last_result);
                // Afficher la commande après traitement de sqrt pour diagnostic
                ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
                // Remplacer les virgules par des points dans la commande traitée
                for (int i = 0; processed_command2[i] != '\0'; i++) {
                    if (processed_command2[i] == ',') {
                        processed_command2[i] = '.'; // Remplacer la virgule par un point
                    }
                }
            } else {
                processed_command2 = processed_command;
            }
            // Remplacer "rep" dans l'expression par la valeur de last_result
            char* processed_command3 = replace_rep(processed_command2, last_result);
            // Évaluer l'expression et renvoyer le résultat
            double result = eval(processed_command3);
            printf("%.2lf\n", result);
            fflush(stdout);  // Envoyer immédiatement le résultat à Python
            last_result = result;
            continue;  // Continuer avec la prochaine itération de la boucle
        }

        // Gestion de l'expression sqrt(nombre) à n'importe quel endroit
        if (strstr(command, "cbrt(") != NULL) {
            char* processed_command2="";
            // Gérer les expressions sqrt
            char* processed_command = handle_cbrt(command, last_result);
            // Afficher la commande après traitement de sqrt pour diagnostic
            ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande traitée
            for (int i = 0; processed_command[i] != '\0'; i++) {
                if (processed_command[i] == ',') {
                    processed_command[i] = '.'; // Remplacer la virgule par un point
                }
            }
            if (strstr(command, "sqrt(") != NULL) {
                // Gérer les expressions sqrt
                processed_command2 = handle_sqrt(processed_command, last_result);
                // Afficher la commande après traitement de sqrt pour diagnostic
                ///printf("Commande après traitement de 'sqrt' : %s\n", processed_command);
                // Remplacer les virgules par des points dans la commande traitée
                for (int i = 0; processed_command2[i] != '\0'; i++) {
                    if (processed_command2[i] == ',') {
                        processed_command2[i] = '.'; // Remplacer la virgule par un point
                    }
                }
            } else {
                processed_command2 = processed_command;
            }
            // Remplacer "rep" dans l'expression par la valeur de last_result
            char* processed_command3 = replace_rep(processed_command2, last_result);
            // Évaluer l'expression et renvoyer le résultat
            double result = eval(processed_command3);
            printf("%.2lf\n", result);
            fflush(stdout);  // Envoyer immédiatement le résultat à Python
            last_result = result;
            continue;  // Continuer avec la prochaine itération de la boucle
        }

        // Remplacer "rep" dans l'expression par la valeur de last_result
        char* modified_command = replace_rep(command, last_result);

        // Afficher la commande après remplacement de "rep" pour diagnostic
        ///printf("Commande après remplacement de 'rep' : %s\n", modified_command);

        //remplacer les ',' par des '.'
        replace_comma_with_dot(modified_command);

        // Évaluer l'expression et renvoyer le résultat
        double result = eval(modified_command);
        printf("%.2lf\n", result);
        fflush(stdout);  // Envoyer immédiatement le résultat à Python

        last_result=result;
    }

    return 0;
}
