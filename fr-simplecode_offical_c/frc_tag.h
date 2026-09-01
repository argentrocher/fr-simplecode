//lib de gestion des tags de l'utilisateur et du système
//toutes les variables systèmes doivent être déclaré avant

//definition des tags systèmes
//le nombre de case fournit maximum
#define SYSTEM_TAG_MAX_KEYS   2
#define SYSTEM_TAG_MAX_STATES 7

typedef struct {
    const char *name;
    int value;
} TagState;

typedef struct {
    size_t number_key; //seul système peut avoir plusieurs clé pour même valeur
    const char *key[SYSTEM_TAG_MAX_KEYS];

    size_t number_states;
    const TagState states[SYSTEM_TAG_MAX_STATES];

    int type_value; //0 = int, 1 = bool
    void *value;
    void *optional_value;  //accès si définit et que la valeur TagState est négative
    //(attention les deux value doivent être du même type, -1 = 0 sur deuxème tampon car remet en positif et soustrait 1)
} SystemTag;

//definition des tags utilisateurs
typedef struct {
    char *key;

    size_t number_states;
    TagState *states;

    int current_value;
} UserTag;

static UserTag *user_tags = NULL;
static size_t user_tags_count = 0;
static size_t user_tags_capacity = 0;

static CRITICAL_SECTION user_tags_lock;

static const SystemTag system_tags[] = {
    {
        2,
        {"sortie","output"},
        5,
        {
            {"classique", 0},
            {"normale",   0},
            {"normal",    0},
            {"tout",      1},
            {"all",       1}
        },
        0,
        &mode_script_output,
        NULL
    },
    {
        2,
        {"sortie_affiche","output_print"},
        7,
        {
            {"pasdevirgule", 1},
            {"notcomma",   1},
            {"virgule",    0},
            {"comma",      0},
            {"[/n]",       -1},
            {"pasde[/n]",   -2},
            {"not[/n]",   -2}
        },
        0,
        &var_affiche_virgule,
        &var_affiche_slash_n
    },
    {
        2,
        {"fonction_num","function_num"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &var_activ_func_num,
        NULL
    },
    {
        2,
        {"fonction_text","function_text"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &var_activ_func_text,
        NULL
    },
    {
        1,
        {"autorise/"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &autorise_slash_text_num_lock,
        NULL
    },
    {
        1,
        {"table_use_script_path"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &use_script_path,
        NULL
    },
    {
        1,
        {"debug_use_script"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &debug_use_script,
        NULL
    },
    {
        1,
        {"dll_arg_analysis"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &dll_arg_analysis,
        NULL
    },
    {
        2,
        {"sortie_erreur","output_error"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &error_in_stderr,
        NULL
    },
    {
        2,
        {"stop_script_erreur","stop_script_error"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &error_lock_program,
        NULL
    },
    {
        2,
        {"voir_avertissement","see_warning"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &warning_error_view,
        NULL
    },
    {
        2,
        {"mode_scientifique","scientific_mode"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &num_scientific_mode,
        NULL
    },
    {
        2,
        {"mode_virgule","comma_mode"},
        4,
        {
            {"oui", 1},
            {"true",   1},
            {"non",    0},
            {"false",      0}
        },
        1,
        &num_type_comma,
        NULL
    },
    {
        1,
        {"global_bool"},
        3,
        {
            {"none", -1},
            {"true",   1},
            {"false",      0}
        },
        0,
        &var_global_bool,
        NULL
    }
};



int get_tag(const char *key, int *value){
    if (!key || !value)
        return 0;

    // 1. Chercher dans les tags système
    for (size_t i = 0; i < (sizeof(system_tags)/sizeof(system_tags[0])); i++) {
        for (size_t j = 0; j < system_tags[i].number_key; j++){
            if (strcmp(system_tags[i].key[j], key) == 0) {
                //on ne renvoie pas la valeur optionel
                if (system_tags[i].type_value == 0) {
                    *value = *(int *)system_tags[i].value;
                } else if (system_tags[i].type_value == 1) {
                    bool tmp = *(bool *)system_tags[i].value;
                    if (tmp)
                        *value = 1;
                    else
                        *value = 0;
                } else
                    return 0; //type inconnu
                return 1;
            }
        }
    }

    // 2. Chercher dans les tags utilisateur
    EnterCriticalSection(&user_tags_lock);

    for (size_t i = 0; i < user_tags_count; i++) {
        if (strcmp(user_tags[i].key, key) == 0) {
            *value = user_tags[i].current_value;
            LeaveCriticalSection(&user_tags_lock);
            return 1;
        }
    }

    LeaveCriticalSection(&user_tags_lock);

    return 0;
}

int set_tag_value(const char *key, const char *state) {
    if (!key || !state)
        return 0;

    // Tags système
    for (size_t i = 0; i < (sizeof(system_tags)/sizeof(system_tags[0])); i++) {
        for (size_t j = 0; j < system_tags[i].number_key; j++) {
            if (strcmp(system_tags[i].key[j], key) != 0)
                continue;

            // Chercher l'état demandé
            for (size_t k = 0; k < system_tags[i].number_states; k++) {

                if (strcmp(system_tags[i].states[k].name, state) != 0)
                    continue;

                int tag_value = system_tags[i].states[k].value;

                //Valeur normale
                if (tag_value >= 0 || !system_tags[i].optional_value) {
                    if (system_tags[i].type_value == 1)
                        *(bool *)system_tags[i].value = tag_value != 0;
                    else
                        *(int *)system_tags[i].value = tag_value;
                }
                // Valeur négative
                else if (system_tags[i].optional_value) {
                    int optional_value = -tag_value - 1;
                    if (system_tags[i].type_value == 1)
                        *(bool *)(system_tags[i].optional_value) = optional_value != 0;
                    else
                        *(int *)(system_tags[i].optional_value) = optional_value;
                }
                return 1;
            }
            //Le tag système existe mais pas cet état.
            return 0;
        }
    }

    // Tags utilisateur
    EnterCriticalSection(&user_tags_lock);

    for (size_t i = 0; i < user_tags_count; i++) {
        if (strcmp(user_tags[i].key, key) != 0)
            continue;
        for (size_t j = 0; j < user_tags[i].number_states; j++) {
            if (strcmp(user_tags[i].states[j].name, state) == 0) {
                user_tags[i].current_value = user_tags[i].states[j].value;

                LeaveCriticalSection(&user_tags_lock);
                return 1;
            }
        }

        LeaveCriticalSection(&user_tags_lock);
        return 0;
    }

    LeaveCriticalSection(&user_tags_lock);
    return 0;
}

int set_tag(const char *key, size_t number_values, const TagState *states) {
    if (!key || !states || number_values == 0)
        return 0;

    EnterCriticalSection(&user_tags_lock);

    // Interdit d'écraser un tag système
    for (size_t i = 0; i < (sizeof(system_tags)/sizeof(system_tags[0])); i++) {
        for (size_t j = 0; j < system_tags[i].number_key; j++) {
            if (strcmp(system_tags[i].key[j], key) == 0) {
                LeaveCriticalSection(&user_tags_lock);
                return 0;
            }
        }
    }

    // Interdit de redéfinir un tag utilisateur
    for (size_t i = 0; i < user_tags_count; i++) {
        if (strcmp(user_tags[i].key, key) == 0) {
            LeaveCriticalSection(&user_tags_lock);
            return 0;
        }
    }

    // Agrandissement du tableau
    if (user_tags_count == user_tags_capacity) {
        size_t new_capacity = user_tags_capacity ? user_tags_capacity * 2 : 16;
        UserTag *new_tags = NULL;
        if (!user_tags_capacity)
            new_tags = malloc(new_capacity * sizeof(UserTag));
        else
            new_tags = realloc(user_tags, new_capacity * sizeof(UserTag));

        if (!new_tags) {
            LeaveCriticalSection(&user_tags_lock);
            return 0;
        }

        user_tags = new_tags;
        user_tags_capacity = new_capacity;
    }

    UserTag *tag = &user_tags[user_tags_count];

    tag->key = strdup(key);

    if (!tag->key) {
        LeaveCriticalSection(&user_tags_lock);
        return 0;
    }

    tag->states = malloc(number_values * sizeof(TagState));

    if (!tag->states) {
        free(tag->key);
        tag->key = NULL;

        LeaveCriticalSection(&user_tags_lock);
        return 0;
    }

    for (size_t i = 0; i < number_values; i++) {
        tag->states[i].name = strdup(states[i].name);
        tag->states[i].value = states[i].value;

        if (!tag->states[i].name) {
            // nettoyage à prévoir
            LeaveCriticalSection(&user_tags_lock);
            return 0;
        }
    }

    tag->number_states = number_values;

    // Par défaut : premier état
    tag->current_value = states[0].value;

    user_tags_count++;

    LeaveCriticalSection(&user_tags_lock);

    return 1;
}

int delete_tag(const char *key){
    if (!key)
        return 0;

    EnterCriticalSection(&user_tags_lock);

    for (size_t i = 0; i < user_tags_count; i++) {

        if (strcmp(user_tags[i].key, key) != 0)
            continue;

        for (size_t j = 0; j < user_tags[i].number_states; j++)
            free((char *)user_tags[i].states[j].name);

        free(user_tags[i].states);
        free(user_tags[i].key);

        if (i + 1 < user_tags_count) {
            memmove(
                &user_tags[i],
                &user_tags[i + 1],
                (user_tags_count - i - 1) * sizeof(UserTag)
            );
        }

        user_tags_count--;

        LeaveCriticalSection(&user_tags_lock);
        return 1;
    }

    LeaveCriticalSection(&user_tags_lock);
    return 0;
}

void clear_user_tags(void){
    EnterCriticalSection(&user_tags_lock);
    if (!user_tags) {
        LeaveCriticalSection(&user_tags_lock);
        return;
    }

    for (size_t i = 0; i < user_tags_count; i++) {

        for (size_t j = 0; j < user_tags[i].number_states; j++)
            free((char *)user_tags[i].states[j].name);

        free(user_tags[i].states);
        free(user_tags[i].key);
    }

    free(user_tags);

    user_tags = NULL;
    user_tags_count = 0;
    user_tags_capacity = 0;

    LeaveCriticalSection(&user_tags_lock);
}




//frc zone

//?canceltag
static int parse_cancel_tag(const char **cursor, const char *end){
    const char *p = *cursor;
    p += 10;

    if (p >= end || *p != '.') {
        error_handler("Erreur : syntaxe invalide sur ?canceltag.nametag{}.", "Error: invalid syntax on ?canceltag.nametag{}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    p++;

    if (is_space(*p)) {
        error_handler("Erreur : syntaxe invalide sur ?canceltag.nametag{}.", "Error: invalid syntax on ?canceltag.nametag{}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    const char *key_start = p;

    //La clé se termine à l'espace, '{' ou fin de ligne.
    while (p < end && *p != '{' && !is_space(*p)) p++;

    size_t key_len = (size_t)(p - key_start);
    if (key_len == 0) {
        error_handler("Erreur : nametag est nul sur ?canceltag.nametag{}.", "Error: nametag is null on ?canceltag.nametag{}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    char * key = malloc(key_len+1);
    if (!key) {
        error_handler__malloc("?canceltag.nametag{}");
        return 0;
    }
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    char *i = key;
    while (*i) {
        if (!(isalpha(*i) || isalnum(*i) || *i == '_')) {
            error_handler("Erreur : nametag est invalide sur ?canceltag.nametag{}.", "Error: nametag is invalid on ?canceltag.nametag{}.", ERROR_MODE_CARRIAGE_RETURN);
            return 0;
        }
        i++;
    }

    //Le {} final est facultatif
    //On va jusqu'à la fin de ligne
    while (p < end && *p != '\n') p++;

    //Si le tag n'existe pas, delete_tag() renvoie 0
    delete_tag(key);

    free(key);

    *cursor = p;

    return 1;
}

static int parse_create_tag(const char **cursor, const char *end) {
    const char *p = *cursor;
    p += 4;

    if (p >= end || *p != '.') {
        error_handler("Erreur : syntaxe invalide sur ?tag.nametag{...}.", "Error: invalid syntax on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    p++;

    if (is_space(*p)) {
        error_handler("Erreur : syntaxe invalide sur ?tag.nametag{...}.", "Error: invalid syntax on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    const char *key_start = p;

    while (p < end && *p != '{' && !is_space(*p)) p++;

    size_t key_len = (size_t)(p - key_start);

    if (key_len == 0){
        error_handler("Erreur : nametag est nul sur ?tag.nametag{...}.", "Error: nametag is null on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    char * key = malloc(key_len+1);
    if (!key) {
        error_handler__malloc("?tag.nametag{...}");
        return 0;
    }
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    char *i = key;
    while (*i) {
        if (!(isalpha(*i) || isalnum(*i) || *i == '_')) {
            error_handler("Erreur : nametag est invalide sur ?canceltag.nametag{}.", "Error: nametag is invalid on ?canceltag.nametag{}.", ERROR_MODE_CARRIAGE_RETURN);
            return 0;
        }
        i++;
    }

    if (p >= end || *p != '{') {
        free(key);
        return 0;
    }

    p++;

    //Allocation temporaire des states
    size_t states_capacity = 8;
    size_t states_count = 0;

    TagState *states = malloc(states_capacity * sizeof(TagState));
    if (!states) {
        free(key);
        error_handler__malloc("?tag.nametag{...}");
        return 0;
    }

    //Lecture des valeurs
    while (p < end) {

        while (p < end && is_space(*p)) p++;

        //Fin du bloc
        if (*p == '}') {
            p++;
            break;
        }

        //Nom de l'état
        const char *name_start = p;

        while (p < end &&
               *p != '=' &&
               *p != '}' &&
               *p != ';') {
            p++;
        }

        const char *name_end = p;

        //Retirer les espaces autour du nom.
        while (name_start < name_end && is_space(*name_start)) name_start++;
        while (name_end > name_start && is_space(name_end[-1])) name_end--;

        size_t name_len = (size_t)(name_end - name_start);

        if (name_len == 0) {
            free(key);
            for (size_t i = 0; i < states_count; i++)
                free((char *)states[i].name);
            free(states);
            error_handler("Erreur : une clé est nulle sur ?tag.nametag{...}.", "Error: one key is null on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
            return 0;
        }

        int int_value = (int)states_count+1;
        if (*p == '=') {
            //value
            p++;
            while (p < end && is_space(*p)) p++;

            if (p >= end || *p == '=') {
                free(key);
                for (size_t i = 0; i < states_count; i++)
                    free((char *)states[i].name);
                free(states);
                error_handler("Erreur : syntaxe invalide sur ?tag.nametag{...}.", "Error: invalid syntax on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
                return 0;
            }

            const char *expr_start = p;

            while (p < end &&
                *p != ';' &&
                *p != '}'){
                p++;
            }

            const char *expr_end = p;

            //Retirer les espaces de fin.
            while (expr_end > expr_start && is_space(expr_end[-1])) expr_end--;

            size_t expr_len = (size_t)(expr_end - expr_start);

            if (expr_len == 0) {
                free(key);
                for (size_t i = 0; i < states_count; i++)
                    free((char *)states[i].name);
                free(states);
                error_handler("Erreur : une valeur est nulle sur ?tag.nametag{...}.", "Error: one value is null on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
                return 0;
            }

            char *expr = malloc(expr_len+1);
            if (!expr) {
                free(key);
                for (size_t i = 0; i < states_count; i++)
                    free((char *)states[i].name);
                free(states);
                error_handler__malloc("?tag.nametag{...}");
                return 0;
            }
            memcpy(expr, expr_start, expr_len);
            expr[expr_len] = '\0';

            //Évaluation math simple
            double result = eval(expr);

            free(expr);

            //Pour l'instant on convertit en int.
            int_value = (int)result;
        }

        //Agrandir le tableau
        if (states_count == states_capacity) {
            size_t new_capacity = states_capacity * 2;

            TagState *new_states = realloc(states, new_capacity * sizeof(TagState));
            if (!new_states) {
                free(key);
                for (size_t i = 0; i < states_count; i++)
                    free((char *)states[i].name);
                free(states);
                error_handler__malloc("?tag.nametag{...}");
                return 0;
            }

            states = new_states;
            states_capacity = new_capacity;
        }

        //Copier le nom
        char *state_name = malloc(name_len+1);
        if (!state_name) {
            free(key);
            for (size_t i = 0; i < states_count; i++)
                free((char *)states[i].name);
            free(states);
            error_handler__malloc("?tag.nametag{...}");
            return 0;
        }

        memcpy(state_name, name_start, name_len);
        state_name[name_len] = '\0';

        states[states_count].name = state_name;
        states[states_count].value = int_value;

        states_count++;

        if (p < end && *p == ';')
            p++;
    }

    //Il faut obligatoirement au moins une valeur.
    if (states_count == 0) {
        free(key);
        free(states);
        *cursor = p;
        error_handler("Erreur : une valeur minimum est requise sur ?tag.nametag{...}.", "Error: one minimum value is required on ?tag.nametag{...}.", ERROR_MODE_CARRIAGE_RETURN);
        return 0;
    }

    //Création du tag
    //printf("tag : %s, %d values\n",key, (int)states_count);
    int result = set_tag(key, states_count, states);

    //set_tag() fait ses propres copies, donc on peut libérer notre tableau temporaire
    for (size_t i = 0; i < states_count; i++)
        free((char *)states[i].name);

    free(states);
    free(key);

    *cursor = p;

    return result;
}

//charge depuis le fichier
int load_tags_from_view(const char *view, size_t view_size){
    if (!view || view_size == 0)
        return 0;

    const char *p = view;
    const char *end = view + view_size;

    int number_tags = 0;

    //On considère le début du fichier comme un début de ligne.
    int line_start = 1;

    while (p < end) {
        if (line_start) {
            const char *q = p;

            //Ignorer les espaces en début de ligne.
            while (q < end && (*q == ' ' || *q == '\t' || *q == '\r')) q++;

            //?tag
            if ((size_t)(end - q) >= 4 && memcmp(q, "?tag", 4) == 0) {
                const char *tmp = q;

                if (parse_create_tag(&tmp, end)) {
                    number_tags++;
                    p = tmp;
                } else {
                    //Si ce n'est pas un tag valide, on continue normalement.
                    p = q + 4;
                }
                line_start = 0;
                continue;
            }

            //?canceltag
            if ((size_t)(end - q) >= 10 && memcmp(q, "?canceltag", 10) == 0) {
                const char *tmp = q;
                if (parse_cancel_tag(&tmp, end)) {
                    number_tags++;
                    p = tmp;
                } else {
                    p = q + 10;
                }
                line_start = 0;
                continue;
            }
        }
        //Gestion du changement de ligne
        if (*p == '\n')
            line_start = 1;
        else
            line_start = 0;
        p++;
    }
    return number_tags;
}


int parse_tag_value(const char *command){
    if (!command)
        return 0;

    const char *prefix = "###";
    const char *suffix = "###";

    size_t len = strlen(command);

    // Minimum : ###a:b###
    if (len < 9)
        return 0;

    if (memcmp(command, prefix, 3) != 0)
        return 0;

    if (memcmp(command + len - 3, suffix, 3) != 0)
        return 0;

    const char *content = command + 3;
    size_t content_len = len - 6;

    //Chercher le ':'
    const char *colon = memchr(
        content,
        ':',
        content_len
    );

    if (!colon) {
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : séparateur ':' introuvable sur tag %s.\n", command);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: separator ':' not found on tag %s.\n", command);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }

    // Le ':' ne doit pas être le premier ou le dernier caractère
    if (colon == content){
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : clé nulle sur tag %s.\n", command);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: key null on tag %s.\n", command);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }
    else if (colon == content + content_len - 1){
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : valeur nulle sur tag %s.\n", command);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: value null on tag %s.\n", command);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
        return 0;
    }

    size_t key_len = (size_t)(colon - content);

    size_t state_len = content_len - key_len - 1;

    char *key = malloc(key_len + 1);
    if (!key) {
        error_handler__malloc("###...:...###");
        return 0;
    }

    char *state = malloc(state_len + 1);
    if (!state) {
        error_handler__malloc("###...:...###");
        free(key);
        return 0;
    }
    memcpy(key, content, key_len);
    key[key_len] = '\0';

    memcpy(state, colon + 1, state_len);
    state[state_len] = '\0';

    int result = set_tag_value(key, state);
    if (!result) {
        char error_msg_fr[2048];
        char error_msg_en[2048];
        snprintf(error_msg_fr, sizeof(error_msg_fr), "Erreur : tag ou etat inconnu de ###%s:%s###.\n", key, state);
        snprintf(error_msg_en, sizeof(error_msg_en), "Error: tag or unknown state of ###%s:%s###.\n", key, state);
        error_handler(error_msg_fr, error_msg_en, ERROR_MODE_DEFAULT);
    }

    free(key);
    free(state);

    return result;
}
