
//dernière version
char *auto_text(const char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;

    char *out = result;
    const char *in = recup;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (strncmp(in, "text(", 5) == 0 || (in > recup && *(in - 1) == '/' && strncmp(in, "text(", 5) == 0)) {
            int is_escaped = (in > recup && *(in - 1) == '/');
            if (autorise_slash_text_num_lock==false) is_escaped=0;

            if (is_escaped) {
                if (i > 0) i--;
            }

            const char *start_text = in - (is_escaped ? 1 : 0);
            in += 5;
            int paren = 1;
            const char *expr_start = in;

            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                //free(result);
                strcpy(result, "0");
                return result;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf_1[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf_1, expr_start, expr_len);
            expr_buf_1[expr_len] = '\0';

            if (is_escaped) {
                int total_len = in - start_text;
                if (i + total_len >= MAX_COMMAND_LEN - 1) break;
                strncpy(out + i, start_text + 1, total_len - 1);
                i += total_len - 1;
            } else {
                char *expr_buf = expr_buf_1;

                //printf("text : %s\n",expr_buf);

                while (*expr_buf && strlen(expr_buf) > 3){
                if (strncmp(expr_buf, "dll|",4) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = process_dll(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;  //ancien : repart à 0 et remodifie des modifs donc pas utile (ou pas voulu)
                    //continue;
                } else {
                if (strncmp(expr_buf,"text(",5) == 0 && var_activ_func_text==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    // Trouver la fin du text(...)
                    const char *start = expr_buf;
                    const char *in = expr_buf + 5; // après "text("
                    int paren_ = 1;
                    while (*in && paren_ > 0) {
                        if (*in == '(') paren_++;
                        else if (*in == ')') paren_--;
                        in++;
                    }
                    if (paren_ != 0) {
                        // Parenthèses déséquilibrées
                        // -> tu peux gérer ton erreur ici
                        expr_buf++;
                        continue;
                    }

                    // Longueur du bloc text(...)
                    int block_len = in - start;  // inclut la parenthèse fermante

                    // Extraire ce bloc
                    char temp_buf[MAX_COMMAND_LEN];
                    strncpy(temp_buf, start, block_len);
                    temp_buf[block_len] = '\0';

                    // Passer uniquement ce bloc à auto_text()
                    char *temp_result = auto_text(temp_buf);
                    if (temp_result) {
                        // Remplacer le bloc par le résultat
                        char new_buf[MAX_COMMAND_LEN];
                        snprintf(new_buf, MAX_COMMAND_LEN, "%s%s",
                        temp_result,     // résultat de text(...)
                        start + block_len); // reste de la chaîne après text(...)

                        memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                        strncpy(expr_buf, new_buf, MAX_COMMAND_LEN);
                        expr_buf[MAX_COMMAND_LEN - 1] = '\0';
                        free(temp_result);

                        if (strcmp(old_buf, expr_buf) == 0) {
                            expr_buf++;
                            continue;
                        }

                        // Avancer expr_buf au premier point de différence
                        char *p_old = old_buf;
                        char *p_new = expr_buf;
                        while (*p_old && *p_new && *p_old != *p_new) {
                            p_old++;
                            p_new++;
                        }
                        expr_buf = p_new;
                        if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                        continue;
                    }
                }

                if (strncmp(expr_buf,"num(",4) == 0 && var_activ_func_num==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, auto_num(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"var_local|",10)== 0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    handle_var_local_get(expr_buf,MAX_COMMAND_LEN);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|", 6) == 0 &&
                    ((strstr(expr_buf, "|=[[") != NULL) ||
                     (strstr(expr_buf, "|=join(") != NULL) ||
                     (strstr(expr_buf, "|=suppr()") != NULL))) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    int results = parse_and_save_table(expr_buf);
                    if (results==4){
                        if (error_in_stderr==false){
                            printf("Erreur: parenthèses non équilibré sur table=[[;]]\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: parenthèses non équilibré sur table=[[;]]\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 3) {
                        if (error_in_stderr==false){
                            printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 2) {
                        if (error_in_stderr==false){
                            printf("Erreur: ]] manquante !.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: ]] manquante !.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results != 1) {
                        if (error_in_stderr==false){
                            printf("Erreur lors de l'enregistrement de la table.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    }
                    strcpy(expr_buf, "0");

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|",6) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, for_get_table_value(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "var|",4) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, handle_variable(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    char *ptr = expr_buf;
                    while (*ptr) {
                        if (*ptr == ',') *ptr = '.';
                        ptr++;
                    }

                    if (strstr(expr_buf, "None") != NULL) {
                        if (error_in_stderr==false){
                            printf("Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stderr);
                        }
                        //if (error_lock_program==true){
                            //var_script_exit=1;
                        //}
                        strcpy(result, "0.00");
                        return result;
                    }

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }
                if (strncmp(expr_buf, "text_replace(\"", 14) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_replace(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_cut(\"",10) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char expr_buf_f[MAX_COMMAND_LEN];
                    strcpy(expr_buf_f, handle_text_cut(expr_buf));
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_letter(\"",13) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_letter(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_uppercase(\"",16) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_uppercase(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_lowercase(\"",16) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_lowercase(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_word(\"",11) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char *expr_buf_f = handle_text_word(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"prompt(",7)==0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);
                    //strcpy(expr_buf,get_prompt(expr_buf));
                    char *expr_buf_f = get_prompt(expr_buf);
                    memset(expr_buf, 0, MAX_COMMAND_LEN); // efface tout le buffer
                    strcpy(expr_buf,expr_buf_f);
                    free(expr_buf_f);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    //size_t old_len = strlen(old_buf);
                    //size_t new_len = strlen(expr_buf);
                    //size_t advance = (new_len > old_len) ? new_len : old_len;
                    //expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    //continue;

                    // avancer expr_buf jusqu'à la portion non modifiée
                    char *p_old = old_buf;
                    char *p_new = expr_buf;
                    while (*p_old && *p_new && *p_old != *p_new) {
                        p_old++;
                        p_new++;
                    }
                    // sécurité : si début identique, avancer au moins d'un caractère
                    expr_buf = p_new;
                    if (p_old != old_buf && expr_buf == p_new) expr_buf++;
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                }
                expr_buf++;
                }

                if (i + strlen(expr_buf_1) >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, expr_buf_1);
                i += strlen(expr_buf_1);
            }

        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}


//ancienne version test

char *auto_text(const char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;

    char *out = result;
    const char *in = recup;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (strncmp(in, "text(", 5) == 0 || (in > recup && *(in - 1) == '/' && strncmp(in, "text(", 5) == 0)) {
            int is_escaped = (in > recup && *(in - 1) == '/');
            if (autorise_slash_text_num_lock==false) is_escaped=0;

            if (is_escaped) {
                if (i > 0) i--;
            }

            const char *start_text = in - (is_escaped ? 1 : 0);
            in += 5;
            int paren = 1;
            const char *expr_start = in;

            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                //free(result);
                strcpy(result, "0");
                return result;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf_1[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf_1, expr_start, expr_len);
            expr_buf_1[expr_len] = '\0';

            if (is_escaped) {
                int total_len = in - start_text;
                if (i + total_len >= MAX_COMMAND_LEN - 1) break;
                strncpy(out + i, start_text + 1, total_len - 1);
                i += total_len - 1;
            } else {
                char *expr_buf = expr_buf_1;
                while (*expr_buf && strlen(expr_buf) > 3){
                if (strncmp(expr_buf, "dll|",4) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, process_dll(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    //nouveau : repartir après la chaine modifier
                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;  //ancien : repart à 0 et remodifie des modifs donc pas utile (ou pas voulu)
                    //continue;
                } else {
                if (strncmp(expr_buf,"text(",5) == 0 && var_activ_func_text==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char temp_buf[MAX_COMMAND_LEN];
                    strncpy(temp_buf, expr_buf, MAX_COMMAND_LEN);
                    temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                    char* temp_result = auto_text(temp_buf);
                    if (temp_result) {
                        strncpy(expr_buf, temp_result, MAX_COMMAND_LEN);
                        expr_buf[MAX_COMMAND_LEN - 1] = '\0';
                        free(temp_result);

                        if (strcmp(old_buf, expr_buf) == 0) {
                            // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                            expr_buf++;
                            continue;
                        }

                        size_t old_len = strlen(old_buf);
                        size_t new_len = strlen(expr_buf);
                        size_t advance = (new_len > old_len) ? new_len : old_len;
                        expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                        continue;

                        //expr_buf = expr_buf_1;
                        //continue;
                    }
                }

                if (strncmp(expr_buf,"num(",4) == 0 && var_activ_func_num==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, auto_num(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"var_local|",10)== 0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    handle_var_local_get(expr_buf,MAX_COMMAND_LEN);

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|", 6) == 0 &&
                    ((strstr(expr_buf, "|=[[") != NULL) ||
                     (strstr(expr_buf, "|=join(") != NULL) ||
                     (strstr(expr_buf, "|=suppr()") != NULL))) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    int results = parse_and_save_table(expr_buf);
                    if (results == 3) {
                        if (error_in_stderr==false){
                            printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 2) {
                        if (error_in_stderr==false){
                            printf("Erreur: ]] manquante !.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: ]] manquante !.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results != 1) {
                        if (error_in_stderr==false){
                            printf("Erreur lors de l'enregistrement de la table.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    }
                    strcpy(expr_buf, "0");

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "table|",6) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, for_get_table_value(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "var|",4) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_variable(expr_buf));

                    char *ptr = expr_buf;
                    while (*ptr) {
                        if (*ptr == ',') *ptr = '.';
                        ptr++;
                    }

                    if (strstr(expr_buf, "None") != NULL) {
                        if (error_in_stderr==false){
                            printf("Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stderr);
                        }
                        //if (error_lock_program==true){
                            //var_script_exit=1;
                        //}
                        strcpy(result, "0.00");
                        return result;
                    }

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }
                if (strncmp(expr_buf, "text_replace(\"", 14) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_text_replace(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf, "text_cut(\"",10) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_text_cut(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                if (strncmp(expr_buf,"prompt(",7)==0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);
                    strcpy(expr_buf,get_prompt(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    size_t old_len = strlen(old_buf);
                    size_t new_len = strlen(expr_buf);
                    size_t advance = (new_len > old_len) ? new_len : old_len;
                    expr_buf += advance;  // avancer expr_buf pour éviter de retraiter cette portion
                    continue;

                    //expr_buf = expr_buf_1;
                    //continue;
                }

                }
                expr_buf++;
                }

                if (i + strlen(expr_buf_1) >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, expr_buf_1);
                i += strlen(expr_buf_1);
            }

        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}


//ancienne version test

char *auto_text(const char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;

    char *out = result;
    const char *in = recup;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (strncmp(in, "text(", 5) == 0 || (in > recup && *(in - 1) == '/' && strncmp(in, "text(", 5) == 0)) {
            int is_escaped = (in > recup && *(in - 1) == '/');
            if (autorise_slash_text_num_lock==false) is_escaped=0;

            if (is_escaped) {
                if (i > 0) i--;
            }

            const char *start_text = in - (is_escaped ? 1 : 0);
            in += 5;
            int paren = 1;
            const char *expr_start = in;

            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                free(result);
                return NULL;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf_1[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf_1, expr_start, expr_len);
            expr_buf_1[expr_len] = '\0';

            if (is_escaped) {
                int total_len = in - start_text;
                if (i + total_len >= MAX_COMMAND_LEN - 1) break;
                strncpy(out + i, start_text + 1, total_len - 1);
                i += total_len - 1;
            } else {
                char *expr_buf = expr_buf_1;
                while (*expr_buf && strlen(expr_buf) > 3){
                if (strncmp(expr_buf, "dll|",4) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, process_dll(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                } else {
                if (strncmp(expr_buf,"text(",5) == 0 && var_activ_func_text==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    char temp_buf[MAX_COMMAND_LEN];
                    strncpy(temp_buf, expr_buf, MAX_COMMAND_LEN);
                    temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                    char* temp_result = auto_text(temp_buf);
                    if (temp_result) {
                        strncpy(expr_buf, temp_result, MAX_COMMAND_LEN);
                        expr_buf[MAX_COMMAND_LEN - 1] = '\0';
                        free(temp_result);

                        if (strcmp(old_buf, expr_buf) == 0) {
                            // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                            expr_buf++;
                            continue;
                        }

                        expr_buf = expr_buf_1;
                        continue;
                    }
                }

                if (strncmp(expr_buf,"num(",4) == 0 && var_activ_func_num==true) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, auto_num(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                if (strncmp(expr_buf,"var_local|",10)== 0){
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    handle_var_local_get(expr_buf,sizeof(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                if (strncmp(expr_buf, "table|", 6) == 0 &&
                    ((strstr(expr_buf, "|=[[") != NULL) ||
                     (strstr(expr_buf, "|=join(") != NULL) ||
                     (strstr(expr_buf, "|=suppr()") != NULL))) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    int results = parse_and_save_table(expr_buf);
                    if (results == 3) {
                        if (error_in_stderr==false){
                            printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 2) {
                        if (error_in_stderr==false){
                            printf("Erreur: ]] manquante !.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: ]] manquante !.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results != 1) {
                        if (error_in_stderr==false){
                            printf("Erreur lors de l'enregistrement de la table.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    }
                    strcpy(expr_buf, "0");

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                if (strncmp(expr_buf, "table|",6) == 0 && strstr(expr_buf, "|[") != NULL) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, for_get_table_value(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                if (strncmp(expr_buf, "var|",4) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_variable(expr_buf));

                    char *ptr = expr_buf;
                    while (*ptr) {
                        if (*ptr == ',') *ptr = '.';
                        ptr++;
                    }

                    if (strstr(expr_buf, "None") != NULL) {
                        if (error_in_stderr==false){
                            printf("Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stderr);
                        }
                        //if (error_lock_program==true){
                            //var_script_exit=1;
                        //}
                        strcpy(result, "0.00");
                        return result;
                    }

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }
                if (strncmp(expr_buf, "text_replace(\"", 14) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_text_replace(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                if (strncmp(expr_buf, "text_cut(\"",10) == 0) {
                    char old_buf[MAX_COMMAND_LEN];
                    strncpy(old_buf, expr_buf, MAX_COMMAND_LEN);

                    strcpy(expr_buf, handle_text_cut(expr_buf));

                    if (strcmp(old_buf, expr_buf) == 0) {
                        // rien n’a changé : avancer d’un caractère pour éviter la boucle infinie
                        expr_buf++;
                        continue;
                    }

                    expr_buf = expr_buf_1;
                    continue;
                }

                }
                expr_buf++;
                }

                if (i + strlen(expr_buf_1) >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, expr_buf_1);
                i += strlen(expr_buf_1);
            }

        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}

//ancienne version

char *auto_text(const char *recup) {
    char *result = malloc(MAX_COMMAND_LEN);
    if (!result) return NULL;

    char *out = result;
    const char *in = recup;
    int i = 0;

    while (*in && i < MAX_COMMAND_LEN - 1) {
        if (strncmp(in, "text(", 5) == 0 || (in > recup && *(in - 1) == '/' && strncmp(in, "text(", 5) == 0)) {
            int is_escaped = (in > recup && *(in - 1) == '/');
            if (autorise_slash_text_num_lock==false) is_escaped=0;

            if (is_escaped) {
                if (i > 0) i--;
            }

            const char *start_text = in - (is_escaped ? 1 : 0);
            in += 5;
            int paren = 1;
            const char *expr_start = in;

            while (*in && paren > 0) {
                if (*in == '(') paren++;
                else if (*in == ')') paren--;
                in++;
            }

            if (paren != 0) {
                if (error_in_stderr==false){
                    printf("Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stdout);
                } else {
                    fprintf(stderr,"Erreur : parenthèses non équilibrées dans text(...)\n");
                    fflush(stderr);
                }
                if (error_lock_program==true){
                    var_script_exit=1;
                }
                free(result);
                return NULL;
            }

            int expr_len = (in - 1) - expr_start;
            char expr_buf[MAX_COMMAND_LEN] = {0};
            strncpy(expr_buf, expr_start, expr_len);
            expr_buf[expr_len] = '\0';

            if (is_escaped) {
                int total_len = in - start_text;
                if (i + total_len >= MAX_COMMAND_LEN - 1) break;
                strncpy(out + i, start_text + 1, total_len - 1);
                i += total_len - 1;
            } else {
                if (strstr(expr_buf, "dll|") != NULL && strstr(expr_buf, "|[") != NULL) {
                    strcpy(expr_buf, process_dll(expr_buf));
                } else {
                if (strstr(expr_buf,"text(") != NULL && var_activ_func_text==true) {
                    char temp_buf[MAX_COMMAND_LEN];
                    strncpy(temp_buf, expr_buf, MAX_COMMAND_LEN);
                    temp_buf[MAX_COMMAND_LEN - 1] = '\0';

                    char* temp_result = auto_text(temp_buf);
                    if (temp_result) {
                        strncpy(expr_buf, temp_result, MAX_COMMAND_LEN);
                        expr_buf[MAX_COMMAND_LEN - 1] = '\0';
                        free(temp_result);
                    }
                }

                if (strstr(expr_buf,"num(") != NULL && var_activ_func_num==true) {
                    strcpy(expr_buf, auto_num(expr_buf));
                }

                if (strstr(expr_buf,"var_local|")!=NULL){
                    handle_var_local_get(expr_buf,sizeof(expr_buf));
                }

                if (strncmp(expr_buf, "table|", 6) == 0 &&
                    ((strstr(expr_buf, "|=[[") != NULL) ||
                     (strstr(expr_buf, "|=join(") != NULL) ||
                     (strstr(expr_buf, "|=suppr()") != NULL))) {

                    int results = parse_and_save_table(expr_buf);
                    if (results == 3) {
                        if (error_in_stderr==false){
                            printf("Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: [[ et ]] interne doivent avoir un ; avant et après ! ([[3;[[6;3]];8]])\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results == 2) {
                        if (error_in_stderr==false){
                            printf("Erreur: ]] manquante !.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur: ]] manquante !.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    } else if (results != 1) {
                        if (error_in_stderr==false){
                            printf("Erreur lors de l'enregistrement de la table.\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur lors de l'enregistrement de la table.\n");
                            fflush(stderr);
                        }
                        if (error_lock_program==true){
                            var_script_exit=1;
                        }
                    }
                    strcpy(expr_buf, "0");
                }

                if (strstr(expr_buf, "table|") != NULL && strstr(expr_buf, "|[") != NULL) {
                    strcpy(expr_buf, for_get_table_value(expr_buf));
                }

                if (strstr(expr_buf, "var|") != NULL) {
                    strcpy(expr_buf, handle_variable(expr_buf));

                    char *ptr = expr_buf;
                    while (*ptr) {
                        if (*ptr == ',') *ptr = '.';
                        ptr++;
                    }

                    if (strstr(expr_buf, "None") != NULL) {
                        if (error_in_stderr==false){
                            printf("Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stdout);
                        } else {
                            fprintf(stderr,"Erreur : définir une variable dans un text() est non conforme et renvoie 0 malgré l'affectation de la variable\n");
                            fflush(stderr);
                        }
                        //if (error_lock_program==true){
                            //var_script_exit=1;
                        //}
                        strcpy(result, "0.00");
                        return result;
                    }
                }
                if (strncmp(expr_buf, "text_replace(\"", 14) == 0) {
                    strcpy(expr_buf, handle_text_replace(expr_buf));
                }

                if (strstr(expr_buf, "text_cut(\"") != NULL) {
                    strcpy(expr_buf, handle_text_cut(expr_buf));
                }

                }

                if (i + strlen(expr_buf) >= MAX_COMMAND_LEN - 1) break;
                strcpy(out + i, expr_buf);
                i += strlen(expr_buf);
            }

        } else {
            result[i++] = *in++;
        }
    }

    result[i] = '\0';
    return result;
}
