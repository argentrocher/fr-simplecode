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

//gcc -shared -o http_serveur_dll.dll http_serveur_dll.c -lws2_32 -lpsapi -Wl,--add-stdcall-alias

//[[[{print("text(dll|C:\Users\TGAPL\Documents\Thomas 2\C,C++\http_serveur_dll|[start_http_server("8080;C:\Users\TGAPL\Documents\Thomas 2\C,C++\web_http_serveur_test.html")])")}]]]

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
static char base_html_path[512] = {0};

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
    static char output[BUFFER_SIZE * 2] = {0};
    const char* tag = "<${SERVER-IP!}$>";
    size_t tag_len = strlen(tag);

    char server_ip[64];
    get_local_ip(server_ip, sizeof(server_ip));

    const char* p = html_raw;
    char* out = output;
    output[0] = '\0'; // Reset le buffer

    while (*p && (out - output) < sizeof(output) - 1) {
        const char* found = strstr(p, tag);
        if (!found) {
            strncat(out, p, sizeof(output) - strlen(output) - 1);
            break;
        }

        strncat(out, p, found - p);  // Copier jusqu’au tag
        strcat(out, server_ip);      // Remplacer par l'IP locale

        p = found + tag_len;
    }

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
void web_page_frc_methode(char* input) {
    char buffer[BUFFER_SIZE * 2] = {0};
    char* out = buffer;
    const char* p = input;

    while (*p) {
        const char* start = strstr(p, "<${");
        if (!start) {
            strcat(out, p);
            break;
        }

        // Copie le texte avant
        strncat(out, p, start - p);
        const char* end = strstr(start, "}$>");
        if (!end) {
            strcat(out, p); // pas bien formé, copie tout
            break;
        }

        char code[128] = {0};
        strncpy(code, start + 3, end - (start + 3));

        if (strcmp(code, "GET!") == 0) {
            strcat(out, get_data && *get_data ? get_data : "0");
        } else if (strcmp(code, "POST!") == 0) {
            strcat(out, post_data && *post_data ? post_data : "0");
        } else if (strncmp(code, "GET:", 4) == 0) {
            const char* key = code + 4;
            char val[256] = {0};
            extract_value_from_data(get_data, key, val, sizeof(val));
            strcat(out, val);
        } else if (strncmp(code, "POST:", 5) == 0) {
            const char* key = code + 5;
            char val[256] = {0};
            extract_value_from_data(post_data, key, val, sizeof(val));
            strcat(out, val);
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
                strncat(temp, src, equal - src);
                strcat(temp, ";");

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
            if (len > 2 && temp[len - 1] == ';') temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            strcat(out, temp[2] ? temp : "0");
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
                    strncat(temp, equal + 1, end - (equal + 1));
                    strcat(temp, ";");
                    src = end + 1;
                } else if (closing) {
                    strncat(temp, equal + 1, closing - (equal + 1));
                    break;
                } else {
                    break;
                }
            }

            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';') temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            strcat(out, temp[2] ? temp : "0");
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
                strncat(temp, src, equal - src);
                strcat(temp, ";");

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
            if (len > 2 && temp[len - 1] == ';') temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            strcat(out, temp[2] ? temp : "0");
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
                    strncat(temp, equal + 1, end - (equal + 1));
                    strcat(temp, ";");
                    src = end + 1;
                } else if (closing) {
                    strncat(temp, equal + 1, closing - (equal + 1));
                    break;
                } else {
                    break;
                }
            }

            size_t len = strlen(temp);
            if (len > 2 && temp[len - 1] == ';') temp[len - 1] = '\0'; // remove last ;
            strcat(temp, "]]");
            strcat(out, temp[2] ? temp : "0");
        } else if (strcmp(code, "GET#FREE") == 0) {
            if (get_data) {
                strcat(out, "1");
                get_data[0] = '\0';
            } else {
                strcat(out, "0");
            }
        } else if (strcmp(code, "POST#FREE") == 0) {
            if (post_data) {
                strcat(out, "1");
                post_data[0] = '\0';
            } else {
                strcat(out, "0");
            }
        } else if (strncmp(code, "REDIRECT_PAGE:", 14) == 0) {
            const char* page = code + 14;
            char fullpath[512] = {0};
            strcpy(fullpath, page);

            // Ajouter ".html" si manquant
            if (!(strlen(fullpath) >= 5 && strcmp(fullpath + strlen(fullpath) - 5, ".html") == 0)) {
                strcat(fullpath, ".html");
            }

            // Vérifie si le fichier existe
            FILE* f = fopen(fullpath, "rb");
            if (f) {
                fclose(f);
                strcpy(base_html_path, fullpath);  // redirige
                strcat(out, "1");
            } else {
                strcat(out, "0");
            }
        }else if (strcmp(code, "INFO:TIME") == 0) {
            strcat(out, info_time_data);
        } else if (strcmp(code, "INFO:TIME.MS") == 0) {
            // Récupère les millisecondes (extrait après le dernier '.')
            const char* dot = strrchr(info_time_data, '.');
            strcat(out, dot ? dot + 1 : "0");
        } else if (strcmp(code, "INFO:WEB") == 0) {
            strcat(out, info_web_data);
        } else if (strcmp(code, "INFO:WEB.HOST") == 0) {
            const char* start = strstr(info_web_data, "Host: ");
            const char* end = strstr(info_web_data, "User-Agent:");
            if (start && end && end > start) {
                char temp[256] = {0};
                strncpy(temp, start + 6, end - (start + 6));
                temp[end - (start + 6)] = '\0';
                strcat(out, temp);
            } else {
                strcat(out, "0");
            }
        } else if (strcmp(code, "INFO:WEB.USER-AGENT") == 0) {
            const char* start = strstr(info_web_data, "User-Agent: ");
            const char* end = strstr(info_web_data, "Accept:");
            if (start && end && end > start) {
                char temp[512] = {0};
                strncpy(temp, start + 12, end - (start + 12));
                temp[end - (start + 12)] = '\0';
                strcat(out, temp);
            } else {
                strcat(out, "0");
            }
        } else if (strcmp(code, "INFO:WEB.ACCEPT") == 0) {
            const char* start = strstr(info_web_data, "Accept: ");
            if (start) {
                strcat(out, start + 8);
            } else {
                strcat(out, "0");
            }
        } else if (strcmp(code, "INFO:PORT") == 0) {
            char port_str[16];
            sprintf(port_str, "%d", port);
            strcat(out, port_str);
        } else if (strcmp(code, "INFO:FRC_TAGS") == 0) {
            char frctags_str[16];
            sprintf(frctags_str, "%d", replace_frc_tags);
            strcat(out, frctags_str);
        } else if (strcmp(code, "INFO:IP") == 0) {
            strcat(out, info_ip_data[0] ? info_ip_data : "0");
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
            strcat(out, full_lang);
        } else if (strcmp(code, "INFO:PROCESS") == 0) {
            strcat(out, get_app_performance_info());
        } else {
            strcat(out, "0"); // pas reconnu
        }

        p = end + 3; // saute }$>
    }

    strcpy(input, buffer);
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
    static char final_html[BUFFER_SIZE * 2];
    const char* start = html_raw;
    char* out = final_html;
    *out = 0;

    while ((start = strstr(start, "<frc>"))) {
        // Copie avant la balise
        strncat(out, html_raw, start - html_raw);

        // Cherche fin balise
        const char* end = strstr(start, "</frc>");
        if (!end) break;

        // Extraction contenu
        char content[BUFFER_SIZE] = {0};
        strncpy(content, start + 5, end - (start + 5));

        // Nettoyage : supprime les \r \n, espaces de début/fin ligne
        char cleaned[BUFFER_SIZE] = {0};
        const char* p = content;
        char* q = cleaned;

        while (*p) {
            // Skip lignes vides
            while (isspace(*p)) p++;
            while (*p && *p != '\n' && *p != '\r') {
                *q++ = *p++;
            }
            if (*(q - 1) != ' ') *q++ = ' ';  // espace entre blocs
            while (*p == '\r' || *p == '\n') p++;
        }
        *q = '\0';

        //supprimé les balises d'info de page web post et get ... ex: <${GET!}$>
        web_page_frc_methode(cleaned);
        // Écrit dans un fichier temporaire
        FILE* f = fopen("http_serveur_frc.frc", "w");
        if (f) {
            fprintf(f, "%s", cleaned);
            fclose(f);
        }

        // Récupère chemin exe appelant
        char exe_path[MAX_PATH];
        GetModuleFileName(NULL, exe_path, MAX_PATH);

        // Commande à exécuter
        char command[MAX_PATH + 256];
        sprintf(command, "\"%s\" --start:\"http_serveur_frc.frc\"", exe_path);

        // Ouvre le processus et récupère stdout
        char result[BUFFER_SIZE*2] = {0};
        if (!run_command_and_capture_output(command, result, sizeof(result))) {
            // result contient toute la sortie de ton programme
            //printf("Résultat :\n%s\n", result);
            printf("Erreur lors de l'exécution\n");
        }

        // Remplace la balise par la sortie
        if (replace_frc_tags) {
            strcat(out, "<div class=\"frc\">");
        }
        strcat(out, result);
        if (replace_frc_tags) {
            strcat(out, "</div>");
        }

        // Avance le pointeur après </frc>
        html_raw = end + 6;
        start = html_raw;
    }

    // Ajoute le reste
    strcat(out, html_raw);
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
                    LeaveCriticalSection(&data_lock);
                    // 413 Payload Too Large (optionnel)
                    const char* too_large = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n\r\n";
                    send(client_socket, too_large, strlen(too_large), 0);
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

                LeaveCriticalSection(&data_lock);

                char header[256];
                snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/x-icon\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n\r\n", ico_len);

                send(client_socket, header, strlen(header), 0);
                send(client_socket, ico_data, ico_len, 0);
                free(ico_data);
                closesocket(client_socket);
                return 0;
            }
        }

        LeaveCriticalSection(&data_lock);

        // Fichier introuvable -> 404
        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, not_found, strlen(not_found), 0);
        closesocket(client_socket);
        return 0;
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
            LeaveCriticalSection(&data_lock);
            // Pas trouvé
            const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client_socket, not_found, strlen(not_found), 0);
            closesocket(client_socket);
            return 0;
        }

        if (!file_exists(final_path)) {
            LeaveCriticalSection(&data_lock);
            // Pas trouvé
            const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client_socket, not_found, strlen(not_found), 0);
            closesocket(client_socket);
            return 0;
        }

        // Lecture binaire
        FILE* f = fopen(final_path, "rb");
        if (!f) {
            LeaveCriticalSection(&data_lock);
            const char* error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            send(client_socket, error, strlen(error), 0);
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

        LeaveCriticalSection(&data_lock);

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
            "Content-Length: %d\r\n\r\n",
            filename, len);
        } else {
            snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
            content_type, len); //"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
        }

        send(client_socket, header, strlen(header), 0);
        send(client_socket, buffer, len, 0);
        free(buffer);
        closesocket(client_socket);
        return 0;
    }


    char html_path[BUFFER_SIZE] = {0};
    char html[BUFFER_SIZE] = "<html><body>Erreur de chargement</body></html>";
    int len = strlen(html);

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
                FILE* f = fopen(decoded_path, "rb");
                if (f) {
                    len = fread(html, 1, sizeof(html) - 1, f);
                    html[len] = '\0';
                    fclose(f);
                }
            } else {
                snprintf(html_path, sizeof(html_path), "%s\\%s", base_directory, filename);
                //printf("html : %s\n",html_path);
                if (file_exists(html_path)) {
                    FILE* f = fopen(html_path, "rb");
                    if (f) {
                        len = fread(html, 1, sizeof(html) - 1, f);
                        html[len] = '\0';
                        fclose(f);
                    }
                } else {
                    // Fichier introuvable  fallback sur base_html_path
                    FILE* f = fopen(base_html_path, "rb");
                    if (f) {
                        len = fread(html, 1, sizeof(html) - 1, f);
                        html[len] = '\0';
                        fclose(f);
                    }
                }
            }
        } else {
            strcpy(html_path, decoded_path);
            if (file_exists(html_path)) {
                FILE* f = fopen(html_path, "rb");
                if (f) {
                    len = fread(html, 1, sizeof(html) - 1, f);
                    html[len] = '\0';
                    fclose(f);
                }
            } else {
                // Fichier introuvable  fallback sur base_html_path
                FILE* f = fopen(base_html_path, "rb");
                if (f) {
                    len = fread(html, 1, sizeof(html) - 1, f);
                    html[len] = '\0';
                    fclose(f);
                }
            }
        }
    } else {
        // Aucun .html dans l’URL  on prend la base_html_path
        FILE* f = fopen(base_html_path, "rb");
        if (f) {
            len = fread(html, 1, sizeof(html) - 1, f);
            html[len] = '\0';
            fclose(f);
        }
    }

    //construit le fichier html interpréter pour le navigateur
    const char* final_html_1 = process_server_ip_tags(html);
    const char* final_html = process_frc_tags(final_html_1);
    len = strlen(final_html);

    LeaveCriticalSection(&data_lock);

    char header[512];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", len);
    send(client_socket, header, strlen(header), 0);
    send(client_socket, final_html, len, 0);

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

    char html[BUFFER_SIZE] = {0};
    size_t len = fread(html, 1, sizeof(html) - 1, f);
    fclose(f);

    html[len] = '\0';

    const char* final_html_1 = process_server_ip_tags(html);
    const char* final_html = process_frc_tags(final_html_1);

    // Init WinSock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return "0";//"Erreur WSAStartup";
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
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
