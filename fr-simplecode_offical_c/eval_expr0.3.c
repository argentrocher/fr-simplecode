double eval_expr(const char *expr, int *index) {
    double value = parse_term(expr, index); // Évaluer le premier terme (gère la multiplication et la division) (+modulo)

    while (expr[*index] != '\0') {
        if (expr[*index] == '+' || expr[*index] == '-') {
            char op = expr[*index];
            (*index)++; // Passer à l'opérateur suivant

            double next_value = parse_term(expr, index); // Récupérer la valeur suivante

            // Appliquer l'opérateur à la valeur
            if (op == '+') {
                value += next_value;
            } else if (op == '-') {
                value -= next_value;
            }
        }
        // Gérer la multiplication et la division
        else if (expr[*index] == '*' || expr[*index] == '/' || expr[*index] == '%' || expr[*index] == '#' || expr[*index] == '^') {
            char op = expr[*index];
            (*index)++; // Passer à l'opérateur suivant

            double next_value = parse_term(expr, index); // Récupérer la valeur suivante

            // Appliquer l'opérateur à la valeur
            if (op == '*') {
                value *= next_value;
            } else if (op == '/') {
                if (next_value == 0) {
                    if (error_in_stderr==false){
                        printf("Erreur : division par zéro\n");
                        fflush(stdout);
                    } else {
                        fprintf(stderr,"Erreur : division par zéro\n");
                        fflush(stderr);
                    }
                    if (error_lock_program==true){
                        var_script_exit=1;
                    }
                    return 0;  // Retourner une valeur par défaut en cas d'erreur
                }
                value /= next_value;
            }
        }
        // Vérifier si une parenthèse ouvrante est présente
        else if (expr[*index] == '(') {
            (*index)++; // Passer la parenthèse ouvrante

            char prev_op = (expr[*index - 2] == '+' || expr[*index - 2] == '-') ? expr[*index - 2] : '+';
            double parenthesis_value = eval_expr(expr, index); // Appel récursif pour traiter l'expression à l'intérieur des parenthèses

            // Appliquer l'opérateur avant la parenthèse à la valeur de la parenthèse
            if (prev_op == '+') {
                value += parenthesis_value;
            } else if (prev_op == '-') {
                value -= parenthesis_value;
            }
        }
        // Vérifier si une parenthèse fermante est présente
        else if (expr[*index] == ')') {
            (*index)++; // Passer la parenthèse fermante
            break; // Sortir de la boucle une fois qu'on a traité la parenthèse
        }
        else {
            break; // Sortir de la boucle si un caractère invalide est trouvé
        }
    }

    return value;
}
