//matrice_table_frc.h pour la gestion des tables en table||=**... (calcul matriciel)

//necessite frc_environnement.h
//necessite frc_table_local.h

#ifndef MATRIX_GROW_PER_CELL
#define MATRIX_GROW_PER_CELL 100
#endif

/* ----------------- util resize ----------------- */
static int ensure_capacity(char **buf, size_t *cap, size_t needed) {
    if (*cap >= needed) return 1;
    size_t newcap = (*cap == 0) ? needed : *cap;
    while (newcap < needed) newcap *= 2;
    char *n = realloc(*buf, newcap);
    if (!n) return 0;
    *buf = n;
    *cap = newcap;
    return 1;
}

static int ensure_capacity_ptrs(char ***buf, size_t *cap, size_t needed)
{
    if (needed <= *cap) return 1;
    size_t newcap = (*cap == 0) ? 8 : *cap * 2;
    if (newcap < needed) newcap = needed;

    char **newbuf = realloc(*buf, newcap * sizeof(char *));
    if (!newbuf) return 0;

    *buf = newbuf;
    *cap = newcap;
    return 1;
}

// ===========================================================
// MODE MATRICE : "**" au début de l’expression
// ===========================================================

static int split_top_level_semicolons_deep_dyn(const char *s, char ***out_tokens) {
    if (!s || !out_tokens) return -1;
    *out_tokens = NULL;

    int in_quotes = 0;
    int bracket_level = 0;
    int paren_level = 0;

    const char *p = s;
    const char *token_start = p;
    int token_count = 0;
    size_t array_cap = 0;
    char **tokens = NULL;

    while (*p) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            p++;
            continue;
        }
        if (!in_quotes && strncmp(p, "[[", 2) == 0) {
            bracket_level++;
            p += 2;
            continue;
        }
        if (!in_quotes && strncmp(p, "]]", 2) == 0) {
            if (bracket_level > 0) bracket_level--;
            p += 2;
            continue;
        }
        if (!in_quotes && *p == '(') {
            paren_level++;
            p++;
            continue;
        }
        if (!in_quotes && *p == ')') {
            if (paren_level > 0) paren_level--;
            p++;
            continue;
        }

        /* top-level separator ; */
        if (!in_quotes && bracket_level == 0 && paren_level == 0 && *p == ';') {
            size_t len = p - token_start;
            /* trim both ends manually */
            const char *a = token_start;
            const char *b = p - 1;
            while (a <= b && isspace((unsigned char)*a)) a++;
            while (b >= a && isspace((unsigned char)*b)) b--;
            size_t clean_len = (b >= a) ? (size_t)(b - a + 1) : 0;

            /* allocate entry */
            char *tok = malloc(clean_len + 1);
            if (!tok) { /* cleanup */ for (int i=0;i<token_count;i++) free(tokens[i]); free(tokens); return -1; }
            if (clean_len) memcpy(tok, a, clean_len);
            tok[clean_len] = '\0';

            /* append to tokens array */
            if (!ensure_capacity_ptrs(&tokens, &array_cap, token_count + 1)) {
                free(tok); for (int i=0;i<token_count;i++) free(tokens[i]); free(tokens); return -1;
            }
            tokens[token_count++] = tok;

            /* advance after ; */
            p++;
            token_start = p;
            continue;
        }

        p++;
    }

    /* Last token (from token_start to end) */
    const char *end = p - 1;
    const char *a = token_start;
    const char *b = end;
    while (a <= b && isspace((unsigned char)*a)) a++;
    while (b >= a && isspace((unsigned char)*b)) b--;
    size_t clean_len = (b >= a) ? (size_t)(b - a + 1) : 0;

    if (clean_len > 0) {
        char *tok = malloc(clean_len + 1);
        if (!tok) { for (int i=0;i<token_count;i++) free(tokens[i]); free(tokens); return -1; }
        memcpy(tok, a, clean_len);
        tok[clean_len] = '\0';
        if (!ensure_capacity_ptrs(&tokens, &array_cap, token_count + 1)) {
            free(tok); for (int i=0;i<token_count;i++) free(tokens[i]); free(tokens); return -1;
        }
        tokens[token_count++] = tok;
    }

    /* If there were zero tokens and string empty, return 0 with tokens==NULL */
    *out_tokens = tokens;
    return token_count;
}

/* free helper */
static void free_token_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; ++i) {
        free(arr[i]);
    }
    free(arr);
}

static void skip_spaces(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static int compute_values_matrix_mode(const char *expr, char *result_out, size_t outsz);

// ===========================================================
// ÉVALUATION D’UNE OPÉRATION MATRICE CASE-PAR-CASE
// ===========================================================

static char *eval_matrix_operation_ex(
    const char *left,
    const char *right,
    char op,
    int depth
) {
    if (depth > 32) {
        char *err = malloc(5);
        if (!err) return NULL;
        strcpy(err, "0.00");
        return err;
    }

    if (!left) left = "0.00";
    if (!right) right = "0.00";

    char lcopy_small[MAX_ORDER_SCRIPT_LEN], rcopy_small[MAX_ORDER_SCRIPT_LEN];
    strncpy(lcopy_small, left, sizeof(lcopy_small)-1); lcopy_small[sizeof(lcopy_small)-1] = '\0';
    strncpy(rcopy_small, right, sizeof(rcopy_small)-1); rcopy_small[sizeof(rcopy_small)-1] = '\0';
    trim_inplace(lcopy_small);
    trim_inplace(rcopy_small);

    int left_is_table = (strncmp(lcopy_small, "[[", 2) == 0
                         && strlen(lcopy_small) >= 4
                         && strcmp(lcopy_small + strlen(lcopy_small) - 2, "]]") == 0);
    int right_is_table = (strncmp(rcopy_small, "[[", 2) == 0
                          && strlen(rcopy_small) >= 4
                          && strcmp(rcopy_small + strlen(rcopy_small) - 2, "]]") == 0);

    /* SCALAIRES */
    if (!left_is_table && !right_is_table) {
        // Détection si ce sont des textes entre guillemets
        int left_is_text  = (lcopy_small[0] == '"' && lcopy_small[strlen(lcopy_small) - 1] == '"');
        int right_is_text = (rcopy_small[0] == '"' && rcopy_small[strlen(rcopy_small) - 1] == '"');

        // Si un des deux est du texte  priorité au gauche
        if (left_is_text || right_is_text) {
            // On renvoie toujours le texte de gauche, sans guillemets doubles externes
            const char *chosen = left_is_text ? lcopy_small : rcopy_small;
            return strdup(chosen);
        }

        char *pl = (char*)left;
        char *pr = (char*)right;
        double lv = condition_script_order(pl);//parse_expression_matrix(&pl);
        double rv = condition_script_order(pr);//parse_expression_matrix(&pr);
        double result = 0.0;
        switch (op) {
            case '+': result = lv + rv; break;
            case '-': result = lv - rv; break;
            case '*': result = lv * rv; break;
            case '/': result = (rv != 0.0) ? lv / rv : 0.0; break;
            case '%': result = fmod(lv, rv); break;
            case '#': result = (rv != 0.0) ? floor(lv / rv) : 0.0; break;
            case '^': result = pow(lv, rv); break;
            default: result = lv + rv; break;
        }
        char tmp[128];
        regex_format_double(result, tmp, sizeof(tmp));
        replace_comma_with_dot(tmp);
        return strdup(tmp);
    }

    /* TABLES : découpage dynamique */
    char **l_tokens = NULL;
    char **r_tokens = NULL;
    int l_count = 0, r_count = 0;

    if (left_is_table) {
        /* extract inner part (between [[ and ]]) */
        size_t len = strlen(lcopy_small);
        if (len < 4) return strdup("0.00");
        size_t inner_len = len - 4 + 1;
        char *inner = malloc(inner_len);
        if (!inner) return strdup("0.00");
        strncpy(inner, lcopy_small + 2, inner_len - 1);
        inner[inner_len - 1] = '\0';
        l_count = split_top_level_semicolons_deep_dyn(inner, &l_tokens);
        free(inner);
        if (l_count < 0) { free_token_array(l_tokens, (l_count>0?l_count:0)); return strdup("0.00"); }
    } else {
        l_count = 1;
        l_tokens = malloc(sizeof(char *));
        if (!l_tokens) return strdup("0.00");
        l_tokens[0] = strdup(lcopy_small);
        if (!l_tokens[0]) { free(l_tokens); return strdup("0.00"); }
    }

    if (right_is_table) {
        size_t len = strlen(rcopy_small);
        if (len < 4) { free_token_array(l_tokens, l_count); return strdup("0.00"); }
        size_t inner_len = len - 4 + 1;
        char *inner = malloc(inner_len);
        if (!inner) { free_token_array(l_tokens, l_count); return strdup("0.00"); }
        strncpy(inner, rcopy_small + 2, inner_len - 1);
        inner[inner_len - 1] = '\0';
        r_count = split_top_level_semicolons_deep_dyn(inner, &r_tokens);
        free(inner);
        if (r_count < 0) { free_token_array(l_tokens, l_count); free_token_array(r_tokens, (r_count>0?r_count:0)); return strdup("0.00"); }
    } else {
        r_count = 1;
        r_tokens = malloc(sizeof(char *));
        if (!r_tokens) { free_token_array(l_tokens, l_count); return strdup("0.00"); }
        r_tokens[0] = strdup(rcopy_small);
        if (!r_tokens[0]) { free_token_array(l_tokens, l_count); free(r_tokens); return strdup("0.00"); }
    }

    /* Estimation de la taille du résultat (adaptative) */
    int max_count = (l_count > r_count) ? l_count : r_count;
    size_t est_len = strlen(left) + strlen(right) + (size_t)max_count * MATRIX_GROW_PER_CELL + 64;
    char *out = malloc(est_len);
    if (!out) { free_token_array(l_tokens, l_count); free_token_array(r_tokens, r_count); return strdup("0.00"); }
    size_t cap = est_len;
    size_t cur_len = 0;
    out[0] = '\0';

    /* ouvrir table */
    if (!ensure_capacity(&out, &cap, cur_len + 3)) { free(out); free_token_array(l_tokens, l_count); free_token_array(r_tokens, r_count); return strdup("0.00"); }
    memcpy(out + cur_len, "[[", 2); cur_len += 2; out[cur_len] = '\0';

    /* parcourir cases */
    for (int i = 0; i < max_count; ++i) {
        const char *lt = l_tokens[i % l_count];
        const char *rt = r_tokens[i % r_count];

        /* recursivement eval (retourne strdup) */
        char *sub = eval_matrix_operation_ex(lt, rt, op, depth + 1);
        if (!sub) {
            free(out);
            free_token_array(l_tokens, l_count);
            free_token_array(r_tokens, r_count);
            return strdup("0.00");
        }

        size_t addlen = strlen(sub);
        /* ajouter ; si nécessaire */
        if (i > 0) {
            if (!ensure_capacity(&out, &cap, cur_len + 1 + addlen + 1)) { free(sub); free(out); free_token_array(l_tokens, l_count); free_token_array(r_tokens, r_count); return strdup("0.00"); }
            out[cur_len++] = ';';
            out[cur_len] = '\0';
        } else {
            if (!ensure_capacity(&out, &cap, cur_len + addlen + 1)) { free(sub); free(out); free_token_array(l_tokens, l_count); free_token_array(r_tokens, r_count); return strdup("0.00"); }
        }

        memcpy(out + cur_len, sub, addlen);
        cur_len += addlen;
        out[cur_len] = '\0';
        free(sub);
    }

    /* fermer table */
    if (!ensure_capacity(&out, &cap, cur_len + 3)) { free(out); free_token_array(l_tokens, l_count); free_token_array(r_tokens, r_count); return strdup("0.00"); }
    memcpy(out + cur_len, "]]", 2); cur_len += 2; out[cur_len] = '\0';

    /* cleanup tokens */
    free_token_array(l_tokens, l_count);
    free_token_array(r_tokens, r_count);

    /* shrink */
    char *shr = realloc(out, cur_len + 1);
    if (shr) out = shr;

    return out;
}

/* wrapper qui copie le résultat dans buffer fourni (sécurité) */
static void eval_matrix_operation(
    const char *left,
    const char *right,
    char op,
    char *outbuf,
    size_t outsz
) {
    if (!outbuf || outsz == 0) return;
    char *res = eval_matrix_operation_ex(left, right, op, 0);
    if (!res) {
        strncpy(outbuf, "0.00", outsz-1);
        outbuf[outsz-1] = '\0';
        return;
    }
    strncpy(outbuf, res, outsz-1);
    outbuf[outsz-1] = '\0';
    free(res);
}

// ===========================================================
// FONCTION PRINCIPALE DU MODE MATRICE
// ===========================================================

static char *eval_matrix_expression(const char **p, int depth);
static char *eval_matrix_term(const char **p, int depth);
static char *eval_matrix_factor(const char **p, int depth);


// ------------------------------------------------------------
// FACTOR — gère parenthèses et puissance
// ------------------------------------------------------------
static char *eval_matrix_factor(const char **p, int depth) {
    skip_spaces(p);

    // Parenthèses prioritaires
    if (**p == '(') {
        (*p)++;
        char *inside = eval_matrix_expression(p, depth + 1);
        skip_spaces(p);
        if (**p == ')') (*p)++;
        return inside;
    }

    // Table ou nombre
    const char *start = *p;
    int bracket_level = 0;
    while (**p) {
        if (strncmp(*p, "[[", 2) == 0) { bracket_level++; (*p) += 2; continue; }
        if (strncmp(*p, "]]", 2) == 0) { bracket_level--; (*p) += 2; if (bracket_level <= 0) break; continue; }
        if (bracket_level == 0 && strchr("+-*/%^#()", **p)) break;
        (*p)++;
    }

    size_t len = *p - start;
    char *content = malloc(len + 1);
    strncpy(content, start, len);
    content[len] = '\0';
    trim_inplace(content);

    return content; // renvoie une table, un nombre ou une sous-expression
}

// ------------------------------------------------------------
// POWER — gère ^
// ------------------------------------------------------------
static char *eval_matrix_power(const char **p, int depth) {
    char *left = eval_matrix_factor(p, depth + 1);
    skip_spaces(p);

    while (**p == '^') {
        char op = **p;
        (*p)++;
        char *right = eval_matrix_factor(p, depth + 1);

        char *res = eval_matrix_operation_ex(left, right, op, depth + 1);

        free(left);
        free(right);
        left = res;
        skip_spaces(p);
    }
    return left;
}

// ------------------------------------------------------------
// TERM — gère *, /, %, #
// ------------------------------------------------------------
static char *eval_matrix_term(const char **p, int depth) {
    char *left = eval_matrix_power(p, depth + 1);
    skip_spaces(p);

    while (**p == '*' || **p == '/' || **p == '%' || **p == '#') {
        char op = **p;
        (*p)++;
        char *right = eval_matrix_power(p, depth + 1);

        char *res = eval_matrix_operation_ex(left, right, op, depth + 1);

        free(left);
        free(right);
        left = res;
        skip_spaces(p);
    }
    return left;
}

// ------------------------------------------------------------
// EXPRESSION — gère +, -
// ------------------------------------------------------------
static char *eval_matrix_expression(const char **p, int depth) {
    char *left = eval_matrix_term(p, depth + 1);
    skip_spaces(p);

    while (**p == '+' || **p == '-') {
        char op = **p;
        (*p)++;
        char *right = eval_matrix_term(p, depth + 1);

        char *res = eval_matrix_operation_ex(left, right, op, depth + 1);

        free(left);
        free(right);
        left = res;
        skip_spaces(p);
    }
    return left;
}

// ------------------------------------------------------------
// * IMPLICITE MULTIPLICATION
// ------------------------------------------------------------
static char *insert_hidden_multiplications_matrix(const char *expr) {
    if (!expr) return NULL;
    size_t len = strlen(expr);
    size_t cap = len + 64; // marge initiale
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t out_len = 0;

    int bracket_level = 0;
    int paren_level = 0;
    int in_quotes = 0;

    for (size_t i = 0; i < len; i++) {
        char c = expr[i];

        // Gestion guillemets
        if (c == '"') {
            in_quotes = !in_quotes;
            if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = c;
            continue;
        }

        // Gestion [[ ]]
        if (!in_quotes && strncmp(expr + i, "[[", 2) == 0) {
            if (bracket_level == 0 && paren_level == 0 && out_len > 0) {
                char prev = out[out_len - 1];
                if (prev != '*' && prev != '+' && prev != '-' && prev != '/' && prev != '%' && prev != '#' && prev != '^' && prev != '(') {
                    if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[out_len++] = '*';
                }
            }
            bracket_level++;
            if (out_len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = '['; out[out_len++] = '[';
            i++;
            continue;
        }

        if (!in_quotes && strncmp(expr + i, "]]", 2) == 0) {
            bracket_level--;
            if (out_len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = ']'; out[out_len++] = ']';
            i++;

            // Vérifier si juste après ]] il y a [[ ou ( pour ajouter '*'
            if (i + 1 < len && bracket_level == 0 && paren_level == 0) {
                char next = expr[i + 1];
                if (next == '[' && strncmp(expr + i + 1, "[[", 2) == 0) {
                    if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[out_len++] = '*';
                } else if (next == '(') {
                    if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[out_len++] = '*';
                }
            }

            continue;
        }

        // Gestion ()
        if (!in_quotes && c == '(') {
            if (bracket_level == 0 && paren_level == 0 && out_len > 0) {
                char prev = out[out_len - 1];
                if (prev != '*' && prev != '+' && prev != '-' && prev != '/' && prev != '%' && prev != '#' && prev != '^' && prev != '(') {
                    if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[out_len++] = '*';
                }
            }
            paren_level++;
            if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = '(';
            continue;
        }

        if (!in_quotes && c == ')') {
            paren_level--;
            if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[out_len++] = ')';

            // Vérifier si juste après ) il y a [[ ou ( pour ajouter '*'
            if (i + 1 < len && bracket_level == 0 && paren_level == 0) {
                char next = expr[i + 1];
                if (next == '(' || (next == '[' && strncmp(expr + i + 1, "[[", 2) == 0)) {
                    if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                    out[out_len++] = '*';
                }
            }

            continue;
        }

        // Copier le caractère normal
        if (out_len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
        out[out_len++] = c;
    }

    // Terminer la chaîne
    if (out_len >= cap) { cap = out_len + 1; out = realloc(out, cap); }
    out[out_len] = '\0';
    return out;
}

// ------------------------------------------------------------
// RECUP TABLE|| IN FILE
// ------------------------------------------------------------
// Préprocesseur : remplace les séquences table|nom| par le contenu de la table lue
static char *expand_table_references_matrix(const char *expr) {
    if (!expr) return NULL;

    size_t len = strlen(expr);
    size_t cap = len + 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    const char *p = expr;
    size_t out_len = 0;

    while (*p) {
        const char *pos_table = strstr(p, "table|");
        const char *pos_local = strstr(p, "table_local|");
        const char *pos;

        int is_local = 0;
        if (pos_table && pos_local) {
            if (pos_table < pos_local) {
                pos = pos_table;
            } else {
                pos = pos_local;
                is_local = 1;
            }
        } else if (pos_table) {
            pos = pos_table;
        } else if (pos_local) {
            pos = pos_local;
            is_local = 1;
        } else {
            pos = NULL;
        }

        if (!pos) {
            // Plus de table| ou table_local| -> copier le reste
            size_t chunk = strlen(p);
            if (out_len + chunk + 1 >= cap) {
                cap = out_len + chunk + 64;
                out = realloc(out, cap);
            }
            strcpy(out + out_len, p);
            out_len += chunk;
            break;
        }

        // Copier avant "table|" ou "table_local|"
        size_t prefix_len = pos - p;
        if (out_len + prefix_len + 1 >= cap) {
            cap = out_len + prefix_len + 64;
            out = realloc(out, cap);
        }
        memcpy(out + out_len, p, prefix_len);
        out_len += prefix_len;
        out[out_len] = '\0';

        // Avancer après le préfixe
        const char *after_prefix = pos + (is_local ? 12 : 6);
        const char *end = strchr(after_prefix, '|');
        if (!end) {
            // pas de fermeture -> copier le reste brut
            size_t rest = strlen(after_prefix);
            if (out_len + rest + 1 >= cap) {
                cap = out_len + rest + 64;
                out = realloc(out, cap);
            }
            memcpy(out + out_len, after_prefix, rest);
            out_len += rest;
            out[out_len] = '\0';
            break;
        }

        // Nom de table
        size_t name_len = end - after_prefix;
        if (name_len == 0 || name_len > 255) {
            p = end + 1;
            continue;
        }
        char table_name[256];
        strncpy(table_name, after_prefix, name_len);
        table_name[name_len] = '\0';
        trim_inplace(table_name);

        // Vérifier le caractère juste après le dernier '|'
        const char *after = end + 1;
        int skip_replace = 0;
        if (*after == '!' || *after == '.') {
            skip_replace = 1;
        } else if (*after == '[' && strncmp(after, "[[", 2) != 0) {
            skip_replace = 1;
        }

        if (!skip_replace) {
            char table_content[MAX_ORDER_SCRIPT_LEN];
            int found = 0;
            if (is_local) {
                found = read_table_inner_from_local(table_name, table_content, sizeof(table_content));
            } else {
                found = read_table_inner_from_file(table_name, table_content, sizeof(table_content));
            }

            if (found) {
                size_t add_len = strlen(table_content) + 4;
                if (out_len + add_len + 1 >= cap) {
                    cap = out_len + add_len + 64;
                    out = realloc(out, cap);
                }
                int written = sprintf(out + out_len, "[[%s]]", table_content);
                if (written < 0) written = 0;
                out_len += written;
                p = end + 1;
                continue;
            }
        }

        // Pas trouvé -> recopier "table|nom|" ou "table_local|nom|"
        size_t raw_len = (end + 1) - (p);
        if (out_len + raw_len + 1 >= cap) {
            cap = out_len + raw_len + 64;
            out = realloc(out, cap);
        }
        memcpy(out + out_len, p, raw_len);
        out_len += raw_len;
        out[out_len] = '\0';
        p = end + 1;
    }

    return out;
}

// ------------------------------------------------------------
// POINT D’ENTRÉE PRINCIPAL
// ------------------------------------------------------------
static int compute_values_matrix_mode(const char *expr, char *result_out, size_t outsz) {
    if (!expr || !result_out) return 0;

    const char *p = expr;
    skip_spaces(&p);

    const char *pp = expand_table_references_matrix(p);
    if (!pp){
        strncpy(result_out, "[[0.00]]", outsz - 1);
        result_out[outsz - 1] = '\0';
        return 0;
    }

    //printf("reponse: %s\n",(char *)pp);

    const char *ppp = insert_hidden_multiplications_matrix(pp);
    if (!ppp){
        strncpy(result_out, "[[0.00]]", outsz - 1);
        result_out[outsz - 1] = '\0';
        return 0;
    }

    char *res = eval_matrix_expression(&ppp, 0);

    if (!res) {
        strncpy(result_out, "[[0.00]]", outsz - 1);
        result_out[outsz - 1] = '\0';
        return 0;
    }

    //! de  base imbriqué avec [[ ]] sauf erreur ou 0.00 donc on englobe
    char current[outsz];
    strncpy(current,res,outsz-1);
    current[outsz - 1] = '\0';
    free(res);
    char * correct_current = current;
    int correct_current_len = strlen(current);
    if (correct_current[0]=='[' && correct_current[1]=='[' && correct_current[correct_current_len-1]==']' && correct_current[correct_current_len-2]==']'){
        correct_current[correct_current_len-1]='\0';
        correct_current[correct_current_len-2]='\0';
        correct_current+=2;
    }
    strncpy(result_out, correct_current, outsz - 1);
    result_out[outsz - 1] = '\0';
    return 1;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// unused !
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static int compute_values_matrix_mode2(const char *expr, char *result_out, size_t outsz) {
    if (!expr || !result_out) return 0;
    char working[MAX_ORDER_SCRIPT_LEN];
    strncpy(working, expr, sizeof(working) - 1);
    working[sizeof(working) - 1] = '\0';
    trim_inplace(working);

    // Priorité : ^, puis */%#, puis +-
    const char *operators[] = {"^", "*/%#", "+-"};
    char current[MAX_ORDER_SCRIPT_LEN];
    strncpy(current, working, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';

    for (int level = 0; level < 3; level++) {
        const char *p = current;
        int in_quotes = 0, bracket_level = 0, paren_level = 0;
        const char *op_pos = NULL;
        char op = 0;

        while (*p) {
            if (*p == '"') in_quotes = !in_quotes;
            else if (!in_quotes && strncmp(p, "[[", 2) == 0) { bracket_level++; p++; }
            else if (!in_quotes && strncmp(p, "]]", 2) == 0) { if (bracket_level > 0) bracket_level--; p++; }
            else if (!in_quotes && *p == '(') paren_level++;
            else if (!in_quotes && *p == ')') paren_level--;
            else if (!in_quotes && bracket_level == 0 && paren_level == 0 && strchr(operators[level], *p)) {
                op = *p;
                op_pos = p;
                break;
            }
            p++;
        }

        if (op_pos) {
            char left[MAX_ORDER_SCRIPT_LEN], right[MAX_ORDER_SCRIPT_LEN];
            size_t left_len = op_pos - current;
            strncpy(left, current, left_len);
            left[left_len] = '\0';
            strncpy(right, op_pos + 1, sizeof(right) - 1);
            right[sizeof(right) - 1] = '\0';
            trim_inplace(left);
            trim_inplace(right);

            char out[MAX_ORDER_SCRIPT_LEN];
            eval_matrix_operation(left, right, op, out, sizeof(out));

            //printf("left %s, right %s, op %c, out %s\n",left,right,op,out);

            char *correct_out = out;
            int correct_out_len = strlen(out);
            if (correct_out[0]=='[' && correct_out[1]=='[' && correct_out[correct_out_len-1]==']' && correct_out[correct_out_len-2]==']'){
                correct_out[correct_out_len-1]='\0';
                correct_out[correct_out_len-2]='\0';
                correct_out+=2;
                //printf("correct out %s\n",correct_out);
            }

            strncpy(result_out, correct_out, outsz - 1);
            result_out[outsz - 1] = '\0';
            return 1;
        }
    }

    // aucun opérateur top-level  évaluer numériquement ou garder la table telle quelle
    if (strncmp(current, "[[", 2) == 0){
        char* correct_current = current;
        int correct_current_len = strlen(correct_current);
        if (correct_current[0]=='[' && correct_current[1]=='[' && correct_current[correct_current_len-1]==']' && correct_current[correct_current_len-2]==']'){
            correct_current[correct_current_len-1]='\0';
            correct_current[correct_current_len-2]='\0';
            correct_current+=2;
        }
        strncpy(result_out, correct_current, outsz - 1);
    } else {
        double v = condition_script_order(current);
        char tmp[64];
        regex_format_double(v, tmp, sizeof(tmp));
        replace_comma_with_dot(tmp);
        snprintf(result_out, outsz, "%s", tmp);
    }

    return 1;
}
