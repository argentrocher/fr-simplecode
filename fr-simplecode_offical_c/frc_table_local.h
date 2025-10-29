//table local process
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//necessite frc_environnement.h

//argentropcher (assigné au compte google argentropcher) tout droit réservé (code libre, modification autorisé avec citation de l'auteur)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//zone fonction pour compute_values_part_from_rhs()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static int read_table_inner_from_local(const char *nom_table, char *out, size_t outsz) {
    if (!nom_table || !out) return 0;

    const char *env_name = current_env_object();
    const char *table_val = get_table_env(env_name, nom_table);

    if (!table_val || strcmp(table_val, "Table non trouvée.") == 0 || strcmp(table_val,"Environnement non trouvé.") == 0)
        return 0;

    const char *p = table_val;
    const char *start = strstr(p, "[[");
    const char *end = NULL;

    if (!start) return 0;
    start += 2; // avance après [[

    const char *q = start;
    int depth = 1;
    int in_quotes = 0;

    while (*q) {
        if (*q == '"') { in_quotes = !in_quotes; q++; continue; }
        if (!in_quotes && strncmp(q, "[[", 2) == 0) { depth++; q += 2; continue; }
        if (!in_quotes && strncmp(q, "]]", 2) == 0) {
            depth--;
            if (depth == 0) { end = q; break; }
            q += 2;
            continue;
        }
        q++;
    }

    if (!end) return 0;

    size_t inner_len = end - start;
    if (inner_len >= outsz) inner_len = outsz - 1;

    strncpy(out, start, inner_len);
    out[inner_len] = '\0';

    return 1;
}

static int is_table_reference_local(const char *s, char *table_name_out, size_t outsz) {
    if (!s || !table_name_out) return 0;

    const char *t = strstr(s, "table_local|");
    if (!t || t != s) return 0; // doit commencer exactement à s

    const char *sep = strchr(t + 12, '|');
    if (!sep) return 0;

    size_t n = sep - (t + 12);
    if (n >= outsz) n = outsz - 1;

    strncpy(table_name_out, t + 12, n);
    table_name_out[n] = '\0';

    return 1;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//fin compute_values_from_rhs()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//pour table_local||=suppr()
int suppr_save_table_local(const char *input_line) {
    if (!input_line) return 0;
    if (strstr(input_line, "|=suppr()") == NULL) return 0;
    if (strncmp(input_line, "table_local|", 12) != 0) return 0;

    const char *env_name = current_env_object();

    // Extraire le nom de la table
    const char *start = input_line + 12;
    const char *mid_bar = strchr(start, '|');
    if (!mid_bar) return 0;

    char table_name[MAX_ORDER_SCRIPT_LEN];
    size_t len = mid_bar - start;
    strncpy(table_name, start, len);
    table_name[len] = '\0';

    // Supprimer la table de l’environnement
    int res = suppr_table_env(env_name, table_name);
    if (!res) {
        if (!error_in_stderr)
            printf("Erreur : table %s inexistante dans l'environnement %s\n", table_name, env_name), fflush(stdout);
        else
            fprintf(stderr, "Erreur : table %s inexistante dans l'environnement %s\n", table_name, env_name), fflush(stderr);
        if (error_lock_program) var_script_exit = 1;
        return 0;
    }

    return 1;
}

//pour table_local||=++ ou table_local||=--
int add_sub_save_table_local(const char *input_line) {
    if (!input_line) return 0;
    if (strncmp(input_line, "table_local|", 12) != 0) return 0;

    const char *env_name = current_env_object();

    // Vérifier si c’est ++ ou --
    int is_add = 0;
    if (strstr(input_line + 12, "|=++") != NULL)
        is_add = 1;
    else if (strstr(input_line + 12, "|=--") == NULL)
        return 0; // ni ++ ni --

    // Extraire le nom de la table
    const char *start = input_line + 12;
    const char *sep = strchr(start, '|');
    if (!sep) return 0;

    char table_name[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(table_name, start, sep - start);
    table_name[sep - start] = '\0';

    // Récupérer le contenu actuel de la table
    const char *current_val = get_table_env(env_name, table_name);
    if (!current_val || strcmp(current_val, "Table non trouvée.") == 0 || strcmp(current_val,"Environnement non trouvé.") == 0) {
        // Si la table n’existe pas encore
        if (is_add) {
            char buf[64];
            regex_format_double(0.0, buf, sizeof(buf));
            char new_table[MAX_ORDER_SCRIPT_LEN];
            snprintf(new_table, sizeof(new_table), "[[%s]]", buf);
            set_table_env(env_name, table_name, new_table);
            return 1;
        } else {
            if (!error_in_stderr)
                printf("Erreur : la table local %s n'existe pas dans %s\n", table_name, env_name), fflush(stdout);
            else
                fprintf(stderr, "Erreur : la table local %s n'existe pas dans %s\n", table_name, env_name), fflush(stderr);
            if (error_lock_program) var_script_exit = 1;
            return 0;
        }
    }

    // Copie de travail
    char buf[MAX_ORDER_SCRIPT_LEN];
    strncpy(buf, current_val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Pointeurs pour [[ ... ]]
    char *val_start = strstr(buf, "[[");
    char *val_end = NULL;
    char *last = strstr(buf, "]]"); // utilitaire -> trouver le dernier ]]
    while (last != NULL){
        val_end = last;
        last = strstr(last + 1, "]]");
    }

    if (!val_start || !val_end) {
        if (!error_in_stderr)
            printf("Erreur : format de table local invalide pour %s\n", table_name), fflush(stdout);
        else
            fprintf(stderr, "Erreur : format de table local invalide pour %s\n", table_name), fflush(stderr);
        if (error_lock_program) var_script_exit = 1;
        return 0;
    }

    if (is_add) {
        // Ajouter un nouvel élément ;[[valeur]]
        char valbuf[64];
        regex_format_double(0.0, valbuf, sizeof(valbuf));

        *val_end = '\0';
        trim_inplace(buf);

        int is_empty = 0;
        if (strcmp(buf, "[[") == 0) is_empty = 1;

        char new_val[MAX_ORDER_SCRIPT_LEN];
        if (is_empty)
            snprintf(new_val, sizeof(new_val), "%s%s]]", buf, valbuf);
        else
            snprintf(new_val, sizeof(new_val), "%s;%s]]", buf, valbuf);

        set_table_env(env_name, table_name, new_val);
    } else {
        // Supprimer le dernier élément
        char *cur = val_end - 1;
        int in_quotes = 0;
        int paren_depth = 0;
        int table_depth = 0;
        int found_sep = 0;

        while (cur > val_start) {
            if (*cur == '"') in_quotes = !in_quotes;
            else if (!in_quotes) {
                if (*cur == ')') paren_depth++;
                else if (*cur == '(' && paren_depth > 0) paren_depth--;
                else if (cur > val_start + 1 && strncmp(cur - 1, "]]", 2) == 0 && paren_depth == 0) { table_depth++; cur--; }
                else if (cur > val_start + 1 && strncmp(cur - 1, "[[", 2) == 0 && paren_depth == 0 && table_depth > 0) { table_depth--; cur--; }
                else if (*cur == ';' && paren_depth == 0 && table_depth == 0 && !in_quotes) {
                    found_sep = 1;
                    break;
                }
            }
            cur--;
        }

        if (found_sep && cur >= val_start) {
            *cur = '\0';
            char new_val[MAX_ORDER_SCRIPT_LEN];
            snprintf(new_val, sizeof(new_val), "%s]]", buf);
            set_table_env(env_name, table_name, new_val);
        } else {
            // Table vide
            set_table_env(env_name, table_name, "[[]]");
        }
    }

    return 1;
}

//fonction pour table=[[;;]]
int parse_and_save_table_local(const char *input_line, const char *save_env_name) {
    char table_name[MAX_ORDER_SCRIPT_LEN];
    const char *start = strstr(input_line, "table_local|");
    if (!start) return 0;

    start += 12;
    const char *sep = strchr(start, '|');
    if (!sep) return 0;

    strncpy(table_name, start, sep - start);
    table_name[sep - start] = '\0';

    if (strpbrk(table_name, "[]={}\"")) {
        if (error_in_stderr==false){
            printf("Erreur : le nom de la table local contient un caractère interdit : [ ] = { } \"\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : le nom de la table local contient un caractère interdit : [ ] = { } \"\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return 0;
    }

    if (strstr(input_line, "|=suppr()")!=NULL){
            int resultat=suppr_save_table_local(input_line);
            return resultat;
    }

    if ((strstr(input_line, "|=--")!=NULL) || (strstr(input_line, "|=++")!=NULL)){
            int resultat=add_sub_save_table_local(input_line);
            return resultat;
    }

    const char *equal = strstr(input_line, "=");
    if (!equal) return 0;
    equal++;

    const char *p = equal;
    while (*p){
        p++;
    }

    const char *end = p;

    char values_part[MAX_ORDER_SCRIPT_LEN];
    char raw_values[MAX_ORDER_SCRIPT_LEN];
    size_t len = end - equal;
    if (len >= sizeof(raw_values)) len = sizeof(raw_values) - 1;
    memcpy(raw_values, equal, len);
    raw_values[len] = '\0';
    trim_inplace(raw_values);

    //printf("a");

    // compute final values_part respecting table refs and operators
    char computed_inner[MAX_ORDER_SCRIPT_LEN] = {0};
    if (!compute_values_part_from_rhs(raw_values, computed_inner, sizeof(computed_inner))) {
        // erreur lors du parsing/eval : retourner 0 (ou laisser inchangé)
        if (error_in_stderr==false){
            printf("Erreur : impossible d'évaluer la partie droite : %s\n", raw_values); fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible d'évaluer la partie droite : %s\n", raw_values); fflush(stderr);
        }
        if (error_lock_program==true) var_script_exit=1;
        return 0;
    }

    snprintf(values_part, sizeof(values_part), "[[%s]]", computed_inner);

    // Sauvegarde en mémoire dans l'environnement local
    if (!save_env_name){
        set_table_env(current_env_object(), table_name, values_part);
    } else {
        set_table_env(save_env_name, table_name, values_part);
    }

    //vérif applique :
    const char* verification_table = get_table_env(current_env_object(),table_name);
    if (!verification_table || strcmp(verification_table, "Table non trouvée.") == 0 || strcmp(verification_table,"Environnement non trouvé.") == 0){
        if (error_in_stderr == false) {
            printf("Erreur : la pile d'environnement ne fonctionne pas, impossible de créer table_local|%s| !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la pile d'environnement ne fonctionne pas, impossible de créer table_local|%s| !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return 0;
    }

    return 1;
}

//fonction pour récupérer le block de l'index la valeur voulu dans table||=[[;]] (utiliser table||![index][index]...)
char* get_table_block_local(const char *recup) {
    static char result[MAX_ORDER_SCRIPT_LEN] = "0.00";

    if (strncmp(recup, "table_local|", 12) != 0) {
        printf("Erreur : la chaîne ne commence pas par 'table|'\n");
        fflush(stdout);
        return result;
    }

    char table_name[MAX_ORDER_SCRIPT_LEN];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep) {
        if (error_in_stderr==false){
            printf("Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    const char *found = get_table_env(current_env_object(), table_name);

    // Extraire les index après le !
    const char *after_bang = sep;
    if (*after_bang == '!') after_bang++;

    int indexes[MAX_ORDER_SCRIPT_LEN], index_count = 0;
    const char *p = after_bang;
    while (*p) {
        if (*p == '[') {
            p++;
            char expr[MAX_ORDER_SCRIPT_LEN] = {0};
            int i = 0;
            int bracket_depth = 1;

            while (*p && bracket_depth > 0) {
                if (*p == '[') bracket_depth++;
                else if (*p == ']') bracket_depth--;
                if (bracket_depth > 0)
                    expr[i++] = *p;
                p++;
            }

            if (bracket_depth != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : crochet non fermé dans table|...|![]\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : crochet non fermé dans table|...|![]\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }

            expr[i] = '\0';
            double dval = condition_script_order(expr);
            int idx = (int)dval - 1;
            indexes[index_count++] = idx;
        } else p++;
    }


    if (!found || !strstr(found, "[[")) {
        if (error_in_stderr==false){
            printf("Erreur : table local '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : table local '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    //si un seul index et que ça valeur est 0 on prend toute la table
    if (index_count == 1 && indexes[0] == -1) {
        char *start_full = strstr(found, "[[");
        if (start_full) {
            strncpy(result, start_full, sizeof(result) - 1);
            result[sizeof(result) - 1] = '\0';

            // Supprimer le \n final s'il y en a un
            size_t len = strlen(result);
            if (len > 0 && result[len - 1] == '\n') {
                result[len - 1] = '\0';
            }
        } else {
            strcpy(result, "0.00");
        }
        return result;
    }

    // Même parsing que get_table_value jusqu'à trouver la bonne case
    char *current = strstr(found, "[[") + 2;
    char list_index_see[MAX_ORDER_SCRIPT_LEN] = ""; //stock les index parcouru pour err
    for (int i = 0; i < index_count; i++) {
        int target_index = indexes[i];
        int current_index = 0;
        int level = 0;
        int in_quotes = 0;
        int paren_depth = 0;

        char *start = current;
        char *scan = start;

        //list_index_see pour le buffer d'err
        char tmp_index_str[32];
        snprintf(tmp_index_str, sizeof(tmp_index_str), "%d", target_index + 1);
        if (i == 0) //pas de . pour le premier
            strncpy(list_index_see, tmp_index_str, sizeof(list_index_see) - 1);
        else {
            strncat(list_index_see, ".", sizeof(list_index_see) - strlen(list_index_see) - 1);
            strncat(list_index_see, tmp_index_str, sizeof(list_index_see) - strlen(list_index_see) - 1);
        }

        // Recherche du bon sous-élément au niveau courant
        while (*scan) {
            if (*scan == '"') {
                in_quotes = !in_quotes;
                scan++;
                continue;
            }

            if (!in_quotes) {
                if (*scan == '(') paren_depth++;
                else if (*scan == ')' && paren_depth > 0) paren_depth--;

                else if (strncmp(scan, "[[", 2) == 0 && paren_depth == 0) {
                    level++;
                    scan += 2;
                    continue;
                }
                else if (strncmp(scan, "]]", 2) == 0 && paren_depth == 0) {
                    if (level > 0) level--;
                    scan += 2;
                    continue;
                }
                else if (*scan == ';' && level == 0 && paren_depth == 0) {
                    if (current_index == target_index) break;
                    current_index++;
                    start = scan + 1;
                }
            }
            scan++;
        }

        // Erreur : index hors limite
        if (current_index != target_index && *scan == '\0') {
            if (target_index+1<=0){
                if (error_in_stderr==false){
                    printf("Erreur : index %s , case %d impossible pour table local '%s' (la valeur minimum d'une case est 1)\n", list_index_see, target_index+1, table_name);
                    fflush(stdout);
                } else {
                    printf("Erreur : index %s , case %d impossible pour table local '%s' (la valeur minimum d'une case est 1)\n", list_index_see, target_index+1, table_name);
                    fflush(stderr);
                }
            } else {
                if (error_in_stderr==false){
                    printf("Erreur : index %s hors limites pour table local '%s'\n", list_index_see, table_name);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index %s hors limites pour table local '%s'\n", list_index_see, table_name);
                    fflush(stderr);
                }
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(result, "0.00");
            return result;
        }

        // Maintenant, on a trouvé l'élément start => on doit en extraire son bloc
        char *end_elem = start;
        int sub_level = 0;
        in_quotes = 0;
        paren_depth = 0;

        while (*end_elem) {
            if (*end_elem == '"') {
                in_quotes = !in_quotes;
                end_elem++;
                continue;
            }

            if (!in_quotes) {
                if (*end_elem == '(') paren_depth++;
                else if (*end_elem == ')' && paren_depth > 0) paren_depth--;

                else if (strncmp(end_elem, "[[", 2) == 0 && paren_depth == 0) {
                    sub_level++;
                    end_elem += 2;
                    continue;
                }
                else if (strncmp(end_elem, "]]", 2) == 0 && paren_depth == 0) {
                    if (sub_level == 0) break;
                    sub_level--;
                    end_elem += 2;
                    continue;
                }
                else if (*end_elem == ';' && sub_level == 0 && paren_depth == 0)
                    break;
            }
            end_elem++;
        }

        // Couper proprement la chaîne du bloc trouvé
        char saved = *end_elem;
        *end_elem = '\0';

        // Copier la portion courante dans result
        strncpy(result, start, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';

        *end_elem = saved;

        // Préparer la descente dans le prochain niveau si besoin
        if (i < index_count - 1) {
            // Si la valeur suivante est un sous-tableau, on avance le pointeur
            char *inner = strstr(result, "[[");
            if (!inner) {
                if (error_in_stderr==false){
                    printf("Erreur : index %s , la case %d de la table local '%s' n’est pas un sous-tableau.\n", list_index_see, target_index + 1, table_name);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index %s , la case %d de la table local '%s' n’est pas un sous-tableau.\n", list_index_see, target_index + 1, table_name);
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }
            current = inner + 2; // aller à l’intérieur du sous-tableau pour le prochain index
        }
    }

    return result;
}

//fonction pour récupérer l'index la valeur voulu dans table||=[[;]] (utiliser table||[index][index]...)
char* get_table_value_local(const char *recup) {
    static char result[MAX_ORDER_SCRIPT_LEN] = "0.00"; //ajout de static si on veut conserver en cas d'érreur la valeur précédente

    if (strncmp(recup, "table_local|", 12) != 0) {
        printf("Erreur : la chaîne ne commence pas par 'table_local|'\n");
        fflush(stdout);
        return result;
    }

    char table_name[MAX_ORDER_SCRIPT_LEN];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep) {
        if (error_in_stderr==false){
            printf("Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : séparateur '|' non trouvé après le nom de la table.\n");
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        return result;
    }

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    char *found = (char*)get_table_env(current_env_object(), table_name);

    // Extraire les index [1][2]...
    int indexes[MAX_ORDER_SCRIPT_LEN], index_count = 0;
    const char *p = sep;
    while (*p) {
        if (*p == '[') {
            p++; // Skip initial '['
            char expr[MAX_ORDER_SCRIPT_LEN] = {0};
            int i = 0;

            int bracket_depth = 1;  // We’ve just passed one '['

            while (*p && bracket_depth > 0) {
                if (*p == '[') bracket_depth++;
                else if (*p == ']') bracket_depth--;

                if (bracket_depth > 0)  // Don't copy closing ']'
                    expr[i++] = *p;

                p++;
            }

            if (bracket_depth != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : crochet fermant manquant dans les indices sur table.\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : crochet fermant manquant dans les indices sur table.\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }

            expr[i] = '\0';

            double dval = condition_script_order(expr);
            int idx = (int)dval-1;

            if (idx < 0) {
                if (error_in_stderr==false){
                    printf("Erreur : index calculé négatif (%d) pour l'expression [%s]\n", idx, expr);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index calculé négatif (%d) pour l'expression [%s]\n", idx, expr);
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }

            indexes[index_count++] = idx;
        } else {
            p++;
        }
    }


    if (!found || !strstr(found, "[[")) {
        if (error_in_stderr==false){
            printf("Erreur : table '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : table '%s' introuvable ou format incorrect.\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

     // Trouver la vraie fin de la table en équilibrant [[ et ]]
    found = strstr(found, "[[") + 2;
    char *table_start = found;
    int open_count = 1;
    int paren__level = 0;  // compteur de parenthèses
    char *ptr = table_start;

    int in_quotes = 0;

    while (*ptr && open_count > 0) {
        if (*ptr == '"') {
            in_quotes = !in_quotes;
            ptr++;
            continue;
        }
        if (!in_quotes) {
            if (*ptr == '(') {
                paren__level++;
                ptr++;
            } else if (*ptr == ')') {
                if (paren__level > 0) paren__level--;
                ptr++;
            } else if (paren__level == 0 && strncmp(ptr, "[[", 2) == 0) {
                // Compter toutes les '[' consécutives
                int count = 0;
                while (ptr[count] == '[') count++;

                int pairs = count / 2;
                int rest  = count % 2;

                open_count += pairs;   // chaque paire "[[" ouvre
                ptr += 2 * pairs;

                if (rest == 1) {
                    // laisser un '[' dans le texte
                    ptr++;
                }

            } else if (paren__level == 0 && strncmp(ptr, "]]", 2) == 0) {
                // Compter toutes les ']' consécutives
                int count = 0;
                while (ptr[count] == ']') count++;

                int pairs = count / 2;
                int rest  = count % 2;

                open_count -= pairs;
                if (open_count < 0) open_count = 0;  // sécurité
                ptr += 2 * pairs;

                if (rest == 1) {
                    // laisser un ']' dans le texte
                    ptr++;
                }

            } else {
                ptr++;
            }
        } else {
            ptr++;
        }
    }

    if (open_count != 0) {
        if (error_in_stderr==false){
            printf("Erreur : la table local '%s' est mal formée (déséquilibre [[ et ]]).\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : la table local '%s' est mal formée (déséquilibre [[ et ]]).\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    ptr[0] = '\0'; // On termine proprement la chaîne

    int paren_level=0; //pour le compteur de parenthèse
    char *current = found;
    for (int i = 0; i < index_count; i++) {
        int level = 0, current_index = 0;
        char *start = current;
        char *scan = start;

        while (*scan) {
    if (strncmp(scan, "[[", 2) == 0) {
        int count = 0;
        while (scan[count] == '[') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        level += pairs;     // chaque paire "[[" augmente le niveau
        scan += 2 * pairs;

        if (rest == 1) {
            // laisser un '[' dans le texte
            scan++;
        }

    } else if (strncmp(scan, "]]", 2) == 0) {
        int count = 0;
        while (scan[count] == ']') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        level -= pairs;
        if (level < 0) level = 0;  // sécurité
        scan += 2 * pairs;

        if (rest == 1) {
            // laisser un ']' dans le texte
            scan++;
        }

    } else if (*scan == ';' && level == 0) {
        if (current_index == indexes[i]) break;
        current_index++;
        start = scan + 1;
        scan++;
    } else if (*scan == '(') {
        // Ignorer les ; dans les parenthèses
        paren_level = 1;
        scan++;
        while (*scan && paren_level > 0) {
            if (*scan == '(') paren_level++;
            else if (*scan == ')') paren_level--;
            scan++;
        }
    } else if (*scan == '"') {
        //Ignorer les " "
        scan++;
        while (*scan && *scan != '"') scan++;
        if (*scan) scan++;
    } else {
        scan++;
    }
}

        if (current_index != indexes[i] && *scan == '\0') {
            if (error_in_stderr==false){
                printf("Erreur : index %d hors limites dans la table local '%s'.\n", indexes[i] + 1, table_name);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : index %d hors limites dans la table local '%s'.\n", indexes[i] + 1, table_name);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(result, "0.00");
            return result;
        }

        // Trouver fin de l'élément
        // Trouver fin de l'élément avec gestion des [[ ]], des restes et des parenthèses
int sub_level = 0;       // niveau des [[ ]]
paren_level = 0;     // niveau des ()
char *end_elem = start;

while (*end_elem) {
    // Gestion des crochets ouvrants [[
    if (strncmp(end_elem, "[[", 2) == 0 && paren_level == 0) {
        int count = 0;
        while (end_elem[count] == '[') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        sub_level += pairs;
        end_elem += 2 * pairs;

        if (rest == 1) {
            // laisser un '[' dans le texte
            end_elem++;
        }
    }
    // Gestion des crochets fermants ]]
    else if (strncmp(end_elem, "]]", 2) == 0 && paren_level == 0) {
        int count = 0;
        while (end_elem[count] == ']') count++;

        int pairs = count / 2;
        int rest  = count % 2;

        if (sub_level > 0) sub_level -= pairs;
        else break;  // fin de l'élément si sub_level == 0

        end_elem += 2 * pairs;
        if (rest == 1) {
            // laisser un ']' dans le texte
            end_elem--;
        }
    }
    // Gestion des points-virgules hors [[ ]] et hors ()
    else if (*end_elem == ';' && sub_level == 0 && paren_level == 0) {
        break;
    }
    // Gestion des parenthèses
    else if (*end_elem == '(') {
        paren_level++;
        end_elem++;
        while (*end_elem && paren_level > 0) {
            if (*end_elem == '(') paren_level++;
            else if (*end_elem == ')') paren_level--;
            end_elem++;
        }
    } else if (*end_elem == '"') {
        //Ignorer les " "
        end_elem++;
        while (*end_elem && *end_elem != '"') end_elem++;
        if (*end_elem) end_elem++;
    }
    else {
        end_elem++;
    }
}

        char saved = *end_elem;
        *end_elem = '\0';
        current = start;
        if (strncmp(current, "[[", 2) == 0) current += 2;
        *end_elem = saved;
    }

    strncpy(result, current, sizeof(result) - 1);
    result[sizeof(result) - 1] = '\0';
    return result;
}

//pour get_table_value_local() et get_table_block_local() remplacer tout les appels dans une chaine table||[][]
char* for_get_table_value_local(char *arg) {
    static char output[MAX_ORDER_SCRIPT_LEN];
    output[0] = '\0';

    //check pour savoir si on laisse les " " dans table||![] (table_local||=)
    bool start_type_is_table = false;
    if(strncmp(arg, "table_local|", 12) == 0) {
        char *end_name = strstr(arg+12,"|");
        if (end_name && strncmp(end_name,"|=",2)==0) start_type_is_table = true;
    }
    //(table||=)
    if(strncmp(arg, "table|", 6) == 0) {
        char *end_name = strstr(arg+6,"|");
        if (end_name && strncmp(end_name,"|=",2)==0) start_type_is_table = true;
    }

    const char *p = arg;
    char *out = output;

    while (*p) {
        const char *match = strstr(p, "table_local|");
        if (!match) {
            strcpy(out, p);
            break;
        }

        // Copier tout avant le match
        while (p < match) *out++ = *p++;
        const char *start = match;

        // Trouver le nom de la table (jusqu’au 2e pipe)
        const char *pipe2 = strchr(start + 12, '|');
        if (!pipe2) {
            strcpy(out, p);
            break;
        }

        const char *after_pipe2 = pipe2 + 1;

        // Nouvelle détection du "!" :
        bool want_block = false;
        if (*after_pipe2 == '!') {
            want_block = true;
            after_pipe2++; // on saute le "!"
        }

        const char *scan = after_pipe2;
        if (*scan != '[') {
            // Pas d’index = ne pas traiter
            while (p <= pipe2) *out++ = *p++;
            continue;
        }

        // Début de parsing des index [..] imbriqués
        const char *expr_end = scan;
        int bracket_depth = 0;

        while (*expr_end) {
            if (*expr_end == '[') {
                bracket_depth++;
                expr_end++;
            } else if (*expr_end == ']') {
                bracket_depth--;
                expr_end++;

                // Si on a fermé tous les crochets, on regarde si un nouveau [ suit
                if (bracket_depth == 0) {
                    const char *test = expr_end;
                    while (*test == ' ' || *test == '\t') test++;
                    if (*test != '[') break;  // pas d'autre bloc
                }
            } else {
                expr_end++;
            }
        }

        if (bracket_depth != 0) {
            if (error_in_stderr==false){
                printf("Erreur : déséquilibre de crochets dans : %s\n", start);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : déséquilibre de crochets dans : %s\n", start);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(out, p);
            return output;
        }

        // Extraire l'expression complète à envoyer à get_table_value
        int len = expr_end - start;
        char full_expr[MAX_ORDER_SCRIPT_LEN] = {0};
        strncpy(full_expr, start, len);
        full_expr[len] = '\0';

        char *val = want_block ? get_table_block_local(full_expr) : get_table_value_local(full_expr);

        //printf("value table : %s\n",val);

        char clean_val[MAX_ORDER_SCRIPT_LEN];
        if (!want_block) {
        // Tronquer val au premier ; trouvé en dehors des () car sinon, la fonction renvoie tout ce qui suit est ne m'intéresse pas
        int paren = 0;
        int i = 0;
        while (val[i]) {
            if (val[i] == '(') paren++;
            else if (val[i] == ')') paren--;
            else if (val[i] == ';' && paren == 0) {
                // remplacer le ; par \0 et arrêter la copie
                clean_val[i] = '\0';
                break;
            } else {
                clean_val[i] = val[i];
            }
            i++;
        }
        clean_val[i] = '\0';

        // Si les deux derniers caractères sont ]] et que la chaîne est assez longue, les enlever
        // Supprimer uniquement les ]] parasites si et seulement si
        // on est vraiment en fin de liste/table (et pas un texte contenant ])
        int len_clean_val = strlen(clean_val);
        if (len_clean_val >= 2) {
            // Vérifier qu’il n’y a rien après dans "val"
            const char *after = clean_val + i;
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;

            if (*after == '\0') {
                // Boucler tant qu'on trouve des "]]" à la fin
                while (len_clean_val >= 2 &&
                    clean_val[len_clean_val - 2] == ']' &&
                    clean_val[len_clean_val - 1] == ']') {
                    clean_val[len_clean_val - 2] = '\0'; // coupe deux caractères
                    len_clean_val -= 2;
                }
            }
        }

        //suppr " " si présent
        len_clean_val = strlen(clean_val);
        if (len_clean_val >= 2 && clean_val[0] == '"' && clean_val[len_clean_val - 1] == '"') {
            // Décaler tout le contenu d’un cran vers la gauche
            memmove(clean_val, clean_val + 1, len_clean_val - 2);
            clean_val[len_clean_val - 2] = '\0';
        }
        } else {
            //si table||![] et que la commande ne commence pas par table||= sinon on laisse les " renvoié
            if (!start_type_is_table) {
                // On nettoie les \" dans val
                const char *src = val;
                char *dst = clean_val;
                while (*src && (dst - clean_val) < (int)sizeof(clean_val) - 1) {
                    // Si \" on suppr
                    if (src[0] == '\"') {
                        //*dst++ = '';
                        src += 1;
                    } else {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';
                //printf("ici: %s\n",clean_val);
            } else {
                // Sinon on garde tel quel
                strncpy(clean_val, val, sizeof(clean_val) - 1);
                clean_val[sizeof(clean_val) - 1] = '\0';
            }
        }

        strcpy(out, clean_val);
        out += strlen(clean_val);
        p = expr_end; // Continuer juste après l'expression traitée
    }

    return output;
}

void table_sea_local(const char *recup) {
    if (!recup || strncmp(recup, "table_local|", 12) != 0) return;

    const char *sea_pos = strstr(recup, "|.sea(");
    if (!sea_pos) return;

    // Extraire le nom de la table
    char table_name[128] = {0};
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep > sea_pos) return;

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Extraire l'attribut entre parenthèses
    const char *start_paren = strchr(sea_pos, '(');
    const char *end_paren = strrchr(sea_pos, ')');
    if (!start_paren || !end_paren || end_paren <= start_paren) return;

    char attr_expr[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(attr_expr, start_paren + 1, end_paren - start_paren - 1);
    attr_expr[end_paren - start_paren - 1] = '\0';

    double result = condition_script_order(attr_expr);
    int add_newline = (result != 1);

    // Récupérer le contenu de la table locale depuis l'environnement
    const char *table_content = get_table_env(current_env_object(), table_name);
    if (!table_content || strcmp(table_content, "Table non trouvée.") == 0 || strcmp(table_content,"Environnement non trouvé.") == 0) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return;
    }

    // Chercher le premier [[
    const char *start_block = strstr(table_content, "[[");
    if (!start_block) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' est mal formée !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' est mal formée !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return;
    }

    // Afficher la table locale telle quelle
    printf("%s", start_block);
    fflush(stdout);
    if (socket_var == 1) {
        send(client, start_block, strlen(start_block), 0);
    }

    if (add_newline) {
        printf("\n");
        fflush(stdout);
    }
}

//fonction pour table_local||.edit([][])(valeur)
int process_table_edit_local(const char *recuperer) {
    if (!recuperer || strncmp(recuperer, "table_local|", 12) != 0) return 0;

    // copie locale sûre
    char recup[MAX_ORDER_SCRIPT_LEN];
    strncpy(recup, recuperer, sizeof(recup) - 1);
    recup[sizeof(recup) - 1] = '\0';

    const char *edit_pos = strstr(recup, "|.edit(");
    if (!edit_pos) return 0;

    //printf("edit : %s\n",recup);

    char table_name[1024];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep != edit_pos) return 0; //pas le même | ce n'est pas table_local||.edit()

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Récupérer les index [x][y] et la valeur (val)
    const char *start_paren = strchr(edit_pos, '(');
    if (!start_paren) return 0;
    start_paren++;

    const char *p = start_paren;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
    }

    if (depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées dans .edit(...)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées dans .edit(...)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    const char *end_indexes = p - 1;

    // Ensuite on a (valeur)
    if (*p != '(') {
        if (error_in_stderr==false){
                printf("Erreur : valeur manquante dans .edit(...)(valeur)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : valeur manquante dans .edit(...)(valeur)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    p++;  // après le '(' ouvrant de la valeur
    const char *val_start = p;
    int val_paren_depth = 1;

    while (*p && val_paren_depth > 0) {
        if (*p == '(') val_paren_depth++;
        else if (*p == ')') val_paren_depth--;
        p++;
    }

    if (val_paren_depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses déséquilibrées dans la valeur à insérer sur table edit.\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses déséquilibrées dans la valeur à insérer sur table edit.\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    char value[MAX_ORDER_SCRIPT_LEN];
    size_t value_len = 0;
    if (p <= val_start) {
        /* paranoia: longueur invalide */
        value[0] = '\0';
    } else {
        ptrdiff_t raw_len = (p - 1) - val_start; // signed
        if (raw_len < 0) raw_len = 0;
        value_len = (size_t) raw_len;
        if (value_len >= sizeof(value)) value_len = sizeof(value) - 1;
        memcpy(value, val_start, value_len);
        value[value_len] = '\0';
    }

    //parsing de value
    char evaluated_value[MAX_ORDER_SCRIPT_LEN] = "";
    size_t len_val = strlen(value);

    // --- TRIM des espaces en début et fin ---
    char *starts = value;
    while (isspace((unsigned char)*starts)) starts++;
    char *ends = value + strlen(value) - 1;
    while (ends > starts && isspace((unsigned char)*ends)) *ends-- = '\0';

    // On copie la partie nettoyée dans une variable temporaire
    char trimmed_value[MAX_ORDER_SCRIPT_LEN];
    strncpy(trimmed_value, starts, sizeof(trimmed_value) - 1);
    trimmed_value[sizeof(trimmed_value) - 1] = '\0';

    // recalcul de la longueur après trim
    len_val = strlen(trimmed_value);

    // Cas 1 : [[ ... ]]
    if (len_val >= 4 && strncmp(trimmed_value, "[[", 2) == 0 && strcmp(trimmed_value + len_val - 2, "]]") == 0) {
        char inner[MAX_ORDER_SCRIPT_LEN];
        strncpy(inner, trimmed_value + 2, len_val - 4);
        inner[len_val - 4] = '\0';

        char *evaluated = evaluate_objects_recursively(inner);
        snprintf(evaluated_value, sizeof(evaluated_value), "[[%s]]", evaluated);
        free(evaluated);
    }
    // Cas 2 : "..." on garde tel quel
    else if (len_val >= 2 && trimmed_value[0] == '"' && trimmed_value[len_val - 1] == '"') {
        strcpy(evaluated_value, trimmed_value);
    }
    // Cas 3 : expression numérique ou logique
    else {
        double res_eval = condition_script_order(trimmed_value);
        regex_format_double(res_eval, evaluated_value, sizeof(evaluated_value));
        replace_comma_with_dot(evaluated_value);
    }

    memset(value,0,sizeof(value));
    strncpy(value,evaluated_value,sizeof(value));
    //fin parsing de value

    /* --- copie sécurisée de la zone d'indexes --- */
    char expr_buf[MAX_ORDER_SCRIPT_LEN];
    size_t len_expr_buf = 0;
    if (end_indexes <= start_paren) {
        expr_buf[0] = '\0';
    } else {
        ptrdiff_t raw_len2 = end_indexes - start_paren;
        if (raw_len2 < 0) raw_len2 = 0;
        len_expr_buf = (size_t) raw_len2;
        if (len_expr_buf >= sizeof(expr_buf)) len_expr_buf = sizeof(expr_buf) - 1;
        memcpy(expr_buf, start_paren, len_expr_buf);
        expr_buf[len_expr_buf] = '\0';
    }

    //printf("edit 4 : %s\n",recup);

    int indexes[MAX_ORDER_SCRIPT_LEN] = {0}, index_count = 0;
    if (!evaluate_indexes(expr_buf, indexes, &index_count)) {
        return 0;
    }

    //printf("edit 5 : %s\n",recup);

    // Charger le contenu de la table locale
    const char* table_inner = get_table_env(current_env_object(),table_name);
    if (!table_inner || strcmp(table_inner, "Table non trouvée.") == 0 || strcmp(table_inner,"Environnement non trouvé.") == 0) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return 0;
    }

    //copie sinon ça modifie la vrai mémoire
    char temp_table[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(temp_table, table_inner, sizeof(temp_table) - 1);
    temp_table[sizeof(temp_table) - 1] = '\0';

    //printf("table 1: %s\n",(char*)table_inner);
    char* start = temp_table;
    if (strncmp(start,"[[",2)==0) start+=2;
    int len_start = strlen(start);
    if (start[len_start-1]==']' && start[len_start-2]==']') {
        start[len_start-2]='\0';
    }
    //printf("table 2: %s\n",start);

    // Modifier la valeur dans la table
    char mod_result[MAX_ORDER_SCRIPT_LEN] = {0};
    if (!replace_at_index(start, indexes, 0, index_count, value, mod_result)) {
        return 0; //echec
    }

    //printf("table 3: %s\n",mod_result);
    char end[MAX_ORDER_SCRIPT_LEN] = {0};
    snprintf(end,sizeof(end),"[[%s]]",mod_result);
    //printf("table 4: %s\n",end);

    //appliqué la nouvelle table
    set_table_env(current_env_object(),table_name,end);
    return 1;
}

// Supprime l’élément visé dans une table_local à partir d’un appel comme table_local|nom|.del([1][2])
int process_table_del_local(const char *recup) {
    if (!recup || strncmp(recup, "table_local|", 12) != 0) return 0;

    const char *del_pos = strstr(recup, "|.del(");
    if (!del_pos) return 0;

    char table_name[1024];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep || sep != del_pos) return 0;  //pas le même | que |.del(

    strncpy(table_name, name_start, sep - name_start);
    table_name[sep - name_start] = '\0';

    // Récupérer les index
    // Trouver le contenu entre les parenthèses de .del(...)
    const char *start_paren = strchr(del_pos, '(');
    if (!start_paren) return 0;
    start_paren++;  // après '('

    const char *p = start_paren;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
    }

    if (depth != 0) {
        if (error_in_stderr==false){
                printf("Erreur : parenthèses non équilibrées dans .del(...)\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : parenthèses non équilibrées dans .del(...)\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    const char *end_paren = p - 1;  // p a avancé d’un caractère après la dernière ')'

    char expr_buf[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(expr_buf, start_paren, end_paren - start_paren - 1);
    expr_buf[end_paren - start_paren - 1] = '\0';

    int indexes[MAX_ORDER_SCRIPT_LEN] = {0}, index_count = 0;
    if (!evaluate_indexes(expr_buf, indexes, &index_count)) {
        //printf("Erreur d’évaluation des indices []\n");
        //fflush(stdout);
        return 0;
    }

    // Charger le contenu de la table locale
    const char* table_inner = get_table_env(current_env_object(),table_name);
    if (!table_inner || strcmp(table_inner, "Table non trouvée.") == 0 || strcmp(table_inner,"Environnement non trouvé.") == 0) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return 0;
    }

    //copie sinon ça modifie la vrai mémoire
    char temp_table[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(temp_table, table_inner, sizeof(temp_table) - 1);
    temp_table[sizeof(temp_table) - 1] = '\0';

    //printf("table 1: %s\n",(char*)table_inner);
    char* start = temp_table;
    if (strncmp(start,"[[",2)==0) start+=2;
    int len_start = strlen(start);
    if (start[len_start-1]==']' && start[len_start-2]==']') {
        start[len_start-2]='\0';
    }
    //printf("table 2: %s\n",start);

    // Modifier la valeur dans la table
    char mod_result[MAX_ORDER_SCRIPT_LEN] = {0};
    if (!remove_at_index(start, indexes, 0, index_count, mod_result)) {
        return 0; //echec
    }

    //printf("table 3: %s\n",mod_result);
    char end[MAX_ORDER_SCRIPT_LEN] = {0};
    snprintf(end,sizeof(end),"[[%s]]",mod_result);
    //printf("table 4: %s\n",end);

    //appliqué la nouvelle table
    set_table_env(current_env_object(),table_name,end);

    return 1;
}

//pour l'ordre table_local||.add()
int add_table_local(const char *input_line) {
    if (!input_line || strncmp(input_line, "table_local|", 12) != 0) return 0;

    const char *add_pos = strstr(input_line, "|.add(");
    if (!add_pos) return 0;

    const char *_check_ = strstr(input_line+12,"|");
    if (!_check_ || add_pos != _check_) return 0; // vérifie que table_local||.add( même |

    // Extraire le nom de la table
    char table_name[1024];
    strncpy(table_name, input_line + 12, add_pos - (input_line + 12));
    table_name[add_pos - (input_line + 12)] = '\0';

    if (strpbrk(table_name, "[]=|{}\"")) {
        if (error_in_stderr==false){
                printf("Erreur : le nom de la table local contient un ou plusieurs caractères interdits : [ ] = { } | \"\n");
                fflush(stdout);
        } else {
                fprintf(stderr,"Erreur : le nom de la table local contient un ou plusieurs caractères interdits : [ ] = { } | \"\n");
                fflush(stderr);
        }
        if (error_lock_program==true){
                var_script_exit=1;
        }
        return 0;
    }

    // Extraire la valeur entre les parenthèses
    const char *val_start = strchr(add_pos, '(');
    if (!val_start) return -2;
    val_start++; // pointer après le (

    int depth = 1;
    const char *val_end = val_start;
    while (*val_end && depth > 0) {
        if (*val_end == '(') depth++;
        else if (*val_end == ')') depth--;
        val_end++;
    }
    val_end-=1;
    val_start-=1;

    // Vérifie que les parenthèses étaient bien équilibrées
    if (depth != 0) return -2;

    char value[MAX_ORDER_SCRIPT_LEN];
    strncpy(value, val_start + 1, val_end - val_start - 1);
    value[val_end - val_start - 1] = '\0';

    //parsing de value
    char evaluated_value[MAX_ORDER_SCRIPT_LEN] = "";
    size_t len_val = strlen(value);

    // --- TRIM des espaces en début et fin ---
    char *start = value;
    while (isspace((unsigned char)*start)) start++;
    char *ends = value + strlen(value) - 1;
    while (ends > start && isspace((unsigned char)*ends)) *ends-- = '\0';

    // On copie la partie nettoyée dans une variable temporaire
    char trimmed_value[MAX_ORDER_SCRIPT_LEN];
    strncpy(trimmed_value, start, sizeof(trimmed_value) - 1);
    trimmed_value[sizeof(trimmed_value) - 1] = '\0';

    // recalcul de la longueur après trim
    len_val = strlen(trimmed_value);

    // Cas 1 : [[ ... ]]
    if (len_val >= 4 && strncmp(trimmed_value, "[[", 2) == 0 && strcmp(trimmed_value + len_val - 2, "]]") == 0) {
        char inner[MAX_ORDER_SCRIPT_LEN];
        strncpy(inner, trimmed_value + 2, len_val - 4);
        inner[len_val - 4] = '\0';

        char *evaluated = evaluate_objects_recursively(inner);
        snprintf(evaluated_value, sizeof(evaluated_value), "[[%s]]", evaluated);
        free(evaluated);
    }
    // Cas 2 : "..." on garde tel quel
    else if (len_val >= 2 && trimmed_value[0] == '"' && trimmed_value[len_val - 1] == '"') {
        strcpy(evaluated_value, trimmed_value);
    }
    // Cas 3 : expression numérique ou logique
    else {
        double res_eval = condition_script_order(trimmed_value);
        regex_format_double(res_eval, evaluated_value, sizeof(evaluated_value));
        replace_comma_with_dot(evaluated_value);
    }

    memset(value,0,sizeof(value));
    strncpy(value,evaluated_value,sizeof(value));
    //fin parsing de value

    // Charger le contenu de la table locale
    const char* table_inner = get_table_env(current_env_object(),table_name);
    if (!table_inner) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return 0;
    }

    char mod_result[MAX_ORDER_SCRIPT_LEN] = {0};

    if (strcmp(table_inner, "Table non trouvée.") == 0 || strcmp(table_inner,"Environnement non trouvé.") == 0) {
        snprintf(mod_result, sizeof(mod_result), "[[%s]]",value);
        //appliqué la nouvelle table
        set_table_env(current_env_object(),table_name,mod_result);

        //vérif applique :
        const char* verification_table = get_table_env(current_env_object(),table_name);
        if (!verification_table || strcmp(verification_table, "Table non trouvée.") == 0 || strcmp(verification_table,"Environnement non trouvé.") == 0){
            if (error_in_stderr == false) {
                printf("Erreur : la pile d'environnement ne fonctionne pas, impossible de créer table_local|%s| !\n", table_name);
                fflush(stdout);
            } else {
                fprintf(stderr, "Erreur : la pile d'environnement ne fonctionne pas, impossible de créer table_local|%s| !\n", table_name);
                fflush(stderr);
            }
            if (error_lock_program == true) var_script_exit = 1;
            return 0;
        }
        return 1;
    }

    //copie sinon ça modifie la vrai mémoire
    char temp_table[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(temp_table, table_inner, sizeof(temp_table) - 1);
    temp_table[sizeof(temp_table) - 1] = '\0';


    if (strncmp(temp_table, "[[]]", 4) == 0 || strncmp(temp_table, "[[ ]]", 5) == 0)
        snprintf(mod_result, sizeof(mod_result), "[[%s]]",value);
    else {
        char* end = temp_table;
        int end_len = strlen(end);
        if (end[end_len-1]==']' && end[end_len-2]==']'){
            end[end_len-2]='\0';
        }
        snprintf(mod_result, sizeof(mod_result), "%s;%s]]", end, value);
    }

    //appliqué la nouvelle table
    set_table_env(current_env_object(),table_name,mod_result);

    return 1;
}

//pour len_table_local (table_local||.len([1])) extrait la table à compter par len_table
char* extract_table_len_local(char *val) {
    static char subtable[MAX_ORDER_SCRIPT_LEN];
    strcpy(subtable, ""); // Reset

    if (strncmp(val, "table_local|", 12) != 0) return NULL;

    // Extraire nom de table
    char *table_start = val + 12;
    char *pipe = strchr(table_start, '|');
    if (!pipe || strncmp(pipe,"|.len(",6) != 0) return NULL;

    char table_name[1024] = {0};
    strncpy(table_name, table_start, pipe - table_start);

    // Charger le contenu de la table locale
    const char* table_inner = get_table_env(current_env_object(),table_name);
    if (!table_inner || strcmp(table_inner, "Table non trouvée.") == 0 || strcmp(table_inner,"Environnement non trouvé.") == 0) {
        if (error_in_stderr == false) {
            printf("Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' n'existe pas !\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program == true) var_script_exit = 1;
        return "0"; //"0" permet de ne pas affiché l'autre msg d'erreur de len_table_local et retourne 0.00
    }

    //copie sinon ça modifie la vrai mémoire
    char temp_table[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(temp_table, table_inner, sizeof(temp_table) - 1);
    temp_table[sizeof(temp_table) - 1] = '\0';


    // Copier la table
    char table_content[MAX_ORDER_SCRIPT_LEN] = {0};
    int level = 0;
    char *src = temp_table; //la table local sur src
    char *dest = table_content;

    int in__quotes = 0;
    while (*src) {
        if (*src=='"'){
            *dest++ = *src++;
            in__quotes = !in__quotes;
            continue;
        }
        if (!in__quotes){
            if (strncmp(src, "[[", 2) == 0) {
            level++;
            *dest++ = *src++;
            *dest++ = *src++;
            } else if (strncmp(src, "]]", 2) == 0) {
                level--;
                *dest++ = *src++;
                *dest++ = *src++;
                if (level == 0) break;
            } else {
                *dest++ = *src++;
            }
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';

    // Extraire indices
    char *len_call = strstr(val, ".len(");
    if (!len_call) return NULL;

    len_call += 5;
    char indices_expr[MAX_ORDER_SCRIPT_LEN] = {0};
    int paren_level = 0;
    char *p = len_call;
    char *q = indices_expr;
    while (*p && (*p != ')' || paren_level > 0)) {
        if (*p == '(') paren_level++;
        if (*p == ')') paren_level--;
        *q++ = *p++;
    }
    *q = '\0';

    int index_list[MAX_ORDER_SCRIPT_LEN] = {0};
    int index_count = 0;

    char *cursor = indices_expr;
    while (*cursor) {
        if (*cursor == '[') {
            cursor++;
            char expr[128] = {0};
            int i = 0;
            while (*cursor && *cursor != ']') {
                expr[i++] = *cursor++;
            }
            expr[i] = '\0';
            if (*cursor != ']') return NULL;
            cursor++;
            index_list[index_count++] = (int)condition_script_order(expr);
        } else {
            cursor++;
        }
    }

    // Plonger récursivement dans la table
    char *current = table_content;
    for (int i = 0; i < index_count; i++) {
        int current_index = 0;
        int desired = index_list[i];
        char *sub = current + 2;  // Skip leading [[
        char element[MAX_ORDER_SCRIPT_LEN] = {0};
        int nesting = 0;

        if (desired <= 0) continue; //sécurité pour index null ou négatif, on ignore

        while (*sub) {
            char *start = sub;
            int len = 0;

            if (strncmp(sub, "[[", 2) == 0) {
                nesting = 1;
                sub += 2;
                int in_quotes = 0;
                while (*sub && nesting) {
                    if (*sub == '"') {
                        in_quotes = !in_quotes;
                        sub++;
                        continue;
                    }
                    if (!in_quotes) {
                        if (strncmp(sub, "[[", 2) == 0) {
                            nesting++;
                            sub += 2;
                        } else if (strncmp(sub, "]]", 2) == 0) {
                            nesting--;
                            sub += 2;
                        } else {
                            sub++;
                        }
                    } else {
                        sub++;
                    }
                }
                len = sub - start;
            } else {
                int in_quotes = 0;
                paren_level = 0;
                // élément simple, attention aux parenthèses
                while (*sub && (paren_level > 0 || in_quotes || (*sub != ';' && strncmp(sub, "]]", 2) != 0))) {
                    if (*sub == '"') in_quotes = !in_quotes;
                    if (*sub == '(') paren_level++;
                    else if (*sub == ')') {
                        if (paren_level > 0) paren_level--;
                    }
                    sub++;
                }
                len = sub - start;
            }

            if (current_index == desired - 1) {
                strncpy(element, start, len);
                element[len] = '\0';

                if (strncmp(element, "[[", 2) == 0 && strncmp(element + strlen(element) - 2, "]]", 2) == 0) {
                    // Valid subtable, set current to new subtable
                    strcpy(subtable, element);
                    current = subtable;
                } else {
                    return "0"; // Not a table
                }
                break;
            }

            if (*sub == ';') sub++;
            current_index++;
        }

        if (current_index != desired - 1) {
            return NULL; // Index hors limite
        }
    }

    // Si aucun index fourni, retourner table de base
    if (index_count == 0) {
        strcpy(subtable, table_content);
    }
    // Si un seul index fourni et qu'il est 0 ou inférieur, retourner table de base
    if (index_count == 1 && index_list[0] <= 0) {
        strcpy(subtable, table_content);
    }

    return subtable;
}

//pour len_table_local (table_local||.len([1]))
char* len_table_local(char *val) {
    static char result[MAX_ORDER_SCRIPT_LEN];
    strcpy(result, "0.00");

    char *table = extract_table_len_local(val);
    if (!table) {
        if (error_in_stderr==false){
            printf("Erreur : %s à échouée.\n",val);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : %s à échouée.\n",val);
            fflush(stderr);
        }
        if (error_lock_program) var_script_exit = 1;
        return result;
    }

    if (strcmp(table,"0")==0){
        return result;
    }

    //printf("table récup : %s",table);

    // Compter les éléments de plus haut niveau dans la table obtenue
    int count = 0;
    char *current = table + 2; // sauter [[
    int level = 0;
    char *p = current;

    while (*p && strncmp(p, "]]", 2) != 0) {
        if (*p=='"'){
            p++;
            while (*p && *p!='"') p++;
            if (*p) p++;
        }
        if (strncmp(p, "[[", 2) == 0) {
            level++;
            p += 2;
            int in_quotes=0;
            while (*p && level) {
                if (*p=='"') {
                    in_quotes = !in_quotes;
                    p++;
                    continue;
                }
                if (!in_quotes){
                    if (strncmp(p, "[[", 2) == 0) {
                        level++;
                        p += 2;
                    } else if (strncmp(p, "]]", 2) == 0) {
                        level--;
                        p += 2;
                    } else {
                        p++;
                    }
                } else {
                    p++;
                }
            }
            count++;
        } else {
            int paren_level = 0;
            int in_quotes = 0;
            while (*p && (paren_level > 0 || in_quotes || (*p != ';' && strncmp(p, "]]", 2) != 0))) {
                if (*p == '"') in_quotes = !in_quotes;
                if (*p == '(') paren_level++;
                else if (*p == ')') {
                    if (paren_level > 0) paren_level--;
                }
                p++;
            }
            count++;
        }

        if (*p == ';') p++;
    }

    snprintf(result, sizeof(result), "%.2f", (double)count);
    return result;
}

//fonction qui gère tout les appels à len_table_local() (table_local||.len())
char* for_len_table_local(char *arg) {
    static char output[MAX_ORDER_SCRIPT_LEN];
    output[0] = '\0';

    const char *p = arg;
    char *out = output;

    while (*p) {
        const char *match = strstr(p, "table_local|");
        if (!match) {
            // Aucun autre match, copier le reste
            strcpy(out, p);
            break;
        }

        // Copier le texte jusqu'au match
        while (p < match) *out++ = *p++;

        const char *table_start = match;
        const char *pipe2 = strchr(table_start + 12, '|');
        if (!pipe2) {
            // Pas de deuxième pipe copier le reste tel quel
            strcpy(out, p);
            break;
        }

        // Vérifier si on a directement ".len(" juste après
        if (strncmp(pipe2 + 1, ".len(", 5) == 0) {
            const char *len_start = pipe2 + 1;
            const char *scan = len_start + 5;
            int paren = 1;
            while (*scan && paren > 0) {
                if (*scan == '(') paren++;
                else if (*scan == ')') paren--;
                scan++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèse non fermée dans .len()\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèse non fermée dans .len()\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(out, p);
                break;
            }

            int expr_len = scan - table_start;
            char full_expr[MAX_ORDER_SCRIPT_LEN] = {0};
            strncpy(full_expr, table_start, expr_len);
            full_expr[expr_len] = '\0';

            // Remplacer par le résultat de len_table_local()
            char *val = len_table_local(full_expr);
            strcpy(out, val);
            out += strlen(val);
            p = scan; // Reprendre juste après l'expression traitée
        } else {
            // Pas un .len() on ne modifie pas, copier normalement
            *out++ = *p++;
        }
    }

    if (*p) {
        strcpy(out, p);
    }
    //*out = '\0';
    return output;
}
