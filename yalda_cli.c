/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @file yalda_cli.c
/// @brief Interactive TUI and CLI client for the Yalda background daemon.
///        Run `yalda` for the live TUI, `yalda status` for a one-shot dump,
///        or `yalda quit` to stop the daemon.
/// @author adiyn

#include "yalda_connection.h"

#ifdef _WIN32

#define YALDA_PIPE_NAME "\\\\.\\pipe\\yalda-ipc"

/// @brief Sends a command to the daemon over the named pipe and reads its response.
static bool daemon_request(const char* cmd, char* respBuf, size_t respSize)
{
    HANDLE hPipe = CreateFileA(
        YALDA_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    DWORD written = 0;
    if (!WriteFile(hPipe, cmd, (DWORD)strlen(cmd), &written, NULL)) {
        CloseHandle(hPipe);
        return false;
    }

    DWORD readBytes = 0;
    if (respBuf && respSize > 1) {
        if (ReadFile(hPipe, respBuf, (DWORD)(respSize - 1), &readBytes, NULL)) {
            respBuf[readBytes] = '\0';
        } else {
            respBuf[0] = '\0';
        }
    }

    CloseHandle(hPipe);
    return true;
}

/// @brief Parses a serialized STATUS response from the daemon and renders the TUI.
static void render_from_response(char* resp)
{
    char activeApp[MAX_PATH] = "";
    char activeCat[32] = "";
    char statusMsg[128] = "";
    char* procs[10] = {0};
    int procCount = 0;

    char* line = strtok(resp, "\n");
    while (line != NULL) {
        if (strncmp(line, "APP:", 4) == 0) {
            snprintf(activeApp, sizeof(activeApp), "%s", line + 4);
        } else if (strncmp(line, "CAT:", 4) == 0) {
            snprintf(activeCat, sizeof(activeCat), "%s", line + 4);
        } else if (strncmp(line, "MSG:", 4) == 0) {
            snprintf(statusMsg, sizeof(statusMsg), "%s", line + 4);
        } else if (strncmp(line, "PROC:", 5) == 0 && procCount < 9) {
            procs[procCount++] = line + 5;
        }
        line = strtok(NULL, "\n");
    }
    procs[procCount] = NULL;

    showTuiFrame(activeApp, activeCat, statusMsg, procs);
}

/// @brief Hides the TUI: clears the screen and waits for a keypress to resume.
///        The daemon keeps running in the background either way.
static void hide_tui(void)
{
    printf("\033[3J\033[H\033[2J");
    printf(KDIM "Yalda hidden — the daemon keeps running in the background.\n"
                "Press any key to show the TUI again..." KNRM "\n");
    fflush(stdout);
    _getch();
}

/// @brief Entry point for `yalda.exe`.
int main(int argc, char** argv)
{
    char resp[MAX_BUFFER_SIZE] = {0};

    if (argc >= 2 && _stricmp(argv[1], "quit") == 0) {
        if (daemon_request("QUIT", resp, sizeof(resp))) {
            printf("Yalda daemon stopped.\n");
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "Yalda daemon is not running.\n");
        return EXIT_FAILURE;
    }

    if (argc >= 2 && _stricmp(argv[1], "status") == 0) {
        if (!daemon_request("STATUS", resp, sizeof(resp))) {
            fprintf(stderr, "Yalda daemon is not running. Start `yalda_daemon.exe` first.\n");
            return EXIT_FAILURE;
        }
        printf("%s", resp);
        return EXIT_SUCCESS;
    }

    SetupConsole();
    atexit(RestoreConsole);

    if (!daemon_request("STATUS", resp, sizeof(resp))) {
        fprintf(stderr, "Yalda daemon is not running. Start `yalda_daemon.exe` first.\n");
        return EXIT_FAILURE;
    }

    for (;;) {
        render_from_response(resp);
        int ch = _getch();

        if (ch == 'q' || ch == 'Q') {
            break; /* Exits TUI only; daemon keeps running in the tray */
        } else if (ch == 'h' || ch == 'H') {
            hide_tui();
        } else if (ch == 'r' || ch == 'R') {
            if (!daemon_request("STATUS", resp, sizeof(resp))) {
                fprintf(stderr, "\nLost connection to the Yalda daemon.\n");
                break;
            }
        } else if (ch == 'c' || ch == 'C') {
            if (!daemon_request("CLEAR", resp, sizeof(resp))) {
                fprintf(stderr, "\nLost connection to the Yalda daemon.\n");
                break;
            }
        } else if (ch >= '1' && ch <= '9') {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "SELECT:%d", ch - '1');
            if (!daemon_request(cmd, resp, sizeof(resp))) {
                fprintf(stderr, "\nLost connection to the Yalda daemon.\n");
                break;
            }
        }
    }

    return EXIT_SUCCESS;
}

#endif /* _WIN32 */