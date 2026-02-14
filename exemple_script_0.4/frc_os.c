// file_storage_dll.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_ORDER_SCRIPT_LEN 5000

// tailles pour allocations internes
#define INITIAL_CONTENT_CAP 1024

// tampons de retour rotatifs (évite malloc/free côté appelant)
#define RETBUF_COUNT 12
static char RETBUF[RETBUF_COUNT][MAX_ORDER_SCRIPT_LEN + 512];
static int RETBUF_IDX = 0;

//gcc -shared -o frc_os.dll frc_os.c -lws2_32

// structure stockée pour chaque "fichier ouvert"
typedef struct FileEntry {
    char *name;
    char *path;
    char mode; // 'r', 'w', 'a'
    size_t pos;
    char *content;      // buffer '\0' terminated
    size_t content_len;
    size_t content_cap;
    struct FileEntry *next;
} FileEntry;

static FileEntry *g_head = NULL;
static CRITICAL_SECTION g_cs;

// ---------- helpers ----------
static void enter_cs(){ EnterCriticalSection(&g_cs); }
static void leave_cs(){ LeaveCriticalSection(&g_cs); }

static char *str_dup(const char *s){
    if (!s) return NULL;
    size_t l = strlen(s) + 1;
    char *r = (char*)malloc(l);
    if (r) memcpy(r, s, l);
    return r;
}

static const char* retstr_printf(const char *fmt, ...){
    EnterCriticalSection(&g_cs);
    int idx = RETBUF_IDX;
    RETBUF_IDX = (RETBUF_IDX + 1) % RETBUF_COUNT;
    leave_cs();

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(RETBUF[idx], sizeof(RETBUF[idx]) - 1, fmt, ap);
    va_end(ap);
    RETBUF[idx][sizeof(RETBUF[idx]) - 1] = '\0';
    return RETBUF[idx];
}

// lit tout le fichier binaire en mémoire (out_buf mallocé)
static int read_file_to_memory(const char *path, char **out_buf, size_t *out_len){
    *out_buf = NULL; *out_len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -2; }
    long sz = ftell(f);
    if (sz < 0) sz = 0;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -3; }
    *out_buf = (char*)malloc((size_t)sz + 1);
    if (!*out_buf) { fclose(f); return -4; }
    size_t n = fread(*out_buf, 1, (size_t)sz, f);
    (*out_buf)[n] = '\0';
    *out_len = n;
    fclose(f);
    return 0;
}

static FileEntry* find_entry_locked(const char *name){
    FileEntry *cur = g_head;
    while (cur){
        if (strcmp(cur->name, name) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

static void free_entry_locked(FileEntry *e){
    if (!e) return;
    if (e->name) free(e->name);
    if (e->path) free(e->path);
    if (e->content) free(e->content);
    free(e);
}

// enlarge content buffer to at least newcap
static int ensure_content_cap(FileEntry *e, size_t newcap){
    if (newcap <= e->content_cap) return 0;
    size_t target = e->content_cap ? e->content_cap : INITIAL_CONTENT_CAP;
    while (target < newcap) target *= 2;
    char *p = (char*)realloc(e->content, target);
    if (!p) return -1;
    e->content = p;
    e->content_cap = target;
    return 0;
}

// ---------- DLL init/cleanup ----------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){
    (void)hinstDLL; (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH){
        InitializeCriticalSection(&g_cs);
    } else if (fdwReason == DLL_PROCESS_DETACH){
        // libère toutes les structures si restes
        enter_cs();
        FileEntry *cur = g_head;
        while (cur){
            FileEntry *n = cur->next;
            free_entry_locked(cur);
            cur = n;
        }
        g_head = NULL;
        leave_cs();
        DeleteCriticalSection(&g_cs);
    }
    return TRUE;
}

// ---------- API exportée ----------
// format expected:
// file_open:  "name;le_chemin_du_fichier;mode"  (mode = r,w,a)
// file_close: "name"
// file_read:  "name;N"   (N decimal, 0 => read up to MAX_ORDER_SCRIPT_LEN)
// file_write: "name;text à écrire..." (text can contain ';')
// file_tell:  "name"

__declspec(dllexport) const char* file_open(char* arg){
    if (!arg) return retstr_printf("ERR: arg null");

    // split into three parts
    // name ; path ; mode
    char *p = strchr(arg, ';');
    if (!p) return retstr_printf("ERR: format: name;path;mode");
    size_t name_len = (size_t)(p - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len); namebuf[name_len] = '\0';

    char *p2 = strchr(p + 1, ';');
    if (!p2) return retstr_printf("ERR: format: name;path;mode");
    size_t path_len = (size_t)(p2 - (p + 1));
    if (path_len == 0) return retstr_printf("ERR: empty path");
    char *pathbuf = (char*)malloc(path_len + 1);
    if (!pathbuf) return retstr_printf("ERR: mem");
    memcpy(pathbuf, p + 1, path_len); pathbuf[path_len] = '\0';

    char modech = '\0';
    if (*(p2 + 1) != '\0') modech = *(p2 + 1);
    if (!(modech == 'r' || modech == 'w' || modech == 'a')) {
        free(pathbuf);
        return retstr_printf("ERR: mode must be r,w or a");
    }

    if (modech == 'r' || modech == 'a') {
        FILE *testf = fopen(pathbuf, "rb");
        if (!testf) {
            free(pathbuf);
            return retstr_printf("ERR: FILE_NOT_EXIST");
        }
        fclose(testf);
    }

    enter_cs();
    // check existing
    if (find_entry_locked(namebuf) != NULL){
        leave_cs();
        free(pathbuf);
        return retstr_printf("ERR: name already exists");
    }

    FileEntry *e = (FileEntry*)malloc(sizeof(FileEntry));
    if (!e){
        leave_cs(); free(pathbuf);
        return retstr_printf("ERR: mem");
    }
    memset(e,0,sizeof(FileEntry));
    e->name = str_dup(namebuf);
    e->path = pathbuf; // already allocated
    e->mode = modech;
    e->pos = 0;
    e->content = NULL;
    e->content_len = 0;
    e->content_cap = 0;
    e->next = g_head;
    g_head = e;

    // if mode == 'r', read file into memory immediately and close file
    if (modech == 'r'){
        char *buf = NULL; size_t len = 0;
        if (read_file_to_memory(e->path, &buf, &len) == 0){
            // assign
            e->content = buf;
            e->content_len = len;
            e->content_cap = len + 1;
        } else {
            // file missing -> empty content
            e->content = (char*)malloc(1);
            if (e->content) e->content[0] = '\0';
            e->content_len = 0;
            e->content_cap = 1;
        }
    } else {
        // modes 'w' or 'a' start with empty content in-memory (we'll append on close/write)
        e->content = (char*)malloc(1);
        if (e->content) e->content[0] = '\0';
        e->content_len = 0;
        e->content_cap = 1;
    }

    leave_cs();
    return retstr_printf("OK");
}

__declspec(dllexport) const char* file_close(char* arg){
    if (!arg) return retstr_printf("ERR: arg null");
    // arg may contain more than name; only parse up to first ';' or end
    char *semi = strchr(arg, ';');
    size_t name_len = semi ? (size_t)(semi - arg) : strlen(arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    char namebuf[512]; if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len); namebuf[name_len] = '\0';

    enter_cs();
    FileEntry *prev = NULL, *cur = g_head;
    while (cur){
        if (strcmp(cur->name, namebuf) == 0) break;
        prev = cur; cur = cur->next;
    }
    if (!cur){ leave_cs(); return retstr_printf("ERR: name not found"); }

    // if mode a or w -> write content out to file
    if (cur->mode == 'a' || cur->mode == 'w'){
        FILE *f = NULL;
        if (cur->mode == 'a') f = fopen(cur->path, "ab");
        else f = fopen(cur->path, "wb"); // w -> overwrite
        if (f){
            if (cur->content_len > 0){
                size_t wrote = fwrite(cur->content, 1, cur->content_len, f);
                (void)wrote;
            }
            fclose(f);
        } else {
            // try to create the file if not exists (fopen "ab" would create but could fail)
            // ignore error: still remove structure
        }
    }

    // unlink and free
    if (prev) prev->next = cur->next; else g_head = cur->next;
    free_entry_locked(cur);
    leave_cs();
    return retstr_printf("OK");
}

__declspec(dllexport) const char* file_read(char* arg){
    if (!arg) return retstr_printf("ERR: arg null");
    // format name;N
    char *semi = strchr(arg, ';');
    if (!semi) return retstr_printf("ERR: format: name;N");
    size_t name_len = (size_t)(semi - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len); namebuf[name_len] = '\0';

    char *numstr = semi + 1;
    long req = atol(numstr);
    if (req <= 0 || req > MAX_ORDER_SCRIPT_LEN) req = MAX_ORDER_SCRIPT_LEN;

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e){ leave_cs(); return retstr_printf(""); /* empty string */ }

    size_t remaining = (e->content_len > e->pos) ? (e->content_len - e->pos) : 0;
    size_t tocopy = (size_t)req;
    if (tocopy > remaining) tocopy = remaining;
    if (tocopy > MAX_ORDER_SCRIPT_LEN) tocopy = MAX_ORDER_SCRIPT_LEN;

    // get a return buffer
    int idx;
    //EnterCriticalSection(&g_cs);
    idx = RETBUF_IDX;
    RETBUF_IDX = (RETBUF_IDX + 1) % RETBUF_COUNT;
    //LeaveCriticalSection(&g_cs);

    if (tocopy > 0){
        size_t copycap = (tocopy < MAX_ORDER_SCRIPT_LEN) ? tocopy : MAX_ORDER_SCRIPT_LEN;
        if (copycap > sizeof(RETBUF[idx]) - 1) copycap = sizeof(RETBUF[idx]) - 1;
        memcpy(RETBUF[idx], e->content + e->pos, copycap);
        RETBUF[idx][copycap] = '\0';
        e->pos += copycap;
        leave_cs();
        // supprimer CR par LF
        for (char *p = RETBUF[idx]; *p; p++) {
            if (*p == '\r')
            *p = '\n';
        }
        return RETBUF[idx];
    } else {
        leave_cs();
        RETBUF[idx][0] = '\0';
        return RETBUF[idx];
    }
}

__declspec(dllexport) const char* file_read_ansi(char* arg) {
    if (!arg) return retstr_printf("ERR: arg null");

    // format name;N
    char *semi = strchr(arg, ';');
    if (!semi) return retstr_printf("ERR: format: name;N");
    size_t name_len = (size_t)(semi - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");

    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len);
    namebuf[name_len] = '\0';

    long req = atol(semi + 1);
    if (req <= 0 || req > MAX_ORDER_SCRIPT_LEN)
        req = MAX_ORDER_SCRIPT_LEN;

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e) {
        leave_cs();
        return retstr_printf("");
    }

    size_t remaining = (e->content_len > e->pos) ? (e->content_len - e->pos) : 0;
    size_t tocopy = (size_t)req;
    if (tocopy > remaining) tocopy = remaining;
    if (tocopy > MAX_ORDER_SCRIPT_LEN) tocopy = MAX_ORDER_SCRIPT_LEN;

    int idx = RETBUF_IDX;
    RETBUF_IDX = (RETBUF_IDX + 1) % RETBUF_COUNT;

    if (tocopy == 0) {
        leave_cs();
        RETBUF[idx][0] = '\0';
        return RETBUF[idx];
    }

    // Copie brute depuis le contenu
    size_t copycap = (tocopy < MAX_ORDER_SCRIPT_LEN) ? tocopy : MAX_ORDER_SCRIPT_LEN;
    if (copycap > sizeof(RETBUF[idx]) - 1) copycap = sizeof(RETBUF[idx]) - 1;

    char tempBuf[MAX_ORDER_SCRIPT_LEN + 2];
    memcpy(tempBuf, e->content + e->pos, copycap);
    tempBuf[copycap] = '\0';
    e->pos += copycap;
    leave_cs();

    // === Conversion UTF-8 -> ANSI ===
    int wlen = MultiByteToWideChar(CP_UTF8, 0, tempBuf, -1, NULL, 0);
    if (wlen <= 0) {
        // Si la conversion échoue, on renvoie le texte brut
        strncpy(RETBUF[idx], tempBuf, sizeof(RETBUF[idx]) - 1);
        RETBUF[idx][sizeof(RETBUF[idx]) - 1] = '\0';
        return RETBUF[idx];
    }

    WCHAR* wideBuf = (WCHAR*)malloc(wlen * sizeof(WCHAR));
    if (!wideBuf) {
        strncpy(RETBUF[idx], tempBuf, sizeof(RETBUF[idx]) - 1);
        RETBUF[idx][sizeof(RETBUF[idx]) - 1] = '\0';
        return RETBUF[idx];
    }

    MultiByteToWideChar(CP_UTF8, 0, tempBuf, -1, wideBuf, wlen);

    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wideBuf, -1, NULL, 0, NULL, NULL);
    if (ansiLen <= 0 || ansiLen >= (int)sizeof(RETBUF[idx])) {
        free(wideBuf);
        strncpy(RETBUF[idx], tempBuf, sizeof(RETBUF[idx]) - 1);
        RETBUF[idx][sizeof(RETBUF[idx]) - 1] = '\0';
        return RETBUF[idx];
    }

    WideCharToMultiByte(CP_ACP, 0, wideBuf, -1, RETBUF[idx], ansiLen, NULL, NULL);
    free(wideBuf);

    RETBUF[idx][sizeof(RETBUF[idx]) - 1] = '\0';

    // supprimer CR par LF
    for (char *p = RETBUF[idx]; *p; p++) {
        if (*p == '\r')
        *p = '\n';
    }

    return RETBUF[idx];
}

__declspec(dllexport) const char* file_write(char* arg){
    if (!arg) return retstr_printf("ERR: arg null");
    // format name;text (text can contain ;)
    char *semi = strchr(arg, ';');
    if (!semi) return retstr_printf("ERR: format: name;text");
    size_t name_len = (size_t)(semi - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len); namebuf[name_len] = '\0';

    char *text = semi + 1; // can be empty string

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e){ leave_cs(); return retstr_printf("ERR: name not found"); }

    if (!(e->mode == 'a' || e->mode == 'w')){
        leave_cs(); return retstr_printf("ERR: file not opened for writing (mode not a or w)");
    }

    size_t addlen = strlen(text);
    size_t needed_len = e->pos + addlen;
    if (needed_len + 1 > e->content_cap){
        if (ensure_content_cap(e, needed_len + 1) != 0){
            leave_cs(); return retstr_printf("ERR: mem");
        }
    }

    // if pos > content_len, pad with '\0' (so result is binary-safe)
    if (e->pos > e->content_len){
        size_t pad = e->pos - e->content_len;
        memset(e->content + e->content_len, 0, pad);
        e->content_len = e->pos;
        e->content[e->content_len] = '\0';
    }

    // write (overwrite existing or append)
    memcpy(e->content + e->pos, text, addlen);
    e->pos += addlen;
    if (e->pos > e->content_len) e->content_len = e->pos;
    e->content[e->content_len] = '\0';

    leave_cs();
    return retstr_printf("OK");
}

__declspec(dllexport) const char* file_tell(char* arg){
    if (!arg) return retstr_printf("ERR: arg null");
    // name only
    char *semi = strchr(arg, ';');
    size_t name_len = semi ? (size_t)(semi - arg) : strlen(arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    char namebuf[512]; if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len); namebuf[name_len] = '\0';

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e){ leave_cs(); return retstr_printf("ERR: name not found"); }
    size_t pos = e->pos;
    leave_cs();
    return retstr_printf("%zu", pos);
}

// Déplace le pointeur de lecture/écriture dans le contenu mémoire
__declspec(dllexport) const char* file_seek(char* arg) {
    if (!arg) return retstr_printf("ERR: arg null");
    // Format: name;offset;origin
    char *p1 = strchr(arg, ';');
    if (!p1) return retstr_printf("ERR: format: name;offset;origin");
    size_t name_len = (size_t)(p1 - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");

    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len);
    namebuf[name_len] = '\0';

    char *p2 = strchr(p1 + 1, ';');
    if (!p2) return retstr_printf("ERR: format: name;offset;origin");

    // Décodage offset et origin
    long offset = atol(p1 + 1);
    int origin = atoi(p2 + 1);

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e) {
        leave_cs();
        return retstr_printf("ERR: file not found");
    }

    size_t newpos = e->pos; // par défaut
    switch (origin) {
        case 0: // SEEK_SET
            if (offset >= 0)
                newpos = (size_t)offset;
            else
                return retstr_printf("ERR: negative pos with SEEK_SET");
            break;

        case 1: // SEEK_CUR
            if (offset < 0 && (size_t)(-offset) > e->pos)
                newpos = 0;
            else
                newpos = e->pos + offset;
            break;

        case 2: // SEEK_END
            if (offset > 0)
                newpos = e->content_len; // au-delà interdit
            else if ((size_t)(-offset) > e->content_len)
                newpos = 0;
            else
                newpos = e->content_len + offset;
            break;

        default:
            leave_cs();
            return retstr_printf("ERR: invalid origin (must be 0,1,2)");
    }

    // Sécuriser les bornes
    if (newpos > e->content_len)
        newpos = e->content_len;

    e->pos = newpos;
    leave_cs();

    return retstr_printf("OK:%zu", newpos);
}

//recherche un mot dans un fichier et renvoie la positiondu pointeur trouver ou -1 si échec
__declspec(dllexport) const char* file_find(char* arg) {
    if (!arg) return retstr_printf("ERR: arg null");

    // format : name;text_find;index_start
    char *p1 = strchr(arg, ';');
    if (!p1) return retstr_printf("ERR: format: name;text_find;index_start");
    size_t name_len = (size_t)(p1 - arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");

    char namebuf[512];
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len);
    namebuf[name_len] = '\0';

    char *p2 = strchr(p1 + 1, ';');
    if (!p2) return retstr_printf("-1");

    char *text_find = p1 + 1;
    size_t find_len = (size_t)(p2 - text_find);
    if (find_len == 0) return retstr_printf("-1");

    long index_start = atol(p2 + 1);

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e) {
        leave_cs();
        return retstr_printf("ERR: file not found");
    }

    size_t flen = e->content_len;
    if (flen == 0) {
        leave_cs();
        return retstr_printf("-1");
    }

    long start;
    if (index_start >= 0) {
        if ((size_t)index_start >= flen) {
            leave_cs();
            return retstr_printf("-1");
        }
        start = index_start;
    } else {
        // depuis la fin
        start = (long)flen + index_start;
        if (start < 0) start = 0;
    }

    // ==== Recherche avant ====
    if (index_start >= 0) {
        for (size_t i = (size_t)start; i + find_len <= flen; i++) {
            if (memcmp(e->content + i, text_find, find_len) == 0) {
                leave_cs();
                return retstr_printf("%zu", i);
            }
        }
    }
    // ==== Recherche arrière ====
    else {
        for (long i = start; i >= 0; i--) {
            if ((size_t)i + find_len <= flen &&
                memcmp(e->content + i, text_find, find_len) == 0) {
                leave_cs();
                return retstr_printf("%ld", i);
            }
        }
    }

    leave_cs();
    return retstr_printf("-1");
}

// Renvoie la longueur du contenu du fichier
__declspec(dllexport) const char* file_len(char* arg) {
    if (!arg) return retstr_printf("ERR: arg null");

    char namebuf[512];
    size_t name_len = strlen(arg);
    if (name_len == 0) return retstr_printf("ERR: empty name");
    if (name_len >= sizeof(namebuf)) return retstr_printf("ERR: name too long");
    memcpy(namebuf, arg, name_len);
    namebuf[name_len] = '\0';

    enter_cs();
    FileEntry *e = find_entry_locked(namebuf);
    if (!e) {
        leave_cs();
        return retstr_printf("ERR: file not found");
    }

    size_t len = e->content_len;
    leave_cs();

    if (len > MAX_ORDER_SCRIPT_LEN)
        return retstr_printf("TO LONG");
    else
        return retstr_printf("%zu", len);
}
