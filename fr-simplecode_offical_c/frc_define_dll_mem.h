//lib pour eviter le chargement de define_dll_frc.txt à chaque fois en map mémoire tant que pas d'écriture

typedef struct {
    char *data; //fichier
    size_t size; //taille
    FILETIME last_write; //derniere ecriture
    char path[MAX_PATH]; //chemin du fichier
} DefineDLLMem;

static DefineDLLMem g_define_dll = {0};


int load_define_dll_file(){
    WIN32_FILE_ATTRIBUTE_DATA info;

    if (!GetFileAttributesExA(
            g_define_dll.path,
            GetFileExInfoStandard,
            &info)) {
        return 0;
    }

    // Déjà chargé et inchangé
    if (g_define_dll.data && CompareFileTime(&info.ftLastWriteTime, &g_define_dll.last_write) == 0) {
        return 1;
    }

    if (g_define_dll.data) free(g_define_dll.data);
    g_define_dll.data = NULL;
    g_define_dll.size = 0;

    HANDLE hFile =
        CreateFileA(
            g_define_dll.path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    LARGE_INTEGER sz;

    if (!GetFileSizeEx(hFile, &sz)){
        CloseHandle(hFile);
        return 0;
    }

    HANDLE hMap =
        CreateFileMappingA(
            hFile,
            NULL,
            PAGE_READONLY,
            0,
            0,
            NULL
        );

    if (!hMap){
        CloseHandle(hFile);
        return 0;
    }

    char *view =
        MapViewOfFile(
            hMap,
            FILE_MAP_READ,
            0,
            0,
            0
        );

    if (!view){
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 0;
    }

    g_define_dll.data = malloc(sz.QuadPart + 1);
    if (!g_define_dll.data){
        UnmapViewOfFile(view);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 0;
    }

    memcpy(g_define_dll.data, view, sz.QuadPart);

    g_define_dll.data[sz.QuadPart] = '\0';
    g_define_dll.size = (size_t)sz.QuadPart;

    g_define_dll.last_write = info.ftLastWriteTime;

    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return 1;
}
