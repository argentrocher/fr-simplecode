#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>

//gcc -shared -o test_dll.dll test_dll.c -lgdi32 -luser32

__declspec(dllexport)
const char* hex(char* arg) {
    static char buffer[32];
    char *endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0') {
        snprintf(buffer, sizeof(buffer), "erreur: %s", arg);
        return buffer;
    }

    if (val < -1024 || val > 1023) {
        snprintf(buffer, sizeof(buffer), "hors limite: %s", arg);
        return buffer;
    }

    unsigned int encoded = (val >= 0) ? val : (2048 + val);  // complément à deux sur 11 bits
    snprintf(buffer, sizeof(buffer), "%03X", encoded);  // 3 chiffres hexa suffisent pour 11 bits

    return buffer;
}

__declspec(dllexport)
const char* bin(char* arg) {
    static char buffer[32];
    char *endptr;
    long val = strtol(arg, &endptr, 10);

    if (*endptr != '\0') {
        snprintf(buffer, sizeof(buffer), "erreur: %s", arg);
        return buffer;
    }

    if (val < -1024 || val > 1023) {
        snprintf(buffer, sizeof(buffer), "hors limite: %s", arg);
        return buffer;
    }

    unsigned int encoded = (val >= 0) ? val : (2048 + val);

    // Conversion binaire sur 11 bits
    for (int i = 10; i >= 0; i--) {
        buffer[10 - i] = ((encoded >> i) & 1) ? '1' : '0';
    }
    buffer[11] = '\0';

    return buffer;
}

__declspec(dllexport)
const char* hello(char* args) {
    static char buffer[256];
    //printf("ARGS: %s\n", args); // <== ajout de debug
    snprintf(buffer, sizeof(buffer), "Bonjour %s depuis la DLL !", args);
    return buffer;
}


static char user_input[256];
static COLORREF backgroundColor = RGB(200, 200, 200);  // Couleur par défaut

WNDPROC OldEditProc;

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        char buffer[256];
        GetWindowText(hwnd, buffer, sizeof(buffer));
        if (strlen(buffer) > 0) {
            strcpy(user_input, buffer);
            PostQuitMessage(1);
            return 0;
        }
    }
    return CallWindowProc(OldEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit, hButton;

    switch (msg) {
        case WM_CREATE: {
            hEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                                 20, 20, 240, 25, hwnd, (HMENU)1, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hButton = CreateWindow("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                   100, 60, 80, 25, hwnd, (HMENU)2, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            SetFocus(hEdit);  // focus automatique

            // Subclass ici — après que hEdit est visible et prêt
            OldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == 2) {  // Bouton OK
                GetWindowText(GetDlgItem(hwnd, 1), user_input, sizeof(user_input));
                PostQuitMessage(1);
            }
            break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(backgroundColor);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);
            return 1;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

__declspec(dllexport)
const char* inputbox(char* color) {
    int r, g, b;
    if (sscanf(color, "%d;%d;%d", &r, &g, &b) == 3) {
        backgroundColor = RGB(r, g, b);
    } else {
        printf("Erreur de format couleur : %s\n", color);
        return "0";
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = InputWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "InputBoxClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    RegisterClass(&wc);

    int width = 300, height = 150;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    HWND hwnd = CreateWindow("InputBoxClass", "Entrez votre texte",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             x, y, width, height,
                             NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBox(NULL, "Erreur lors de la création de la fenêtre", "Erreur", MB_ICONERROR);
        return "0";
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hwnd);

    return msg.wParam == 1 ? user_input : "0";
}
