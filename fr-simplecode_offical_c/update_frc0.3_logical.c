
//latest

    //pour compté les ]] à la fin pour savoir si ils sont présent en bon nombre pair ou impair pour savoir si on doit ajouté un ] final
    char save_found[MAX_COMMAND_LEN];
    strncpy(save_found, found, MAX_COMMAND_LEN - 1);
    save_found[MAX_COMMAND_LEN - 1] = '\0';


    // Rechercher current dans found current pour vérifier le nombre de ] final pour s'assurer de ne pas loupé le dernier si impaire
    //printf("%s",save_found);
    char *pos = strstr(save_found, current);
    //désactivé (marche mais pas supper car si deux fois la même chose risque d'erreur)
    if (1==0) { //remettre 'pos' pour activé
        char *scan = pos;
        int paren_level_ = 0;

        // Parcourir jusqu'au ; hors parenthèses ou fin de chaîne
        while (*scan) {
            if (*scan == '(') paren_level_++;
            else if (*scan == ')') paren_level_--;
            else if (*scan == ';' && paren_level_ == 0) break;
            scan++;
        }

        // Calculer la longueur de la portion à copier dans current
        int len = scan - pos;
        if (len >= MAX_COMMAND_LEN) len = MAX_COMMAND_LEN - 1;

        // Copier dans current et terminer par '\0'
        strncpy(current, pos, len);
        current[len] = '\0';

        // Enlever les \n et \r éventuels au début et à la fin
        // Début
        while (*current == '\n' || *current == '\r') memmove(current, current + 1, strlen(current));
        // Fin
        while (len > 0 && (current[len-1] == '\n' || current[len-1] == '\r')) {
            current[len-1] = '\0';
            len--;
        }
    }





//update
// Trouver fin de l'élément 
        int sub_level = 0; 
        char *end_elem = start; 
        while (*end_elem) { 
            if (strncmp(end_elem, "[[", 2) == 0) { sub_level++; end_elem += 2; } 
            else if (strncmp(end_elem, "]]", 2) == 0) { 
                if (sub_level > 0) sub_level--; 
                else break; 
                end_elem += 2; 
                int count = 0; 
                while (end_elem[count] == ']') count++; 
                int rest = count % 2; 
                if (rest == 1) { // laisser un ']' dans la valeur -> on recule d’un 
                    end_elem--; 
                } 
            } else if (*end_elem == ';' && sub_level == 0) { 
                break; 
            } else { 
                end_elem++; 
            } 
        }
