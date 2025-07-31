#include <stdlib.h>
#include <string.h>
#include "duktape.h"

//gcc -shared -o javascript_duktape_dll.dll javascript_duktape_dll.c duktape.c -Wl,--out-implib,libjavascript_duktape_dll.a

__declspec(dllexport) const char* javascript(const char* script) {
    static char result[1024];

    duk_context *ctx = duk_create_heap_default();
    if (!ctx) {
        snprintf(result, sizeof(result), "Erreur: contexte JAVA non créé");
        return result;
    }

    if (duk_peval_string(ctx, script) != 0) {
        snprintf(result, sizeof(result), "Erreur JS: %s", duk_safe_to_string(ctx, -1));
    } else {
        snprintf(result, sizeof(result), "%s", duk_safe_to_string(ctx, -1));
    }

    duk_destroy_heap(ctx);
    return result;
}

__declspec(dllexport) const char* info(const char* unused){
   return "javacript est fournit par duktape tout droit reserver,\nle code de duktape est libre, merci de respecter ses droits pour une utilisation commercial ou public\ncette dll est fournit pour fr-simplecode tout droit reserver assigné au compte google argentrocher\nla licence consernant les droits d'utilisation est celle de fr-simplecode\n";
}
