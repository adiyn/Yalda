/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @file yalda_connection.h
/// @brief Shared types, ANSI styles, TUI helpers, and platform IPC declarations for Yalda.
/// @author adiyn

#ifndef YALDA_CONNECTION_H
#define YALDA_CONNECTION_H

#define DIS_APP_ID      "1143261501633867846"
#define MAX_BUFFER_SIZE 4096

/* ANSI color & style escape sequences */
#define KGRN            "\x1B[32m"
#define KRED            "\x1B[31m"
#define KYEL            "\x1B[33m"
#define KBLU            "\x1B[34m"
#define KDIM            "\x1B[2m"
#define KBOLD           "\x1B[1m"
#define MAGHB           "\x1B[0;105m"
#define KNRM            "\x1B[0m"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/// @brief Logical application categories used for Rich Presence formatting.
typedef enum {
    CAT_OTHER = 0,
    CAT_BROWSER,
    CAT_MUSIC,
    CAT_CODE,
    CAT_WORK
} YaldaCategory;

/// @brief Maps an executable name to a fallback Discord asset key and category.
typedef struct {
    const char* exe;
    const char* asset;
    YaldaCategory category;
} YaldaAppEntry;

/// @brief Dynamic array used while collecting deduplicated process names.
typedef struct {
    char** names;
    size_t count;
    size_t capacity;
} YaldaProcList;

/* Shared TUI & database helpers (src/yalda_tui.c) */
void showLogo(void);
void showTuiFrame(const char* activeApp, const char* activeCat, const char* statusMsg, char** procs);
const char* yaldaCategoryLabel(YaldaCategory cat);
const YaldaAppEntry* yaldaFindApp(const char* exeName);
const char* appNameToAssetName(const char* appName);

/* Platform process scanner */
char** getProcsNames(void);
void freeProcs(char** procs);

#ifdef __linux__

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <termios.h>
#include <strings.h>
#include <ctype.h>

#pragma pack(push, 1)
typedef struct PacketHeader {
    uint32_t opCode;
    uint32_t opLength;
} PacketHeader;
#pragma pack(pop)

int ConnectionDiscord(void);
bool Disconnect(int sockFd);

#elif defined(_WIN32)

#ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <lmcons.h>
#include <psapi.h>
#include <shellapi.h>
#include <conio.h>
#include <winhttp.h>
#include <time.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

/* Discord's local IPC endpoint. Multiple client instances can exist,
 * so ConnectionDiscord() below probes discord-ipc-0 .. discord-ipc-9. */

#pragma pack(push, 1)
typedef struct PacketHeader {
    uint32_t opCode;
    DWORD opLength;
} PacketHeader;
#pragma pack(pop)

bool SetupConsole(void);
void RestoreConsole(void);
bool isToBeIgnored(const char* name);
void SendHandshake(HANDLE hPipe);
HANDLE ConnectionDiscord(void);
HANDLE EstablishConnection(void);
bool Disconnect(HANDLE hPipe);
bool getWindowTitleForExe(const char* exeName, char* outTitle, size_t outSize);

/// @brief Queries the free iTunes Search API for a track's artwork, duration,
///        and builds a Qobuz.com search link for the track.
/// @param queryUtf8       Search term (e.g. "Artist - Track").
/// @param outUrl          Receives the 512x512 artwork URL.
/// @param outUrlSize      Size of outUrl.
/// @param outTrackUrl     Receives a Qobuz.com search URL for the track.
/// @param outTrackUrlSize Size of outTrackUrl.
/// @param outDurationSec  Receives track length in seconds (0 if unknown).
/// @return true if artwork was found (other fields best-effort).
bool fetchItunesMetadata(const char* queryUtf8,
                         char* outUrl, size_t outUrlSize,
                         char* outTrackUrl, size_t outTrackUrlSize,
                         int* outDurationSec);

/// @brief Sends a SET_ACTIVITY payload and waits for Discord's reply.
/// @param hPipe         Connected Discord IPC pipe handle.
/// @param details       Primary activity details string (e.g. track title).
/// @param appName       Target executable name (used for fallback asset lookup).
/// @param imageOverride Optional HTTPS image URL overriding the static Discord asset.
/// @param startEpoch    Unix timestamp when the track started (0 = no timer).
/// @param durationSec   Track length in seconds (0 = no end timestamp).
/// @param trackUrl      URL for the presence button (NULL/"" = no button).
/// @param errOut        Buffer receiving Discord's error message on failure (may be NULL).
/// @param errSize       Size of errOut in bytes.
/// @return true if Discord acknowledged the presence update, false otherwise.
bool UpdatePresence(HANDLE hPipe, const char* details, const char* appName,
                    const char* imageOverride, time_t startEpoch, int durationSec,
                    const char* trackUrl, char* errOut, size_t errSize);

#endif /* __linux__ / _WIN32 */
#endif /* YALDA_CONNECTION_H */
