#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//gcc -shared -o frc_windows_creator.dll frc_windows_creator.c -lgdi32 -luser32 -lmsimg32

#define MAX_WINDOWS 100
#define MAX_TITLE_LEN 256
#define MAX_CLASSNAME_LEN 64

//variable de fr-simplecode0.3 qui informe avant 200ms de l'iteruption de la dll et sa libération du buffer (sinon, erreur mémoire crash)
__declspec(dllexport) volatile int frc_interrupted_dll = 0;

//état du thread gestionnaire de fenêtre 0=ok 1=destroy
volatile int state_global_window_thread = 0;

// Extrait la partie entière avant ',' ou '.' et convertit en int pour tout les nombres
int parse_int_ignore_fraction(const char* token) {
    char temp[32];
    int i = 0;
    // Copie jusqu'à ',' ou '.' ou fin de chaîne
    while (token[i] != '\0' && token[i] != ',' && token[i] != '.' && i < (int)(sizeof(temp) - 1)) {
        temp[i] = token[i];
        i++;
    }
    temp[i] = '\0';

    return atoi(temp);
}

//info pour les codes :
typedef struct {
    const char* key_name;
    const char* code;
} KeyCodeEntry;

KeyCodeEntry key_event_table[] = {
    {"espace", "0"}, {"a", "1"}, {"b", "2"}, {"c", "3"}, {"d", "4"}, {"e", "5"}, {"f", "6"}, {"g", "7"},
    {"h", "8"}, {"i", "9"}, {"j", "10"}, {"k", "11"}, {"l", "12"}, {"m", "13"}, {"n", "14"}, {"o", "15"},
    {"p", "16"}, {"q", "17"}, {"r", "18"}, {"s", "19"}, {"t", "20"}, {"u", "21"}, {"v", "22"}, {"w", "23"},
    {"x", "24"}, {"y", "25"}, {"z", "26"},
    {"flèchehaut", "26"}, {"flèchebas", "27"}, {"flèchegauche", "28"}, {"flèchedroite", "29"},
    {"0", "30"}, {"1", "31"}, {"2", "32"}, {"3", "33"}, {"4", "34"}, {"5", "35"}, {"6", "36"}, {"7", "37"},
    {"8", "38"}, {"9", "39"}, {"enter", "40"}, {"effaceenarrière", "41"}, {"suppr", "42"},
    {"echap", "43"}, {"majuscule", "44"}, {"verroumaj", "45"}, {",", "46"}, {";", "47"}, {":", "48"},
    {"!", "49"}, {"ù", "50"},
    {"clicgauche", "-2"}, {"doubleclicgauche", "-3"},
    {"clicdroit", "-4"}, {"doubleclicdroit", "-5"},
    {"clicmolette", "-6"},
    {"NULL", "-1"}
};
//fonction d'info pour les codes
__declspec(dllexport) const char* get_window_code_event(char* arg) {
    static char result[16];

    // Nettoyage du suffixe ",00" ou ".00"
    size_t len = strlen(arg);
    if (len > 3) {
        if ((strcmp(arg + len - 3, ",00") == 0) || (strcmp(arg + len - 3, ".00") == 0)) {
            arg[len - 3] = '\0';  // Coupe la chaîne à ce point
        }
    }

    for (int i = 0; key_event_table[i].key_name != NULL; ++i) {
        if (strcmp(arg, key_event_table[i].key_name) == 0) {
            // Si l'argument est un nom de touche, retourne le code
            snprintf(result, sizeof(result), "%s", key_event_table[i].code);
            return result;
        }
        if (strcmp(arg, key_event_table[i].code) == 0) {
            // Si l'argument est un code, retourne le nom
            snprintf(result, sizeof(result), "%s", key_event_table[i].key_name);
            return result;
        }
    }

    return "-1";
}

//code de touche du clavier
int get_key_code(WPARAM wParam) {
    if (wParam == VK_SPACE) return 0;
    if (wParam >= 'A' && wParam <= 'Z') return wParam - 'A' + 1;
    if (wParam >= '0' && wParam <= '9') return wParam - '0' + 30;
    if (wParam == VK_RETURN) return 40;
    if (wParam == VK_BACK) return 41;
    if (wParam == VK_DELETE) return 42;
    if (wParam == VK_ESCAPE) return 43;
    if (wParam == VK_SHIFT) return 44;          // Maj (Shift)
    if (wParam == VK_CAPITAL) return 45;        // Maj (cadenas)
    if (wParam == VK_UP) return 26;
    if (wParam == VK_DOWN) return 27;
    if (wParam == VK_LEFT) return 28;
    if (wParam == VK_RIGHT) return 29;
    if (wParam == VK_OEM_COMMA) return 46;      // ,
    if (wParam == VK_OEM_1) return 47;          // ;
    if (wParam == VK_OEM_2) return 48;          // :
    if (wParam == VK_OEM_3) return 49;          // !
    if (wParam == 0xDB) return 50;              // ù (approximation, clavier FR AZERTY)
    return -999;  // touche non gérée
}

typedef struct {
    int id;
    int width, height;
    int xpos, ypos;
    int r, g, b;
    char title[MAX_TITLE_LEN];
} WindowParams;

typedef struct {
    WindowParams params;
    HWND hwnd;
    COLORREF bgColor;

    HDC hMemDC;
    HBITMAP hBitmap;

    int bitmapWidth;
    int bitmapHeight;

    int is_fullscreen;
    WINDOWPLACEMENT prev_placement;
} WindowInfo;

static WindowInfo windows[MAX_WINDOWS];
static int window_count = 0;

static CRITICAL_SECTION queue_lock;
static WindowParams creation_queue[MAX_WINDOWS];
static int queue_start = 0;
static int queue_end = 0;

static HANDLE thread_handle = NULL;
static DWORD thread_id = 0;
static HANDLE event_new_window = NULL;

//destruction de la fenêtre
static int destroy_queue[MAX_WINDOWS];
static int destroy_start = 0;
static int destroy_end = 0;

static void enqueue_destroy(int id) {
    int next = (destroy_end + 1) % MAX_WINDOWS;
    if (next != destroy_start) {
        destroy_queue[destroy_end] = id;
        destroy_end = next;
    }
}

//icon de la fenêtre
typedef struct {
    int id;
    char path[MAX_PATH];
} IconRequest;

static IconRequest icon_queue[MAX_WINDOWS];
static int icon_queue_start = 0;
static int icon_queue_end = 0;

static void enqueue_icon_request(int id, const char* path) {
    int next = (icon_queue_end + 1) % MAX_WINDOWS;
    if (next != icon_queue_start) {
        icon_queue[icon_queue_end].id = id;
        strncpy(icon_queue[icon_queue_end].path, path, MAX_PATH - 1);
        icon_queue[icon_queue_end].path[MAX_PATH - 1] = '\0';
        icon_queue_end = next;
    }
}

//taille de la fenêtre
typedef struct {
    int id;
    int width;
    int height;
} WindowSizeRequest;

#define MAX_WINDOW_SIZE_REQUESTS 256
WindowSizeRequest window_size_requests[MAX_WINDOW_SIZE_REQUESTS];
int window_size_request_count = 0;
CRITICAL_SECTION window_size_cs;

void handle_window_size_requests() {
    EnterCriticalSection(&window_size_cs);
    for (int i = 0; i < window_size_request_count; ++i) {
        WindowSizeRequest* req = &window_size_requests[i];

        for (int w = 0; w < window_count; ++w) {
            if (windows[w].params.id == req->id) {
                HWND hwnd = windows[w].hwnd;

                if (req->width == 0 && req->height == 0) {
                    ShowWindow(hwnd, SW_MAXIMIZE);
                }
                else if (req->width == -1 && req->height == -1) {
                    MONITORINFO mi = { sizeof(mi) };
                    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                        windows[w].is_fullscreen = TRUE;
                        GetWindowPlacement(hwnd, &windows[w].prev_placement);
                        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                        SetWindowPos(hwnd, HWND_TOP,
                            mi.rcMonitor.left, mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top,
                            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                    }
                }
                else if (req->width > 0 && req->height > 0) {
                    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                    SetWindowPos(hwnd, NULL, 0, 0, req->width, req->height,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
                    ShowWindow(hwnd, SW_NORMAL);
                    windows[w].is_fullscreen = FALSE;
                } else {
                    printf("Paramètres invalides pour window_size: %d;%d\n", req->width, req->height);
                    fflush(stdout);
                }
                break;
            }
        }
    }
    window_size_request_count = 0;
    LeaveCriticalSection(&window_size_cs);
}

//pixel sur une fenêtre
typedef struct {
    int id;
    int x, y;
    int r, g, b;
} PixelRequest;

static PixelRequest pixel_queue[MAX_WINDOWS * 10];
static int pixel_queue_start = 0;
static int pixel_queue_end = 0;

static void enqueue_pixel_request(PixelRequest req) {
    int next = (pixel_queue_end + 1) % (MAX_WINDOWS * 10);
    if (next != pixel_queue_start) {
        pixel_queue[pixel_queue_end] = req;
        pixel_queue_end = next;
    }
}

//rectangle sur une fenêtre
typedef struct {
    int id;
    int x1, y1, x2, y2;
    int r, g, b;
} RectRequest;

static RectRequest rect_queue[MAX_WINDOWS * 10];
static int rect_queue_start = 0;
static int rect_queue_end = 0;

static void enqueue_rect_request(RectRequest req) {
    int next = (rect_queue_end + 1) % (MAX_WINDOWS * 10);
    if (next != rect_queue_start) {
        rect_queue[rect_queue_end] = req;
        rect_queue_end = next;
    }
}

//cercle plein sur la fenêtre
typedef struct {
    int id;
    int cx, cy;
    int radius;
    int r, g, b;
} CircleRequest;

static CircleRequest circle_queue[MAX_WINDOWS * 10];
static int circle_queue_start = 0;
static int circle_queue_end = 0;

static void enqueue_circle_request(CircleRequest req) {
    int next = (circle_queue_end + 1) % (MAX_WINDOWS * 10);
    if (next != circle_queue_start) {
        circle_queue[circle_queue_end] = req;
        circle_queue_end = next;
    }
}

//ligne sur la fenêtre
typedef struct {
    int id;
    int x1, y1;
    int x2, y2;
    int r, g, b;
} LineRequest;

static LineRequest line_queue[MAX_WINDOWS * 10];
static int line_queue_start = 0;
static int line_queue_end = 0;

static void enqueue_line_request(LineRequest req) {
    int next = (line_queue_end + 1) % (MAX_WINDOWS * 10);
    if (next != line_queue_start) {
        line_queue[line_queue_end] = req;
        line_queue_end = next;
    }
}

//texte sur la fenêtre
typedef struct {
    int id;
    int x, y;
    int size;
    int style;
    int r, g, b;
    char text[512];
} TextRequest;

static TextRequest text_queue[MAX_WINDOWS * 10];
static int text_queue_start = 0;
static int text_queue_end = 0;

static void enqueue_text_request(TextRequest req) {
    int next = (text_queue_end + 1) % (MAX_WINDOWS * 10);
    if (next != text_queue_start) {
        text_queue[text_queue_end] = req;
        text_queue_end = next;
    }
}

//background_color
typedef struct {
    int id;
    COLORREF color;
} BgColorRequest;

BgColorRequest bgcolor_queue[MAX_WINDOWS];
volatile int bgcolor_queue_start = 0;
volatile int bgcolor_queue_end = 0;

//window_event
typedef struct EventRequest {
    int id;                 // fenêtre concernée
    int event_code;         // code de l'événement détecté (rempli par le thread)
    int ready;              // flag pour savoir si un évènement est arrivé
    HANDLE event_ready;     // event Windows pour attendre dans la fonction DLL
    struct EventRequest* next;
} EventRequest;

#define MAX_EVENTS 100
//static EventRequest event_requests[MAX_EVENTS];
static int event_req_start = 0;
static int event_req_end = 0;
//static CRITICAL_SECTION event_lock;
EventRequest* event_request_list = NULL;
CRITICAL_SECTION event_lock;

// fermeture des event
void flush_all_event_requests() {
    EnterCriticalSection(&event_lock);
    EventRequest* cur = event_request_list;
    while (cur) {
        if (cur->event_ready) SetEvent(cur->event_ready);
        cur = cur->next;
    }
    LeaveCriticalSection(&event_lock);
}


static volatile int running = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    for (int i = 0; i < window_count; ++i) {
        if (windows[i].hwnd == hwnd) {
            // Affiche le contenu du buffer mémoire
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            // Remplir le fond avec bgColor
            HBRUSH bgBrush = CreateSolidBrush(windows[i].bgColor);
            FillRect(hdc, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            TransparentBlt(hdc,
            0, 0,
            clientRect.right, clientRect.bottom,
            windows[i].hMemDC,
            0, 0,
            clientRect.right, clientRect.bottom,
            RGB(1, 1, 1) // couleur à traiter comme transparente
            );
            break;
        }
    }

    EndPaint(hwnd, &ps);
    return 0;
    }
    case WM_SIZE:{
    InvalidateRect(hwnd, NULL, TRUE); // Juste redessiner
    return 0;
    }
    case WM_KEYDOWN:{
    if (wParam == VK_ESCAPE) {
        for (int i = 0; i < window_count; ++i) {
            if (windows[i].hwnd == hwnd && windows[i].is_fullscreen) {
                SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                SetWindowPlacement(hwnd, &windows[i].prev_placement);
                SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                windows[i].is_fullscreen = FALSE;
                break;
            }
        }
    }
    int code = get_key_code(wParam);
    if (code >= 0) {
        EnterCriticalSection(&event_lock);

        // Trouver l'ID de la fenêtre à partir du HWND
        int window_id = -1;
        for (int i = 0; i < window_count; ++i) {
            if (windows[i].hwnd == hwnd) {
                window_id = windows[i].params.id;
                break;
            }
        }

        if (window_id != -1) {
            EventRequest* cur = event_request_list;
            while (cur) {
                if (cur->id == window_id) {
                    cur->event_code = code;
                    SetEvent(cur->event_ready);
                }
                cur = cur->next;
            }
        }

        LeaveCriticalSection(&event_lock);
    }

    return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: {
    int event_code_ = 0;
    switch (uMsg) {
        case WM_LBUTTONDOWN: event_code_ = -2; break;
        case WM_LBUTTONDBLCLK: event_code_ = -3; break;
        case WM_RBUTTONDOWN: event_code_ = -4; break;
        case WM_RBUTTONDBLCLK: event_code_ = -5; break;
        case WM_MBUTTONDOWN: event_code_ = -6; break;
    }

    EnterCriticalSection(&event_lock);

    int window_id = -1;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].hwnd == hwnd) {
            window_id = windows[i].params.id;
            break;
        }
    }

    if (window_id != -1) {
        EventRequest* cur = event_request_list;
        while (cur) {
            if (cur->id == window_id) {
                cur->event_code = event_code_;
                SetEvent(cur->event_ready);
            }
            cur = cur->next;
        }
    }

    LeaveCriticalSection(&event_lock);
    return 0;
    }
    case WM_MOUSEMOVE: {
    int window_id = -1;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].hwnd == hwnd) {
            window_id = windows[i].params.id;
            break;
        }
    }

    if (window_id != -1) {
        EnterCriticalSection(&event_lock);
        EventRequest* cur = event_request_list;
        while (cur) {
                if (cur->id == window_id) {
                    cur->event_code = -10;
                    SetEvent(cur->event_ready);
                }
                cur = cur->next;
        }
        LeaveCriticalSection(&event_lock);
    }

    return 0;
    }
    case WM_DESTROY:
        // Trouver la fenêtre et la retirer du tableau
        for (int i = 0; i < window_count; ++i) {
            if (windows[i].hwnd == hwnd) {
                // Retirer cette fenêtre du tableau en décalant à gauche
                for (int j = i; j < window_count - 1; ++j) {
                    windows[j] = windows[j + 1];
                }
                window_count--;
                break;
            }
        }

        if (window_count == 0) {
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static void RegisterWindowClass(HINSTANCE hInstance, const char* className, COLORREF bgColor) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = NULL;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    if (!RegisterClass(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            printf("RegisterClass failed: %lu\n", err);
            fflush(stdout);
            // On ne quitte pas le thread ici, on tente quand même
        }
    }
}

static int enqueue_window(WindowParams* params) {
    EnterCriticalSection(&queue_lock);
    int next_pos = (queue_end + 1) % MAX_WINDOWS;
    if (next_pos == queue_start) {
        // Queue pleine
        LeaveCriticalSection(&queue_lock);
        return -1;
    }
    creation_queue[queue_end] = *params;
    queue_end = next_pos;
    LeaveCriticalSection(&queue_lock);
    SetEvent(event_new_window); // Notifie le thread
    return 0;
}

static int dequeue_window(WindowParams* params) {
    EnterCriticalSection(&queue_lock);
    if (queue_start == queue_end) {
        LeaveCriticalSection(&queue_lock);
        return 0; // queue vide
    }
    *params = creation_queue[queue_start];
    queue_start = (queue_start + 1) % MAX_WINDOWS;
    LeaveCriticalSection(&queue_lock);
    return 1;
}

DWORD WINAPI WindowThread(LPVOID lpParam) {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    running = 1;

    MSG msg;
    while (running&& !frc_interrupted_dll) {
        handle_window_size_requests();

        //destruction de la fenêtre
        int id_to_destroy;
        while (destroy_start != destroy_end) {
            id_to_destroy = destroy_queue[destroy_start];
            destroy_start = (destroy_start + 1) % MAX_WINDOWS;

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == id_to_destroy && windows[i].hwnd) {
                    PostMessage(windows[i].hwnd, WM_CLOSE, 0, 0); // envoie WM_CLOSE
                    break;
                }
            }
        }

        //icon de la fenêtre
        while (icon_queue_start != icon_queue_end) {
            IconRequest req = icon_queue[icon_queue_start];
            icon_queue_start = (icon_queue_start + 1) % MAX_WINDOWS;

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    HICON hIcon = (HICON)LoadImageA(NULL, req.path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
                    if (hIcon) {
                        SendMessage(windows[i].hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                        SendMessage(windows[i].hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                    } else {
                        printf("Erreur chargement icône : %s\n", req.path);
                        fflush(stdout);
                    }
                    break;
                }
            }
        }
        //pixel sur une fenêtre
        while (pixel_queue_start != pixel_queue_end) {
            PixelRequest req = pixel_queue[pixel_queue_start];
            pixel_queue_start = (pixel_queue_start + 1) % (MAX_WINDOWS * 10);

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    HDC hdc = GetDC(windows[i].hwnd);
                    if (hdc) {
                        //SetPixel(hdc, req.x, req.y, RGB(req.r, req.g, req.b));
                        //ReleaseDC(windows[i].hwnd, hdc);
                        SetPixel(windows[i].hMemDC, req.x, req.y, RGB(req.r, req.g, req.b));
                        InvalidateRect(windows[i].hwnd, NULL, FALSE);
                    }
                    break;
                }
            }
        }

        // rectangle sur une fenêtre
        while (rect_queue_start != rect_queue_end) {
            RectRequest req = rect_queue[rect_queue_start];
            rect_queue_start = (rect_queue_start + 1) % (MAX_WINDOWS * 10);

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    HBRUSH brush = CreateSolidBrush(RGB(req.r, req.g, req.b));
                    RECT rect = { req.x1, req.y1, req.x2, req.y2 };
                    FillRect(windows[i].hMemDC, &rect, brush);
                    DeleteObject(brush);
                    InvalidateRect(windows[i].hwnd, &rect, FALSE);
                    break;
                }
            }
        }

        // cercle plein sur une fenêtre
        while (circle_queue_start != circle_queue_end) {
            CircleRequest req = circle_queue[circle_queue_start];
            circle_queue_start = (circle_queue_start + 1) % (MAX_WINDOWS * 10);

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    HBRUSH brush = CreateSolidBrush(RGB(req.r, req.g, req.b));
                    HBRUSH oldBrush = SelectObject(windows[i].hMemDC, brush);

                    HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
                    HPEN oldPen = SelectObject(windows[i].hMemDC, nullPen);

                    int left = req.cx - req.radius;
                    int top = req.cy - req.radius;
                    int right = req.cx + req.radius;
                    int bottom = req.cy + req.radius;

                    Ellipse(windows[i].hMemDC, left, top, right, bottom);

                    SelectObject(windows[i].hMemDC, oldPen);
                    SelectObject(windows[i].hMemDC, oldBrush);
                    DeleteObject(brush);

                    RECT updateRect = { left, top, right, bottom };
                    InvalidateRect(windows[i].hwnd, &updateRect, FALSE);
                    break;
                }
            }
        }
        // ligne sur la fenêtre
        while (line_queue_start != line_queue_end) {
            LineRequest req = line_queue[line_queue_start];
            line_queue_start = (line_queue_start + 1) % (MAX_WINDOWS * 10);

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    HDC hdc = windows[i].hMemDC;
                    if (hdc) {
                        HPEN pen = CreatePen(PS_SOLID, 1, RGB(req.r, req.g, req.b));
                        HGDIOBJ oldPen = SelectObject(hdc, pen);

                        MoveToEx(hdc, req.x1, req.y1, NULL);
                        LineTo(hdc, req.x2, req.y2);

                        SelectObject(hdc, oldPen);
                        DeleteObject(pen);

                        InvalidateRect(windows[i].hwnd, NULL, FALSE);
                    }
                    break;
                }
            }
        }
        // texte sur la fenêtre
        while (text_queue_start != text_queue_end) {
    TextRequest req = text_queue[text_queue_start];
    text_queue_start = (text_queue_start + 1) % (MAX_WINDOWS * 10);

    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
            HDC hdc = windows[i].hMemDC;
            if (!hdc) break;

            int weight = (req.style == 3 || req.style == 5 || req.style == 6 || req.style == 7) ? FW_BOLD : FW_NORMAL;
            BOOL italic = (req.style == 1 || req.style == 4 || req.style == 5 || req.style == 7);
            BOOL underline = (req.style == 2 || req.style == 4 || req.style == 6 || req.style == 7);

            HFONT hFont = CreateFont(
                -MulDiv(req.size, GetDeviceCaps(hdc, LOGPIXELSY), 72),
                0, 0, 0, weight, italic, underline, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial"
            );

            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            SetTextColor(hdc, RGB(req.r, req.g, req.b));
            SetBkMode(hdc, TRANSPARENT);

            RECT rect;
            rect.left = req.x;
            rect.top = req.y;
            rect.right = rect.left + 10000; // grande largeur
            rect.bottom = rect.top + 10000;

            HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
            HPEN oldPen = SelectObject(hdc, nullPen);

            // Remplacer [/n] par \n
            char formatted[512];
            strncpy(formatted, req.text, sizeof(formatted)-1);
            formatted[sizeof(formatted)-1] = '\0';
            for (char* p = strstr(formatted, "[/n]"); p; p = strstr(p, "[/n]")) {
                *p = '\n';
                memmove(p + 1, p + 4, strlen(p + 4) + 1);
            }

            DrawText(hdc, formatted, -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            InvalidateRect(windows[i].hwnd, NULL, FALSE);
            break;
        }
    }
}
        //background_color
        while (bgcolor_queue_start != bgcolor_queue_end) {
            BgColorRequest req = bgcolor_queue[bgcolor_queue_start];
            bgcolor_queue_start = (bgcolor_queue_start + 1) % MAX_WINDOWS;

            for (int i = 0; i < window_count; ++i) {
                if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
                    windows[i].bgColor = req.color;
                    // Mettre à jour le fond du buffer mémoire (bitmap)
                    RECT rect;
                    GetClientRect(windows[i].hwnd, &rect);
                    //HBRUSH bgBrush = CreateSolidBrush(req.color);
                    //FillRect(windows[i].hMemDC, &rect, bgBrush);
                    //DeleteObject(bgBrush);
                    InvalidateRect(windows[i].hwnd, NULL, FALSE);
                    UpdateWindow(windows[i].hwnd);
                    break;
                }
            }
        }

        // Créer toutes les fenêtres en attente
        WindowParams params;
        while (dequeue_window(&params)) {
            char className[MAX_CLASSNAME_LEN];
            snprintf(className, sizeof(className), "WindowClass_%d", params.id);
            RegisterWindowClass(hInstance, className, RGB(params.r, params.g, params.b));

            HWND hwnd = CreateWindowEx(
                0,
                className,
                params.title,
                WS_OVERLAPPEDWINDOW,
                params.xpos, params.ypos,
                params.width, params.height,
                NULL, NULL, hInstance, NULL);

            if (!hwnd) {
                printf("CreateWindow failed for ID %d\n", params.id);
                fflush(stdout);
                continue;
            }

            if (window_count < MAX_WINDOWS) {
                windows[window_count].params = params;
                windows[window_count].hwnd = hwnd;
                windows[window_count].bgColor = RGB(params.r, params.g, params.b);

                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);

                // Initialiser le buffer mémoire
                HDC hdc = GetDC(hwnd);
                windows[window_count].hMemDC = CreateCompatibleDC(hdc);
                windows[window_count].hBitmap = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
                SelectObject(windows[window_count].hMemDC, windows[window_count].hBitmap);
                ReleaseDC(hwnd, hdc);

                // Initialiser bitmap avec une "couleur transparente" (par exemple noir)
                HBRUSH transparentBrush = CreateSolidBrush(RGB(1, 1, 1)); // ou une autre couleur
                RECT rect = {0, 0, screenWidth, screenHeight};
                FillRect(windows[window_count].hMemDC, &rect, transparentBrush);
                DeleteObject(transparentBrush);

                windows[window_count].bitmapWidth = screenWidth;
                windows[window_count].bitmapHeight = screenHeight;

                // Peindre la couleur de fond dans la bitmap
                //HBRUSH bgBrush = CreateSolidBrush(windows[window_count].bgColor);
                //RECT rect = {0, 0, params.width, params.height};
                //FillRect(windows[window_count].hMemDC, &rect, bgBrush);
                //DeleteObject(bgBrush);

                window_count++;
            }

            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);

            SetForegroundWindow(hwnd);   // Donne le focus au clavier
            SetActiveWindow(hwnd);       // Rend la fenêtre active
            SetFocus(hwnd);              // Donne le focus aux contrôles internes
        }

        // Traitement classique des messages Windows
        // On utilise PeekMessage pour ne pas bloquer si pas de message
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Si aucune fenêtre ouverte et aucune création en attente -> stop thread
        EnterCriticalSection(&queue_lock);
        int empty_queue = (queue_start == queue_end);
        LeaveCriticalSection(&queue_lock);

        if (window_count == 0 && empty_queue) {
            running = 0;
            break;
        }

        // Attente d'un nouvel évènement ou d’un délai court pour continuer
        WaitForSingleObject(event_new_window, 100);
        ResetEvent(event_new_window);
    }
    //libérer si il y a des event sur la fenêtre (window_event ...)
    flush_all_event_requests();

    //arrêt du thread de fenêtre et donc arrêt des threads d'évènement (ex : window_event ...)
    state_global_window_thread = 1;

    printf("frc_windows_creator interrompt le thread de fenêtre\n");
    fflush(stdout);

    //restart param
    queue_start=0;
    queue_end=0;
    window_count=0;

    return 0;
}

__declspec(dllexport) const char* create_window(char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg) {
        printf("Argument null\n");
        fflush(stdout);
        return error;
    }

    int delimiter_count = 0;
    for (char* p = arg; *p; p++) {
        if (*p == ';') delimiter_count++;
    }
    if (delimiter_count != 8) {
        printf("nombre d'arguments invalides: %s\n", arg);
        fflush(stdout);
        return error;
    }

    WindowParams params;
    char buf[512];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tokens[9];
    char* tok = strtok(buf, ";");
    int count = 0;

    while (tok && count < 9) {
    tokens[count++] = tok;
    tok = strtok(NULL, ";");
    }

    if (count != 9) {
    printf("erreur de parsing des arguments: %s\n", arg);
    fflush(stdout);
    return error;
    }

    int verify_id = parse_int_ignore_fraction(tokens[0]);
    if (verify_id<=0){
        printf("id invalide: %d (n'est pas suppérieur à 0)\n", verify_id);
        fflush(stdout);
        return error;
    }

    params.id     = parse_int_ignore_fraction(tokens[0]);
    params.width  = parse_int_ignore_fraction(tokens[1]);
    params.height = parse_int_ignore_fraction(tokens[2]);
    params.xpos   = parse_int_ignore_fraction(tokens[3]);
    params.ypos   = parse_int_ignore_fraction(tokens[4]);
    params.r      = parse_int_ignore_fraction(tokens[5]);
    params.g      = parse_int_ignore_fraction(tokens[6]);
    params.b      = parse_int_ignore_fraction(tokens[7]);
    strncpy(params.title, tokens[8], 255);
    params.title[255] = '\0';
    //printf("parsed=%d, title='%s'\n", parsed, params.title);

    // Centrage si demandé (xpos == -1 ou ypos == -1)
    if (params.xpos == -1) {
        params.xpos = (GetSystemMetrics(SM_CXSCREEN) - params.width) / 2;
    }
    if (params.ypos == -1) {
        params.ypos = (GetSystemMetrics(SM_CYSCREEN) - params.height) / 2;
    }

    if (params.xpos < 0 || params.ypos < 0 ||
        params.width <= 0 || params.height <= 0 ||
        params.r < 0 || params.r > 255 ||
        params.g < 0 || params.g > 255 ||
        params.b < 0 || params.b > 255) {
        printf("valeur(s) d'un ou des arguments invalides: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Check duplicate ID
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == params.id) {
            printf("l'ID existe déjà pour une fenêtre en cour: %d\n", params.id);
            fflush(stdout);
            return error;
        }
    }

    static int initialized = 0;

    DWORD exit_code;
    if (!thread_handle || !GetExitCodeThread(thread_handle, &exit_code) || exit_code != STILL_ACTIVE) {
        if (thread_handle) CloseHandle(thread_handle);
        thread_handle = NULL;
        initialized = 0;
    }


    // Création du thread une seule fois

    if (!initialized) {
        InitializeCriticalSection(&queue_lock);
        InitializeCriticalSection(&window_size_cs);
        InitializeCriticalSection(&event_lock);
        event_new_window = CreateEvent(NULL, TRUE, FALSE, NULL);
        thread_handle = CreateThread(NULL, 0, WindowThread, NULL, 0, &thread_id);
        if (!thread_handle) {
            printf("échec de création du thread de la fenêtre\n");
            fflush(stdout);
            return error;
        }
        initialized = 1;
    }

    if (enqueue_window(&params) != 0) {
        printf("File d’attente de création de fenêtre pleine !\n");
        fflush(stdout);
        return error;
    }

    return success;
}

//détruire la fenêtre
__declspec(dllexport) const char* destroy_window(const char* arg) {
    static const char* success = "1";
    static const char* error = "-1";

    if (!arg || strcmp(arg, "") == 0 || strcmp(arg, "0.00") == 0 || strcmp(arg, "0,00") == 0 || strcmp(arg, "0") == 0) {
        // Détruire toutes les fenêtres
        for (int i = 0; i < window_count; ++i) {
            enqueue_destroy(windows[i].params.id);
        }
        SetEvent(event_new_window);
        // On ne vide pas window_count ici, il sera mis à jour via WM_DESTROY
        return success;
    }

    int id = parse_int_ignore_fraction(arg);
    if (id < 0) {
        printf("ID invalide pour la destruction : %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Cherche la fenêtre avec cet ID
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            if (windows[i].hwnd && IsWindow(windows[i].hwnd)) {
                enqueue_destroy(id);
                SetEvent(event_new_window);  // Réveille le thread fenêtre
                return success;
            } else {
                printf("Fenêtre trouvée mais HWND invalide pour ID : %d\n", id);
                fflush(stdout);
                return error;
            }
        }
    }

    printf("Aucune fenêtre avec l'ID spécifié : %d\n", id);
    fflush(stdout);
    return error;
}

//fenêtre exist ou non :
__declspec(dllexport) const char* window_exist(const char* arg) {
    static const char* success = "1";
    static const char* error = "-1";

    if (!arg || strcmp(arg, "") == 0 || strcmp(arg, "0.00") == 0 || strcmp(arg, "0,00") == 0 || strcmp(arg, "0") == 0) {
        //voir si aumoins une fenêtre existe avec un id
        for (int i = 0; i < window_count; ++i) {
            if (windows[i].hwnd && IsWindow(windows[i].hwnd)) {
                return success;  // Il y a au moins une fenêtre valide
            }
        }
        return error;
    }

    int id = parse_int_ignore_fraction(arg);
    if (id < 0) {
        printf("ID impossible pour window_exist : %s\n", arg);
        fflush(stdout);
        return error;
    }

    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            if (windows[i].hwnd && IsWindow(windows[i].hwnd)) {
                return success;
            } else {
                printf("Fenêtre trouvée mais HWND invalide pour ID : %d\n", id);
                fflush(stdout);
                return error;
            }
        }
    }

    // Aucune fenêtre trouvée avec cet ID
    //printf("Fenêtre ID %d introuvable dans window_exist.\n", id);
    //fflush(stdout);
    return error;
}

//icon ico de la fenêtre
__declspec(dllexport) const char* window_icon(const char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg || strlen(arg) < 3) {
        printf("Argument icon invalide : %s\n", arg);
        fflush(stdout);
        return error;
    }

    int id;
    char path[MAX_PATH];
    char buf[512];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tok = strtok(buf, ";");
    char* second = strtok(NULL, "");

    if (!tok || !second) {
    printf("Erreur parsing arg pour window_icon : %s\n", arg);
    fflush(stdout);
    return error;
    }

    id = parse_int_ignore_fraction(tok);
    strncpy(path, second, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    // Vérifie que la fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Aucune fenêtre avec ID %d pour icône.\n", id);
        fflush(stdout);
        return error;
    }

    enqueue_icon_request(id, path);
    SetEvent(event_new_window); // pour réveiller le thread

    return success;
}

//taille de la fenêtre
__declspec(dllexport) const char* window_size(const char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg || strlen(arg) < 5) return error;

    WindowSizeRequest req;
    char buf[64];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tok = strtok(buf, ";");
    int values[3];
    int count = 0;

    while (tok && count < 3) {
    values[count++] = parse_int_ignore_fraction(tok);
    tok = strtok(NULL, ";");
    }

    if (count != 3) {
    printf("Erreur parsing window_size: %s\n", arg);
    fflush(stdout);
    return error;
    }

    req.id     = values[0];
    req.width  = values[1];
    req.height = values[2];

    EnterCriticalSection(&window_size_cs);
    if (window_size_request_count < MAX_WINDOW_SIZE_REQUESTS) {
        window_size_requests[window_size_request_count++] = req;
        LeaveCriticalSection(&window_size_cs);
        SetEvent(event_new_window);
        return success;
    } else {
        LeaveCriticalSection(&window_size_cs);
        printf("Trop de requêtes window_size.\n");
        fflush(stdout);
        return error;
    }
}

//pixel sur la fenêtre
__declspec(dllexport) const char* create_pixel(const char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg || strlen(arg) < 5) return error;

    PixelRequest req;
    char buf[64];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tok = strtok(buf, ";");
    int values[6];
    int count = 0;

    while (tok && count < 6) {
        values[count++] = parse_int_ignore_fraction(tok);
        tok = strtok(NULL, ";");
    }

    if (count != 6) {
        printf("Erreur parsing pixel: %s\n", arg);
        fflush(stdout);
        return error;
    }

    req.id = values[0];
    req.x  = values[1];
    req.y  = values[2];
    req.r  = values[3];
    req.g  = values[4];
    req.b  = values[5];

    // Validation des valeurs
    if (req.x < 0 || req.y < 0 ||
        req.r < 0 || req.r > 255 ||
        req.g < 0 || req.g > 255 ||
        req.b < 0 || req.b > 255) {
        printf("Valeurs invalides pixel: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Vérifie que la fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour pixel.\n", req.id);
        fflush(stdout);
        return error;
    }

    enqueue_pixel_request(req);
    SetEvent(event_new_window); // Réveiller le thread

    return success;
}

//rectangle sur la fenêtre
__declspec(dllexport) const char* create_rectangle(char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg) return error;

    RectRequest req;
    char buf[128];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tok = strtok(buf, ";");
    int values[8];
    int count = 0;

    while (tok && count < 8) {
        values[count++] = parse_int_ignore_fraction(tok);
        tok = strtok(NULL, ";");
    }

    if (count != 8) {
        printf("Erreur parsing rectangle: %s\n", arg);
        fflush(stdout);
        return error;
    }

    req.id = values[0];
    req.x1 = values[1];
    req.y1 = values[2];
    req.x2 = values[3];
    req.y2 = values[4];
    req.r  = values[5];
    req.g  = values[6];
    req.b  = values[7];

    if (req.r < 0 || req.r > 255 ||
        req.g < 0 || req.g > 255 ||
        req.b < 0 || req.b > 255) {
        printf("Valeurs invalides de pixel sur rectangle: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Optionnel : vérifier que x2 > x1 et y2 > y1
    if (req.x2 < req.x1 || req.y2 < req.y1) {
        printf("Valeurs invalides de position sur rectangle: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Vérifie que la fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour rectangle.\n", req.id);
        fflush(stdout);
        return error;
    }

    enqueue_rect_request(req);
    return success;
}

//cercle sur la fenêtre
__declspec(dllexport) const char* create_circle(char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg) return error;

    // Copie la chaîne pour la découper
    char buffer[128];
    strncpy(buffer, arg, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    char* tokens[7];
    int token_count = 0;
    char* p = strtok(buffer, ";");
    while (p && token_count < 7) {
        tokens[token_count++] = p;
        p = strtok(NULL, ";");
    }

    if (token_count != 7) {
        printf("Erreur parsing cercle: %s\n", arg);
        fflush(stdout);
        return error;
    }

    CircleRequest req;
    req.id = parse_int_ignore_fraction(tokens[0]);
    req.cx = parse_int_ignore_fraction(tokens[1]);
    req.cy = parse_int_ignore_fraction(tokens[2]);
    req.radius = parse_int_ignore_fraction(tokens[3]);
    req.r = parse_int_ignore_fraction(tokens[4]);
    req.g = parse_int_ignore_fraction(tokens[5]);
    req.b = parse_int_ignore_fraction(tokens[6]);

    if (req.radius <= 0 ||
        req.r < 0 || req.r > 255 ||
        req.g < 0 || req.g > 255 ||
        req.b < 0 || req.b > 255) {
        printf("Valeurs invalides sur cercle: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Vérifie que la fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour cercle.\n", req.id);
        fflush(stdout);
        return error;
    }

    enqueue_circle_request(req);
    return success;
}

//ligne sur la fenêtre
__declspec(dllexport) const char* create_line(const char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg || strlen(arg) < 11) return error;

    LineRequest req;
    char buf[128];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, ";");
    int values[8];
    int count = 0;

    while (tok && count < 8) {
    values[count++] = parse_int_ignore_fraction(tok);
    tok = strtok(NULL, ";");
    }

    if (count != 8) {
    printf("Erreur parsing ligne: %s\n", arg);
    fflush(stdout);
    return error;
    }

    req.id = values[0];
    req.x1 = values[1];
    req.y1 = values[2];
    req.x2 = values[3];
    req.y2 = values[4];
    req.r  = values[5];
    req.g  = values[6];
    req.b  = values[7];

    // Validation des couleurs
    if (req.r < 0 || req.r > 255 ||
        req.g < 0 || req.g > 255 ||
        req.b < 0 || req.b > 255) {
        printf("Valeurs couleurs invalides ligne: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Vérifie si fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour ligne.\n", req.id);
        fflush(stdout);
        return error;
    }

    enqueue_line_request(req);
    SetEvent(event_new_window); // Réveiller le thread

    return success;
}

//création de texte sur la fenêtre
__declspec(dllexport) const char* create_text_(const char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg || strlen(arg) < 5) return error;

    TextRequest req;
    char buf[1024];
    strncpy(buf, arg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* tokens[9];
    char* p = buf;

    // Découpe manuelle des 8 premiers champs par ';'
    for (int i = 0; i < 8; ++i) {
        tokens[i] = p;
        char* next_sep = strchr(p, ';');
        if (!next_sep) {
            printf("Erreur parsing champ %d: %s\n", i, arg);
            fflush(stdout);
            return error;
        }
        *next_sep = '\0';  // coupe ici
        p = next_sep + 1;
    }

    tokens[8] = p;


    req.id    = parse_int_ignore_fraction(tokens[0]);
    req.x     = parse_int_ignore_fraction(tokens[1]);
    req.y     = parse_int_ignore_fraction(tokens[2]);
    req.size  = parse_int_ignore_fraction(tokens[3]);
    req.style = parse_int_ignore_fraction(tokens[4]);
    req.r     = parse_int_ignore_fraction(tokens[5]);
    req.g     = parse_int_ignore_fraction(tokens[6]);
    req.b     = parse_int_ignore_fraction(tokens[7]);
    strncpy(req.text, tokens[8], sizeof(req.text) - 1);
    req.text[sizeof(req.text) - 1] = '\0';

    if (req.x < 0 || req.y < 0 || req.size <= 0 ||
        req.r < 0 || req.r > 255 ||
        req.g < 0 || req.g > 255 ||
        req.b < 0 || req.b > 255) {
        printf("Valeurs invalides texte: %s\n", arg);
        fflush(stdout);
        return error;
    }

    if (req.style < 0 || req.style > 7){
        printf("le style de texte doit être compris entre 0 et 7: %d\n", req.style);
        fflush(stdout);
        return error;
    }

    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == req.id && IsWindow(windows[i].hwnd)) {
            hwnd_found = windows[i].hwnd;
            break;
        }
    }

    if (!hwnd_found) {
        printf("Fenêtre ID %d introuvable pour texte.\n", req.id);
        fflush(stdout);
        return error;
    }

    enqueue_text_request(req);
    SetEvent(event_new_window); // Pour réveiller le thread

    return success;
}


//gestion de la couleur du fond background
__declspec(dllexport) const char* window_background_color(char* arg) {
    static const char* error = "-1";
    static const char* success = "1";

    if (!arg) {
        printf("Argument null\n");
        fflush(stdout);
        return error;
    }

    int id, r, g, b;
    char buf[64];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, ";");
    int values[4];
    int count = 0;

    while (tok && count < 4) {
    values[count++] = parse_int_ignore_fraction(tok);
    tok = strtok(NULL, ";");
    }

    if (count != 4) {
    printf("Erreur parsing window_background: %s\n", arg);
    fflush(stdout);
    return error;
    }

    id = values[0];
    r  = values[1];
    g  = values[2];
    b  = values[3];

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        printf("Valeurs RGB invalides: %s\n", arg);
        fflush(stdout);
        return error;
    }

    // Vérifie si fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour couleur de fond.\n", id);
        fflush(stdout);
        return error;
    }

    // Chercher la fenêtre
    EnterCriticalSection(&queue_lock);
    int next_pos = (bgcolor_queue_end + 1) % MAX_WINDOWS;
    if (next_pos == bgcolor_queue_start) {
        LeaveCriticalSection(&queue_lock);
        printf("file de modification de couleur pleine\n");
        fflush(stdout);
        return error;
    }

    bgcolor_queue[bgcolor_queue_end].id = id;
    bgcolor_queue[bgcolor_queue_end].color = RGB(r, g, b);
    bgcolor_queue_end = next_pos;
    LeaveCriticalSection(&queue_lock);

    SetEvent(event_new_window); // notifier le thread
    return success;
}


//gestion d'évènement sur la fenêtre
__declspec(dllexport) const char* window_event(char* arg) {
    static char result[32];
    int id;
    char buf[64];
    if (!arg){
        printf("Argument null\n");
        fflush(stdout);
        return "-1";
    }

    if (strstr(arg,";")!=NULL){
        printf("Argument unique id, pas de ; sur window_event\n");
        fflush(stdout);
        return "-1";
    }

    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    id = parse_int_ignore_fraction(buf);

    // Vérifie si fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour window_event.\n", id);
        fflush(stdout);
        return "-1";
    }

    EventRequest* req = (EventRequest*)malloc(sizeof(EventRequest));
    if (!req) return "-1";
    req->id = id;
    req->event_code = -1;
    req->event_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!req->event_ready) {
        free(req);
        return "-1";
    }

    EnterCriticalSection(&event_lock);
    req->next = event_request_list;
    event_request_list = req;
    LeaveCriticalSection(&event_lock);

    int code = -1;
    do {
        DWORD wait_res = WaitForSingleObject(req->event_ready, 50);
        if (frc_interrupted_dll || state_global_window_thread) {
            // Libère proprement
            CloseHandle(req->event_ready);
            EnterCriticalSection(&event_lock);
            req->event_ready = NULL;
            req->ready = 0;
            LeaveCriticalSection(&event_lock);
            return "-1";
        }

        if (wait_res == WAIT_OBJECT_0) {
            code = req->event_code;
            if (code == -10) {
                ResetEvent(req->event_ready);
                req->ready = 0;
            }
        }
    } while (code == -10);

    EnterCriticalSection(&event_lock);
    EventRequest** cur = &event_request_list;
    while (*cur) {
        if (*cur == req) {
            *cur = req->next;
            break;
        }
        cur = &((*cur)->next);
    }
    LeaveCriticalSection(&event_lock);

    CloseHandle(req->event_ready);
    free(req);

    snprintf(result, sizeof(result), "%d", code);
    return result;
}

//gestion d'évènement sur la fenêtre 2
__declspec(dllexport) const char* window_on_event(char* arg) {
    static char result[32];
    int id, expected_code;
    char buf[64];

    if (!arg){
        printf("Argument null\n");
        fflush(stdout);
        return "-1";
    }

    if (strstr(arg,";")==NULL){
        printf("pas de ; sur window_on_event\n");
        fflush(stdout);
        return "-1";
    }

    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Découpe sur le premier point-virgule uniquement
    char* tok = strtok(buf, ";");
    if (!tok){
        printf("pas de ; sur window_on_event\n");
        fflush(stdout);
        return "-1";
    }
    id = parse_int_ignore_fraction(tok);

    // Récupère la suite (expected_code et éventuellement du bruit après)
    tok = strtok(NULL, "");  // récupère tout le reste de la chaîne
    if (!tok) return "-1";
    expected_code = parse_int_ignore_fraction(tok);

    // Vérifie si fenêtre existe
    int found = 0;
    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id) {
            hwnd_found = windows[i].hwnd;
            if (hwnd_found && IsWindow(hwnd_found)) {
                found = 1;
            }
            break;
        }
    }

    if (!found) {
        printf("Fenêtre ID %d introuvable pour window_on_event.\n", id);
        fflush(stdout);
        return "-1";
    }

    EventRequest* req = (EventRequest*)malloc(sizeof(EventRequest));
    if (!req) return "-1";
    req->id = id;
    req->event_code = -1;
    req->event_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!req->event_ready) {
        free(req);
        return "-1";
    }

    EnterCriticalSection(&event_lock);
    req->next = event_request_list;
    event_request_list = req;
    LeaveCriticalSection(&event_lock);

    while (1) {
        DWORD wait_res = WaitForSingleObject(req->event_ready, 50);
        if (frc_interrupted_dll || state_global_window_thread) {
            // Libère proprement
            CloseHandle(req->event_ready);
            EnterCriticalSection(&event_lock);
            req->event_ready = NULL;
            req->ready = 0;
            LeaveCriticalSection(&event_lock);
            return "-1";
        }
        if (wait_res == WAIT_OBJECT_0) {
            if (req->event_code == expected_code)
                break;
        }
        ResetEvent(req->event_ready);
    }

    EnterCriticalSection(&event_lock);
    EventRequest** cur = &event_request_list;
    while (*cur) {
        if (*cur == req) {
            *cur = req->next;
            break;
        }
        cur = &((*cur)->next);
    }
    LeaveCriticalSection(&event_lock);

    CloseHandle(req->event_ready);
    free(req);

    snprintf(result, sizeof(result), "1");
    return result;
}

//mouvement de la souris sur la fenêtre
__declspec(dllexport) const char* window_on_mouse_move(char* arg) {
    static char result[32];
    static const char* error = "-1";
    int id;
    char buf[64];

    if (!arg) {
        printf("Argument null\n");
        fflush(stdout);
        return error;
    }

    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, "");
    if (!tok) {
        printf("Erreur parsing id sur window_on_mouse_move\n");
        fflush(stdout);
        return error;
    }

    id = parse_int_ignore_fraction(tok);

    HWND hwnd_found = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id && IsWindow(windows[i].hwnd)) {
            hwnd_found = windows[i].hwnd;
            break;
        }
    }

    if (!hwnd_found) {
        printf("Fenêtre ID %d introuvable pour window_on_mouse_move.\n", id);
        fflush(stdout);
        return error;
    }

    EventRequest* req = (EventRequest*)malloc(sizeof(EventRequest));
    if (!req) return error;
    req->id = id;
    req->event_code = -1;
    req->event_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!req->event_ready) {
        free(req);
        return error;
    }

    EnterCriticalSection(&event_lock);
    req->next = event_request_list;
    event_request_list = req;
    LeaveCriticalSection(&event_lock);

    while (1) {
        DWORD wait_res = WaitForSingleObject(req->event_ready, 50);
        if (frc_interrupted_dll || state_global_window_thread) {
            // Libère proprement
            CloseHandle(req->event_ready);
            EnterCriticalSection(&event_lock);
            req->event_ready = NULL;
            req->ready = 0;
            LeaveCriticalSection(&event_lock);
            return "-1";
        }
        if (wait_res == WAIT_OBJECT_0){
            if (req->event_code == -10)
                break;
        }
        ResetEvent(req->event_ready);
    }

    EnterCriticalSection(&event_lock);
    EventRequest** cur = &event_request_list;
    while (*cur) {
        if (*cur == req) {
            *cur = req->next;
            break;
        }
        cur = &((*cur)->next);
    }
    LeaveCriticalSection(&event_lock);

    CloseHandle(req->event_ready);
    free(req);

    snprintf(result, sizeof(result), "1");
    return result;
}



//position de la souris sur la fenêtre
__declspec(dllexport) const char* window_mouse_position(const char* arg) {
    static char result[32];
    static const char* error = "-1";

    if (!arg || strlen(arg) < 1) {
        printf("Argument null\n");
        fflush(stdout);
        return error;
    }

    char buf[64];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, ";");
    if (!tok) {
        printf("pas de ; sur position de la souris\n");
        fflush(stdout);
        return error;
    }
    int id = parse_int_ignore_fraction(tok);

    tok = strtok(NULL, "");
    if (!tok) return error;
    int axis = parse_int_ignore_fraction(tok);

    if (axis != 0 && axis != 1) {
        printf("valeur de l'axe %d incorrect pour position de la souris (0 ou 1).\n", axis);
        fflush(stdout);
        return error;
    }

    // Trouver la fenêtre correspondant à l'ID
    HWND hwnd = NULL;
    for (int i = 0; i < window_count; ++i) {
        if (windows[i].params.id == id && IsWindow(windows[i].hwnd)) {
            hwnd = windows[i].hwnd;
            break;
        }
    }

    if (!hwnd) {
        printf("Fenêtre ID %d introuvable pour position de la souris.\n", id);
        fflush(stdout);
        return error;
    }

    // Récupérer la position de la souris dans la fenêtre
    POINT pt;
    if (!GetCursorPos(&pt)) return error;
    if (!ScreenToClient(hwnd, &pt)) return error;

    snprintf(result, sizeof(result), "%d", (axis == 0) ? pt.x : pt.y);
    return result;
}
