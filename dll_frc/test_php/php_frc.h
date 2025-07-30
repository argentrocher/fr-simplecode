// php_frc.h
#ifndef PHP_FRC_H
#define PHP_FRC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <ctype.h>

#ifndef TMP_FILENAME
#define TMP_FILENAME "tmp_php_code.php"
#endif // TMP_FILENAME

#define MAX_ENV_LEN 2048
#define MAX_CMD_LEN 1024
#define MAX_LINE 1024

char info_web_data_h[2048];
char info_time_data_h[64];
char get_data_h[2048];
char post_data_h[2048];
char info_php[128];
char info_software[128]= "PHP_FRC_Server/0.3";
char info_client_ip[128];

void url_decode_h(char *dst, const char *src) {
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void extract_query_params_h(const char* src, char* dst) {
    strncpy(dst, src, 2047);
    dst[2047] = '\0';
}

void replace_block_collision_h(char* str) {
    while ((str = strchr(str, '+'))) {
        *str = ' ';
    }
}

// === Fonction principale pour exécuter du PHP (php-cgi.exe)===
char* php_process_cgi(const char* php_code, const char* buffer) {
    // Écrit le fichier PHP temporaire
    FILE* tmp = fopen(TMP_FILENAME, "w");
    if (!tmp) return NULL;
    fputs(php_code, tmp);
    fclose(tmp);

    // === Extraction de la requête ===
    char method[8] = "", path[1024] = "", decoded_path[1024] = "";
    sscanf(buffer, "%7s %1023s", method, path);
    url_decode_h(decoded_path, path);

    // Headers
    char* ua = strstr(buffer, "User-Agent:");
    char* host = strstr(buffer, "Host:");
    char* accept = strstr(buffer, "Accept:");

    info_web_data_h[0] = '\0';
    info_time_data_h[0] = '\0';
    get_data_h[0] = '\0';
    post_data_h[0] = '\0';

    if (host) {
        char line[MAX_LINE] = {0};
        sscanf(host, "Host: %[^\r\n]", line);
        strcat(info_web_data_h, "Host: ");
        strcat(info_web_data_h, line);
        strcat(info_web_data_h, "\n");
    }
    if (ua) {
        char line[MAX_LINE] = {0};
        sscanf(ua, "User-Agent: %[^\r\n]", line);
        strcat(info_web_data_h, "User-Agent: ");
        strcat(info_web_data_h, line);
        strcat(info_web_data_h, "\n");
    }
    if (accept) {
        char line[MAX_LINE] = {0};
        sscanf(accept, "Accept: %[^\r\n]", line);
        strcat(info_web_data_h, "Accept: ");
        strcat(info_web_data_h, line);
    }

    // Heure
    SYSTEMTIME t;
    GetLocalTime(&t);
    sprintf(info_time_data_h, "%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

    // Requête POST
    char* query = strchr(decoded_path, '?');
    if (query) {
        extract_query_params_h(query + 1, get_data_h);
        //replace_block_collision_h(get_data_h); pas utile les plus c'est que dans post
    }

    char* body = strstr(buffer, "\r\n\r\n");
    if (body) body += 4;
    if (strcmp(method, "POST") == 0 && body) {
        extract_query_params_h(body, post_data_h);
        replace_block_collision_h(post_data_h);
    }

    // === Environnement CGI ===
    char env_script[MAX_ENV_LEN];
    char env_method[64];
    char env_query[MAX_ENV_LEN] = "";
    char env_length[64] = "";

    snprintf(env_script, MAX_ENV_LEN, "SCRIPT_FILENAME=%s", TMP_FILENAME);
    snprintf(env_method, 64, "REQUEST_METHOD=%s", method);
    if (query && strlen(query + 1))
        snprintf(env_query, MAX_ENV_LEN, "QUERY_STRING=%s", query + 1);
    if (strcmp(method, "POST") == 0)
        snprintf(env_length, 64, "CONTENT_LENGTH=%zu", strlen(post_data_h));

    putenv("QUERY_STRING=");         // vide les restes de GET
    putenv("CONTENT_LENGTH=");      // vide les restes de POST
    putenv("CONTENT_TYPE=");        // vide les restes éventuels
    putenv("HTTP_HOST=");
    putenv("HTTP_USER_AGENT=");

    putenv("GATEWAY_INTERFACE=CGI/1.1");
    putenv("REDIRECT_STATUS=200");
    putenv(env_script);
    putenv(env_method);
    if (*env_query) putenv(env_query);
    if (*env_length) putenv(env_length);
    if (strcmp(method, "POST") == 0)
        putenv("CONTENT_TYPE=application/x-www-form-urlencoded");

    if (host) {
        char line[MAX_LINE] = {0};
        sscanf(host, "Host: %[^\r\n]", line);
        static char env_host[MAX_LINE + 20];
        snprintf(env_host, sizeof(env_host), "HTTP_HOST=%s", line);
        putenv(env_host);
    }

    if (ua) {
        char line[MAX_LINE] = {0};
        sscanf(ua, "User-Agent: %[^\r\n]", line);
        static char env_ua[MAX_LINE + 30];
        snprintf(env_ua, sizeof(env_ua), "HTTP_USER_AGENT=%s", line);
        putenv(env_ua);
    }

    // Nom du serveur HTTP
    if (info_software[0]){
        char env_software[128];
        snprintf(env_software, sizeof(env_software), "SERVER_SOFTWARE=%s", info_software);
        putenv(env_software);
    }

    //IP du client
    if (info_client_ip[0]){
        char env_remote[128];
        snprintf(env_remote, sizeof(env_remote), "REMOTE_ADDR=%s", info_client_ip);
        putenv(env_remote);
    }

    // === Exécution de PHP ===
    FILE* pipe = NULL;
    if (strcmp(method, "POST") == 0 && post_data_h[0]) {
        FILE* f = fopen("php_input.txt", "wb");
        fwrite(post_data_h, 1, strlen(post_data_h), f);
        fclose(f);
        pipe = _popen("php-cgi < php_input.txt", "r");
    } else {
        pipe = _popen("php-cgi", "r");
    }
    if (!pipe) return NULL;


    // === Lecture de la sortie ===
    size_t size = 0, cap = 8192;
    char* output = (char*)malloc(cap);
    if (!output) return NULL;

    char buf[1024];
    output[0] = '\0';
    info_php[0] = '\0';

    while (fgets(buf, sizeof(buf), pipe)) {
        // Stocke X-Powered-By dans info_php si trouvé
        if (strncmp(buf, "X-Powered-By:", 13) == 0) {
            strncpy(info_php, buf + 13, sizeof(info_php) - 1);
            info_php[sizeof(info_php) - 1] = '\0';
            // Nettoyage : retire les \r\n éventuels
            char* nl = strpbrk(info_php, "\r\n");
            if (nl) *nl = '\0';
        }

        // Construit la sortie (tout) dans output
        size_t len = strlen(buf);
        if (size + len >= cap) {
            cap *= 2;
            output = (char*)realloc(output, cap);
            if (!output) return NULL;
        }
        memcpy(output + size, buf, len);
        size += len;
    }
    output[size] = '\0';

    _pclose(pipe);
    remove(TMP_FILENAME);
    if (strcmp(method, "POST") == 0)
        remove("php_input.txt");

    // Cherche la séparation entre headers CGI et corps de la réponse
    char* body_start = strstr(output, "\r\n\r\n");
    if (!body_start) body_start = strstr(output, "\n\n");  // fallback au cas où

    char* final_result;
    if (body_start) {
        body_start += (body_start[1] == '\n') ? 2 : 4;  // saute le \r\n\r\n ou \n\n
        final_result = strdup(body_start);
    } else {
        final_result = strdup(output);  // pas de séparation trouvée, on prend tout
    }
    free(output);
    return final_result;
}


// Fonction principale : exécute le contenu PHP avec php-cgi.exe (get auto, pas de post)
// `php_code` = contenu de la page (avec code PHP)
// `request_uri` = URI de la requête (ex: "/index.php")
// Retourne dynamiquement la réponse HTML (le free() doit être fait par l'appelant)
char* php_process_page(const char* php_code, const char* request_uri) {
    // Écrit le code PHP dans un fichier temporaire
    FILE* tmp = fopen(TMP_FILENAME, "w");
    if (!tmp) return NULL;
    fputs(php_code, tmp);
    fclose(tmp);

    // Variables CGI de base
    char env_script_filename[MAX_ENV_LEN];
    snprintf(env_script_filename, MAX_ENV_LEN, "SCRIPT_FILENAME=%s", TMP_FILENAME);
    putenv("GATEWAY_INTERFACE=CGI/1.1");
    putenv("REQUEST_METHOD=GET");
    putenv("REDIRECT_STATUS=200");
    putenv(env_script_filename);

    // Facultatif : ajouter URI
    if (request_uri && strlen(request_uri) < MAX_ENV_LEN - 20) {
        static char env_uri[MAX_ENV_LEN];
        snprintf(env_uri, MAX_ENV_LEN, "REQUEST_URI=%s", request_uri);
        putenv(env_uri);
    }

    // Appelle php-cgi.exe
    FILE* pipe = _popen("php-cgi", "r");
    if (!pipe) return NULL;

    // Lis toute la sortie
    size_t size = 0, cap = 8192;
    char* output = (char*)malloc(cap);
    if (!output) return NULL;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t len = strlen(buffer);
        if (size + len >= cap) {
            cap *= 2;
            output = (char*)realloc(output, cap);
            if (!output) return NULL;
        }
        memcpy(output + size, buffer, len);
        size += len;
    }
    output[size] = '\0';

    _pclose(pipe);
    remove(TMP_FILENAME);  // Nettoyage

    return output;
}

char* get_info_php(){
    if (info_php){
        return info_php;
    } else {
        return "NULL";
    }
}

char* rename_server_php(char* name){
    snprintf(info_software, sizeof(info_software),"%s", name);
    return "1";
}

char* set_client_ip(char* ip){
    snprintf(info_client_ip, sizeof(info_client_ip),"%s", ip);
    return "1";
}

#endif // PHP_FRC_H
