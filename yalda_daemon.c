/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @file yalda_daemon.c
/// @brief Hidden tray daemon: Discord RPC presence + named-pipe IPC server.
///        Watches the active music app's window title and restarts the
///        presence timer on every track change.
/// @author adiyn

#include "yalda_connection.h"

#ifdef _WIN32

#define YALDA_PIPE_NAME  "\\\\.\\pipe\\yalda-ipc"
#define WM_TRAYICON      (WM_APP + 1)
#define IDM_SHOW_TUI     1001
#define IDM_QUIT         1002
#define POLL_INTERVAL_MS 1000

static const char YALDA_WND_CLASS[] = "YaldaDaemonWnd";

static NOTIFYICONDATAA g_nid;
static HWND g_hwnd = NULL;
static HANDLE g_pipeThread = NULL;
static HANDLE g_pollThread = NULL;
static HANDLE g_quitEvent = NULL;
static HANDLE g_discordPipe = NULL;
static CRITICAL_SECTION g_stateLock;   /* imagine having something called CreateMutex but it sucks ass so bad that u gotta create something called a CRITICAL_SECTION */

static char g_activeApp[MAX_PATH] = "";
static char g_activeCat[32] = "";
static char g_statusMsg[128] = "ready";
static char g_currentTitle[256] = "";   /* last seen music window title */
static time_t g_titleStart = 0;         /* epoch when title last changed */
static int g_trackDurationSec = 0;      /* iTunes-reported track length */
static char g_trackUrl[512] = "";       /* Qobuz search link for the track */

/// @brief Strips a trailing " - Qobuz" suffix if present to keep iTunes queries clean.
static void clean_music_title(char* title)
{
    char* suffix = strstr(title, " - Qobuz");
    if (suffix) {
        *suffix = '\0';
    }
}

/// @brief Pushes a presence payload to Discord and records the outcome for the TUI.
static void push_presence_locked(const char* details, const char* appName,
                                 const char* coverUrl, time_t start,
                                 int durationSec, const char* trackUrl)
{
    char err[96] = "";

    if (!g_discordPipe) {
        g_discordPipe = EstablishConnection();
    }

    if (!g_discordPipe) {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Discord not reachable");
        return;
    }

    if (UpdatePresence(g_discordPipe, details, appName, coverUrl, start,
            durationSec, trackUrl, err, sizeof(err)))
    {
        if (durationSec > 0) {
            snprintf(g_statusMsg, sizeof(g_statusMsg),
                coverUrl[0] ? "RPC ok: +%d:%02d, art, link"
                            : "RPC ok: +%d:%02d, link",
                durationSec / 60, durationSec % 60
            );
        } else {
            snprintf(g_statusMsg, sizeof(g_statusMsg),
                coverUrl[0] ? "RPC ok: no duration, art"
                            : "RPC ok: no duration"
            );
        }
    } else if (err[0] != '\0') {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Discord: %.100s", err);
    } else {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Discord rejected presence");
    }
}

/// @brief Activates an application, reads its window title, fetches artwork if music,
///        stamps the track start time, and updates Discord RPC.
static void apply_app_presence(const char* exeName)
{
    const YaldaAppEntry* entry = yaldaFindApp(exeName);
    const char* cat = entry ? yaldaCategoryLabel(entry->category) : "other";
    bool isMusic = (entry && entry->category == CAT_MUSIC);

    char title[256] = "";
    char coverUrl[512] = "";
    char trackUrl[512] = "";
    int durationSec = 0;

    getWindowTitleForExe(exeName, title, sizeof(title));

    if (isMusic && title[0] != '\0') {
        clean_music_title(title);
        fetchItunesMetadata(title, coverUrl, sizeof(coverUrl),
                            trackUrl, sizeof(trackUrl), &durationSec);
    }

    EnterCriticalSection(&g_stateLock);
    snprintf(g_activeApp, sizeof(g_activeApp), "%s", exeName);
    snprintf(g_activeCat, sizeof(g_activeCat), "%s", cat);

    if (isMusic && title[0] != '\0') {
        snprintf(g_currentTitle, sizeof(g_currentTitle), "%s", title);
        snprintf(g_trackUrl, sizeof(g_trackUrl), "%s", trackUrl);
        g_trackDurationSec = durationSec;
        g_titleStart = time(NULL);
    } else {
        g_currentTitle[0] = '\0';
        g_trackUrl[0] = '\0';
        g_trackDurationSec = 0;
        g_titleStart = 0;
    }

    const char* details = (title[0] != '\0') ? title : "Active via Yalda";
    time_t start = (isMusic && g_titleStart != 0) ? g_titleStart : 0;
    push_presence_locked(details, exeName, coverUrl, start,
                         g_trackDurationSec, g_trackUrl);
    LeaveCriticalSection(&g_stateLock);
}

/// @brief Resets stored state and clears the Discord presence.
static void clear_presence(void)
{
    EnterCriticalSection(&g_stateLock);
    g_activeApp[0] = '\0';
    g_activeCat[0] = '\0';
    g_currentTitle[0] = '\0';
    g_trackDurationSec = 0;
    g_trackUrl[0] = '\0';
    g_titleStart = 0;
    push_presence_locked("Idle", "Yalda", "", 0, 0, "");
    LeaveCriticalSection(&g_stateLock);
}

/// @brief Polls the active music app's window title; on change, re-stamps
///        the start time, refetches artwork, and pushes a fresh presence.
static DWORD WINAPI music_poll_thread(LPVOID arg)
{
    (void)arg;

    while (WaitForSingleObject(g_quitEvent, POLL_INTERVAL_MS) != WAIT_OBJECT_0) {
        char exeName[MAX_PATH];
        char lastTitle[256];

        EnterCriticalSection(&g_stateLock);
        bool isMusic = (strcmp(g_activeCat, "music") == 0) && g_activeApp[0] != '\0';
        if (isMusic) {
            snprintf(exeName, sizeof(exeName), "%s", g_activeApp);
            snprintf(lastTitle, sizeof(lastTitle), "%s", g_currentTitle);
        }
        LeaveCriticalSection(&g_stateLock);

        if (!isMusic) {
            continue;
        }

        char title[256] = "";
        getWindowTitleForExe(exeName, title, sizeof(title));
        if (title[0] == '\0' || strcmp(title, lastTitle) == 0) {
            continue; /* same track (or window gone), nothing to do */
        }

        clean_music_title(title);

        char coverUrl[512] = "";
        char trackUrl[512] = "";
        int durationSec = 0;
        fetchItunesMetadata(title, coverUrl, sizeof(coverUrl),
                            trackUrl, sizeof(trackUrl), &durationSec);

        EnterCriticalSection(&g_stateLock);
        snprintf(g_currentTitle, sizeof(g_currentTitle), "%s", title);
        snprintf(g_trackUrl, sizeof(g_trackUrl), "%s", trackUrl);
        g_trackDurationSec = durationSec;
        g_titleStart = time(NULL);
        push_presence_locked(title, exeName, coverUrl, g_titleStart,
                             g_trackDurationSec, g_trackUrl);
        LeaveCriticalSection(&g_stateLock);
    }

    return 0;
}

/// @brief Builds the serialized STATUS payload expected by yalda_cli.
static void build_status_response(char* out, size_t outSize)
{
    char** procs = getProcsNames();
    int offset = snprintf(out, outSize, "APP:%s\nCAT:%s\nMSG:%s\n",
                          g_activeApp, g_activeCat, g_statusMsg);

    if (procs && offset > 0 && (size_t)offset < outSize) {
        for (int i = 0; i < 9 && procs[i] != NULL; i++) {
            int n = snprintf(out + offset, outSize - (size_t)offset, "PROC:%s\n", procs[i]);
            if (n <= 0 || (size_t)offset + (size_t)n >= outSize) {
                break;
            }
            offset += n;
        }
    }

    if (procs) {
        freeProcs(procs);
    }
}

/// @brief Processes a single IPC command from the CLI client.
static void handle_command(const char* cmd, char* resp, size_t respSize, bool* shouldQuit)
{
    if (strcmp(cmd, "QUIT") == 0) {
        *shouldQuit = true;
        snprintf(resp, respSize, "OK\n");
        return;
    }

    if (strcmp(cmd, "CLEAR") == 0) {
        clear_presence();
        build_status_response(resp, respSize);
        return;
    }

    if (strncmp(cmd, "SELECT:", 7) == 0) {
        int idx = atoi(cmd + 7);
        char** procs = getProcsNames();
        if (procs && idx >= 0) {
            size_t count = 0;
            while (procs[count] != NULL) {
                count++;
            }
            if ((size_t)idx < count) {
                apply_app_presence(procs[idx]);
            }
        }
        if (procs) {
            freeProcs(procs);
        }
        build_status_response(resp, respSize);
        return;
    }

    build_status_response(resp, respSize);
}

/// @brief Serves one connected CLI client (single request/response).
static void serve_client(HANDLE hPipe)
{
    char req[MAX_BUFFER_SIZE] = {0};
    DWORD bytesRead = 0;

    if (ReadFile(hPipe, req, (DWORD)(sizeof(req) - 1), &bytesRead, NULL) && bytesRead > 0) {
        req[bytesRead] = '\0';
        char resp[MAX_BUFFER_SIZE] = {0};
        bool shouldQuit = false;

        handle_command(req, resp, sizeof(resp), &shouldQuit);

        DWORD bytesWritten = 0;
        WriteFile(hPipe, resp, (DWORD)strlen(resp), &bytesWritten, NULL);
        FlushFileBuffers(hPipe);

        if (shouldQuit) {
            SetEvent(g_quitEvent);
        }
    }
}

/// @brief Named-pipe server thread using overlapped connects so quit events wake it.  (AI used here to understand how tf CreateNamedPipeA works and wtf PIPE_ACCESS_DUPLEX means)
static DWORD WINAPI pipe_server_thread(LPVOID arg)
{
    (void)arg;

    while (WaitForSingleObject(g_quitEvent, 0) != WAIT_OBJECT_0) {
        HANDLE hPipe = CreateNamedPipeA(
            YALDA_PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            MAX_BUFFER_SIZE,
            MAX_BUFFER_SIZE,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }

        OVERLAPPED ov = {0};
        ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!ov.hEvent) {
            CloseHandle(hPipe);
            break;
        }

        BOOL connected = ConnectNamedPipe(hPipe, &ov);
        if (!connected && GetLastError() != ERROR_IO_PENDING &&
            GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(ov.hEvent);
            CloseHandle(hPipe);
            continue;
        }

        HANDLE waits[2] = { g_quitEvent, ov.hEvent };
        DWORD wr = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (wr == WAIT_OBJECT_0) {
            CancelIoEx(hPipe, &ov);
            CloseHandle(ov.hEvent);
            CloseHandle(hPipe);
            break;
        }

        if (wr == (WAIT_OBJECT_0 + 1) &&
            (GetOverlappedResult(hPipe, &ov, &(DWORD){0}, FALSE) ||
             GetLastError() == ERROR_PIPE_CONNECTED)) {
            serve_client(hPipe);
        }

        CloseHandle(ov.hEvent);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    return 0;
}

/// @brief Launches yalda.exe from the daemon's own directory (shows the TUI).
static void launch_tui(void)
{
    char exeDir[MAX_PATH];
    char tuiPath[MAX_PATH + 16];
    DWORD len = GetModuleFileNameA(NULL, exeDir, MAX_PATH);

    if (len == 0 || len >= MAX_PATH) {
        return;
    }

    char* slash = strrchr(exeDir, '\\');
    if (!slash) {
        return;
    }
    *slash = '\0';

    int n = snprintf(tuiPath, sizeof(tuiPath), "%s\\yalda.exe", exeDir);
    if (n <= 0 || (size_t)n >= sizeof(tuiPath)) {
        return;
    }

    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessA(tuiPath, NULL, NULL, NULL, FALSE, 0, NULL, exeDir, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

/// @brief Shows the tray context menu (Show TUI / Quit).
static void show_tray_menu(void)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuA(menu, MF_STRING, IDM_SHOW_TUI, "Show TUI");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_QUIT, "Quit Yalda");

    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    PostMessage(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

/// @brief Hidden window procedure handling tray icon and menu messages.
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                launch_tui();
            } else if (lParam == WM_RBUTTONUP) {
                show_tray_menu();
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_SHOW_TUI) {
                launch_tui();
            } else if (LOWORD(wParam) == IDM_QUIT) {
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            Shell_NotifyIconA(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

/// @brief GUI entry point for yalda_daemon.exe (no console window).  (AI used here)
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow)
{
    (void)hPrev; (void)cmdLine; (void)nShow;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hInst;
    wc.lpszClassName = YALDA_WND_CLASS;
    wc.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
    RegisterClassA(&wc);

    g_hwnd = CreateWindowExA(0, YALDA_WND_CLASS, "Yalda", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, hInst, NULL);
    if (!g_hwnd) return EXIT_FAILURE;

    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconA(NULL, IDI_APPLICATION);
    lstrcpyA(g_nid.szTip, "Yalda RPC");
    Shell_NotifyIconA(NIM_ADD, &g_nid);

    InitializeCriticalSection(&g_stateLock);
    g_quitEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

    g_pipeThread = CreateThread(NULL, 0, pipe_server_thread, NULL, 0, NULL);
    g_pollThread = CreateThread(NULL, 0, music_poll_thread, NULL, 0, NULL);

    MSG msg;
    BOOL gm;
    while ((gm = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    SetEvent(g_quitEvent);
    if (g_pipeThread) {
        WaitForSingleObject(g_pipeThread, 3000);
        CloseHandle(g_pipeThread);
    }
    if (g_pollThread) {
        WaitForSingleObject(g_pollThread, 3000);
        CloseHandle(g_pollThread);
    }

    EnterCriticalSection(&g_stateLock);
    if (g_discordPipe) {
        Disconnect(g_discordPipe);
        g_discordPipe = NULL;
    }
    LeaveCriticalSection(&g_stateLock);

    DeleteCriticalSection(&g_stateLock);
    CloseHandle(g_quitEvent);
    return EXIT_SUCCESS;
}

#endif /* _WIN32 */
