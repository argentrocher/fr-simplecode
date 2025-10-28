#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TABLE_SIZE
#define TABLE_SIZE 16384
#endif

//gcc -std=c11 -pthread frc_environnement.h -o frc_environnement.exe

//code qui gère l'environnement des obj local (table_local / var_local)

typedef struct Entry {
    char* key;
    char* value;
    struct Entry* next;
} Entry;

typedef struct HashTable {
    char* name;
    Entry* buckets_vars[TABLE_SIZE];
    Entry* buckets_tables[TABLE_SIZE];
    struct HashTable* next;
} HashTable;

HashTable* environments = NULL;  // Liste des environnements (global, local, etc.)

#define MAX_ENV_STACK 1024
#define MAX_ENV_LEN 2048

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
        fflush(stdout);
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

// Obtenir l'environnement précédent
const char* last_current_env_object() {
    if (env_stack_top >= 1) {
        return env_stack[env_stack_top-1];
    } else {
        return "0";  // par défaut
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
    for (int i = 0; i < TABLE_SIZE; i++) {
        new_env->buckets_vars[i] = NULL;
        new_env->buckets_tables[i] = NULL;
    }
    new_env->next = environments;
    environments = new_env;
    return new_env;
}

//ajouté une variable dans un env
void set_env_object(const char* env_name, const char* var, const char* value) {
    HashTable* env = find_or_create_env(env_name);
    unsigned int idx = hash(var);
    Entry* curr = env->buckets_vars[idx];

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
    new_entry->next = env->buckets_vars[idx];
    env->buckets_vars[idx] = new_entry;
}

//récupérer une variable dans un env
const char* get_env_object(const char* env_name, const char* var) {
    HashTable* env = environments;
    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            unsigned int idx = hash(var);
            Entry* curr = env->buckets_vars[idx];
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

// Supprime complètement un environnement donné
void free_env_object(const char* env_name) {
    HashTable* prev = NULL;
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            // Supprimer toutes les entrées
            for (int i = 0; i < TABLE_SIZE; i++) {
                free_entries_env(env->buckets_vars[i]);
                free_entries_env(env->buckets_tables[i]);
            }

            free(env->name);

            // Retirer de la liste chaînée
            if (prev) {
                prev->next = env->next;
            } else {
                environments = env->next;
            }

            free(env);
            return;
        }

        prev = env;
        env = env->next;
    }
}

// Retirer l'environnement courant
void pop_env_object() {
    if (env_stack_top >= 0) {
        const char* env_name = env_stack[env_stack_top];
        free_env_object(env_name);
        env_stack_top--;
    } else {
        printf("Aucun environnement à retirer.\n");
        fflush(stdout);
    }
}

//supprimé tout les env autre que global et leurs contenu
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
                free_entries_env(to_delete->buckets_vars[i]);
                free_entries_env(to_delete->buckets_tables[i]);
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

//liste de tout les env varable et contenu
char* list_env_object() {
    size_t buf_size = TABLE_SIZE;
    char* output = malloc(buf_size);
    if (!output) return NULL;
    output[0] = '\0';

    HashTable* env = environments;
    while (env) {
        strncat(output, "Environnement ", buf_size - strlen(output) - 1);
        strncat(output, env->name, buf_size - strlen(output) - 1);
        strncat(output, " :\n", buf_size - strlen(output) - 1);

        int has_vars = 0;
        int has_tables = 0;

        // Variables
        for (int i = 0; i < TABLE_SIZE; i++) {
            for (Entry* e = env->buckets_vars[i]; e; e = e->next) {
                if (!has_vars) {
                    strncat(output, " Variables:\n", buf_size - strlen(output) - 1);
                    has_vars=1;
                }
                char line[2048];
                snprintf(line, sizeof(line), "  %s = %s\n", e->key, e->value);
                strncat(output, line, buf_size - strlen(output) - 1);
            }
        }

        // Tables
        for (int i = 0; i < TABLE_SIZE; i++) {
            for (Entry* e = env->buckets_tables[i]; e; e = e->next) {
                if (!has_tables) {
                    strncat(output, " Tables:\n", buf_size - strlen(output) - 1);
                    has_tables=1;
                }
                char line[2048];
                snprintf(line, sizeof(line), "  %s = %s\n", e->key, e->value);
                strncat(output, line, buf_size - strlen(output) - 1);
            }
        }

        strncat(output, "\n", buf_size - strlen(output) - 1);
        env = env->next;
    }

    return output;
}

//supprime toutes les variables de l'environnement actuel
int free_var_env_object(const char* env_name) {
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            for (int i = 0; i < TABLE_SIZE; i++) {
                Entry* curr = env->buckets_vars[i];
                while (curr) {
                    Entry* temp = curr;
                    curr = curr->next;

                    free(temp->key);
                    free(temp->value);
                    free(temp);
                }
                env->buckets_vars[i] = NULL;  // Vide le bucket
            }
            return 1;  // Succès
        }
        env = env->next;
    }

    return 0;  // Environnement non trouvé
}

//supprime la variable demander de l'environnement actuel
int suppr_var_env_object(const char* env_name, const char* var) {
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            unsigned int idx = hash(var);
            Entry* curr = env->buckets_vars[idx];
            Entry* prev = NULL;

            while (curr) {
                if (strcmp(curr->key, var) == 0) {
                    if (prev) {
                        prev->next = curr->next;
                    } else {
                        env->buckets_vars[idx] = curr->next;
                    }

                    free(curr->key);
                    free(curr->value);
                    free(curr);
                    return 1;  // Supprimée avec succès
                }
                prev = curr;
                curr = curr->next;
            }

            return 0;  // Variable non trouvée
        }

        env = env->next;
    }

    return 0;  // Environnement non trouvé
}

//table_local
void set_table_env(const char* env_name, const char* table_name, const char* content) {
    HashTable* env = find_or_create_env(env_name);
    unsigned int idx = hash(table_name);
    Entry* curr = env->buckets_tables[idx];

    while (curr) {
        if (strcmp(curr->key, table_name) == 0) {
            free(curr->value);
            curr->value = strdup(content);
            return;
        }
        curr = curr->next;
    }

    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->key = strdup(table_name);
    new_entry->value = strdup(content);
    new_entry->next = env->buckets_tables[idx];
    env->buckets_tables[idx] = new_entry;
}

const char* get_table_env(const char* env_name, const char* table_name) {
    HashTable* env = environments;
    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            unsigned int idx = hash(table_name);
            Entry* curr = env->buckets_tables[idx];
            while (curr) {
                if (strcmp(curr->key, table_name) == 0) {
                    return curr->value;
                }
                curr = curr->next;
            }
            return "Table non trouvée.";
        }
        env = env->next;
    }
    return "Environnement non trouvé.";
}

int free_tables_env(const char* env_name) {
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            for (int i = 0; i < TABLE_SIZE; i++) {
                free_entries_env(env->buckets_tables[i]);
                env->buckets_tables[i] = NULL;
            }
            return 1;
        }
        env = env->next;
    }
    return 0;
}

int suppr_table_env(const char* env_name, const char* table_name) {
    HashTable* env = environments;

    while (env) {
        if (strcmp(env->name, env_name) == 0) {
            unsigned int idx = hash(table_name);
            Entry* curr = env->buckets_tables[idx];
            Entry* prev = NULL;

            while (curr) {
                if (strcmp(curr->key, table_name) == 0) {
                    if (prev) prev->next = curr->next;
                    else env->buckets_tables[idx] = curr->next;

                    free(curr->key);
                    free(curr->value);
                    free(curr);
                    return 1;
                }
                prev = curr;
                curr = curr->next;
            }
            return 0;
        }
        env = env->next;
    }
    return 0;
}
