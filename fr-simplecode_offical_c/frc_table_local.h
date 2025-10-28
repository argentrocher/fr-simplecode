//table local process
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//necessite frc_environnement.h

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//zone fonction pour compute_values_part_from_rhs()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static int read_table_inner_from_local(const char *nom_table, char *out, size_t outsz) {
    if (!nom_table || !out) return 0;

    const char *env_name = current_env_object();
    const char *table_val = get_table_env(env_name, nom_table);

    if (!table_val || strcmp(table_val, "Table non trouv�e.") == 0 || strcmp(table_val,"Environnement non trouv�.") == 0)
        return 0;

    const char *p = table_val;
    const char *start = strstr(p, "[[");
    const char *end = NULL;

    if (!start) return 0;
    start += 2; // avance apr�s [[

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
    if (!t || t != s) return 0; // doit commencer exactement � s

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

    // Supprimer la table de l�environnement
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

    // V�rifier si c�est ++ ou --
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

    // R�cup�rer le contenu actuel de la table
    const char *current_val = get_table_env(env_name, table_name);
    if (!current_val || strcmp(current_val, "Table non trouv�e.") == 0 || strcmp(current_val,"Environnement non trouv�.") == 0) {
        // Si la table n�existe pas encore
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
        // Ajouter un nouvel �l�ment ;[[valeur]]
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
        // Supprimer le dernier �l�ment
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
int parse_and_save_table_local(const char *input_line) {
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
            printf("Erreur : le nom de la table contient un caract�re interdit : [ ] = { } \"\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : le nom de la table contient un caract�re interdit : [ ] = { } \"\n");
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
        // erreur lors du parsing/eval : retourner 0 (ou laisser inchang�)
        if (error_in_stderr==false){
            printf("Erreur : impossible d'�valuer la partie droite : %s\n", raw_values); fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : impossible d'�valuer la partie droite : %s\n", raw_values); fflush(stderr);
        }
        if (error_lock_program==true) var_script_exit=1;
        return 0;
    }

    snprintf(values_part, sizeof(values_part), "[[%s]]", computed_inner);

    // Sauvegarde en m�moire dans l'environnement local
    set_table_env(current_env_object(), table_name, values_part);

    return 1;
}

//fonction pour r�cup�rer le block de l'index la valeur voulu dans table||=[[;]] (utiliser table||![index][index]...)
char* get_table_block_local(const char *recup) {
    static char result[MAX_ORDER_SCRIPT_LEN] = "0.00";

    if (strncmp(recup, "table_local|", 12) != 0) {
        printf("Erreur : la cha�ne ne commence pas par 'table|'\n");
        fflush(stdout);
        return result;
    }

    char table_name[MAX_ORDER_SCRIPT_LEN];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep) {
        if (error_in_stderr==false){
            printf("Erreur : s�parateur '|' non trouv� apr�s le nom de la table.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : s�parateur '|' non trouv� apr�s le nom de la table.\n");
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

    // Extraire les index apr�s le !
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
                    printf("Erreur : crochet non ferm� dans table|...|![]\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : crochet non ferm� dans table|...|![]\n");
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

    //si un seul index et que �a valeur est 0 on prend toute la table
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

    // M�me parsing que get_table_value jusqu'� trouver la bonne case
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

        // Recherche du bon sous-�l�ment au niveau courant
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

        // Maintenant, on a trouv� l'�l�ment start => on doit en extraire son bloc
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

        // Couper proprement la cha�ne du bloc trouv�
        char saved = *end_elem;
        *end_elem = '\0';

        // Copier la portion courante dans result
        strncpy(result, start, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';

        *end_elem = saved;

        // Pr�parer la descente dans le prochain niveau si besoin
        if (i < index_count - 1) {
            // Si la valeur suivante est un sous-tableau, on avance le pointeur
            char *inner = strstr(result, "[[");
            if (!inner) {
                if (error_in_stderr==false){
                    printf("Erreur : index %s , la case %d de la table local '%s' n�est pas un sous-tableau.\n", list_index_see, target_index + 1, table_name);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index %s , la case %d de la table local '%s' n�est pas un sous-tableau.\n", list_index_see, target_index + 1, table_name);
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                strcpy(result, "0.00");
                return result;
            }
            current = inner + 2; // aller � l�int�rieur du sous-tableau pour le prochain index
        }
    }

    return result;
}

//fonction pour r�cup�rer l'index la valeur voulu dans table||=[[;]] (utiliser table||[index][index]...)
char* get_table_value_local(const char *recup) {
    static char result[MAX_ORDER_SCRIPT_LEN] = "0.00"; //ajout de static si on veut conserver en cas d'�rreur la valeur pr�c�dente

    if (strncmp(recup, "table_local|", 12) != 0) {
        printf("Erreur : la cha�ne ne commence pas par 'table_local|'\n");
        fflush(stdout);
        return result;
    }

    char table_name[MAX_ORDER_SCRIPT_LEN];
    const char *name_start = recup + 12;
    const char *sep = strchr(name_start, '|');
    if (!sep) {
        if (error_in_stderr==false){
            printf("Erreur : s�parateur '|' non trouv� apr�s le nom de la table.\n");
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : s�parateur '|' non trouv� apr�s le nom de la table.\n");
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

            int bracket_depth = 1;  // We�ve just passed one '['

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
                    printf("Erreur : index calcul� n�gatif (%d) pour l'expression [%s]\n", idx, expr);
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : index calcul� n�gatif (%d) pour l'expression [%s]\n", idx, expr);
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

     // Trouver la vraie fin de la table en �quilibrant [[ et ]]
    found = strstr(found, "[[") + 2;
    char *table_start = found;
    int open_count = 1;
    int paren__level = 0;  // compteur de parenth�ses
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
                // Compter toutes les '[' cons�cutives
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
                // Compter toutes les ']' cons�cutives
                int count = 0;
                while (ptr[count] == ']') count++;

                int pairs = count / 2;
                int rest  = count % 2;

                open_count -= pairs;
                if (open_count < 0) open_count = 0;  // s�curit�
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
            printf("Erreur : la table local '%s' est mal form�e (d�s�quilibre [[ et ]]).\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr,"Erreur : la table local '%s' est mal form�e (d�s�quilibre [[ et ]]).\n", table_name);
            fflush(stderr);
        }
        if (error_lock_program==true){
            var_script_exit=1;
        }
        strcpy(result, "0.00");
        return result;
    }

    ptr[0] = '\0'; // On termine proprement la cha�ne

    int paren_level=0; //pour le compteur de parenth�se
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
        if (level < 0) level = 0;  // s�curit�
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
        // Ignorer les ; dans les parenth�ses
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

        // Trouver fin de l'�l�ment
        // Trouver fin de l'�l�ment avec gestion des [[ ]], des restes et des parenth�ses
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
        else break;  // fin de l'�l�ment si sub_level == 0

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
    // Gestion des parenth�ses
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

        // Trouver le nom de la table (jusqu�au 2e pipe)
        const char *pipe2 = strchr(start + 12, '|');
        if (!pipe2) {
            strcpy(out, p);
            break;
        }

        const char *after_pipe2 = pipe2 + 1;

        // Nouvelle d�tection du "!" :
        bool want_block = false;
        if (*after_pipe2 == '!') {
            want_block = true;
            after_pipe2++; // on saute le "!"
        }

        const char *scan = after_pipe2;
        if (*scan != '[') {
            // Pas d�index = ne pas traiter
            while (p <= pipe2) *out++ = *p++;
            continue;
        }

        // D�but de parsing des index [..] imbriqu�s
        const char *expr_end = scan;
        int bracket_depth = 0;

        while (*expr_end) {
            if (*expr_end == '[') {
                bracket_depth++;
                expr_end++;
            } else if (*expr_end == ']') {
                bracket_depth--;
                expr_end++;

                // Si on a ferm� tous les crochets, on regarde si un nouveau [ suit
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
                printf("Erreur : d�s�quilibre de crochets dans : %s\n", start);
                fflush(stdout);
            } else {
                fprintf(stderr,"Erreur : d�s�quilibre de crochets dans : %s\n", start);
                fflush(stderr);
            }
            if (error_lock_program==true){
                var_script_exit=1;
            }
            strcpy(out, p);
            return output;
        }

        // Extraire l'expression compl�te � envoyer � get_table_value
        int len = expr_end - start;
        char full_expr[MAX_ORDER_SCRIPT_LEN] = {0};
        strncpy(full_expr, start, len);
        full_expr[len] = '\0';

        char *val = want_block ? get_table_block_local(full_expr) : get_table_value_local(full_expr);

        //printf("value table : %s\n",val);

        char clean_val[MAX_ORDER_SCRIPT_LEN];
        if (!want_block) {
        // Tronquer val au premier ; trouv� en dehors des () car sinon, la fonction renvoie tout ce qui suit est ne m'int�resse pas
        int paren = 0;
        int i = 0;
        while (val[i]) {
            if (val[i] == '(') paren++;
            else if (val[i] == ')') paren--;
            else if (val[i] == ';' && paren == 0) {
                // remplacer le ; par \0 et arr�ter la copie
                clean_val[i] = '\0';
                break;
            } else {
                clean_val[i] = val[i];
            }
            i++;
        }
        clean_val[i] = '\0';

        // Si les deux derniers caract�res sont ]] et que la cha�ne est assez longue, les enlever
        // Supprimer uniquement les ]] parasites si et seulement si
        // on est vraiment en fin de liste/table (et pas un texte contenant ])
        int len_clean_val = strlen(clean_val);
        if (len_clean_val >= 2) {
            // V�rifier qu�il n�y a rien apr�s dans "val"
            const char *after = clean_val + i;
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;

            if (*after == '\0') {
                // Boucler tant qu'on trouve des "]]" � la fin
                while (len_clean_val >= 2 &&
                    clean_val[len_clean_val - 2] == ']' &&
                    clean_val[len_clean_val - 1] == ']') {
                    clean_val[len_clean_val - 2] = '\0'; // coupe deux caract�res
                    len_clean_val -= 2;
                }
            }
        }

        //suppr " " si pr�sent
        len_clean_val = strlen(clean_val);
        if (len_clean_val >= 2 && clean_val[0] == '"' && clean_val[len_clean_val - 1] == '"') {
            // D�caler tout le contenu d�un cran vers la gauche
            memmove(clean_val, clean_val + 1, len_clean_val - 2);
            clean_val[len_clean_val - 2] = '\0';
        }
        } else {
            //si table||![] et que la commande ne commence pas par table||= sinon on laisse les " renvoi�
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
        p = expr_end; // Continuer juste apr�s l'expression trait�e
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

    // Extraire l'attribut entre parenth�ses
    const char *start_paren = strchr(sea_pos, '(');
    const char *end_paren = strrchr(sea_pos, ')');
    if (!start_paren || !end_paren || end_paren <= start_paren) return;

    char attr_expr[MAX_ORDER_SCRIPT_LEN] = {0};
    strncpy(attr_expr, start_paren + 1, end_paren - start_paren - 1);
    attr_expr[end_paren - start_paren - 1] = '\0';

    double result = condition_script_order(attr_expr);
    int add_newline = (result != 1);

    // R�cup�rer le contenu de la table locale depuis l'environnement
    const char *table_content = get_table_env(current_env_object(), table_name);
    if (!table_content || strcmp(table_content, "Table non trouv�e.") == 0 || strcmp(table_content,"Environnement non trouv�.") == 0) {
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
            printf("Erreur : la table locale '%s' est mal form�e !\n", table_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "Erreur : la table locale '%s' est mal form�e !\n", table_name);
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
