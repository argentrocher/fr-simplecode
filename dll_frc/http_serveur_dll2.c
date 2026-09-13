#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <psapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")

#define BUFFER_SIZE 8192

//gcc -shared -o http_serveur_dll2.dll http_serveur_dll2.c -lws2_32 -lpsapi -Wl,--add-stdcall-alias

//[[[{print("text(dll|http_serveur_dll2|[start_http_server("8080;web_http_serveur_test.html")])")}]]]

//def close pour fonction start
__declspec(dllexport) const char* close_http_server(const char* unused);

__declspec(dllexport) volatile int frc_interrupted_dll = 0;

static SOCKET server_socket = INVALID_SOCKET;
static HANDLE server_thread = NULL;
static volatile int server_running = 0;

//donné reçu par la page web
static char post_data[BUFFER_SIZE] = {0};
static char get_data[BUFFER_SIZE] = {0};

//chemin de la page web à exécuté
static char base_html_path[1024] = {0};

//stocke les infos de la page web
static char info_web_data[BUFFER_SIZE] = {0};
//stocke l'heure de la requête
static char info_time_data[256] = {0};
//stocke l'ip du client de la requête
static char info_ip_data[64] = {0};
//pour mettre des div ou non à la place des balises frc 0=non 1=oui
static int replace_frc_tags = 0;

//chemin de répertoire qui peu comptenir des fichiers html/imgage en passant par les url
static char base_directory[BUFFER_SIZE] = {0};

//chemin d'une image .ico définit pour l'icon du navigateur
static char favicon_directory[BUFFER_SIZE] = {0};

//nom du serveur dans les requêtes
static char server_name[BUFFER_SIZE/2] = {0};

static int port = 8080;

CRITICAL_SECTION data_lock;

DWORD WINAPI server_loop(LPVOID lpParam);

//savoir si un fichier existe
int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

//inversé les / en  //
void normalize_path(char* path) {
    for (char* p = path; *p; ++p) {
        if (*p == '/') *p = '\\';
    }
}

//enlevé les caractères % et les + par des espaces
void url_decode(char *dst, const char *src) {
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

//partie SERVER-IP partout sur la page web fournit
// Obtenir l'adresse IP locale du serveur (en dehors de 127.0.0.1)
void get_local_ip(char* ip_buffer, size_t buffer_size) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        strncpy(ip_buffer, "127.0.0.1", buffer_size);
        return;
    }

    struct hostent* he = gethostbyname(hostname);
    if (!he || !he->h_addr_list[0]) {
        strncpy(ip_buffer, "127.0.0.1", buffer_size);
        return;
    }

    struct in_addr addr;
    memcpy(&addr, he->h_addr_list[0], sizeof(struct in_addr));
    strncpy(ip_buffer, inet_ntoa(addr), buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';
}

// Fonction de remplacement de <${SERVER-IP!}$> sur la page web fournit
char* process_server_ip_tags(const char* html_raw) {
    const char* tag = "<${SERVER-IP!}$>";
    size_t tag_len = strlen(tag);

    char server_ip[64];
    get_local_ip(server_ip, sizeof(server_ip));

    size_t input_len = strlen(html_raw);
    size_t ip_len = strlen(server_ip);

    // Taille initiale : au moins la taille du HTML original
    size_t capacity = input_len + 1;

    char* output = malloc(capacity);
    if (!output)
        return NULL;

    size_t out_len = 0;
    const char* p = html_raw;

    while (*p) {
        const char* found = strstr(p, tag);

        if (!found) {
            size_t remaining = strlen(p);

            if (out_len + remaining + 1 > capacity) {
                size_t new_capacity = capacity;

                while (out_len + remaining + 1 > new_capacity)
                    new_capacity *= 2;

                char* tmp = realloc(output, new_capacity);
                if (!tmp) {
                    free(output);
                    return NULL;
                }

                output = tmp;
                capacity = new_capacity;
            }

            memcpy(output + out_len, p, remaining);
            out_len += remaining;
            break;
        }

        // Copier ce qu'il y a avant le tag
        size_t before = (size_t)(found - p);

        if (out_len + before + ip_len + 1 > capacity) {
            size_t new_capacity = capacity;

            while (out_len + before + ip_len + 1 > new_capacity)
                new_capacity *= 2;

            char* tmp = realloc(output, new_capacity);
            if (!tmp) {
                free(output);
                return NULL;
            }

            output = tmp;
            capacity = new_capacity;
        }

        memcpy(output + out_len, p, before);
        out_len += before;

        // Remplacement par l'IP
        memcpy(output + out_len, server_ip, ip_len);
        out_len += ip_len;

        p = found + tag_len;
    }

    output[out_len] = '\0';

    return output;
}
//fin de partie <${SERVER-IP!}$> sur la page web fournit


//fonction pour les performances de l'application
char* get_app_performance_info() {
    static char info[256];
    PROCESS_MEMORY_COUNTERS pmc;
    FILETIME createTime, exitTime, kernelTime, userTime;

    HANDLE hProcess = GetCurrentProcess();

    // Mémoire
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        SIZE_T memKB = pmc.WorkingSetSize / 1024;

        // Temps CPU
        if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kernelTime.dwLowDateTime;
            k.HighPart = kernelTime.dwHighDateTime;
            u.LowPart = userTime.dwLowDateTime;
            u.HighPart = userTime.dwHighDateTime;
            unsigned long totalCPUms = (DWORD)((k.QuadPart + u.QuadPart) / 10000);

            sprintf(info, "RAM: %lu KB; CPU: %lu ms", (unsigned long)memKB, totalCPUms);
        } else {
            sprintf(info, "RAM: %lu KB; CPU: N/A", (unsigned long)memKB);
        }
    } else {
        strcpy(info, "RAM/CPU: N/A");
    }

    return info;
}


//fonction pour extraire les valeur correspondant au clé dans les données get_data et post_data
void extract_value_from_data(const char* source, const char* key, char* dest, size_t dest_size) {
    dest[0] = '\0';  // Par défaut, vide

    const char* p = source;

    //printf("source : %s, clé : %s\n",p,key);

    size_t key_len = strlen(key);
    while ((p = strstr(p, key))) {
        // Vérifie que c’est bien la bonne clé (ex: "nom=" et non "prenom=")
        if ((p == source || *(p - 1) == ';' || *(p - 1) == '[') && p[key_len] == '=') {
            p += key_len + 1; // saute "clé="

            size_t i = 0;
            while (*p && *p != ';' && *p != ']' && i < dest_size - 1) {
                dest[i++] = *p++;
            }
            dest[i] = '\0';
            return;
        }
        p += key_len; // avance pour ne pas reboucler sur la même position
    }

    strcpy(dest, "0"); // clé non trouvée
}

//suppression des balises de code <${ }$> dans le fichier à executé par le programme lanceur
void web_page_frc_methode(char **input) {
    if (!input || !*input)
        return;

    size_t capacity = BUFFER_SIZE;
    size_t output_len = 0;

    char *buffer = (char *)malloc(capacity);
    if (!buffer)
        return;

    buffer[0] = '\0';

    const char *p = *input;

#define APPEND_TEXT(src, n) do {                                      \
        size_t _n = (n);                                             \
        if (output_len + _n + 1 > capacity) {                       \
            size_t _new_capacity = capacity;                        \
            while (output_len + _n + 1 > _new_capacity)              \
                _new_capacity *= 2;                                 \
            char *_tmp = (char *)realloc(buffer, _new_capacity);    \
            if (!_tmp) {                                            \
                free(buffer);                                       \
                return;                                             \
            }                                                        \
            buffer = _tmp;                                          \
            capacity = _new_capacity;                               \
        }                                                            \
        memcpy(buffer + output_len, (src), _n);                     \
        output_len += _n;                                            \
        buffer[output_len] = '\0';                                   \
    } while (0)

    while (*p) {

        const char *start = strstr(p, "<${");

        if (!start) {
            APPEND_TEXT(p, strlen(p));
            break;
        }

        // Copie le texte avant
        APPEND_TEXT(p, (size_t)(start - p));

        const char *end = strstr(start, "}$>");

        if (!end) {
            // Pas bien formé : copie tout le reste
            APPEND_TEXT(p, strlen(p));
            break;
        }

        char code[128] = {0};

        size_t code_len = (size_t)(end - (start + 3));

        if (code_len >= sizeof(code))
            code_len = sizeof(code) - 1;

        memcpy(code, start + 3, code_len);
        code[code_len] = '\0';

        if (strcmp(code, "GET!") == 0) {
            const char *value = get_data && *get_data ? get_data : "0";
            APPEND_TEXT(value, strlen(value));
        } else if (strcmp(code, "POST!") == 0) {
            const char *value = post_data && *post_data ? post_data : "0";
            APPEND_TEXT(value, strlen(value));
        } else if (strncmp(code, "GET:", 4) == 0) {
            const char *key = code + 4;
            char val[512] = {0};
            extract_value_from_data(get_data, key, val, sizeof(val));
            APPEND_TEXT(val, strlen(val));
        } else if (strncmp(code, "POST:", 5) == 0) {
            const char *key = code + 5;
            char val[512] = {0};
            extract_value_from_data(post_data, key, val, sizeof(val));
            APPEND_TEXT(val, strlen(val));
        } else if (strcmp(code, "GET.KEYS!") == 0) {
            char temp[BUFFER_SIZE] = "[[";
            const char* src = get_data;
            while (*src) {
                if (strncmp(src, "[[", 2) == 0) {
                    src += 2; // skip [[
                }
                const char* equal = strchr(src, '=');
                const char* semicolon = strchr(src, ';');
                const char* closing = strstr(src, "]]");
                if (!equal) break;
                // ignore si un ; vient avant =
                if (semicolon && semicolon < equal) {
                    src = semicolon + 1;
                    continue;
                }
                // ajoute la clé si tout va bien
                size_t key_len = (size_t)(equal - src);
                if (strlen(temp) + key_len + 1 < sizeof(temp)) {
                    strncat(temp, src, key_len);
                    strcat(temp, ";");
                }
                // avancer
                if (semicolon && (!closing || semicolon < closing)) {
                    src = semicolon + 1;
                } else if (closing) {
                    break;
                } else {
                    break;
                }
            }
            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';')
                temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            if (temp[2])
                APPEND_TEXT(temp, strlen(temp));
            else
                APPEND_TEXT("0", 1);
        } else if (strcmp(code, "GET.VALUES!") == 0) {
            char temp[BUFFER_SIZE] = "[[";
            const char* src = get_data;
            while (*src) {
                if (strncmp(src, "[[", 2) == 0) {
                    src += 2; // skip [[
                }
                const char* equal = strchr(src, '=');
                if (!equal) break;
                const char* end = strchr(equal, ';');
                const char* closing = strstr(equal, "]]");
                if (end && (!closing || end < closing)) {
                    size_t value_len = (size_t)(end - (equal + 1));
                    if (strlen(temp) + value_len + 1 < sizeof(temp)) {
                        strncat(temp, equal + 1, value_len);
                        strcat(temp, ";");
                    }
                    src = end + 1;
                } else if (closing) {
                    size_t value_len = (size_t)(closing - (equal + 1));
                    if (strlen(temp) + value_len + 1 < sizeof(temp)) {
                        strncat(temp, equal + 1, value_len);
                    }
                    break;
                } else {
                    break;
                }
            }
            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';')
                temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            if (temp[2])
                APPEND_TEXT(temp, strlen(temp));
            else
                APPEND_TEXT("0", 1);
        } else if (strcmp(code, "POST.KEYS!") == 0) {
            char temp[BUFFER_SIZE] = "[[";
            const char* src = post_data;
            while (*src) {
                if (strncmp(src, "[[", 2) == 0) {
                    src += 2; // skip [[
                }
                const char* equal = strchr(src, '=');
                const char* semicolon = strchr(src, ';');
                const char* closing = strstr(src, "]]");
                if (!equal) break;
                // ignore si un ; vient avant =
                if (semicolon && semicolon < equal) {
                    src = semicolon + 1;
                    continue;
                }
                // ajoute la clé si tout va bien
                size_t key_len = (size_t)(equal - src);
                if (strlen(temp) + key_len + 1 < sizeof(temp)) {
                    strncat(temp, src, key_len);
                    strcat(temp, ";");
                }
                // avancer
                if (semicolon && (!closing || semicolon < closing)) {
                    src = semicolon + 1;
                } else if (closing) {
                    break;
                } else {
                    break;
                }
            }
            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';')
                temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            if (temp[2])
                APPEND_TEXT(temp, strlen(temp));
            else
                APPEND_TEXT("0", 1);
        } else if (strcmp(code, "POST.VALUES!") == 0) {
            char temp[BUFFER_SIZE] = "[[";
            const char* src = post_data;
            while (*src) {
                if (strncmp(src, "[[", 2) == 0) {
                    src += 2; // skip [[
                }
                const char* equal = strchr(src, '=');
                if (!equal) break;
                const char* end = strchr(equal, ';');
                const char* closing = strstr(equal, "]]");
                if (end && (!closing || end < closing)) {
                    size_t value_len = (size_t)(end - (equal + 1));
                    if (strlen(temp) + value_len + 1 < sizeof(temp)) {
                        strncat(temp, equal + 1, value_len);
                        strcat(temp, ";");
                    }
                    src = end + 1;
                } else if (closing) {
                    size_t value_len = (size_t)(closing - (equal + 1));
                    if (strlen(temp) + value_len + 1 < sizeof(temp)) {
                        strncat(temp, equal + 1, value_len);
                    }
                    break;
                } else {
                    break;
                }
            }
            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';')
                temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            if (temp[2])
                APPEND_TEXT(temp, strlen(temp));
            else
                APPEND_TEXT("0", 1);
        } else if (strcmp(code, "GET#FREE") == 0) {
            if (get_data) {
                APPEND_TEXT("1", 1);
                get_data[0] = '\0';
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strcmp(code, "POST#FREE") == 0) {
            if (post_data) {
                APPEND_TEXT("1", 1);
                post_data[0] = '\0';
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strncmp(code, "REDIRECT_PAGE:", 14) == 0) {
            const char* page = code + 14;
            char fullpath[512] = {0};
            strcpy(fullpath, page);
            // Ajouter ".html" si manquant
            if (!(strlen(fullpath) >= 5 &&
                  strcmp(fullpath + strlen(fullpath) - 5, ".html") == 0)) {
                strcat(fullpath, ".html");
            }
            // Vérifie si le fichier existe
            FILE* f = fopen(fullpath, "rb");
            if (f) {
                fclose(f);
                strcpy(base_html_path, fullpath);  // redirige
                APPEND_TEXT("1", 1);
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strcmp(code, "INFO:TIME") == 0) {
            APPEND_TEXT(info_time_data, strlen(info_time_data));
        } else if (strcmp(code, "INFO:TIME.MS") == 0) {
            // Récupère les millisecondes (extrait après le dernier '.')
            const char* dot = strrchr(info_time_data, '.');
            if (dot)
                APPEND_TEXT(dot + 1, strlen(dot + 1));
            else
                APPEND_TEXT("0", 1);
        } else if (strcmp(code, "INFO:WEB") == 0) {
            APPEND_TEXT(info_web_data, strlen(info_web_data));
        } else if (strcmp(code, "INFO:WEB.HOST") == 0) {
            const char* start = strstr(info_web_data, "Host: ");
            const char* end = strstr(info_web_data, "User-Agent:");
            if (start && end && end > start) {
                char temp[256] = {0};
                size_t len = (size_t)(end - (start + 6));
                if (len >= sizeof(temp))
                    len = sizeof(temp) - 1;
                strncpy(temp, start + 6, len);
                temp[len] = '\0';
                APPEND_TEXT(temp, strlen(temp));
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strcmp(code, "INFO:WEB.USER-AGENT") == 0) {
            const char* start = strstr(info_web_data, "User-Agent: ");
            const char* end = strstr(info_web_data, "Accept:");
            if (start && end && end > start) {
                char temp[512] = {0};
                size_t len = (size_t)(end - (start + 12));
                if (len >= sizeof(temp))
                    len = sizeof(temp) - 1;
                strncpy(temp, start + 12, len);
                temp[len] = '\0';
                APPEND_TEXT(temp, strlen(temp));
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strcmp(code, "INFO:WEB.ACCEPT") == 0) {
            const char* start = strstr(info_web_data, "Accept: ");
            if (start) {
                APPEND_TEXT(start + 8, strlen(start + 8));
            } else {
                APPEND_TEXT("0", 1);
            }
        } else if (strcmp(code, "INFO:PORT") == 0) {
            char port_str[16];
            sprintf(port_str, "%d", port);
            APPEND_TEXT(port_str, strlen(port_str));
        } else if (strcmp(code, "INFO:FRC_TAGS") == 0) {
            char frctags_str[16];
            sprintf(frctags_str, "%d", replace_frc_tags);
            APPEND_TEXT(frctags_str, strlen(frctags_str));
        } else if (strcmp(code, "INFO:IP") == 0) {
            const char *value = info_ip_data[0] ? info_ip_data : "0";
            APPEND_TEXT(value, strlen(value));
        } else if (strcmp(code, "INFO:LOCALISATION") == 0) {
            LANGID langid = GetSystemDefaultLangID();
            char lang[24] = {0};
            switch (PRIMARYLANGID(langid)) {
                case LANG_FRENCH:
                    strcpy(lang, "fr");
                    break;
                case LANG_ENGLISH:
                    strcpy(lang, "en");
                    break;
                case LANG_GERMAN:
                    strcpy(lang, "de");
                    break;
                case LANG_SPANISH:
                    strcpy(lang, "es");
                    break;
                case LANG_ITALIAN:
                    strcpy(lang, "it");
                    break;
                case LANG_JAPANESE:
                    strcpy(lang, "ja");
                    break;
                case LANG_CHINESE:
                    strcpy(lang, "zh");
                    break;
                case LANG_RUSSIAN:
                    strcpy(lang, "ru");
                    break;
                default:
                    strcpy(lang, "unknown");
                    break;
            }
            // Ajout du pays s'il est connu (LANGID secondaire)
            char full_lang[32] = {0};
            sprintf(full_lang, "%s-%02x", lang, SUBLANGID(langid));
            APPEND_TEXT(full_lang, strlen(full_lang));
        } else if (strcmp(code, "INFO:PROCESS") == 0) {
            const char *value = get_app_performance_info();
            if (value)
                APPEND_TEXT(value, strlen(value));
            else
                APPEND_TEXT("0", 1);
        } else {
            APPEND_TEXT("0", 1);
        }
        p = end + 3;
    }

    free(*input);
    *input = buffer;

#undef APPEND_TEXT
}

//fonction qui execute l'exe fr-simplecode pour les balises <frc> dans le code
int run_command_and_capture_output(const char* cmd, char* output, size_t output_size) {
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return 0;
    }

    // Assure-toi que stdout du fils va vers notre pipe
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = { 0 };

    // Créer une copie modifiable de la commande (CreateProcess la modifie)
    char cmdline[1024];
    strncpy(cmdline, cmd, sizeof(cmdline) - 1);

    BOOL success = CreateProcessA(
        NULL,         // Nom de l'exe (NULL = dans la commande)
        cmdline,      // Ligne de commande (modifiable)
        NULL, NULL,   // sécurité
        TRUE,         // Hérite des handles (stdout)
        0,            // Pas de flags spéciaux
        NULL, NULL,   // environnement et dossier courant
        &si, &pi
    );

    CloseHandle(hWritePipe); // Fermer côté écriture dans le parent

    if (!success) {
        CloseHandle(hReadPipe);
        return 0;
    }

    // Lire la sortie du programme
    DWORD readBytes;
    char buffer[512];
    size_t total = 0;
    output[0] = '\0';

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &readBytes, NULL) && readBytes > 0) {
        buffer[readBytes] = '\0';
        if (total + readBytes < output_size - 1) {
            strcat(output, buffer);
            total += readBytes;
        } else {
            break; // éviter de dépasser la taille
        }
    }

    // Attendre que le programme se termine
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return 1;
}


//fonction pour les balises frc dans la page html --> run_command_and_capture_output pour lancer l'exe
char* process_frc_tags(const char* html_raw) {

    size_t input_len = strlen(html_raw);

    size_t capacity = input_len + 1;

    char* final_html = malloc(capacity);
    if (!final_html)
        return NULL;

    size_t out_len = 0;

    const char* current = html_raw;

    while (1) {

        const char* start = strstr(current, "<frc>");

        if (!start) {
            // Ajouter le reste
            size_t remaining = strlen(current);

            if (out_len + remaining + 1 > capacity) {

                size_t new_capacity = capacity;

                while (out_len + remaining + 1 > new_capacity)
                    new_capacity *= 2;

                char* tmp = realloc(final_html, new_capacity);

                if (!tmp) {
                    free(final_html);
                    return NULL;
                }

                final_html = tmp;
                capacity = new_capacity;
            }

            memcpy(final_html + out_len, current, remaining);
            out_len += remaining;

            break;
        }

        // Copier ce qui précède <frc>
        size_t before = (size_t)(start - current);

        if (out_len + before + 1 > capacity) {

            size_t new_capacity = capacity;

            while (out_len + before + 1 > new_capacity)
                new_capacity *= 2;

            char* tmp = realloc(final_html, new_capacity);

            if (!tmp) {
                free(final_html);
                return NULL;
            }

            final_html = tmp;
            capacity = new_capacity;
        }

        memcpy(final_html + out_len, current, before);
        out_len += before;


        // Chercher </frc>
        const char* end = strstr(start + 5, "</frc>");

        if (!end) {

            // Balise <frc> non terminée :
            // on remet le texte restant tel quel.
            size_t remaining = strlen(start);

            if (out_len + remaining + 1 > capacity) {

                size_t new_capacity = capacity;

                while (out_len + remaining + 1 > new_capacity)
                    new_capacity *= 2;

                char* tmp = realloc(final_html, new_capacity);

                if (!tmp) {
                    free(final_html);
                    return NULL;
                }

                final_html = tmp;
                capacity = new_capacity;
            }

            memcpy(final_html + out_len, start, remaining);
            out_len += remaining;

            break;
        }


        // =========================================================
        // Extraction du contenu <frc>...</frc>
        // =========================================================

        size_t content_len = (size_t)(end - (start + 5));

        char* content = malloc(content_len + 1);

        if (!content) {
            free(final_html);
            return NULL;
        }

        memcpy(content, start + 5, content_len);
        content[content_len] = '\0';


        // =========================================================
        // Nettoyage
        // =========================================================

        char* cleaned = malloc(content_len + 1);

        if (!cleaned) {
            free(content);
            free(final_html);
            return NULL;
        }

        const char* p = content;
        char* q = cleaned;

        while (*p) {

            // Skip espaces début de ligne
            while (*p && isspace((unsigned char)*p))
                p++;

            while (*p && *p != '\n' && *p != '\r') {
                *q++ = *p++;
            }

            if (q > cleaned && q[-1] != ' ')
                *q++ = ' ';

            while (*p == '\r' || *p == '\n')
                p++;
        }

        *q = '\0';

        free(content);


        // =========================================================
        // Traitement GET / POST etc.
        // =========================================================

        web_page_frc_methode(&cleaned);


        // =========================================================
        // Fichier temporaire
        // =========================================================

        FILE* f = fopen("http_serveur_frc.frc", "w");

        if (f) {
            fprintf(f, "%s", cleaned);
            fclose(f);
        }


        // =========================================================
        // Exécution fr-simplecode
        // =========================================================

        char exe_path[MAX_PATH];

        GetModuleFileName(NULL, exe_path, MAX_PATH);

        char command[MAX_PATH + 256];

        sprintf(command,
                "\"%s\" --start:\"http_serveur_frc.frc\"",
                exe_path);


        // =========================================================
        // Résultat dynamique
        // =========================================================

        size_t result_capacity = BUFFER_SIZE;
        size_t result_len = 0;

        char* result = malloc(result_capacity);

        if (!result) {
            free(cleaned);
            free(final_html);
            return NULL;
        }

        result[0] = '\0';

        /*
         * IMPORTANT :
         * run_command_and_capture_output() utilise actuellement
         * un buffer fixe.
         *
         * Il faut donc au minimum continuer à lui donner un
         * buffer suffisamment grand, ou modifier cette fonction
         * également si la sortie FRC peut dépasser BUFFER_SIZE.
         */
        if (!run_command_and_capture_output(
                command,
                result,
                result_capacity)) {

            printf("Erreur lors de l'exécution\n");
        }

        free(cleaned);


        // =========================================================
        // Ajouter le résultat au HTML final
        // =========================================================

        size_t result_len_tmp = strlen(result);

        size_t wrapper_len = replace_frc_tags ? 23 : 0;

        size_t required =
            out_len +
            wrapper_len +
            result_len_tmp +
            1;

        if (required > capacity) {

            size_t new_capacity = capacity;

            while (required > new_capacity)
                new_capacity *= 2;

            char* tmp = realloc(final_html, new_capacity);

            if (!tmp) {
                free(result);
                free(final_html);
                return NULL;
            }

            final_html = tmp;
            capacity = new_capacity;
        }

        if (replace_frc_tags) {

            const char* open = "<div class=\"frc\">";
            const char* close = "</div>";

            size_t open_len = 17; //strlen(open);
            size_t close_len = 6; //strlen(close);

            memcpy(final_html + out_len,
                   open,
                   open_len);

            out_len += open_len;

            memcpy(final_html + out_len,
                   result,
                   result_len_tmp);

            out_len += result_len_tmp;

            memcpy(final_html + out_len,
                   close,
                   close_len);

            out_len += close_len;

        } else {

            memcpy(final_html + out_len,
                   result,
                   result_len_tmp);

            out_len += result_len_tmp;
        }

        free(result);

        current = end + 6;
    }

    final_html[out_len] = '\0';

    return final_html;
}


// Utilitaire : supprime les virgules ou points d'un port "8080.0" => 8080 puis fait le chemin du fichier html de base pour le start server
void parse_args(const char* arg) {
    if (!arg) return;

    char port_str[16] = {0};
    int i = 0;

    // Lire jusqu'à la première virgule, point ou point-virgule
    while (arg[i] && arg[i] != ',' && arg[i] != '.' && arg[i] != ';' && i < 15) {
        port_str[i] = arg[i];
        i++;
    }
    port_str[i] = '\0';
    port = atoi(port_str);

    // Trouver le premier point-virgule pour le chemin
    const char* sep = strchr(arg, ';');
    if (sep) {
        strncpy(base_html_path, sep + 1, sizeof(base_html_path) - 1);
    }
}

void extract_query_params(const char* src, char* dest) {
    char temp[BUFFER_SIZE] = {0};
    strcpy(temp, src);
    char* token = strtok(temp, "&");
    strcat(dest, "[[");
    while (token) {
        strcat(dest, token);
        token = strtok(NULL, "&");
        if (token){
            strcat(dest, ";");
        }
    }
    strcat(dest, "]]");
}

void replace_block_collision(char* buffer) {
    char* ptr = strstr(buffer, "]][[");
    while (ptr) {
        // Remplace les 4 caractères par ';' et décale la suite
        *ptr = ';';
        memmove(ptr + 1, ptr + 4, strlen(ptr + 4) + 1);  // +1 pour le \0
        ptr = strstr(ptr + 1, "]][[");
    }
}

//ip du client socket
void me_ip_(SOCKET client_socket) {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);

    if (getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        strcpy(info_ip_data, inet_ntoa(client_addr.sin_addr));
    } else {
        strcpy(info_ip_data, "0.0.0.0"); // fallback
    }
}

//gestion en thread s'éparé de chaques demandes du navigateur
DWORD WINAPI handle_client(LPVOID param) {
    SOCKET client_socket = (SOCKET)param;
    char buffer[BUFFER_SIZE];
    int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        closesocket(client_socket);
        return 0;
    }
    buffer[received] = '\0';

    // Stockage des infos navigateur
    char* ua = strstr(buffer, "User-Agent:");
    char* host = strstr(buffer, "Host:");
    char* accept = strstr(buffer, "Accept:");

    EnterCriticalSection(&data_lock);

    me_ip_(client_socket);

    //info de la requete
    info_web_data[0] = '\0';

    if (host) {
        char line[256] = {0};
        sscanf(host, "Host: %[^\r\n]", line);
        strcat(info_web_data, "Host: ");
        strcat(info_web_data, line);
        strcat(info_web_data, "\n");
    }
    if (ua) {
        char line[512] = {0};
        sscanf(ua, "User-Agent: %[^\r\n]", line);
        strcat(info_web_data, "User-Agent: ");
        strcat(info_web_data, line);
        strcat(info_web_data, "\n");
    }
    if (accept) {
        char line[256] = {0};
        sscanf(accept, "Accept: %[^\r\n]", line);
        strcat(info_web_data, "Accept: ");
        strcat(info_web_data, line);
    }

    // Stockage de l'heure
    SYSTEMTIME t;
    GetLocalTime(&t);
    sprintf(info_time_data, "%02d:%02d:%02d.%03d",t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

    char method[8], path[1024];
    sscanf(buffer, "%s %s", method, path);
    char decoded_path[1024];
    url_decode(decoded_path, path);
    char* body = strstr(buffer, "\r\n\r\n");
    if (body) body += 4;

    if (strcmp(method, "GET") == 0) {
        char* query = strchr(decoded_path, '?');
        if (query) {
            extract_query_params(query + 1, get_data);
            replace_block_collision(get_data);
        }
    } else if (strcmp(method, "POST") == 0 && body) {
        extract_query_params(body, post_data);
        replace_block_collision(post_data);
    }


    // Vérifie si la requête est pour favicon.ico
    if (strstr(decoded_path, "favicon.ico")) {
        //char ico_path[BUFFER_SIZE] = {0};

        // On vérifie que le répertoire favicon est bien défini
        if (favicon_directory[0]) {
            //snprintf(ico_path, sizeof(ico_path), "%s\\favicon.ico", favicon_directory);

            FILE* ico_file = fopen(favicon_directory, "rb");
            if (ico_file) {
                fseek(ico_file, 0, SEEK_END);
                long ico_len = ftell(ico_file);
                fseek(ico_file, 0, SEEK_SET);

                if (ico_len <= 0 || ico_len > 10 * 1024 * 1024) { // Sécurité : 10 Mo max
                    fclose(ico_file);
                    // 413 Payload Too Large (optionnel)
                    char header[BUFFER_SIZE];
                    snprintf(header, sizeof(header), "HTTP/1.1 413 Payload Too Large\r\nServer: %s\r\nContent-Length: 0\r\n\r\n", server_name);
                    LeaveCriticalSection(&data_lock);
                    send(client_socket, header, strlen(header), 0);
                    closesocket(client_socket);
                    return 0;
                }

                char* ico_data = malloc(ico_len);
                if (!ico_data) {
                    fclose(ico_file);
                    LeaveCriticalSection(&data_lock);
                    return 0;
                }

                fread(ico_data, 1, ico_len, ico_file);
                fclose(ico_file);

                char header[BUFFER_SIZE];
                snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/x-icon\r\n"
                "Server: %s\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n\r\n", server_name, ico_len);

                LeaveCriticalSection(&data_lock);

                send(client_socket, header, strlen(header), 0);
                send(client_socket, ico_data, ico_len, 0);
                free(ico_data);
                closesocket(client_socket);
                return 0;
            }
        }

        // Fichier introuvable -> 404 (désactiver car il peut y avoir favicon.ico non défini explicitement)
        //char header[BUFFER_SIZE];
        //snprintf(header, sizeof(header), "HTTP/1.1 404 Not Found\r\nServer: %s\r\nContent-Length: 0\r\n\r\n", server_name);
        //LeaveCriticalSection(&data_lock);
        //send(client_socket, header, strlen(header), 0);
        //closesocket(client_socket);
        //return 0;
    }

    char* query_start = strchr(decoded_path, '?');

    // Vérifie si le fichier est une image supportée
    char* ext_ptr = NULL;
    if ((ext_ptr = strstr(decoded_path, ".ico")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".png")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".jpg")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".jpeg")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".gif")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".bmp")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".webp")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".avif")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".svg")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".jp2")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".tif")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".mp4")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".m4a")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".mp3")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".wav")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".pdf")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".webm")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".ogg")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".ogv")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".mov")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".opus")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".aac")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".css")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".json")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".js")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".xml")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".txt")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".text")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".md")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".zip")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".gz")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".7z")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".exe")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".bin")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".dll")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".py")) && (!query_start || ext_ptr < query_start) ||
    (ext_ptr = strstr(decoded_path, ".frc")) && (!query_start || ext_ptr < query_start)) {

        int download_request=0;
        if (strstr(decoded_path, "?download")) download_request=1;

        // Coupe après l'extension
        const char* exts[] = { ".ico", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".avif", ".svg", ".jp2", ".tif", ".mp4", ".m4a", ".mp3", ".wav", ".pdf", ".webm", ".ogg", ".ogv", ".mov", ".opus", ".aac", ".css", ".json", ".js", ".xml", ".txt", ".text", ".md",".zip", ".gz", ".7z", ".exe", ".bin", ".dll", ".py", ".frc"};
        for (int i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
            char* ext_ptr2 = strstr(decoded_path, exts[i]);
            if (ext_ptr2) {
                ext_ptr2[strlen(exts[i])] = '\0';
                break;
            }
        }

        char* filename = strrchr(decoded_path, '/');
        filename = filename ? filename : decoded_path;

        normalize_path(decoded_path);

        if (decoded_path[0] == '\\') {
            memmove(decoded_path, decoded_path + 1, strlen(decoded_path));
        }

        char final_path[BUFFER_SIZE] = {0};
        if (file_exists(decoded_path)) {
            strcpy(final_path, decoded_path);
        } else if (base_directory[0]) {
            snprintf(final_path, sizeof(final_path), "%s\\%s", base_directory, filename);
        } else {
            // Pas trouvé
            char header[BUFFER_SIZE];
            snprintf(header, sizeof(header), "HTTP/1.1 404 Not Found\r\nServer: %s\r\nContent-Length: 0\r\n\r\n", server_name);
            LeaveCriticalSection(&data_lock);
            send(client_socket, header, strlen(header), 0);
            closesocket(client_socket);
            return 0;
        }

        if (!file_exists(final_path)) {
            // Pas trouvé
            char header[BUFFER_SIZE];
            snprintf(header, sizeof(header), "HTTP/1.1 404 Not Found\r\nServer: %s\r\nContent-Length: 0\r\n\r\n", server_name);
            LeaveCriticalSection(&data_lock);
            send(client_socket, header, strlen(header), 0);
            closesocket(client_socket);
            return 0;
        }

        // Lecture binaire
        FILE* f = fopen(final_path, "rb");
        if (!f) {
            char header[BUFFER_SIZE];
            snprintf(header, sizeof(header), "HTTP/1.1 500 Internal Server Error\r\nServer: %s\r\nContent-Length: 0\r\n\r\n", server_name);
            LeaveCriticalSection(&data_lock);
            send(client_socket, header, strlen(header), 0);
            closesocket(client_socket);
            return 0;
        }

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = malloc(len);
        if (!buffer) {
            fclose(f);
            LeaveCriticalSection(&data_lock);
            closesocket(client_socket);
            return 0;
        }

        fread(buffer, 1, len, f);
        fclose(f);


        // Détecte le type MIME
        const char* content_type = "application/octet-stream"; // fallback
        if (strstr(final_path, ".png")) content_type = "image/png";
        else if (strstr(final_path, ".jpg") || strstr(final_path, ".jpeg")) content_type = "image/jpeg";
        else if (strstr(final_path, ".gif")) content_type = "image/gif";
        else if (strstr(final_path, ".bmp")) content_type = "image/bmp";
        else if (strstr(final_path, ".webp")) content_type = "image/webp";
        else if (strstr(final_path, ".ico")) content_type = "image/x-icon";
        else if (strstr(final_path, ".svg")) content_type = "image/svg+xml";
        else if (strstr(final_path, ".tif")) content_type = "image/tiff";
        else if (strstr(final_path, ".jp2")) content_type = "image/jp2";
        else if (strstr(final_path, ".avif")) content_type = "image/avif";
        else if (strstr(final_path, ".mp4")) content_type = "video/mp4";
        else if (strstr(final_path, ".webm")) content_type = "video/webm";
        else if (strstr(final_path, ".ogv")) content_type = "video/ogg";
        else if (strstr(final_path, ".mov")) content_type = "video/quicktime";
        else if (strstr(final_path, ".ogg")) content_type = "audio/ogg";
        else if (strstr(final_path, ".m4a")) content_type = "audio/mp4";
        else if (strstr(final_path, ".mp3")) content_type = "audio/mpeg";
        else if (strstr(final_path, ".wav")) content_type = "audio/wav";
        else if (strstr(final_path, ".opus")) content_type = "audio/opus";
        else if (strstr(final_path, ".aac")) content_type = "audio/aac";
        else if (strstr(final_path, ".css")) content_type = "text/css";
        else if (strstr(final_path, ".json")) content_type = "application/json";
        else if (strstr(final_path, ".js")) content_type = "application/javascript";
        else if (strstr(final_path, ".xml")) content_type = "application/xml";
        else if (strstr(final_path, ".txt") || strstr(final_path, ".text")) content_type = "text/plain";
        else if (strstr(final_path, ".md") || strstr(final_path, ".py") || strstr(final_path, ".frc")) content_type = "text/markdown";
        else if (strstr(final_path, ".pdf")) content_type = "application/pdf";
        else if (strstr(final_path, ".zip")) content_type = "application/zip";
        else if (strstr(final_path, ".gz")) content_type = "application/gzip";
        else if (strstr(final_path, ".7z")) content_type = "application/x-7z-compressed";
        else if (strstr(final_path, ".exe") || strstr(final_path, ".bin") || strstr(final_path, ".dll")) content_type = "application/octet-stream";

        // Envoie des headers + contenu image
        char header[256];
        if (download_request) {
            //*strstr(decoded_path, "?download") = '\0';  // Coupe à ?download

            sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Server: %s\r\n"
            "Content-Length: %ld\r\n\r\n",
            filename, server_name, len);
        } else {
            snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nServer: %s\r\nContent-Length: %ld\r\n\r\n",
            content_type, server_name, len); //"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
        }

        LeaveCriticalSection(&data_lock);

        send(client_socket, header, strlen(header), 0);
        send(client_socket, buffer, len, 0);
        free(buffer);
        closesocket(client_socket);
        return 0;
    }


    char html_path[BUFFER_SIZE] = {0};

    // HTML source dynamique.
    char *html = NULL;
    size_t html_size = 0;

    const char *mapped_html = NULL;
    HANDLE html_file = INVALID_HANDLE_VALUE;
    HANDLE html_mapping = NULL;

    // HTML par défaut en cas d'erreur.
    const char *default_html = "<html><body>Erreur de chargement</body></html>";
    const char *default_error_html = "<html><body>Erreur du serveur</body></html>";

    // Essayer de prendre un fichier depuis l’URL (si se termine par .html)
    if (strstr(decoded_path, ".html")) {
        // Tronquer après ".html"
        char* html_ext = strstr(decoded_path, ".html");
        if (html_ext) {
            html_ext[5] = '\0';  // coupe après ".html"
        }

        char* filename = strrchr(decoded_path, '/');
        filename = filename ? filename : decoded_path;

        normalize_path(decoded_path);

        if (decoded_path[0] == '\\') { //c:/  aussi   && isalpha(decoded_path[1]) && decoded_path[2] == ':'
            memmove(decoded_path, decoded_path + 1, strlen(decoded_path)); // décale vers la gauche
        }

        //printf("path trouvé : %s\n",decoded_path);
        //fflush(stdout);

        // Dossier de base
        if (base_directory[0]) {
            if (file_exists(decoded_path)) {
                strcpy(html_path, decoded_path);
            } else {
                snprintf(html_path, sizeof(html_path), "%s\\%s", base_directory, filename);
                //printf("html : %s\n",html_path);
                if (!file_exists(html_path)) {
                    // Fallback
                    strncpy(html_path, base_html_path, sizeof(html_path) - 1);
                    html_path[sizeof(html_path) - 1] = '\0';
                }
            }
        } else {
            strcpy(html_path, decoded_path);
            if (!file_exists(html_path)) {
                // Fallback
                strncpy(html_path, base_html_path, sizeof(html_path) - 1);
                html_path[sizeof(html_path) - 1] = '\0';
            }
        }
    } else {
        // Aucun .html dans l’URL  on prend la base_html_path
        strncpy(html_path, base_html_path, sizeof(html_path) - 1);
        html_path[sizeof(html_path) - 1] = '\0';
    }

    html_file = CreateFileA(
        html_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (html_file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;

        if (GetFileSizeEx(html_file, &file_size) &&
            file_size.QuadPart > 0) {

            html_size = (size_t)file_size.QuadPart;

            html_mapping = CreateFileMappingA(
                html_file,
                NULL,
                PAGE_READONLY,
                0,
                0,
                NULL
            );

            if (html_mapping) {
                mapped_html = (const char *)MapViewOfFile(
                    html_mapping,
                    FILE_MAP_READ,
                    0,
                    0,
                    0
                );

                if (mapped_html) {
                    html = malloc(html_size + 1);
                    if (html) {
                        memcpy(html, mapped_html, html_size);
                        html[html_size] = '\0';
                    }

                    UnmapViewOfFile(mapped_html);
                    mapped_html = NULL;
                }

                CloseHandle(html_mapping);
                html_mapping = NULL;
            }
        }

        CloseHandle(html_file);
        html_file = INVALID_HANDLE_VALUE;
    }

    size_t len = 0;
    if (!html) {
        //erreur pas de page
        len = strlen(default_html);

        char header[512];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nServer: %s\r\nContent-Length: %d\r\n\r\n", server_name, len);

        LeaveCriticalSection(&data_lock);

        send(client_socket, header, strlen(header), 0);
        send(client_socket, default_html, len, 0);

        closesocket(client_socket);
        return 0;
    }

    //construit le fichier html interpréter pour le navigateur
    char* final_html_1 = process_server_ip_tags(html);
    free(html);
    if (!final_html_1) {
        //erreur pas de page
        len = strlen(default_error_html);

        char header[512];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nServer: %s\r\nContent-Length: %d\r\n\r\n", server_name, len);

        LeaveCriticalSection(&data_lock);

        send(client_socket, header, strlen(header), 0);
        send(client_socket, default_error_html, len, 0);

        closesocket(client_socket);
        return 0;
    }
    char* final_html = process_frc_tags(final_html_1);
    free(final_html_1);
    if (!final_html) {
        //erreur pas de page
        len = strlen(default_error_html);

        char header[512];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nServer: %s\r\nContent-Length: %d\r\n\r\n", server_name, len);

        LeaveCriticalSection(&data_lock);

        send(client_socket, header, strlen(header), 0);
        send(client_socket, default_error_html, len, 0);

        closesocket(client_socket);
        return 0;
    }

    len = strlen(final_html);

    char header[512];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nServer: %s\r\nContent-Length: %d\r\n\r\n", server_name, len);

    LeaveCriticalSection(&data_lock);

    send(client_socket, header, strlen(header), 0);
    send(client_socket, final_html, len, 0);

    free(final_html);

    closesocket(client_socket);
    return 0;
}

DWORD WINAPI server_loop(LPVOID lpParam) {
    WSADATA wsa;
    struct sockaddr_in server, client;
    int client_len = sizeof(client);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) return 1;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) return 1;
    listen(server_socket, 5);
    server_running = 1;

    while (server_running && frc_interrupted_dll == 0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        struct timeval timeout = {0, 150000};  // 150ms
        if (select(0, &readfds, NULL, NULL, &timeout) > 0) {
            SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client, &client_len);
            if (client_socket != INVALID_SOCKET) {
                CreateThread(NULL, 0, handle_client, (LPVOID)client_socket, 0, NULL);
            }
        }
        if (frc_interrupted_dll) {
            Sleep(20);  // Donne le temps à l'appelant de fermer proprement
            break;
        }
    }

    closesocket(server_socket);
    WSACleanup();
    server_running = 0;
    return 0;
}


__declspec(dllexport) const char* start_http_server(const char* arg) {
    if (server_thread) {
        close_http_server(NULL);
    }
    parse_args(arg);
    InitializeCriticalSection(&data_lock);

    if (*server_name == '\0') {
        char exe_path[MAX_PATH];
        DWORD len_exe_path = GetModuleFileName(NULL, exe_path, MAX_PATH);
        if (len_exe_path==0) {
            printf("Erreur : impossible de récupérer le nom du processus appelant pour le nom du serveur !\n");
            fflush(stdout);
            memcpy(server_name, "server", 7);
        } else {
            char *exe_name = strrchr(exe_path, '\\');
            if (!exe_name) {
                exe_name = exe_path;
            } else {
                exe_name++;
            }
            strncpy(server_name, exe_name, sizeof(server_name)-1);
            server_name[sizeof(server_name)-1] = '\0';
        }
    }

    memset(post_data, 0, sizeof(post_data));
    memset(get_data, 0, sizeof(get_data));
    server_thread = CreateThread(NULL, 0, server_loop, NULL, 0, NULL);
    return "1";
}

__declspec(dllexport) const char* get_post(const char* unused) {
    EnterCriticalSection(&data_lock);
    static char result[BUFFER_SIZE];
    strcpy(result, post_data);
    post_data[0] = '\0';
    LeaveCriticalSection(&data_lock);
    return result;
}

__declspec(dllexport) const char* get_get(const char* unused) {
    EnterCriticalSection(&data_lock);
    static char result[BUFFER_SIZE];
    strcpy(result, get_data);
    get_data[0] = '\0';
    LeaveCriticalSection(&data_lock);
    return result;
}

__declspec(dllexport) const char* close_http_server(const char* unused) {
    server_running = 0;
    if (server_thread) {
        WaitForSingleObject(server_thread, 1000);
        CloseHandle(server_thread);
        server_thread = NULL;
    }
    DeleteCriticalSection(&data_lock);
    return "1";
}

__declspec(dllexport) const char* configure_frc_tags_true(const char* unused) {
    EnterCriticalSection(&data_lock);
    replace_frc_tags=1;
    LeaveCriticalSection(&data_lock);
    return "1";
}

__declspec(dllexport) const char* configure_frc_tags_false(const char* unused) {
    EnterCriticalSection(&data_lock);
    replace_frc_tags=0;
    LeaveCriticalSection(&data_lock);
    return "1";
}


__declspec(dllexport) const char* start_web_page(const char* arg) {
    // Vérifie si le chemin finit par ".html", sinon l'ajoute
    char fixed_path[MAX_PATH];
    strncpy(fixed_path, arg, sizeof(fixed_path) - 6); // 5 + 1 de marge
    fixed_path[sizeof(fixed_path) - 6] = '\0';

    size_t len_arg = strlen(fixed_path);
    if (len_arg < 5 || strcmp(fixed_path + len_arg - 5, ".html") != 0) {
        strcat(fixed_path, ".html");
    }

    // Lire le fichier HTML
    FILE* f = fopen(fixed_path, "rb");
    if (!f) {
        printf("Erreur d'ouverture du fichier : %s", fixed_path);
        fflush(stdout);
        return "0";
    }

    // HTML source dynamique.
    char *html = NULL;
    size_t html_size = 0;

    const char *mapped_html = NULL;
    HANDLE html_file = INVALID_HANDLE_VALUE;
    HANDLE html_mapping = NULL;

    html_file = CreateFileA(
        fixed_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (html_file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;

        if (GetFileSizeEx(html_file, &file_size) &&
            file_size.QuadPart > 0) {

            html_size = (size_t)file_size.QuadPart;

            html_mapping = CreateFileMappingA(
                html_file,
                NULL,
                PAGE_READONLY,
                0,
                0,
                NULL
            );

            if (html_mapping) {
                mapped_html = (const char *)MapViewOfFile(
                    html_mapping,
                    FILE_MAP_READ,
                    0,
                    0,
                    0
                );

                if (mapped_html) {
                    html = malloc(html_size + 1);
                    if (html) {
                        memcpy(html, mapped_html, html_size);
                        html[html_size] = '\0';
                    }

                    UnmapViewOfFile(mapped_html);
                    mapped_html = NULL;
                }

                CloseHandle(html_mapping);
                html_mapping = NULL;
            }
        }

        CloseHandle(html_file);
        html_file = INVALID_HANDLE_VALUE;
    }

    if (!html) {
        printf("Erreur d'ouverture du fichier : %s", fixed_path);
        fflush(stdout);
        return "0";
    }

    char* final_html_1 = process_server_ip_tags(html);
    free(html);
    if (!final_html_1) {
        //erreur pas de page
        printf("Erreur du serveur\n");
        fflush(stdout);
        return 0;
    }
    char* final_html = process_frc_tags(final_html_1);
    free(final_html_1);
    if (!final_html) {
        //erreur pas de page
        printf("Erreur du serveur\n");
        fflush(stdout);
        return 0;
    }

    // Init WinSock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        free(final_html);
        return "0";//"Erreur WSAStartup";
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        free(final_html);
        return "0";//"Erreur socket";
    }

    // Choisir un port aléatoire non utilisé (entre 10000–60000)
    //srand((unsigned)time(NULL));
    //int temp_port = 10000 + rand() % 50000;
    int temp_port = port+2;

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),  // 127.0.0.1 uniquement
        .sin_port = htons(temp_port)
    };

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        free(final_html);
        return "0";//"Erreur bind";
    }

    listen(sock, 1);

    // Lancer Edge
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cmd /c start msedge http://127.0.0.1:%d", temp_port);
    system(cmd);

    // Accepter une connexion
    struct sockaddr_in client;
    int client_len = sizeof(client);
    SOCKET client_sock = accept(sock, (struct sockaddr*)&client, &client_len);
    if (client_sock == INVALID_SOCKET) {
        closesocket(sock);
        WSACleanup();
        free(final_html);
        return "0";//"Erreur accept";
    }

    // Envoyer la page
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n", strlen(final_html));

    send(client_sock, header, strlen(header), 0);
    send(client_sock, final_html, strlen(final_html), 0);

    free(final_html);

    Sleep(300);

    // Fermer
    closesocket(client_sock);
    closesocket(sock);
    WSACleanup();

    return "1";
}


__declspec(dllexport) const char* configure_base_directory(const char* directory) {
    if (!directory || strcmp(directory, "") == 0 || strcmp(directory, "0") == 0) {
        EnterCriticalSection(&data_lock);
        base_directory[0] = '\0';  // Réinitialiser
        LeaveCriticalSection(&data_lock);
        return "1";
    }

    DWORD attrs = GetFileAttributesA(directory);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        EnterCriticalSection(&data_lock);
        strncpy(base_directory, directory, BUFFER_SIZE - 1);
        base_directory[BUFFER_SIZE - 1] = '\0';  // Sécurité
        LeaveCriticalSection(&data_lock);
        return "1";
    }

    return "0";
}


__declspec(dllexport) const char* configure_favicon_directory(const char* directory) {
    // Si NULL, vide ou "0" on désactive le favicon
    if (!directory || directory[0] == '\0' || strcmp(directory, "") == 0 || strcmp(directory, "0") == 0) {
        EnterCriticalSection(&data_lock);
        favicon_directory[0] = '\0';
        LeaveCriticalSection(&data_lock);
        return "1";
    }

    // Vérifie que ça se termine par ".ico"
    const char* ext = strrchr(directory, '.');
    if (!ext || _stricmp(ext, ".ico") != 0) {
        return "0"; // Mauvaise extension
    }

    DWORD attrib = GetFileAttributesA(directory);

    // Vérifie que le fichier existe
    if (attrib == INVALID_FILE_ATTRIBUTES || (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return "0"; // Fichier introuvable
    }

    // Copie le chemin validé
    EnterCriticalSection(&data_lock);
    strncpy(favicon_directory, directory, BUFFER_SIZE - 1);
    favicon_directory[BUFFER_SIZE - 1] = '\0';
    LeaveCriticalSection(&data_lock);
    return "1";
}

__declspec(dllexport) const char* configure_server_name(char* name) {
    if (*name == '\0') return "0";

    EnterCriticalSection(&data_lock);
    char *i = name;
    char *j = server_name;
    while (*i && (size_t)(j-server_name) < (sizeof(server_name)-1)) {
        if (*i != '"' && *i != '\'' && *i != ':' && *i != ';' && *i >= 32) {
            *j = *i;
            j++;
        }
        i++;
    }
    *j = '\0';
    //strncpy(server_name, name, sizeof(server_name) - 1);
    server_name[sizeof(server_name) - 1] = '\0';

    if (*server_name == '\0') {
        char exe_path[MAX_PATH];
        DWORD len_exe_path = GetModuleFileName(NULL, exe_path, MAX_PATH);
        if (len_exe_path==0) {
            memcpy(server_name, "server", 7);
        } else {
            char *exe_name = strrchr(exe_path, '\\');
            if (!exe_name) {
                exe_name = exe_path;
            } else {
                exe_name++;
            }
            strncpy(server_name, exe_name, sizeof(server_name)-1);
            server_name[sizeof(server_name)-1] = '\0';
        }
    }
    LeaveCriticalSection(&data_lock);
    return "1";
}
