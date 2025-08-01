#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 200

//gcc -std=c11 -pthread frc_environnement.h -o frc_environnement.exe

typedef struct Entry {
    char* key;
    char* value;
    struct Entry* next;
} Entry;

typedef struct HashTable {
    char* name;
    Entry* buckets[TABLE_SIZE];
    struct HashTable* next;
} HashTable;

HashTable* environments = NULL;  // Liste des environnements (global, local, etc.)

#define MAX_ENV_STACK 64
#define MAX_ENV_LEN 512

// Pile locale à chaque thread
_Thread_local char env_stack[MAX_ENV_STACK][MAX_ENV_LEN];
_Thread_local int env_stack_top = -1;

// Pousser un nouvel environnement
void push_env_object(const char* env_name) {
    if (env_stack_top + 1 < MAX_ENV_STACK) {
        env_stack_top++;
        strncpy(env_stack[env_stack_top], env_name, MAX_ENV_LEN - 1);
        env_stack[env_stack_top][MAX_ENV_LEN - 1] = '\0';  // sécurité
    } else {
        printf("Stack d'environnements pleine !\n");
    }
}

// Retirer l'environnement courant
void pop_env_object() {
    if (env_stack_top >= 0) {
        env_stack_top--;
    } else {
        printf("Aucun environnement à retirer.\n");
    }
}

// Obtenir l'environnement courant
const char* current_env_object() {
    if (env_stack_top >= 0) {
        return env_stack[env_stack_top];
    } else {
        return "global";  // par défaut
    }
}

unsigned int hash(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash % TABLE_SIZE;
}

HashTable* find_or_create_env(const char* env_name) {
    HashTable* env = environments;
    while (env) {
        if (strcmp(env->name, env_name) == 0) return env;
        env = env->next;
    }

    HashTable* new_env = (HashTable*)malloc(sizeof(HashTable));
    new_env->name = strdup(env_name);
    for (int i = 0; i < TABLE_SIZE; i++) new_env->buckets[i] = NULL;
    new_env->next = environments;
    environments = new_env;
    return new_env;
}

void set_env_object(const char* env_name, const char* var, const char* value) {
    HashTable* env = find_or_create_env(env_name);
    unsigned int idx = hash(var);
    Entry* curr = env->buckets[idx];

    while (curr) {
        if (strcmp(curr->key, var) == 0) {
            free(curr->value);
            curr->value = strdup(value);
            return;
        }
        curr = curr->next;
    }

    Entry* new_entry = (Entry*)malloc(sizeof(Entry));
    new_entry->key = strdup(var);
    new_entry->value = strdup(value);
    new_entry->next = env->buckets[idx];
    env->buckets[idx] = new_entry;
}

const char* get_env_object(const char* env_name, const char* var) {
    HashTable* env = environments;
    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            unsigned int idx = hash(var);
            Entry* curr = env->buckets[idx];
            while (curr) {
                if (strcmp(curr->key, var) == 0) {
                    return curr->value;
                }
                curr = curr->next;
            }
            return "Variable non trouvée.";
        }
        env = env->next;
    }
    return "Environnement non trouvé.";
}

void free_entries_env(Entry* entry) {
    while (entry) {
        Entry* next = entry->next;
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }
}

void clear_env_object_except_global() {
    HashTable* prev = NULL;
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, "global") == 0) {
            // On garde global, on avance
            prev = env;
            env = env->next;
        } else {
            // Supprimer cet environnement
            HashTable* to_delete = env;
            env = env->next;

            // Libérer les entrées
            for (int i = 0; i < TABLE_SIZE; i++) {
                free_entries_env(to_delete->buckets[i]);
            }
            free(to_delete->name);
            free(to_delete);

            // Retirer de la liste chaînée
            if (prev) {
                prev->next = env;
            } else {
                // Si on supprime le premier élément (différent de global)
                environments = env;
            }
        }
    }
}
