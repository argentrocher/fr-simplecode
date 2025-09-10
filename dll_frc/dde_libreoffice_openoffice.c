#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//gcc -shared -o dde_libreoffice.dll dde_libreoffice.c -luser32

//soffice --calc "C:\path\testdde.ods"

//[[[{print("text(dll|C:path\dde_libreoffice.dll|[ReadDDE("server=soffice;topic=C:\path\testdde.ods;item=Feuille1.A9:A10;")])")}]]] --> renvoie le comptenu des cases A9 à A10
//seul la fonction ReadDDE fonctionne car WriteDDE est blocker nativement par libre office ou open office
// ! libre office jusqu'à la v6 car la fonction de dde peut être désactivé après

#define API __declspec(dllexport)

// Buffer global pour stocker le retour (simple, pas thread-safe)
static char g_retbuf[1024];

static DWORD g_dwDdeInst = 0;

HDDEDATA CALLBACK DdeCallback(
    UINT uType, UINT uFmt, HCONV hConv, HSZ hsz1,
    HSZ hsz2, HDDEDATA hData, ULONG_PTR dwData1, ULONG_PTR dwData2)
{
    return (HDDEDATA)NULL;
}

static int ensureDDE() {
    if (g_dwDdeInst != 0) return 1;
    UINT res = DdeInitializeW(&g_dwDdeInst, (PFNCALLBACK)DdeCallback,
                              APPCMD_CLIENTONLY, 0);
    return (res == DMLERR_NO_ERROR);
}

static void cleanupDDE() {
    if (g_dwDdeInst != 0) {
        DdeUninitialize(g_dwDdeInst);
        g_dwDdeInst = 0;
    }
}

// petit parseur clé=valeur;
char* getParam(const char* args, const char* key) {
    const char* p = strstr(args, key);
    if (!p) return NULL;
    p += strlen(key);
    if (*p == '=') p++;
    const char* end = strchr(p, ';');
    if (!end) end = p + strlen(p);
    size_t len = end - p;

    char* buf = (char*)malloc(len + 1); // alloue un buffer
    if (!buf) return NULL;
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf; // à libérer après usage
}

// ===== Fonction EXPORT écriture =====
//impossible par libre office/open office de base poke failed
API const char* WriteDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";

    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    char* s_text  = getParam(args, "text");
    if (!s_server || !s_topic || !s_item || !s_text) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s, text=%s\n",
           s_server, s_topic, s_item, s_text);
    fflush(stdout);

    // HSZ ANSI pour compatibilité CF_TEXT
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic,  CP_WINANSI);
    HSZ hszItem    = DdeCreateStringHandle(g_dwDdeInst, s_item,   CP_WINANSI);
    if (!hszService || !hszTopic || !hszItem) {
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: connect";
    }

    char abuf[2048];
    if (s_text[0] != '"') {
        snprintf(abuf, sizeof(abuf), "\"%s\"", s_text);
    } else {
        strncpy(abuf, s_text, sizeof(abuf)-1);
        abuf[sizeof(abuf)-1] = 0;
    }

    // Créer un buffer ANSI pour CF_TEXT
    SIZE_T cb = strlen(abuf) + 1;
    HDDEDATA hData = DdeCreateDataHandle(g_dwDdeInst,
                                         (LPBYTE)abuf,
                                         (DWORD)cb,
                                         0,
                                         hszItem,
                                         CF_TEXT,
                                         0);
    if (!hData) {
        DdeDisconnect(hConv);
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: datahandle";
    }

    HDDEDATA res = DdeClientTransaction(
        (LPBYTE)abuf,      // données à envoyer
        (DWORD)cb,           // taille en octets
        hConv,
        hszItem,
        CF_TEXT,
        XTYP_POKE,
        5000,
        NULL
    );

    DdeDisconnect(hConv);
    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server); free(s_topic); free(s_item); free(s_text);

    if (!res) return "ERR: poke failed";
    return "OK";
}

// ===== Fonction EXPORT lecture =====
API const char* ReadDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";
    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    if (!s_server || !s_topic || !s_item) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s\n",s_server,s_topic,s_item);
    fflush(stdout);

    //wchar_t wserver[256], wtopic[2048], witem[256];
    //mbstowcs(wserver, s_server, 255);
    //mbstowcs(wtopic,  s_topic,  2048);
    //mbstowcs(witem,   s_item,   255);

    wchar_t witem[256];
    mbstowcs(witem,   s_item,   255);

    // **HSZ ANSI**
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic, CP_WINANSI);

    if (!hszService || !hszTopic) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        //DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: connect";
    }

    HSZ hszItem    = DdeCreateStringHandleW(g_dwDdeInst, witem, CP_WINUNICODE);
    if (!hszItem) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle 2";
    }

    HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem,
                                         CF_TEXT, XTYP_REQUEST,
                                         5000, NULL);
    if (!hData) {
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: request";
    }

    DWORD cb;
    LPBYTE p = DdeAccessData(hData, &cb);
    if (!p) {
        DdeFreeDataHandle(hData);
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: access";
    }

    // copie directe ANSI
    strncpy(g_retbuf, (char*)p, sizeof(g_retbuf)-1);
    g_retbuf[sizeof(g_retbuf)-1] = 0;

    DdeUnaccessData(hData);
    DdeFreeDataHandle(hData);
    DdeDisconnect(hConv);

    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server);
    free(s_topic);
    free(s_item);

    return g_retbuf;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH) {
        cleanupDDE();
    }
    return TRUE;
}

// Buffer global pour stocker le retour (simple, pas thread-safe)
static char g_retbuf[1024];

static DWORD g_dwDdeInst = 0;

HDDEDATA CALLBACK DdeCallback(
    UINT uType, UINT uFmt, HCONV hConv, HSZ hsz1,
    HSZ hsz2, HDDEDATA hData, ULONG_PTR dwData1, ULONG_PTR dwData2)
{
    return (HDDEDATA)NULL;
}

static int ensureDDE() {
    if (g_dwDdeInst != 0) return 1;
    UINT res = DdeInitializeW(&g_dwDdeInst, (PFNCALLBACK)DdeCallback,
                              APPCMD_CLIENTONLY, 0);
    return (res == DMLERR_NO_ERROR);
}

static void cleanupDDE() {
    if (g_dwDdeInst != 0) {
        DdeUninitialize(g_dwDdeInst);
        g_dwDdeInst = 0;
    }
}

// petit parseur clé=valeur;
char* getParam(const char* args, const char* key) {
    const char* p = strstr(args, key);
    if (!p) return NULL;
    p += strlen(key);
    if (*p == '=') p++;
    const char* end = strchr(p, ';');
    if (!end) end = p + strlen(p);
    size_t len = end - p;

    char* buf = (char*)malloc(len + 1); // alloue un buffer
    if (!buf) return NULL;
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf; // à libérer après usage
}

// ===== Fonction EXPORT écriture =====
//impossible par libre office/open office de base poke failed
API const char* WriteDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";

    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    char* s_text  = getParam(args, "text");
    if (!s_server || !s_topic || !s_item || !s_text) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s, text=%s\n",
           s_server, s_topic, s_item, s_text);
    fflush(stdout);

    // HSZ ANSI pour compatibilité CF_TEXT
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic,  CP_WINANSI);
    HSZ hszItem    = DdeCreateStringHandle(g_dwDdeInst, s_item,   CP_WINANSI);
    if (!hszService || !hszTopic || !hszItem) {
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: connect";
    }

    char abuf[2048];
    if (s_text[0] != '"') {
        snprintf(abuf, sizeof(abuf), "\"%s\"", s_text);
    } else {
        strncpy(abuf, s_text, sizeof(abuf)-1);
        abuf[sizeof(abuf)-1] = 0;
    }

    // Créer un buffer ANSI pour CF_TEXT
    SIZE_T cb = strlen(abuf) + 1;
    HDDEDATA hData = DdeCreateDataHandle(g_dwDdeInst,
                                         (LPBYTE)abuf,
                                         (DWORD)cb,
                                         0,
                                         hszItem,
                                         CF_TEXT,
                                         0);
    if (!hData) {
        DdeDisconnect(hConv);
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: datahandle";
    }

    HDDEDATA res = DdeClientTransaction(
        (LPBYTE)abuf,      // données à envoyer
        (DWORD)cb,           // taille en octets
        hConv,
        hszItem,
        CF_TEXT,
        XTYP_POKE,
        5000,
        NULL
    );

    DdeDisconnect(hConv);
    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server); free(s_topic); free(s_item); free(s_text);

    if (!res) return "ERR: poke failed";
    return "OK";
}

// ===== Fonction EXPORT lecture =====
API const char* ReadDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";
    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    if (!s_server || !s_topic || !s_item) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s\n",s_server,s_topic,s_item);
    fflush(stdout);

    //wchar_t wserver[256], wtopic[2048], witem[256];
    //mbstowcs(wserver, s_server, 255);
    //mbstowcs(wtopic,  s_topic,  2048);
    //mbstowcs(witem,   s_item,   255);

    wchar_t witem[256];
    mbstowcs(witem,   s_item,   255);

    // **HSZ ANSI**
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic, CP_WINANSI);

    if (!hszService || !hszTopic) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        //DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: connect";
    }

    HSZ hszItem    = DdeCreateStringHandleW(g_dwDdeInst, witem, CP_WINUNICODE);
    if (!hszItem) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle 2";
    }

    HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem,
                                         CF_TEXT, XTYP_REQUEST,
                                         5000, NULL);
    if (!hData) {
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: request";
    }

    DWORD cb;
    LPBYTE p = DdeAccessData(hData, &cb);
    if (!p) {
        DdeFreeDataHandle(hData);
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: access";
    }

    // copie directe ANSI
    strncpy(g_retbuf, (char*)p, sizeof(g_retbuf)-1);
    g_retbuf[sizeof(g_retbuf)-1] = 0;

    DdeUnaccessData(hData);
    DdeFreeDataHandle(hData);
    DdeDisconnect(hConv);

    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server);
    free(s_topic);
    free(s_item);

    return g_retbuf;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH) {
        cleanupDDE();
    }
    return TRUE;
}

// Buffer global pour stocker le retour (simple, pas thread-safe)
static char g_retbuf[1024];

static DWORD g_dwDdeInst = 0;

HDDEDATA CALLBACK DdeCallback(
    UINT uType, UINT uFmt, HCONV hConv, HSZ hsz1,
    HSZ hsz2, HDDEDATA hData, ULONG_PTR dwData1, ULONG_PTR dwData2)
{
    return (HDDEDATA)NULL;
}

static int ensureDDE() {
    if (g_dwDdeInst != 0) return 1;
    UINT res = DdeInitializeW(&g_dwDdeInst, (PFNCALLBACK)DdeCallback,
                              APPCMD_CLIENTONLY, 0);
    return (res == DMLERR_NO_ERROR);
}

static void cleanupDDE() {
    if (g_dwDdeInst != 0) {
        DdeUninitialize(g_dwDdeInst);
        g_dwDdeInst = 0;
    }
}

// petit parseur clé=valeur;
char* getParam(const char* args, const char* key) {
    const char* p = strstr(args, key);
    if (!p) return NULL;
    p += strlen(key);
    if (*p == '=') p++;
    const char* end = strchr(p, ';');
    if (!end) end = p + strlen(p);
    size_t len = end - p;

    char* buf = (char*)malloc(len + 1); // alloue un buffer
    if (!buf) return NULL;
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf; // à libérer après usage
}

// ===== Fonction EXPORT écriture =====
//impossible par libre office/open office de base poke failed
API const char* WriteDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";

    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    char* s_text  = getParam(args, "text");
    if (!s_server || !s_topic || !s_item || !s_text) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s, text=%s\n",
           s_server, s_topic, s_item, s_text);
    fflush(stdout);

    // HSZ ANSI pour compatibilité CF_TEXT
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic,  CP_WINANSI);
    HSZ hszItem    = DdeCreateStringHandle(g_dwDdeInst, s_item,   CP_WINANSI);
    if (!hszService || !hszTopic || !hszItem) {
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: connect";
    }

    char abuf[2048];
    if (s_text[0] != '"') {
        snprintf(abuf, sizeof(abuf), "\"%s\"", s_text);
    } else {
        strncpy(abuf, s_text, sizeof(abuf)-1);
        abuf[sizeof(abuf)-1] = 0;
    }

    // Créer un buffer ANSI pour CF_TEXT
    SIZE_T cb = strlen(abuf) + 1;
    HDDEDATA hData = DdeCreateDataHandle(g_dwDdeInst,
                                         (LPBYTE)abuf,
                                         (DWORD)cb,
                                         0,
                                         hszItem,
                                         CF_TEXT,
                                         0);
    if (!hData) {
        DdeDisconnect(hConv);
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server); free(s_topic); free(s_item); free(s_text);
        return "ERR: datahandle";
    }

    HDDEDATA res = DdeClientTransaction(
        (LPBYTE)abuf,      // données à envoyer
        (DWORD)cb,           // taille en octets
        hConv,
        hszItem,
        CF_TEXT,
        XTYP_POKE,
        5000,
        NULL
    );

    DdeDisconnect(hConv);
    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server); free(s_topic); free(s_item); free(s_text);

    if (!res) return "ERR: poke failed";
    return "OK";
}

// ===== Fonction EXPORT lecture =====
API const char* ReadDDE(const char* args) {
    if (!ensureDDE()) return "ERR: init failed";
    char* s_server = getParam(args, "server");
    char* s_topic  = getParam(args, "topic");
    char* s_item   = getParam(args, "item");
    if (!s_server || !s_topic || !s_item) return "ERR: missing param";

    printf("arg dde : server=%s, topic=%s, item=%s\n",s_server,s_topic,s_item);
    fflush(stdout);

    //wchar_t wserver[256], wtopic[2048], witem[256];
    //mbstowcs(wserver, s_server, 255);
    //mbstowcs(wtopic,  s_topic,  2048);
    //mbstowcs(witem,   s_item,   255);

    wchar_t witem[256];
    mbstowcs(witem,   s_item,   255);

    // **HSZ ANSI**
    HSZ hszService = DdeCreateStringHandle(g_dwDdeInst, s_server, CP_WINANSI);
    HSZ hszTopic   = DdeCreateStringHandle(g_dwDdeInst, s_topic, CP_WINANSI);

    if (!hszService || !hszTopic) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle";
    }

    HCONV hConv = DdeConnect(g_dwDdeInst, hszService, hszTopic, NULL);
    if (!hConv) {
        DdeFreeStringHandle(g_dwDdeInst, hszService);
        DdeFreeStringHandle(g_dwDdeInst, hszTopic);
        //DdeFreeStringHandle(g_dwDdeInst, hszItem);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: connect";
    }

    HSZ hszItem    = DdeCreateStringHandleW(g_dwDdeInst, witem, CP_WINUNICODE);
    if (!hszItem) {
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: string handle 2";
    }

    HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem,
                                         CF_TEXT, XTYP_REQUEST,
                                         5000, NULL);
    if (!hData) {
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: request";
    }

    DWORD cb;
    LPBYTE p = DdeAccessData(hData, &cb);
    if (!p) {
        DdeFreeDataHandle(hData);
        DdeDisconnect(hConv);
        free(s_server);
        free(s_topic);
        free(s_item);
        return "ERR: access";
    }

    // copie directe ANSI
    strncpy(g_retbuf, (char*)p, sizeof(g_retbuf)-1);
    g_retbuf[sizeof(g_retbuf)-1] = 0;

    DdeUnaccessData(hData);
    DdeFreeDataHandle(hData);
    DdeDisconnect(hConv);

    DdeFreeStringHandle(g_dwDdeInst, hszService);
    DdeFreeStringHandle(g_dwDdeInst, hszTopic);
    DdeFreeStringHandle(g_dwDdeInst, hszItem);

    free(s_server);
    free(s_topic);
    free(s_item);

    return g_retbuf;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH) {
        cleanupDDE();
    }
    return TRUE;
}
