/*
#include <windows.h>
#include <string.h>
#include <stdlib.h>
*/

/*
Cette version oblige la sauvgarde imédiate du process,

cache_save_variable doit être appeler en plus de save_variable

*/

#define VARIABLE_CACHE_SIZE 10 // 10 slot suffisent
#define VARIABLE_CACHE_TIME_MS 10 //time check 10ms

typedef struct {
    char *name;
    char *value;
    ULONGLONG timestamp;
    bool used;
} VariableCacheEntry;

_Thread_local static VariableCacheEntry variable_cache[VARIABLE_CACHE_SIZE] = {0};

int variable_cache_find(const char *name){
    if (!name)
        return -1;

    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {
        if (variable_cache[i].used &&
            strcmp(variable_cache[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int variable_cache_get_slot(void) {
    // Chercher une case libre
    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {
        if (!variable_cache[i].used)
            return i;
    }

    // Cache plein : remplacer la plus ancienne
    int oldest = 0;

    for (int i = 1; i < VARIABLE_CACHE_SIZE; i++) {
        if (variable_cache[i].timestamp <
            variable_cache[oldest].timestamp) {
            oldest = i;
        }
    }

    free(variable_cache[oldest].name);
    free(variable_cache[oldest].value);

    variable_cache[oldest].name = NULL;
    variable_cache[oldest].value = NULL;
    variable_cache[oldest].used = false;

    return oldest;
}

int cache_save_variable(const char *name, const char *value_str) {
    if (!name || !value_str)
        return 0;

    int index = variable_cache_find(name);

    if (index < 0)
        index = variable_cache_get_slot();

    char *new_name = NULL;
    char *new_value = NULL;

    // Nouvelle entrée
    if (!variable_cache[index].used) {

        new_name = strdup(name);
        new_value = strdup(value_str);

        if (!new_name) {
            free(new_name);
            return 0;
        }
        if (!new_value) {
            free(new_value);
            return 0;
        }

        variable_cache[index].name = new_name;
        variable_cache[index].value = new_value;
        variable_cache[index].used = true;

    } else {
        // Entrée existante : remplacer uniquement la valeur
        new_value = strdup(value_str);

        if (!new_value)
            return 0;

        free(variable_cache[index].value);
        variable_cache[index].value = new_value;
    }

    variable_cache[index].timestamp = GetTickCount64();

    //information succes ou non (save est obligatoire comme même)
    return 1;
}

char *cache_get_variable(const char *name) {
    if (!name)
        return NULL;

    int index = variable_cache_find(name);

    if (index < 0)
        return NULL;

    ULONGLONG now = GetTickCount64();

    if (now - variable_cache[index].timestamp > VARIABLE_CACHE_TIME_MS) {
        //Trop vieux
        //Le prochain save pourra la remettre à jour.
        return NULL;
    }

    char *result = strdup(variable_cache[index].value);

    return result;
}

void cache_clean_variables(void){
    for (int i = 0; i < VARIABLE_CACHE_SIZE; i++) {

        if (variable_cache[i].name) free(variable_cache[i].name);
        if (variable_cache[i].value) free(variable_cache[i].value);

        variable_cache[i].name = NULL;
        variable_cache[i].value = NULL;
        variable_cache[i].timestamp = 0;
        variable_cache[i].used = false;
    }
}

//juste correspondance avec frc_cache2.h mais pas utilisé
int cache_variable_init(void) {
    return 1;
}
