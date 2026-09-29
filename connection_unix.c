#ifdef __linux__
/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @brief DisRPC is an RPC tool made in C !
/// @author adiyn

#include "./src/connection.h"
#include "src/connection.h"

const AppAssetMap APP_MAP[] = {
    { "firefox",    "firefox"   },
    { "chrome",     "chrome"    },
    { "code",       "vscode"    },
    { "msedge",     "edge"      },
    { "discord",    "discord"   },
    { "qobuz",      "qobuz"     },
};
const size_t APP_MAP_COUNT = sizeof(APP_MAP) / sizeof(APP_MAP[0]);

/// @brief shows the logo on the terminal
void showLogo()
{
    printf("\n\n\n");
    printf("\t%s                       ▄▄▄▄▄▄                   ▄▄▄▄▄▄     ▄▄▄▄▄▄    ▄   ▄▄▄▄▄                     %s\n", MAGHB, KNRM);
    printf("\t%s                      █▀██▀▀██                 █▀██▀▀▀█▄  █▀██▀▀▀█▄  ▀████▄▄▄█▀                    %s\n", MAGHB, KNRM);
    printf("\t%s                        ██   ██  ▀▀ ▄██▀█         ██▄▄▄█▀    ██▄▄▄█▀    ██                         %s\n", MAGHB, KNRM);
    printf("\t%s                        ██   ██  ██ ▀███▄         ██▀▀█▄     ██▀▀▀     ██                          %s\n", MAGHB, KNRM);
    printf("\t%s                      ▄ ██   ██  ██ ▄▄██        ▄ ██  ██   ▄ ██        ██▄                         %s\n", MAGHB, KNRM);
    printf("\t%s                     ▀▀▀▀▀▀▀▀▀  ▀▀▀             ▀▀▀▀  ▀▀▀▀ ▀▀▀▀▀        ▀▀▀▀▀▀                     %s\n", MAGHB, KNRM);
}
#define INVALID_HANDLE_VALUE -1

/// @brief sleeps ms miliseconds 
void Sleep(int ms) {
    usleep(ms * 1000);
}

/// @brief returns socket connected to discord app
int ConnectionDiscord()
{
    char socket_path[256];
    const char *dir = "/tmp";

    snprintf(socket_path, sizeof(socket_path), "%s/discord-ipc-0", dir);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return INVALID_HANDLE_VALUE;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return INVALID_HANDLE_VALUE;
    }

    return sock;
}

/// @brief disconnects socket and returns whether it succeeded or not
bool Disconnect(int socket) {
    if (socket != INVALID_HANDLE_VALUE) {
        close(socket);
        return true;
    }
    return false;
}

/// @brief determines whether an app is to be ignored or not by checking if it's in the ignored list
/// @param name the name of the app
/// @return whether app is to be ignored
bool isToBeIgnored(const char* name)
{
    const char* ignored[] = {
        NULL
    };
    
    for (int i = 0; ignored[i] != NULL; i++) {
        if (strcasecmp(name, ignored[i]) == 0) return true;
    }
    return false;
}

/// @brief returns AppList of processes currently running
void getProcsNames(void)
{
    // TODO
}
#endif