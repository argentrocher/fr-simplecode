//frc_file_cache.h
//lib fr-simplecode pour le cache du dernier fichier de code lu (cache unique partagé multithread).
//le cache du fichier actuel, cela evite les rechargement permanent et permet d'appliquer les tag uniquement quand on charge réelement le fichier.


//Programme :
//start : InitializeCriticalSection(&code_file_cache_lock);
//end : free_code_file_cache();

typedef struct {
    char path[2048];
    FILETIME update;
    char *content;
    size_t content_size;
} CodeFileCache;

static CodeFileCache code_file_cache = {
    .path = "",
    .update = {0},
    .content = NULL,
    .content_size = 0
};

static CRITICAL_SECTION code_file_cache_lock;

//fin du programme libération de la mémoire
void free_code_file_cache() {
    EnterCriticalSection(&code_file_cache_lock);

    if (code_file_cache.content) free(code_file_cache.content);
    code_file_cache.content = NULL;
    code_file_cache.content_size = 0;
    code_file_cache.path[0] = '\0';
    code_file_cache.update = (FILETIME){0};

    LeaveCriticalSection(&code_file_cache_lock);
    DeleteCriticalSection(&code_file_cache_lock);
    return;
}

//fonction qui charge le fichier de code (return : 0 = err, 1 = file load, 2 = cache)
int import_code_file(char *path, char **content, size_t *content_size){
    if (!path || !content || !content_size)
        return 0;

    WIN32_FILE_ATTRIBUTE_DATA info;

    *content = NULL;
    *content_size = 0;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info)) {
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : impossible d'obtenir les informations du fichier : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: unable to get file information: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }

    //Vérification du cache
    EnterCriticalSection(&code_file_cache_lock);

    if (code_file_cache.content && strcmp(code_file_cache.path, path) == 0 && CompareFileTime(&info.ftLastWriteTime, &code_file_cache.update) == 0) {
        *content = code_file_cache.content;
        *content_size = code_file_cache.content_size;

        LeaveCriticalSection(&code_file_cache_lock);
        return 2;
    }

    LeaveCriticalSection(&code_file_cache_lock);


    //Tentative de verrouillage
    int locked = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (try_lock_file(path)) {
            locked = 1;
            break;
        } else {
            Sleep(2);
        }
    }

    if (!locked) {
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : fichier '%s' déjà utilisé, tentative échouée après 3 essais.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: '%s' file already used, failed after 3 attempts.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }


    //Ouverture du fichier
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        unlock_file(path);
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : impossible d’ouvrir le fichier : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: unable to open file: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }


    //Taille du fichier
    LARGE_INTEGER size;

    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        unlock_file(path);

        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : impossible d’obtenir la taille du fichier : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: unable to get file size: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }

    if (size.QuadPart < 0 ||
        (unsigned long long)size.QuadPart > (unsigned long long)SIZE_MAX - 1) {

        CloseHandle(hFile);
        unlock_file(path);
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : taille de fichier invalide : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: invalid file size: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }

    size_t file_size = (size_t)size.QuadPart;


    //Fichier vide
    if (file_size == 0) {

        HANDLE hMap = NULL;

        //Pas besoin de MapViewOfFile pour un fichier vide
        char *new_content = malloc(1);
        if (!new_content) {
            CloseHandle(hFile);
            unlock_file(path);
            error_handler__malloc("file cache");
            return 0;
        }

        new_content[0] = '\0';

        CloseHandle(hFile);
        unlock_file(path);


        EnterCriticalSection(&code_file_cache_lock);

        if (code_file_cache.content) free(code_file_cache.content);

        code_file_cache.content = new_content;
        code_file_cache.content_size = 0;
        code_file_cache.update = info.ftLastWriteTime;

        strncpy(code_file_cache.path, path, sizeof(code_file_cache.path) - 1);
        code_file_cache.path[sizeof(code_file_cache.path) - 1] = '\0';

        *content = code_file_cache.content;
        *content_size = code_file_cache.content_size;

        LeaveCriticalSection(&code_file_cache_lock);
        return 1;
    }


    //Création du mapping
    HANDLE hMap = CreateFileMappingA(
        hFile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );

    if (!hMap) {
        CloseHandle(hFile);
        unlock_file(path);
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : impossible d’ouvrir le fichier : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: unable to open file: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }


    //Mapping mémoire
    char *view = MapViewOfFile(
        hMap,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (!view) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        unlock_file(path);
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : impossible d’ouvrir le fichier : '%s'.\n", path);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: unable to open file: '%s'.\n", path);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }


    //Copier dans le cache
    char *new_content = malloc(file_size + 1);
    if (!new_content) {
        UnmapViewOfFile(view);
        CloseHandle(hMap);
        CloseHandle(hFile);
        unlock_file(path);
        error_handler__malloc("file cache");
        return 0;
    }

    memcpy(new_content, view, file_size);
    new_content[file_size] = '\0';

    //Libérer le mapping
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
    unlock_file(path);

    //Mettre à jour le cache
    EnterCriticalSection(&code_file_cache_lock);

    if (code_file_cache.content) free(code_file_cache.content);

    code_file_cache.content = new_content;
    code_file_cache.content_size = file_size;
    code_file_cache.update = info.ftLastWriteTime;

    strncpy(code_file_cache.path, path, sizeof(code_file_cache.path) - 1);

    code_file_cache.path[sizeof(code_file_cache.path) - 1] = '\0';

    *content = code_file_cache.content;
    *content_size = code_file_cache.content_size;

    LeaveCriticalSection(&code_file_cache_lock);
    return 1;
}
