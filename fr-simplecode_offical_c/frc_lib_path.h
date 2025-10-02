//ficher de gestion des librairy de fonction (chemin des fichiers)

typedef struct {
    char *key;    // ex: "math"
    char *path;   // ex: "scripts/math.func"
} FuncNamespace;

FuncNamespace func_namespaces[128];
int func_namespaces_count = 0;

// ajouter une association clé/valeur
void register_func_namespace(const char *key, const char *path) {
    if (func_namespaces_count < 128) {
        func_namespaces[func_namespaces_count].key = strdup(key);
        func_namespaces[func_namespaces_count].path = strdup(path);
        func_namespaces_count++;
    }
}

// retrouver le chemin à partir de la clé
const char *find_func_namespace(const char *key) {
    for (int i = 0; i < func_namespaces_count; i++) {
        if (strcmp(func_namespaces[i].key, key) == 0) {
            return func_namespaces[i].path;
        }
    }
    return NULL;
}

void clear_all_register_func_namespace() {
    for (int i = 0; i < func_namespaces_count; i++) {
        if (func_namespaces[i].key) {
            free(func_namespaces[i].key);
            func_namespaces[i].key = NULL;
        }
        if (func_namespaces[i].path) {
            free(func_namespaces[i].path);
            func_namespaces[i].path = NULL;
        }
    }
    func_namespaces_count = 0;
}
