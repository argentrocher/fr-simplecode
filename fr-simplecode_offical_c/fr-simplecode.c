#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <windows.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define MAX_COMMAND_LEN 16384
#define MAX_LEN 1024

//fonction licence :
void licence() {
    // Obtenir le chemin absolu de l'ex�cutable
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        // Si l'obtention du chemin �choue, afficher une erreur
        printf("Erreur lors de l'obtention du chemin de l'ex�cutable.\n");
        fflush(stdout);
        return;
    }

    // Extraire le r�pertoire de l'ex�cutable
    char *last_backslash = strrchr(exe_path, '\\');
    if (last_backslash != NULL) {
        *last_backslash = '\0'; // Terminer la cha�ne � la position du dernier backslash
    }

    // Construire le chemin complet du fichier de licence
    char licence_path[MAX_PATH];
    snprintf(licence_path, sizeof(licence_path), "%s\\licence_fr-simplecode.txt", exe_path);

    // V�rifier si le fichier existe d�j�
    FILE *file = fopen(licence_path, "r");
    if (file) {
        // Si le fichier existe, on le ferme et on n'�crit rien
        fclose(file);
        //printf("Le fichier de licence existe d�j�.\n");
    } else {
        // Si le fichier n'existe pas, on le cr�e et on y �crit la licence
        file = fopen(licence_path, "w");
        if (file == NULL) {
            // Si l'ouverture du fichier �choue, on affiche un message d'erreur
            printf("Erreur lors de la cr�ation du fichier de licence.\n");
            fflush(stdout);
            return;
        }

        // Texte de la licence
        const char *licence_text =
            "Licence du logiciel fr-simplecode.exe sur Windows :\n"
            "---------------------------------------------------\n"
            "Ce logiciel est sous licence pour un usage sous contrainte. Vous n'�tes pas autoris� "
            "� modifier ou reproduire ce logiciel sans l'autorisation explicite de l'auteur argentropcher, assign� au compte Google argentropcher. "
            "L'utilisation commerciale est autoris�e � condition de respecter "
            "les termes de cette licence et de ne pas enfreindre les droits de l'auteur.\n\n"
            "Avertissement :\n"
            "Ce logiciel est fourni tel quel, sans aucune garantie. L'auteur ne pourra �tre tenu responsable "
            "de tout dommage r�sultant de l'utilisation de ce logiciel sur votre appareil.\n";

        // �crire la licence dans le fichier
        fprintf(file, "%s", licence_text);

        // Fermer le fichier
        fclose(file);
        //printf("Le fichier de licence a �t� cr�� avec succ�s.\n");
    }
}

//fonction pour remplacer time() dans la command
char *replace_actual_time(const char *str) {
    size_t new_size = strlen(str) + 1;  // Taille initiale de la cha�ne
    char *result = malloc(new_size);    // Allocation m�moire
    if (!result) return NULL;

    char *pos, *start = (char *)str;
    result[0] = '\0'; // Initialise une cha�ne vide

    while ((pos = strstr(start, "time(")) != NULL) {
        // Obtenir l'heure actuelle
        time_t rawtime;
        struct tm *timeinfo;
        char time_str[16] = ""; // Stockage pour hhmm.ss ou autres formats

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        size_t replace_length = 0; // Nombre de caract�res � supprimer (ex: "time()")

        // V�rifier le type de time()
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
        else if (strncmp(pos, "time()", 6) == 0) {
            snprintf(time_str, sizeof(time_str), "%02d%02d.%02d",
                     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            replace_length = 6;  // Longueur de "time()"
        }
        else {
            // Si "time(" mal form�, on avance d'un caract�re pour �viter une boucle infinie
            strncat(result, start, pos - start + 1);
            start = pos + 1;
            continue;
        }

        // Ajuster la m�moire pour le remplacement
        new_size += strlen(time_str) - replace_length;
        result = realloc(result, new_size);
        if (!result) return NULL;

        // Copier la partie avant "time(...)"
        strncat(result, start, pos - start);
        strcat(result, time_str); // Ajouter l'heure format�e

        // Avancer apr�s "time(...)" compl�tement
        start = pos + replace_length;
    }

    // Ajouter le reste de la cha�ne originale apr�s la derni�re occurrence
    strcat(result, start);

    return result; // Retourne la nouvelle cha�ne allou�e dynamiquement
}

double last_result; //variable pour rep

int var_script_exit; //variable pour une sortie exit dans un script
int var_script_pause; //variable pour une sortie de boucle ou condition dans un script
int var_affiche_virgule; //variable pour afficher la virgule des nombres dans les print()/affiche()
int var_affiche_slash_n; //variable pour afficher le retour automatique � la fin d'un print()/affiche() ou non (0=oui 1=non)
int var_boucle_infini; //variable qui est � 1 lorsque une boucle infini est en court, sinon 0

bool var_sortir_entree; //variable pour savoir si il faut renvoi� l'entr� de l'utilisateur dans la sortie

int mode_script_output; //variable pour le type de sortie en script (1= tout les blocks, 0= uniquement les erreurs + les sorties voulu)

// Fonction pour �valuer une simple expression avec priorit� des op�rateurs (d�claration de la fonction)
double eval_expr(const char *expr, int *index);

// Fonction calcul expression pour la fonction affiche de script et les conditions if
double condition_script_order(char *command);

// fonction remplacer les virgules en points
void replace_comma_with_dot(char *str) {
    while (*str) {
        if (*str == ',') {
            *str = '.';
        }
        str++;
    }
}

// Fonction pour obtenir le r�pertoire de l'ex�cutable
char* get_executable_path() {
    static char path[MAX_PATH];
    GetModuleFileName(NULL, path, sizeof(path));
    char *last_backslash = strrchr(path, '\\');
    if (last_backslash) {
        *last_backslash = '\0'; // Terminer le chemin au dernier backslash
    }
    return path;
}

// D�finition du fichier de r�sultat
char FILENAME[MAX_PATH];

void initialize_filename() {
    char *exec_path = get_executable_path();
    snprintf(FILENAME, sizeof(FILENAME), "%s\\result.txt", exec_path); // Met � jour le chemin complet
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
        // Enlever le saut de ligne � la fin de la ligne lue
        line[strcspn(line, "\n")] = '\0';

        // V�rifier si la ligne contient la valeur recherch�e
        if (strstr(line, value) != NULL) {
            // Si la valeur est trouv�e dans la ligne
            fclose(file);  // Fermer le fichier apr�s la recherche
            return 1.00;
        }
    }

    fclose(file);  // Fermer le fichier si la valeur n'est pas trouv�e
    return 0.00;  // Retourne 0.00 si la valeur n'a pas �t� trouv�e
}

void replace_in_var(char *command) {
    char result[1024] = "";  // La cha�ne pour stocker le r�sultat final
    char *cursor = command;
    char *start_in_var, *end_in_var;
    char temp[256];  // Pour stocker la valeur de remplacement

    // Parcours de toute la cha�ne
    while ((start_in_var = strstr(cursor, "in_var(")) != NULL) {
        // Ajouter le texte avant 'in_var('
        strncat(result, cursor, start_in_var - cursor);

        // Trouver la parenth�se fermante
        end_in_var = strchr(start_in_var, ')');
        if (end_in_var != NULL) {
            // Extraire l'argument entre les parenth�ses
            size_t length = end_in_var - start_in_var - 7;  // La longueur entre "in_var(" et ")"
            char value[length + 1];
            strncpy(value, start_in_var + 7, length);  // Copier l'argument
            value[length] = '\0';  // Terminer la cha�ne

            // Obtenir la valeur retourn�e par handle_in_var
            double result_in_var = handle_in_var(value);

            // Convertir la valeur en cha�ne de caract�res
            snprintf(temp, sizeof(temp), "%.2f", result_in_var);
            replace_comma_with_dot(temp); //remplace la ',' du nombre par un '.'

            // Ajouter la valeur au r�sultat
            strncat(result, temp, sizeof(result) - strlen(result) - 1);

            // D�placer le curseur apr�s la parenth�se fermante
            cursor = end_in_var + 1;
        } else {
            // Si parenth�se fermante non trouv�e, ajouter le reste du texte
            strncat(result, cursor, strlen(cursor));
            break;
        }
    }

    // Ajouter le reste du texte apr�s le dernier in_var
    if (*cursor != '\0') {
        strncat(result, cursor, sizeof(result) - strlen(result) - 1);
    }

    // Copier le r�sultat final dans la variable command
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
        // Supprimer compl�tement le fichier var.txt
        if (remove(VAR_FILE) == 0) {
            printf("Toutes les variables ont �t� supprim�es.\n");
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
        printf("Erreur : Impossible de cr�er un fichier temporaire.\n");
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
                found = 1; // Marquer que la variable a �t� trouv�e et ignor�e (donc supprim�e)
                continue;
            }
        }
        fputs(line, tempFile); // Copier la ligne dans le fichier temporaire si ce n'est pas la variable � supprimer
    }

    fclose(file);
    fclose(tempFile);

    // Remplacer var.txt par le fichier temporaire si une suppression a eu lieu
    if (found) {
        remove(VAR_FILE);
        rename("var_temp.txt", VAR_FILE);
        remove("var_temp.txt");
        printf("La variable \"%s\" a �t� supprim�e.\n", arg);
        fflush(stdout);
    } else {
        remove("var_temp.txt"); // Si aucune suppression n'a eu lieu, supprimer le fichier temporaire
        printf("Variable \"%s\" non trouv�e, aucune suppression effectu�e.\n", arg);
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
            // Si la variable existe d�j�, on remplace sa valeur
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

    // �crire dans le fichier
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
    return 0.00;  // Si la variable n'est pas trouv�e
}

char* handle_variable(char *expr) {
    static char new_expr[MAX_LEN];
    char temp_expr[MAX_LEN];
    char *start = expr, *pos;
    char name[50];
    char valueforsave[1024];
    double value;

    new_expr[0] = '\0';  // R�initialiser l'expression

    // V�rifier si c'est une affectation `var|nom|=valeur`
    if (sscanf(expr, "var|%49[^|]|=%49[^\n]", name, &valueforsave) == 2) {
        int index = 0;  // Position de d�part pour l'analyse
        if (strstr(valueforsave, "var|") != NULL) {
            replace_comma_with_dot(valueforsave);
            strcpy(valueforsave, handle_variable(valueforsave));
            replace_comma_with_dot(valueforsave);
        }
        double result = eval_expr(valueforsave, &index);  // �valuer l'expression
        char result_str[50];
        snprintf(result_str, sizeof(result_str), "%.2f", result);  // Convertir en cha�ne
        save_variable(name, result_str);  // Sauvegarder sous forme de texte
        if (mode_script_output==1){
            printf("Variable mise � jour : %s = %s\n", name, result_str);
            fflush(stdout);
        }
        return "None";  // Indique qu'il ne faut pas ex�cuter l'expression
    }

    // Copier l'expression originale
    strcpy(temp_expr, expr);

    while ((pos = strstr(temp_expr, "var|")) != NULL) {
        // Copier le texte avant la variable
        strncat(new_expr, temp_expr, pos - temp_expr);

        // Trouver la fin du nom de variable
        char *end = strchr(pos + 4, '|');
        if (!end) break;  // Format incorrect, on arr�te

        // Extraire le nom de la variable
        char var_name[50];
        strncpy(var_name, pos + 4, end - (pos + 4));
        var_name[end - (pos + 4)] = '\0';

        // R�cup�rer la valeur de la variable
        double value = get_variable(var_name);

        // Ajouter la valeur dans new_expr
        char value_str[50];
        snprintf(value_str, sizeof(value_str), "%.2f", value);
        strcat(new_expr, value_str);

        // Passer � la suite
        start = end + 1;
        strcpy(temp_expr, start);
    }

    // Ajouter le reste de l'expression
    strcat(new_expr, temp_expr);

    return new_expr;
}
//fin traitement zone de variable




// Fonction pour g�rer les occurrences de random[nombre1, nombre2]
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
            range[length] = '\0';  // Fin de cha�ne

            // V�rifier si la virgule est pr�sente
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

            // V�rifier si num1 est "rep", "var||" ou un nombre
            if (strcmp(num1_str, "rep") == 0) {
                num1 = (int)last_result;
            } else if (strncmp(num1_str, "var|", 4) == 0) {  // V�rifier si num1_str commence par "var||"
                // Remplacer "var||" par sa valeur via la fonction handle_variable
                strcpy(num1_str, handle_variable(num1_str));  // Appel de la fonction handle_variable
                num1 = atoi(num1_str);  // Convertir la cha�ne modifi�e en nombre entier
            } else if (isdigit(num1_str[0]) || num1_str[0] == '-') {
                num1 = atoi(num1_str);
            } else {
                fprintf(stderr, "Erreur : Valeur incorrecte dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // V�rifier si num2 est "rep", "var||" ou un nombre
            if (strcmp(num2_str, "rep") == 0) {
                num2 = (int)last_result;
            } else if (strncmp(num2_str, "var|", 4) == 0) {  // V�rifier si num2_str commence par "var||"
                // Remplacer "var||" par sa valeur via la fonction handle_variable
                strcpy(num2_str, handle_variable(num2_str));  // Appel de la fonction handle_variable
                num2 = atoi(num2_str);  // Convertir la cha�ne modifi�e en nombre entier
            } else if (isdigit(num2_str[0]) || num2_str[0] == '-') {
                num2 = atoi(num2_str);
            } else {
                fprintf(stderr, "Erreur : Valeur incorrecte dans random[].\n");
                fflush(stdout);
                strncpy(start, "0.00", 4);
                return expr;
            }

            // V�rifier l'ordre des bornes
            if (num1 > num2) {
                int temp = num1;
                num1 = num2;
                num2 = temp;
            }

            // G�n�rer un nombre al�atoire entre num1 et num2
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

    // V�rifier s'il y a un signe "-" ou "+"
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

    // Lire la partie d�cimale si pr�sente
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

// Fonction pour �valuer les termes s�par�s par * ou /
double parse_term(const char *expr, int *index) {
    double value = 0;
    int state = 0;

    // Lire un nombre ou une parenth�se
    if (expr[*index] == '(') {
        (*index)++;  // Passer la parenth�se ouvrante
        value = eval_expr(expr, index);  // �valuer l'expression � l'int�rieur des parenth�ses
        if (expr[*index] == ')') {
            (*index)++;  // Passer la parenth�se fermante
        }
    } else {
        value = parse_number(expr, index);  // Si ce n'est pas une parenth�se, on lit un nombre
    }

    // G�rer les multiplicateurs et diviseurs en suivant les priorit�s
    while (expr[*index] == '*' || expr[*index] == '/') {
        char op = expr[*index];
        (*index)++;

        // Si l'op�rateur suivant est *, diviser ou multiplier le terme pr�c�dent
        double next_value = 0;
        if (expr[*index] == '(') {
            (*index)++;  // Passer la parenth�se ouvrante
            next_value = eval_expr(expr, index);  // R�cursivement �valuer l'expression dans les parenth�ses
            if (expr[*index] == ')') {
                (*index)++;  // Passer la parenth�se fermante
            }
        } else {
            next_value = parse_number(expr, index);  // Lire un nombre normal
        }

        if (op == '*') {
            value *= next_value;
        } else if (op == '/') {
            if (next_value == 0) {
                printf("Erreur : division par z�ro\n");
                fflush(stdout);
                state = 1;
                // Sortir en cas de division par z�ro
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

// Fonction principale pour �valuer une expression
double eval_expr(const char *expr, int *index) {
    double value = parse_term(expr, index); // �valuer le premier terme (g�re la multiplication et la division)

    while (expr[*index] != '\0') {
        if (expr[*index] == '+' || expr[*index] == '-') {
            char op = expr[*index];
            (*index)++; // Passer � l'op�rateur suivant

            double next_value = parse_term(expr, index); // R�cup�rer la valeur suivante

            // Appliquer l'op�rateur � la valeur
            if (op == '+') {
                value += next_value;
            } else if (op == '-') {
                value -= next_value;
            }
        }
        // G�rer la multiplication et la division
        else if (expr[*index] == '*' || expr[*index] == '/') {
            char op = expr[*index];
            (*index)++; // Passer � l'op�rateur suivant

            double next_value = parse_term(expr, index); // R�cup�rer la valeur suivante

            // Appliquer l'op�rateur � la valeur
            if (op == '*') {
                value *= next_value;
            } else if (op == '/') {
                if (next_value == 0) {
                    printf("Erreur : division par z�ro\n");
                    return 0;  // Retourner une valeur par d�faut en cas d'erreur
                }
                value /= next_value;
            }
        }
        // V�rifier si une parenth�se ouvrante est pr�sente
        else if (expr[*index] == '(') {
            (*index)++; // Passer la parenth�se ouvrante

            char prev_op = (expr[*index - 2] == '+' || expr[*index - 2] == '-') ? expr[*index - 2] : '+';
            double parenthesis_value = eval_expr(expr, index); // Appel r�cursif pour traiter l'expression � l'int�rieur des parenth�ses

            // Appliquer l'op�rateur avant la parenth�se � la valeur de la parenth�se
            if (prev_op == '+') {
                value += parenthesis_value;
            } else if (prev_op == '-') {
                value -= parenthesis_value;
            }
        }
        // V�rifier si une parenth�se fermante est pr�sente
        else if (expr[*index] == ')') {
            (*index)++; // Passer la parenth�se fermante
            break; // Sortir de la boucle une fois qu'on a trait� la parenth�se
        }
        else {
            break; // Sortir de la boucle si un caract�re invalide est trouv�
        }
    }

    return value;
}

// Fonction pour ajouter une multiplication implicite dans l'expression
void handle_implicit_multiplication(char *expr) {
    int len = strlen(expr);

    // Parcours de l'expression pour v�rifier si un nombre est suivi imm�diatement d'une parenth�se ouvrante
    for (int i = 0; i < len - 1; i++) {
        if (isdigit(expr[i]) && expr[i + 1] == '(') {
            // Ins�rer un '*' entre le nombre et la parenth�se
            memmove(&expr[i + 2], &expr[i + 1], len - i);  // D�caler les caract�res
            expr[i + 1] = '*';  // Ajouter '*' entre le nombre et la parenth�se
            len++;  // Mettre � jour la longueur de l'expression apr�s insertion
            i++;  // Passer � l'indice suivant pour �viter de traiter plusieurs fois la m�me paire
        }
    }
}

// Fonction principale pour �valuer une expression � partir d'une cha�ne de caract�res
double eval(char *expr) {
    handle_implicit_multiplication(expr);  // Appeler la fonction pour ajouter les multiplications implicites
    int index = 0;
    return eval_expr(expr, &index);  // Appeler eval_expr pour �valuer l'expression modifi�e
}



// Fonction pour remplacer "rep" par la valeur de last_result dans la commande
char* replace_rep(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN];
    char *pos;

    // Convertir last_result en cha�ne de caract�res
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

        // Ajouter la cha�ne qui repr�sente le dernier r�sultat
        strcat(buffer, result_str);

        // Ajouter le reste de la commande apr�s "rep"
        strcat(buffer, pos + 3);

        // Copier la commande mise � jour dans "command"
        strcpy(command, buffer);
    }

    return command; // Retourner la commande modifi�e
}

// Fonction pour sauvegarder le r�sultat dans un fichier
void save_result(double result) {
    FILE *file = fopen(FILENAME, "w");
    if (file == NULL) {
        perror("Erreur : impossible d'ouvrir le fichier pour �crire");
        fflush(stdout);
        return;
    }
    fprintf(file, "%.2lf\n", result);
    fclose(file);
    printf("R�sultat %.2lf",result);
    fflush(stdout);
    printf(" sauvegard� dans %s\n", FILENAME);
    fflush(stdout);
}

// Fonction pour charger le r�sultat depuis un fichier
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
    printf("R�sultat charg� depuis %s : %.s", FILENAME);
    fflush(stdout);
    printf("rep=%.2lf\n", result);
    fflush(stdout);
    return result;
}





double def_airetriangle(char* command, double last_result) {
    double base = 0, hauteur = 0;
    char* base_str;
    char* hauteur_str;

    // V�rifier si la commande commence par "triangle("
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

        // Trouver la partie apr�s le ";"
        hauteur_str = strtok(NULL, ")");

        // V�rification que les deux param�tres (base et hauteur) existent
        if (base_str == NULL || hauteur_str == NULL) {
            printf("Erreur : format incorrect, il faut 'triangle(base;hauteur)'\n");
            return -0.01;
        }

        // Si "rep" est trouv�, remplacer par last_result
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

    // V�rifier si la commande commence par "cercle("
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

        // V�rification que le rayon est bien fourni
        if (rayon_str == NULL) {
            printf("Erreur : format incorrect, il faut 'cercle(rayon)'\n");
            return -0.01;
        }

        // Si "rep" est trouv�, remplacer par last_result
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

// Fonction pour g�rer et �valuer toutes les occurrences de sqrt(...) dans la commande
char* handle_sqrt(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN]; // Pour stocker la commande modifi�e
    char *start, *end; // Pointeurs pour trouver les parenth�ses
    char inner_expr[MAX_COMMAND_LEN]; // Pour stocker l'expression interne

    // Initialiser le buffer avec la commande d'origine
    strcpy(buffer, command);

    // Chercher les occurrences de sqrt(...)
    while ((start = strstr(buffer, "sqrt(")) != NULL) {
        end = strchr(start, ')'); // Trouver la parenth�se fermante

        // V�rifier que nous avons trouv� une parenth�se fermante
        if (end == NULL) {
            break; // Pas de parenth�se fermante, sortir de la boucle
        }

        // Extraire l'expression � l'int�rieur de sqrt(...)
        int length = end - (start + 5); // longueur de l'expression entre sqrt( et )
        strncpy(inner_expr, start + 5, length); // Copier l'expression interne
        inner_expr[length] = '\0'; // Terminer la cha�ne

        // Remplacer "rep" par last_result dans l'expression interne
        replace_rep(inner_expr, last_result);

        // �valuer l'expression interne
        double sqrt_value = eval(inner_expr);

        // V�rifier si le r�sultat est n�gatif
        if (sqrt_value < 0) {
            printf("Erreur : racine carr�e d'un nombre n�gatif.\n");
            return command; // Retourner la commande originale si une erreur se produit
        }

        // Calculer la racine carr�e
        double result = sqrt(sqrt_value);

        // Remplacer la premi�re occurrence de sqrt(...) dans le buffer par son r�sultat
        char result_str[MAX_COMMAND_LEN];
        snprintf(result_str, sizeof(result_str), "%.2lf", result);

        // Construire la nouvelle cha�ne sans sqrt
        char new_buffer[MAX_COMMAND_LEN];
        // Copier la partie avant sqrt(
        int prefix_length = start - buffer;
        strncpy(new_buffer, buffer, prefix_length);
        new_buffer[prefix_length] = '\0'; // Terminer la nouvelle cha�ne

        // Ajouter le r�sultat
        strcat(new_buffer, result_str);

        // Ajouter le reste de la cha�ne apr�s )
        strcat(new_buffer, end + 1); // end + 1 pour passer la parenth�se fermante

        // Copier la nouvelle cha�ne dans buffer
        strcpy(buffer, new_buffer);
    }

    return buffer; // Retourner la commande modifi�e
}


// Fonction pour g�rer et �valuer toutes les occurrences de sqrt(...) dans la commande
char* handle_cbrt(char *command, double last_result) {
    static char buffer[MAX_COMMAND_LEN]; // Pour stocker la commande modifi�e
    char *start, *end; // Pointeurs pour trouver les parenth�ses
    char inner_expr[MAX_COMMAND_LEN]; // Pour stocker l'expression interne

    // Initialiser le buffer avec la commande d'origine
    strcpy(buffer, command);

    // Chercher les occurrences de sqrt(...)
    while ((start = strstr(buffer, "cbrt(")) != NULL) {
        end = strchr(start, ')'); // Trouver la parenth�se fermante

        // V�rifier que nous avons trouv� une parenth�se fermante
        if (end == NULL) {
            break; // Pas de parenth�se fermante, sortir de la boucle
        }

        // Extraire l'expression � l'int�rieur de sqrt(...)
        int length = end - (start + 5); // longueur de l'expression entre sqrt( et )
        strncpy(inner_expr, start + 5, length); // Copier l'expression interne
        inner_expr[length] = '\0'; // Terminer la cha�ne

        // Remplacer "rep" par last_result dans l'expression interne
        replace_rep(inner_expr, last_result);

        // �valuer l'expression interne
        double sqrt_value = eval(inner_expr);

        // Calculer la racine cubique
        double result = cbrt(sqrt_value);

        // Remplacer la premi�re occurrence de sqrt(...) dans le buffer par son r�sultat
        char result_str[MAX_COMMAND_LEN];
        snprintf(result_str, sizeof(result_str), "%.2lf", result);

        // Construire la nouvelle cha�ne sans sqrt
        char new_buffer[MAX_COMMAND_LEN];
        // Copier la partie avant sqrt(
        int prefix_length = start - buffer;
        strncpy(new_buffer, buffer, prefix_length);
        new_buffer[prefix_length] = '\0'; // Terminer la nouvelle cha�ne

        // Ajouter le r�sultat
        strcat(new_buffer, result_str);

        // Ajouter le reste de la cha�ne apr�s )
        strcat(new_buffer, end + 1); // end + 1 pour passer la parenth�se fermante

        // Copier la nouvelle cha�ne dans buffer
        strcpy(buffer, new_buffer);
    }

    return buffer; // Retourner la commande modifi�e
}

double local_value = 0.00; // Stocker la valeur de local


// Fonction pour d�finir la valeur de local(argument)
void set_local(const char* expr, double last_result) {
    char temp_expr[MAX_COMMAND_LEN];
    strcpy(temp_expr, expr); // Copier l'expression de l'argument
    // Remplacer "rep" dans l'expression par la valeur de last_result
    char* processed_expr = replace_rep(temp_expr, last_result);
    // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
    if (strstr(processed_expr, "sqrt(") != NULL) {
        //printf("%s\n",processed_expr);
        char* processed_command2="";
        // G�rer les expressions sqrt
        char* processed_command = handle_sqrt(processed_expr, last_result);
        // Afficher la commande apr�s traitement de sqrt pour diagnostic
        ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
        // Remplacer les virgules par des points dans la commande trait�e
        for (int i = 0; processed_command[i] != '\0'; i++) {
            if (processed_command[i] == ',') {
                processed_command[i] = '.'; // Remplacer la virgule par un point
            }
        }
        // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
        if (strstr(processed_expr, "cbrt(") != NULL) {
            // G�rer les expressions sqrt
            processed_command2 = handle_cbrt(processed_command, last_result);
            // Afficher la commande apr�s traitement de sqrt pour diagnostic
            ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande trait�e
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
        // �valuer l'expression et renvoyer le r�sultat
        double result = eval(processed_command3);
        printf("Valeur de local d�finie � : %.2lf\n", result);
        fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
        local_value = result;
        return;  // Continuer avec la prochaine it�ration de la boucle
    }

    // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
    if (strstr(processed_expr, "cbrt(") != NULL) {
        char* processed_command2="";
        // G�rer les expressions sqrt
        char* processed_command = handle_cbrt(processed_expr, last_result);
        // Afficher la commande apr�s traitement de sqrt pour diagnostic
        ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
        // Remplacer les virgules par des points dans la commande trait�e
        for (int i = 0; processed_command[i] != '\0'; i++) {
            if (processed_command[i] == ',') {
                processed_command[i] = '.'; // Remplacer la virgule par un point
            }
        }
        if (strstr(processed_expr, "sqrt(") != NULL) {
            // G�rer les expressions sqrt
            processed_command2 = handle_sqrt(processed_command, last_result);
            // Afficher la commande apr�s traitement de sqrt pour diagnostic
            ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande trait�e
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
        // �valuer l'expression et renvoyer le r�sultat
        double result = eval(processed_command3);
        printf("Valeur de local d�finie � : %.2lf\n", result);
        fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
        local_value = result;
        return;  // Continuer avec la prochaine it�ration de la boucle
    }



    // �valuer l'expression
    local_value = eval(processed_expr);

    printf("Valeur de local d�finie � : %.2lf\n", local_value);
    fflush(stdout);
}

// Fonction pour r�cup�rer la valeur de local() et la stocker dans last_result
double get_local() {
    printf("Valeur de local r�cup�r�e : %.2lf\n", local_value);
    fflush(stdout);
    return local_value;
}

// fonction pour lancer un .exe windows uniquement (cmd)
// Fonction pour ex�cuter le fichier sp�cifi�
void execute(const char *file_path) {
    // V�rifier si le chemin est valide
    if (file_path == NULL || strlen(file_path) == 0) {
        printf("Erreur : chemin invalide.\n");
        return;
    }

    // Construire la commande � ex�cuter
    char command[2048];  // Assurez-vous que la taille est suffisante
    snprintf(command, sizeof(command), "cmd /c \"%s\"", file_path);

    // Utiliser system() pour ex�cuter le fichier
    int result = system(command);

    // V�rifier si l'ex�cution a r�ussi
    if (result == 0) {
        printf("Ex�cution r�ussie.\n");
        fflush(stdout);
    } else {
        printf("Erreur lors de l'ex�cution de la commande.\n");
        fflush(stdout);
    }
}

// Fonction pour extraire l'argument de la commande var="execute(chemin)"
void parse_and_execute(const char *var) {
    // Chercher "execute(" et ")" dans la cha�ne
    const char *debut = strstr(var, "execute(");
    const char *fin = strchr(var, ')');

    if (debut != NULL && fin != NULL && fin > debut) {
        // Extraire le chemin entre les parenth�ses
        debut += strlen("execute(");  // D�placer le pointeur apr�s "execute("

        int length = fin - debut;  // Calculer la longueur du chemin
        char file_path[length + 1];  // Allouer la m�moire pour le chemin

        strncpy(file_path, debut, length);  // Copier le chemin dans la variable
        file_path[length] = '\0';  // Ajouter le caract�re de fin de cha�ne

        // Appeler la fonction execute avec le chemin extrait
        execute(file_path);
    } else {
        printf("Erreur : format de commande invalide.\n");
        fflush(stdout);
    }
}
//fin fonction lancer un exe window uniquement (cmd)

//partie script

//fonction pour pouvoir sauter des lignes gr�ce � [/n] dans un affiche
void replace_newline_tags_in_fonction_script_affiche(char *buffer) {
    char temp[1024]; // Stockage temporaire
    int i = 0, j = 0;

    while (buffer[i] != '\0') {
        // V�rifie si on a trouv� `[/n]`
        if (strncmp(&buffer[i], "[/n]", 4) == 0) {
            temp[j++] = '\n'; // Ajoute un saut de ligne
            i += 4; // Saute `[/n]`
        } else {
            temp[j++] = buffer[i++]; // Copie le caract�re normal
        }
    }

    temp[j] = '\0'; // Terminer la nouvelle cha�ne
    strcpy(buffer, temp); // Remplace buffer par la nouvelle version
}

//Fonction affiche pour script
void fonction_script_affiche(char *text){
    char buffer[1024];  // Stocke temporairement les parties de texte
    int index = 0;
    int in_quotes = 0; // Indique si on est � l'int�rieur de guillemets

    //printf("Affichage : ");

    while (*text) {
        if (*text == '"') {
            if (in_quotes) {
                buffer[index] = '\0'; // Terminer la cha�ne
                replace_newline_tags_in_fonction_script_affiche(buffer); //fonction pour pouvoir sauter des lignes gr�ce � [/n]
                printf("%s", buffer); // Afficher le texte entre ""
                fflush(stdout);
            }
            in_quotes = !in_quotes; // Basculer l'�tat (entr�e ou sortie de guillemets)
            index = 0; // R�initialiser le buffer
        }
        else if (in_quotes) {
            buffer[index++] = *text; // Stocker le texte entre guillemets
        }
        else { // On est en dehors des guillemets
            if (isspace(*text)) {
                text++; // Ignorer les espaces
                continue;
            }
            // Lire une expression (jusqu'� un " ou un espace)
            while (*text && *text != '"' && !isspace(*text)) {
                buffer[index++] = *text++;
            }
            buffer[index] = '\0'; // Terminer la cha�ne

            // �valuer et afficher le r�sultat de l'expression
            if (index > 0) {

                if (var_affiche_virgule==0){
                    printf("%.2f", condition_script_order(buffer));
                    fflush(stdout);
                } else {
                    int var_not_comma = (int)condition_script_order(buffer);
                    printf("%d",var_not_comma);
                    fflush(stdout);
                }

            }
            index = 0; // R�initialiser le buffer
            continue; // Reprendre la boucle sans incr�menter text ici
        }
        text++;
    }
    if (var_affiche_slash_n==0){
        printf("\n");
        fflush(stdout);
    }
}

void fonction_script_command_for_affiche(char *command) {
    char *start = NULL;

    if (strncmp(command, "affiche(", 8) == 0) {
        start = command + 8;  // Pointer juste apr�s "affiche("
    } else if (strncmp(command, "print(", 6) == 0) {
        start = command + 6;  // Pointer juste apr�s "print("
    }

    if (start != NULL) {
        // Trouver la position de la ")"
        char *end = strrchr(start, ')');
        if (end != NULL) {
            *end = '\0';  // Remplacer la ")" par un caract�re nul
            fonction_script_affiche(start);  // Passer le contenu extrait � la fonction
        } else {
            printf("Erreur : Parenth�se de fermeture manquante sur affiche ou print.\n");
            fflush(stdout);
        }
    }
}

// Fonction qui ex�cute une commande
void execute_script_order(char *command) {
    if (strncmp(command, "###sortie:classique###",22)==0 || strncmp(command,"###sortie:normale###",20)==0 || strncmp(command,"###output:normal###",19)==0){
        mode_script_output=0;
        return;
    }
    if (mode_script_output==1){
        printf("%s\n", command); // Simulation d'une ex�cution
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

    // Supprimer les espaces blancs et retours � la ligne de l'expression
    command[strcspn(command, "\n")] = 0;
    replace_in_var(command); //fonction pour remplacer les in_var()
    //remplacer les time()
    strcpy(command,replace_actual_time(command));
    //remplacer les random[,]
    handle_random(command,last_result);

    if (strstr(command, "var|") != NULL) {
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(command, replace_rep(command, last_result));

            //supprim� les var|| apr�s le '=' si il y en a un en les traitant
            char* command_after_equal2 = strchr(command, '=');
            if (command_after_equal2 != NULL) {
                // Sauvegarder la partie avant `=`
                char prefix2[256] = {0};
                strncpy(prefix2, command, command_after_equal2 - command);
                prefix2[command_after_equal2 - command] = '\0';  // Terminer proprement

                // Passer apr�s '='
                command_after_equal2++;

                if (strstr(command_after_equal2, "var|") != NULL) {
                        strcpy(command_after_equal2, handle_variable(command_after_equal2));
                        replace_comma_with_dot(command_after_equal2);
                }
                // Construire la nouvelle commande avec le r�sultat
                snprintf(command, sizeof(command)+1, "%s=%s", prefix2, command_after_equal2);
            }

            // G�rer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
            strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son r�sultat
            strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son r�sultat
            // V�rification des commandes pour les aires
            if (strstr(command, "triangle(") != NULL || strstr(command, "cercle(") != NULL) {
                // Trouver le signe '='
                char* command_after_equal = strchr(command, '=');

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer proprement

                    // Passer apr�s '='
                    command_after_equal++;

                    double aire = 0.00;

                    //regarder si il y a un sous appel d'une variable dans triangle ou cercle ex: 'var|x|=cercle(var|y|)', remplace 'var|y|'
                    if (strstr(command_after_equal, "var|") != NULL) {
                        strcpy(command_after_equal, handle_variable(command_after_equal));
                        replace_comma_with_dot(command_after_equal);
                    }

                    // V�rifier si c'est un triangle ou un cercle
                    if (strstr(command_after_equal, "triangle(") != NULL) {
                        aire = def_airetriangle(command_after_equal, last_result);
                    } else if (strstr(command_after_equal, "cercle(") != NULL) {
                        aire = def_airecercle(command_after_equal, last_result);
                    }

                    // V�rification de la valeur retourn�e
                    if (aire == -0.01) {
                        aire = 0.00;
                    }

                    // Construire la nouvelle commande avec le r�sultat
                    snprintf(command, 256, "%s=%.2f", prefix, aire);
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouv�, assigner "0.00"
                }
            }
            handle_implicit_multiplication(command);
            //printf("v�rif %s\n",command);
            replace_comma_with_dot(command);
            strcpy(command, handle_variable(command));  // Remplace les variables

            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la cha�ne caract�re par caract�re
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
    //remplacer les random [,]
    handle_random(command,last_result);

    replace_comma_with_dot(command);

    if (strstr(command, "var|") != NULL) {
            strcpy(command, handle_variable(command));  // Remplace les variables
            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la cha�ne caract�re par caract�re
                if (*ptr == ',') {
                    *ptr = '.';
                }
                ptr++;
            }
            //printf("%s\n", command);  // Affichage correct
            if (strstr(command, "None") != NULL) {
                //printf("ici");
                printf("Erreur : d�finir une variable dans une condition est non conforme et renvoie 0 malgr� l'affectation de la variable\n");
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

    // G�rer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
    strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son r�sultat
    strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son r�sultat
    replace_comma_with_dot(command);

    //printf("%s\n",command);
    double resultat = eval(command);
    return resultat;
}

// Fonction d'�valuation d'une condition
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
    if (strncmp(expr, "repeat||",8)== 0 || strncmp(expr, "r�p�te||",8)== 0 || strncmp(expr, "boucle||",8)==0){
        int repetitions = 0;
        int a_0_repetitions = 0;
        char *start_num, *end_num;
        char *text_boucle = malloc(MAX_COMMAND_LEN);
        char *text_after_second_pipe = malloc(MAX_COMMAND_LEN);
        // Trouver la premi�re occurence de "||" apr�s "repeat||" ou similaire
        start_num = strstr(expr, "||") + 2;  // Avancer apr�s "||"

        // Trouver la deuxi�me occurence de "||"
        end_num = strstr(start_num, "||");

        if (start_num && end_num) {
            // Extraire le texte entre les deux "||" (nombre de r�p�titions)
            size_t length = end_num - start_num;
            char num_str[length + 1];
            strncpy(num_str, start_num, length);
            num_str[length] = '\0';  // Terminer la cha�ne

            if (strncmp(num_str,"infini",6)==0 || strncmp(num_str,"endless",7)==0){
                char *first_pipe = strstr(expr, "||");
                if (first_pipe) {
                    // Trouver la deuxi�me occurrence de "||"
                    char *second_pipe = strstr(first_pipe + 2, "||");
                    if (second_pipe) {
                        // Extraire le texte apr�s la deuxi�me "||"
                        text_after_second_pipe = second_pipe + 2;  // Commence juste apr�s "||"
                        char *end_of_text = strchr(text_after_second_pipe, '\0');  // Fin de la cha�ne
                        if (end_of_text) {
                            // Calculer la longueur du texte � extraire
                            size_t length = end_of_text - text_after_second_pipe;
                            text_boucle = malloc(length + 6 + 1);  // Allouer m�moire pour "[[[" + texte + "]]]" + '\0'

                            // Ajouter "[[[" au d�but
                            strcpy(text_boucle, "[[[");

                            // Copier le texte apr�s la deuxi�me "||"
                            strncat(text_boucle, text_after_second_pipe, length);

                            // Ajouter "]]]" � la fin
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
                    // Trouver la deuxi�me occurrence de "||"
                    char *second_pipe = strstr(first_pipe + 2, "||");
                    if (second_pipe) {
                        // Extraire le texte apr�s la deuxi�me "||"
                        text_after_second_pipe = second_pipe + 2;  // Commence juste apr�s "||"
                        char *end_of_text = strchr(text_after_second_pipe, '\0');  // Fin de la cha�ne
                        if (end_of_text) {
                            // Calculer la longueur du texte � extraire
                            size_t length = end_of_text - text_after_second_pipe;
                            text_boucle = malloc(length + 6 + 1);  // Allouer m�moire pour "[[[" + texte + "]]]" + '\0'

                            // Ajouter "[[[" au d�but
                            strcpy(text_boucle, "[[[");

                            // Copier le texte apr�s la deuxi�me "||"
                            strncat(text_boucle, text_after_second_pipe, length);

                            // Ajouter "]]]" � la fin
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

    // Extraire les valeurs et l'op�rateur

    char *p = token + 2; // Apr�s le premier "||"
    char *egal = strchr(p, '{'); // '{' � remplacer le '=' car les '{' sont forcement pr�cente si un autre block commence (ou un block dans un block)

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
    if (token && (!egal || token < egal)) { //condition pour ignorer les [[ d'une autre condition (� l'int�rieur de la premi�re) si il n'y en a pas sur la premi�re en utilosant le rep�re avant ou apr�s le '='
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
        // Remettre expr � sa position initiale
        expr = initial_expr;
    }
    //printf("%d\n",n_valid);

    // V�rifier la condition
    int condition_reussie = evaluate_condition(valeur1, operateur, valeur2);

    char *start = strchr(expr, '{');
    if (!start) {
        printf("Erreur : Aucune instruction � ex�cuter dans une condition.\n");
        fflush(stdout);
        return;
    }

    char result[1024] = "[[[";
    int open_braces = 0, valid_count = 0, else_count = 0;
    int block_started = 0;
    int skipped_blocks = 0;
    int take_all_remaining = 0;  //  Ajout� : Permet de prendre tout apr�s `N` blocs ignor�s
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

    //v�rifier si il y a eut un exit dans un block pr�c�dent
    if (var_script_exit==1){
        return;
    }


    char *start = strstr(expr, "{");
    char *end_script = strstr(expr, "]]]");

    if (!start || !end_script) {
        printf("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
        fflush(stdout);
        return;
    }

    while (start && start < end_script) {
        if (*start != '{') {
            start++; // S�curit�, on avance jusqu'� la prochaine accolade
            continue;
        }

        char *cursor = start + 1; // D�but du contenu apr�s '{'
        int open_braces = 1;  // Compteur d'accolades ouvertes
        char order[MAX_COMMAND_LEN] = {0}; // Buffer pour stocker l'ordre
        int index = 0;

        while (*cursor && cursor < end_script) {
            if (*cursor == '{') {
                open_braces++;
            } else if (*cursor == '}') {
                open_braces--;
                if (open_braces == 0) {
                    break; // Fin de l'ordre trouv�
                }
            }

            order[index++] = *cursor;
            cursor++;
        }

        order[index] = '\0'; // Terminaison de cha�ne correcte

        //v�rifier si il y a exit dans un block � ex�cuter
        if (strncmp(order, "exit", 4) == 0){
            var_script_exit = 1;
            return;
        }

        if (strncmp(order, "stop_pause()",12) == 0 || strncmp(order,"stop_break()",12) == 0) {
            var_script_pause=0;
        }

        if (strncmp(order,"pause(",6) == 0 || strncmp(order,"break(",5) == 0){
            char *start_pause = strchr(order, '(');  // Trouver la premi�re parenth�se ouvrante
            char *end_pause = strchr(order, ')');    // Trouver la parenth�se fermante

            if (start_pause != NULL && end_pause != NULL && end_pause > start_pause) {
                // Extraire la valeur entre les parenth�ses
                size_t length_pause = end_pause - start_pause - 1;  // Longueur entre '(' et ')'
                char num_str_pause[length_pause + 1];         // Stockage de la valeur sous forme de cha�ne
                strncpy(num_str_pause, start_pause + 1, length_pause);
                num_str_pause[length_pause] = '\0';  // Terminer la cha�ne

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

        //v�rifier si on � fait une demande de sortir d'un nombre de boucle et condition
        if (var_script_pause>0){
            var_script_pause=var_script_pause-1;
            return;
        }

        // Ex�cuter l'ordre
        execute_script(order);

        // Chercher la prochaine accolade
        start = strstr(cursor + 1, "{");
    }
}
//fin partie script


int autorise=0;

int main() {

    licence(); //cr�er la licence de fr-simplecode

    char command[MAX_COMMAND_LEN];
    if (isatty(STDIN_FILENO)) {
        // Ex�cut� directement dans le terminal
        //system("chcp 850 > nul");  // Active latin-1
        //setlocale(LC_ALL, "fr_FR.ISO8859-1");  // Utilise Latin-1
        system("chcp 1252 > nul");
        setlocale(LC_ALL, "French_France.1252");
    } else {
        // Ex�cut� par un autre programme
        setlocale(LC_ALL, "fr_FR.UTF-8");
    }
    //system("chcp 65001 > nul");
    //setlocale(LC_ALL, "fr_FR.UTF-8");
    printf("fr-simplecode on windows : (tapez 'info' pour les fonctions ou 'exit' pour quitter) \n");
    fflush(stdout);
    // Initialiser le chemin du fichier
    initialize_filename();

    last_result = 0.00;  // Stocke le dernier r�sultat
    var_script_exit = 0; //configure qu'il n'y a pas eut d'appel exit dans script (1 si il y a un appel)
    var_script_pause = 0; //configure le nombre de sortie de boucle (0 pas de sortie, autre, nombre de sortie de boucle ou condition)
    mode_script_output = 0; //sortie classique pour script
    var_affiche_virgule = 0; //sortie pour les print()/affiche() avec les nombres � virgule ou sans
    var_affiche_slash_n = 0; //sortie pour les print()/affiche() avec le retour � la ligne autoatique ou non
    var_boucle_infini = 0; //gestion des mode en boucle infini ou non (1 ou 0)
    var_sortir_entree=false; //gestion de l'affichage de l'entr�e de l'utilisateur ou non

    srand(time(NULL));

    while (1) {
        // Lire une commande du pipe (entr�e standard)
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;  // Si aucune commande, on sort de la boucle
        }


        // V�rifier si l'utilisateur a demand� d'arr�ter
        if (strncmp(command, "exit", 4) == 0) {
            printf("Fin du programme fr-simplecode.\n(tout droits r�serv�s � argentropcher, asign� au compte Google argentropcher,\nmodification strictement interdite selon l'article L.111-1 du Code de la propri�t� intellectuelle)\n");
            fflush(stdout);
            break;
        } else if (strncmp(command, "info", 4) == 0) {
            printf("INFO :\nLes calculs pris en compte sont basique: addition, soustraction, multiplication, division.\n(pour les virgules, utilisez '.')\nFonction :\n'rep' pour reprendre le r�sultat pr�c�dent\n");
            fflush(stdout);
            printf("'random[nombre1,nombre2]' renvoie un nombre al�atoire entre les 2 nombres entiers choisi\n'execute()' permet d'executer des commandes\n'var|'nomdelavariable'|' permet de cr�er/modifier des variables d'environnement (voir var.txt):\nutilisez 'var|nomdelavariable|=nombre/calcul/cercle()/triangle(;)/random[,]...' permet de d�finir la fonction\n'var|nomdelavariable|' la rappel du fichier si elle existe, (sinon valeur 0.00) pour l'utiliser dans un calcul\n+utiliser la fonction 'var_list()' pour obtenir la liste des variables de var.txt,\net 'var_suppr(all)' pour supprim� le fichier var.txt\nou 'var_suppr(nomdelavariable)' pour supprim� une variable du fichier var.txt\n'in_var(votrerecherche)' permet de savoir si il y a votrerecherche dans var.txt\nque se soit un nom de variable ou une valeur.\n");
            fflush(stdout);
            printf("'m' pour sauvgarder le r�sultat pr�c�dent\n'm+' pour importer le r�sultat sauvgarder et le stocker dans 'rep'\n'sqrt(argument)' pour la racine carr�e\n'cbrt(argument)' pour la racine cubique\n");
            fflush(stdout);
            printf("'local(argument)' pour sauvgarder une variable dans la m�moire vive avec en argument un calcul\n'inlocal()' pour r�cup�rer la valeur de 'local(argument)' et la mettre dans 'rep'\n");
            fflush(stdout);
            printf("'triangle(x;y)'pour calculer l'air d'un triangle de base=x et de hauteur=y\n'cercle(rayon)' pour calculer l'aire du cercle\n'fonction(argument)' pour plus d'info sur les fonctions\n'version' pour version\n'licence' pour licence\n'exit' pour quitter\n");
            fflush(stdout);
            printf("PARTIE CODE/PROGRAMME :\nLes programmes ont une structure de block imbriqu�e les un dans les autres.\nUn programme commence toujours par '[[[' et fini toujours par ']]]'.\nLes blocks � l'int�rieur de cela commence et finissent toutjours par '{' et '}'.\nLes diff�rents blocks :\n");
            fflush(stdout);
            printf("'{print(argument)}' ou '{affiche(argument)}' sont des blocks pour renvoi� un r�sultat\n l'argument peut �tre un calcul ou  un texte entre '' double.\nPour mettre 2 calcul diff�rents sans texte, s'�parer les par des '' double coll�.\n");
            fflush(stdout);
            printf("'{var|nomdelavariable|=valeurendouble}' est la m�me chose que pr�c�dament et permet de d�finir une variable\nutiliser 'var|nomdelavariable|' pour la rappeler.\nLes fonctions 'var_list()', 'var_suppr(nomdelavariableoutout)'\net 'in_var(votrevaleurouvariablerechercher)' sont toujours disponible en les int�grants dans des '{}'.\n");
            fflush(stdout);
            printf("'{if||v1||condition||v2||accoladeimbriqu�{}}' le if peut �tre remplacer par si, v1 et v2 sont des calculs, variable ...\ncondition peut �tre '?=' si c'est �gal, '!=' si ce n'est pas �gal, '<', '>', '<=' ou '>='.\nOn peut �galement rajout� en cas de condition fausse [[nombrede{}�sauterencasdeconditionfausse]]\nqui saute les premier {} si la condition est fausse.\n");
            fflush(stdout);
            printf("'{repeat||nombrederepetition||accoladeimbriqu�{}...}', le mot 'repeat' peut �tre remplacer par 'boucle' ou par 'r�p�te'\n nombreder�p�tition est le nombre de fois qu'il va ex�cuter la boucle,\nil peut aussi �tre 'infini' ou 'endless' pour fonctionn� ind�finiment.\nPour arr�ter une boucle, ont peut utiliser 'pause(nombred'arr�tdeboucle)' ou 'break(nombred'arr�tdeboucle)'\nqui permet de sortir des boucles\nsi la boucle est 'infini' ou 'endless', le nombred'arr�t � 1 suffi,\nsinon, il faut compter le nombre de r�p�tition � arr�ter\nSi vous ne voulez pas compter ou si la logique est trop compliqu�e,\nmettez une grande valeur et utilisez 'stop_break()' ou 'stop_pause()' � la sortie de la boucle.\n");
            fflush(stdout);
            printf("'{exit}' sort du programme compl�tement.\nFonction suppl�mentaire de l'affichage :\n");
            fflush(stdout);
            printf("en attente d'explication ...\n");
            fflush(stdout);
            continue;
        } else if (strncmp(command, "version", 7) == 0){
            char *exec_path = get_executable_path();
            printf("VERSION : 0.1\nutilisation win64_bit, path : %s\n", exec_path);
            fflush(stdout);
            continue;
        } else if (strncmp(command,"fonction(",9)==0){
            if (strncmp(command,"fonction()",10)==0 || strncmp(command,"fonction( )",10)==0 || strncmp(command,"fonction(fonction())",20)==0){
                printf("'fonction()' permet d'expliquer en d�taille une fonction (�cricre : 'fonction(la_fonction_choisie)'\n");
            } else if (strncmp(command,"fonction(m+)",12)==0){
               printf("'m+' permet de r�cup�rer la valeur de 'm' enregistr� dans result.txt\n");
            } else if (strncmp(command,"fonction(m)",11)==0) {
                printf("'m' permet de sauvgarder la valeur du r�sultat pr�c�dent dans une variable � long terme,\non peut r�cup�rer sa valeur lors d'une autre ex�cution de calculatrice basique C\n");
            } else if (strncmp(command,"fonction(rep)",13)==0) {
                printf("'rep' r�cup�re automatiquement le r�sultat de v�tre calcul pr�c�dent,\nvous pouvez l'utiliser directement dans un calcul (ex:'5/-rep+sqrt(rep*8)')\n");
            } else if (strncmp(command,"fonction(sqrt())",16)==0 || strncmp(command,"fonction(sqrt(argument))",24)==0) {
                printf("'sqrt(argument)' permet de calculer la racine carr� d'un nombre,\nil s'utilise directement dans un calcul (ex:'50-sqrt(5+2*2)/rep')\n");
            } else if (strncmp(command,"fonction(cbrt())",16)==0 || strncmp(command,"fonction(cbrt(argument))",24)==0) {
                printf("'cbrt(argument)' permet de calculer la racine cubique d'un nombre,\nil s'utilise directement dans un calcul (ex:'cbrt(5+22)+7')\n");
            } else if (strncmp(command,"fonction(local())",17)==0 || strncmp(command,"fonction(local(argument))",25)==0) {
                printf("'local(argument)' permet de calculer des '+', '-', '*', '/' et des 'sqrt()' et 'cbrt()' dans les parenth�ses,\nils sont sauvgard�s dans une m�moire vive uniquement lors de l'utilisation de l'appli,\nmais ne prend pas d'argument hors des parenth�ses (ex: 'local(5+cbrt(rep))')\n");
            } else if (strncmp(command,"fonction(inlocal())",19)==0) {
                printf("'inlocal()' permet de renvoyer la valeur stock�e durant la fonction 'local(argument)'(voir : 'fonction(local())'),\nla valeur est renvoyer dans rep (ex: 'inlocal()')\n");
            } else if (strncmp(command,"fonction(triangle(x;y))",23)==0 || strncmp(command,"fonction(triangle())",20)==0) {
                printf("'triangle(x;y)' permet de calculer l'aire d'un triangle (base*hauteur/2),\nx et y ne doivent �tre que des nombres et non des calculs,\non ne peut pas mettre des arguments � l'ext�rieur (ex: 'triangle(3;rep)')\n");
            } else if (strncmp(command,"fonction(cercle())",18)==0 || strncmp(command,"fonction(cercle(rayon))",23)==0) {
                printf("'cercle(rayon)' permet de calculer l'aire d'un cercle (pi*rayon�),\nrayon doit �tre uniquement un nombre et non un calcul,\non ne peut pas mettre d'argument hors de la parenth�se (ex: 'cercle(3)')\n");
            } else if (strncmp(command,"fonction(var||)",15)==0 || strncmp(command,"fonction(var)",13)==0) {
                printf("'var||' permet de cr�er des variables qui sont stock� uniquement sous la forme de double,\npour la d�finir, on utilise = apr�s son nom, et pour l'utiliser, il sufit d'�crire 'var|nomdelavariable|'.\nSi une erreur survient lors de sa d�finition ou si elle n'existe pas, �a valeur est automatiquement 0.00.\n");
            } else if (strncmp(command,"fonction(var_list())",20)==0 || strncmp(command,"fonction(var_list)",18)==0) {
                printf("'var_list()' permet de voir la liste des variables du fichier var.txt\n");
            }else if (strncmp(command,"fonction(random[])",18)==0 || strncmp(command,"fonction(random[nombre1,nombre2])",33)==0 || strncmp(command,"fonction(random)",16)==0){
                printf("fonction 'random[nombre1,nombre2]' renvoie un nombre entier al�atoire compris entre les 2 nombres entiers choisi par l'utilisateur\n");
            } else if (strncmp(command,"fonction(execute())",19)==0 || strncmp(command,"fonction(execute)",17)==0){
                printf("'execute()' permet de passer des commandes de type cmd depuis l'application, par exemple pour d�marer un programme gr�ce � la fonction start du cmd ...\n");
            } else {
                printf("Erreur : 'fonction(argument)' argument inconnu,\ntapez 'info' pour conna�tre les diff�rentes fonctions !\n");
            }
            fflush(stdout);
            continue;
        } else if (strncmp(command,"licence",7)==0){
            licence();
            // R�cup�rer le chemin de l'ex�cutable pour changer de r�pertoire
            char exe_path[MAX_PATH];
            if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
                printf("Erreur lors de l'obtention du chemin de l'ex�cutable.\n");
                fflush(stdout);
                continue;;
            }

            // Extraire le r�pertoire de l'ex�cutable
            char *last_backslash = strrchr(exe_path, '\\');
            if (last_backslash != NULL) {
                *last_backslash = '\0'; // Terminer la cha�ne � la position du dernier backslash
            }

            // Construire la commande pour changer de r�pertoire et ouvrir le fichier
            char command[MAX_PATH + 50];  // Buffer pour la commande
            snprintf(command, sizeof(command), "cd %s && start licence_fr-simplecode.txt", exe_path);

            // Ex�cuter la commande
            int licence_var = system(command);
            if (licence_var==0){
                printf("fichier licence_fr-simplecode.txt ouvert\n");
            } else {
                printf("Erreur d'ouverture de licence_fr_simplecode.txt\n");
            }
            fflush(stdout);
            continue;
        }

        //zone pour r�aficher la commande de l'utilisateur ou non
        if (strncmp(command,"output_input(false)",19)==0 || strncmp(command,"sortir_entr�e(faux)",19)==0){
            var_sortir_entree=false;
            printf("l'affichage de l'entr�e de l'utilisateur est d�sactiv�\n");
            fflush(stdout);
            continue;
        }
        if (var_sortir_entree==true){
            printf("%s",command);
            fflush(stdout);
        }
        if (strncmp(command,"output_input(true)",18)==0 || strncmp(command,"sortir_entr�e(vrai)",19)==0){
            var_sortir_entree=true;
            printf("l'affichage de l'entr�e de l'utilisateur est activ�\n");
            fflush(stdout);
            continue;
        }
        //fin zone pour r�aficher la commande de l'utilisateur ou non

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
            // Trouver la position de la parenth�se ouvrante et fermante
            char *start_in_var = strchr(command, '(');  // Trouve '('
            char *end_in_var = strchr(command, ')');    // Trouve ')'

            if (start_in_var != NULL && end_in_var != NULL && end_in_var > start_in_var) {
                // Extraire le contenu entre les parenth�ses
                size_t length = end_in_var - start_in_var - 1;  // longueur entre '(' et ')'
                char value[length + 1];  // +1 pour le caract�re null '\0'
                strncpy(value, start_in_var + 1, length);  // Copie tout sauf '(' et ')'
                value[length] = '\0';  // Ajouter le caract�re de fin de cha�ne

                // Passer la valeur extraite � handle_in_var()
                double result_in_var = handle_in_var(value);

                // Afficher le r�sultat
                printf("Le r�sultat de in_var est: %.2f (1.00 = recherche trouv�, 0.00 = pas trouv� dans le fichier)\n", result_in_var);
                fflush(stdout);
            } else {
                printf("Erreur : parenth�ses mal form�es.\n");
                fflush(stdout);
            }
            continue;
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
            handle_script(command);
            continue;
        } else {
            mode_script_output = 1;
        }

        // Supprimer les espaces blancs et retours � la ligne de l'expression
        command[strcspn(command, "\n")] = 0;
        //remplacer les time()
        strcpy(command,replace_actual_time(command));
        //remplacer les random[,]
        handle_random(command,last_result);

        if (strstr(command, "var|") != NULL) {
            //remplacer les in_var()
            replace_in_var(command);
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(command, replace_rep(command, last_result));
            // G�rer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
            strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son r�sultat
            strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son r�sultat
            // V�rification des commandes pour les aires
            if (strstr(command, "triangle(") != NULL) {
                // Trouver le signe '=' dans la commande
                char* command_after_equal = strchr(command, '=');  // Trouver '='

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer la cha�ne proprement

                    // Passer la commande apr�s '=' � la fonction
                    command_after_equal++;  // Avancer d'un caract�re pour ignorer '='
                    double aire_triangle = def_airetriangle(command_after_equal, last_result); // Appel de la fonction aire du triangle

                    if (aire_triangle != -0.01) {
                        snprintf(command, sizeof(command), "%s=%.2f", prefix, aire_triangle); // Convertir l'aire du triangle en cha�ne de caract�res
                    } else {
                        snprintf(command, sizeof(command), "%s=0.00", prefix); // Si l'aire est invalide, assigner "0.00"
                    }
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouv�, assigner "0.00"
                }
            } else if (strstr(command, "cercle(") != NULL) {
                // Trouver le signe '=' dans la commande
                char* command_after_equal = strchr(command, '=');  // Trouver '='

                if (command_after_equal != NULL) {
                    // Sauvegarder la partie avant `=`
                    char prefix[256] = {0};
                    strncpy(prefix, command, command_after_equal - command);
                    prefix[command_after_equal - command] = '\0';  // Terminer la cha�ne proprement

                    // Passer la commande apr�s '=' � la fonction
                    command_after_equal++;  // Avancer d'un caract�re pour ignorer '='
                    double aire_cercle = def_airecercle(command_after_equal, last_result); // Appel de la fonction aire du cercle

                    if (aire_cercle != -0.01) {
                        snprintf(command, sizeof(command), "%s=%.2f", prefix, aire_cercle); // Convertir l'aire du cercle en cha�ne de caract�res
                    } else {
                        snprintf(command, sizeof(command), "%s=0.00", prefix); // Si l'aire est invalide, assigner "0.00"
                    }
                } else {
                    strcpy(command, "0.00"); // Si pas de '=' trouv�, assigner "0.00"
                }
            }
            handle_implicit_multiplication(command);
            //printf("%s\n",command);
            replace_comma_with_dot(command);
            strcpy(command, handle_variable(command));  // Remplace les variables

            // Remplacement des virgules par des points
            char *ptr = command;
            while (*ptr) {  // Parcourt la cha�ne caract�re par caract�re
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

        // V�rifier que l'expression n'est pas vide
        if (strlen(command) == 0) {
            printf("Commande vide, r�essayez\n");
            fflush(stdout);
            continue;
        } else if (strcmp(command, "m") != 0) {
            autorise=0;
        }

        // Gestion des commandes sp�ciales
        if (strcmp(command, "m") == 0) {
            // Sauvegarder le dernier r�sultat dans un fichier
            if (last_result !=0.00){
                save_result(last_result);
                continue;
            } else {
                if (autorise==1){
                    autorise=0;
                    save_result(last_result);
                    continue;
                } else {
                    printf("�tes vous sur de vouloir sauvgarder une valeur null ? Si oui, refaite 'm', sinon faite une autre commande\n");
                    autorise=1;
                    continue;
                }
            }
        } else if (strcmp(command, "m+") == 0) {
            // Charger le r�sultat depuis un fichier et le placer dans last_result
            last_result = load_result();
            continue;
        }
        if (strncmp(command, "triangle(",9) == 0) {
            double airtriangle = def_airetriangle(command, last_result);
            printf("aire du triangle : %.2lf", airtriangle);
            printf(" m�\n");
            fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
            if (airtriangle!=-0.01){
                last_result=airtriangle;
                }
            continue;
        } else if (strncmp(command, "cercle(", 7) == 0) {
            double aircercle = def_airecercle(command, last_result);
            printf("aire du cercle : %.2lf", aircercle);
            printf(" m�\n");
            fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
            if (aircercle!=-0.01){
                last_result=aircercle;
                }
            continue;
        } else if (strncmp(command, "local(", 6) == 0) {
            // Extraire l'expression � l'int�rieur de local()
            char* start_expr = command + 6;
            char* end_expr =  strrchr(start_expr, ')');
            if (end_expr) {
                *end_expr = '\0'; // Terminer l'expression � la parenth�se fermante
                set_local(start_expr, last_result);
            } else {
                printf("Erreur : parenth�se fermante manquante pour local().\n");
                fflush(stdout);
            }
            continue;
        } else if (strcmp(command, "inlocal()") == 0) {
            last_result = get_local();
            printf("rep = %.2lf\n", last_result);
            fflush(stdout);
            continue;
        }

        // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
        if (strstr(command, "sqrt(") != NULL) {
            char* processed_command2="";
            // G�rer les expressions sqrt
            char* processed_command = handle_sqrt(command, last_result);
            // Afficher la commande apr�s traitement de sqrt pour diagnostic
            ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande trait�e
            for (int i = 0; processed_command[i] != '\0'; i++) {
                if (processed_command[i] == ',') {
                    processed_command[i] = '.'; // Remplacer la virgule par un point
                }
            }
            // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
            if (strstr(command, "cbrt(") != NULL) {
                // G�rer les expressions sqrt
                processed_command2 = handle_cbrt(processed_command, last_result);
                // Afficher la commande apr�s traitement de sqrt pour diagnostic
                ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
                // Remplacer les virgules par des points dans la commande trait�e
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
            // �valuer l'expression et renvoyer le r�sultat
            double result = eval(processed_command3);
            printf("%.2lf\n", result);
            fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
            last_result = result;
            continue;  // Continuer avec la prochaine it�ration de la boucle
        }

        // Gestion de l'expression sqrt(nombre) � n'importe quel endroit
        if (strstr(command, "cbrt(") != NULL) {
            char* processed_command2="";
            // G�rer les expressions sqrt
            char* processed_command = handle_cbrt(command, last_result);
            // Afficher la commande apr�s traitement de sqrt pour diagnostic
            ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
            // Remplacer les virgules par des points dans la commande trait�e
            for (int i = 0; processed_command[i] != '\0'; i++) {
                if (processed_command[i] == ',') {
                    processed_command[i] = '.'; // Remplacer la virgule par un point
                }
            }
            if (strstr(command, "sqrt(") != NULL) {
                // G�rer les expressions sqrt
                processed_command2 = handle_sqrt(processed_command, last_result);
                // Afficher la commande apr�s traitement de sqrt pour diagnostic
                ///printf("Commande apr�s traitement de 'sqrt' : %s\n", processed_command);
                // Remplacer les virgules par des points dans la commande trait�e
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
            // �valuer l'expression et renvoyer le r�sultat
            double result = eval(processed_command3);
            printf("%.2lf\n", result);
            fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python
            last_result = result;
            continue;  // Continuer avec la prochaine it�ration de la boucle
        }

        // Remplacer "rep" dans l'expression par la valeur de last_result
        char* modified_command = replace_rep(command, last_result);

        // Afficher la commande apr�s remplacement de "rep" pour diagnostic
        ///printf("Commande apr�s remplacement de 'rep' : %s\n", modified_command);

        //remplacer les ',' par des '.'
        replace_comma_with_dot(modified_command);

        // �valuer l'expression et renvoyer le r�sultat
        double result = eval(modified_command);
        printf("%.2lf\n", result);
        fflush(stdout);  // Envoyer imm�diatement le r�sultat � Python

        last_result=result;
    }

    return 0;
}
