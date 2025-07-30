#include "php_frc.h"
#include <stdio.h>
#include <stdlib.h>

//gcc main.c -o main.exe

int main() {
    char* code_php = "<?php echo 'Bonjour le monde !'; ?>";
    char* uri = "/page.php";

    char* result = php_process_page(code_php, uri);
    if (result) {
        printf("----- REPONSE PHP -----\n%s\n", result);
        free(result);
    } else {
        fprintf(stderr, "Erreur lors de l'exécution du code PHP.\n");
    }

    code_php= "<?php echo 'Hello, ' . (isset($_GET['name']) ? $_GET['name'] : 'guest'); ?>";
    uri =
    "GET /index.php?name=John HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: TestClient/1.0\r\n"
    "Accept: */*\r\n"
    "\r\n";

    result = php_process_cgi(code_php,uri);
    if (result) {
        printf("----- REPONSE PHP -----\n%s\n", result);
        free(result);
    } else {
        fprintf(stderr, "Erreur lors de l'exécution du code PHP.\n");
    }

    code_php =
    "<?php\n"
    "echo \"<h1>PHP CGI Debug</h1>\";\n"
    "echo \"<p><strong>POST name:</strong> \" . (isset($_POST['name']) ? $_POST['name'] : 'undefined') . \"</p>\";\n"
    "echo \"<p><strong>Host:</strong> \" . $_SERVER['HTTP_HOST'] . \"</p>\";\n"
    "echo \"<p><strong>User-Agent:</strong> \" . $_SERVER['HTTP_USER_AGENT'] . \"</p>\";\n"
    "echo \"<p><strong>Request Method:</strong> \" . $_SERVER['REQUEST_METHOD'] . \"</p>\";\n"
    "echo \"<p><strong>Request URI:</strong> \" . $_SERVER['REQUEST_URI'] . \"</p>\";\n"
    "echo \"<p><strong>POST data:</strong><br><pre>\" . print_r($_POST, true) . \"</pre></p>\";\n"
    "echo \"<p><strong>GET data:</strong><br><pre>\" . print_r($_GET, true) . \"</pre></p>\";\n"
    "?>";
    uri =
    "POST /index.php HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: DebugClient/1.0\r\n"
    "Accept: */*\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: 10\r\n"
    "\r\n"
    "name=Johny";

    result = php_process_cgi(code_php,uri);
    if (result) {
        printf("----- REPONSE PHP -----\n%s\n", result);
        free(result);
    } else {
        fprintf(stderr, "Erreur lors de l'exécution du code PHP.\n");
    }

    printf("\nversion php : %s\n",get_info_php());

    return 0;
}
