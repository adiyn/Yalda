/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @file yalda_proc_win32.c
/// @brief Win32 console helpers, visible-window process enumeration, window title reader,
///        iTunes Search API artwork fetcher, and Discord IPC client.
/// @author adiyn

#include "yalda_connection.h"

#ifdef _WIN32

static HANDLE stdoutHandle = NULL;
static DWORD oldCodePage = 0;
static DWORD outModeInit = 0;

/// @brief Context passed to EnumWindows when searching for a process's window title.
typedef struct {
    const char* targetExe;
    char* outTitle;
    size_t outSize;
    bool found;
} TitleSearchCtx;

/// @brief Enables UTF-8 codepage and ANSI virtual terminal processing on Windows console.
/// @return true on success, false otherwise.
bool SetupConsole(void)
{
    stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle == INVALID_HANDLE_VALUE || stdoutHandle == NULL) {
        return false;
    }

    oldCodePage = GetConsoleOutputCP();
    if (!GetConsoleMode(stdoutHandle, &outModeInit)) {
        return false;
    }

    SetConsoleOutputCP(CP_UTF8);
    DWORD outMode = outModeInit | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(stdoutHandle, outMode)) {
        return false;
    }

    return true;
}

/// @brief Restores the Windows console codepage and mode to their initial state.
void RestoreConsole(void)
{
    if (stdoutHandle == NULL || stdoutHandle == INVALID_HANDLE_VALUE) {
        return;
    }
    SetConsoleOutputCP(oldCodePage);
    SetConsoleMode(stdoutHandle, outModeInit);
}

/// @brief Checks whether a process executable name should be filtered out.
/// @param name Executable filename.
/// @return true if ignored, false otherwise.
bool isToBeIgnored(const char* name)
{
    const char* ignored[] = {
        "yalda_daemon.exe", "yalda.exe", "dis-rpC.exe", "app.exe", NULL
    };

    for (int i = 0; ignored[i] != NULL; i++) {
        if (_stricmp(name, ignored[i]) == 0) {
            return true;
        }
    }
    return false;
}

/// @brief EnumWindows callback collecting deduplicated visible application executable names. also this sh** broken kinda idk
static BOOL CALLBACK EnumWindowsFilter(HWND hwnd, LPARAM lParam)
{
    YaldaProcList* list = (YaldaProcList*)lParam;

    if (!IsWindowVisible(hwnd)) return TRUE;
    LONG_PTR extendedStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (extendedStyle & WS_EX_TOOLWINDOW) return TRUE;
    if (GetWindowTextLengthA(hwnd) == 0) return TRUE;

    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0) {
            return TRUE;
        }
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return TRUE;

    char fullPath[MAX_PATH];
    DWORD size = (DWORD)sizeof(fullPath);
    if (QueryFullProcessImageNameA(hProc, 0, fullPath, &size)) {
        char* exeName = strrchr(fullPath, '\\');
        exeName = exeName ? exeName + 1 : fullPath;

        if (!isToBeIgnored(exeName)) {
            bool isDupe = false;
            for (size_t i = 0; i < list->count; i++) {
                if (_stricmp(list->names[i], exeName) == 0) {
                    isDupe = true;
                    break;
                }
            }
            if (!isDupe) {
                if (list->count >= list->capacity) {
                    size_t newCap = (list->capacity == 0) ? 16 : list->capacity * 2;
                    char** tmp = realloc(list->names, sizeof(char*) * newCap);
                    if (tmp) {
                        list->names = tmp;
                        list->capacity = newCap;
                    }
                }
                if (list->count < list->capacity) {
                    char* dup = _strdup(exeName);
                    if (dup) {
                        list->names[list->count++] = dup;
                    }
                }
            }
        }
    }

    CloseHandle(hProc);
    return TRUE;
}

/// @brief Enumerates currently running GUI applications with visible windows.
/// @return Dynamically allocated, NULL-terminated array of executable name strings.
char** getProcsNames(void)
{
    YaldaProcList list = { .names = NULL, .count = 0, .capacity = 0 };
    EnumWindows(EnumWindowsFilter, (LPARAM)&list);

    char** finalNames = realloc(list.names, sizeof(char*) * (list.count + 1));
    if (!finalNames) {
        freeProcs(list.names);
        return NULL;
    }
    finalNames[list.count] = NULL;
    return finalNames;
}

/// @brief Frees a NULL-terminated array of process name strings.
/// @param procs Array returned by getProcsNames().
void freeProcs(char** procs)
{
    if (!procs) return;
    for (size_t i = 0; procs[i] != NULL; i++) {
        free(procs[i]);
    }
    free(procs);
}

/// @brief EnumWindows callback locating the first visible window title for a target exe.
static BOOL CALLBACK EnumTitleCallback(HWND hwnd, LPARAM lParam)
{
    TitleSearchCtx* ctx = (TitleSearchCtx*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;

    wchar_t wTitle[512];
    int wLen = GetWindowTextW(hwnd, wTitle, 512);
    if (wLen <= 0) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return TRUE;

    char fullPath[MAX_PATH];
    DWORD size = (DWORD)sizeof(fullPath);
    if (QueryFullProcessImageNameA(hProc, 0, fullPath, &size)) {
        char* exeName = strrchr(fullPath, '\\');
        exeName = exeName ? exeName + 1 : fullPath;

        if (_stricmp(exeName, ctx->targetExe) == 0) {
            WideCharToMultiByte(CP_UTF8, 0, wTitle, -1,
                                ctx->outTitle, (int)ctx->outSize, NULL, NULL);
            ctx->found = true;
            CloseHandle(hProc);
            return FALSE;
        }
    }

    CloseHandle(hProc);
    return TRUE;
}

/// @brief Reads the UTF-8 window title of the first visible window matching exeName.
/// @param exeName  Target executable filename (case-insensitive).
/// @param outTitle Output buffer receiving the UTF-8 window title.
/// @param outSize  Size of outTitle in bytes.
/// @return true if a visible window title was found, false otherwise.
bool getWindowTitleForExe(const char* exeName, char* outTitle, size_t outSize)
{
    if (!exeName || !outTitle || outSize == 0) return false;
    outTitle[0] = '\0';

    TitleSearchCtx ctx = {
        .targetExe = exeName,
        .outTitle = outTitle,
        .outSize = outSize,
        .found = false
    };
    EnumWindows(EnumTitleCallback, (LPARAM)&ctx);
    return ctx.found;
}

/// @brief Percent-encodes a UTF-8 string for use in an HTTP query parameter.
static void url_encode_utf8(const char* src, char* dst, size_t dstSize)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 4 < dstSize; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            dst[j++] = (char)c;
        } else if (c == ' ') {
            dst[j++] = '+';
        } else {
            dst[j++] = '%';
            dst[j++] = hex[(c >> 4) & 0x0F];
            dst[j++] = hex[c & 0x0F];
        }
    }
    dst[j] = '\0';
}

/// @brief Escapes quotes, backslashes, and control characters for safe JSON embedding.
static void json_escape(const char* src, char* dst, size_t dstSize)
{
    if (!dst || dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dstSize; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c < 0x20) {
            dst[j++] = ' ';
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

/// @brief Queries the free iTunes Search API for a track's artwork and duration,
///        and builds a Qobuz.com search link for the track.
/// @return true if artwork was found (duration and link are best-effort).  (AI used here)
bool fetchItunesMetadata(const char* queryUtf8,
                         char* outUrl, size_t outUrlSize,
                         char* outTrackUrl, size_t outTrackUrlSize,
                         int* outDurationSec)
{
    if (outTrackUrl && outTrackUrlSize > 0) outTrackUrl[0] = '\0';
    if (outDurationSec) *outDurationSec = 0;
    if (!queryUtf8 || queryUtf8[0] == '\0' || !outUrl || outUrlSize == 0) return false;
    outUrl[0] = '\0';

    /* Always build the Qobuz search link, even if the API call fails. */
    if (outTrackUrl && outTrackUrlSize > 0) {
        char encoded[512];
        url_encode_utf8(queryUtf8, encoded, sizeof(encoded));
        snprintf(outTrackUrl, outTrackUrlSize,
                 "https://www.qobuz.com/us-en/search?q=%s", encoded);
    }

    char enc2[512];
    url_encode_utf8(queryUtf8, enc2, sizeof(enc2));

    char pathA[768];
    snprintf(pathA, sizeof(pathA), "/search?term=%s&entity=song&limit=1", enc2);

    wchar_t pathW[768];
    MultiByteToWideChar(CP_UTF8, 0, pathA, -1, pathW, 768);

    HINTERNET hSession = WinHttpOpen(L"YaldaRPC/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"itunes.apple.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", pathW, NULL,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    bool ok = false;
    if (hRequest &&
        WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        char body[16384];
        size_t total = 0;
        DWORD avail = 0;

        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0 &&
               total + 1 < sizeof(body)) {
            DWORD toRead = avail;
            if (total + toRead >= sizeof(body)) {
                toRead = (DWORD)(sizeof(body) - 1 - total);
            }
            DWORD readNow = 0;
            if (!WinHttpReadData(hRequest, body + total, toRead, &readNow) || readNow == 0) {
                break;
            }
            total += readNow;
        }
        body[total] = '\0';

        /* Track duration (milliseconds) — best-effort.*/
        const char* durKey = "\"trackTimeMillis\":";
        char* dur = strstr(body, durKey);
        if (dur && outDurationSec) {
            long ms = strtol(dur + strlen(durKey), NULL, 10);
            if (ms > 0) {
                *outDurationSec = (int)(ms / 1000);
            }
        }

        const char* key = "\"artworkUrl100\":\"";
        char* start = strstr(body, key);
        if (start) {
            start += strlen(key);
            char* end = strchr(start, '"');
            if (end && (size_t)(end - start) < outUrlSize) {
                size_t len = (size_t)(end - start);
                memcpy(outUrl, start, len);
                outUrl[len] = '\0';

                /* Upgrade 100x100bb -> 512x512bb for crisp Discord cover art */
                char* res = strstr(outUrl, "100x100bb");
                if (res) {
                    memcpy(res, "512x512bb", 9);
                }
                ok = true;
            }
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

/// @brief Reads one Discord IPC frame (header + payload) from the pipe.
/// @param hPipe      Connected pipe handle.
/// @param opCode     Receives the opcode.
/// @param payload    Buffer receiving the JSON payload.
/// @param payloadSz  Size of payload buffer.
/// @return true if a full frame was read.
static bool read_ipc_frame(HANDLE hPipe, uint32_t* opCode, char* payload, size_t payloadSz)
{
    PacketHeader header;
    DWORD read = 0;

    if (!ReadFile(hPipe, &header, (DWORD)sizeof(header), &read, NULL) ||
        read != sizeof(header)) {
        return false;
    }

    *opCode = header.opCode;

    if (payload && payloadSz > 0) {
        payload[0] = '\0';
        if (header.opLength > 0) {
            size_t toRead = (header.opLength < payloadSz - 1)
                ? (size_t)header.opLength
                : payloadSz - 1;
            if (!ReadFile(hPipe, payload, (DWORD)toRead, &read, NULL) || read == 0) {
                return false;
            }
            payload[read] = '\0';

            /* If the frame was bigger than our buffer, drain the rest so the
             * stream stays aligned for the next reply. */
            size_t consumed = read;
            char sink[512];
            while (consumed < header.opLength) {
                size_t want = header.opLength - consumed;
                if (want > sizeof(sink)) want = sizeof(sink);
                if (!ReadFile(hPipe, sink, (DWORD)want, &read, NULL) || read == 0) {
                    break;
                }
                consumed += read;
            }
        }
    } else if (header.opLength > 0) {
        /* No payload buffer requested: drain and discard. */
        char sink[512];
        size_t consumed = 0;
        while (consumed < header.opLength) {
            size_t want = header.opLength - consumed;
            if (want > sizeof(sink)) want = sizeof(sink);
            if (!ReadFile(hPipe, sink, (DWORD)want, &read, NULL) || read == 0) {
                break;
            }
            consumed += read;
        }
    }

    return true;
}

/// @brief Extracts the "message" field from a Discord error payload, if present.
/// @param payload JSON payload string (may be NULL or empty).
/// @param errOut  Output buffer for the message; left "" if none found.
/// @param errSize Size of errOut in bytes.
static void extract_error_message(const char* payload, char* errOut, size_t errSize)
{
    if (errOut && errSize > 0) {
        errOut[0] = '\0';
    }
    if (!payload || !errOut || errSize == 0) return;

    const char* key = "\"message\":\"";
    char* start = strstr(payload, key);
    if (!start) return;

    start += strlen(key);
    char* end = strchr(start, '"');
    if (!end) return;

    size_t len = (size_t)(end - start);
    if (len >= errSize) {
        len = errSize - 1;
    }
    memcpy(errOut, start, len);
    errOut[len] = '\0';
}

/// @brief Opens a handle to the local Discord IPC named pipe.
///
/// Discord's endpoint index isn't fixed: clients occupy discord-ipc-0 up
/// to discord-ipc-9 (a second client, Vencord's separate instance, etc.
/// may hold -0). We probe all ten and use the first that opens.
///
/// @return Valid HANDLE on success, or NULL if Discord is unreachable.
HANDLE ConnectionDiscord(void)
{
    for (int i = 0; i < 10; i++) {
        char pipeName[64];
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\discord-ipc-%d", i);

        HANDLE hPipe = CreateFileA(
            pipeName,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL
        );

        if (hPipe != INVALID_HANDLE_VALUE) {
            return hPipe;
        }
    }
    return NULL;
}

/// @brief Sends the initial Discord RPC handshake packet over the pipe
///        and consumes Discord's READY reply so the stream stays aligned.
/// @param hPipe Connected Discord IPC pipe handle.
void SendHandshake(HANDLE hPipe)
{
    char json_buffer[MAX_BUFFER_SIZE];
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"v\": 1, \"client_id\": \"%s\"}", DIS_APP_ID);

    DWORD json_len = (DWORD)strlen(json_buffer);
    PacketHeader header = { .opCode = 0, .opLength = json_len };

    DWORD bytesWritten = 0;
    if (!WriteFile(hPipe, &header, (DWORD)sizeof(PacketHeader), &bytesWritten, NULL) ||
        !WriteFile(hPipe, json_buffer, json_len, &bytesWritten, NULL)) {
        return;
    }

    /* Drain the op-0 READY frame; ignoring it desyncs later replies. */
    uint32_t op = 0;
    char ready[MAX_BUFFER_SIZE];
    read_ipc_frame(hPipe, &op, ready, sizeof(ready));
}

/// @brief Retries connecting to Discord's IPC pipe up to 10 times and sends the handshake.
/// @return Connected HANDLE on success, or NULL if connection failed.
HANDLE EstablishConnection(void)
{
    HANDLE hPipe = NULL;
    for (int attempts = 10; attempts > 0 && hPipe == NULL; attempts--) {
        hPipe = ConnectionDiscord();
        if (hPipe == NULL && attempts > 1) {
            Sleep(100);
        }
    }
    if (hPipe != NULL) {
        SendHandshake(hPipe);
    }
    return hPipe;
}

/// @brief Sends a SET_ACTIVITY payload to Discord RPC and verifies the reply.
/// @return true if Discord acknowledged the presence, false otherwise.
bool UpdatePresence(HANDLE hPipe, const char* details, const char* appName,
                    const char* imageOverride, time_t startEpoch, int durationSec,
                    const char* trackUrl, char* errOut, size_t errSize)
{
    if (errOut && errSize > 0) {
        errOut[0] = '\0';
    }
    if (!hPipe) return false;

    char escDetails[512];
    char escLargeImg[512];
    char escTrackUrl[512];

    const char* largeImg = (imageOverride && imageOverride[0] != '\0')
        ? imageOverride
        : appNameToAssetName(appName);

    json_escape(details ? details : "", escDetails, sizeof(escDetails));
    json_escape(largeImg, escLargeImg, sizeof(escLargeImg));
    json_escape(trackUrl ? trackUrl : "", escTrackUrl, sizeof(escTrackUrl));

    /* Discreet static hover text instead of the exe name */
    const char* largeText = "Yalda - discord RPC by @adiyn";

    /* Timer block: start + end -> Discord shows remaining track time. */
    char tsBlock[96] = "";
    if (startEpoch > 0 && durationSec > 0) {
        snprintf(tsBlock, sizeof(tsBlock),
                 "\"timestamps\":{\"start\":%lld,\"end\":%lld},",
                 (long long)startEpoch,
                 (long long)startEpoch + (long long)durationSec);
    } else if (startEpoch > 0) {
        snprintf(tsBlock, sizeof(tsBlock), "\"timestamps\":{\"start\":%lld},",
                 (long long)startEpoch);
    }

    /* Presence button (max 2 allowed; we use one) */
    char btnBlock[600] = "";
    if (escTrackUrl[0] != '\0') {
        snprintf(btnBlock, sizeof(btnBlock),
                 "\"buttons\":[{\"label\":\"Listen on Qobuz\",\"url\":\"%s\"}],",
                 escTrackUrl);
    }

    char json_buffer[MAX_BUFFER_SIZE];
    DWORD appPid = GetCurrentProcessId();
    static int nonceCounter = 0;
    int nonce = (rand() + (++nonceCounter)) % 1000000;

    const char* json_payload =
        "{"
            "\"cmd\":\"SET_ACTIVITY\","
            "\"args\":{"
                "\"pid\":%lu,"
                "\"activity\":{"
                    "%s"
                    "%s"
                    "\"details\":\"%s\","
                    "\"assets\":{"
                        "\"large_image\":\"%s\","
                        "\"large_text\":\"%s\""
                    "}"
                "}"
            "},"
            "\"nonce\":\"%d\""
        "}";

    int jsonLen = snprintf(json_buffer, sizeof(json_buffer), json_payload,
                           (unsigned long)appPid,
                           tsBlock,
                           btnBlock,
                           escDetails,
                           escLargeImg,
                           largeText,
                           nonce);

    if (jsonLen <= 0 || (size_t)jsonLen >= sizeof(json_buffer)) {
        return false;
    }

    PacketHeader header = { .opCode = 1, .opLength = (DWORD)jsonLen };
    DWORD bytesWritten = 0;

    if (!WriteFile(hPipe, &header, (DWORD)sizeof(PacketHeader), &bytesWritten, NULL) ||
        !WriteFile(hPipe, json_buffer, (DWORD)jsonLen, &bytesWritten, NULL)) {
        return false;
    }

    uint32_t op = 0;
    char response[MAX_BUFFER_SIZE];

    if (!read_ipc_frame(hPipe, &op, response, sizeof(response))) {
        if (errOut && errSize > 0) {
            snprintf(errOut, errSize, "no reply from Discord");
        }
        return false;
    }

    if (strstr(response, "\"ERROR\"") != NULL || strstr(response, "\"error\"") != NULL) {
        extract_error_message(response, errOut, errSize);
        if (errOut && errSize > 0 && errOut[0] == '\0') {
            snprintf(errOut, errSize, "Discord returned an error frame");
        }
        return false;
    }

    char nonceStr[32];
    snprintf(nonceStr, sizeof(nonceStr), "\"nonce\":\"%d\"", nonce);
    if (strstr(response, nonceStr) == NULL) {
        if (errOut && errSize > 0) {
            snprintf(errOut, errSize, "unexpected reply from Discord");
        }
        return false;
    }

    return true;
}

/// @brief Closes the Discord IPC pipe handle.
/// @param hPipe Pipe handle to close.
/// @return true if closed successfully.
bool Disconnect(HANDLE hPipe)
{
    return hPipe ? (CloseHandle(hPipe) != 0) : false;
}

#endif /* _WIN32 */
