/*
#include <windows.h>
#include <string.h>
#include <stdlib.h>
*/

#define VARIABLE_CACHE_SIZE 100 //100 slot pour temps de sauvgarde
#define VARIABLE_CACHE_TIME_MS 10 //time check 10ms

typedef struct {
    char *name;
    char *value;
    //delay de validité de la variable (! n'est utiliser que lorsque la variable est déjà sauvgardé)
    ULONGLONG timestamp;
    // Numéro de version de l'entrée. Incrémenté à chaque modification.
    ULONGLONG generation;
    bool used; //utilisation en cours
    bool dirty; //besoin de sauvgarde en attente du thread
} VariableCacheEntry;

//les slot de sauvgardes des variables
static VariableCacheEntry variable_cache[VARIABLE_CACHE_SIZE] = {0};

static CRITICAL_SECTION variable_cache_cs;

static HANDLE variable_save_thread_handle = NULL; //thread qui gère l'enregistrement
static HANDLE variable_free_slot_event = NULL; //event du thread quand il libère un slot
static HANDLE variable_save_event = NULL; //event du process quand besion de réveiller le thread si en sleep pour l'enregistrement

static volatile LONG variable_save_shutdown = 0; //variable qui passe à 1 quand on demande l'arrêt du thread

//renvoie l'index si on a déjà la variable dans un slot
int variable_cache_find(const char *name){
    if (!name)
        return -1;

    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {
        if (variable_cache[i].used &&
            variable_cache[i].name &&
            strcmp(variable_cache[i].name, name) == 0) {

            return i;
        }
    }

    return -1;
}

//recupère un slot qui n'est pas en attente de sauvgarde (renvoie -1 sinon pour que le process attende la sauvgarde)
int variable_cache_get_slot(void) {
    // Case libre
    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {
        if (!variable_cache[i].used)
            return i;
    }

    // Chercher la plus ancienne entrée dirty.
    int oldest = -1;

    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {
        if (variable_cache[i].dirty)
            continue;

        if (oldest < 0 ||
            variable_cache[i].timestamp <
            variable_cache[oldest].timestamp) {

            oldest = i;
        }
    }

    //desactivé, attendre que un slot se libère
    /*
    // Si tout est propre, prendre quand même la plus ancienne.
    if (oldest < 0) {
        oldest = 0;

        for (int i = 1; i < VARIABLE_CACHE_SIZE; i++) {
            if (variable_cache[i].timestamp <
                variable_cache[oldest].timestamp) {

                oldest = i;
            }
        }
    }*/

    return oldest;
}

//renvoie l'index du slot le plus ancien qui attend une sauvgarde
int variable_cache_oldest_dirty(void) {
    int oldest = -1;

    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {

        if (!variable_cache[i].used ||
            !variable_cache[i].dirty) {

            continue;
        }

        if (oldest < 0 ||
            variable_cache[i].timestamp <
            variable_cache[oldest].timestamp) {

            oldest = i;
        }
    }

    return oldest;
}

//fonction d'entrée qui remplace l'appel direct à save_variable()
int cache_save_variable(const char *name, const char *value_str) {
    if (!name || !value_str)
        return 0;

    EnterCriticalSection(&variable_cache_cs);

    int index = variable_cache_find(name);

    if (index < 0) {
        while (1) {
            index = variable_cache_get_slot();
            //printf("index %d\n", index);

            if (index >= 0) {
                // Si la case était utilisée, elle est forcément remplacée.
                if (variable_cache[index].used) {
                    if (variable_cache[index].name) free(variable_cache[index].name);
                    if (variable_cache[index].value) free(variable_cache[index].value);

                    variable_cache[index].name = NULL;
                    variable_cache[index].value = NULL;
                    variable_cache[index].used = false;
                    variable_cache[index].dirty = false;
                }

                char *new_name = strdup(name);
                char *new_value = strdup(value_str);

                if (!new_name || !new_value) {
                    if (!new_name) free(new_name);
                    if (!new_value) free(new_value);

                    LeaveCriticalSection(&variable_cache_cs);
                    return 0;
                }

                variable_cache[index].name = new_name;
                variable_cache[index].value = new_value;

                variable_cache[index].used = true;
                variable_cache[index].dirty = true;
                variable_cache[index].generation = 1;
                break;
            } else {
                //printf("point sleep\n");
                LeaveCriticalSection(&variable_cache_cs);
                // Réveiller le thread de sauvegarde si endormi.
                SetEvent(variable_save_event);
                //attendre que un slot se libère
                WaitForSingleObject(variable_free_slot_event, 20);

                EnterCriticalSection(&variable_cache_cs);
            }
        }
    } else {
        char *new_value = strdup(value_str);

        if (!new_value) {
            LeaveCriticalSection(&variable_cache_cs);
            return 0;
        }

        if (variable_cache[index].value) free(variable_cache[index].value);
        variable_cache[index].value = new_value;

        variable_cache[index].dirty = true;
        variable_cache[index].generation++;
    }

    variable_cache[index].timestamp = GetTickCount();

    LeaveCriticalSection(&variable_cache_cs);

    // Réveiller le thread de sauvegarde.
    SetEvent(variable_save_event);

    return 1;
}

//fonction qui récupère la variable si elle est déjà présente dans le cache pour ne pas rechargé
char *cache_get_variable(const char *name) {
    if (!name)
        return NULL;

    EnterCriticalSection(&variable_cache_cs);

    int index = variable_cache_find(name);

    if (index < 0) {
        LeaveCriticalSection(&variable_cache_cs);
        return NULL;
    }

    if (variable_cache[index].dirty) {
        char *result = strdup(variable_cache[index].value);
        LeaveCriticalSection(&variable_cache_cs);
        return result;
    }

    ULONGLONG now = GetTickCount();

    if (now - variable_cache[index].timestamp >
        VARIABLE_CACHE_TIME_MS) {

        LeaveCriticalSection(&variable_cache_cs);
        return NULL;
    }

    char *result = strdup(variable_cache[index].value);

    LeaveCriticalSection(&variable_cache_cs);

    return result;
}

// Supprime une variable du cache
int cache_suppr_variable(const char *name){
    if (!name) return 0;

    EnterCriticalSection(&variable_cache_cs);

    int index = variable_cache_find(name);
    if (index < 0) {
        LeaveCriticalSection(&variable_cache_cs);
        return 0;
    }

    if (variable_cache[index].name) free(variable_cache[index].name);
    if (variable_cache[index].value) free(variable_cache[index].value);
    variable_cache[index].name = NULL;
    variable_cache[index].value = NULL;
    variable_cache[index].timestamp = 0;
    variable_cache[index].generation = 0;
    variable_cache[index].used = false;
    variable_cache[index].dirty = false;

    //event si le process attend un slot disponible (pas utile si le process est ici)
    //SetEvent(variable_free_slot_event);

    LeaveCriticalSection(&variable_cache_cs);
    return 1;
}

//thread qui ne fait que de la sauvgarde
static DWORD WINAPI variable_save_thread(void *arg) {
    (void)arg;

    for (;;) {
        //Attendre qu'une modification arrive.
        WaitForSingleObject(variable_save_event, INFINITE);

        for (;;) {

            char *name = NULL;
            char *value = NULL;
            ULONGLONG generation = 0;
            int index = -1;

            //Récupération de l'entrée à sauvegarder.
            EnterCriticalSection(&variable_cache_cs);

            index = variable_cache_oldest_dirty();

            if (index >= 0) {
                name = strdup(variable_cache[index].name);
                value = strdup(variable_cache[index].value);

                generation = variable_cache[index].generation;
            }

            //Vérifier si shutdown demandé.
            //on termine d'abord toutes les entrées dirty avant de quitte.
            bool shutdown = (InterlockedCompareExchange(
                &variable_save_shutdown,
                0,
                0) != 0);

            LeaveCriticalSection(&variable_cache_cs);

            //Plus rien à sauvegarder.
            if (index < 0) {
                if (shutdown)
                    return 0;
                break;
            }

            //erreur mem
            if (!name || !value) {
                if (!name) free(name);
                if (!value) free(value);
                continue;
            }

            //Écriture dans le fichier.
            save_variable(name, value);

            free(name);
            free(value);

            EnterCriticalSection(&variable_cache_cs);

            //L'entrée peut avoir été remplacée entre-temps.
            //Si la génération est identique, la valeur écrite correspond toujours à la dernière valeur connue.
            if (index >= 0 &&
                variable_cache[index].used &&
                variable_cache[index].generation == generation) {

                variable_cache[index].dirty = false;
                //event si le process attend un slot disponible
                SetEvent(variable_free_slot_event);
            }

            LeaveCriticalSection(&variable_cache_cs);
        }
        //printf("thread à tout enregistrer\n");
    }

    return 0;
}

//initialisation du thread et des event
int cache_variable_init(void) {
    InitializeCriticalSection(&variable_cache_cs);

    variable_save_event = CreateEventA(
        NULL,
        FALSE,      // auto-reset
        FALSE,
        NULL);

    if (!variable_save_event) {
        DeleteCriticalSection(&variable_cache_cs);
        return 0;
    }

    variable_free_slot_event = CreateEventA(
        NULL,
        FALSE,      // auto-reset
        FALSE,
        NULL);

    if (!variable_free_slot_event) {
        CloseHandle(variable_save_event);
        variable_save_event = NULL;
        DeleteCriticalSection(&variable_cache_cs);
        return 0;
    }

    InterlockedExchange(&variable_save_shutdown, 0);

    variable_save_thread_handle = CreateThread(
        NULL,
        0,
        variable_save_thread,
        NULL,
        0,
        NULL);

    if (!variable_save_thread_handle) {
        CloseHandle(variable_save_event);
        variable_save_event = NULL;
        CloseHandle(variable_free_slot_event);
        variable_free_slot_event = NULL;

        DeleteCriticalSection(&variable_cache_cs);
        return 0;
    }

    return 1;
}

//fin de programme, terminer la sauvgarde avant de exit
void cache_clean_variables(void) {
    if (!variable_save_thread_handle)
        return;

    //Demander au thread de terminer.
    InterlockedExchange(&variable_save_shutdown, 1);

    // Réveiller le thread de sauvegarde.
    SetEvent(variable_save_event);

    //Le thread va terminer toutes les entrées dirty puis s'arrêter
    WaitForSingleObject(variable_save_thread_handle, INFINITE);

    //thread terminé (clean memoire avant exit)

    CloseHandle(variable_save_thread_handle);
    variable_save_thread_handle = NULL;

    CloseHandle(variable_save_event);
    variable_save_event = NULL;
    CloseHandle(variable_free_slot_event);
    variable_free_slot_event = NULL;

    EnterCriticalSection(&variable_cache_cs);

    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {

        if (variable_cache[i].name) free(variable_cache[i].name);
        if (variable_cache[i].value) free(variable_cache[i].value);

        variable_cache[i].name = NULL;
        variable_cache[i].value = NULL;

        variable_cache[i].timestamp = 0;
        variable_cache[i].generation = 0;

        variable_cache[i].used = false;
        variable_cache[i].dirty = false;
    }

    LeaveCriticalSection(&variable_cache_cs);

    DeleteCriticalSection(&variable_cache_cs);
}
