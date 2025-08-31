// --------- Parseur booléen pour if( ... ) avec format ||...|| --------- (shunting-yard)

typedef enum {
    TK_VAL,        // valeur ||...||
    TK_UN_NOT,     // !||...||
    TK_UN_Q,       // ?||...||
    TK_OP,         // opérateur binaire (==, !=, <, >, <=, >=, ?=, AND, OR)
    TK_LPAREN,     // (
    TK_RPAREN      // )
} TokType;

typedef struct {
    TokType type;
    char    text[64];  // pour opérateurs / debugging car le type est défini par toktype (montre les vrai charactères)
    char    payload[1024]; // pour VAL / UN_NOT / UN_Q : contenu entre || || (les condition_script_order)
} Tok;

static int is_space(char c){ return c==' ' || c=='\t' || c=='\n' || c=='\r'; }

static int starts_with_ci(const char *s, const char *kw){
    size_t n = strlen(kw);
    for (size_t i=0;i<n;i++){
        char a = s[i], b = kw[i];
        if (!a) return 0;
        if (a>='A'&&a<='Z') a = (char)(a-'A'+'a');
        if (b>='A'&&b<='Z') b = (char)(b-'A'+'a');
        if (a!=b) return 0;
    }
    return 1;
}

// Tokenizer du segment [cond_start, cond_end)
static int tokenize_if_paren(const char *beg, const char *end, Tok out[], int max_out){
    int nt = 0;
    const char *p = beg;
    while (p < end){
        // espaces
        if (is_space(*p)) { p++; continue; }

        // parenthèses
        if (*p == '('){ if (nt<max_out) out[nt++] = (Tok){TK_LPAREN,"(",""}; p++; continue; }
        if (*p == ')'){ if (nt<max_out) out[nt++] = (Tok){TK_RPAREN,")",""}; p++; continue; }

        // opérateurs mots-clés and/or (et/ou)
        if (starts_with_ci(p,"and")){
            if (nt<max_out) out[nt++] = (Tok){TK_OP,"&&",""};
            p += 3; continue;
        }
        if (starts_with_ci(p,"or")){
            if (nt<max_out) out[nt++] = (Tok){TK_OP,"||",""};
            p += 2; continue;
        }
        if (starts_with_ci(p,"et")){
            if (nt<max_out) out[nt++] = (Tok){TK_OP,"&&",""};
            p += 2; continue;
        }
        if (starts_with_ci(p,"ou")){
            if (nt<max_out) out[nt++] = (Tok){TK_OP,"||",""};
            p += 2; continue;
        }

        // unaire !||...||  ou  ?||...||
        if ((*p=='!' || *p=='?') && (p+2<end) && p[1]=='|' && p[2]=='|'){
            char kind = *p; p += 3; // saute "!||" ou "?||"
            const char *val_start = p;
            const char *q = p;
            while (q+1 < end) {
                if (q[0]=='|' && q[1]=='|') {
                    const char *next = q+2;
                    // vérifier si fin valide: fin chaîne ou espace suivi d'op ou )
                    if (next >= end ||
                        is_space(*next) ||
                        *next == ')' ||
                        starts_with_ci(next,"and") ||
                        starts_with_ci(next,"or") ||
                        starts_with_ci(next,"et") ||
                        starts_with_ci(next,"ou")
                    ) break;
                }
                q++;
            }
            if (q+1>=end) return -1; // erreur : "||" fermant manquant
            size_t len = (size_t)(q - val_start);
            Tok t = (Tok){ kind=='!'?TK_UN_NOT:TK_UN_Q, "", "" };
            if (len >= sizeof t.payload) len = sizeof t.payload - 1;
            memcpy(t.payload, val_start, len); t.payload[len] = '\0';
            if (nt<max_out) out[nt++] = t;
            p = q + 2; // saute "||" fermant
            continue;
        }

        // valeur ||...||
        if (*p=='|' && (p+1<end) && p[1]=='|'){
            p += 2;
            const char *val_start = p;
            const char *q = p;
            while (q+1 < end) {
                if (q[0]=='|' && q[1]=='|') {
                    const char *next = q+2;
                    // vérifier si fin valide: fin chaîne ou espace suivi d'op ou ) ou opérateur symbolique
                    if (next >= end ||
                        is_space(*next) ||
                        *next == ')' ||
                        starts_with_ci(next,"and") ||
                        starts_with_ci(next,"or") ||
                        starts_with_ci(next,"et") ||
                        starts_with_ci(next,"ou") ||
                        (next+1<end && (strncmp(next,"?=",2)==0 || strncmp(next,"!=",2)==0 || strncmp(next,">=",2)==0 || strncmp(next,"<=",2)==0)) ||
                        (*next=='>' || *next=='<')
                    ) break;
                }
                q++;
            }
            if (q+1>=end) return -2; // erreur
            size_t len = (size_t)(q - val_start);
            Tok t = (Tok){ TK_VAL, "", "" };
            if (len >= sizeof t.payload) len = sizeof t.payload - 1;
            memcpy(t.payload, val_start, len); t.payload[len] = '\0';
            if (nt<max_out) out[nt++] = t;
            p = q + 2;
            continue;
        }

        // opérateurs symboliques binaires
        //if (p+1<end && p[0]=='=' && p[1]=='='){ if (nt<max_out) out[nt++] = (Tok){TK_OP,"==",""}; p+=2; continue; }
        if (p+1<end && p[0]=='!' && p[1]=='='){ if (nt<max_out) out[nt++] = (Tok){TK_OP,"!=",""}; p+=2; continue; }
        if (p+1<end && p[0]=='<' && p[1]=='='){ if (nt<max_out) out[nt++] = (Tok){TK_OP,"<=",""}; p+=2; continue; }
        if (p+1<end && p[0]=='>' && p[1]=='='){ if (nt<max_out) out[nt++] = (Tok){TK_OP,">=",""}; p+=2; continue; }
        if (p[0]=='<'){ if (nt<max_out) out[nt++] = (Tok){TK_OP,"<",""}; p++; continue; }
        if (p[0]=='>'){ if (nt<max_out) out[nt++] = (Tok){TK_OP,">",""}; p++; continue; }

        // alias ?= (==)
        if (p+1<end && p[0]=='?' && p[1]=='='){ if (nt<max_out) out[nt++] = (Tok){TK_OP,"==",""}; p+=2; continue; }

        // si on arrive ici : caractère inattendu
        return -9;
    }
    return nt;
}

static int precedence(const char *op){
    if (strcmp(op,"||")==0) return 1; // OR
    if (strcmp(op,"&&")==0) return 2; // AND
    if (!strcmp(op,"==")||!strcmp(op,"!=")||!strcmp(op,"<")||!strcmp(op,"<=")||!strcmp(op,">")||!strcmp(op,">=")) return 3;
    return 0;
}
static int is_right_assoc(const char *op){ (void)op; return 0; }

static int to_rpn(const Tok in[], int n, Tok out[], int max_out){
    Tok stack[256]; int sp=0, rp=0; //stack tableau : la pile temporaire pour l’algorithme de Shunting-Yard, qui stocke les opérateurs et parenthèses pendant la conversion en RPN.
    for (int i=0;i<n;i++){
        Tok t = in[i];
        if (t.type==TK_VAL || t.type==TK_UN_NOT || t.type==TK_UN_Q){
            if (rp<max_out) out[rp++] = t;
        } else if (t.type==TK_OP){
            while (sp>0 && stack[sp-1].type==TK_OP){
                int p1 = precedence(stack[sp-1].text), p2 = precedence(t.text);
                if (p1 > p2 || (p1==p2 && !is_right_assoc(t.text))){
                    if (rp<max_out) out[rp++] = stack[--sp];
                } else break;
            }
            stack[sp++] = t;
        } else if (t.type==TK_LPAREN){
            stack[sp++] = t;
        } else if (t.type==TK_RPAREN){
            while (sp>0 && stack[sp-1].type!=TK_LPAREN){
                if (rp<max_out) out[rp++] = stack[--sp];
            }
            if (sp==0) return -20; // parenthèse manquante
            sp--; // pop '('
        }
    }
    while (sp>0){
        if (stack[sp-1].type==TK_LPAREN) return -21;
        if (rp<max_out) out[rp++] = stack[--sp];
    }
    return rp;
}

static double as_bool(double v){ return v!=0.0 ? 1.0 : 0.0; }

//evaluation du postfix  fait par to_rpn convertissant infix en postfix via shunting yard
static int eval_rpn(const Tok rpn[], int nrpn){
    double st[1024]; int sp=0;
    for (int i=0;i<nrpn;i++){
        const Tok *t=&rpn[i];
        if (t->type==TK_VAL){
            char temp[1024];
            strncpy(temp, t->payload, 1023); temp[1023] = '\0';
            double v = condition_script_order(temp);
            st[sp++] = v;
        } else if (t->type==TK_UN_NOT){
            char temp[1024];
            strncpy(temp, t->payload, 1023); temp[1023] = '\0';
            double v = condition_script_order(temp);
            st[sp++] = (v==0.0) ? 1.0 : 0.0;
        } else if (t->type==TK_UN_Q){
            char temp[1024];
            strncpy(temp, t->payload, 1023); temp[1023] = '\0';
            double v = condition_script_order(temp);
            st[sp++] = (v!=0.0) ? 1.0 : 0.0;
        } else if (t->type==TK_OP){
            // binaire: pop b puis a
            if (sp<2) return 0;
            double b = st[--sp], a = st[--sp];
            double r = 0.0;
            if (!strcmp(t->text,"&&")) r = (as_bool(a) && as_bool(b)) ? 1.0 : 0.0;
            else if (!strcmp(t->text,"||")) r = (as_bool(a) || as_bool(b)) ? 1.0 : 0.0;
            else if (!strcmp(t->text,"==")) r = (a==b) ? 1.0 : 0.0;
            else if (!strcmp(t->text,"!=")) r = (a!=b) ? 1.0 : 0.0;
            else if (!strcmp(t->text,"<"))  r = (a<b)  ? 1.0 : 0.0;
            else if (!strcmp(t->text,"<=")) r = (a<=b) ? 1.0 : 0.0;
            else if (!strcmp(t->text,">"))  r = (a>b)  ? 1.0 : 0.0;
            else if (!strcmp(t->text,">=")) r = (a>=b) ? 1.0 : 0.0;
            st[sp++] = r;
        }
    }
    return (sp==1 && st[0]!=0.0) ? 1 : 0;
}

// API principale : évalue la condition entre cond_start (inclus) et cond_end (exclu)
static int evaluate_if_paren_condition(const char *cond_start, const char *cond_end){
    Tok toks[512], rpn[512]; //tableau
    int nt = tokenize_if_paren(cond_start, cond_end, toks, 512);
    if (nt < 0) return 0; // erreur de tokenisation => faux
    int nr = to_rpn(toks, nt, rpn, 512);
    if (nr < 0) return 0; // erreur RPN
    return eval_rpn(rpn, nr);
}
