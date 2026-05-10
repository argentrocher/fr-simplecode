//ancien parseur de fonction extrait (remplacement par mappingfile de l'api windows)



                FILE *f = fopen(chemin, "r");
                if (!f) {
                    if (!error_in_stderr) {
                        printf("Erreur : Impossible d'ouvrir '%s'.\n", chemin);
                        fflush(stdout);
                    } else {
                        fprintf(stderr, "Erreur : Impossible d'ouvrir '%s'.\n", chemin);
                        fflush(stderr);
                    }
                    if (error_lock_program) var_script_exit = 1;
                    unlock_file(chemin);
                    free(result);
                    free(temp_buffer);
                    return NULL;
                }

                char *line = malloc(MAX_COMMAND_LEN);
                size_t line_size = MAX_COMMAND_LEN;
                int found = 0;
                long func_pos = 0;
                size_t func_name_len = strlen(func_name);
                while (fgets(line,(line_size), f)) {
                    if (strncmp(line, "?func.", 6) == 0) {
                        char *name_start = line + 6;
                        // Vérifie que le début correspond exactement
                        if (strncmp(name_start, func_name, func_name_len) == 0) {
                            char next = name_start[func_name_len];
                            if (next == '(' || isspace((unsigned char)next)) {
                                found = 1;
                                func_pos = ftell(f) - strlen(line);
                                break;
                            }
                        }
                    }
                }

                if (found) {
                    //pour savoir si on à déjà traité une fonction avec ce même nom qui se trouve dans la pile mais que ce bloc fait plusieurs appels à la même fonction
                    bool is_my_tracked=false;
                    for (int i = 0; i < tracked_count; i++) {
                        if (strcmp(tracked_functions[i], func_name) == 0) is_my_tracked = true; // valeur déjà présente
                    }

                    char func_all[MAX_ORDER_SCRIPT_LEN];
                    if (func_arg) {
                        snprintf(func_all, sizeof(func_all),"%s(%s)",func_name,func_arg);
                    } else {
                        snprintf(func_all, sizeof(func_all),"%s()",func_name);
                    }

                    //si déjà dans la pile de return la réponse à la valeur
                    char *prev_return = find_return(func_all); // utiliser le nom de la fonction comme frame_id
                    if (prev_return && is_my_tracked==false) {
                        //printf("fonction trouvé : %s\n",func_name);
                        // La fonction a déjà été exécutée, utiliser directement sa valeur
                        strncat(result, prev_return, MAX_ORDER_SCRIPT_LEN - strlen(result) - 1);
                        p = q; // avancer après la parenthèse fermante
                        free(line);
                        free(prev_return);
                        //printf("la fonction recherchée est trouvée, on regarde si une autre fonction existe dans ce même block\n");
                        fclose(f);
                        unlock_file(chemin);
                        return_delete_accept=true; //autorisé la suppression car on a récupérer aumoin une fonction
                        if (tracked_count < 100) {
                            strncpy(tracked_functions[tracked_count], func_name, 1023);
                            tracked_functions[tracked_count][1023] = '\0';
                            tracked_count++;
                        }
                        continue;
                    } else if (prev_return && is_my_tracked) {
                        strncat(result, start_name, (q - start_name)); //conservé
                        p = q; // avancer après la parenthèse fermante
                        free(line);
                        free(prev_return);
                        //printf("la fonction recherchée est trouvée, on regarde si une autre fonction existe dans ce même block\n");
                        fclose(f);
                        unlock_file(chemin);
                        return_delete_accept=true; //autorisé la suppression car on a récupérer aumoin une fonction (dont déjà la même)
                        //reculé le pointeur pour réexécuté
                        have_a_new_repetition=true;
                        //printf("ici\n");
                        continue;
                    }

                    // revenir au début de la ligne trouvée
                    fseek(f, func_pos, SEEK_SET);

                    // récupérer args du prototype
                    char func_arg_file[MAX_ORDER_SCRIPT_LEN] = {0};
                    char *func_script_file = malloc(MAX_COMMAND_LEN);
                    if (!func_script_file) { fclose(f); unlock_file(chemin); free(result); free(temp_buffer); free(line); return NULL; }
                    func_script_file[0] = '\0';

                    int depth_proto = 0;
                    int in_args = 0;
                    int in_block = 0;
                    int brace_depth = 0;
                    int in_read_script_block = 0; //passe à 1 quand tout lu pour sortir de la boucle de ligne

                    size_t func_arg_len = 0;
                    size_t func_script_len = 0;

                    while (fgets(line, (line_size), f)) {
                        char *ptr = line;

                        // 1. Récupérer args si pas encore fait
                        if (!in_block) {
                            while (*ptr) {
                                if (*ptr == '(') {
                                    if (depth_proto == 0) in_args = 1;  // début args
                                    depth_proto++;
                                    ptr++;
                                    continue;
                                }
                                if (*ptr == ')') {
                                    depth_proto--;
                                    if (depth_proto == 0 && in_args) {
                                        in_args = 0;
                                        ptr++; // avancer après la parenthèse fermante
                                        break; // on passe à la recherche du bloc [[[ sur cette ligne
                                    }
                                }
                                if (in_args && depth_proto > 0) {
                                    //size_t len = strlen(func_arg_file);
                                    if (func_arg_len < sizeof(func_arg_file)-1) {
                                        func_arg_file[func_arg_len++] = *ptr;
                                    }
                                }
                                ptr++;
                            }

                            // 2. Chercher le bloc [[[ sur le reste de la ligne
                            while (*ptr && isspace((unsigned char)*ptr)) ptr++; // skip espaces
                            if (strncmp(ptr, "[[[", 3) == 0) {
                                in_block = 1;
                                ptr += 3; // début du bloc
                            }
                        }

                        // 3. Capturer le script
                        if (in_block) {
                            // ajouter toute la ligne restante
                            while (*ptr) {
                                // vérifier fin bloc ]]] seulement si depth {} = 0
                                if (strncmp(ptr, "]]]", 3) == 0 && brace_depth == 0) {
                                    in_block = 0;
                                    in_read_script_block = 1; //arrêté la lecture du fichier
                                    break;
                                }
                                // compter accolades imbriquées
                                if (*ptr == '{') brace_depth++;
                                else if (*ptr == '}') {
                                    if (brace_depth > 0) brace_depth--;
                                }

                                // ajouter caractère au script
                                //size_t len_script = strlen(func_script_file);
                                if (func_script_len < MAX_COMMAND_LEN - 1) {
                                    func_script_file[func_script_len++] = *ptr;
                                }
                                ptr++;
                            }

                            // si bloc toujours ouvert, ajouter retour à la ligne
                            if (in_block) {
                                //size_t len_script = strlen(func_script_file);
                                if (func_script_len < MAX_COMMAND_LEN - 1) {
                                    func_script_file[func_script_len++] = '\n';
                                }
                            }
                        }
                        if (in_read_script_block){
                            break; //arrêté la lecture du fichier pour ne pas lire les args d'autres fonction
                        }
                    }

                    func_arg_file[func_arg_len] = '\0';
                    func_script_file[func_script_len] = '\0';

                    free(line);
                    fclose(f);
                    unlock_file(chemin);
