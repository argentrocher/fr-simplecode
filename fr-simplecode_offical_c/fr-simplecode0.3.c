#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <direct.h>

#include <winsock2.h> //pour un server tcp
#include <ws2tcpip.h> //pour un server tcp
#pragma comment(lib, "Ws2_32.lib") //pour un server tcp

#include <windows.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include <sapi.h> //pour la voix speak() et parle()

//gestionnaire des var_local|| pour le stockage
#include "frc_environnement.h"

#define MAX_COMMAND_LEN 16384
#define MAX_LEN 1024

#define MAX_THREADS 101  // nombre de threads suivi pour pouvoir les fermés (ici maximum 101)

//fonction utilitaire pour le traitement des lignes des fichiers txt type table_frc.txt
size_t getline_win(char **lineptr, size_t *n, FILE *stream) {
    if (lineptr == NULL || n == NULL || stream == NULL)
        return -1;

    char *buffer = *lineptr;
    size_t size = *n;

    if (buffer == NULL || size == 0) {
        size = 128;
        buffer = malloc(size);
        if (buffer == NULL) return -1;
        *lineptr = buffer;
        *n = size;
    }

    size_t i = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (i + 1 >= size) {
            size *= 2;
            char *new_buffer = realloc(buffer, size);
            if (!new_buffer) return -1;
            buffer = new_buffer;
            *lineptr = buffer;
            *n = size;
        }
        buffer[i++] = c;
        if (c == '\n') break;
    }

    if (i == 0 && c == EOF) return -1;

    buffer[i] = '\0';
    return i;
}

bool error_in_stderr=false; //savoir si on doit envoyé les msg d'erreur sur stderr
bool error_lock_program=false;  //savoir si une erreur interompt le programme

int var_script_exit; //variable pour une sortie exit dans un script

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
        if (error_in_stderr==false){
            printf("Erreur lors de l'obtention du chemin de l'exécutable.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur lors de l'obtention du chemin de l'exécutable.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
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
            if (error_in_stderr==false){
                printf("Erreur lors de la création du fichier de licence.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur lors de la création du fichier de licence.\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return;
        }

        // Texte de la licence
        const char *licence_text =
            "Licence du logiciel fr-simplecode.exe sur Windows (v0.3):\n"
            "---------------------------------------------------\n"
            "Ce logiciel est sous licence pour un usage sous contrainte. Vous n'êtes pas autorisé "
            "à modifier ou reproduire ce logiciel sans l'autorisation explicite de l'auteur argentropcher, assigné au compte Google argentropcher. "
            "L'utilisation commerciale est autorisée à condition de respecter "
            "les termes de cette licence et de ne pas enfreindre les droits de l'auteur.\n\n"
            "Avertissement :\n"
            "Ce logiciel est fourni tel quel, sans aucune garantie. L'auteur ne pourra être tenu responsable "
            "de tout dommage résultant de l'utilisation de ce logiciel sur votre appareil.\n\n"
            "Information sur les dll :\nn'exécuter pas de fichier dll sans savoir ce qu'ils font réelment, aucune garantie n'est promise pour l'usage de dll depuis fr-simplecode !\n\n\nLicence d’utilisation inspirée du principe CC BY-ND (Attribution – Pas de modification).\n";

        // Écrire la licence dans le fichier
        fprintf(file, "%s", licence_text);

        // Fermer le fichier
        fclose(file);
        //printf("Le fichier de licence a été créé avec succès.\n");
    }
}

//version de l'app v0.3
char *handle_version(char *recup) {
    static char buffer[MAX_COMMAND_LEN]; // buffer de sortie
    char *src = recup;
    char *found;
    char version[64] = "0.3";

    buffer[0] = '\0';

    while ((found = strstr(src, "version()"))) {
        strncat(buffer, src, found - src);
        strcat(buffer, version);
        src = found + 9;
    }
    strcat(buffer, src);

    return buffer;
}

//configuration pour tout les programmes de var grâce à fr-simplecode.conf
void create_frc_conf() {
    char path_exe[1024];
    DWORD path_len = GetModuleFileNameA(NULL, path_exe, 1024);
    if (path_len == 0 || path_len == 1024) {
        printf("Erreur lors de la récupération du chemin de l'exécutable.\n");
        fflush(stdout);
        return;
    }

    // Cherche le dernier backslash pour trouver le dossier
    char *last_slash = strrchr(path_exe, '\\');
    if (last_slash != NULL) {
        // Coupe le chemin juste après le dernier backslash
        *(last_slash + 1) = '\0';
    }

    // Construit le chemin complet du fichier
    char path_file[1024];
    snprintf(path_file, sizeof(path_file), "%s%s", path_exe, "fr-simplecode.conf");

    // On essaie d'ouvrir le fichier en lecture seule
    FILE *fichier = fopen(path_file, "r");

    if (fichier != NULL) {
        // Le fichier existe déjà, on le ferme et on ne fait rien
        fclose(fichier);
        return;
    }

    // Le fichier n'existe pas, on le crée en écriture
    fichier = fopen(path_file, "w");
    if (fichier == NULL) {
        printf("Erreur lors de la création du fichier .conf\n");
        fflush(stdout);
        return;
    }

    //fwrite("\xEF\xBB\xBF", 1, 3, fichier); //utf-8

    fprintf(fichier, "Variable de configuration de fr-simplecode (depuis v0.3) global sans avoir besoin de les remettre à chaque exécution avec ###...###;\n###sortie:classique###\n###sortie_affiche:virgule###\n###sortie_affiche:[/n]###\n###fonction_num:oui###\n###fonction_text:oui###\n###autorise/:oui###\n###table_use_script_path:oui###\n###debug_use_script:non###\n###analyse_argument_dll:oui###\n###sortie_erreur:non###\n###stop_script_erreur:non###\n\nby argentropcher\n");
    fclose(fichier);
}

//ouvrir le fichier fr-simplecode.conf avec bloc-note
void start_frc_conf() {
    char path_exe[1024];
    DWORD path_len = GetModuleFileNameA(NULL, path_exe, sizeof(path_exe));
    if (path_len == 0 || path_len >= sizeof(path_exe)) {
        if (error_in_stderr == false) {
            printf("Erreur lors de la récupération du chemin de l'exécutable.\n");
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur lors de la récupération du chemin de l'exécutable.\n");
            fflush(stderr);
        }
        return;
    }

    // Isoler le dossier
    char* last_slash = strrchr(path_exe, '\\');
    if (last_slash != NULL) {
        *(last_slash + 1) = '\0';
    }

    // Construire chemin complet du fichier
    char path_file[1024];
    snprintf(path_file, sizeof(path_file), "%s%s", path_exe, "fr-simplecode.conf");

    // Préparer la ligne de commande
    char cmd_line[2048];
    snprintf(cmd_line, sizeof(cmd_line), "notepad.exe \"%s\"", path_file);

    // Structure pour CreateProcess
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);

    BOOL success = CreateProcessA(
        NULL,
        cmd_line,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        if (error_in_stderr == false) {
            printf("Erreur lors de l'ouverture du Bloc-notes.\n");
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur lors de l'ouverture du Bloc-notes.\n");
            fflush(stderr);
        }
        return;
    }

    // Donne un peu de temps pour que la fenêtre s'ouvre
    Sleep(500);

    // Tenter de mettre la fenêtre en avant-plan (très approximatif)
    HWND hwnd = NULL;
    // On peut essayer de récupérer la fenêtre du Notepad avec EnumWindows, en cherchant par PID
    DWORD pid = pi.dwProcessId;

    // Fonction pour récupérer la fenêtre principale via PID
    BOOL CALLBACK EnumWindowsProc(HWND wnd, LPARAM lParam) {
        DWORD wnd_pid = 0;
        GetWindowThreadProcessId(wnd, &wnd_pid);
        if (wnd_pid == (DWORD)lParam) {
            // vérifier que c'est une fenêtre visible
            if (IsWindowVisible(wnd)) {
                *(HWND*)(&hwnd) = wnd; // stocker la fenêtre trouvée
                return FALSE; // arrêter l'énumération
            }
        }
        return TRUE; // continuer
    }
    EnumWindows(EnumWindowsProc, (LPARAM)pid);

    if (hwnd != NULL) {
        AllowSetForegroundWindow(pi.dwProcessId);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hwnd);
        FlashWindow(hwnd, TRUE);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("fichier fr-simplecode.conf ouvert\n");
    fflush(stdout);
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
//pour récupérer la taille de l'écran
// Remplace dynamiquement une sous-chaîne par une autre dans une chaîne source
char *replace_all_size(const char *source, const char *target, const char *replacement) {
    size_t src_len = strlen(source);
    size_t tgt_len = strlen(target);
    size_t rep_len = strlen(replacement);
    size_t buffer_size = src_len + 1;

    // Estimation très large du buffer (au cas où remplacement > target et plusieurs occurrences)
    char *result = malloc(buffer_size * 4);
    if (!result) return NULL;

    const char *pos = source;
    char *dest = result;
    while ((pos = strstr(pos, target))) {
        size_t prefix_len = pos - source;
        strncpy(dest, source, prefix_len);
        dest += prefix_len;
        strcpy(dest, replacement);
        dest += rep_len;
        pos += tgt_len;
        source = pos;
    }
    strcpy(dest, source); // Ajoute le reste

    return result;
}

// Fonction principale : remplace screen_size(x), ecran_taille(y), etc.
char *replace_screen_dimensions(const char *input) {
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    char width_str[32], height_str[32];
    sprintf(width_str, "%d", screen_width);
    sprintf(height_str, "%d", screen_height);

    char *step1 = replace_all_size(input, "screen_size(x)", width_str);
    char *step2 = replace_all_size(step1, "ecran_taille(x)", width_str);
    free(step1);

    step1 = replace_all_size(step2, "screen_size(y)", height_str);
    free(step2);

    step2 = replace_all_size(step1, "ecran_taille(y)", height_str);
    free(step1);

    return step2; // Chaîne finale allouée dynamiquement (à libérer)
}
//fin pour récupérer la taille de l'écran

double last_result; //variable pour rep

char main_arg_global[2048] = ""; // variable globale pour --arg: fournit au démarage (récup dans code avec [main_arg])

int var_script_pause; //variable pour une sortie de boucle ou condition dans un script
int var_affiche_virgule; //variable pour afficher la virgule des nombres dans les print()/affiche()
int var_affiche_slash_n; //variable pour afficher le retour automatique à la fin d'un print()/affiche() ou non (0=oui 1=non)
int var_boucle_infini; //variable qui est à 1 lorsque une boucle infini est en court, sinon 0

bool var_activ_func_text=true; //variable pour savoir si la fonction text() est active ou non
bool var_activ_func_num=true; //variable pour savoir si la fonction num() est active ou non
bool autorise_slash_text_num_lock=true; //variable pour savoir si on active la désactivation des fonctions num() et text() si il y a /num() ou /text()

bool var_sortir_entree; //variable pour savoir si il faut renvoié l'entré de l'utilisateur dans la sortie

int mode_script_output; //variable pour le type de sortie en script (1= tout les blocks, 0= uniquement les erreurs + les sorties voulu)

int var_in_module; //variable pour savoir si on peut utiliser [input] si on est en module (0 ou 1)

bool dll_arg_analysis=true; //variable pour savoir si on peut analysé les arguments comment les print speak prompt (false=non)

int var_global_bool=-1; //variable libre pour l'utilisateur à 3 état none (-1), true (1) et false (0) ex: ###global_bool:true### ##state_global_bool##

bool use_script_path=true; //variable pour savoir si le chemin du script de use_script() doit être enregistré dans une table ou non (frc_use_script_path=[[vôtrechemin]])
bool debug_use_script=false; //variable pour afficher le script récupérer par use_script() lors de la construction du programme par un utilisateur (test de code)

int block_execute=0; //variable qui compte le nombre de block exécuté dans le programme

//controle des dll
// Version avec cache (simple) pour garder les dll en mémoire pour les threads dans les dll
static HINSTANCE hDllCache[128];
static int dllCount = 0;

int socket_var=0; //var pour le socket actif ou non pour les print() et affiche() (attention ne gère pas les messages d'erreurs qui sont visibles en exécution local)
SOCKET client;

// Fonction pour évaluer une simple expression avec priorité des opérateurs (déclaration de la fonction)
double eval_expr(const char *expr, int *index);

// Fonction calcul expression pour la fonction affiche de script et les conditions if
double condition_script_order(char *command);
//gestionnaire des if complexe
#include "frc_condition_power.h"

//fonction pour récupérer les 2 chiffres après la virgule
char *get_comma_only(const char *command);

//fonction pour table : demande d'une valeur (table||[][]...)
char* for_get_table_value(char *arg);
//fonction pour table : demande de la longueur d'une table (table||.len([]))
char* for_len_table(char *arg);
//fonction pour table : enregistrer une table + suppr() et join() (table||=[[ ; ; ]])
int parse_and_save_table(const char *input_line);
//fonction pour ajouté une valeur à la fin d'une table table||.add()
int add_table(const char *input_line);
//fonction pour détruire une valeur d'un table||.del([][])
int process_table_del(const char *recup);
//fonction pour table||.edit([][])(valeur)
int process_table_edit(const char *recup);
//fonction pour table||.sea()
void table_sea(const char *recup);

//fonction use_script()[]
void use_script_launcher(const char *command);

//traité les chaines de guillemet pour print, speak ...
char* traiter_chaine_guillemet(const char *input);
//num()
char *auto_num(char *recup);
//text()
char *auto_text(const char *recup);

void fonction_start_remove_newlines(char *str);

void handle_script(char *expr); //definition de handle_script ici pour pouvoir l'utiliser dans le thread (fonction threading())

char *get_prompt(const char *recup); //pour les prompt()

//zone thread
HANDLE thread_handles[MAX_THREADS];
int thread_count = 0;

int have_thread=0;

// Structure pour passer des données au thread
typedef struct {
    char expr[MAX_COMMAND_LEN];
} ThreadData;

// Fonction wrapper pour le thread
DWORD WINAPI thread_entry(LPVOID lpParam) {
    ThreadData *data = (ThreadData *)lpParam;
    handle_script(data->expr);
    free(data);  // Libération de la mémoire allouée

    have_thread--; //le thread est fini, libération du thread pour obtimiser les performences

    return 0;
}

// Fonction principale demandée threading()
void process_threading(char *recup) {
    if (strncmp(recup, "threading(", 10) != 0) return;

    char *ptr = recup + 10;
    int paren_depth = 1;
    int brace_depth = 0;
    char *start = ptr;
    char *end = NULL;

    while (*ptr && paren_depth > 0) {
        if (*ptr == '{') {
            brace_depth++;
        } else if (*ptr == '}') {
            if (brace_depth > 0) brace_depth--;
        } else if (*ptr == '(' && brace_depth == 0) {
            paren_depth++;
        } else if (*ptr == ')' && brace_depth == 0) {
            paren_depth--;
            if (paren_depth == 0) {
                end = ptr;
                break;
            }
        }
        ptr++;
    }

    if (!end) {
        if (error_in_stderr==false){
            printf("Erreur : parenthèses déséquilibrées dans threading(...)\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : parenthèses déséquilibrées dans threading(...)\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    // Extraire l'expression
    int len = end - start;
    if (len >= MAX_COMMAND_LEN) {
        if (error_in_stderr==false){
            printf("Erreur : expression trop longue pour threading(...)\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : expression trop longue pour threading(...)\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    ThreadData *data = malloc(sizeof(ThreadData));
    if (!data) {
        if (error_in_stderr==false){
            printf("Erreur : allocation mémoire pour le thread\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : allocation mémoire pour le thread\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    strncpy(data->expr, start, len);
    data->expr[len] = '\0';

    have_thread++;
    if (have_thread<1){
        have_thread=1;
    }

    // Lancer le thread
    HANDLE hThread = CreateThread(
        NULL,       // attributs de sécurité par défaut
        0,          // taille de pile par défaut
        thread_entry, // fonction du thread
        data,       // argument
        0,          // flags
        NULL        // ID du thread
    );

    if (!hThread) {
        if (error_in_stderr==false){
            printf("Erreur : impossible de créer le thread\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible de créer le thread\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        free(data);
        have_thread-=1;
        return;
    }

    // Sauvegarde le handle du thread
    if (thread_count < MAX_THREADS) {
        thread_handles[thread_count++] = hThread;
    } else {
        // Trop de threads – le thread est lancé, mais on ne le suit pas
        CloseHandle(hThread);  // éviter les fuites si on ne le stocke pas
        printf("! trop de threading en même temps : %d (ils ne sont plus arrêtable !)\n",thread_count);
        fflush(stdout);
    }
}

void terminate_all_threadings() {
    for (int i = 0; i < thread_count; i++) {
        if (thread_handles[i]) {
            TerminateThread(thread_handles[i], 0);
            CloseHandle(thread_handles[i]);         // Nettoyage
            thread_handles[i] = NULL;
        }
    }
    thread_count = 0;
    have_thread = 0;
}
//fin zone thread

//debut zone controle des fichier dans les threads
//control des fichiers pour empécher la simultanné sur les mêmes fichiers à cause des threading()
HANDLE get_file_mutex(const char* filename) {
    // Nom du mutex global basé sur le nom du fichier
    char mutex_name[MAX_PATH];
    snprintf(mutex_name, MAX_PATH, "Global\\FILE_LOCK_%s", filename);

    // Supprimer les caractères non valides pour les noms d'objets
    for (int i = 0; mutex_name[i]; ++i) {
        if (mutex_name[i] == '\\' || mutex_name[i] == '/' || mutex_name[i] == ':')
            mutex_name[i] = '_';
    }

    return CreateMutexA(NULL, FALSE, mutex_name);
}

int try_lock_file(const char* filename) {
    HANDLE hMutex = get_file_mutex(filename);
    if (!hMutex) {
        if (error_in_stderr==false){
            printf("Erreur : impossible de créer un mutex pour %s\n", filename);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible de créer un mutex pour %s\n", filename);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return 0;
    }

    DWORD result = WaitForSingleObject(hMutex, 0);  // ne bloque pas
    if (result == WAIT_OBJECT_0) {
        // On a le verrou
        return 1;
    } else {
        // Verrou déjà pris
        CloseHandle(hMutex);
        return 0;
    }
}

void unlock_file(const char* filename) {
    HANDLE hMutex = get_file_mutex(filename);
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
}
//fin zone controle des fichier dans les threads

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
//pour m et m+
void initialize_filename() {
    char *exec_path = get_executable_path();
    snprintf(FILENAME, sizeof(FILENAME), "%s\\result.txt", exec_path); // Met à jour le chemin complet
}

//nano start frc
char* handle_nano_start_frc(char *command) {
    const char *prefix = "nano start frc ";
    if (strncmp(command, prefix, strlen(prefix)) != 0) {
        return NULL; // Pas une commande nano start frc
    }

    char *raw = command + strlen(prefix);
    raw[strcspn(raw, "\r\n")] = '\0'; // Nettoyer retour chariot / saut de ligne

    // Chercher une éventuelle section [argument]
    char *bracket_open = strchr(raw, '[');
    char *bracket_close = (bracket_open != NULL) ? strchr(bracket_open, ']') : NULL;

    char filename[256] = {0};
    char arg_text[256] = {0};

    if (bracket_open && bracket_close && bracket_close > bracket_open) {
        // Extraire le nom du fichier (en supprimant les espaces avant le '[')
        size_t name_len = (size_t)(bracket_open - raw);
        while (name_len > 0 && isspace((unsigned char)raw[name_len - 1])) {
            name_len--;
        }
        strncpy(filename, raw, name_len);
        filename[name_len] = '\0';

        // Extraire le contenu entre les crochets
        size_t arg_len = (size_t)(bracket_close - bracket_open - 1);
        strncpy(arg_text, bracket_open + 1, arg_len);
        arg_text[arg_len] = '\0';
    } else {
        // Aucun argument [arg], prendre tout
        strncpy(filename, raw, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }

    // Refuser les chemins (slash)
    if (strchr(filename, '/') || strchr(filename, '\\')) {
        if (error_in_stderr==false){
            printf("Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stderr);
        }
        return NULL;
    }

    // Ajouter l'extension .frc si manquante
    char clean_name[256];
    if (strstr(filename, ".frc") == NULL || strcmp(filename + strlen(filename) - 4, ".frc") != 0) {
        snprintf(clean_name, sizeof(clean_name), "%s.frc", filename);
    } else {
        strncpy(clean_name, filename, sizeof(clean_name));
        clean_name[sizeof(clean_name) - 1] = '\0';
    }

    // Obtenir le chemin de l'exécutable
    char *exe_dir = get_executable_path();

    // Construire le chemin du dossier frc-file
    char frc_dir[MAX_PATH];
    snprintf(frc_dir, sizeof(frc_dir), "%s\\frc-file", exe_dir);

    // Créer le dossier si nécessaire
    _mkdir(frc_dir);

    // Construire le chemin complet du fichier
    char full_filename[MAX_PATH];
    snprintf(full_filename, sizeof(full_filename), "%s\\%s", frc_dir, clean_name);

    // Vérifier si le fichier existe
    FILE *test = fopen(full_filename, "r");
    if (!test) {
        if (error_in_stderr==false){
            printf("Erreur : le fichier '%s' n'existe pas.\n", clean_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : le fichier '%s' n'existe pas.\n",clean_name);
            fflush(stderr);
        }
        return NULL;
    }
    fclose(test);

    char for_use_script[512];
    if (arg_text[0] != '\0') {
        snprintf(for_use_script, sizeof(for_use_script), "use_script(%s)[%s]", full_filename, arg_text);
    } else {
        snprintf(for_use_script, sizeof(for_use_script), "use_script(%s)", full_filename);
    }
    use_script_launcher(for_use_script);

    return NULL;
}

//nano suppr frc
char* handle_nano_suppr_frc(char *command) {
    const char *prefix = "nano suppr frc ";
    if (strncmp(command, prefix, strlen(prefix)) != 0) {
        return NULL; // Pas une commande nano suppr frc
    }

    char *filename = command + strlen(prefix);

    filename[strcspn(filename, "\r\n")] = '\0';

    // Refuser les chemins (slash)
    if (strchr(filename, '/') || strchr(filename, '\\')) {
        if (error_in_stderr==false){
            printf("Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stderr);
        }
        return NULL;
    }

    // Ajouter l'extension .frc si manquante
    char clean_name[256];
    if (strstr(filename, ".frc") == NULL || strcmp(filename + strlen(filename) - 4, ".frc") != 0) {
        snprintf(clean_name, sizeof(clean_name), "%s.frc", filename);
    } else {
        strncpy(clean_name, filename, sizeof(clean_name));
        clean_name[sizeof(clean_name) - 1] = '\0';
    }

    // Obtenir le chemin de l'exécutable
    char *exe_dir = get_executable_path();

    // Construire le chemin du dossier frc-file
    char frc_dir[MAX_PATH];
    snprintf(frc_dir, sizeof(frc_dir), "%s\\frc-file", exe_dir);

    // Créer le dossier si nécessaire
    _mkdir(frc_dir);

    // Construire le chemin complet du fichier
    char full_filename[MAX_PATH];
    snprintf(full_filename, sizeof(full_filename), "%s\\%s", frc_dir, clean_name);

    // Vérifier si le fichier existe
    FILE *test = fopen(full_filename, "r");
    if (!test) {
        if (error_in_stderr==false){
            printf("Erreur : le fichier '%s' n'existe pas.\n", clean_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : le fichier '%s' n'existe pas.\n",clean_name);
            fflush(stderr);
        }
        return NULL;
    }
    fclose(test);

    // Supprimer le fichier
    if (remove(full_filename) == 0) {
        printf("Fichier '%s' supprimé avec succès.\n", clean_name);
        fflush(stdout);
    } else {
        if (error_in_stderr==false){
            printf("Erreur lors de la suppression de %s\n",clean_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur lors de la suppression de %s\n",clean_name);
            fflush(stderr);
        }
    }

    return NULL;
}


//nano frc
char* handle_nano_frc(char *command) {
    const char *prefix = "nano frc ";
    if (strncmp(command, prefix, strlen(prefix)) != 0) {
        return NULL; // Ne gère pas cette commande
    }

    char *filename = command + strlen(prefix);

    filename[strcspn(filename, "\r\n")] = '\0';

    // Vérifie qu’il n’y a pas de / ou \ (donc pas de chemin)
    if (strchr(filename, '/') || strchr(filename, '\\')) {
        if (error_in_stderr==false){
            printf("Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : seuls les noms de fichiers simples sont autorisés, pas de chemins.\n");
            fflush(stderr);
        }
        return NULL;
    }

    // Ajoute ".frc" si nécessaire
    char full_filename[256];
    if (!strstr(filename, ".frc")) {
        snprintf(full_filename, sizeof(full_filename), "%s.frc", filename);
    } else {
        strncpy(full_filename, filename, sizeof(full_filename) - 1);
        full_filename[sizeof(full_filename) - 1] = '\0';
    }

    // Obtenir le chemin de l'exécutable
    char *exe_path = get_executable_path();

    // Créer le chemin du dossier frc-file
    char frc_folder[MAX_PATH];
    snprintf(frc_folder, sizeof(frc_folder), "%s\\frc-file", exe_path);
    _mkdir(frc_folder); // Crée le dossier si nécessaire (ignore si déjà existant)

    // Construire le chemin complet du fichier
    char filepath[MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s\\%s", frc_folder, full_filename);

    // Vérifie si le fichier existe
    FILE *file = fopen(filepath, "r");
    if (file) {
        fclose(file);
        // Ouvre avec Bloc-notes
        ShellExecute(NULL, "open", "notepad.exe", filepath, NULL, SW_SHOW);
        printf("Le fichier '%s' est ouvert\n", full_filename);
        fflush(stdout);
    } else {
        // Fichier inexistant saisie utilisateur
        printf("Le fichier '%s' n'existe pas. Entrez son contenu ci-dessous (ligne vide pour terminer ou 'nano exit'):\n", full_filename);
        fflush(stdout);

        FILE *new_file = fopen(filepath, "w");
        if (!new_file) {
            if (error_in_stderr==false){
                printf("Erreur : impossible de créer le fichier frc.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : impossible de créer le fichier frc.\n");
                fflush(stderr);
            }
            return NULL;
        }

        char line[1024];
        int empty_line_count = 0;

        while (true) {
            printf("> ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) break;

            // Vérifie si la ligne commence par "nano exit"
            if (strncmp(line, "nano exit", 9) == 0) {
                break;
            }

            fputs(line, new_file);

            // Vérifier si c’est une ligne vide (juste un retour à la ligne)
            if (strcmp(line, "\n") == 0) {
                empty_line_count++;
                if (empty_line_count >= 2) break; // Deux lignes vides consécutives
                continue; // On ne l’écrit pas dans le fichier
            } else {
                empty_line_count = 0;
            }
        }

        fclose(new_file);

        printf("\n");
        fflush(stdout);

        // Lire tout le fichier pour validation
        FILE *check_file = fopen(filepath, "r");
        if (!check_file) {
            if (error_in_stderr==false){
                printf("Erreur : impossible de relire le fichier pour validation.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : impossible de relire le fichier pour validation.\n");
                fflush(stderr);
            }
            return NULL;
        }

        char file_content[65536];  // 64k max pour le script
        size_t total_read = fread(file_content, 1, sizeof(file_content) - 1, check_file);
        file_content[total_read] = '\0';  // Terminer proprement
        fclose(check_file);

        // Vérification des balises
        char *start = strstr(file_content, "[[[");
        char *end = strstr(file_content, "]]]");

        if (!start || !end || start > end) {
            remove(filepath);
            if (error_in_stderr==false){
                printf("Erreur : fichier invalide. Il doit contenir [[[ ... ]]] avec des instructions valides.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : fichier invalide. Il doit contenir [[[ ... ]]] avec des instructions valides.\n");
                fflush(stderr);
            }
            return NULL;
        }

        // Vérifie qu'il y a au moins un { et un } entre [[[ et ]]]
        int has_open = 0, has_close = 0;
        for (char *p = start; p < end; p++) {
            if (*p == '{') has_open = 1;
            if (*p == '}') has_close = 1;
        }

        if (!has_open || !has_close) {
            remove(filepath);
            if (error_in_stderr==false){
                printf("Erreur : le bloc [[[ ... ]]] doit contenir au moins une accolade ouvrante et fermante.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : le bloc [[[ ... ]]] doit contenir au moins une accolade ouvrante et fermante.\n");
                fflush(stderr);
            }
            return NULL;
        }

        printf("Fichier '%s' créé avec succès.\n", full_filename);
        fflush(stdout);
    }

    return NULL; // Peut être modifié pour renvoyer une info
}
//fin zone nano

//debut traitement zone de variable local (var_local||)
// Liste des caractères interdits dans un nom de variable
int has_invalid_var_local_chars(const char* varname) {
    for (int i = 0; varname[i]; ++i) {
        if (varname[i] == '[' || varname[i] == ']' || varname[i] == '{' ||
            varname[i] == '}' || varname[i] == ';' || varname[i] == '"' ||
            varname[i] == '(' || varname[i] == ')') {
            return 1;
        }
    }
    return 0;
}

//assigné une variable local
char* handle_var_local_assignment(char* line) {
    if (strncmp(line, "var_local|", 10) != 0) return "0";

    char* mid = strstr(line + 10, "|=");
    if (!mid) return "0";

    char varname[512] = {0};
    strncpy(varname, line + 10, mid - (line + 10));
    if (has_invalid_var_local_chars(varname)) {
        if (error_in_stderr==false){
            printf("Erreur : nom de variable locale invalide.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : nom de variable locale invalide.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return "0";
    }

    char* equal = strchr(mid + 1, '=');
    if (!equal) return "0";

    char* raw_value = equal + 1;
    char final_value[MAX_COMMAND_LEN] = {0};

    if (*raw_value == '"') {
        int paren_depth = 0;
        char *end_quote = NULL;
        char *p = raw_value + 1;

        while (*p) {
            if (*p == '(') {
                paren_depth++;
            } else if (*p == ')') {
                if (paren_depth > 0) paren_depth--;
            } else if (*p == '"' && paren_depth == 0) {
                end_quote = p;
                break; // trouvé le guillemet de fin
            }
            p++;
        }
        if (!end_quote) {
            if (error_in_stderr==false){
                printf("Erreur : guillemet de fin manquant sur var_local\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : guillemet de fin manquant sur var_local\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return "0";
        }
        char pre_value[MAX_COMMAND_LEN] = {0};
        strncpy(pre_value, raw_value + 1, end_quote - (raw_value + 1));

        if (strstr(pre_value,"text(") != NULL && var_activ_func_text==true) {
            char* temp_result = auto_text(pre_value);
            if (temp_result) {
                strncpy(final_value, temp_result, 2048);
                final_value[2048 - 1] = '\0';
                free(temp_result);
            } else {
                strcpy(final_value,pre_value);
            }
        } else {
            strcpy(final_value,pre_value);
        }

        if (strstr(final_value,"num(") != NULL && var_activ_func_num==true) {
            strcpy(final_value, auto_num(final_value));
        }

    } else {
        char expression[MAX_COMMAND_LEN];
        if (raw_value[0] == '+' && raw_value[1] == '+') {
            // Remplacer "++" par "var_local|varname|+1"
            snprintf(expression, sizeof(expression), "var_local|%s|+1", varname);
        } else if (raw_value[0] == '-' && raw_value[1] == '-') {
            // Remplacer "--" par "var_local|varname|-1"
            snprintf(expression, sizeof(expression), "var_local|%s|-1", varname);
        } else {
            //laissé l'expression original
            strncpy(expression, raw_value, sizeof(expression) - 1);
            expression[sizeof(expression) - 1] = '\0';
        }
        // Appel condition_script_order
        double results = condition_script_order(expression);
        char result[512];
        snprintf(result, sizeof(result), "%.2f", results);
        if (result) strncpy(final_value, result, sizeof(final_value) - 1);
        else strncpy(final_value, "0", sizeof(final_value));
    }

    set_env_object(current_env_object(), varname, final_value);
    return "1";
}

//récupérer une variable local
void handle_var_local_get(char* text, size_t max_size) {
    char buffer[MAX_COMMAND_LEN] = {0};
    char* src = text;
    char* dst = buffer;
    bool in_quotes = false;

    while (*src && (dst - buffer) < (int)max_size - 1) {
        if (*src == '"') {
            // Guillemet trouvé -> ignorer si dedans
            in_quotes = !in_quotes;
            *dst++ = *src++;
            continue;
        }

        if (!in_quotes && strncmp(src, "var_local|", 10) == 0) {
            char* end = strchr(src + 10, '|');
            if (end && *(end + 1) != '=') {
                char varname[512] = {0};
                strncpy(varname, src + 10, end - (src + 10));
                const char* value = get_env_object(current_env_object(), varname);
                if (strcmp(value, "Variable non trouvée.") == 0) value = "0";
                size_t len = strlen(value);
                if ((dst - buffer) + len >= max_size - 1) break;
                strcpy(dst, value);
                dst += len;
                src = end + 1;
                continue;
            }
        }

        *dst++ = *src++;
    }
    *dst = '\0';
    strncpy(text, buffer, max_size - 1);
    text[max_size - 1] = '\0';
}

//supprimé une variable local dans l'environnement actuel (var_local_suppr())
void handle_var_local_suppr(char* line) {
    const char* prefix = "var_local_suppr(";
    size_t len_prefix = strlen(prefix);

    if (strncmp(line, prefix, len_prefix) != 0) {
        return; // Ne commence pas par var_local_suppr(
    }

    int depth = 1;
    char* start = line + len_prefix;
    char* ptr = start;

    while (*ptr && depth > 0) {
        if (*ptr == '(') depth++;
        else if (*ptr == ')') depth--;
        ptr++;
    }

    if (depth != 0) {
        if (error_in_stderr==false){
            printf("Erreur : parenthèses non équilibrées sur var_local_suppr()\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : parenthèses non équilibrées sur var_local_suppr()\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    size_t var_len = ptr - 1 - start;
    char varname[512] = {0};
    if (var_len >= sizeof(varname)) {
        if (error_in_stderr==false){
            printf("Erreur : nom de variable trop long sur var_local_suppr().\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : nom de variable trop long sur var_local_suppr().\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    strncpy(varname, start, var_len);
    varname[var_len] = '\0';

    if (suppr_var_env_object(current_env_object(), varname)) {
        // Succès, rien à faire
    } else {
        if (error_in_stderr==false){
            printf("Erreur : variable '%s' introuvable dans l'environnement actuel.\n", varname);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : variable '%s' introuvable dans l'environnement actuel.\n", varname);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
    }
}

//fin traitement zone de variable local (var_local||)

//debut traitement zone de variable
char VAR_FILE[MAX_PATH];

void init_var_file_path() {
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    // Retirer le nom de l'exe pour ne garder que le dossier
    char *last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';  // coupe après le dernier '\'
    }

    snprintf(VAR_FILE, sizeof(VAR_FILE), "%s%s", exe_path, "var.txt");
}

double handle_in_var(char *value) {
    if (have_thread>=1){
        // Tentative de verrouillage du fichier avec 2 essais espacés
        int locked = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (try_lock_file(VAR_FILE)) {
                locked = 1;
                break;
            } else {
                Sleep(200);  // attendre 200 ms avant une nouvelle tentative
        }
        }

        if (!locked) {
            if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return 0.00;
        }
    }

    FILE *file = fopen(VAR_FILE, "r");  // Ouvre var.txt en lecture
    if (file == NULL) {
        // Si le fichier n'existe pas, retourner 0.00
        if (error_in_stderr==false){
            printf("Erreur : Le fichier var.txt n'existe pas.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : Le fichier var.txt n'existe pas.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        if (have_thread>=1){
            unlock_file(VAR_FILE);
        }
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
            if (have_thread>=1){
                unlock_file(VAR_FILE);
            }
            return 1.00;
        }
    }

    fclose(file);  // Fermer le fichier si la valeur n'est pas trouvée
    if (have_thread>=1){
        unlock_file(VAR_FILE);
    }
    return 0.00;  // Retourne 0.00 si la valeur n'a pas été trouvée
}

void replace_in_var(char *command) {
    char result[MAX_COMMAND_LEN] = "";  // La chaîne pour stocker le résultat final
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
    if (have_thread>=1){
        // Tentative de verrouillage du fichier avec 3 essais espacés
        int locked = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (try_lock_file(VAR_FILE)) {
                locked = 1;
                break;
            } else {
                Sleep(200);  // attendre 200 ms avant une nouvelle tentative
        }
        }

        if (!locked) {
            if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return;
        }
    }
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) {
        if (error_in_stderr==false){
            printf("Erreur : Impossible d'ouvrir var.txt\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : Impossible d'ouvrir var.txt\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        if (have_thread>=1){
            unlock_file(VAR_FILE);
        }
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
    if (have_thread>=1){
        unlock_file(VAR_FILE);
    }
}

void handle_var_suppr(const char *arg) {
    if (strcmp(arg, "tout") == 0 || strcmp(arg, "all") == 0) {
        // Supprimer complètement le fichier var.txt
        if (remove(VAR_FILE) == 0) {
            printf("Toutes les variables ont été supprimées.\n");
            fflush(stdout);
        } else {
            if (error_in_stderr==false){
                printf("Erreur : Impossible de supprimer le fichier %s.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Impossible de supprimer le fichier %s.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
        }
        return;
    }

    if (have_thread>=1){
        // Tentative de verrouillage du fichier avec 2 essais espacés
        int locked = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (try_lock_file(VAR_FILE)) {
                locked = 1;
                break;
            } else {
                Sleep(200);  // attendre 200 ms avant une nouvelle tentative
        }
        }

        if (!locked) {
            if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return;
        }
    }
    // Si ce n'est pas "tout" ou "all", chercher et supprimer une seule variable
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) {
        if (error_in_stderr==false){
            printf("Erreur : Impossible d'ouvrir %s.\n", VAR_FILE);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : Impossible d'ouvrir %s.\n", VAR_FILE);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        if (have_thread>=1){
            unlock_file(VAR_FILE);
        }
        return;
    }

    FILE *tempFile = fopen("var_temp.txt", "w");
    if (!tempFile) {
        if (error_in_stderr==false){
            printf("Erreur : Impossible de créer un fichier temporaire.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : Impossible de créer un fichier temporaire.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        fclose(file);
        if (have_thread>=1){
            unlock_file(VAR_FILE);
        }
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
        //printf("La variable \"%s\" a été supprimée.\n", arg);
        //fflush(stdout);
    } else {
        remove("var_temp.txt"); // Si aucune suppression n'a eu lieu, supprimer le fichier temporaire
        printf("Variable \"%s\" non trouvée, aucune suppression effectuée.\n", arg);
        fflush(stdout);
    }
    if (have_thread>=1){
        unlock_file(VAR_FILE);
    }
}

void save_variable(const char *name, const char *value_str) {
    if (have_thread>=1){
        // Tentative de verrouillage du fichier avec 3 essais espacés
        int locked = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (try_lock_file(VAR_FILE)) {
                locked = 1;
                break;
            } else {
                Sleep(200);  // attendre 200 ms avant une nouvelle tentative
        }
        }

        if (!locked) {
            if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return;
        }
    }
    FILE *file = fopen(VAR_FILE, "r");
    if (!file) {
        file = fopen(VAR_FILE, "w");
        if (!file) {
            if (error_in_stderr==false){
                printf("Erreur : Impossible d'ouvrir var.txt\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Impossible d'ouvrir var.txt\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            if (have_thread>=1){
                unlock_file(VAR_FILE);
            }
            return;
        }
    }

    // Lire toutes les variables existantes
    char line[100], temp[MAX_LEN] = "";
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        char var_name[128], var_value_str[1024];

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
    file = fopen(VAR_FILE, "w");
    if (file) {
        fputs(temp, file);
        fclose(file);
    }
    if (have_thread>=1){
        unlock_file(VAR_FILE);
    }
}



char* get_variable(const char *name) {
    static char value[1024];  // buffer statique pour le retour
    value[0] = '\0';

    if (have_thread>=1){
        // Tentative de verrouillage du fichier avec 3 essais espacés
        int locked = 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (try_lock_file(VAR_FILE)) {
                locked = 1;
                break;
            } else {
                Sleep(200);  // attendre 200 ms avant une nouvelle tentative
        }
        }

        if (!locked) {
            if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", VAR_FILE);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return "0.00";
        }
    }
    FILE *file = fopen(VAR_FILE, "r");
    if (!file){
        if (have_thread>=1){
            unlock_file(VAR_FILE);
        }
        return "0.00";  // Si le fichier n'existe pas, renvoyer 0.00
    }

    char line[100], var_name[128], var_value[1024];


    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%127[^=]=%1023[^\n]", var_name, var_value) == 2) {
            if (strcmp(var_name, name) == 0) {
                fclose(file);
                if (have_thread>=1){
                    unlock_file(VAR_FILE);
                }
                strncpy(value, var_value, sizeof(value)-1);
                value[sizeof(value)-1] = '\0';
                return value;
            }
        }
    }

    fclose(file);
    if (have_thread>=1){
        unlock_file(VAR_FILE);
    }
    return "0.00";  // Si la variable n'est pas trouvée
}

char* handle_variable(char *expr) {
    static char new_expr[MAX_LEN];
    char temp_expr[MAX_LEN];
    char *start = expr, *pos;
    char name[128];
    char valueforsave[1024];
    double value;

    new_expr[0] = '\0';  // Réinitialiser l'expression

    // Vérifier si c'est une affectation `var|nom|=valeur`
    if (sscanf(expr, "var|%127[^|]|=%1023[^\n]", name, &valueforsave) == 2) {
        if (strpbrk(name, "[]={}\"()")) {
            if (error_in_stderr==false){
                printf("Erreur : le nom de la variable contient un caractère interdit : [ ] = { } \" ( )\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : le nom de la variable contient un caractère interdit : [ ] = { } \" ( )\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return "None";
        }

        // Vérifier si c'est du texte entre guillemets
        size_t len = strlen(valueforsave);
        if (len >= 2 && valueforsave[0] == '"' && valueforsave[len - 1] == '"') {
            // Retirer les guillemets
            valueforsave[len - 1] = '\0';
            memmove(valueforsave, valueforsave + 1, len - 1);
            // Sauvegarder directement comme texte
            save_variable(name, valueforsave);
            if (mode_script_output==1){
                printf("Variable texte mise à jour : %s = \"%s\"\n", name, valueforsave);
                fflush(stdout);
            }
            return "None";
        }

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
        // Ajouter la valeur dans new_expr
        char *value_text = get_variable(var_name);
        strcat(new_expr, value_text);

        // Passer à la suite
        start = end + 1;
        strcpy(temp_expr, start);
    }

    // Ajouter le reste de l'expression
    strcat(new_expr, temp_expr);

    return new_expr;
}
//fin traitement zone de variable


//new random plus performant random(;) peut faire du nombre à virgule et plus aléatoire
char* handle_random2(char *expr) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));  // Seed random generator
        seeded = 1;
    }

    static char buffer[MAX_COMMAND_LEN];
    strcpy(buffer, expr);

    char *start = strstr(buffer, "random(");

    while (start != NULL) {
        char *ptr = start + 7;  // après "random("
        int open_parens = 1;
        while (*ptr && open_parens > 0) {
            if (*ptr == '(') open_parens++;
            else if (*ptr == ')') open_parens--;
            ptr++;
        }

        if (open_parens != 0) {
            if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées dans random().\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées dans random().\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strncpy(start, "0.00", 4);
            return buffer;
        }

        char *end = ptr - 1;

        // Extraire ce qu'il y a entre les parenthèses
        int len = end - start - 7;  // exclut "random("
        if (len <= 0 || len >= MAX_COMMAND_LEN) {
            if (error_in_stderr==false){
                printf("Erreur : syntaxe incorrecte dans random().\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : syntaxe incorrecte dans random().\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strncpy(start, "0.00", 4);
            return buffer;
        }

        char range[MAX_COMMAND_LEN];
        strncpy(range, start + 7, len);
        range[len] = '\0';

        // Chercher le séparateur ;
        char *sep = NULL;
        int paren_level = 0;
        for (int i = 0; range[i] != '\0'; i++) {
            if (range[i] == '(') paren_level++;
            else if (range[i] == ')') paren_level--;
            else if (range[i] == ';' && paren_level == 0) {
                sep = &range[i];
                break;
            }
        }
        if (!sep) {
            if (error_in_stderr==false){
                printf("Erreur : point-virgule manquant dans random().\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : point-virgule manquant dans random().\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strncpy(start, "0.00", 4);
            return buffer;
        }

        *sep = '\0'; // séparer les deux nombres
        char num1_str[128];
        char num2_str[128];
        strncpy(num1_str, range, sizeof(num1_str));
        strncpy(num2_str, sep + 1, sizeof(num2_str));

        //printf("random  n1: %s//n2: %s\n",num1_str,num2_str);

        double num1 = condition_script_order(num1_str);
        double num2 = condition_script_order(num2_str);
        //printf("result  random n1: %.2f//n2: %.2f\n",num1,num2);

        // Calcul borne min/max
        double min = num1 < num2 ? num1 : num2;
        double max = num1 > num2 ? num1 : num2;

        // Vérifier si les deux valeurs ont une partie décimale non nulle
        int has_decimal = (num1 != (int)num1) || (num2 != (int)num2);

        char result_value[128];
        if (has_decimal) {
            double rnd = ((double)rand() / RAND_MAX) * (max - min) + min;
            snprintf(result_value, sizeof(result_value), "%.4f", rnd);  // 4 décimales pour la précision
        } else {
            int rnd = rand() % ((int)max - (int)min + 1) + (int)min;
            snprintf(result_value, sizeof(result_value), "%d", rnd);
        }

        // Construire la nouvelle chaîne
        char new_expr[MAX_COMMAND_LEN];
        int prefix_len = start - buffer;
        strncpy(new_expr, buffer, prefix_len);
        new_expr[prefix_len] = '\0';

        strcat(new_expr, result_value);
        strcat(new_expr, end + 1);

        strcpy(buffer, new_expr);

        // Chercher la prochaine occurrence
        start = strstr(buffer, "random(");
    }

    //printf("random %s\n",buffer);
    return buffer;
}


// Fonction pour gérer les occurrences de random[nombre1, nombre2]
char* handle_random(char *expr, double last_result) {
    char *start = strstr(expr, "random[");

    while (start != NULL) {
        char *end = strchr(start, ']');

        if (end != NULL) {
            // Extraire la sous-expression entre les crochets
            int length = end - start - 7;  // Exclure "random[" et "]"
            if (length <= 0) {
                if (error_in_stderr==false){
                    printf("Erreur : Syntaxe incorrecte dans random[].\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : Syntaxe incorrecte dans random[].\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strncpy(start, "0.00", 4);  // Remplace par "0.00"
                return expr;
            }

            char range[length + 1];
            strncpy(range, start + 7, length);
            range[length] = '\0';  // Fin de chaîne

            // Vérifier si la virgule est présente
            char *comma = strchr(range, ',');
            if (!comma) {
                if (error_in_stderr==false){
                    printf("Erreur : Il manque une virgule dans random[].\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : Il manque une virgule dans random[].\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strncpy(start, "0.00", 4);
                return expr;
            }

            // Variables pour stocker les deux valeurs
            char num1_str[128], num2_str[128];
            int num1, num2;

            // Extraire les deux parties
            if (sscanf(range, "%49[^,],%49s", num1_str, num2_str) != 2) {
                if (error_in_stderr==false){
                    printf("Erreur : Format incorrect dans random[].\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : Format incorrect dans random[].\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
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
                if (error_in_stderr==false){
                    printf("Erreur : Valeur incorrecte dans random[].\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : Valeur incorrecte dans random[].\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
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
                if (error_in_stderr==false){
                    printf("Erreur : Valeur incorrecte dans random[].\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : Valeur incorrecte dans random[].\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
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
            if (error_in_stderr==false){
                printf("Erreur : Fermeture ']' manquante dans random[].\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Fermeture ']' manquante dans random[].\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
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
                if (error_in_stderr==false){
                    printf("Erreur : division par zéro\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : division par zéro\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
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
                    if (error_in_stderr==false){
                        printf("Erreur : division par zéro\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : division par zéro\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
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
    buffer[0] = '\0';

    char result_str[32];
    snprintf(result_str, sizeof(result_str), "%.2lf", last_result);

    // Remplacer virgules par points (selon locale)
    for (int i = 0; result_str[i]; i++) {
        if (result_str[i] == ',') result_str[i] = '.';
    }

    const char *src = command;
    char *dst = buffer;

    while (*src) {
        if (src[0] == 'r' && src[1] == 'e' && src[2] == 'p') {
            char before = (src == command) ? '\0' : src[-1];
            char after = src[3];

            if (!isalpha(before) && !isalpha(after)) {
                // Remplacement de "rep"
                strcpy(dst, result_str);
                dst += strlen(result_str);
                src += 3;
                continue;
            }
        }

        *dst++ = *src++;
    }

    *dst = '\0';
    strcpy(command, buffer);
    return command;
}

// Fonction pour sauvegarder le résultat dans un fichier
void save_result(double result) {
    FILE *file = fopen(FILENAME, "w");
    if (file == NULL) {
        if (error_in_stderr==false){
            printf("Erreur : impossible d'ouvrir le fichier pour écrire\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible d'ouvrir le fichier pour écrire\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
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
        if (error_in_stderr==false){
            printf("Erreur : impossible d'ouvrir le fichier pour lire\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible d'ouvrir le fichier pour lire\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
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

//renvoie le numéro du block script en cour d'exécution
char *handle_block_executed(const char *recup) {
    // alloue un buffer pour le résultat
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;
    result[0] = '\0';

    const char *p = recup;
    char numbuf[32];

    while (*p) {
        if (strncmp(p, "block_executed()", 16) == 0) {
            // convertir la valeur de la variable en texte
            snprintf(numbuf, sizeof(numbuf), "%d", block_execute);
            strncat(result, numbuf, MAX_COMMAND_LEN - strlen(result) - 1);
            p += 16; // avancer après "block_executed()"
        } else {
            size_t len = strlen(result);
            if (len < MAX_COMMAND_LEN - 1) {
                result[len] = *p;
                result[len + 1] = '\0';
            }
            p++;
        }
    }

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
            if (error_in_stderr==false){
                printf("Erreur : format incorrect, il faut 'triangle(base;hauteur)'\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : format incorrect, il faut 'triangle(base;hauteur)'\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
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
            if (error_in_stderr==false){
                printf("Erreur : format incorrect, il faut 'cercle(rayon)'\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : format incorrect, il faut 'cercle(rayon)'\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            return -0.01;
        }

        // Si "rep" est trouvé, remplacer par last_result
        if (strcmp(rayon_str, "rep") == 0) {
            rayon = last_result;
        } else {
            rayon = atof(rayon_str);  // Convertir le rayon en double
        }

        // Calculer l'aire du cercle ( * rayon)
        double aire = 3.14159265358979323846 * rayon * rayon;
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
            if (error_in_stderr==false){
                printf("Erreur : sur () ou ; dans modulo !\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : sur () ou ; dans modulo !\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            break;
        }

        // Extraire les deux expressions
        char expr1[MAX_COMMAND_LEN] = {0};
        char expr2[MAX_COMMAND_LEN] = {0};
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
            if (error_in_stderr==false){
                printf("Erreur : sur () ou ; dans modulo !\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : sur () ou ; dans modulo !\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            break;
        }

        // Calcul des valeurs
        double v1 = condition_script_order(expr1);
        double v2 = condition_script_order(expr2);

        if (v2 == 0) {
            strcat(result, "[ERREUR_DIV0]");
            if (error_in_stderr==false){
                printf("Erreur : division par 0 dans modulo !\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : division par 0 dans modulo !\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
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
        // Trouver la parenthèse fermante correspondante avec comptage
        char *ptr = start + 5; // après "cbrt("
        int open_parens = 1;
        while (*ptr && open_parens > 0) {
            if (*ptr == '(') open_parens++;
            else if (*ptr == ')') open_parens--;
            ptr++;
        }

        if (open_parens != 0) {
            // Parenthèses non équilibrées, sortir
            break;
        }

        char *end = ptr - 1;

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
        double sqrt_value = condition_script_order(inner_expr); //new retourne condition_script_order() plus performant que eval() + upload auto

        // Vérifier si le résultat est négatif
        if (sqrt_value < 0) {
            if (error_in_stderr==false){
                printf("Erreur : racine carrée d'un nombre négatif.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : racine carrée d'un nombre négatif.\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
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
        // Trouver la parenthèse fermante correspondante avec comptage
        char *ptr = start + 5; // après "cbrt("
        int open_parens = 1;
        while (*ptr && open_parens > 0) {
            if (*ptr == '(') open_parens++;
            else if (*ptr == ')') open_parens--;
            ptr++;
        }

        if (open_parens != 0) {
            // Parenthèses non équilibrées, sortir
            break;
        }

        char *end = ptr - 1;

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
        double sqrt_value = condition_script_order(inner_expr); //new retourne condition_script_order() plus performant que eval() + upload auto

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
        if (error_in_stderr==false){
            printf("Erreur : chemin invalide.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : chemin invalide.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    // Vérifier si la commande est interdite
    const char *banned_keywords[] = {"control ", "net ", "start cmd", "start powershell", "cmd", "powershell", "conhost.exe"};
    for (int i = 0; i < sizeof(banned_keywords) / sizeof(banned_keywords[0]); ++i) {
        if (strstr(file_path, banned_keywords[i]) != NULL) {
            if (!error_in_stderr) {
                printf("Commande bloquée pour des raisons de sécurité : %s\n", banned_keywords[i]);
                fflush(stdout);
            } else {
                fprintf(stderr, "Commande bloquée pour des raisons de sécurité : %s\n", banned_keywords[i]);
                fflush(stderr);
            }
            if (error_lock_program) {
                var_script_exit = 1;
            }
            return;
        }
    }

    // Supprimer les espaces en début, fin, et réduire les espaces consécutifs pour sécurité start
    char cleaned_path[2048];
    const char *start_ptr = file_path;

    // Ignorer les espaces au début
    while (*start_ptr && isspace((unsigned char)*start_ptr)) start_ptr++;

    // Traiter la chaîne
    size_t i = 0;
    bool in_space = false;
    for (; *start_ptr && i < sizeof(cleaned_path) - 1; start_ptr++) {
        if (isspace((unsigned char)*start_ptr)) {
            if (!in_space) {
                cleaned_path[i++] = ' ';
                in_space = true;
            }
        } else {
            cleaned_path[i++] = *start_ptr;
            in_space = false;
        }
    }

    // Supprimer l’espace final si présent
    if (i > 0 && cleaned_path[i - 1] == ' ') {
        i--;
    }
    cleaned_path[i] = '\0';

    //si start tout seul (car lance aussi le cmd)
    if (strcasecmp(cleaned_path, "start") == 0 ||
        strcasecmp(cleaned_path, "start \"\"") == 0 ||
        strcasecmp(cleaned_path, "start \" \"") == 0) {
        if (!error_in_stderr) {
            printf("Commande bloquée pour des raisons de sécurité : start cmd\n");
            fflush(stdout);
        } else {
            fprintf(stderr, "Commande bloquée pour des raisons de sécurité : start cmd\n");
            fflush(stderr);
        }
        if (error_lock_program) {
            var_script_exit = 1;
        }
        return;
    }

    // Bloquer toute commande contenant à la fois "&&" et "start"
    if ((strstr(cleaned_path, "&&") != NULL || strstr(cleaned_path, "||") != NULL)&& strstr(cleaned_path, "start") != NULL) {
        if (!error_in_stderr) {
            printf("Commande bloquée : combinaison suspecte '&&' ou '||' avec 'start' détectée (potentielle problème de sécurité).\n");
            fflush(stdout);
        } else {
            fprintf(stderr, "Commande bloquée : combinaison suspecte '&&' ou '||' avec 'start' détectée (potentielle problème de sécurité).\n");
            fflush(stderr);
        }
        if (error_lock_program) {
            var_script_exit = 1;
        }
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
        if (error_in_stderr==false){
            printf("Erreur lors de l'exécution de la commande execute.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur lors de l'exécution de la commande execute.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
    }
}

// Fonction pour extraire l'argument de la commande var="execute(chemin)"
void parse_and_execute(const char *var) {
    // Chercher "execute(" et ")" dans la chaîne
    const char *debut = strstr(var, "execute(");
    const char *fin = NULL;
    if (debut) {
        const char *ptr = debut + strlen("execute(");
        int paren_count = 1;
        while (*ptr && paren_count > 0) {
            if (*ptr == '(') paren_count++;
            else if (*ptr == ')') paren_count--;
            if (paren_count == 0) {
                fin = ptr;
                break;
            }
            ptr++;
        }
    }

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
        if (error_in_stderr==false){
            printf("Erreur : format de commande invalide sur execute().\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : format de commande invalide sur execute().\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
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
            if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées.\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
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

//fonction pour les global_bool
char* replace_state_global_bool(const char* chaine_source) {
    static char buffer[MAX_COMMAND_LEN];
    const char *pattern = "##state_global_bool##";

    const char *remplacement;
    if (var_global_bool == 1) {
        remplacement = "1";
    } else if (var_global_bool == -1) {
        remplacement = "-1";
    } else {
        remplacement = "0";
    }

    size_t len_pattern = strlen(pattern);
    size_t len_remplacement = strlen(remplacement);

    const char *src = chaine_source;
    char *dest = buffer;
    size_t max_len = MAX_COMMAND_LEN - 1;

    while (*src && (dest - buffer + len_remplacement < max_len)) {
        const char *pos = strstr(src, pattern);
        if (!pos) {
            strncpy(dest, src, max_len - (dest - buffer));
            dest += strlen(dest);
            break;
        }

        size_t len_avant = pos - src;
        if (dest - buffer + len_avant + len_remplacement >= max_len) {
            break; // éviter le dépassement
        }

        memcpy(dest, src, len_avant);
        dest += len_avant;
        memcpy(dest, remplacement, len_remplacement);
        dest += len_remplacement;
        src = pos + len_pattern;
    }

    *dest = '\0';
    return buffer;
}

//fonction pour les etats de use_script_path ##state_table_use_script_path## 1 ou 0 en fonction de son etat
char* replace_state_table_use_script_path(const char* chaine_source) {
    static char buffer[MAX_COMMAND_LEN];
    const char *pattern = "##state_table_use_script_path##";
    const char *remplacement = (use_script_path ? "1" : "0");
    size_t len_pattern = strlen(pattern);
    size_t len_remplacement = strlen(remplacement);

    const char *src = chaine_source;
    char *dest = buffer;
    size_t max_len = MAX_COMMAND_LEN - 1;

    while (*src && (dest - buffer + len_remplacement < max_len)) {
        const char *pos = strstr(src, pattern);
        if (!pos) {
            strncpy(dest, src, max_len - (dest - buffer));
            dest += strlen(dest);
            break;
        }

        size_t len_avant = pos - src;
        if (dest - buffer + len_avant + len_remplacement >= max_len) {
            break; // ne pas dépasser la taille
        }

        memcpy(dest, src, len_avant);
        dest += len_avant;
        memcpy(dest, remplacement, len_remplacement);
        dest += len_remplacement;
        src = pos + len_pattern;
    }

    *dest = '\0';
    return buffer;
}

//text_replace("arg";"arg";"arg")
void replace_all(char *str, const char *search, const char *replace) {
    char buffer[MAX_COMMAND_LEN];
    char *pos, *last_pos = str;
    buffer[0] = '\0';

    size_t search_len = strlen(search);
    size_t replace_len = strlen(replace);

    while ((pos = strstr(last_pos, search)) != NULL) {
        // Copier tout avant search
        strncat(buffer, last_pos, pos - last_pos);
        // Ajouter replace
        strcat(buffer, replace);
        last_pos = pos + search_len;
    }
    // Ajouter le reste
    strcat(buffer, last_pos);
    // Copier le résultat dans str
    strncpy(str, buffer, MAX_COMMAND_LEN - 1);
    str[MAX_COMMAND_LEN - 1] = '\0';
}

//texte eval pour parenthèse imbriqué
char* find_unescaped_separator(char *start, const char *sep) {
    int in_quote = 0;
    int parens = 0;

    char *p = start;
    size_t sep_len = strlen(sep);

    while (*p) {
        if (*p == '\\') {
            p += 2; // skip escaped character
            continue;
        }



            if (*p == '(') {
                parens++;
            } else if (*p == ')') {
                if (parens == 0) {
                    // erreur ici : parenthèse fermante sans ouvrante
                    return NULL;
                }
                parens--;
            } else if (parens == 0 && strncmp(p, sep, sep_len) == 0) {
                return p;  // trouvé hors parenthèses et hors string
            }


        p++;
    }

    // parenthèses mal fermées ?
    if (parens > 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées sur text_eval.\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées sur text_eval.\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return NULL;
    }

    return NULL;  // non trouvé
}

//text_eval("arg";operateur;"arg")
char* handle_text_eval(char *recup) {
    static char buffer[4096];
    char temp[2048];
    char *p = recup;
    buffer[0] = '\0';

    //printf("text_eval: %s\n",p);

    while (*p) {
        char *start = strstr(p, "text_eval(\"");
        if (!start) {
            strcat(buffer, p);
            break;
        }

        // copier avant text_eval
        strncat(buffer, p, start - p);

        char *arg1_start = start + strlen("text_eval(\"");

        // chercher premier '";' fin de arg1
        char *arg1_end = find_unescaped_separator(arg1_start, "\";");
        if (!arg1_end) goto skip;
        *arg1_end = '\0';

        char *op_start = arg1_end + 2; // après '";'

        // chercher prochain ';\"' fin de opérateur
        char *op_end = find_unescaped_separator(op_start, ";\"");
        if (!op_end) goto skip;
        *op_end = '\0';

        char *arg2_start = op_end + 2; // après ';\"'

        // chercher '")' qui termine le pattern
        char *end = find_unescaped_separator(arg2_start, "\")");
        if (!end) goto skip;
        *end = '\0';

        if (strstr(arg1_start,"num(") != NULL && var_activ_func_num==true) {
            strcpy(arg1_start, auto_num(arg1_start));
        }

        if (strstr(arg2_start,"num(") != NULL && var_activ_func_num==true) {
            strcpy(arg2_start, auto_num(arg2_start));
        }

        char *arg1_processed = NULL;
        char *arg2_processed = NULL;

        //auto_text
        if (var_activ_func_text) {
            if (strstr(arg1_start,"text(")!=NULL){
                arg1_processed = auto_text(arg1_start);
                if (!arg1_processed) break;
            } else {
                arg1_processed = strdup(arg1_start);
                if (!arg1_processed) break;
            }

            if (strstr(arg2_start,"text(")!=NULL){
                arg2_processed = auto_text(arg2_start);
                if (!arg2_processed) {
                    free(arg1_processed);
                    break;
                }
            } else {
                arg2_processed = strdup(arg2_start);
                if (!arg2_processed) {
                    free(arg1_processed);
                    break;
                }
            }
        } else {
            arg1_processed = strdup(arg1_start);  // Copier tel quel
            arg2_processed = strdup(arg2_start);
            if (!arg1_processed || !arg2_processed) {
                free(arg1_processed);
                free(arg2_processed);
                break;
            }
        }

        // maintenant arg1_start, op_start et arg2_start sont clean
        // faire la comparaison
        int result = 0;
        if (strcmp(op_start, "?=") == 0) {
            result = (strcmp(arg1_processed, arg2_processed) == 0) ? 1 : 0;
        } else if (strcmp(op_start, "!=") == 0) {
            result = (strcmp(arg1_processed, arg2_processed) != 0) ? 1 : 0;
        } else if (strcmp(op_start, "/=") == 0) {
            result = (strncmp(arg1_processed, arg2_processed, strlen(arg2_processed)) == 0) ? 1 : 0;
        } else if (strcmp(op_start, "\\=") == 0) {
            size_t len1 = strlen(arg1_processed);
            size_t len2 = strlen(arg2_processed);
            result = (len1 >= len2 && strcmp(arg1_processed + len1 - len2, arg2_processed) == 0) ? 1 : 0;
        } else if (strcmp(op_start, "#=") == 0) {
            result = (strstr(arg1_processed, arg2_processed) != NULL) ? 1 : 0;
        } else if (strcmp(op_start, "!#=") == 0) {
            result = (strstr(arg1_processed, arg2_processed) == NULL) ? 1 : 0;
        } else {
            free(arg1_processed);
            free(arg2_processed);
            goto skip;
        }

        // ajouter résultat
        char res_str[8];
        snprintf(res_str, sizeof(res_str), "%d", result);
        strcat(buffer, res_str);

        // Libérer la mémoire allouée par auto_text
        free(arg1_processed);
        free(arg2_processed);

        // avancer après le pattern
        p = end + 2; // sauter '")'
        continue;

    skip:
        // si erreur, copier tel quel et avancer
        strncat(buffer, start, 10); // copier 'text_eval('
        p = start + 10;
    }

    return buffer;
}

//text_replace("arg";"arg";"arg")
char *handle_text_replace(char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    char temp[MAX_COMMAND_LEN];
    char *start = recup;
    char *pos;

    result[0] = '\0';

    while ((pos = strstr(start, "text_replace(\"")) != NULL) {
        // Copier tout avant text_replace
        strncat(result, start, pos - start);

        // Avancer après text_replace("
        char *arg_start = pos + strlen("text_replace(\"");

        // Chercher fin de arg1
        char *arg1_end = arg_start;
        int paren_level = 0;
        while (*arg1_end) {
            if (*arg1_end == '(') paren_level++;
            else if (*arg1_end == ')') paren_level--;
            else if (paren_level == 0 && arg1_end[0] == '"' && arg1_end[1] == ';') {
                break;
            }
            arg1_end++;
        }
        if (!*arg1_end) break; // pas trouvé

        char arg1[1024];
        strncpy(arg1, arg_start, arg1_end - arg_start);
        arg1[arg1_end - arg_start] = '\0';

        // Chercher début arg2
        char *arg2_start = arg1_end + 2; // passer "\";"
        if (*arg2_start != '\"') break;
        arg2_start++;

        // Chercher fin arg2
        char *arg2_end = arg2_start;
        paren_level = 0;
        while (*arg2_end) {
            if (*arg2_end == '(') paren_level++;
            else if (*arg2_end == ')') paren_level--;
            else if (paren_level == 0 && arg2_end[0] == '"' && arg2_end[1] == ';') {
                break;
            }
            arg2_end++;
        }
        if (!*arg2_end) break; // pas trouvé

        char arg2[1024];
        strncpy(arg2, arg2_start, arg2_end - arg2_start);
        arg2[arg2_end - arg2_start] = '\0';

        // Chercher début arg3
        char *arg3_start = arg2_end + 2; // passer "\";"
        if (*arg3_start != '\"') break;
        arg3_start++;

        // Chercher fin arg3
        char *arg3_end = arg3_start;
        paren_level = 0;
        while (*arg3_end) {
            if (*arg3_end == '(') paren_level++;
            else if (*arg3_end == ')') paren_level--;
            else if (paren_level == 0 && arg3_end[0] == '"' && arg3_end[1] == ')') {
                break;
            }
            arg3_end++;
        }
        if (!*arg3_end) break; // pas trouvé

        char arg3[1024];
        strncpy(arg3, arg3_start, arg3_end - arg3_start);
        arg3[arg3_end - arg3_start] = '\0';

        // Chercher fin de l'appel text_replace(...) : il doit y avoir un ')'
        char *close_paren = strchr(arg3_end, ')');
        if (!close_paren) break;

        if (strstr(arg1,"num(") != NULL && var_activ_func_num==true) {
            strcpy(arg1, auto_num(arg1));
        }

        if (strstr(arg2,"num(") != NULL && var_activ_func_num==true) {
            strcpy(arg2, auto_num(arg2));
        }

        if (strstr(arg3,"num(") != NULL && var_activ_func_num==true) {
            strcpy(arg3, auto_num(arg3));
        }

        if (strstr(arg1,"text(") != NULL && var_activ_func_text==true) {
            char temp_buf[1024];
            strncpy(temp_buf, arg1, 1024);
            temp_buf[1024 - 1] = '\0';

            char* temp_result = auto_text(temp_buf);
            if (temp_result) {
                strncpy(arg1, temp_result, 1024);
                arg1[1024 - 1] = '\0';
                free(temp_result);
            }
        }

        if (strstr(arg2,"text(") != NULL && var_activ_func_text==true) {
            char temp_buf[1024];
            strncpy(temp_buf, arg2, 1024);
            temp_buf[1024 - 1] = '\0';

            char* temp_result = auto_text(temp_buf);
            if (temp_result) {
                strncpy(arg2, temp_result, 1024);
                arg2[1024 - 1] = '\0';
                free(temp_result);
            }
        }

        if (strstr(arg3,"text(") != NULL && var_activ_func_text==true) {
            char temp_buf[1024];
            strncpy(temp_buf, arg3, 1024);
            temp_buf[1024 - 1] = '\0';

            char* temp_result = auto_text(temp_buf);
            if (temp_result) {
                strncpy(arg3, temp_result, 1024);
                arg3[1024 - 1] = '\0';
                free(temp_result);
            }
        }

        //printf("arg1 : %s\n",arg1);
        //printf("arg2 : %s\n",arg2);
        //printf("arg3 : %s\n",arg3);

        // Remplacer toutes les occurrences de arg2 par arg3 dans arg1
        strncpy(temp, arg1, sizeof(temp)-1);
        temp[sizeof(temp)-1] = '\0';

        replace_all(temp, arg2, arg3);

        // Ajouter le texte transformé
        strcat(result, temp);

        // Passer à la suite
        start = close_paren + 1;
    }

    // Ajouter le reste
    strcat(result, start);

    return result;
}

//pour text_cut("";0;0)
char* handle_text_cut(char *recup) {
    static char output[MAX_COMMAND_LEN * 2];
    strcpy(output, "");

    char *src = recup;
    while (*src) {
        char *cut_start = strstr(src, "text_cut(\"");
        if (!cut_start) {
            strcat(output, src);
            break;
        }

        // Copier tout ce qui précède text_cut
        strncat(output, src, cut_start - src);

        // Position après "text_cut(\""
        char *arg_start = cut_start + strlen("text_cut(\"");

        // Extraire le texte à couper (jusqu’à la fermeture du ")
        char *text_end = strchr(arg_start, '"');
        if (!text_end) {
            strncat(output, cut_start,1); // Erreur : syntaxe incomplète, copier tel quel
            src = cut_start + 1;
            continue;
        }

        int text_len = text_end - arg_start;
        char target[1024] = {0};
        strncpy(target, arg_start, text_len);

        // Poursuivre après le guillemet
        char *param_start = text_end + 1;
        if (*param_start != ';') {
            strncat(output, cut_start,1); // Erreur de syntaxe
            src = cut_start + 1;
            continue;
        }
        param_start++;

        // Extraire index1 et index2 avec respect des parenthèses
        char expr1[256] = {0}, expr2[256] = {0};
        int i = 0, paren = 0;
        while (*param_start && (*param_start != ';' || paren > 0)) {
            if (*param_start == '(') paren++;
            if (*param_start == ')') paren--;
            expr1[i++] = *param_start++;
        }
        expr1[i] = '\0';
        if (*param_start != ';') {
            strncat(output, cut_start,1); // Syntaxe invalide
            src = cut_start + 1;
            continue;
        }
        param_start++; // skip ';'

        i = 0; paren = 0;
        while (*param_start && *param_start != ')') {
            if (*param_start == '(') paren++;
            if (*param_start == ')') paren--;
            expr2[i++] = *param_start++;
        }
        expr2[i] = '\0';

        if (*param_start != ')') {
            strncat(output, cut_start,1); // parenthèse fermante manquante
            src = cut_start + 1;
            continue;
        }
        param_start++; // skip ')'

        // Évaluer les index
        int idx1 = (int)condition_script_order(expr1);
        int idx2 = (int)condition_script_order(expr2);

        if (strstr(target,"text(") != NULL && var_activ_func_text==true) {
            char temp_buf[1024];
            strncpy(temp_buf, target, 1024);
            temp_buf[1024 - 1] = '\0';

            char* temp_result = auto_text(temp_buf);
            if (temp_result) {
                strncpy(target, temp_result, 1024);
                target[1024 - 1] = '\0';
                free(temp_result);
            }
        }

        if (strstr(target,"num(") != NULL && var_activ_func_num==true) {
            strcpy(target, auto_num(target));
        }

        int len = strlen(target);
        int pos1 = (idx1 > 0) ? idx1 - 1 : (idx1 < 0) ? len + idx1 : -1;
        int pos2 = (idx2 > 0) ? idx2 - 1 : (idx2 < 0) ? len + idx2 : -1;

        if (pos1 < 0 || pos2 < 0 || pos1 >= len || pos2 >= len) {
            if (error_in_stderr==false){
                printf("Erreur d'index sur text_cut("";index;index) : valeur null ou suppérieur à la taille du texte)\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur d'index sur text_cut("";index;index) : valeur null ou suppérieur à la taille du texte)\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
                break;
            } else {
                //strncat(output, cut_start,1); // Erreur d’index
                //src = cut_start + 1;
                strcat(output, "0"); //mieux 0, montre l'erreur
                continue;
            }
        }

        // Supprimer du plus petit au plus grand index
        int from = (pos1 < pos2) ? pos1 : pos2;
        int to = (pos1 > pos2) ? pos1 : pos2;

        char modified[1024] = {0};
        strncat(modified, target, from);
        strcat(modified, target + to + 1);

        strcat(output, modified);

        // Continuer après l’appel text_cut(...)
        src = param_start;
    }

    return output;
}

char* handle_text_len(char *recup) {
    static char buffer[MAX_COMMAND_LEN];
    char *src = recup;
    char *dst = buffer;

    while (*src && (dst - buffer) < MAX_COMMAND_LEN - 1) {
        char *pos = strstr(src, "text_len(\"");
        if (!pos) {
            // Copie le reste
            strncpy(dst, src, MAX_COMMAND_LEN - (dst - buffer) - 1);
            break;
        }

        // Copier ce qui est avant text_len(
        size_t before_len = pos - src;
        if (before_len > (size_t)(MAX_COMMAND_LEN - (dst - buffer) - 1))
            before_len = MAX_COMMAND_LEN - (dst - buffer) - 1;
        memcpy(dst, src, before_len);
        dst += before_len;

        // Avancer après text_len("
        pos += 10; // longueur de text_len("
        char *start = pos;

        int parentheses_level = 0;
        bool quotes_end = false; // cherché la fermeture de text_len("")
        while (*pos) {
            if (*pos == '"' && *(pos + 1) == ')' && parentheses_level == 0) {
                // Fin des guillemets principaux
                pos++;
                quotes_end = true;
                break;
            } else if (*pos == '(') {
                parentheses_level++;
            } else if (*pos == ')') {
                if (parentheses_level > 0)
                    parentheses_level--;
            }
            pos++;
        }

        if (!quotes_end){
            if (error_in_stderr==false){
                printf("Erreur de syntaxe sur text_len()\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur de syntaxe sur text_len()\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            //écrire 0
            memcpy(dst, "0", strlen("0"));
            dst += strlen("0");
        } else {
            // Extraire le texte interne
            size_t inner_len_raw = (size_t)(pos - 1 - start);
            char inner_buf[MAX_COMMAND_LEN];
            if (inner_len_raw >= MAX_COMMAND_LEN)
                inner_len_raw = MAX_COMMAND_LEN - 1;
            memcpy(inner_buf, start, inner_len_raw);
            inner_buf[inner_len_raw] = '\0';

            // Appliquer text() si présent
            if (strstr(inner_buf, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, inner_buf, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(inner_buf, temp_result, MAX_COMMAND_LEN);
                    inner_buf[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent
            if (strstr(inner_buf, "num(") != NULL && var_activ_func_num == true) {
                strcpy(inner_buf, auto_num(inner_buf));
            }

            // Calculer longueur interne
            size_t inner_len = strlen(inner_buf);
            // Écrire la longueur en texte
            char numbuf[32];
            snprintf(numbuf, sizeof(numbuf), "%zu", inner_len);

            size_t numlen = strlen(numbuf);
            if (numlen > (size_t)(MAX_COMMAND_LEN - (dst - buffer) - 1))
                numlen = MAX_COMMAND_LEN - (dst - buffer) - 1;
            memcpy(dst, numbuf, numlen);
            dst += numlen;
        }

        // Sauter jusqu'à la parenthèse fermante de text_len(...)
        while (*pos && *pos != ')') pos++;
        if (*pos == ')') pos++; // sauter la parenthèse

        src = pos;
    }

    *dst = '\0';
    return buffer;
}

//pour récupérer une lettre d'un mot text_letter("";)
char* handle_text_letter(char *recup) {
    char *output = malloc(MAX_COMMAND_LEN);  // buffer de sortie
    char *p = recup;
    char *out = output;
    *out = '\0';  // vider

    while (*p) {
        // Chercher "text_letter("
        if (strncmp(p, "text_letter(", 12) == 0) {
            p += 12; // avancer après "text_letter("

            // Extraire arg1 entre guillemets
            if (*p != '"') {
                strcat(out, "0");  // erreur
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le premier "
            char arg1[MAX_COMMAND_LEN];
            int i = 0;
            int paren = 0; // compteur de parenthèses

            while (*p && (paren > 0 || *p != '"') && i < MAX_COMMAND_LEN - 1) {
                if (*p == '(') {
                    paren++;
                    arg1[i++] = *p++;
                } else if (*p == ')') {
                    if (paren > 0) paren--;
                    arg1[i++] = *p++;
                } else {
                    arg1[i++] = *p++;
                }
            }
            arg1[i] = '\0';

            if (*p != '"') {
                strcat(out, "0"); // guillemet manquant
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le guillemet fermant

            // Vérifier séparateur ;
            if (*p != ';') {
                strcat(out, "0"); // erreur
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le ;

            // Appliquer text() si présent sur arg1
            if (strstr(arg1, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg1, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg1, temp_result, MAX_COMMAND_LEN);
                    arg1[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg1
            if (strstr(arg1, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg1));
                strcpy(arg1, tmp);
            }

            // Extraire arg2 jusqu'à ')'
            char arg2[MAX_COMMAND_LEN];
            i = 0;
            paren = 0;

            while (*p) {
                if (*p == '(') {
                    paren++;
                    arg2[i++] = *p++;
                } else if (*p == ')') {
                    if (paren == 0) {
                        // c'est la vraie fin de text_letter()
                        break;
                    } else {
                        if (paren > 0){
                            paren--;
                        }
                        arg2[i++] = *p++;
                    }
                } else {
                    arg2[i++] = *p++;
                }

                if (i >= MAX_COMMAND_LEN - 1) break;
            }

            arg2[i] = '\0'; //finir l'arg2 proprement sinon erreur entre appel positif et négatif

            if (*p == ')') p++; // passer le )

            // Calculer la position
            double pos = condition_script_order(arg2);
            int len = strlen(arg1);
            int index = 0;

            if (pos > 0) index = (int)pos - 1;       // partir du début
            else if (pos < 0) index = len + (int)pos; // depuis la fin
            else index = -1; // interdit zéro

            // Vérifier limites
            if (index < 0 || index >= len) {
                strcat(out, "0");
                if (error_in_stderr==false){
                    printf("Erreur : l'index est trop grand ou null sur text_letter(\"arg\";index)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : l'index est trop grand ou null sur text_letter(\"arg\";index)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            } else {
                int l = strlen(out);
                out[l] = arg1[index]; // ajouter la lettre
                out[l+1] = '\0';
            }

        } else {
            // Copier caractère normal
            int l = strlen(output);
            output[l] = *p;
            output[l+1] = '\0';
            p++;
        }
    }

    return output;
}

//pour mettre tout le text contenu en majuscule text_uppercase("")
char* handle_text_uppercase(char *recup) {
    char *output = malloc(MAX_COMMAND_LEN);
    char *p = recup;
    char *out = output;
    *out = '\0';

    while (*p) {
        if (strncmp(p, "text_uppercase(", 15) == 0) {
            p += 15; // avancer après "text_uppercase("

            // On attend un guillemet ouvrant
            if (*p != '"') {
                strcat(out, "0"); // erreur de syntaxe
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le premier "

            char arg[MAX_COMMAND_LEN];
            int i = 0;
            int paren = 0;

            while (*p && (paren > 0 || *p != '"') && i < MAX_COMMAND_LEN-1) {
                if (*p == '(') {
                    paren++;
                    arg[i++] = *p++;
                } else if (*p == ')') {
                    if (paren > 0) paren--;
                    arg[i++] = *p++;
                } else {
                    arg[i++] = *p++;
                }
            }
            arg[i] = '\0';

            if (*p != '"') {
                strcat(out, "0"); // guillemet manquant
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le guillemet fermant

            if (*p == ')') {
                p++; // passer la parenthèse fermante
            } else {
                // erreur de syntaxe, pas de ')'
                strcat(out, "0");
                continue;
            }

            // Appliquer text() si présent sur arg
            if (strstr(arg, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg, temp_result, MAX_COMMAND_LEN);
                    arg[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg
            if (strstr(arg, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg));
                strcpy(arg, tmp);
            }


            // Transformer en majuscule
            for (int j = 0; arg[j]; j++) {
                arg[j] = (char)toupper((unsigned char)arg[j]);
            }

            // Ajouter le résultat
            strncat(out, arg, MAX_COMMAND_LEN - strlen(out) - 1);

        } else {
            // Copier caractère normal
            int l = strlen(out);
            if (l < MAX_COMMAND_LEN - 1) {
                out[l] = *p;
                out[l+1] = '\0';
            }
            p++;
        }
    }

    return output;
}


//pour mettre tout le text contenu en minuscule text_lowercase("")
char* handle_text_lowercase(char *recup) {
    char *output = malloc(MAX_COMMAND_LEN);
    char *p = recup;
    char *out = output;
    *out = '\0';

    while (*p) {
        if (strncmp(p, "text_lowercase(", 15) == 0) {
            p += 15; // avancer après "text_uppercase("

            // On attend un guillemet ouvrant
            if (*p != '"') {
                strcat(out, "0"); // erreur de syntaxe
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le premier "

            char arg[MAX_COMMAND_LEN];
            int i = 0;
            int paren = 0;

            while (*p && (paren > 0 || *p != '"') && i < MAX_COMMAND_LEN-1) {
                if (*p == '(') {
                    paren++;
                    arg[i++] = *p++;
                } else if (*p == ')') {
                    if (paren > 0) paren--;
                    arg[i++] = *p++;
                } else {
                    arg[i++] = *p++;
                }
            }
            arg[i] = '\0';

            if (*p != '"') {
                strcat(out, "0"); // guillemet manquant
                while (*p && *p != ')') p++;
                if (*p == ')') p++;
                continue;
            }
            p++; // passer le guillemet fermant

            if (*p == ')') {
                p++; // passer la parenthèse fermante
            } else {
                // erreur de syntaxe, pas de ')'
                strcat(out, "0");
                continue;
            }

            // Appliquer text() si présent sur arg
            if (strstr(arg, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg, temp_result, MAX_COMMAND_LEN);
                    arg[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg
            if (strstr(arg, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg));
                strcpy(arg, tmp);
            }


            // Transformer en majuscule
            for (int j = 0; arg[j]; j++) {
                arg[j] = (char)tolower((unsigned char)arg[j]);
            }

            // Ajouter le résultat
            strncat(out, arg, MAX_COMMAND_LEN - strlen(out) - 1);

        } else {
            // Copier caractère normal
            int l = strlen(out);
            if (l < MAX_COMMAND_LEN - 1) {
                out[l] = *p;
                out[l+1] = '\0';
            }
            p++;
        }
    }

    return output;
}

//cherché des mot text_word("arg";index)
char *handle_text_word(char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;
    result[0] = '\0';

    char *p = recup;
    while (*p) {
        if (strncmp(p, "text_word(", 10) == 0) {
            p += 10; // avancer après "text_word("

            // ----- extraire arg -----
            if (*p != '"') {
                strcat(result, "0");
                continue;
            }
            p++; // sauter le "

            char arg[MAX_COMMAND_LEN];
            int i = 0, paren = 0;
            while (*p && (paren > 0 || *p != '"') && i < MAX_COMMAND_LEN-1) {
                if (*p == '(') paren++;
                else if (*p == ')') {
                    if (paren > 0) paren--;
                }
                arg[i++] = *p++;
            }
            arg[i] = '\0';

            if (*p != '"') { // erreur syntaxe
                strcat(result, "0");
                continue;
            }
            p++; // passer le "

            if (*p != ';') { // il faut un ;
                strcat(result, "0");
                continue;
            }
            p++; // sauter le ;

            // ----- extraire index -----
            char idx_buf[64];
            i = 0;
            paren = 0;
            while (*p && (paren > 0 || *p != ')') && i < (int)sizeof(idx_buf)-1) {
                if (*p == '(') paren++;
                else if (*p == ')') {
                    if (paren > 0) paren--;
                }
                idx_buf[i++] = *p++;
            }
            idx_buf[i] = '\0';

            if (*p != ')') { // parenthèse fermante manquante
                strcat(result, "0");
                continue;
            }
            p++; // passer le )

            // ----- interprétation de l’index -----
            if (strlen(idx_buf) == 0) {
                if (error_in_stderr==false){
                    printf("Erreur: index null dans text_word(\"arg\";index)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: index null dans text_word(\"arg\";index)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcat(result, "0");
                continue;
            }

            int index = 0;
            if (strlen(idx_buf) != 0) {
                double index_in_double = condition_script_order(idx_buf);
                index = (int)index_in_double;
            }
            if (index == 0) {
                if (error_in_stderr==false){
                    printf("Erreur: index null dans text_word(\"arg\";index)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: index null dans text_word(\"arg\";index)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                // pas un entier valide
                strcat(result, "0");
                continue;
            }

            // Appliquer text() si présent sur arg
            if (strstr(arg, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg, temp_result, MAX_COMMAND_LEN);
                    arg[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg
            if (strstr(arg, "num(") != NULL && var_activ_func_num == true) {
                char tmp_[MAX_COMMAND_LEN];
                strcpy(tmp_, auto_num(arg));
                strcpy(arg, tmp_);
            }

            // ----- découper arg en mots -----
            char *words[MAX_COMMAND_LEN];
            int word_count = 0;

            char *tmp = strdup(arg);
            char *tok = strtok(tmp, " ");
            while (tok && word_count < (int)(MAX_COMMAND_LEN)) {
                words[word_count++] = tok;
                tok = strtok(NULL, " ");
            }

            const char *replacement = "0";
            if (word_count > 0) {
                if (index > 0) { // depuis le début
                    if (index <= word_count) {
                        replacement = words[index-1];
                    }
                } else if (index < 0) { // depuis la fin
                    int pos = word_count + index; // ex: -1 => dernier
                    if (pos >= 0 && pos < word_count) {
                        replacement = words[pos];
                    }
                }
            }

            strncat(result, replacement, MAX_COMMAND_LEN - strlen(result) - 1);

            free(tmp);

        } else {
            // recopier caractère normal
            size_t len = strlen(result);
            if (len < MAX_COMMAND_LEN-1) {
                result[len] = *p;
                result[len+1] = '\0';
            }
            p++;
        }
    }

    return result; // malloc => doit être free() après usage
}


// Compter le nombre de fois que 'needle' (aiguille) est dans 'haystack' (botte de foin) pour text_count("";"")
static int text_count_occurrences(const char *haystack, const char *needle) {
    if (!*needle) return 0; // needle vide = 0 occurrences
    int count = 0;
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += strlen(needle);
    }
    return count;
}

//text_count("arg1";"arg2") affiche le nombre de fois que arg2 est présent dans arg1
char *handle_text_count(char *recup) {
    static char output[MAX_COMMAND_LEN]; // buffer de sortie (taille ajustable)
    char *p = recup;
    char *out = output;
    *out = '\0';

    while (*p) {
        if (strncmp(p, "text_count(", 11) == 0) {
            p += 11; // avancer après "text_count("

            // ----- extraire arg1 -----
            if (*p != '"') {
                // erreur syntaxe
                strcat(output, "0");
                return output;
            }
            p++; // sauter le "

            char arg1[MAX_COMMAND_LEN];
            int i = 0, paren = 0;
            while (*p && (paren > 0 || *p != '"') && i < (int)sizeof(arg1)-1) {
                if (*p == '(') paren++;
                if (*p == ')') { if (paren > 0) paren--; }
                arg1[i++] = *p++;
            }
            arg1[i] = '\0';

            if (*p != '"') { // pas de guillemet fermant
                strcat(output, "0");
                return output;
            }
            p++; // passer le "

            if (*p != ';') { // après arg1, on attend ";"
                strcat(output, "0");
                return output;
            }
            p++; // sauter le ;

            // ----- extraire arg2 -----
            if (*p != '"') {
                strcat(output, "0");
                return output;
            }
            p++; // sauter le "

            char arg2[MAX_COMMAND_LEN];
            i = 0; paren = 0;
            while (*p && (paren > 0 || *p != '"') && i < (int)sizeof(arg2)-1) {
                if (*p == '(') paren++;
                if (*p == ')') { if (paren > 0) paren--; }
                arg2[i++] = *p++;
            }
            arg2[i] = '\0';

            if (*p != '"') { // pas de guillemet fermant
                strcat(output, "0");
                return output;
            }
            p++; // passer le "

            if (*p != ')') { // on attend la parenthèse fermante
                strcat(output, "0");
                return output;
            }
            p++; // sauter le )

            // Appliquer text() si présent sur arg1
            if (strstr(arg1, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg1, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg1, temp_result, MAX_COMMAND_LEN);
                    arg1[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg1
            if (strstr(arg1, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg1));
                strcpy(arg1, tmp);
            }

            // Appliquer text() si présent sur arg2
            if (strstr(arg2, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg2, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg2, temp_result, MAX_COMMAND_LEN);
                    arg2[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg2
            if (strstr(arg2, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg2));
                strcpy(arg2, tmp);
            }

            // ----- appliquer la logique -----
            int nb = text_count_occurrences(arg1, arg2);

            char buf[256];
            snprintf(buf, sizeof(buf), "%d", nb);
            strncat(out, buf, sizeof(output)-strlen(out)-1);

        } else {
            // recopier caractère normal
            size_t len = strlen(out);
            if (len < sizeof(output)-1) {
                out[len] = *p;
                out[len+1] = '\0';
            }
            p++;
        }
    }

    return output;
}

//text_find("arg1";"arg2";index) renvoie la position de arg2 dans arg1 en fonction de index
char *handle_text_find(char *recup) {
    //char *result = malloc(MAX_COMMAND_LEN);
    //if (!result) return NULL;
    static char result[MAX_COMMAND_LEN];

    result[0] = '\0';

    char *start = recup;
    char *pos;

    while ((pos = strstr(start, "text_find(\"")) != NULL) {
        // Copier tout avant text_find
        strncat(result, start, pos - start);

        // Avancer après text_find("
        char *arg1_start = pos + strlen("text_find(\"");

        // --- Trouver fin de arg1 ---
        char *arg1_end = arg1_start;
        int paren_level = 0;
        while (*arg1_end) {
            if (*arg1_end == '(') paren_level++;
            else if (*arg1_end == ')') paren_level--;
            else if (paren_level == 0 && arg1_end[0] == '"' && arg1_end[1] == ';') {
                break;
            }
            arg1_end++;
        }
        if (!*arg1_end) break;

        char arg1[1024];
        strncpy(arg1, arg1_start, arg1_end - arg1_start);
        arg1[arg1_end - arg1_start] = '\0';

        // --- Trouver début arg2 ---
        char *arg2_start = arg1_end + 2; // passer "\";"
        if (*arg2_start != '\"') break;
        arg2_start++;

        // --- Trouver fin arg2 ---
        char *arg2_end = arg2_start;
        paren_level = 0;
        while (*arg2_end) {
            if (*arg2_end == '(') paren_level++;
            else if (*arg2_end == ')') paren_level--;
            else if (paren_level == 0 && arg2_end[0] == '"' && arg2_end[1] == ';') {
                break;
            }
            arg2_end++;
        }
        if (!*arg2_end) break;

        char arg2[1024];
        strncpy(arg2, arg2_start, arg2_end - arg2_start);
        arg2[arg2_end - arg2_start] = '\0';

        // --- Trouver début arg3 (index) ---
        char *arg3_start = arg2_end + 2; // passer "\";"
        char *arg3_end = arg3_start;
        paren_level = 0;
        while (*arg3_end) {
            if (*arg3_end == '(') paren_level++;
            else if (*arg3_end == ')') {
                if (paren_level == 0) break;  // fin réelle de l'index
                paren_level--;
            }
            arg3_end++;
        }
        if (!*arg3_end) break;

        char arg3[64];
        strncpy(arg3, arg3_start, arg3_end - arg3_start);
        arg3[arg3_end - arg3_start] = '\0';

        // Conversion index
        int index = 0;
        if (strlen(arg3) != 0) {
            double index_in_double = condition_script_order(arg3);
            index = (int)index_in_double;
        }

        char replace_buf[64];
        replace_buf[0] = '\0';

        if (index == 0) {
            strcpy(replace_buf, "0");
            if (error_in_stderr==false){
                printf("Erreur : l'index est null sur text_find(\"arg1\";\"arg2\";index)\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : l'index est null sur text_find(\"arg1\";\"arg2\";index)\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
        } else {
            // Appliquer text() si présent sur arg1
            if (strstr(arg1, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg1, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg1, temp_result, MAX_COMMAND_LEN);
                    arg1[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg1
            if (strstr(arg1, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg1));
                strcpy(arg1, tmp);
            }

            // Appliquer text() si présent sur arg2
            if (strstr(arg2, "text(") != NULL && var_activ_func_text == true) {
                char temp_buf[MAX_COMMAND_LEN];
                strncpy(temp_buf, arg2, MAX_COMMAND_LEN);
                temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                char* temp_result = auto_text(temp_buf);
                if (temp_result) {
                    strncpy(arg2, temp_result, MAX_COMMAND_LEN);
                    arg2[MAX_COMMAND_LEN - 1] = '\0';
                    free(temp_result);
                }
            }

            // Appliquer num() si présent sur arg2
            if (strstr(arg2, "num(") != NULL && var_activ_func_num == true) {
                char tmp[MAX_COMMAND_LEN];
                strcpy(tmp, auto_num(arg2));
                strcpy(arg2, tmp);
            }

            // Recherche
            int found_pos = 0; // position trouvée (1-based)
            int count = 0;
            size_t len1 = strlen(arg1);
            size_t len2 = strlen(arg2);

            //printf("DEBUG: arg1='%s' (len=%zu), arg2='%s' (len=%zu)\n", arg1, len1, arg2, len2);

            if (len2 == 0 || len1 == 0) {
                strcpy(replace_buf, "0");
                if (error_in_stderr==false){
                    printf("Erreur : au moins un argument texte est null sur text_find(\"arg1\";\"arg2\";index)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : au moins un argument texte est null sur text_find(\"arg1\";\"arg2\";index)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            } else {
                if (index > 0) {
                    // recherche depuis début
                    for (size_t i = 0; i + len2 <= len1; i++) {
                        if (strncmp(arg1 + i, arg2, len2) == 0) {
                            count++;
                            if (count == index) {
                                found_pos = (int)i + 1; // position en 1-based
                                break;
                            }
                        }
                    }
                } else {
                    // recherche depuis fin
                    for (int i = (int)len1 - (int)len2; i >= 0; i--) {
                        if (strncmp(arg1 + i, arg2, len2) == 0) {
                            count++;
                            if (count == -index) {
                                found_pos = (int)i + 1; // position en 1-based
                                break;
                            }
                        }
                    }
                }

                if (found_pos == 0) {
                    strcpy(replace_buf, "0"); //pas trouvé
                } else {
                    sprintf(replace_buf, "%d", found_pos);
                }
            }
        }

        // Ajouter le résultat à la sortie
        strcat(result, replace_buf);

        // Continuer après l'appel trouvé
        start = arg3_end + 1;
    }

    // Ajouter le reste
    strcat(result, start);

    return result;
}

//fonction pour use_script(chemindufichier;input)[nomduscriptou0pourinit]
void use_script_launcher(const char *command) {
    // Extrait les parties entre () et []
    const char *start_args = strchr(command, '(');
    const char *end_args = strchr(command, ')');
    const char *start_block = strchr(command, '[');
    const char *end_block = strrchr(command, ']');

    if (!start_args || !end_args) {
        if (error_in_stderr==false){
                printf("Erreur de syntaxe dans use_script()\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur de syntaxe dans use_script()\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
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
                if (error_in_stderr==false){
                    printf("Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stderr);
                }
                //if (error_lock_program==true){
                    //var_script_exit=1;
                //}
                strcpy(parametre, "0.00");
            }
    }
    }

    char nom_script[512] = {0};

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

    //vérifier l'existance du fichier
    if (_access(chemin, 0) != 0){
        if (error_in_stderr==false){
                printf("Erreur : le fichier %s n'existe pas\n",chemin);
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : le fichier %s n'existe pas\n",chemin);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // Tentative de verrouillage du fichier avec 2 essais espacés
    int locked = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (try_lock_file(chemin)) {
            locked = 1;
            break;
        } else {
            Sleep(250);  // attendre 200 ms avant une nouvelle tentative
        }
    }

    if (!locked) {
        if (error_in_stderr==false){
                printf("Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", chemin);
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : Fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", chemin);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    FILE *f = fopen(chemin, "r");
    if (!f) {
        if (error_in_stderr==false){
                printf("Impossible d’ouvrir le fichier : %s\n", chemin);
                fflush(stdout);
        } else {
                fprintf(stderr,"Impossible d’ouvrir le fichier : %s\n", chemin);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        unlock_file(chemin); //libérer l'utilisation du fichier pour le script
        return;
    }

    if (use_script_path==true){
        char tablewrite[1024];
        snprintf(tablewrite, sizeof(tablewrite),"table|frc_use_script_path|=[[%s]]",chemin);
        int infosuccestable=parse_and_save_table(tablewrite);
        if (infosuccestable!=1){
            printf("! la table de sauvgarde du chemin de use_script a échoué : %d", infosuccestable);
            fflush(stdout);
        }
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *fichier_contenu = calloc(len + 2, 1);
    if (!fichier_contenu) {
        if (error_in_stderr==false){
                printf("Erreur : allocation mémoire sur use_script\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : allocation mémoire sur use_script\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        fclose(f);
        unlock_file(chemin);
        return;
    }
    size_t lu = fread(fichier_contenu, 1, len, f);
    fichier_contenu[lu] = '\0';
    fclose(f);

    // Supprimer BOM UTF-8 si présent
    if ((unsigned char)fichier_contenu[0] == 0xEF &&
    (unsigned char)fichier_contenu[1] == 0xBB &&
    (unsigned char)fichier_contenu[2] == 0xBF) {
        memmove(fichier_contenu, fichier_contenu + 3, lu - 2);
        lu -= 3;
        fichier_contenu[lu] = '\0';
    }

    // Normalise les fins de ligne : remplace tous les \r\n ou \r par \n
    char *p = fichier_contenu, *q = fichier_contenu;
    while (*p) {
        if (*p == '\r') {
            if (*(p+1) == '\n') p++; // saute le \r si \r\n
            *q++ = '\n';
        } else {
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';

    // Nettoyage des caractères non imprimables (fin du fichier)
    for (int i = lu - 1; i >= 0; i--) {
        unsigned char c = fichier_contenu[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            fichier_contenu[i] = '\0';
        } else {
            break;
        }
    }

    char *script_debut = NULL;
    if (strcmp(nom_script, "0") == 0) {
        script_debut = strstr(fichier_contenu, "#init[[[");
        if (!script_debut) {
            script_debut = strstr(fichier_contenu, "[[[");
        }
    } else {
        script_debut = NULL;
        char *tmp = fichier_contenu;
        size_t len_nom = strlen(nom_script);

        while ((tmp = strstr(tmp, "#")) != NULL) {
            // Sauter les lignes où il y a plusieurs '#' (ex : '##', '###', etc.)
            if (*(tmp + 1) == '#' || *(tmp - 1) == '#') {
                tmp++;  // avance juste d'un pour pas rester bloqué sur le même '#'
                continue;
            }
            if (strncmp(tmp + 1, nom_script, len_nom) == 0) {
                char *p = tmp + 1 + len_nom;
                while (*p == '\t' || *p == '\n' || *p == '\r') p++; //*p == ' ' ||
                if (strncmp(p, "[[[", 3) == 0) {
                    script_debut = tmp;
                    break;
                }
            }
            tmp++;
        }
    }

    if (!script_debut) {
        if (error_in_stderr==false){
                printf("Script non trouvé dans le fichier\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Script non trouvé dans le fichier\n");
                fflush(stderr);
        }
        //if (error_lock_program==true){
                //var_script_exit=1;
        //}
        free(fichier_contenu);
        unlock_file(chemin); //libérer l'utilisation du fichier pour le script
        return;
    }

    char *bloc_debut = strstr(script_debut, "[[[");
    if (!bloc_debut) {
        if (error_in_stderr==false){
                printf("Erreur : [[[ non trouvé après la balise de script.\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : [[[ non trouvé après la balise de script.\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        free(fichier_contenu);
        unlock_file(chemin);
        return;
    }
    //char *bloc_fin = strstr(bloc_debut, "]]]");
    char *bloc_fin = bloc_debut + 3; // sauter les [[[

    int count_braces = 0;
    while (*bloc_fin) {
        if (*bloc_fin == '{') {
            count_braces++;
        } else if (*bloc_fin == '}') {
            if (count_braces > 0)
                count_braces--;
        } else if (strncmp(bloc_fin, "]]]", 3) == 0 && count_braces == 0) {
            break;
        }
        bloc_fin++;
    }

    if (!*bloc_fin || strncmp(bloc_fin, "]]]", 3) != 0) {
        if (error_in_stderr==false){
                printf("Bloc script mal formé (pas de fermeture ]]] hors d’un bloc {}).\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Bloc script mal formé (pas de fermeture ]]] hors d’un bloc {}).\n");
                fflush(stderr);
        }
        //if (error_lock_program==true){
                //var_script_exit=1;
        //}
        free(fichier_contenu);
        unlock_file(chemin); //libérer l'utilisation du fichier pour le script
        return;
    }

    //bloc_fin += 3; // inclure les 3 crochets

    if (!bloc_debut || !bloc_fin) {
        if (error_in_stderr==false){
                printf("Bloc script mal formé.\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Bloc script mal formé.\n");
                fflush(stderr);
        }
        free(fichier_contenu);
        unlock_file(chemin); //libérer l'utilisation du fichier pour le script
        return;
    }

    bloc_fin += 3; // inclure les 3 crochets
    size_t taille_script = bloc_fin - bloc_debut;

    // Sécurité supplémentaire : ignorer les caractères invisibles après le script
    while (taille_script > 0 &&
       ((unsigned char)bloc_debut[taille_script - 1] < 32 && bloc_debut[taille_script - 1] != '\n' && bloc_debut[taille_script - 1] != '\r')) {
        taille_script--;
    }

    char *script = malloc(taille_script + 2);
    memcpy(script, bloc_debut, taille_script);
    script[taille_script] = '\0';

    free(fichier_contenu);
    unlock_file(chemin);

    fonction_start_remove_newlines(script);

    var_in_module=1;//activation de l'[input]

    if (debug_use_script==true){
        printf("use_script:%s\n",script);
        fflush(stdout);
    }

    //changement de l'environnement pour le fichier donc les var_local
    if (chemin) {
        char next_env[512];
        snprintf(next_env, sizeof(next_env), "%s+%s", nom_script, chemin);
        push_env_object(next_env);
    }

    if (strncmp(script, "[[[", 3) == 0) {
        var_script_exit = 0; //les variables doivent déjà être définit avant donc pas besoin des autres initialisation
        //var_script_pause = 0;
        //mode_script_output = 0;
        //var_affiche_virgule = 0;
        //var_affiche_slash_n = 0;
        //var_boucle_infini = 0;
        handle_script(script);
    }

    //retour à l'environnement précédent
    if (chemin) {
        pop_env_object();
    }

    var_in_module=0;//désactivation de l'[input]
    free(script);
}

//fonction pour définir des chemin de dll dans le fichier pour l'exécutable
void process_define_dll(char* recup) {
    if (strncmp(recup, "define_dll(", 11) != 0) return;

    char* ptr = recup + 11;
    int depth = 1;
    char* val1_start = ptr;
    char* val1_end = NULL;

    // Trouve fin de valeur1 (parenthèse fermante équilibrée)
    while (*ptr && depth > 0) {
        if (*ptr == '(') depth++;
        else if (*ptr == ')') depth--;
        if (depth == 0) val1_end = ptr;
        ptr++;
    }

    if (!val1_end || *ptr != '('){
        if (error_in_stderr==false){
                printf("Erreur : parenthèses sur define_dll()()\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses sur define_dll()()\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // valeur1 extrait
    int len1 = val1_end - val1_start;
    char valeur1[512];
    strncpy(valeur1, val1_start, len1);
    valeur1[len1] = '\0';

    // Vérifie si valeur1 contient des caractères interdits
    if (strpbrk(valeur1, "[]=|{}\"")) {
        if (error_in_stderr==false){
                printf("Erreur : la définition de la dll contient aumoins un caractère interdit : [ ] = { } | \"\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : la définition de la dll contient aumoins un caractère interdit : [ ] = { } | \"\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // Trouve fin de valeur2
    ptr++;  // Sauter la parenthèse ouvrante de valeur2
    depth = 1;
    char* val2_start = ptr;
    char* val2_end = NULL;
    while (*ptr && depth > 0) {
        if (*ptr == '(') depth++;
        else if (*ptr == ')') depth--;
        if (depth == 0) val2_end = ptr;
        ptr++;
    }

    if (!val2_end) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses sur define_dll()()\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses sur define_dll()()\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // valeur2 extrait
    int len2 = val2_end - val2_start;
    char valeur2[512];
    strncpy(valeur2, val2_start, len2);
    valeur2[len2] = '\0';

    // Ajoute ".dll" à valeur2 si nécessaire
    char valeur2_final[MAX_PATH];
    size_t len_val2 = strlen(valeur2);
    if (len_val2 >= 4 && _stricmp(valeur2 + len_val2 - 4, ".dll") == 0) {
        strncpy(valeur2_final, valeur2, MAX_PATH);
    } else {
        snprintf(valeur2_final, MAX_PATH, "%s.dll", valeur2);
    }

    // Vérifie si valeur2_final existe comme fichier
    DWORD attr = GetFileAttributes(valeur2_final);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        if (error_in_stderr==false){
                printf("Erreur : fichier dll %s est introuvable !\n", valeur2_final);
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : fichier dll %s est introuvable !\n", valeur2_final);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // Fichier de mapping dans le dossier de l'exécutable
    char exe_path[MAX_PATH];
    GetModuleFileName(NULL, exe_path, MAX_PATH);
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) *last_slash = '\0';

    char define_path[MAX_PATH];
    snprintf(define_path, sizeof(define_path), "%s\\define_dll_frc.txt", exe_path);

    // Lire les lignes existantes (s'il y en a)
    FILE* file = fopen(define_path, "r");
    char lines[500][1024];
    int count = 0;
    int found = 0;

    if (file) {
        while (fgets(lines[count], sizeof(lines[count]), file) && count < 100) {
            char key[512];
            if (sscanf(lines[count], "%[^=]=%*s", key) == 1) {
                if (strcmp(key, valeur1) == 0) {
                    snprintf(lines[count], sizeof(lines[count]), "%s=%s\n", valeur1, valeur2);
                    found = 1;
                }
            }
            count++;
        }
        fclose(file);
    }

    if (!found) {
        snprintf(lines[count], sizeof(lines[count]), "%s=%s\n", valeur1, valeur2);
        count++;
    }

    // Réécrire tout
    file = fopen(define_path, "w");
    if (!file) {
        if (error_in_stderr==false){
                printf("Erreur : écriture sur le fichier define_dll_frc.txt\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : écriture sur le fichier define_dll_frc.txt\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    for (int i = 0; i < count; ++i) {
        fputs(lines[i], file);
    }

    fclose(file);
}

//supprimer le stockage des dll (free_dll() permet de les libérer)
void release_all_dlls() {
    for (int i = 0; i < dllCount; i++) {
        if (hDllCache[i]) {
            FARPROC pInt = GetProcAddress(hDllCache[i], "frc_interrupted_dll");
            if (pInt) {
                volatile int* pInterrupted = (volatile int*)pInt;
                *pInterrupted = 1;
                Sleep(200); // Laisse le thread s'arrêter
            }
            FreeLibrary(hDllCache[i]);
        }
    }
    dllCount = 0;
}

//fonction qui regarde si le chemin est valide ou si définit dans le fichier gestionnaire des dll : define_dll_frc.txt
char* resolve_dll_path(const char* path_with_ext) {
    char original_path[MAX_PATH];

    // Ajoute ".dll" si ce n'est pas déjà à la fin
    size_t len = strlen(path_with_ext);
    if (len >= 4 && _stricmp(path_with_ext + len - 4, ".dll") == 0) {
        strncpy(original_path, path_with_ext, MAX_PATH);
    } else {
        snprintf(original_path, MAX_PATH, "%s.dll", path_with_ext);
    }

    // Vérifie si le chemin est déjà valide
    DWORD attrib = GetFileAttributes(original_path);
    if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return strdup(original_path);
    }

    // Récupère le chemin du dossier de l'exécutable
    char exe_path[MAX_PATH];
    GetModuleFileName(NULL, exe_path, MAX_PATH);
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) *last_slash = '\0';

    char define_file[MAX_PATH];
    snprintf(define_file, sizeof(define_file), "%s\\define_dll_frc.txt", exe_path);

    FILE* file = fopen(define_file, "r");
    if (!file) return NULL;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char key[512], value[512];
        if (sscanf(line, "%[^=]=%[^\r\n]", key, value) == 2) {
            if (strcmp(key, path_with_ext) == 0) {
                fclose(file);
                // Appliquer .dll à la clé du fichier si besoin
                char value_with_ext[MAX_PATH];
                size_t value_len = strlen(value);
                if (value_len >= 4 && _stricmp(value + value_len - 4, ".dll") == 0) {
                    strncpy(value_with_ext, value, MAX_PATH);
                } else {
                    snprintf(value_with_ext, MAX_PATH, "%s.dll", value);
                }
                DWORD new_attrib = GetFileAttributes(value_with_ext);
                if (new_attrib != INVALID_FILE_ATTRIBUTES && !(new_attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                    return strdup(value_with_ext);
                } else {
                    return NULL; // Défini mais fichier inexistant
                }
            }
        }
    }

    fclose(file);
    return NULL; // Pas trouvé
}

// Fonction qui appelle une fonction de type const char* func(const char*) sur un fichier dll
char* call_dll_function(const char* dll_path, const char* func_name, const char* args) {
    char* resolved_path = resolve_dll_path(dll_path);
    if (!resolved_path) {
        if (error_in_stderr==false){
                printf("Erreur : le chemin de la DLL est indéfini !\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : le chemin de la DLL est indéfini !\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return strdup("[ERREUR DLL]");
    }

    HINSTANCE hDll = LoadLibrary(resolved_path);
    if (!hDll){
        if (error_in_stderr==false){
                printf("Erreur: DLL non chargée\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur: DLL non chargée\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return strdup("[ERREUR DLL]");
    }

    typedef const char* (*FuncType)(char*);
    FuncType func = (FuncType)GetProcAddress(hDll, func_name);
    if (!func) {
        FreeLibrary(hDll);
        if (error_in_stderr==false){
                printf("Erreur: fonction inconnue sur dll\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur: fonction inconnue sur dll\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return strdup("[ERREUR DLL]");
    }

    char* args_copy = strdup(args);  // Copie modifiable
    const char* result = func(args_copy);
    char* copy = strdup(result ? result : "");
    free(args_copy);  // Libère l'argument après appel

    //origine empêche les threads des dll
    //FreeLibrary(hDll);
    //nouveau, stockage des dll
    // Ne pas libérer maintenant, on garde en cache
    if (dllCount < 128) hDllCache[dllCount++] = hDll;

    return copy;
}

// Fonction qui traite toute la chaîne d'entrée pour dll||[func()]
char* process_dll(const char* input) {
    char* output = malloc(MAX_COMMAND_LEN);
    output[0] = '\0';

    const char* ptr = input;
    while (*ptr) {
        const char* start = strstr(ptr, "dll|");
        if (!start) {
            strcat(output, ptr);
            break;
        }

        strncat(output, ptr, start - ptr);
        const char* dll_sep = strchr(start + 4, '|');
        const char* func_start = strchr(start, '[');
        const char* func_end = NULL;
        int depth = 0;
        if (func_start) {
            const char* p = func_start;
            while (*p) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                if (depth == 0) {
                    func_end = p;
                    break;
                }
                p++;
            }
        }

        if (!dll_sep || !func_start || !func_end || func_end < func_start) {
            strcat(output, "[ERREUR SYNTAXE]");
            if (error_in_stderr==false){
                printf("Erreur : syntaxe incorrect sur dll||[]\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : syntaxe incorrect sur dll||[]\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            break;
        }

        char dll_path[256], func_call[256];
        strncpy(dll_path, start + 4, dll_sep - (start + 4));
        dll_path[dll_sep - (start + 4)] = '\0';

        strncpy(func_call, func_start + 1, func_end - func_start - 1);
        func_call[func_end - func_start - 1] = '\0';

        char* open_paren = strchr(func_call, '(');
        if (!open_paren) {
            strcat(output, "[ERREUR APPELS]");
            if (error_in_stderr==false){
                printf("Erreur : parenthèse ouvrante non trouvée sur dll||[()]\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : parenthèse ouvrante non trouvée sur dll||[()]\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            ptr = func_end + 1;
            continue;
        }

        // Chercher la parenthèse fermante correspondante
        depth = 1;
        char* scan = open_paren + 1;
        char* close_paren = NULL;
        while (*scan && depth > 0) {
            if (*scan == '(') depth++;
            else if (*scan == ')') depth--;
            scan++;
        }

        if (depth != 0) {
            // Pas de parenthèse fermante correspondante
            strcat(output, "[ERREUR APPELS]");
            if (error_in_stderr==false){
                printf("Erreur : parenthèses incorrect sur dll||[()]\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : parenthèses incorrect sur dll||[()]\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            ptr = func_end + 1;
            continue;
        }

        close_paren = scan - 1;


        char func_name[256], func_args[256];
        strncpy(func_name, func_call, open_paren - func_call);
        func_name[open_paren - func_call] = '\0';

        strncpy(func_args, open_paren + 1, close_paren - open_paren - 1);
        func_args[close_paren - open_paren - 1] = '\0';

        if (dll_arg_analysis==true){
            //forcer la numerisation partout...
            if (strstr(func_args,"num(") != NULL && var_activ_func_num==true){
                strcpy(func_args,auto_num(func_args));
            }

            //textualisation des tables, print, spreak...
            if (strstr(func_args,"text(") != NULL && var_activ_func_text==true){
                char *temp = auto_text(func_args);
                strcpy(func_args, temp);  // OK car temp est séparé de arg
                free(temp);
            }

            char *arg=traiter_chaine_guillemet(func_args);
            strcpy(func_args,arg);
            free(arg);
        }

        //printf("%s : %s : %s\n",dll_path,func_name,func_args);

        char* result = call_dll_function(dll_path, func_name, func_args);
        strcat(output, result);
        free(result);

        ptr = func_end + 1;
    }

    return output;
}

//fonction sleep()
void process_sleep(char *recup) {
    if (strncmp(recup, "sleep(", 6) != 0) {
        return; // Pas une instruction sleep()
    }

    char *ptr = recup + 6;
    int paren_count = 1;
    char *start = ptr;

    // Trouver la fermeture de la parenthèse principale
    while (*ptr && paren_count > 0) {
        if (*ptr == '(') paren_count++;
        else if (*ptr == ')') paren_count--;
        ptr++;
    }

    if (paren_count != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses mal formées dans sleep().\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses mal formées dans sleep().\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    // Extraire l'expression entre les parenthèses
    int len = ptr - start - 1;
    if (len <= 0) {
       if (error_in_stderr==false){
                printf("Erreur : expression vide dans sleep().\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : expression vide dans sleep().\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    char expr[1024];
    strncpy(expr, start, len);
    expr[len] = '\0';

    // Évaluer la durée
    double seconds = condition_script_order(expr);
    int seconds_int = (int)seconds;

    //printf(" sleep : %d\n",seconds_int);

    if (seconds_int > 0) {
        Sleep(seconds_int);
    }
}

//fonction pour traiter les ""blabla"" dans les print() affiche() ou speak() parle()
char* traiter_chaine_guillemet(const char *input) {
    const char *ptr = input;
    char *result = malloc(MAX_COMMAND_LEN);
    result[0] = '\0';

    char temp[1024];
    int temp_i = 0;
    bool in_quotes = false;
    int paren_level = 0; // compteur de parenthèses

    while (*ptr) {
        // Mettre à jour le compteur de parenthèses
        if (*ptr == '(') paren_level++;
        else if (*ptr == ')') if (paren_level > 0) paren_level--;

        if (*ptr == '"' && paren_level == 0) {
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
                if (error_in_stderr==false){
                    printf("Erreur : définir une variable dans un speak est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : définir une variable dans un speak est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stderr);
                }
                //if (error_lock_program==true){
                    //var_script_exit=1;
                //}
                strcpy(textToSpeak, "0.00");
            }
            }

            // Convertir le texte à dire en format wchar_t (UTF-16)
            wchar_t wideText[1024];
            MultiByteToWideChar(CP_UTF8, 0, textToSpeak, -1, wideText, 1024);

            ISpVoice *pVoice = NULL;

            // Initialiser COM
            if (FAILED(CoInitialize(NULL))) {
                if (error_in_stderr==false){
                    printf("Erreur d'initialisation COM sur speak\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur d'initialisation COM sur speak\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                return;
            }

            // Créer l'objet de voix SAPI
            if (SUCCEEDED(CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL, &IID_ISpVoice, (void **)&pVoice))) {
                // Faire parler le texte
                pVoice->lpVtbl->Speak(pVoice, wideText, 0, NULL);
                // Libérer l'objet voix après utilisation
                pVoice->lpVtbl->Release(pVoice);
            } else {
                if (error_in_stderr==false){
                    printf("Erreur de création de la voix.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur de création de la voix.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
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
        // Vérifie si on a trouvé un tag spécial
        if (strncmp(&buffer[i], "[/n]", 4) == 0) {
            temp[j++] = '\n';
            i += 4;
        } else if (strncmp(&buffer[i], "[/l0]", 5) == 0) {
            temp[j++] = ' ';
            i += 5;
        } else if (strncmp(&buffer[i], "[/l1]", 5) == 0) {
            temp[j++] = '\"';  // guillemet "
            i += 5;
        } else if (strncmp(&buffer[i], "[/l2]", 5) == 0) {
            temp[j++] = '|';
            i += 5;
        } else if (strncmp(&buffer[i], "[/l3]", 5) == 0) {
            temp[j++] = '[';
            i += 5;
        } else if (strncmp(&buffer[i], "[/l4]", 5) == 0) {
            temp[j++] = ']';
            i += 5;
        } else if (strncmp(&buffer[i], "[/l5]", 5) == 0) {
            temp[j++] = '(';
            i += 5;
        } else if (strncmp(&buffer[i], "[/l6]", 5) == 0) {
            temp[j++] = ')';
            i += 5;
        } else if (strncmp(&buffer[i], "[/0]", 4) == 0) {
            // Arrêt immédiat de la chaîne
            break;
        } else {
            temp[j++] = buffer[i++];
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
    int paren_level = 0; // Compteur de parenthèses

    char send_buf[MAX_COMMAND_LEN];
    int send_index = 0;

    //printf("Affichage : ");

    while (*text) {
        if (*text == '(') {
            paren_level++;
        }
        else if (*text == ')') {
            if (paren_level > 0) paren_level--;
        }

        if (*text == '"' && paren_level == 0) {
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
            while (*text) { //&& !isspace(*text)   pour les espaces bloquant
                if (*text == '(') {
                    paren_level++;
                } else if (*text == ')') {
                    if (paren_level > 0) paren_level--;
                } else if (*text == '"' && paren_level == 0) {
                    break; // guillemet rencontré hors parenthèses
                }

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
            if (error_in_stderr==false){
                    printf("Erreur : Parenthèse de fermeture manquante sur affiche ou print.\n");
                    fflush(stdout);
            } else {
                    fprintf(stderr,"Erreur : Parenthèse de fermeture manquante sur affiche ou print.\n");
                    fflush(stderr);
            }
            if (error_lock_program==true){
                    var_script_exit=1;
            }
        }
    }
}


//fonction de numérisation forcer pour les liste num()
char *auto_num(char *recup) {
    static char result[MAX_COMMAND_LEN];
    char *in = recup;
    char *out = result;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (!autorise_slash_text_num_lock && in[0] == '/') {
            // Ne pas traiter cela comme une fonction, juste copier le caractère '/'
            result[i++] = *in++;
            continue;
        }
        if (strncmp(in, "num(", 4) == 0 || (in[0] == '/' && strncmp(in + 1, "num(", 4) == 0)) {
        int is_escaped = (in[0] == '/');
        if (autorise_slash_text_num_lock == false) is_escaped = 0;

        const char *start = in + (is_escaped ? 1 : 0);  // start pointe sur "num("

        if (is_escaped) {
            // On saute le '/', on cherche juste la fin de "num(...)"
            in = start + 4;
            int paren = 1;
            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans num(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans num(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                return recup;
            }

            // On copie tel quel "num(...)"
            int len = in - start;
            if (i + len >= MAX_COMMAND_LEN - 1) break;
            strncpy(out + i, start, len);
            i += len;
        } else {
            // Cas normal : exécution de num(...)
            in = in + 4;  // skip "num("
            int paren = 1;
            const char *expr_start = in;
            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans num(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans num(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                return recup;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf, expr_start, expr_len);
            expr_buf[expr_len] = '\0';

            //printf("num : %s\n",expr_buf);

            double val = condition_script_order(expr_buf);
            char val_buf[128];
            snprintf(val_buf, sizeof(val_buf), "%.2f", val);
            replace_comma_with_dot(val_buf); //remettre , par .
            int len = strlen(val_buf);
            if (i + len >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, val_buf);
                i += len;
            }
        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}

//fonction pour les print() affiche speak() et les tables, retourne en texte le contenu des listes pas de calcul (prend var aussi)
char *auto_text(const char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;

    char *out = result;
    const char *in = recup;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (strncmp(in, "text(", 5) == 0 || (in > recup && *(in - 1) == '/' && strncmp(in, "text(", 5) == 0)) {
            int is_escaped = (in > recup && *(in - 1) == '/');
            if (autorise_slash_text_num_lock==false) is_escaped=0;

            if (is_escaped) {
                if (i > 0) i--;
            }

            const char *start_text = in - (is_escaped ? 1 : 0);
            in += 5;
            int paren = 1;
            const char *expr_start = in;

            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                //free(result);
                strcpy(result, "0");
                return result;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf_1[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf_1, expr_start, expr_len);
            expr_buf_1[expr_len] = '\0';

            if (is_escaped) {
                int total_len = in - start_text;
                if (i + total_len >= MAX_COMMAND_LEN - 1) break;
                strncpy(out + i, start_text + 1, total_len - 1);
                i += total_len - 1;
            } else {
                char *expr_buf = expr_buf_1;

                //printf("text : %s\n",expr_buf);

                while (*expr_buf && strlen(expr_buf) > 3){
                if (strncmp(expr_buf, "dll|",4) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = process_dll(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;  //ancien : repart à 0 et remodifie des modifs donc pas utile (ou pas voulu)
                    //continue;
                } else {
                if (strncmp(expr_buf,"text(",5) == 0 && var_activ_func_text==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    // Trouver la fin du text(...)
                    const char *start = expr_buf;
                    const char *in = expr_buf + 5; // après "text("
                    int paren_ = 1;
                    while (*in && paren_ > 0) {
                        if (*in == '(') paren_++;
                        else if (*in == ')') paren_--;
                        in++;
                    }
                    if (paren_ != 0) {
                        // Parenthèses déséquilibrées
                        // -> tu peux gérer ton erreur ici
                        expr_buf++;
                        continue;
                    }

                    // Longueur du bloc text(...)
                    int block_len = in - start;  // inclut la parenthèse fermante

                    // Extraire ce bloc
                    char temp_buf[MAX_COMMAND_LEN];
                    strncpy(temp_buf, start, block_len);
                    temp_buf[block_len] = '\0';

                    // Passer uniquement ce bloc à auto_text()
                    char *temp_result = auto_text(temp_buf);
                    if (temp_result) {
                        // Remplacer le bloc par le résultat
                        char new_buf[MAX_COMMAND_LEN];
                        snprintf(new_buf, MAX_COMMAND_LEN, "%s%s",
                        temp_result,     // résultat de text(...)
                        start + block_len); // reste de la chaîne après text(...)

                        memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                        strncpy(expr_buf, new_buf, MAX_COMMAND_LEN);
                        expr_buf[MAX_COMMAND_LEN - 1] = '\0';
                        free(temp_result);

                        if (strcmp(old_buf, expr_buf) == 0) {
                            expr_buf++;
                            continue;
                        }

                        // Avancer expr_buf au premier point de différence
                        char *p_old = old_buf;
                        char *p_new = expr_buf;
                        while (*p_old && *p_new && *p_old != *p_new) {
                            p_old++;
                            p_new++;
                        }
                        expr_buf = p_new;
                        if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                        continue;
                    }
                }

                if (strncmp(expr_buf,"num(",4) == 0 && var_activ_func_num==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, auto_num(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"var_local|",10)== 0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    handle_var_local_get(expr_buf,MAX_COMMAND_LEN);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|", 6) == 0 &&
                    ((strstr(expr_buf, "|=[[") != NULL) ||
                     (strstr(expr_buf, "|=join(") != NULL) ||
                     (strstr(expr_buf, "|=suppr()") != NULL))) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    int results = parse_and_save_table(expr_buf);
                    if (results==4){
                        if (error_in_stderr==false){
                            printf("Erreur: parenthèses non équilibré sur table=[[;]]\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: parenthèses non équilibré sur table=[[;]]\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 3) {
                        if (error_in_stderr==false){
                            printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 2) {
                        if (error_in_stderr==false){
                            printf("Erreur: ]] manquante !.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: ]] manquante !.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results != 1) {
                        if (error_in_stderr==false){
                            printf("Erreur lors de l'enregistrement de la table.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    }
                    strcpy(expr_buf, "0");

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|",6) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, for_get_table_value(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "var|",4) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, handle_variable(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    char *ptr = expr_buf;
                    while (*ptr) {
                        if (*ptr == ',') *ptr = '.';
                        ptr++;
                    }

                    if (strstr(expr_buf, "None") != NULL) {
                        if (error_in_stderr==false){
                            printf("Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stderr);
                        }
                        //if (error_lock_program==true){
                            //var_script_exit=1;
                        //}
                        strcpy(result, "0.00");
                        return result;
                    }

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }
                if (strncmp(expr_buf, "text_replace(\"", 14) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_replace(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_cut(\"",10) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, handle_text_cut(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_letter(\"",13) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_letter(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_uppercase(\"",16) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_uppercase(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_lowercase(\"",16) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_lowercase(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_word(\"",11) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_word(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"prompt(",7)==0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);
                    //strcpy(expr_buf,get_prompt(expr_buf));
                    char *expr_buf_f = get_prompt(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"[input]",8)==0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

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
                        char *src = expr_buf;
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
                        memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                        strcpy(expr_buf, new_command);
                    } else {
                        // Si var_in_module != 1, remplacer [input] par 0.00
                        char new_command[MAX_COMMAND_LEN] = {0};
                        char *src = expr_buf;
                        char *found;

                        while ((found = strstr(src, "[input]"))) {
                            strncat(new_command, src, found - src);
                            strcat(new_command, "0.00");
                            src = found + 7;
                        }

                        strcat(new_command, src);
                        memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                        strcpy(expr_buf, new_command);
                    }

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "[main_arg]",10)==0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    // Valeur à insérer : soit main_arg_global, soit "0" si vide
                    const char *replacement = (main_arg_global[0] != '\0') ? main_arg_global : "0";

                    char new_command[MAX_COMMAND_LEN] = {0};
                    char *src = expr_buf;
                    char *found;

                    while ((found = strstr(src, "[main_arg]"))) {
                        // Copier la partie avant [main_arg]
                        strncat(new_command, src, found - src);

                        // Ajouter la valeur de remplacement
                        strncat(new_command, replacement, sizeof(new_command) - strlen(new_command) - 1);

                        // Sauter "[main_arg]" (10 caractères)
                        src = found + 10;
                    }

                    // Ajouter la fin de la chaîne restante
                    strncat(new_command, src, sizeof(new_command) - strlen(new_command) - 1);

                    // Remplacer la commande originale
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf, new_command);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                }
                expr_buf++;
                }

                if (i + strlen(expr_buf_1) >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, expr_buf_1);
                i += strlen(expr_buf_1);
            }

        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}

//fonction pour les prompt()
char *get_prompt(const char *recup) {
    const char *ptr = recup;
    char *result = malloc(MAX_COMMAND_LEN);
    result[0] = '\0';

    while (*ptr) {
        if (strncmp(ptr, "prompt(", 7) == 0) {
            ptr += 7; // Skip "prompt("
            int paren_count = 1;
            const char *start = ptr;

            while (*ptr && paren_count > 0) {
                if (*ptr == '(') paren_count++;
                else if (*ptr == ')') paren_count--;
                ptr++;
            }

            if (paren_count != 0) {
                strcat(result, "0"); // Malformed
                break;
            }

            char arg[2048];
            int len = ptr - start - 1;
            if (len > 0 && len < sizeof(arg)) {
                strncpy(arg, start, len);
                arg[len] = '\0';
            } else {
                strcat(result, "0");
                continue;
            }

            //forcer la numerisation partout, ici prompt(interne)...
            if (strstr(arg,"num(") != NULL && var_activ_func_num==true){
                strcpy(arg,auto_num(arg));
            }

            //textualisation des tables, print, spreak, ici prompt(interne)...
            if (strstr(arg,"text(") != NULL && var_activ_func_text==true){
                char *temp = auto_text(arg);
                strcpy(arg, temp);  // OK car temp est séparé de arg
                free(temp);
            }

            char *message = traiter_chaine_guillemet(arg);

            char user_input[512] = {0};
            if (socket_var == 1) {
                // Envoie le message via socket
                char prompt_msg[1024];
                snprintf(prompt_msg, sizeof(prompt_msg), "%s :\n>> ", message);
                send(client, prompt_msg, strlen(prompt_msg), 0);

                // Attend réponse client (bloc jusqu'à réception)
                int recv_len = recv(client, user_input, sizeof(user_input) - 1, 0);
                if (recv_len <= 0) {
                    strcat(result, "0"); // erreur ou timeout
                } else {
                    user_input[recv_len] = '\0';
                    // Nettoyer \r\n si présent (Windows)
                    size_t l = strlen(user_input);
                    if (l > 0 && (user_input[l - 1] == '\n' || user_input[l - 1] == '\r')) user_input[l - 1] = '\0';
                    strcat(result, user_input);
                }
            } else {
                printf("%s :\n>> ", message);
                fflush(stdout);

                time_t start_time = time(NULL);
                while (difftime(time(NULL), start_time) < 60) {
                    if (fgets(user_input, sizeof(user_input), stdin)) {
                        size_t l = strlen(user_input);
                        if (l > 0 && user_input[l - 1] == '\n') user_input[l - 1] = '\0';
                        break;
                    }
                }

                if (strlen(user_input) == 0) {
                    //printf("\nDélai dépassé. Valeur par défaut : 0\n");
                    strcat(result, "0");
                } else {
                    strcat(result, user_input);
                }
            }

            free(message);
        } else {
            strncat(result, ptr, 1);
            ptr++;
        }
    }

    return result;
}

// Fonction qui exécute une commande
void execute_script_order(char *command) {
    if (strncmp(command, "###sortie:classique###",22)==0 || strncmp(command,"###sortie:normale###",20)==0 || strncmp(command,"###output:normal###",19)==0){
        mode_script_output=0;
        return;
    }
    if (mode_script_output==1){
        printf("%s\n", command); // Simulation d'une exécution
        fflush(stdout);
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
    if (strncmp(command,"###fonction_num:oui###",22)==0 || strncmp(command,"###function_num:true###",23)==0 || strncmp(command,"###fonction_num:true###",23)==0){
        var_activ_func_num=true;
        return;
    }
    if (strncmp(command,"###fonction_num:non###",22)==0 || strncmp(command,"###function_num:false###",23)==0 || strncmp(command,"###fonction_num:false###",23)==0){
        var_activ_func_num=false;
        return;
    }
    if (strncmp(command,"###fonction_text:oui###",23)==0 || strncmp(command,"###function_text:true###",24)==0 || strncmp(command,"###fonction_text:true###",24)==0){
        var_activ_func_text=true;
        return;
    }
    if (strncmp(command,"###fonction_text:non###",23)==0 || strncmp(command,"###function_text:false###",25)==0 || strncmp(command,"###fonction_text:false###",25)==0){
        var_activ_func_text=false;
        return;
    }
    //pour désactivé la non exécution des num() et des text() si il sont précéder d'un / ex: '/text()' avec 'true' renvoie 'text()', avec 'false': '/lerésultatdetext'
    if (strncmp(command,"###autorise/:oui###",19)==0 || strncmp(command,"###autorise/:true###",20)==0){
        autorise_slash_text_num_lock=true;
        return;
    }
    if (strncmp(command,"###autorise/:non###",19)==0 || strncmp(command,"###autorise/:false###",21)==0){
        autorise_slash_text_num_lock=false;
        return;
    }
    //pour enregistré ou non la table|frc_use_script_path|=[[chemin]]
    if (strncmp(command,"###table_use_script_path:true###",32)==0 || strncmp(command,"###table_use_script_path:oui###",31)==0){
        use_script_path=true;
        return;
    }
    if (strncmp(command,"###table_use_script_path:false###",33)==0 || strncmp(command,"###table_use_script_path:non###",31)==0){
        use_script_path=false;
        return;
    }
    //debug use_script avec un print du script avant sont exécution :
    if (strncmp(command,"###debug_use_script:true###",27)==0 || strncmp(command,"###debug_use_script:oui###",26)==0){
        debug_use_script=true;
        return;
    }
    if (strncmp(command,"###debug_use_script:false###",28)==0 || strncmp(command,"###debug_use_script:non###",26)==0){
        debug_use_script=false;
        return;
    }
    //activé le traitement des argument des fonctions dll comme dans les print, speak ...
    if (strncmp(command,"###dll_arg_analysis:true###",27)==0 || strncmp(command,"###analyse_argument_dll:oui###",30)==0){
        dll_arg_analysis=true;
        return;
    }
    if (strncmp(command,"###dll_arg_analysis:false###",28)==0 || strncmp(command,"###analyse_argument_dll:non###",30)==0){
        dll_arg_analysis=false;
        return;
    }

    //afficher les erreurs sur stderr
    if (strncmp(command,"###sortie_erreur:oui###",23)==0 || strncmp(command,"###sortie_erreur:true###",24)==0 || strncmp(command,"###output_error:true###",23)==0){
        error_in_stderr=true;
        return;
    }
    if (strncmp(command,"###sortie_erreur:non###",23)==0 || strncmp(command,"###sortie_erreur:false###",25)==0 || strncmp(command,"###output_error:false###",24)==0){
        error_in_stderr=false;
        return;
    }
    //arrêter le prog si il y a une erreur
    if (strncmp(command,"###stop_script_erreur:oui###",28)==0 || strncmp(command,"###stop_script_erreur:true###",29)==0 || strncmp(command,"###stop_script_error:true###",28)==0){
        error_lock_program=true;
        return;
    }
    if (strncmp(command,"###stop_script_erreur:non###",28)==0 || strncmp(command,"###stop_script_erreur:false###",30)==0 || strncmp(command,"###stop_script_error:false###",29)==0){
        error_lock_program=false;
        return;
    }

    //global_bool
    if (strncmp(command,"###global_bool:none###",22)==0){
        var_global_bool=-1;
        return;
    }
    if (strncmp(command,"###global_bool:false###",23)==0){
        var_global_bool=0;
        return;
    }
    if (strncmp(command,"###global_bool:true###",22)==0){
        var_global_bool=1;
        return;
    }

    //affiche l'environnement actuel pour les variables local (var_local)
    if (strncmp(command,"var_local_env()",15)==0){
        printf("l'environnement actuel des var_local est : %s\n",current_env_object());
        fflush(stdout);
        return;
    }

    //supprimé les environnements inutilisé si on est en environnement global
    if (strncmp(command,"clear_var_local_env()",21)==0 || strncmp(command,"efface_var_local_env()",22)==0) {
        if (strcmp(current_env_object(),"global")==0) {
            clear_env_object_except_global();
            return;
        } else {
            if (error_in_stderr==false){
                printf("Erreur : l'environnement actuel n'est pas global, impossible de supprimer les environnements\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : l'environnement actuel n'est pas global, impossible de supprimer les environnements\n");
                fflush(stderr);
            }
            //if (error_lock_program==true){
                            //var_script_exit=1;
            //}
            return;
        }
    }

    //supprimé toutes les variables local de l'environnement actuel
    if (strncmp(command,"free_var_local()",16)==0 || strncmp(command,"libere_var_local()",18)==0 || strncmp(command,"libère_var_local()",18)==0) {
        if (free_var_env_object(current_env_object()) == 0) {
            if (error_in_stderr==false){
                printf("Erreur : system error in env in free_var_local(), please exit and restart\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : system error in env in free_var_local(), please exit and restart\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
        }
        return;
    }

    //supprimé une variable local de l'environnement actuel
    if (strncmp(command,"var_local_suppr(",16)==0){
        handle_var_local_suppr(command);
        return;
    }

    if (strncmp(command,"free_dll()",10)==0 || strncmp(command,"dll_libre()",11)==0){
        release_all_dlls();
        return;
    }

    if (strncmp(command,"sleep(",6)==0){
        process_sleep(command);
    }

    if (strncmp(command,"threading(",10)==0){
        process_threading(command);
        return;
    }

    if (strncmp(command,"close_threading()",17)==0){
        terminate_all_threadings();
        return;
    }

    //fonction pour les prompt()
    if (strstr(command,"prompt(") != NULL){
        char *temp = get_prompt(command);
        strcpy(command, temp);  // OK car temp est séparé de command
        free(temp);
    }

    //textualisation des tables, print, spreak ...
    if (strstr(command,"text(") != NULL && var_activ_func_text==true){
        char *temp = auto_text(command);
        strcpy(command, temp);  // OK car temp est séparé de command
        free(temp);
    }

    //forcer la numerisation partout
    if (strstr(command,"num(") != NULL && var_activ_func_num==true){
        strcpy(command,auto_num(command));
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

    //fonction define_dll()()
    if (strncmp(command,"define_dll(",11)==0){
        process_define_dll(command);
        return;
    }

    if (strncmp(command,"affiche(",8)==0 || strncmp(command,"print(",6)==0){
        fonction_script_command_for_affiche(command);
        return;
    }

    //fonction pour les speak()
    if (strncmp(command, "speak(", 6) == 0 || strncmp(command, "parle(", 6) == 0) {
        handle_speak(command);
        return;
    }

    //enregistré la variable local
    if (strncmp(command,"var_local|",10)==0 && strstr(command,"|=") != NULL){
        char* successs = handle_var_local_assignment(command);
        if (strncmp(successs,"1",1)==0){
            return;
        }
    }

    // Supprimer les espaces blancs et retours à la ligne de l'expression
    command[strcspn(command, "\n")] = 0;

    //fonction pour exécuter un dll de traitement
    if (strstr(command,"dll|")!=NULL && strstr(command,"|[")!=NULL){
        char *temp = process_dll(command);
        //memset(command, 0, MAX_COMMAND_LEN); // efface tout le buffer
        strcpy(command,temp);
        free(temp);
    }

    //pour savoir l'état de la variable table_use_script_path
    if (strstr(command,"##state_table_use_script_path##")!=NULL){
        strcpy(command,replace_state_table_use_script_path(command));
    }

    //pour savoir l'état de la variable global_bool
    if (strstr(command,"##state_global_bool##")!=NULL){
        strcpy(command,replace_state_global_bool(command));
    }

    replace_in_var(command); //fonction pour remplacer les in_var()
    //remplacer les time()
    strcpy(command,replace_actual_time(command));
    //remplacer les num_day()
    if (strstr(command,"num_day()")!=NULL){
        strcpy(command,handle_num_day(command));
    }
    //taille de l'écran
    if (strstr(command,"screen_size(x)")!=NULL || strstr(command,"screen_size(y)")!=NULL || strstr(command,"ecran_taille(x)")!=NULL || strstr(command,"ecran_taille(y)")!=NULL){
        char *save_size = replace_screen_dimensions(command);
        strcpy(command,save_size);
        free(save_size);
    }

    if (strstr(command,"version()")!=NULL){
        strcpy(command,handle_version(command));
    }

    if (strstr(command,"block_executed()")!=NULL){
        char *command_f=handle_block_executed(command);
        memset(command, 0, MAX_COMMAND_LEN);
        strcpy(command,command_f);
        free(command_f);
    }

    if (strstr(command,"text_eval(\"")!=NULL){
        strcpy(command,handle_text_eval(command));
    }

    if (strstr(command,"text_len(\"")!=NULL){
        strcpy(command,handle_text_len(command));
    }

    if (strstr(command,"text_count(\"")!=NULL){
        strcpy(command,handle_text_count(command));
    }

    if (strstr(command,"text_find(\"")!=NULL){
        strcpy(command,handle_text_find(command));
        //char *command_f = handle_text_find(command);
        //strcpy(command,command_f);
        //free(command_f);
    }

    //remplacer les random[,]
    handle_random(command,last_result);
    //remplacer les random(;)
    if (strstr(command,"random(")!= NULL){
        strcpy(command, handle_random2(command));
    }

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

    if (strstr(command, "table|") != NULL && strstr(command, "|[") != NULL) {
            strcpy(command,for_get_table_value(command));
            //printf("%s\n",command);
    }
    if (strstr(command, "table|") != NULL && strstr(command, "|.len(") != NULL) {
            strcpy(command,for_len_table(command));
            //printf("%s\n",command);
    }

    //récupérer la varable local
    if (strstr(command,"var_local|")!=NULL){
        handle_var_local_get(command,MAX_LEN);
    }

    if (strstr(command, "var|") != NULL) {
    // Remplacer "rep" par last_result
    strcpy(command, replace_rep(command, last_result));

    // Chercher '=' pour diviser
    char *equal_pos = strchr(command, '=');
    if (equal_pos != NULL) {
        char prefix[256] = {0};
        strncpy(prefix, command, equal_pos - command);
        prefix[equal_pos - command] = '\0';

        char *after_equal = equal_pos + 1;

        // Vérifier si après '=' ça commence par "++" ou "--"
        if (strncmp(after_equal, "++", 2) == 0) {
            snprintf(command, 1024, "%s=%s+1", prefix, prefix);
        } else if (strncmp(after_equal, "--", 2) == 0) {
            snprintf(command, 1024, "%s=%s-1", prefix, prefix);
        } else if (*after_equal == '"') {
            // Ne pas évaluer, laisser tel quel
            snprintf(command, 1024, "%s=%s", prefix, after_equal);
        } else {
            if (strstr(after_equal, "var|") != NULL) {
                // Étape 1 : remplacer var|...| après le = par leur valeur
                strcpy(after_equal, handle_variable(after_equal));
            }
            // Étape 2 : envoyer à condition_script_order pour gérer sqrt, cercle, etc.
            char results[512] = {0};
            replace_comma_with_dot(after_equal);
            double val = condition_script_order(after_equal);
            snprintf(results, sizeof(results), "%.2f", val);
            replace_comma_with_dot(results);
            // Reconstruire la commande
            snprintf(command, 1024, "%s=%s", prefix, results);
        }
    }
    // Étape finale : handle_variable doit aussi gérer l'enregistrement
    strcpy(command, handle_variable(command));

    // Sécurité : remplacer les virgules par des points
    replace_comma_with_dot(command);

    // Vérifier s'il y a "None"
    if (strstr(command, "None") != NULL) {
        return;
    }

    replace_comma_with_dot(command);

    }
    if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.sea(") != NULL) {
            table_sea(command);
            return;
    }

    if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.add(") != NULL){
            int result = add_table(command);
            //printf("add operation: %d\n",result);
            if (result==-2){
                if (error_in_stderr==false){
                    printf("Erreur: problème de ( ) sur table||.add()\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: problème de ( ) sur table||.add()\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            }
            if (result==-1){
                if (error_in_stderr==false){
                    printf("Erreur: sur lecture/écriture fichier table_frc.txt\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: sur lecture/écriture fichier table_frc.txt\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            }
            return;
    }
    if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.del(") != NULL){
            int result = process_table_del(command);
            //printf("del operation: %d\n",result);
            return;
    }
    if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.edit(") != NULL){
            int result = process_table_edit(command);
            //printf("edit operation: %d\n",result);
            return;
    }

    if (strncmp(command, "table|", 6) == 0 && ((strstr(command, "|=[[") != NULL)|| (strstr(command, "|=join(") != NULL) || (strstr(command, "|=suppr()") != NULL))) {
            int result = parse_and_save_table(command);
            if (result==4){
                if (error_in_stderr==false){
                    printf("Erreur: parenthèses non équilibré sur table=[[;]]\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: parenthèses non équilibré sur table=[[;]]\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            } else if (result==3){
                if (error_in_stderr==false){
                    printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            } else if (result==2){
                if (error_in_stderr==false){
                    printf("Erreur: ]] manquante !.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur: ]] manquante !.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            } else if (result != 1) {
                if (error_in_stderr==false){
                    printf("Erreur lors de l'enregistrement de la table.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            }
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

    if (strstr(command, "[main_arg]")) {
        // Valeur à insérer : soit main_arg_global, soit "0" si vide
        const char *replacement = (main_arg_global[0] != '\0') ? main_arg_global : "0";

        char new_command[MAX_COMMAND_LEN] = {0};
        char *src = command;
        char *found;

        while ((found = strstr(src, "[main_arg]"))) {
            // Copier la partie avant [main_arg]
            strncat(new_command, src, found - src);

            // Ajouter la valeur de remplacement
            strncat(new_command, replacement, sizeof(new_command) - strlen(new_command) - 1);

            // Sauter "[main_arg]" (10 caractères)
            src = found + 10;
        }

        // Ajouter la fin de la chaîne restante
        strncat(new_command, src, sizeof(new_command) - strlen(new_command) - 1);

        // Remplacer la commande originale
        strcpy(command, new_command);
    }

    if (strstr(command,"execute(") != NULL){
        parse_and_execute(command);
        return;
    }
}

double condition_script_order(char *command) {
    //pour récupérer l'état de la variable table_use_script_path
    if (strstr(command,"##state_table_use_script_path##")!=NULL){
        strcpy(command,replace_state_table_use_script_path(command));
    }
    //pour savoir l'état de la variable global_bool
    if (strstr(command,"##state_global_bool##")!=NULL){
        strcpy(command,replace_state_global_bool(command));
    }
    //fonction pour exécuter un dll de traitement
    if (strstr(command,"dll|")!=NULL && strstr(command,"|[")!=NULL){
        strcpy(command,process_dll(command));
    }
    //fonction pour les prompt()
    if (strstr(command,"prompt(") != NULL){
        char *temp = get_prompt(command);
        strcpy(command, temp);  // OK car temp est séparé de command
        free(temp);
    }
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
    //taille de l'écran
    if (strstr(command,"screen_size(x)")!=NULL || strstr(command,"screen_size(y)")!=NULL || strstr(command,"ecran_taille(x)")!=NULL || strstr(command,"ecran_taille(y)")!=NULL){
        char *save_size = replace_screen_dimensions(command);
        strcpy(command,save_size);
        free(save_size);
    }

    if (strstr(command,"version()")!=NULL){
        strcpy(command,handle_version(command));
    }

    if (strstr(command,"block_executed()")!=NULL){
        char *command_f=handle_block_executed(command);
        memset(command, 0, MAX_COMMAND_LEN);
        strcpy(command,command_f);
        free(command_f);
    }

    if (strstr(command,"text_eval(\"")!=NULL){
        strcpy(command,handle_text_eval(command));
    }

    if (strstr(command,"text_len(\"")!=NULL){
        strcpy(command,handle_text_len(command));
    }

    if (strstr(command,"text_count(\"")!=NULL){
        strcpy(command,handle_text_count(command));
    }

    if (strstr(command,"text_find(\"")!=NULL){
        strcpy(command,handle_text_find(command));
        //char *command_f = handle_text_find(command);
        //strcpy(command,command_f);
        //free(command_f);
    }

    //remplacer les random [,]
    handle_random(command,last_result);
    //remplacer les random(;)
    if (strstr(command,"random(")!= NULL){
        strcpy(command, handle_random2(command));
    }

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

    if (strstr(command, "table|") != NULL && strstr(command, "|[") != NULL) {
            strcpy(command,for_get_table_value(command));
            //printf("%s\n",command);
    }
    if (strstr(command, "table|") != NULL && strstr(command, "|.len(") != NULL) {
            strcpy(command,for_len_table(command));
            //printf("%s\n",command);
    }

    replace_comma_with_dot(command);

    //récupérer la variable local
    if (strstr(command,"var_local|")!=NULL){
        handle_var_local_get(command,MAX_LEN);
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
                if (error_in_stderr==false){
                    printf("Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : définir une variable dans une condition est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                    fflush(stderr);
                }
                //if (error_lock_program==true){
                    //var_script_exit=1;
                //}
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

    if (strstr(command, "[main_arg]")) {
        // Valeur à insérer : soit main_arg_global, soit "0" si vide
        const char *replacement = (main_arg_global[0] != '\0') ? main_arg_global : "0";

        char new_command[MAX_COMMAND_LEN] = {0};
        char *src = command;
        char *found;

        while ((found = strstr(src, "[main_arg]"))) {
            // Copier la partie avant [main_arg]
            strncat(new_command, src, found - src);

            // Ajouter la valeur de remplacement
            strncat(new_command, replacement, sizeof(new_command) - strlen(new_command) - 1);

            // Sauter "[main_arg]" (10 caractères)
            src = found + 10;
        }

        // Ajouter la fin de la chaîne restante
        strncat(new_command, src, sizeof(new_command) - strlen(new_command) - 1);

        // Remplacer la commande originale
        strcpy(command, new_command);
    }

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
    } else if (strncmp(expr,"if(",3) == 0 || strncmp(expr,"si(",3) == 0){
        // 1) extraire la condition entre la ( première après if et la ) correspondante
        const char *pcond = strchr(expr, '(');
        if (!pcond) { /* gestion erreur */ return; }
        const char *cond_start = pcond + 1;

        // trouver la ) qui ferme en respectant l'imbrication
        int depth = 1, in_string = 0;
        const char *s = cond_start;
        while (*s && depth > 0) {
            if (*s == '"' && (s==cond_start || *(s-1)!='\\')) in_string = !in_string;
            else if (!in_string) {
                if (*s == '(') depth++;
                else if (*s == ')') depth--;
            }
            s++;
        }
        if (depth != 0) {
            if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibré sur if( )/si( )\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : parenthèses non équilibré sur if( )/si( )\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }    /* parenthèses non fermées */
            return;
        }
        const char *cond_end = s - 1;

        // 2) évaluer la condition frc_condition_power.h
        int condition_reussie = evaluate_if_paren_condition(cond_start, cond_end);
        if (condition_reussie==-1){
            if (error_in_stderr==false){
                printf("Erreur : argument incorrect  sur if( )/si( )\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : argument incorrect sur if( )/si( )\n");
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }    /* problème d'argument sur if()*/
            return;
        }

        // Lire [[N#M]]
        char *egal = strchr(s, '{'); //egal cherche l'acolade car c'est plus sur que '=', il y en a forcement si il y a un autre block
        char *token = strstr(s, "[[");
        int n_valid = 0;
        if (token && (!egal || token < egal)) { //condition pour ignorer les [[ d'une autre condition (à l'intérieur de la première) si il n'y en a pas sur la première en utilosant le repère avant ou après le '='
            sscanf(token, "[[%d]]", &n_valid);
        } else {
            // Nouvelle logique : chercher "||else||", et compter les '{' avant
            n_valid = 0;
            const char *scan = expr;
            int depth = 0, paren_depth = 0, in_string = 0;
            const char *else_token = NULL;

            while (*scan) {
                if (*scan == '"' && (scan == expr || *(scan - 1) != '\\')) {
                    in_string = !in_string;
                }
                else if (!in_string) {
                    if (*scan == '(') paren_depth++;
                    else if (*scan == ')' && paren_depth > 0) paren_depth--;
                    else if (paren_depth == 0) {
                        if (*scan == '{') {
                            depth++;
                        } else if (*scan == '}') {
                            if (depth > 0) depth--;
                        }

                        // Détecter ||else|| seulement au niveau 0
                        if (depth == 0 &&
                        (strncmp(scan, " ||else|| ", 10) == 0 ||
                        strncmp(scan, "}||else||{", 10) == 0 ||
                        strncmp(scan, " ||else||{", 10) == 0 ||
                        strncmp(scan, "}||else|| ", 10) == 0 ||
                        strncmp(scan, " ||sinon|| ", 11) == 0||
                        strncmp(scan, "}||sinon||{", 11) == 0||
                        strncmp(scan, " ||sinon||{", 11) == 0||
                        strncmp(scan, "}||sinon|| ", 11) == 0)) {
                            else_token = scan;
                            break;
                        }
                    }
                }
                scan++;
            }

            if (else_token) {
                const char *scan2 = expr;
                int depth2 = 0;
                int paren_depth2 = 0;
                int in_string2 = 0;

                while (scan2 < else_token) {
                    if (*scan2 == '"' && (scan2 == expr || *(scan2 - 1) != '\\')) {
                        in_string2 = !in_string2;
                    }
                    else if (!in_string2) {
                        if (*scan2 == '(') {
                            paren_depth2++;
                        } else if (*scan2 == ')' && paren_depth2 > 0) {
                            paren_depth2--;
                        }
                        else if (paren_depth2 == 0) {
                            if (*scan2 == '{') {
                                if (depth2 == 0) {
                                    n_valid++; // Compter seulement si on est au niveau 0
                                }
                                depth2++;
                            } else if (*scan2 == '}') {
                                if (depth2 > 0) {
                                    depth2--;
                                }
                            }
                        }
                    }
                    scan2++;
                }
            } else {
                //compter tous les '{' si pas de [[N]] ni de ||else||
                //printf("%s\n",expr);
                // Sauvegarder la position initiale de expr
                n_valid = 0;
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
        }

        char *start = strchr(s, '{');
        if (!start) {
            if (error_in_stderr==false){
                printf("Erreur : Aucune instruction à exécuter dans une condition.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Aucune instruction à exécuter dans une condition.\n");
                fflush(stderr);
            }
            //if (error_lock_program==true){
                //var_script_exit=1;
            //}
            return;
        }

        char result[MAX_COMMAND_LEN] = "[[[";
        int open_braces = 0, valid_count = 0, else_count = 0;
        int block_started = 0;
        int skipped_blocks = 0;
        int take_all_remaining = 0;  //  Ajouté : Permet de prendre tout après `N` blocs ignorés
        char *cursor = start;

        in_string = 0;
        int paren_depth = 0;

        while (*cursor) {
            if (*cursor == '"' && (cursor == start || *(cursor - 1) != '\\')) {
                in_string = !in_string;
            }
            else if (!in_string) {
                if (*cursor == '(') paren_depth++;
                else if (*cursor == ')' && paren_depth > 0) paren_depth--;
                else if (paren_depth == 0) {
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
                }
            }

            if (block_started || take_all_remaining) {
                strncat(result, cursor, 1);
            }

            if (!in_string && paren_depth == 0) {
                if (*cursor == '}') {
                    open_braces--;
                    if (open_braces == 0) {
                        block_started = 0;
                    }
                }
            }

            cursor++;
        }

        strcat(result, "]]]");

        if (strstr(result,"[[[]]]") == NULL) {
            handle_script(result);
        }

    } else if (strncmp(expr, "if!||", 4) == 0 || strncmp(expr, "si!||", 4) == 0 || strncmp(expr, "if?||", 4) == 0 || strncmp(expr, "si?||", 4) == 0){
        //gestion des conditions if spécial
        char expr_copy[MAX_COMMAND_LEN];
        strcpy(expr_copy, expr);

        char type_condition[MAX_COMMAND_LEN] = {0};
        char *p_type=expr_copy+2; //passé le si ou if pour récupérer ! ou ?
        strncpy(type_condition,p_type,1);

        char *fin_token = strstr(expr_copy, "||"); //le début de la valeur
        //printf(token);

        char v1[MAX_COMMAND_LEN] = {0};

        // Lire `valeur2`
        char *p = fin_token +2; // Passer `||` du début de la valeur
        fin_token = strstr(p, "||[["); // Fin de la valeur 2 juste avant `[[`
        if (!fin_token) {
            fin_token = strstr(p, "||{");
            if (!fin_token){
                fin_token = strstr(p, "|| ");
                if (!fin_token) {
                    if (error_in_stderr==false){
                        printf("Erreur : format incorrect dans une condition.\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : format incorrect dans une condition.\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                    return;
                }
            }
        }
        strncpy(v1, p, fin_token - p);
        v1[fin_token - p] = '\0';

        double valeur1 = condition_script_order(v1);

        // Lire [[N#M]]
        char *egal = strchr(fin_token, '{'); //egal cherche l'acolade car c'est plus sur que '=', il y en a forcement si il y a un autre block
        char *token = strstr(fin_token, "[[");
        int n_valid = 0;
        if (token && (!egal || token < egal)) { //condition pour ignorer les [[ d'une autre condition (à l'intérieur de la première) si il n'y en a pas sur la première en utilosant le repère avant ou après le '='
            sscanf(token, "[[%d]]", &n_valid);
        } else {
            // Nouvelle logique : chercher "||else||", et compter les '{' avant
            n_valid = 0;
            const char *scan = expr;
            int depth = 0, paren_depth = 0, in_string = 0;
            const char *else_token = NULL;

            while (*scan) {
                if (*scan == '"' && (scan == expr || *(scan - 1) != '\\')) {
                    in_string = !in_string;
                }
                else if (!in_string) {
                    if (*scan == '(') paren_depth++;
                    else if (*scan == ')' && paren_depth > 0) paren_depth--;
                    else if (paren_depth == 0) {
                        if (*scan == '{') {
                            depth++;
                        } else if (*scan == '}') {
                            if (depth > 0) depth--;
                        }

                        // Détecter ||else|| seulement au niveau 0
                        if (depth == 0 &&
                        (strncmp(scan, " ||else|| ", 10) == 0 ||
                        strncmp(scan, "}||else||{", 10) == 0 ||
                        strncmp(scan, " ||else||{", 10) == 0 ||
                        strncmp(scan, "}||else|| ", 10) == 0 ||
                        strncmp(scan, " ||sinon|| ", 11) == 0||
                        strncmp(scan, "}||sinon||{", 11) == 0||
                        strncmp(scan, " ||sinon||{", 11) == 0||
                        strncmp(scan, "}||sinon|| ", 11) == 0)) {
                            else_token = scan;
                            break;
                        }
                    }
                }
                scan++;
            }

            if (else_token) {
                const char *scan2 = expr;
                int depth2 = 0;
                int paren_depth2 = 0;
                int in_string2 = 0;

                while (scan2 < else_token) {
                    if (*scan2 == '"' && (scan2 == expr || *(scan2 - 1) != '\\')) {
                        in_string2 = !in_string2;
                    }
                    else if (!in_string2) {
                        if (*scan2 == '(') {
                            paren_depth2++;
                        } else if (*scan2 == ')' && paren_depth2 > 0) {
                            paren_depth2--;
                        }
                        else if (paren_depth2 == 0) {
                            if (*scan2 == '{') {
                                if (depth2 == 0) {
                                    n_valid++; // Compter seulement si on est au niveau 0
                                }
                                depth2++;
                            } else if (*scan2 == '}') {
                                if (depth2 > 0) {
                                    depth2--;
                                }
                            }
                        }
                    }
                    scan2++;
                }
            } else {
                //compter tous les '{' si pas de [[N]] ni de ||else||
                //printf("%s\n",expr);
                // Sauvegarder la position initiale de expr
                n_valid = 0;
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
        }

        //vérifier la condition
        int condition_reussie = 0;
        if (strcmp(type_condition,"!")==0){
            if (valeur1==0){
                condition_reussie = 1;
            }
        } else if (strcmp(type_condition,"?")==0){
            if (valeur1!=0){
                condition_reussie = 1;
            }
        }

        char *start = strchr(fin_token, '{');
        if (!start) {
            if (error_in_stderr==false){
                printf("Erreur : Aucune instruction à exécuter dans une condition.\n");
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : Aucune instruction à exécuter dans une condition.\n");
                fflush(stderr);
            }
            //if (error_lock_program==true){
                //var_script_exit=1;
            //}
            return;
        }

        char result[MAX_COMMAND_LEN] = "[[[";
        int open_braces = 0, valid_count = 0, else_count = 0;
        int block_started = 0;
        int skipped_blocks = 0;
        int take_all_remaining = 0;  //  Ajouté : Permet de prendre tout après `N` blocs ignorés
        char *cursor = start;

        int in_string = 0, paren_depth = 0;

        while (*cursor) {
            if (*cursor == '"' && (cursor == start || *(cursor - 1) != '\\')) {
                in_string = !in_string;
            }
            else if (!in_string) {
                if (*cursor == '(') paren_depth++;
                else if (*cursor == ')' && paren_depth > 0) paren_depth--;
                else if (paren_depth == 0) {
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
                }
            }

            if (block_started || take_all_remaining) {
                strncat(result, cursor, 1);
            }

            if (!in_string && paren_depth == 0) {
                if (*cursor == '}') {
                    open_braces--;
                    if (open_braces == 0) {
                        block_started = 0;
                    }
                }
            }

            cursor++;
        }

        strcat(result, "]]]");

        if (strstr(result,"[[[]]]") == NULL) {
            handle_script(result);
        }
    } else if (strncmp(expr, "if||", 4) != 0 && strncmp(expr, "si||", 4) != 0) {
        execute_script_order(expr); //si il n'y a pas de if ni de repeat
        return;
    }

    if (strncmp(expr, "if||", 4) != 0 && strncmp(expr, "si||", 4) != 0) {return;}

    //gestion des conditions if
    char expr_copy[MAX_COMMAND_LEN];
    strcpy(expr_copy, expr);

    char *token = strstr(expr_copy, "||");
    //printf(token);

    char v1[MAX_COMMAND_LEN] = {0}, operateur[3] = {0}, v2[MAX_COMMAND_LEN] = {0};

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
                    if (!fin_token || (egal && egal < fin_token)) {
                            if (error_in_stderr==false){
                                printf("Erreur : format incorrect dans une condition.\n");
                                fflush(stdout);
                            } else {
                                fprintf(stderr,"Erreur : format incorrect dans une condition.\n");
                                fflush(stderr);
                            }
                            if (error_lock_program==true){
                                var_script_exit=1;
                            }
                            return;
                    }
            }
        }
    }
    strncpy(v1, p, fin_token - p);
    v1[fin_token - p] = '\0';

    // Lire `operateur`
    p = fin_token + 2; // Passer `||`
    fin_token = strstr(p, "||");
    if (!fin_token) {
        if (error_in_stderr==false){
            printf("Erreur : format incorrect dans une condition.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : format incorrect dans une condition.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }
    strncpy(operateur, p, fin_token - p);
    operateur[fin_token - p] = '\0';

    // Lire `valeur2`
    p = fin_token +2; // Passer `||`
    fin_token = strstr(p, "||[["); // Fin de la valeur 2 juste avant `[[`
    if (!fin_token) {
            fin_token = strstr(p, "||{");
            if (!fin_token){
                fin_token = strstr(p, "|| ");
                if (!fin_token) {
                    if (error_in_stderr==false){
                        printf("Erreur : format incorrect dans une condition.\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : format incorrect dans une condition.\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                    return;
                }
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
        // Nouvelle logique : chercher "||else||", et compter les '{' avant
        n_valid = 0;
        const char *scan = expr;
        int depth = 0, paren_depth = 0, in_string = 0;
        const char *else_token = NULL;

        while (*scan) {
            if (*scan == '"' && (scan == expr || *(scan - 1) != '\\')) {
                in_string = !in_string;
            }
            else if (!in_string) {
                if (*scan == '(') paren_depth++;
                else if (*scan == ')' && paren_depth > 0) paren_depth--;
                else if (paren_depth == 0) {
                    if (*scan == '{') {
                        depth++;
                    } else if (*scan == '}') {
                        if (depth > 0) depth--;
                    }

                    // Détecter ||else|| seulement au niveau 0
                    if (depth == 0 &&
                    (strncmp(scan, " ||else|| ", 10) == 0 ||
                     strncmp(scan, "}||else||{", 10) == 0 ||
                     strncmp(scan, " ||else||{", 10) == 0 ||
                     strncmp(scan, "}||else|| ", 10) == 0 ||
                     strncmp(scan, " ||sinon|| ", 11) == 0||
                     strncmp(scan, "}||sinon||{", 11) == 0||
                     strncmp(scan, " ||sinon||{", 11) == 0||
                     strncmp(scan, "}||sinon|| ", 11) == 0)) {

                        else_token = scan;
                        break;
                    }
                }
            }
            scan++;
        }

        if (else_token) {
            const char *scan2 = expr;
            int depth2 = 0;
            int paren_depth2 = 0;
            int in_string2 = 0;

            while (scan2 < else_token) {
                if (*scan2 == '"' && (scan2 == expr || *(scan2 - 1) != '\\')) {
                    in_string2 = !in_string2;
                }
                else if (!in_string2) {
                    if (*scan2 == '(') {
                        paren_depth2++;
                    } else if (*scan2 == ')' && paren_depth2 > 0) {
                        paren_depth2--;
                    }
                    else if (paren_depth2 == 0) {
                        if (*scan2 == '{') {
                            if (depth2 == 0) {
                                n_valid++; // Compter seulement si on est au niveau 0
                            }
                            depth2++;
                        } else if (*scan2 == '}') {
                            if (depth2 > 0) {
                                depth2--;
                            }
                        }
                    }
                }
                scan2++;
            }
        } else {
            //compter tous les '{' si pas de [[N]] ni de ||else||
            //printf("%s\n",expr);
            // Sauvegarder la position initiale de expr
            n_valid = 0;
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
    }
    //printf("%d\n",n_valid);

    // Vérifier la condition
    int condition_reussie = evaluate_condition(valeur1, operateur, valeur2);

    char *start = strchr(expr, '{');
    if (!start) {
        if (error_in_stderr==false){
            printf("Erreur : Aucune instruction à exécuter dans une condition.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : Aucune instruction à exécuter dans une condition.\n");
            fflush(stderr);
        }
        //if (error_lock_program==true){
            //var_script_exit=1;
        //}
        return;
    }

    char result[MAX_COMMAND_LEN] = "[[[";
    int open_braces = 0, valid_count = 0, else_count = 0;
    int block_started = 0;
    int skipped_blocks = 0;
    int take_all_remaining = 0;  //  Ajouté : Permet de prendre tout après `N` blocs ignorés
    char *cursor = start;

    int in_string = 0, paren_depth = 0;

    while (*cursor) {
        if (*cursor == '"' && (cursor == start || *(cursor - 1) != '\\')) {
            in_string = !in_string;
        }
        else if (!in_string) {
            if (*cursor == '(') paren_depth++;
            else if (*cursor == ')' && paren_depth > 0) paren_depth--;
            else if (paren_depth == 0) {
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
            }
        }

        if (block_started || take_all_remaining) {
            strncat(result, cursor, 1);
        }

        if (!in_string && paren_depth == 0) {
            if (*cursor == '}') {
                open_braces--;
                if (open_braces == 0) {
                    block_started = 0;
                }
            }
        }

        cursor++;
    }

    strcat(result, "]]]");

    if (strstr(result,"[[[]]]") == NULL) {
        handle_script(result);
    }
}

//supprimé les commentaire $// et //$
char *handle_commentary(const char *program) {
    if (!program) return NULL;

    size_t buf_size = strlen(program) + 1;   // taille de départ (au moins la taille d'origine)
    char *output = malloc(buf_size);
    if (!output) return NULL;

    const char *p = program;
    size_t out_len = 0;
    output[0] = '\0';

    while (*p) {
        const char *start = strstr(p, "$//");
        const char *end   = strstr(p, "//$");

        if (!start && !end) {
            // plus de commentaire -> recopier le reste
            size_t len = strlen(p);
            if (out_len + len + 1 > buf_size) {
                buf_size = out_len + len + 1;
                output = realloc(output, buf_size);
                if (!output) return NULL;
            }
            strcpy(output + out_len, p);
            break;
        }

        if (end && (!start || end < start)) {
            // trouvé //$ avant $// -> erreur
            free(output);
            return strdup("0");
        }

        if (start) {
            // recopier tout ce qui est avant $//
            size_t len = start - p;
            if (out_len + len + 1 > buf_size) {
                buf_size = out_len + len + 1;
                output = realloc(output, buf_size);
                if (!output) return NULL;
            }
            strncat(output, p, len);
            out_len += len;
            output[out_len] = '\0';

            if (!end) {
                // pas de //$ après -> erreur
                free(output);
                return strdup("0");
            }

            // sauter le commentaire
            p = end + 3; // avancer après "//$"
        }
    }

    return output;  // ! doit être free() par l'appelant
}

void handle_script(char *expr) {
    if (strncmp(expr, "[[[", 3) != 0) {
        if (error_in_stderr==false){
            printf("Erreur : format de script invalide.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : format de script invalide.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return;
    }

    //vérifier si il y a eut un exit dans un block précédent
    if (var_script_exit==1){
        return;
    }

    //Nettoyage des commentaires $// et //$
    char *cleaned_expr = handle_commentary(expr);

    // si erreur -> handle_commentary renvoie "0"
    if (strcmp(cleaned_expr, "0") == 0) {
        if (error_in_stderr==false){
            printf("Erreur : commentaire invalide : doit être sous la forme $// et //$\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : commentaire invalide : doit être sous la forme $// et //$\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        free(cleaned_expr);
        return;
    }

    strcpy(expr,cleaned_expr);
    free(cleaned_expr);

    //trouvé aumoins le début d'un bloc pour savoir si c'est bien un script
    char *start = strstr(expr, "{");
    if (!start){
        if (error_in_stderr==false){
            printf("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        if (socket_var == 1) {
            send(client, "Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n", strlen("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n"), 0); //erreur courante donc gérer par le socket aussi
        }
        return;
    }
    int depth = 0;
    char *p = start;
    char *end_script = NULL;

    int paren_depth = 0; //pour ({)
    int in_string = 0; // pour "{"

    while (*p) {
        // Bascule dans/ hors d'une chaîne
        if (*p == '"' && (p == start || *(p - 1) != '\\')) {
            in_string = !in_string;
        }
        else if (!in_string) {
            // Comptage des parenthèses
            if (*p == '(') paren_depth++;
            else if (*p == ')' && paren_depth > 0) paren_depth--;
            // Seulement si on n'est pas dans une parenthèse
            else if (paren_depth == 0) {
            if (strncmp(p, "[[[", 3) == 0 && depth == 0) {
                // Nouveau [[[ trouvé hors des accolades => invalide ex: si [[[{}[[[{}]]] erreur
                end_script = NULL;
                if (error_in_stderr==false){
                    printf("Erreur : script '[[[' imbriqué détecté sans fermeture du précédent.(']]]')\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : script '[[[' imbriqué détecté sans fermeture du précédent.(']]]')\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                return;
            }

            if (*p == '{') {
                depth++;
            } else if (*p == '}') {
                if (depth > 0) depth--;
            } else if (strncmp(p, "]]]", 3) == 0 && depth == 0) { //ignore les ']]]' de fin dans un {}
                end_script = p;
                break;
            }
            }
        }
        p++;
    }

    if (!start || !end_script) {
        if (error_in_stderr==false){
            printf("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        if (socket_var == 1) {
            send(client, "Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n", strlen("Erreur : syntaxe incorrecte, {{{ ou ]]] manquant.\n"), 0); //erreur courante donc gérer par le socket aussi
        }
        return;
    }

    // Vérifier [[[{{ … }}]]] pour du multi block imbriqué (souvent à cause des condition ou repeat pour être plus souple)
    if (strncmp(expr, "[[[{{", 5) == 0 && strncmp(end_script - 2, "}}]]]", 4) == 0) {
        int depth_check = 0;
        char *p_check = expr + 4; // commencer après le premier '{'
        int remove_braces = 1;    // hypothèse : on peut enlever les accolades

        paren_depth = 0;
        in_string = 0;

        while (p_check < end_script - 2) { // ignorer le dernier '}'
            if (*p_check == '"' && (p_check == expr + 4 || *(p_check - 1) != '\\')) {
                // bascule dans/ hors de la chaîne (en ignorant les \")
                in_string = !in_string;
            }
            else if (!in_string) {
                if (*p_check == '(') paren_depth++;
                else if (*p_check == ')' && paren_depth > 0) paren_depth--;
                else if (paren_depth == 0) { // seulement si on est hors parenthèses
                    if (*p_check == '{') depth_check++;
                    else if (*p_check == '}') {
                        depth_check--;
                        if (depth_check < 0) {
                            remove_braces = 0; // on a atteint niveau 0 avant la fin, ne pas supprimer
                            break;
                        }
                    }
                }
            }
            p_check++;
        }

        if (remove_braces) {
            // Décaler le début pour sauter le premier '{'
            memmove(expr + 3, expr + 4, strlen(expr + 4) + 1); // +1 pour '\0'
            end_script--; // réduire la fin pour supprimer le dernier '}'
            *(end_script - 1) = *(end_script); // décaler le '\0' si nécessaire
        }
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

        paren_depth = 0;
        in_string = 0;

        while (*cursor && cursor < end_script) {
            if (*cursor == '"' && (cursor == start + 1 || *(cursor - 1) != '\\')) {
                in_string = !in_string;
            }
            else if (!in_string) {
                if (*cursor == '(') paren_depth++;
                else if (*cursor == ')' && paren_depth > 0) paren_depth--;
                else if (paren_depth == 0) {
                    if (*cursor == '{') {
                        open_braces++;
                    } else if (*cursor == '}') {
                        open_braces--;
                        if (open_braces == 0) {
                            break; // Fin de l'ordre trouvé
                        }
                    }
                }
            }
            order[index++] = *cursor;
            cursor++;
        }

        order[index] = '\0'; // Terminaison de chaîne correcte

        //zone pour suppr les espaces au début d'un bloc si il y en a
        char *p_order = order;
        // Avancer tant que ce sont des espaces et qu'il reste des caractères
        while (*p_order == ' ' && *(p_order + 1) != '\0') {
            p_order++;
        }
        // Décaler la chaîne vers la gauche
        if (p_order != order) {
            memmove(order, p_order, strlen(p_order) + 1); // +1 pour copier le '\0'
        }

        block_execute++; //exécution d'un nouveau block

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
            char *end_pause = NULL;

            if (start_pause) {
                int count = 1;
                char *p = start_pause + 1;

                while (*p && count > 0) {
                    if (*p == '(') count++;
                    else if (*p == ')') count--;
                    if (count == 0) {
                        end_pause = p;
                        break;
                    }
                    p++;
                }

                if (end_pause) {
                    size_t len = end_pause - start_pause - 1;
                    char content[1024] = {0};
                    strncpy(content, start_pause + 1, len);
                    content[len] = '\0';
                } else {
                    if (error_in_stderr==false){
                        printf("Erreur : format de pause/break invalide.\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : format de pause/break invalide.\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                }
            }

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
                if (error_in_stderr==false){
                    printf("Erreur : format de pause/break invalide.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : format de pause/break invalide.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
            }
        }

        //printf("var pause: %d\n",var_script_pause);

        //vérifier si on à fait une demande de sortir d'un nombre de boucle et condition
        if (var_script_pause>0){
            var_script_pause=var_script_pause-1;
            return;
        }

        //vérifier si il y a eut un exit dans un block précédent
        if (var_script_exit==1){
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

//fonction pour supprimé la table du fichier table||=suppr()
int suppr_save_table(const char *input_line) {
    //printf(">> Input: %s\n", input_line);

    if (strstr(input_line, "|=suppr()") == NULL) {
        //printf("[DEBUG] Pas de |=suppr() trouvé.\n");
        return 0;
    }

    // Extraire nom entre table| et | before =suppr()
    char nom[1000];
    const char *table_pos = strstr(input_line, "table|");
    if (!table_pos) {
        //printf("[DEBUG] 'table|' non trouvé.\n");
        return -1;
    }

    const char *mid_bar = strchr(table_pos + 6, '|');
    if (!mid_bar) {
        //printf("[DEBUG] Deuxième '|' non trouvé.\n");
        return -1;
    }

    size_t len = mid_bar - (table_pos + 6);
    strncpy(nom, table_pos + 6, len);
    nom[len] = '\0';

    //printf("[DEBUG] Nom à supprimer : '%s'\n", nom);

    // Ouvrir table_frc.txt et charger en mémoire
    char filePath[MAX_PATH];
    GetModuleFileNameA(NULL, filePath, MAX_PATH);
    char *lastSlash = strrchr(filePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(filePath, "table_frc.txt");

    FILE *file = fopen(filePath, "r");
    if (!file) {
        //printf("[DEBUG] Fichier table_frc.txt introuvable.\n");
        return -1;
    }

    // Lire et stocker les lignes dynamiquement
    char **lines = NULL;
    size_t line_count = 0;
    size_t capacity = 0;

    char *line = NULL;
    size_t bufsize = 0;

    while (getline_win(&line, &bufsize, file) != -1) {
        if (line_count >= capacity) {
            capacity = capacity ? capacity * 2 : 128;
            lines = realloc(lines, capacity * sizeof(char*));
            if (!lines) {
                free(line);
                fclose(file);
                return -1;
            }
        }
        lines[line_count++] = strdup(line);
    }

    free(line);
    fclose(file);

    // Réécriture sans la ligne à supprimer
    file = fopen(filePath, "w");
    if (!file) {
        for (size_t i = 0; i < line_count; ++i) free(lines[i]);
        free(lines);
        return -1;
    }

    int supprime = 0;
    for (size_t i = 0; i < line_count; ++i) {
        if (strncmp(lines[i], nom, strlen(nom)) == 0 && lines[i][strlen(nom)] == '=') {
            supprime = 1;
            free(lines[i]);
            continue;
        }
        if (lines[i][0] != '\n' && lines[i][0] != '\0'){
            fputs(lines[i], file);
        }
        free(lines[i]);
    }

    free(lines);
    fclose(file);
    //printf("[DEBUG] Suppression %s.\n", supprime ? "effectuée" : "non nécessaire");
    return 1;
}

//fonction pour join les 2 tables en une table table|i|=join(table|i|;table|j|)
int join_save_table(const char *input_line) {
    //printf(">> Input: %s\n", input_line);

    if (strstr(input_line, "|=join(") == NULL) {
        //printf("[DEBUG] Pas de |=join( trouvé.\n");
        return 0;
    }

    char line_copy[MAX_COMMAND_LEN];
    strcpy(line_copy, input_line);

    char *join_pos = strstr(line_copy, "|=join(");
    char var_name[1000];
    char *equal_sign = strchr(line_copy, '=');
    if (!equal_sign) {
        //printf("[DEBUG] '=' introuvable ou mal positionné.\n");
        return -1;
    }

    char *table_pos = strstr(line_copy, "table|");
    if (!table_pos || table_pos > equal_sign) {
        //printf("[DEBUG] 'table|' introuvable ou mal positionné.\n");
        return -1;
    }

    strncpy(var_name, table_pos + 6, equal_sign - table_pos - 7);
    var_name[equal_sign - table_pos - 6] = '\0';
    //printf("[DEBUG] Nom de la variable cible: '%s'\n", var_name);

    char *args_start = strchr(join_pos, '(');
    char *args_end = strrchr(join_pos, ')');
    if (!args_start || !args_end || args_end <= args_start) {
        //printf("[DEBUG] Parenthèses de join() incorrectes.\n");
        return -1;
    }

    char args[MAX_COMMAND_LEN];
    strncpy(args, args_start + 1, args_end - args_start - 1);
    args[args_end - args_start - 1] = '\0';
    //printf("[DEBUG] Arguments de join: '%s'\n", args);

    char nom1[1000], nom2[1000];
    if (sscanf(args, "table|%[^|]|;table|%[^|]|", nom1, nom2) != 2) {
        //printf("[DEBUG] Erreur parsing des deux noms.\n");
        return -1;
    }

    //printf("[DEBUG] nom1: '%s', nom2: '%s'\n", nom1, nom2);

    // Chemin du fichier
    char filePath[MAX_PATH];
    GetModuleFileNameA(NULL, filePath, MAX_PATH);
    char *lastSlash = strrchr(filePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(filePath, "table_frc.txt");

    //printf("[DEBUG] Chemin du fichier: %s\n", filePath);

    FILE *file = fopen(filePath, "r");
    if (!file) {
        //printf("[DEBUG] Impossible d’ouvrir le fichier table_frc.txt.\n");
        return -1;
    }

    char **lines = NULL;
    int line_count = 0;
    size_t allocated_lines = 0;
    char data1[MAX_COMMAND_LEN] = "", data2[MAX_COMMAND_LEN] = "";
    char buffer[MAX_COMMAND_LEN];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (line_count >= allocated_lines) {
            allocated_lines = allocated_lines == 0 ? 128 : allocated_lines * 2;
            lines = realloc(lines, allocated_lines * sizeof(char *));
            if (!lines) {
                fclose(file);
                return -1; // Erreur d'allocation mémoire
            }
        }
        lines[line_count] = strdup(buffer); // copie la ligne
        if (!lines[line_count]) {
            fclose(file);
            return -1; // Erreur de strdup
        }
        char *line = lines[line_count];

        if (strncmp(line, nom1, strlen(nom1)) == 0 && line[strlen(nom1)] == '=') {
            char *val = strchr(line, '=');
            if (val) {
                val++;
                size_t len = strlen(val);
                if (len >= 3) {
                    strncpy(data1, val, len - 3);
                    data1[len - 3] = '\0'; // remove ]]
                    //printf("[DEBUG] Contenu de '%s' (data1): %s\n", nom1, data1);
                }
            }
        }

        if (strncmp(line, nom2, strlen(nom2)) == 0 && line[strlen(nom2)] == '=') {
            char *val = strchr(line, '=');
            if (val) {
                val++;
                char *start_data = strstr(val, "[[");
                if (start_data) {
                    strcpy(data2, start_data + 2); // remove [[
                    char *newline = strchr(data2, '\n');
                    if (newline) *newline = '\0';
                    //printf("[DEBUG] Contenu de '%s' (data2): %s\n", nom2, data2);
                }
            }
        }

        line_count++;
    }
    fclose(file);

    if (strlen(data1) == 0 || strlen(data2) == 0) {
        //printf("[DEBUG] Données manquantes : data1 ou data2 vide.\n");
        for (int i = 0; i < line_count; ++i) {
            free(lines[i]);
        }
        free(lines);
        return -1;
    }

    size_t total_len = strlen(data1) + strlen(data2) + 2; // +1 for ';' +1 for '\0'
    char *final_data = malloc(total_len);
    if (!final_data) {
        // handle error
        for (int i = 0; i < line_count; ++i) {
            free(lines[i]);
        }
        free(lines);
        if (error_in_stderr==false){
            printf("Erreur : mémoire sur table||=join()\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : mémoire sur table||=join()\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return -1;
    }
    snprintf(final_data, total_len, "%s;%s", data1, data2);
    //printf("[DEBUG] Chaîne finale assemblée: %s\n", final_data);

    file = fopen(filePath, "w");
    if (!file) {
        //printf("[DEBUG] Erreur en réouvrant le fichier pour écriture.\n");
        for (int i = 0; i < line_count; ++i) {
            free(lines[i]);
        }
        free(lines);
        return -1;
    }

    int found = 0;
    for (int i = 0; i < line_count; ++i) {
        if (strncmp(lines[i], var_name, strlen(var_name)) == 0 && lines[i][strlen(var_name)] == '=') {
            fprintf(file, "%s=%s\n", var_name, final_data);
            found = 1;
            //printf("[DEBUG] Ligne mise à jour: %s=%s\n", var_name, final_data);
        } else {
            fputs(lines[i], file);
        }
    }

    if (!found) {
        fprintf(file, "%s=%s\n", var_name, final_data);
        //printf("[DEBUG] Ligne ajoutée à la fin: %s=%s\n", var_name, final_data);
    }

    fclose(file);
    //printf("[DEBUG] Opération terminée avec succès.\n");

    for (int i = 0; i < line_count; ++i) {
        free(lines[i]);
    }
    free(lines);
    return 1;
}


//fonction pour table=[[;;]]
int parse_and_save_table(const char *input_line) {
    char table_name[MAX_COMMAND_LEN];
    const char *start = strstr(input_line, "table|");
    if (!start) return 0;

    start += 6;
    const char *sep = strchr(start, '|');
    if (!sep) return 0;

    strncpy(table_name, start, sep - start);
    table_name[sep - start] = '\0';

    if (strpbrk(table_name, "[]={}\"")) {
        if (error_in_stderr==false){
            printf("Erreur : le nom de la table contient un caractère interdit : [ ] = { } \"\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : le nom de la table contient un caractère interdit : [ ] = { } \"\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return 0;
    }

    if (strstr(input_line, "|=join(")!=NULL){
            int resultat=join_save_table(input_line);
            return resultat;
    }

    if (strstr(input_line, "|=suppr()")!=NULL){
            int resultat=suppr_save_table(input_line);
            return resultat;
    }

    const char *equal = strstr(input_line, "=[[");
    if (!equal) return 0;

    equal += 3;
    const char *p = equal;
    int open_brackets = 1;
    //int first = 1;

    int paren_level = 0;
    while (*p && open_brackets > 0) {
        if (*p == '(') {
            paren_level++;
            p++;
        } else if (*p == ')') {
            if (paren_level > 0) paren_level--;
            else return 4; // parenthèse fermante sans ouverture
            p++;
        } else if (paren_level == 0 && strncmp(p, "[[", 2) == 0) {
            // doit être précédé de ';' ou '['
            if (p != equal) {
                char prev = *(p - 1);
                if (prev != ';' && prev != '[') return 3;
            }
            open_brackets++;
            p += 2;
        } else if (paren_level == 0 && strncmp(p, "]]", 2) == 0) {
            char next = *(p + 2);
            if (next != ';' && next != ']' && next != '\0') return 3;
            open_brackets--;
            p += 2;
        } else {
            p++;
        }
    }

    if (open_brackets != 0) return 2;
    if (paren_level != 0) return 4; // erreur : parenthèses mal fermées

    const char *end = p - 2;

    const char *start_val = equal;
    // Compter les ']' restants à la fin
    int count = 0;
    const char *scan = end;
    while (scan >= start_val && *scan == ']') {
        count++;
        scan++;
    }

    // Si le nombre est impair, inclure le dernier ']' dans values_part
    int rest = count % 2;
    if (rest == 1) {
        end++;
    }

    char values_part[MAX_COMMAND_LEN];
    strncpy(values_part, equal, end - equal);
    values_part[end - equal] = '\0';

    // Obtenir le chemin de l'exécutable
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char *lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    // Chemin du fichier principal
    char dataFile[MAX_PATH];
    strcpy(dataFile, exePath);
    strcat(dataFile, "table_frc.txt");

    // Chemin du fichier temporaire
    char tmpFile[MAX_PATH];
    strcpy(tmpFile, exePath);
    strcat(tmpFile, "table_frc.tmp");

    FILE *f = fopen(dataFile, "r");
    FILE *tmp = fopen(tmpFile, "w");
    if (!tmp) return 0;

    int found = 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if (f) {
        while ((read = getline_win(&line, &len, f)) != -1) {
            if (strncmp(line, table_name, strlen(table_name)) == 0 && line[strlen(table_name)] == '=') {
                fprintf(tmp, "%s=[[%s]]\n", table_name, values_part);
                found = 1;
            } else {
                fputs(line, tmp);
            }
        }
        fclose(f);
    }

    if (!found) {
        fprintf(tmp, "%s=[[%s]]\n", table_name, values_part);
    }

    free(line);
    fclose(tmp);

    // Remplacer l'ancien fichier par le nouveau
    remove(dataFile);
    rename(tmpFile, dataFile);

    return 1;
}

//fonction pour récupérer l'index la valeur voulu dans table||=[[;]] (utiliser table||[index][index]...)
char* get_table_value(const char *recup) {
    static char result[MAX_COMMAND_LEN] = "0.00"; //ajout de static si on veut conserver en cas d'érreur la valeur précédente

    if (strncmp(recup, "table|", 6) != 0) {
        printf("Erreur : la chaîne ne commence pas par 'table|'\n");
        fflush(stdout);
        return result;
    }

    char table_name[MAX_COMMAND_LEN];
    const char *name_start = recup + 6;
    const char *sep = strchr(name_start, '|');
    if (!sep) {
        if (error_in_stderr==false){
            printf("Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return result;
    }

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Extraire les index [1][2]...
    int indexes[MAX_COMMAND_LEN], index_count = 0;
    const char *p = sep;
    while (*p) {
        if (*p == '[') {
            p++; // Skip initial '['
            char expr[MAX_COMMAND_LEN] = {0};
            int i = 0;

            int bracket_depth = 1;  // We’ve just passed one '['

            while (*p && bracket_depth > 0) {
                if (*p == '[') bracket_depth++;
                else if (*p == ']') bracket_depth--;

                if (bracket_depth > 0)  // Don't copy closing ']'
                    expr[i++] = *p;

                p++;
            }

            if (bracket_depth != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : crochet fermant manquant dans les indices sur table.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : crochet fermant manquant dans les indices sur table.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }

            expr[i] = '\0';

            double dval = condition_script_order(expr);
            int idx = (int)dval-1;

            if (idx < 0) {
                if (error_in_stderr==false){
                    printf("Erreur : index calculé négatif (%d) pour l'expression [%s]\n", idx, expr);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index calculé négatif (%d) pour l'expression [%s]\n", idx, expr);
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }

            indexes[index_count++] = idx;
        } else {
            p++;
        }
    }

    // Chemin de l'exécutable
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char *lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(exePath, "table_frc.txt");

    FILE *f = fopen(exePath, "r");
    if (!f) {
        if (error_in_stderr==false){
            printf("Erreur : fichier table_frc.txt introuvable.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : fichier table_frc.txt introuvable.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    char line[MAX_COMMAND_LEN];
    char *found = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, table_name, strlen(table_name)) == 0 && line[strlen(table_name)] == '=') {
            found = strchr(line, '=');
            break;
        }
    }
    fclose(f);

    if (!found || !strstr(found, "[[")) {
        if (error_in_stderr==false){
            printf("Erreur : table '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : table '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

     // Trouver la vraie fin de la table en équilibrant [[ et ]]
    found = strstr(found, "[[") + 2;
    char *table_start = found;
    int open_count = 1;
    int paren__level = 0;  // compteur de parenthèses
    char *ptr = table_start;

    while (*ptr && open_count > 0) {
    if (*ptr == '(') {
        paren__level++;
        ptr++;
    } else if (*ptr == ')') {
        if (paren__level > 0) paren__level--;
        ptr++;
    } else if (paren__level == 0 && strncmp(ptr, "[[", 2) == 0) {
        // Compter toutes les '[' consécutives
        int count = 0;
        while (ptr[count] == '[') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        open_count += pairs;   // chaque paire "[[" ouvre
        ptr += 2 * pairs;

        if (rest == 1) {
            // laisser un '[' dans le texte
            ptr++;
        }

    } else if (paren__level == 0 && strncmp(ptr, "]]", 2) == 0) {
        // Compter toutes les ']' consécutives
        int count = 0;
        while (ptr[count] == ']') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        open_count -= pairs;
        if (open_count < 0) open_count = 0;  // sécurité
        ptr += 2 * pairs;

        if (rest == 1) {
            // laisser un ']' dans le texte
            ptr++;
        }

    } else {
        ptr++;
    }
}

    if (open_count != 0) {
        if (error_in_stderr==false){
            printf("Erreur : la table '%s' est mal formée (déséquilibre [[ et ]]).\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : la table '%s' est mal formée (déséquilibre [[ et ]]).\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    ptr[0] = '\0'; // On termine proprement la chaîne

    int paren_level=0; //pour le compteur de parenthèse
    char *current = found;
    for (int i = 0; i < index_count; i++) {
        int level = 0, current_index = 0;
        char *start = current;
        char *scan = start;

        while (*scan) {
    if (strncmp(scan, "[[", 2) == 0) {
        int count = 0;
        while (scan[count] == '[') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        level += pairs;     // chaque paire "[[" augmente le niveau
        scan += 2 * pairs;

        if (rest == 1) {
            // laisser un '[' dans le texte
            scan++;
        }

    } else if (strncmp(scan, "]]", 2) == 0) {
        int count = 0;
        while (scan[count] == ']') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        level -= pairs;
        if (level < 0) level = 0;  // sécurité
        scan += 2 * pairs;

        if (rest == 1) {
            // laisser un ']' dans le texte
            scan++;
        }

    } else if (*scan == ';' && level == 0) {
        if (current_index == indexes[i]) break;
        current_index++;
        start = scan + 1;
        scan++;
    } else if (*scan == '(') {
        // Ignorer les ; dans les parenthèses
        paren_level = 1;
        scan++;
        while (*scan && paren_level > 0) {
            if (*scan == '(') paren_level++;
            else if (*scan == ')') paren_level--;
            scan++;
        }
    } else {
        scan++;
    }
}

        if (current_index != indexes[i] && *scan == '\0') {
            if (error_in_stderr==false){
                printf("Erreur : index %d hors limites dans la table '%s'.\n", indexes[i] + 1, table_name);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : index %d hors limites dans la table '%s'.\n", indexes[i] + 1, table_name);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(result, "0.00");
            return result;
        }

        // Trouver fin de l'élément
        // Trouver fin de l'élément avec gestion des [[ ]], des restes et des parenthèses
int sub_level = 0;       // niveau des [[ ]]
int paren_level = 0;     // niveau des ()
char *end_elem = start;

while (*end_elem) {
    // Gestion des crochets ouvrants [[
    if (strncmp(end_elem, "[[", 2) == 0 && paren_level == 0) {
        int count = 0;
        while (end_elem[count] == '[') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        sub_level += pairs;
        end_elem += 2 * pairs;

        if (rest == 1) {
            // laisser un '[' dans le texte
            end_elem++;
        }
    }
    // Gestion des crochets fermants ]]
    else if (strncmp(end_elem, "]]", 2) == 0 && paren_level == 0) {
        int count = 0;
        while (end_elem[count] == ']') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        if (sub_level > 0) sub_level -= pairs;
        else break;  // fin de l'élément si sub_level == 0

        end_elem += 2 * pairs;
        if (rest == 1) {
            // laisser un ']' dans le texte
            end_elem--;
        }
    }
    // Gestion des points-virgules hors [[ ]] et hors ()
    else if (*end_elem == ';' && sub_level == 0 && paren_level == 0) {
        break;
    }
    // Gestion des parenthèses
    else if (*end_elem == '(') {
        paren_level++;
        end_elem++;
        while (*end_elem && paren_level > 0) {
            if (*end_elem == '(') paren_level++;
            else if (*end_elem == ')') paren_level--;
            end_elem++;
        }
    }
    else {
        end_elem++;
    }
}

        char saved = *end_elem;
        *end_elem = '\0';
        current = start;
        if (strncmp(current, "[[", 2) == 0) current += 2;
        *end_elem = saved;
    }

    strncpy(result, current, sizeof(result) - 1);
    result[sizeof(result) - 1] = '\0';
    return result;
}

//pour get_table_value() remplacer tout les appels dans une chaine table||[][]
char* for_get_table_value(char *arg) {
    static char output[MAX_COMMAND_LEN * 2];
    output[0] = '\0';

    const char *p = arg;
    char *out = output;

    while (*p) {
        const char *match = strstr(p, "table|");
        if (!match) {
            strcpy(out, p);
            break;
        }

        // Copier tout avant le match
        while (p < match) *out++ = *p++;
        const char *start = match;

        // Trouver le nom de la table (jusqu’au 2e pipe)
        const char *pipe2 = strchr(start + 6, '|');
        if (!pipe2) {
            strcpy(out, p);
            break;
        }

        const char *scan = pipe2 + 1;
        if (*scan != '[') {
            // Pas d’index = ne pas traiter
            while (p <= pipe2) *out++ = *p++;
            continue;
        }

        // Début de parsing des index [..] imbriqués
        const char *expr_end = scan;
        int bracket_depth = 0;

        while (*expr_end) {
            if (*expr_end == '[') {
                bracket_depth++;
                expr_end++;
            } else if (*expr_end == ']') {
                bracket_depth--;
                expr_end++;

                // Si on a fermé tous les crochets, on regarde si un nouveau [ suit
                if (bracket_depth == 0) {
                    const char *test = expr_end;
                    while (*test == ' ' || *test == '\t') test++;
                    if (*test != '[') break;  // pas d'autre bloc
                }
            } else {
                expr_end++;
            }
        }

        if (bracket_depth != 0) {
            if (error_in_stderr==false){
                printf("Erreur : déséquilibre de crochets dans : %s\n", start);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : déséquilibre de crochets dans : %s\n", start);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(out, p);
            return output;
        }

        // Extraire l'expression complète à envoyer à get_table_value
        int len = expr_end - start;
        char full_expr[MAX_COMMAND_LEN] = {0};
        strncpy(full_expr, start, len);
        full_expr[len] = '\0';

        char *val = get_table_value(full_expr);

        //printf("value table : %s\n",val);

        // Tronquer val au premier ; trouvé en dehors des () car sinon, la fonction renvoie tout ce qui suit est ne m'intéresse pas
        char clean_val[MAX_COMMAND_LEN];
        int paren = 0;
        int i = 0;
        while (val[i]) {
            if (val[i] == '(') paren++;
            else if (val[i] == ')') paren--;
            else if (val[i] == ';' && paren == 0) {
                // remplacer le ; par \0 et arrêter la copie
                clean_val[i] = '\0';
                break;
            } else {
                clean_val[i] = val[i];
            }
            i++;
        }
        clean_val[i] = '\0';

        // Si les deux derniers caractères sont ]] et que la chaîne est assez longue, les enlever
        // Supprimer uniquement les ]] parasites si et seulement si
        // on est vraiment en fin de liste/table (et pas un texte contenant ])
        int len_clean_val = strlen(clean_val);
        if (len_clean_val >= 2) {
            // Vérifier qu’il n’y a rien après dans "val"
            const char *after = clean_val + i;
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;

            if (*after == '\0') {
                // Boucler tant qu'on trouve des "]]" à la fin
                while (len_clean_val >= 2 &&
                    clean_val[len_clean_val - 2] == ']' &&
                    clean_val[len_clean_val - 1] == ']') {
                    clean_val[len_clean_val - 2] = '\0'; // coupe deux caractères
                    len_clean_val -= 2;
                }
            }
        }

        strcpy(out, clean_val);
        out += strlen(clean_val);
        p = expr_end; // Continuer juste après l'expression traitée
    }

    return output;
}

//pour len_table (table||.len([1])) extrait la table à compter par len_table
char* extract_table_len(char *val) {
    static char subtable[MAX_COMMAND_LEN];
    strcpy(subtable, ""); // Reset

    if (strncmp(val, "table|", 6) != 0) return NULL;

    // Extraire nom de table
    char *table_start = val + 6;
    char *pipe = strchr(table_start, '|');
    if (!pipe) return NULL;

    char table_name[128] = {0};
    strncpy(table_name, table_start, pipe - table_start);

    // Lire fichier
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char *lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(exePath, "table_frc.txt");

    FILE *f = fopen(exePath, "r");
    if (!f) return NULL;

    char line[MAX_COMMAND_LEN];
    char *found = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, table_name, strlen(table_name)) == 0 && line[strlen(table_name)] == '=') {
            found = strstr(line, "[[");
            break;
        }
    }
    fclose(f);
    if (!found) return NULL;

    // Copier la table
    char table_content[MAX_COMMAND_LEN] = {0};
    int level = 0;
    char *src = found;
    char *dest = table_content;

    while (*src) {
        if (strncmp(src, "[[", 2) == 0) {
            level++;
            *dest++ = *src++;
            *dest++ = *src++;
        } else if (strncmp(src, "]]", 2) == 0) {
            level--;
            *dest++ = *src++;
            *dest++ = *src++;
            if (level == 0) break;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';

    // Extraire indices
    char *len_call = strstr(val, ".len(");
    if (!len_call) return NULL;

    len_call += 5;
    char indices_expr[MAX_COMMAND_LEN] = {0};
    int paren_level = 0;
    char *p = len_call;
    char *q = indices_expr;
    while (*p && (*p != ')' || paren_level > 0)) {
        if (*p == '(') paren_level++;
        if (*p == ')') paren_level--;
        *q++ = *p++;
    }
    *q = '\0';

    int index_list[MAX_COMMAND_LEN] = {0};
    int index_count = 0;

    char *cursor = indices_expr;
    while (*cursor) {
        if (*cursor == '[') {
            cursor++;
            char expr[128] = {0};
            int i = 0;
            while (*cursor && *cursor != ']') {
                expr[i++] = *cursor++;
            }
            expr[i] = '\0';
            if (*cursor != ']') return NULL;
            cursor++;
            index_list[index_count++] = (int)condition_script_order(expr);
        } else {
            cursor++;
        }
    }

    // Plonger récursivement dans la table
    char *current = table_content;
    for (int i = 0; i < index_count; i++) {
        int current_index = 0;
        int desired = index_list[i];
        char *sub = current + 2;  // Skip leading [[
        char element[MAX_COMMAND_LEN] = {0};
        int nesting = 0;

        while (*sub) {
            char *start = sub;
            int len = 0;

            if (strncmp(sub, "[[", 2) == 0) {
                nesting = 1;
                sub += 2;
                while (*sub && nesting) {
                    if (strncmp(sub, "[[", 2) == 0) {
                        nesting++;
                        sub += 2;
                    } else if (strncmp(sub, "]]", 2) == 0) {
                        nesting--;
                        sub += 2;
                    } else {
                        sub++;
                    }
                }
                len = sub - start;
            } else {
                // élément simple, attention aux parenthèses
                while (*sub && (paren_level > 0 || (*sub != ';' && strncmp(sub, "]]", 2) != 0))) {
                    if (*sub == '(') paren_level++;
                    else if (*sub == ')') {
                        if (paren_level > 0) paren_level--;
                    }
                    sub++;
                }
                len = sub - start;
            }

            if (current_index == desired - 1) {
                strncpy(element, start, len);
                element[len] = '\0';

                if (strncmp(element, "[[", 2) == 0 && strncmp(element + strlen(element) - 2, "]]", 2) == 0) {
                    // Valid subtable, set current to new subtable
                    strcpy(subtable, element);
                    current = subtable;
                } else {
                    return "0"; // Not a table
                }
                break;
            }

            if (*sub == ';') sub++;
            current_index++;
        }

        if (current_index != desired - 1) {
            return NULL; // Index hors limite
        }
    }

    // Si aucun index fourni, retourner table de base
    if (index_count == 0) {
        strcpy(subtable, table_content);
    }

    return subtable;
}

//pour len_table (table||.len([1]))
char* len_table(char *val) {
    static char result[MAX_COMMAND_LEN];
    strcpy(result, "0.00");

    char *table = extract_table_len(val);
    if (!table) {
        if (error_in_stderr==false){
            printf("Erreur : %s à échouée.\n",val);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : %s à échouée.\n",val);
            fflush(stderr);
        }
        if (error_lock_program) var_script_exit = 1;
        return result;
    }

    if (strcmp(table,"0")==0){
        return result;
    }

    //printf("table récup : %s",table);

    // Compter les éléments de plus haut niveau dans la table obtenue
    int count = 0;
    char *current = table + 2; // sauter [[
    int level = 0;
    char *p = current;

    while (*p && strncmp(p, "]]", 2) != 0) {
        if (strncmp(p, "[[", 2) == 0) {
            level++;
            p += 2;
            while (*p && level) {
                if (strncmp(p, "[[", 2) == 0) {
                    level++;
                    p += 2;
                } else if (strncmp(p, "]]", 2) == 0) {
                    level--;
                    p += 2;
                } else {
                    p++;
                }
            }
            count++;
        } else {
            int paren_level = 0;
            while (*p && (paren_level > 0 || (*p != ';' && strncmp(p, "]]", 2) != 0))) {
                if (*p == '(') paren_level++;
                else if (*p == ')') {
                    if (paren_level > 0) paren_level--;
                }
                p++;
            }
            count++;
        }

        if (*p == ';') p++;
    }

    snprintf(result, sizeof(result), "%.2f", (double)count);
    return result;
}


//fonction qui gère tout les appels à len_table() (table||.len())
char* for_len_table(char *arg) {
    static char output[MAX_COMMAND_LEN+2];
    output[0] = '\0';

    const char *p = arg;
    char *out = output;

    while (*p) {
        const char *match = strstr(p, "table|");
        if (!match) {
            // Aucun autre match, copier le reste
            strcpy(out, p);
            break;
        }

        // Copier le texte jusqu'au match
        while (p < match) *out++ = *p++;

        const char *table_start = match;
        const char *pipe2 = strchr(table_start + 6, '|');
        if (!pipe2) {
            // Pas de deuxième pipe copier le reste tel quel
            strcpy(out, p);
            break;
        }

        // Vérifier si on a directement ".len(" juste après
        if (strncmp(pipe2 + 1, ".len(", 5) == 0) {
            const char *len_start = pipe2 + 1;
            const char *scan = len_start + 5;
            int paren = 1;
            while (*scan && paren > 0) {
                if (*scan == '(') paren++;
                else if (*scan == ')') paren--;
                scan++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèse non fermée dans .len()\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèse non fermée dans .len()\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(out, p);
                break;
            }

            int expr_len = scan - table_start;
            char full_expr[MAX_COMMAND_LEN] = {0};
            strncpy(full_expr, table_start, expr_len);
            full_expr[expr_len] = '\0';

            // Remplacer par le résultat de len_table()
            char *val = len_table(full_expr);
            strcpy(out, val);
            out += strlen(val);
            p = scan; // Reprendre juste après l'expression traitée
        } else {
            // Pas un .len() on ne modifie pas, copier normalement
            *out++ = *p++;
        }
    }

    if (*p) {
        strcpy(out, p);
    }
    //*out = '\0';
    return output;
}

//pour l'ordre table||.add()
int add_table(const char *input_line) {
    if (!input_line || strncmp(input_line, "table|", 6) != 0) return 0;

    const char *add_pos = strstr(input_line, "|.add(");
    if (!add_pos) return 0;

    // Extraire le nom de la table
    char table_name[1024];
    strncpy(table_name, input_line + 6, add_pos - (input_line + 6));
    table_name[add_pos - (input_line + 6)] = '\0';

    if (strpbrk(table_name, "[]=|{}\"")) {
        if (error_in_stderr==false){
                printf("Erreur : le nom de la table contient un ou plusieurs caractères interdits : [ ] = { } | \"\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : le nom de la table contient un ou plusieurs caractères interdits : [ ] = { } | \"\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    // Extraire la valeur entre les parenthèses
    const char *val_start = strchr(add_pos, '(');
    if (!val_start) return -2;
    val_start++; // pointer après le (

    int depth = 1;
    const char *val_end = val_start;

    while (*val_end && depth > 0) {
        if (*val_end == '(') depth++;
        else if (*val_end == ')') depth--;
        val_end++;
    }
    val_end-=1;
    val_start-=1;

    // Vérifie que les parenthèses étaient bien équilibrées
    if (depth != 0) return -2;

    char value[MAX_COMMAND_LEN];
    strncpy(value, val_start + 1, val_end - val_start - 1);
    value[val_end - val_start - 1] = '\0';

    // Chemin du fichier
    char filePath[MAX_PATH];
    GetModuleFileNameA(NULL, filePath, MAX_PATH);
    char *lastSlash = strrchr(filePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(filePath, "table_frc.txt");

    // Fichier original et fichier temporaire
    FILE *file = fopen(filePath, "r");
    FILE *out = fopen("table_frc.tmp", "w");
    if (!file || !out) return -1;

    char line[MAX_COMMAND_LEN];
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (!found &&
            strncmp(line, table_name, strlen(table_name)) == 0 &&
            line[strlen(table_name)] == '=') {

            // Ligne correspondante trouvée
            char *equal_pos = strchr(line, '=');
            if (equal_pos) {
                char *end = equal_pos + strlen(equal_pos) - 1;

                // Recule pour trouver les deux derniers ']'
                int close_brackets = 0;
                while (end > equal_pos && close_brackets < 2) {
                    if (*end == ']') close_brackets++;
                    else if (close_brackets > 0) break; // si on est entré dans un autre contenu
                    end--;
                }

                if (close_brackets == 2) {
                    *(end + 1) = '\0'; // coupage après les deux derniers ']'
                    fprintf(out, "%s=[[%s;%s]]\n", table_name, equal_pos + 3, value);
                    found = 1;
                    continue;
                }
            }
        }
        fputs(line, out); // Copie normale
    }

    if (!found) {
        fprintf(out, "%s=[[%s]]\n", table_name, value);
    }

    fclose(file);
    fclose(out);

    // Remplace l'ancien fichier
    remove(filePath);
    rename("table_frc.tmp", filePath);

    return 1;
}

// Fonction utilitaire : saute les espaces (sur table||.del())
char *skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    return p;
}

// Fonction récursive : supprime l'élément ciblé à une profondeur donnée
int remove_at_index(char *start, int *indexes, int depth, int max_depth, char *out) {
    int level = 0;
    int idx = 0;
    char *scan = start;
    char temp[MAX_COMMAND_LEN] = {0};
    int out_pos = 0;
    int in_subexpr = 0;
    int paren = 0;

    int found = 0;

    while (*scan) {
        char *elem_start = scan;
        int bracket_depth = 0;

        // Sauter les espaces
        elem_start = skip_ws(elem_start);

        // Délimiter l'élément
        char *elem_end = elem_start;
        while (*elem_end) {
            if (*elem_end == '(') paren++;
            else if (*elem_end == ')') paren--;
            else if (strncmp(elem_end, "[[", 2) == 0) {
                bracket_depth++;
                elem_end += 2;
                continue;
            } else if (strncmp(elem_end, "]]", 2) == 0) {
                bracket_depth--;
                elem_end += 2;
                continue;
            } else if (*elem_end == ';' && bracket_depth == 0 && paren == 0) {
                break;
            }
            elem_end++;
        }

        int len = elem_end - elem_start;
        if (depth == max_depth - 1) {
            if (idx == indexes[depth]) {
                found = 1; //index trouvé
            }
            // On est au bon niveau pour supprimer
            if (idx != indexes[depth]) {
                if (out_pos > 0) out[out_pos++] = ';';
                strncpy(out + out_pos, elem_start, len);
                out_pos += len;
            }
        } else {
            if (idx == indexes[depth]) {
                found = 1; //index trouvé
                // Recursion ici
                char inner[MAX_COMMAND_LEN] = {0};
                if (elem_start[0] == '[' && elem_start[1] == '[') {
                    // Trouver la sous-table
                    char *inner_start = elem_start + 2;
                    char *inner_end = inner_start;
                    int open = 1;
                    while (*inner_end && open > 0) {
                        if (strncmp(inner_end, "[[", 2) == 0) {
                            open++;
                            inner_end += 2;
                        } else if (strncmp(inner_end, "]]", 2) == 0) {
                            open--;
                            inner_end += 2;
                        } else {
                            inner_end++;
                        }
                    }
                    char sub_expr[MAX_COMMAND_LEN] = {0};
                    strncpy(sub_expr, inner_start, inner_end - inner_start - 2);  // exclut le ]]
                    sub_expr[inner_end - inner_start - 2] = '\0';

                    char new_inner[MAX_COMMAND_LEN] = {0};
                    if (remove_at_index(sub_expr, indexes, depth + 1, max_depth, new_inner)) {
                        if (out_pos > 0) out[out_pos++] = ';';
                        out[out_pos++] = '[';
                        out[out_pos++] = '[';
                        strcpy(out + out_pos, new_inner);
                        out_pos += strlen(new_inner);
                        out[out_pos++] = ']';
                        out[out_pos++] = ']';
                    }
                } else {
                    // Index profond mais l’élément n’est pas une sous-table
                    if (error_in_stderr==false){
                        printf("Erreur : tentative d’accès à une profondeur invalide pour table del.\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : tentative d’accès à une profondeur invalide pour table del.\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                    return 0;
                }
            } else {
                // Copier tel quel
                if (out_pos > 0) out[out_pos++] = ';';
                strncpy(out + out_pos, elem_start, len);
                out_pos += len;
            }
        }

        // Avancer au prochain élément
        if (*elem_end == ';') elem_end++;
        scan = elem_end;
        idx++;
    }

    out[out_pos] = '\0';

    if (!found) {
        if (error_in_stderr==false){
                printf("Erreur : index %d non trouvé à la profondeur %d\n", indexes[depth] + 1, depth);
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : index %d non trouvé à la profondeur %d\n", indexes[depth] + 1, depth);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    return 1;
}

//fonction pour évaluer les index dans table||.del([index][index]) et dans table||.edit([index][index])(valeur)
int evaluate_indexes(const char *expr, int *indexes, int *count) {
    const char *p = expr;
    *count = 0;

    while (*p) {
        if (*p == '[') {
            p++;  // Skip [
            char expr_buf[MAX_COMMAND_LEN] = {0};
            int i = 0;

            int bracket_depth = 1;
            while (*p && bracket_depth > 0) {
                if (*p == '[') bracket_depth++;
                else if (*p == ']') bracket_depth--;
                if (bracket_depth > 0)
                    expr_buf[i++] = *p;

                    p++; // Skip the closing ]
            }

            expr_buf[i] = '\0';
            //printf("Évaluating expression : %s\n", expr_buf);

            double val = condition_script_order(expr_buf);
            int idx = (int)val - 1; // 1-based to 0-based
            if (idx < 0) {
                if (error_in_stderr==false){
                    printf("Erreur : index négatif ou null pour [%s] => %d\n", expr_buf, idx);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index négatif ou null pour [%s] => %d\n", expr_buf, idx);
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                return 0;
            }

            indexes[*count] = idx;
            (*count)++;
        } else {
            p++;
        }
    }

    return 1;
}

// Supprime l’élément visé dans une table à partir d’un appel comme table|nom|.del([1][2])
int process_table_del(const char *recup) {
    if (!recup || strncmp(recup, "table|", 6) != 0) return 0;

    const char *del_pos = strstr(recup, "|.del(");
    if (!del_pos) return 0;

    char table_name[512];
    const char *name_start = recup + 6;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep > del_pos) return 0;

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Récupérer les index
    // Trouver le contenu entre les parenthèses de .del(...)
    const char *start_paren = strchr(del_pos, '(');
    if (!start_paren) return 0;
    start_paren++;  // après '('

    const char *p = start_paren;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
    }

    if (depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées dans .del(...)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées dans .del(...)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    const char *end_paren = p - 1;  // p a avancé d’un caractère après la dernière ')'

    char expr_buf[MAX_COMMAND_LEN] = {0};
    strncpy(expr_buf, start_paren, end_paren - start_paren - 1);
    expr_buf[end_paren - start_paren - 1] = '\0';

    int indexes[MAX_COMMAND_LEN] = {0}, index_count = 0;
    if (!evaluate_indexes(expr_buf, indexes, &index_count)) {
        //printf("Erreur d’évaluation des indices []\n");
        //fflush(stdout);
        return 0;
    }

    // Charger le fichier
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(path, "table_frc.txt");

    FILE *in = fopen(path, "r");
    FILE *outf = fopen("table_frc.tmp", "w");
    if (!in || !outf) return 0;

    char line[MAX_COMMAND_LEN];
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, table_name, strlen(table_name)) == 0 && line[strlen(table_name)] == '=') {
            char *start = strstr(line, "[[");
            char *end = NULL;
            char *scan = line;
            while ((scan = strstr(scan, "]]")) != NULL) {
                end = scan;
                scan += 2;
            }
            if (!start || !end || end <= start) {
                fputs(line, outf);
                continue;
            }
            start += 2;
            *end = '\0';

            char mod_result[MAX_COMMAND_LEN] = {0};
            if (!remove_at_index(start, indexes, 0, index_count, mod_result)) {
                //printf("Erreur lors de la suppression\n");
                fclose(in); fclose(outf); return 0;
            }

            fprintf(outf, "%s=[[%s]]\n", table_name, mod_result);
        } else {
            fputs(line, outf);
        }
    }

    fclose(in);
    fclose(outf);

    remove(path);
    rename("table_frc.tmp", path);
    return 1;
}

//fonction pour table||.edit([][])(valeur)
int replace_at_index(char *start, int *indexes, int depth, int max_depth, char *new_value, char *out) {
    int idx = 0;
    char *scan = start;
    int out_pos = 0;
    int found = 0;
    int paren = 0;

    while (*scan) {
        char *elem_start = skip_ws(scan);
        char *elem_end = elem_start;
        int bracket_depth = 0;

        while (*elem_end) {
            if (*elem_end == '(') paren++;
            else if (*elem_end == ')') paren--;
            else if (strncmp(elem_end, "[[", 2) == 0) {
                bracket_depth++;
                elem_end += 2;
                continue;
            } else if (strncmp(elem_end, "]]", 2) == 0) {
                bracket_depth--;
                elem_end += 2;
                continue;
            } else if (*elem_end == ';' && bracket_depth == 0 && paren == 0) {
                break;
            }
            elem_end++;
        }

        int len = elem_end - elem_start;

        if (depth == max_depth - 1) {
            if (idx == indexes[depth]) {
                found = 1;
                if (out_pos > 0) out[out_pos++] = ';';
                strncpy(out + out_pos, new_value, strlen(new_value));
                out_pos += strlen(new_value);
            } else {
                if (out_pos > 0) out[out_pos++] = ';';
                strncpy(out + out_pos, elem_start, len);
                out_pos += len;
            }
        } else {
            if (idx == indexes[depth]) {
                found = 1;
                char inner[MAX_COMMAND_LEN] = {0};
                if (elem_start[0] == '[' && elem_start[1] == '[') {
                    char *inner_start = elem_start + 2;
                    char *inner_end = inner_start;
                    int open = 1;
                    while (*inner_end && open > 0) {
                        if (strncmp(inner_end, "[[", 2) == 0) {
                            open++;
                            inner_end += 2;
                        } else if (strncmp(inner_end, "]]", 2) == 0) {
                            open--;
                            inner_end += 2;
                        } else {
                            inner_end++;
                        }
                    }
                    char sub_expr[MAX_COMMAND_LEN] = {0};
                    strncpy(sub_expr, inner_start, inner_end - inner_start - 2);
                    sub_expr[inner_end - inner_start - 2] = '\0';

                    char new_inner[MAX_COMMAND_LEN] = {0};
                    if (replace_at_index(sub_expr, indexes, depth + 1, max_depth, new_value, new_inner)) {
                        if (out_pos > 0) out[out_pos++] = ';';
                        out[out_pos++] = '[';
                        out[out_pos++] = '[';
                        strcpy(out + out_pos, new_inner);
                        out_pos += strlen(new_inner);
                        out[out_pos++] = ']';
                        out[out_pos++] = ']';
                    }
                } else {
                    if (error_in_stderr==false){
                        printf("Erreur : tentative d'accès à une profondeur invalide sur table edit.\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : tentative d'accès à une profondeur invalide sur table edit.\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                    return 0;
                }
            } else {
                if (out_pos > 0) out[out_pos++] = ';';
                strncpy(out + out_pos, elem_start, len);
                out_pos += len;
            }
        }

        if (*elem_end == ';') elem_end++;
        scan = elem_end;
        idx++;
    }

    out[out_pos] = '\0';

    if (!found) {
        if (error_in_stderr==false){
                printf("Erreur : index %d non trouvé à la profondeur %d sur table edit\n", indexes[depth] + 1, depth);
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : index %d non trouvé à la profondeur %d sur table edit\n", indexes[depth] + 1, depth);
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    return 1;
}

//fonction pour lister les tables sur le fichier table_frc.txt table_list()
void table_list() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(path, "table_frc.txt");

    FILE *file = fopen(path, "r");
    if (!file) {
        if (error_in_stderr==false){
                printf("Erreur : impossible d’ouvrir le fichier table_frc.txt\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : impossible d’ouvrir le fichier table_frc.txt\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    printf("Liste des tables dans le fichier table_frc.txt :\n");
    fflush(stdout);

    char line[MAX_COMMAND_LEN];
    while (fgets(line, sizeof(line), file)) {
        // Cherche =[[ dans les 32 premiers caractères
        char *match = strstr(line, "=[[");
        if (match && (match - line) <= 32) {
            // Extraire le nom
            int len = match - line;
            char name[256] = {0};
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            strncpy(name, line, len);
            name[len] = '\0';

            // Nettoyer les espaces, tabulations, retour chariot
            for (int i = 0; name[i]; i++) {
                if (name[i] == '\t' || name[i] == '\n' || name[i] == '\r') { //name[i] == ' ' ||
                    name[i] = '\0';
                    break;
                }
            }

            if (strlen(name) > 0) {
                printf("%s\n", name);
                fflush(stdout);
            }
        }
    }

    fclose(file);
    //printf("\n");
}

void table_sea(const char *recup) {
    if (!recup || strncmp(recup, "table|", 6) != 0) return;

    const char *sea_pos = strstr(recup, "|.sea(");
    if (!sea_pos) return;

    // Extraire le nom de la table
    char table_name[128] = {0};
    const char *name_start = recup + 6;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep > sea_pos) return;

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Extraire l'attribut
    const char *start_paren = strchr(sea_pos, '(');
    const char *end_paren = strrchr(sea_pos, ')');
    if (!start_paren || !end_paren || end_paren <= start_paren) return;

    char attr_expr[MAX_COMMAND_LEN] = {0};
    strncpy(attr_expr, start_paren + 1, end_paren - start_paren - 1);
    attr_expr[end_paren - start_paren - 1] = '\0';

    // Évaluer l'attribut
    double result = condition_script_order(attr_expr);
    int add_newline = (result != 1);

    // Construire le chemin du fichier
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(path, "table_frc.txt");

    FILE *file = fopen(path, "r");
    if (!file) {
        if (error_in_stderr==false){
                printf("Erreur : impossible d’ouvrir le fichier table_frc.txt\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : impossible d’ouvrir le fichier table_frc.txt\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return;
    }

    char line[MAX_COMMAND_LEN];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, table_name, strlen(table_name)) == 0 && strstr(line, "=[[") != NULL) {
            // Cherche le premier [[ après le nom=
            char *start = strstr(line, "[[");
            if (start) {
                // Supprimer le saut de ligne final s’il existe
                char *newline = strchr(start, '\n');
                if (newline) *newline = '\0';
                //start += 2;  // sauter les [[
                printf("%s", start);
                fflush(stdout);
                if (socket_var == 1) {
                    send(client, start, strlen(start), 0); //pour le voir depuis le serveur, voir la table
                }
                found = 1;
                break;
            }
        }
    }

    fclose(file);

    if (found && add_newline) {
        printf("\n");
        fflush(stdout);
    }
    if (found==0) {
        if (error_in_stderr==false){
                printf("Erreur : la table demandée n'existe pas !\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : la table demandée n'existe pas !\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
    }
}

//fonction pour table||.edit([][])(valeur)
int process_table_edit(const char *recup) {
    if (!recup || strncmp(recup, "table|", 6) != 0) return 0;

    const char *edit_pos = strstr(recup, "|.edit(");
    if (!edit_pos) return 0;

    char table_name[512];
    const char *name_start = recup + 6;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep > edit_pos) return 0;

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Récupérer les index [x][y] et la valeur (val)
    const char *start_paren = strchr(edit_pos, '(');
    if (!start_paren) return 0;
    start_paren++;

    const char *p = start_paren;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
    }

    if (depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées dans .edit(...)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées dans .edit(...)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    const char *end_indexes = p - 1;

    // Ensuite on a (valeur)
    if (*p != '(') {
        if (error_in_stderr==false){
                printf("Erreur : valeur manquante dans .edit(...)(valeur)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : valeur manquante dans .edit(...)(valeur)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    p++;  // après le '(' ouvrant de la valeur
    const char *val_start = p;
    int val_paren_depth = 1;

    while (*p && val_paren_depth > 0) {
        if (*p == '(') val_paren_depth++;
        else if (*p == ')') val_paren_depth--;
        p++;
    }

    if (val_paren_depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses déséquilibrées dans la valeur à insérer sur table edit.\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses déséquilibrées dans la valeur à insérer sur table edit.\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    char value[MAX_COMMAND_LEN] = {0};
    strncpy(value, val_start, (p-1) - val_start);
    value[(p-1) - val_start] = '\0';

    char expr_buf[MAX_COMMAND_LEN] = {0};
    strncpy(expr_buf, start_paren, end_indexes - start_paren);
    expr_buf[end_indexes - start_paren] = '\0';

    int indexes[MAX_COMMAND_LEN] = {0}, index_count = 0;
    if (!evaluate_indexes(expr_buf, indexes, &index_count)) {
        return 0;
    }

    // Charger le fichier
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(path, "table_frc.txt");

    FILE *in = fopen(path, "r");
    FILE *outf = fopen("table_frc.tmp", "w");
    if (!in || !outf) return 0;

    char line[MAX_COMMAND_LEN];
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, table_name, strlen(table_name)) == 0 && line[strlen(table_name)] == '=') {
            char *start = strstr(line, "[[");
            char *end = NULL;
            char *scan = line;
            while ((scan = strstr(scan, "]]")) != NULL) {
                end = scan;
                scan += 2;
            }

            if (!start || !end || end <= start) {
                fputs(line, outf);
                continue;
            }

            start += 2;
            *end = '\0';

            char mod_result[MAX_COMMAND_LEN] = {0};
            if (!replace_at_index(start, indexes, 0, index_count, value, mod_result)) {
                fclose(in); fclose(outf); return 0;
            }

            fprintf(outf, "%s=[[%s]]\n", table_name, mod_result);
        } else {
            fputs(line, outf);
        }
    }

    fclose(in);
    fclose(outf);

    remove(path);
    rename("table_frc.tmp", path);

    return 1;
}

//récupérer le contennu du fichier .conf pour chaque exécution
void get_frc_conf() {
    char path_exe[1024];
    DWORD path_len = GetModuleFileNameA(NULL, path_exe, sizeof(path_exe));
    if (path_len == 0 || path_len >= sizeof(path_exe)) {
        printf("Erreur lors de la récupération du chemin de l'exécutable pour fr-simplecode.conf\n");
        fflush(stdout);
        return;
    }

    // Isoler le dossier
    char *last_slash = strrchr(path_exe, '\\');
    if (last_slash != NULL) {
        *(last_slash + 1) = '\0';
    }

    // Chemin complet du fichier
    char path_file[1024];
    snprintf(path_file, sizeof(path_file), "%s%s", path_exe, "fr-simplecode.conf");

    // Vérifier si le fichier existe
    FILE *file = fopen(path_file, "r");
    if (file == NULL) {
        // Fichier non trouvé, on ne fait rien
        return;
    }

    // Lire ligne par ligne
    char line[1048];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Enlever retour à la ligne éventuel
        line[strcspn(line, "\r\n")] = 0;

        char *command = line;

        // Tes tests d'options :
        if (strncmp(command, "###sortie:classique###",22)==0 || strncmp(command,"###sortie:normale###",20)==0 || strncmp(command,"###output:normal###",19)==0){
            mode_script_output=0;
            continue;
        }
        if (strncmp(command, "###sortie:tout###",17)==0 || strncmp(command,"###sortie:all###",16)==0 || strncmp(command,"###output:all###",16)==0){
            mode_script_output=1;
            continue;
        }
        if (strncmp(command,"###sortie_affiche:pasdevirgule###",33)==0 || strncmp(command,"###output_print:notcomma###",27)==0){
            var_affiche_virgule=1;
            continue;
        }
        if (strncmp(command,"###sortie_affiche:virgule###",28)==0 || strncmp(command,"###output_print:comma###",24)==0){
            var_affiche_virgule=0;
            continue;
        }
        if (strncmp(command,"###sortie_affiche:[/n]###",25)==0 || strncmp(command,"###output_print:[/n]###",23)==0){
            var_affiche_slash_n=0;
            continue;
        }
        if (strncmp(command,"###sortie_affiche:pasde[/n]###",30)==0 || strncmp(command,"###output_print:not[/n]###",26)==0){
            var_affiche_slash_n=1;
            continue;
        }
        if (strncmp(command,"###fonction_num:oui###",22)==0 || strncmp(command,"###function_num:true###",23)==0 || strncmp(command,"###fonction_num:true###",23)==0){
            var_activ_func_num=true;
            continue;
        }
        if (strncmp(command,"###fonction_num:non###",22)==0 || strncmp(command,"###function_num:false###",23)==0 || strncmp(command,"###fonction_num:false###",23)==0){
            var_activ_func_num=false;
            continue;
        }
        if (strncmp(command,"###fonction_text:oui###",23)==0 || strncmp(command,"###function_text:true###",24)==0 || strncmp(command,"###fonction_text:true###",24)==0){
            var_activ_func_text=true;
            continue;
        }
        if (strncmp(command,"###fonction_text:non###",23)==0 || strncmp(command,"###function_text:false###",25)==0 || strncmp(command,"###fonction_text:false###",25)==0){
            var_activ_func_text=false;
            continue;
        }
        if (strncmp(command,"###autorise/:oui###",19)==0 || strncmp(command,"###autorise/:true###",20)==0){
            autorise_slash_text_num_lock=true;
            continue;
        }
        if (strncmp(command,"###autorise/:non###",19)==0 || strncmp(command,"###autorise/:false###",21)==0){
            autorise_slash_text_num_lock=false;
            continue;
        }
        if (strncmp(command,"###table_use_script_path:true###",32)==0 || strncmp(command,"###table_use_script_path:oui###",31)==0){
            use_script_path=true;
            continue;
        }
        if (strncmp(command,"###table_use_script_path:false###",33)==0 || strncmp(command,"###table_use_script_path:non###",31)==0){
            use_script_path=false;
            continue;
        }
        if (strncmp(command,"###debug_use_script:true###",27)==0 || strncmp(command,"###debug_use_script:oui###",26)==0){
            debug_use_script=true;
            continue;
        }
        if (strncmp(command,"###debug_use_script:false###",28)==0 || strncmp(command,"###debug_use_script:non###",26)==0){
            debug_use_script=false;
            continue;
        }
        if (strncmp(command,"###dll_arg_analysis:true###",27)==0 || strncmp(command,"###analyse_argument_dll:oui###",30)==0){
            dll_arg_analysis=true;
            continue;
        }
        if (strncmp(command,"###dll_arg_analysis:false###",28)==0 || strncmp(command,"###analyse_argument_dll:non###",30)==0){
            dll_arg_analysis=false;
            continue;
        }
        if (strncmp(command,"###sortie_erreur:oui###",23)==0 || strncmp(command,"###sortie_erreur:true###",24)==0 || strncmp(command,"###output_error:true###",23)==0){
            error_in_stderr=true;
            continue;
        }
        if (strncmp(command,"###sortie_erreur:non###",23)==0 || strncmp(command,"###sortie_erreur:false###",25)==0 || strncmp(command,"###output_error:false###",24)==0){
            error_in_stderr=false;
            continue;
        }
        if (strncmp(command,"###stop_script_erreur:oui###",28)==0 || strncmp(command,"###stop_script_erreur:true###",29)==0 || strncmp(command,"###stop_script_error:true###",28)==0){
            error_lock_program=true;
            continue;
        }
        if (strncmp(command,"###stop_script_erreur:non###",28)==0 || strncmp(command,"###stop_script_erreur:false###",30)==0 || strncmp(command,"###stop_script_error:false###",29)==0){
            error_lock_program=false;
            continue;
        }
    }

    fclose(file);
}



int autorise=0;

//-lws2_32 pour server tcp gestion de script
//gcc -std=c11 -pthread fr-simplecode0.3.c icon_frc.o -o fr-simplecode0.3.exe -lole32 -luuid -lsapi -lws2_32
int main(int argc, char *argv[]) {
    //printf("Arguments reçus :\n");
    //for (int i = 0; i < argc; i++) {
        //printf("argv[%d]: %s\n", i, argv[i]);
    //}

    licence(); //créer la licence de fr-simplecode
    create_frc_conf(); //créer le fichier fr-simplecode.conf qui est le fichier de configuration des variables de frc system pour les ###...###

    init_var_file_path(); //initialise le chemin pour VAR_FILE au niveau de l'exe (var||)

    if ((argc == 2 && (strcmp(argv[1], "--version") == 0)) || (argc == 2 && (strcmp(argv[1], "--v") == 0))) {
        char *exec_path = get_executable_path();
        printf("VERSION : 0.3\nutilisation win64_bit, path : %s\n", exec_path);
        fflush(stdout);
        return 0;  // Quitter immédiatement après avoir affiché la version
    } else if (argc == 2 && (strcmp(argv[1], "--licence") == 0)){
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

    //récupération d'un argument fournit par l'utilisateur nécessaire dans son programme
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--arg:", 6) == 0) {
            // récupérer tout ce qui vient après "--arg:"
            const char *value = argv[i] + 6;
            strncpy(main_arg_global, value, sizeof(main_arg_global) - 1);
            main_arg_global[sizeof(main_arg_global) - 1] = '\0'; // sécurité
            break;
        }
    }

    if (argc == 2 && (strncmp(argv[1], "--e:",4) == 0)){
        char *expression = argv[1] + 4; // Pointeur sur ce qui suit "--e:"
        double result = condition_script_order(expression);
        printf("Résultat : %.2f\n", result);
        fflush(stdout);
        return 0;
    } else if (argc == 2 && (strcmp(argv[1], "--info") == 0)){
        printf("info  sur -- : utilisez --version ou --v pour version, --licence pour licence,\n--e: pour faire un calcul directement en le collant au ':' (! pas d'espace sinon erreur)\net ne prend que des calculs, pas de script '[[[]]]' !\nPour random, utiliser --random (nombre entre 0 et 1000)\n--start:'cheminduficher' lance le premier [[[]]] du fichier frc (ou .txt) qu'il y est un #init ou non\n--server:... il faut entrer un port après les : (nombre int) le serveur démarre et ne peut prendre qu'un client,\nutilisé exit pour quitter, il peut lire des scripts [[[]]] et des commandes basiques (! pas de var|| ni table|| !)\n! les erreurs ne sont pas envoyé sur le socket, regardé le serveur !\n--\"nano 'suite_de_la_commande_nano_comme_sur_la_console'\" permet d'exécuter, de créer et de supprimer des scripts\n--arg: permet de fournir un argument après les : qui sera récupérable dans le programme avec [main_arg] dans text() ou dans num()\n(plus généralement une valeur numérique si l'arg fournit est bien un nombre, sinon il deviendra 0, ! un seul --arg: possible)\n");
        fflush(stdout);
        return 0;
    } else if (argc == 2 && strcmp(argv[1], "--random") == 0) {
        // Initialisation du générateur pseudo-aléatoire avec l'heure actuelle
        srand((unsigned int)time(NULL));
        // Génère un nombre entre 0 et 1000 inclus
        int aleatoire = rand() % 1001;
        // Affiche le résultat
        printf("Résultat : %d\n", aleatoire);
        return 0;
    } else if (argc == 2 && strncmp(argv[1], "--nano",6)==0){
        char com_nano[256];
        strncpy(com_nano, argv[1] + 2, sizeof(com_nano) - 1);
        com_nano[sizeof(com_nano) - 1] = '\0';
        if (strncmp(com_nano,"nano frc ",9)==0){
            handle_nano_frc(com_nano);
            return 0;
        } else if (strncmp(com_nano,"nano suppr frc ",15)==0){
            handle_nano_suppr_frc(com_nano);
            return 0;
        } else if (strncmp(com_nano,"nano start frc ",15)==0){
            mode_script_output = 0; //assurer qu'il n'affiche pas le détaille du script
            get_frc_conf(); //si conf existe, on l'applique
            handle_nano_start_frc(com_nano);
            return 0;
        } else {
            printf("commande nano incorrect, fonctionne de la même manière que depuis la console !\n(ex: \"--nano frc 'nom_ficher_a_creer')\"\n");
            fflush(stdout);
            return 0;
        }
    } else if (argc >= 2 && strncmp(argv[1], "--server:", 9) == 0) {
        WSADATA wsa;
        SOCKET server;
        struct sockaddr_in server_addr, client_addr;
        int port = atoi(argv[1] + 9);
        char buffer[MAX_COMMAND_LEN];

        // Initialiser Winsock
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            printf("Échec de l'initialisation de Winsock.\n");
            return 1;
        }

        // Affichage de l'adresse IP locale
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
            printf("Erreur gethostname()\n");
        } else {
            struct hostent *host_entry = gethostbyname(hostname);
            if (host_entry == NULL) {
                printf("Erreur gethostbyname()\n");
            } else {
                char *ip = inet_ntoa(*(struct in_addr*)host_entry->h_addr_list[0]);
                printf("IP locale : %s\n", ip);
            }
        }

        printf("Démarrage du serveur tcp sur le port %d...\n", port);

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
                printf("Fin du programme fr-simplecode,\narrêt du serveur.\n");
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
                debug_use_script=false;
                var_global_bool=-1; //met global_bool a none à chaque nouveaux programme
                error_in_stderr=false; //erreur sur stdout
                error_lock_program=false; //les erreurs n'arrête pas le script
                get_frc_conf(); //si conf existe, on l'applique de préférence (de plus il ne gère pas toutes celles présente ici, uniquement celle configurable par les ###...###)
                block_execute=0;
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
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(buffer, replace_rep(buffer, last_result));

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
    } else if (argc >= 2 && strncmp(argv[1], "--start:", 8) == 0) {
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
            debug_use_script=false;
            var_global_bool=-1; //met global_bool a none à chaque nouveaux programme
            get_frc_conf(); //si conf existe, on l'applique de préférence (de plus il ne gère pas toutes celles présente ici, uniquement celle configurable par les ###...###)
            block_execute=0;
            handle_script(clean_script);
        }

        free(buffer);
        free(clean_script);
        return 0;
    } else if (argc == 2 && _access(argv[1],0)==0) { //exécute un fichier founit au démarage
        char prog[2048];
        snprintf(prog, sizeof(prog), "[[[{use_script(%s)}]]]", argv[1]);
        var_script_exit= 0;
        var_script_pause= 0;
        mode_script_output = 0;
        var_affiche_virgule = 0;
        var_affiche_slash_n = 0;
        var_boucle_infini = 0;
        var_in_module=0;
        debug_use_script=false;
        var_global_bool=-1; //met global_bool a none à chaque nouveaux programme
        get_frc_conf(); //si conf existe, on l'applique de préférence (de plus il ne gère pas toutes celles présente ici, uniquement celle configurable par les ###...###)
        block_execute=0;
        handle_script(prog);

        Sleep(300);

        printf("\nProgramme terminé,\nfr-simplecode va s'arrêté automatiquement ...\n\n(tout droits réservés à argentropcher, asigné au compte Google argentropcher,\nmodification strictement interdite selon l'article L.111-1 du Code de la propriété intellectuelle)\n");
        fflush(stdout);

        Sleep(2500);

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
            printf("INFO :\nLes calculs pris en compte sont basique: addition, soustraction, multiplication, division.\n(pour les virgules, utilisez '.')\nFonction :\n'rep' pour reprendre le résultat précédent\nversion() renvoie le numéro de la version de l'app\n");
            //fflush(stdout);
            printf("'random[nombre1,nombre2]' renvoie un nombre aléatoire entre les 2 nombres entiers choisi\n'random(nombre1;nombre2)' est plus performant et à une graine, il peut faire du random sur des nombres à virgules depuis la v0.3\n'execute()' permet d'executer des commandes cmd\n'get_comma(calcul)' et 'prend_virgule(calcul)' récupère les deux chiffres après la virgule en entier\n'var|'nomdelavariable'|' permet de créer/modifier des variables d'environnement (voir var.txt):\nutilisez 'var|nomdelavariable|=nombre/calcul/cercle()/triangle(;)/random[,]...' permet de définir la fonction\n'var|nomdelavariable|' la rappel du fichier si elle existe, (sinon valeur 0.00) pour l'utiliser dans un calcul\n+utiliser la fonction 'var_list()' pour obtenir la liste des variables de var.txt,\net 'var_suppr(all)' pour supprimé le fichier var.txt\nou 'var_suppr(nomdelavariable)' pour supprimé une variable du fichier var.txt\n'in_var(votrerecherche)' permet de savoir si il y a votrerecherche dans var.txt\nque se soit un nom de variable ou une valeur.\n");
            //fflush(stdout);
            printf("'m' pour sauvgarder le résultat précédent\n'm+' pour importer le résultat sauvgarder et le stocker dans 'rep'\n'sqrt(argument)' pour la racine carrée\n'cbrt(argument)' pour la racine cubique\nLes fonctions exp(), log(), tan(), arctan(), sin(), arcsin(), cos(), arccos() sont également disponible\n'modulo(calcul1;calcul2)' permet de faire un modulo (comme le pourcentage de python sauf que le séparateur est ';')\n'time(arg)' renvoie en fonction de arg les infos sur l'heure ...\nsi arg est null, cela renvoie l'heure miniute, seconde\nsi arg est 'm', juste les minute, 'h' juste l'heure, 's' les secondes, 'ms' les milisecondes\n'date' jour mois,année, 'day' le jour. Pour avoir le numéro du jour de la semaine, utilisez la fonction num_day().\n");
            //fflush(stdout);
            printf("'local(argument)' pour sauvgarder une variable dans la mémoire vive avec en argument un calcul\n'inlocal()' pour récupérer la valeur de 'local(argument)' et la mettre dans 'rep'\n");
            //fflush(stdout);
            printf("'triangle(x;y)'pour calculer l'air d'un triangle de base=x et de hauteur=y\n'cercle(rayon)' pour calculer l'aire du cercle\n'fonction(argument)' pour plus d'info sur les fonctions\n'version' pour version\n'licence' pour licence\n'exit' pour quitter\n");
            //fflush(stdout);
            printf("PARTIE CODE/PROGRAMME :\nLes programmes ont une structure de block imbriquée les un dans les autres.\nUn programme commence toujours par '[[[' et fini toujours par ']]]'.\nLes blocks à l'intérieur de cela commence et finissent toutjours par '{' et '}'.\nLes différents blocks :\n");
            //fflush(stdout);
            printf("'{print(argument)}' ou '{affiche(argument)}' sont des blocks pour renvoié un résultat\n l'argument peut être un calcul ou  un texte entre \"\" double.\nPour mettre 2 calcul différents sans texte, s'éparer les par des '' double collé.\n");
            //fflush(stdout);
            printf("'{var|nomdelavariable|=valeurendouble}' est la même chose que précédament et permet de définir une variable\nutiliser 'var|nomdelavariable|' pour la rappeler.\nLes fonctions 'var_list()', 'var_suppr(nomdelavariableoutout)'\net 'in_var(votrevaleurouvariablerechercher)' sont toujours disponible en les intégrants dans des '{}'.\nla definition de variable texte est possible en mettant des guillemets \" après le =, pour récupérer le texte, utilisé la fonction text()\n");
            //fflush(stdout);
            printf("'{if||v1||condition||v2||accoladeimbriqué{}}' le if peut être remplacer par si, v1 et v2 sont des calculs, variable ...\ncondition peut être '?=' si c'est égal, '!=' si ce n'est pas égal, '<', '>', '<=' ou '>='.\nOn peut également rajouté en cas de condition fausse [[nombrede{}àsauterencasdeconditionfausse]]\nqui saute les premiers {} si la condition est fausse.\n||else|| fonctionne aussi et évite de compté les {} mais est moins rapide ex:[[[{if||1||?=||1||{print(\"a\")}||else||{print(\"b\")}}]]]\nif!||v|| fonctionne de la même façon et renvoie vrai si v?=0 sinon faux, et if?||v|| renvoie vrai si v!=0 sinon faux\nif(argdecondition) prend une série de conditions imbriqué sous les formes présédentes mais ajoute les or/ou and/et et fonctionne de la même façon\n");
            //fflush(stdout);
            printf("'{repeat||nombrederepetition||accoladeimbriqué{}...}', le mot 'repeat' peut être remplacer par 'boucle' ou par 'répète'\n nombrederépétition est le nombre de fois qu'il va exécuter la boucle,\nil peut aussi être 'infini' ou 'endless' pour fonctionné indéfiniment.\nPour arrêter une boucle, ont peut utiliser 'pause(nombred'arrêtdeboucle)' ou 'break(nombred'arrêtdeboucle)'\nqui permet de sortir des boucles\nsi la boucle est 'infini' ou 'endless', le nombred'arrêt à 1 suffi,\nsinon, il faut compter le nombre de répétition à arrêter\nSi vous ne voulez pas compter ou si la logique est trop compliquée,\nmettez une grande valeur et utilisez 'stop_break()' ou 'stop_pause()' à la sortie de la boucle.\n");
            //fflush(stdout);
            printf("'{exit}' sort du programme complètement.\nFonction supplémentaire de l'affichage :\n");
            //fflush(stdout);
            printf("'output_input(false)' et 'sortir_entrée(faux)' désactive le renvoie de l'entrée de l'utilisateur,\n'output_input(true)' et 'sortir_entrée(vrai)' l'active\n'{###sortie:classique###}', '{###sortie:normale###}' et '{###output:normal###}'désactive\nles messages script lors de l'execution,\n'{###sortie:tout###}', '{###sortie:all###}' et '{###output:all###}' active les messages script\nPOUR LES PRINT/AFFICHE :\n'{###sortie_affiche:pasdevirgule###}', '{###output_print:notcomma###}' n'affiche pas les virgules\ndes nombres dans les print() et affiche()\n'{###sortie_affiche:[/n]###}' et '{###output_print:[/n]###}' active le saut de ligne automatique dans la console\n'{###sortie_affiche:pasde[/n]###}' et '{###output_print:not[/n]###}' le désactive\nSi vous voulez sauter de ligne dans un print, écrivez entre \"\" double '[/n]'\n");
            //fflush(stdout);
            printf("SON :\n'error_sound()' et 'son_erreur()' permet de jouer le son erreur de windows,\n'speak(arg)' et 'parle(arg)' disent arg (comme les print() et affiche())\n");
            //fflush(stdout);
            printf("dernière version 0.3, ajoute :\n");
            //fflush(stdout);
            printf("les table :\nles table sont comme des variables sauf quelle enregistre sous forme de liste et peuvent prendre du texte\n(voir fonction text() et num() pour avoir les bonnes méthodes)\ndéfinir une table : table|nomdelatable|=[[...]] les séparateurs sont des ; et on peut imbriqué des [[;[[;]]]] dans des tables\npour supprimé une table, utilisé table||nomdelatable||=suppr()\npour récupérer la longueur d'une table, utiliser table|nomdelatable|.len() renvoie la longueur de la table principale (minimum 1)\n.len() peut prendre des index pour récupérer la longueur d'une table dans une table : .len([2]) renvoie 3 dans cette table : [[;[[;;]];]]\nles index peuvent être cumulé : .len([1][2]) par exemple si il y a une table dans 2 autres table ...\ntable|nomdelatable|=join(table|nomdelatable|;table|nomdelatable|)\npermet de mettre 2 tables différentes ou identique pour les assemblé en une seule sous nomdelatable comptenant les 2 autres ou même table\n");
            //fflush(stdout);
            printf("table|nomdelatable|.add(valeur) permet d'ajouté une valeur dans une nouvelle case à la fin de la table sans utiliser join pour une simple valeur\ntable|nomdelatable|.edit(index)(valeur) index possitionne l'emplacement de changement de valeur dans la table et la remplace par valeur\ntable|nomdelatable|.del(index) index est la case que l'on souhaite supprimé dans la table si elle existe\ntable|nomdelatable|[index] index permet de choisir la case dont on veux récupérer la valeur (ou la sous table)\ntable|nomdelatable|.sea(valeur) permet d'afficher la table en entier (valeur si 1 ne fait pas de saut de ligne dans l'affichage, sinon, fait le saut)\ntable_list() donne la liste de toutes les tables\n(+ les table sont définit au niveau de l'exe, elles sont donc global pour tous pas comme les variables)\n(fichier des 'table' : table_frc.txt)\n");
            //fflush(stdout);
            printf("autre :\nprompt(argument) fonctionne comme un print (! pas de [/n]) mais ce remplace une fois que l'utilisateur entre une valeur (ou autre prog)\nthreading(script) permet de lancer un script en parallèle sous la même forme qu'on script classique [[[{}]]]\nclose_threading() arrête tous les threads en cours\nsleep(valeur) applique un temps de pause en milliseconde souhaité\nscreen_size(x) et ecran_taille(x) donne la valeur de la largeur de l'écran en pixel,\nscreen_size(y) et ecran_taille(y) la valeur de sa hauteur.\ntext(argument) et num(argument) son des fonctions pour les print() et les table,\nelles permettent de préciser le tipe à l'enregistrement et de forcer l'affichage.\n");
            //fflush(stdout);
            printf("dll|nomdeladll|[fonctiondeladll(argumentdeladll)] execute une dll à l'emplacement du nomdeladll\n(soit le chemin soit définit avec define_dll(nom)(chemin))\npour créer une dll (de préférence en c car fr-simplecode est en c), utiliser uniquement des fonction sous cette forme :\n__declspec(dllexport) const char* \"nomdelafonction\"(char* \"arg\")\nutilisez un return final pour que le programme fonctionne même si vous n'avez pas besoin de arg\n###dll_arg_analysis:true### et ###analyse_argument_dll:oui### permettent de traité l'argument de la fonction de la dll comme les print()\n###dll_arg_analysis:false### et ###analyse_argument_dll:non### le désactive.\nfree_dll() et dll_libre() libère les buffers de stockage de toutes les dll (! si la dll utilise threading, erreur fatale si il n'y a pas de protection !)\nla dll peut être protéger contre free_dll() par __declspec(dllexport) volatile int frc_interrupted_dll = 0; lorsqu'elle passe à 1, la dll est tué (200ms)\n");
            //fflush(stdout);
            printf("###fonction_num:oui###, ###function_num:true### et ###fonction_num:true### active la fonction num() si elle est appelé,\n###fonction_num:non###, ###function_num:false### et ###fonction_num:false### désactive la fonction num()\n###fonction_text:oui###, ###function_text:true### et ###fonction_text:true### active la fonction text() si elle est appelé,\n###fonction_text:non###, ###function_text:false### et ###fonction_text:false### la désactive\n");
            //fflush(stdout);
            printf("###autorise/:oui### et ###autorise/:true### permettent de désactiver les fonction num() et text() si elles sont précédé d'un /,\n###autorise/:non### et ###autorise/:false### active l'autorisation d'un / devant num() et text() sans désactiver leurs fonctions (si elles sont actives)\ntext() peut inclure des fonctions parseur de texte : text_replace(\"chaine\";\"cherche\";\"remplace\") : chaine est la chaine de texte que l'on veut modifier,\n cherche et ce que l'on va changer dans la chaine, et remplace est la valeur qui remplace cherche dans la chaine.\n###table_use_script_path:true### et ###table_use_script_path:oui### permettent d'enregistré dans la table \"frc_use_script_path\"\nle chemin utilisé lorsque l'on utilise use_script()[],\n###table_use_script_path:false### et ###table_use_script_path:non### désactive l'enregistrement dans la table\n##state_table_use_script_path## renvoie 1 ou 0 si l'enregistrement dans la table est actif ou non\n");
            //fflush(stdout);
            printf("###debug_use_script:true### et ###debug_use_script:oui### affiche le script avant sont exécution lorsque l'on utilise use_script()[],\n###debug_use_script:false### et ###debug_use_script:non### n'affiche pas le script de use_script()[]\n###sortie_erreur:oui###, ###sortie_erreur:true### et ###output_error:true### envoie les messages d'erreurs sur stderr (sortie d'erreur de windows),\n###sortie_erreur:non###, ###sortie_erreur:false### et ###output_error:false### envoie les erreurs sur la sortie standard (stdout), c'est l'état initial.\n###stop_script_erreur:oui###, ###stop_script_erreur:true### et ###stop_script_error:true### arrête l'exécution du script si il y a une erreur (en dev),\n###stop_script_erreur:non###, ###stop_script_erreur:false### et ###stop_script_error:false### laisse passé les erreurs (mode initial)\n");
            //fflush(stdout);
            printf("variable unique bool partagée (mieux pour du threading en opération rapide que des variable pour une condition):\n###global_bool:none### est l'état initial (-1),\n###global_bool:true### et ###global_bool:false### sont les deux autres position\n##state_global_bool## récupère la valeur de global bool -1 si none, 0 si false et 1 si true.\n");
            //fflush(stdout);
            printf("CONF des variables systèmes:\nun fichier fr-simplecode.conf est créé automatiquement et contient les variables d'exécution au démarrage de chaque script, sous la forme des ###...###,\nvous pouvez les modifier directement dans le fichier pour changer les configurations sans avoir à les repréciser dans votre script.\nPour ouvrir le fichier, tapez conf ou CONF.\n");
            fflush(stdout);
            printf("comparateur de texte grâce à text_eval(\"arg1\";op;\"arg2\") (disponible en temps que nombre mais pas sur text())\narg1 est le texte que l'on annalyse, arg2 est le texte recherché en fonction de op (renvoie 0 ou 1)\nvaleur de op : ?= et != pour savoir si il sont identiques ou non, /= et \\= si arg1 commence ou fini par arg2, #= et !#= si arg1 contient ou non arg2\ntext_replace(\"arg1\";\"arg2\";\"arg3\") permet de modifier du texte, arg1 est le texte modifié, arg2 est la recherche dans arg1 qui sera remplacer par arg3\ntext_cut(\"arg\";index1;index2) supprime de arg le texte compris entre index1 et index2 (si un index est négatif, par de la fin), fonctionne dans text()\ntext_letter(\"arg\";index) renvoie le caractère visé par index (si négatif, par de la fin), fonctionne dans text()\ntext_lowercase(\"arg\") met arg en majuscule, fonctionne dans text()\ntext_uppercase(\"arg\") met arg en minuscule, fonctionne dans text()\ntext_word(\"arg\";index) renvoie le mot visé par index (si positif, du début, sinon de la fin), fonctionne dans text()\ntext_len(\"arg\") renvoie le nombre de lettre de arg (! pas dans text())\ntext_count(\"arg1\";\"arg2\") renvoie le nombre de fois que arg2 est présent dans arg1 (! pas dans text())\ntext_find(\"arg1\";\"arg2\";index) renvoie la position du premier charactère cherché par arg2 trouvé dans arg1 et du début ou de la fin en fonction de index\n");
            fflush(stdout);
            printf("les variables locales (! pas de fichier mémoire comme var||, uniquement dans l'application):\n var_local|nom|=value permet de créer une variable local, value peut être un texte si \" \", sinon un nombre,\net var_local|| permet de la récupérer. var_local_env() affiche l'environnement actuel (débugage), clear_var_local_env() et efface_var_local_env()\néfface tout les environnements hors global (! uniquement en global sinon erreur).\nfree_var_local() et libère_var_local() supprime toutes les variables de l'environnement actuel (pas l'environnement)\nvar_local_list() affiche toutes les variables de tous les environnements (seulement hors script sur la console)\nvar_local_suppr(nomdelavariable) supprime une variable donné dans l'environnement actuel si elle existe sinon erreur.\n");
            fflush(stdout);
            continue;
        } else if (strcmp(command, "version\n") == 0){
            char *exec_path = get_executable_path();
            printf("VERSION : 0.3\nutilisation win64_bit, path : %s\n", exec_path);
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
                printf("fonction 'random[nombre1,nombre2]' renvoie un nombre entier aléatoire compris entre les 2 nombres entiers choisi par l'utilisateur\n(! pas de graine, utilise random(;) depuis la v0.3 pour un meilleur fonctionnement)\n");
            } else if (strncmp(command,"fonction(execute())",19)==0 || strncmp(command,"fonction(execute)",17)==0){
                printf("'execute()' permet de passer des commandes de type cmd depuis l'application, par exemple pour démarer un programme grâce à la fonction start du cmd ...\n");
            } else {
                printf("Erreur : 'fonction(argument)' argument inconnu,\ntapez 'info' pour connaître les différentes fonctions !\n(info, à parit de la 0.2, les nouvelles fonctions ne sont plus expliqués, il ne reste que les fonctions de la calculatrice !)\n");
            }
            fflush(stdout);
            continue;
        } else if (strcmp(command,"script_example\n")==0 || strcmp(command,"script_exemple\n")==0){
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
        } else if (strcmp(command,"licence\n")==0){
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
        } else if (_stricmp(command, "conf\n") == 0) { //CONF et conf
            start_frc_conf(); //ouvrir le fichier de configuration
            continue;
        }

        //les commandes nano pour les fichier frc (création exécution et déstruction sécurisé, aucun fichier local indésirable accèssible)
        if (strncmp(command,"nano frc ",9)==0){
            handle_nano_frc(command);
            continue;
        } else if (strncmp(command,"nano suppr frc ",15)==0){
            handle_nano_suppr_frc(command);
            continue;
        } else if (strncmp(command,"nano start frc ",15)==0){
            mode_script_output = 0; //assurer qu'il n'affiche pas le détaille du script
            get_frc_conf(); //si conf existe, on l'applique
            handle_nano_start_frc(command);
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
            continue;
        }

        if (strncmp(command,"affiche(",8)==0 || strncmp(command,"print(",6)==0){
            fonction_script_command_for_affiche(command);
            continue;
        }

        //affiche l'environnement actuel pour les variables local (var_local)
        if (strncmp(command,"var_local_env()",15)==0){
            printf("l'environnement actuel des var_local est : %s\n",current_env_object());
            fflush(stdout);
            continue;
        }

        //supprimé les environnements inutilisé si on est en environnement global
        if (strncmp(command,"clear_var_local_env()",21)==0 || strncmp(command,"efface_var_local_env()",22)==0) {
            if (strncmp(current_env_object(),"global",6)==0) {
                clear_env_object_except_global();
                printf("Les variables locales des environnements autres que global sont supprimés\n");
                fflush(stdout);
                continue;
            }
        }

        if (strncmp(command,"var_local_list()",16)==0){
            printf("liste des variables locales :\n%s",list_env_object());
            fflush(stdout);
            continue;
        }

        //supprimé toutes les variables local de l'environnement actuel
        if (strncmp(command,"free_var_local()",16)==0 || strncmp(command,"libere_var_local()",18)==0 || strncmp(command,"libère_var_local()",18)==0) {
            if (free_var_env_object(current_env_object()) == 0) {
                printf("Erreur : system error in env in free_var_local(), please exit and restart\n");
                fflush(stdout);
            }
            continue;
        }

        //supprimé une variable local de l'environnement actuel
        if (strncmp(command,"var_local_suppr(",16)==0){
            handle_var_local_suppr(command);
            continue;
        }

        int reminder_block_executed = 0;
        if (strncmp(command, "INDEX_BLOCK_EXECUTED[[[", 23) == 0) {
            // Décaler la chaîne pour commencer directement à "[[["
            memmove(command, command + 20, strlen(command + 20)+1);
            // Flag pour se rappeler que ce bloc est marqué comme exécuté
            reminder_block_executed = 1;
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
            debug_use_script=false;
            var_global_bool=-1; //met global_bool a none à chaque nouveaux programme
            error_in_stderr=false; //erreur sur stdout
            error_lock_program=false; //les erreurs n'arrête pas le script
            get_frc_conf(); //si conf existe, on l'applique de préférence (de plus il ne gère pas toutes celles présente ici, uniquement celle configurable par les ###...###)
            block_execute=0;
            handle_script(command);
            if (reminder_block_executed==1){
                printf("block_executed : %d\n",(int)block_execute);
                fflush(stdout);
            }
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
        if (strstr(command,"screen_size(x)")!=NULL || strstr(command,"screen_size(y)")!=NULL || strstr(command,"ecran_taille(x)")!=NULL || strstr(command,"ecran_taille(y)")!=NULL){
            char *save_size = replace_screen_dimensions(command);
            strcpy(command,save_size);
            free(save_size);
        }

        //remplacer les random[,]
        handle_random(command,last_result);
        //remplacer les random(;)
        if (strstr(command,"random(")!= NULL){
            strcpy(command, handle_random2(command));
        }

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

        //table_list()
        if (strncmp(command, "table_list()", 12) == 0){
            table_list();
            continue;
        }

        if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.sea(") != NULL) {
            table_sea(command);
            continue;
        }

        //les table||[] et table||.len()
        if (strstr(command, "table|") != NULL && strstr(command, "|[") != NULL) {
            strcpy(command,for_get_table_value(command));
            //printf("%s\n",command);
        }
        if (strstr(command, "table|") != NULL && strstr(command, "|.len(") != NULL) {
            strcpy(command,for_len_table(command));
            //printf("%s\n",command);
        }

        //enregistré la variable local
        if (strncmp(command,"var_local|",10)==0 && strstr(command,"|=") != NULL){
            char* successs = handle_var_local_assignment(command);
            if (strncmp(successs,"1",1)==0){
                continue;
            }
        }

        //récupérer la varable local
        if (strstr(command,"var_local|")!=NULL){
            handle_var_local_get(command,MAX_LEN);
        }

        if (strstr(command, "var|") != NULL) {
            //remplacer les in_var()
            replace_in_var(command);
            // Remplacer "rep" dans l'expression par la valeur de last_result
            strcpy(command, replace_rep(command, last_result));
            // Gérer toutes les occurrences de sqrt(...) et cbrt(...) dans la commande
            strcpy(command, handle_sqrt(command, last_result));  // Remplace sqrt(...) par son résultat
            strcpy(command, handle_cbrt(command, last_result));  // Remplace cbrt(...) par son résultat
            //pour les var||=++ ou var||=--
            char *equal_pos = strchr(command, '=');
            if (equal_pos != NULL) {
                char prefixxx[256] = {0};
                strncpy(prefixxx, command, equal_pos - command);
                prefixxx[equal_pos - command] = '\0';

                char *after_equal = equal_pos + 1;

                if (strncmp(after_equal, "++", 2) == 0) {
                    snprintf(command, sizeof(command), "%s=%s+1", prefixxx, prefixxx);
                } else if (strncmp(after_equal, "--", 2) == 0) {
                    snprintf(command, sizeof(command), "%s=%s-1", prefixxx, prefixxx);
                }
            }

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

        //les table|| action sur le fichier table_frc.txt
        if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.add(") != NULL){
            int result = add_table(command);
            //printf("add operation: %d\n",result);
            if (result==-2){
                printf("Erreur: problème de ( ) sur table||.add()\n");
                fflush(stdout);
            }
            if (result==-1){
                printf("Erreur: sur lecture/écriture fichier table_frc.txt\n");
                fflush(stdout);
            }
            continue;
        }

        if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.edit(") != NULL){
            int result = process_table_edit(command);
            //printf("edit operation: %d\n",result);
            continue;
        }

        if (strncmp(command, "table|", 6) == 0 && strstr(command, "|.del(") != NULL){
            int result = process_table_del(command);
            //printf("del operation: %d\n",result);
            continue;
        }

        if (strncmp(command, "table|", 6) == 0 && ((strstr(command, "|=[[") != NULL)|| (strstr(command, "|=join(") != NULL) || (strstr(command, "|=suppr()") != NULL))) {
            int result = parse_and_save_table(command);
            if (result==4){
                printf("Erreur: parenthèses non équilibré dans la table !\n");
                fflush(stdout);
            } else if (result==3){
                printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                fflush(stdout);
            } else if (result==2){
                printf("Erreur: ]] manquante !.\n");
                fflush(stdout);
            }else if (result==1) {
                printf("Table enregistrée avec succès.\n");
                fflush(stdout);
            } else {
                printf("Erreur lors de l'enregistrement de la table.\n");
                fflush(stdout);
            }
            continue;
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
