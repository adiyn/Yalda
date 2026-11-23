/// ₍ᐢ. .ᐢ₎ ₊˚⊹ by @adiyn
/// @file yalda_tui.c
/// @brief Terminal UI rendering, YALDA ASCII banner, and application lookup helpers.
/// @author adiyn

#include "yalda_connection.h"

/// @brief Known applications database used for category and Discord asset resolution.
static const YaldaAppEntry YALDA_APPS[] = {
    { "qobuz.exe",        "qobuz",   CAT_MUSIC   },
    { "qobuz",            "qobuz",   CAT_MUSIC   },
    { "firefox.exe",      "firefox", CAT_BROWSER },
    { "firefox",          "firefox", CAT_BROWSER },
    { "chrome.exe",       "chrome",  CAT_BROWSER },
    { "msedge.exe",       "edge",    CAT_BROWSER },
    { "zen.exe",          "zen",     CAT_BROWSER },
    { "code.exe",         "vscode",  CAT_CODE    },
    { "code",             "vscode",  CAT_CODE    },
    { "sublime_text.exe", "sublime", CAT_CODE    },
};

static const size_t YALDA_APPS_COUNT = sizeof(YALDA_APPS) / sizeof(YALDA_APPS[0]);

/// @brief Converts a YaldaCategory enum value into a display string.
/// @param cat The category enum value.
/// @return Lowercase category label.
const char* yaldaCategoryLabel(YaldaCategory cat)
{
    switch (cat) {
        case CAT_BROWSER: return "browser";
        case CAT_MUSIC:   return "music";
        case CAT_CODE:    return "code";
        case CAT_WORK:    return "work";
        default:          return "other";
    }
}

/// @brief Searches the known application table for a matching executable name.
/// @param exeName Executable name to match (case-insensitive).
/// @return Matching YaldaAppEntry pointer, or NULL if unknown.
const YaldaAppEntry* yaldaFindApp(const char* exeName)
{
    if (!exeName || exeName[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < YALDA_APPS_COUNT; i++) {
#ifdef _WIN32
        if (_stricmp(exeName, YALDA_APPS[i].exe) == 0) {
#else
        if (strcasecmp(exeName, YALDA_APPS[i].exe) == 0) {
#endif
            return &YALDA_APPS[i];
        }
    }
    return NULL;
}

/// @brief Resolves an executable name to its Discord asset key (or "default").
/// @param appName Detected executable name.
/// @return Discord asset string identifier.
const char* appNameToAssetName(const char* appName)
{
    const YaldaAppEntry* entry = yaldaFindApp(appName);
    return entry ? entry->asset : "default";
}

/// @brief Prints the YALDA ASCII banner to stdout.
void showLogo(void)
{
    printf("\n");
    printf("  %s██    ██  █████  ██      ██████   █████%s\n", KBOLD KBLU, KNRM);
    printf("  %s ██  ██  ██   ██ ██      ██   ██ ██   ██%s\n", KBOLD KBLU, KNRM);
    printf("  %s  ████   ███████ ██      ██   ██ ███████%s\n", KBOLD KBLU, KNRM);
    printf("  %s   ██    ██   ██ ██      ██   ██ ██   ██%s\n", KBOLD KBLU, KNRM);
    printf("  %s   ██    ██   ██ ███████ ██████  ██   ██%s\n", KBOLD KBLU, KNRM);
    printf("  %s(o.o) Yalda RPC & Process Classifier%s\n\n", KDIM, KNRM);
}

/// @brief Prints a single box-bordered line padded cleanly to a fixed inner width.
/// @param innerWidth Visible character width inside the box borders.
/// @param plainText  Plain text used to measure/truncate visible width.
/// @param styledText Formatted text (may contain ANSI escape codes) to print.
///
/// If the plain text fits inside the box, the styled variant is printed
/// (styling never changes the visible width). If it does NOT fit, we fall
/// back to a truncated plain print so long window titles or process names
/// can never bleed through the right border and corrupt the layout.
static void print_box_line(size_t innerWidth, const char* plainText, const char* styledText)
{
    size_t len = plainText ? strlen(plainText) : 0;
    if (len > innerWidth) {
        len = innerWidth;
    }
    size_t pad = innerWidth - len;

    printf("  │ ");
    if (styledText && len < innerWidth) {
        printf("%s", styledText);
    } else if (plainText) {
        printf("%.*s", (int)len, plainText);
    }
    for (size_t i = 0; i < pad; i++) {
        putchar(' ');
    }
    printf(" │\n");
}

/// @brief Renders a structured TUI panel with active presence info and scanned processes.
/// @param activeApp Currently selected application name (or NULL if idle).
/// @param activeCat Category label for the active application (or NULL).
/// @param statusMsg Short status or IPC message line (or NULL).
/// @param procs     NULL-terminated array of detected process names (optional).
void showTuiFrame(const char* activeApp,
                  const char* activeCat,
                  const char* statusMsg,
                  char** procs)
{
    const size_t W = 54;
    char plainBuf[256];
    char styleBuf[512];

    printf("\033[3J\033[H\033[2J");
    showLogo();

    printf("  ┌────────────────────────────────────────────────────────┐\n");
    print_box_line(W, "ACTIVE PRESENCE", KBOLD KGRN "ACTIVE PRESENCE" KNRM);
    printf("  ├────────────────────────────────────────────────────────┤\n");

    if (activeApp && activeApp[0] != '\0') {
        const char* catStr = (activeCat && activeCat[0] != '\0') ? activeCat : "other";
        snprintf(plainBuf, sizeof(plainBuf), "App      : %.40s", activeApp);
        snprintf(styleBuf, sizeof(styleBuf), "App      : %s%.40s%s", KBOLD, activeApp, KNRM);
        print_box_line(W, plainBuf, styleBuf);

        snprintf(plainBuf, sizeof(plainBuf), "Category : %.40s", catStr);
        snprintf(styleBuf, sizeof(styleBuf), "Category : %s%.40s%s", KGRN, catStr, KNRM);
        print_box_line(W, plainBuf, styleBuf);
    } else {
        print_box_line(W, "App      : <idle / none>", "App      : " KDIM "<idle / none>" KNRM);
        print_box_line(W, "Category : -", "Category : " KDIM "-" KNRM);
    }

    const char* st = (statusMsg && statusMsg[0] != '\0') ? statusMsg : "ready";
    snprintf(plainBuf, sizeof(plainBuf), "Status   : %.39s", st);
    snprintf(styleBuf, sizeof(styleBuf), "Status   : %.39s", st);
    print_box_line(W, plainBuf, styleBuf);

    printf("  ├────────────────────────────────────────────────────────┤\n");
    print_box_line(W, "DETECTED PROCESSES", KBOLD KYEL "DETECTED PROCESSES" KNRM);
    printf("  ├────────────────────────────────────────────────────────┤\n");

    if (!procs || !procs[0]) {
        print_box_line(W, "No processes scanned yet.", KDIM "No processes scanned yet." KNRM);
    } else {
        for (int i = 0; i < 9 && procs[i] != NULL; i++) {
            const YaldaAppEntry* entry = yaldaFindApp(procs[i]);
            const char* cat = entry ? yaldaCategoryLabel(entry->category) : "other";
            snprintf(plainBuf, sizeof(plainBuf), "[%d] %-26.26s (%s)", i + 1, procs[i], cat);
            snprintf(styleBuf, sizeof(styleBuf), "[%d] %-26.26s %s(%s)%s",
                     i + 1, procs[i], KDIM, cat, KNRM);
            print_box_line(W, plainBuf, styleBuf);
        }
    }

    printf("  └────────────────────────────────────────────────────────┘\n");
    printf("  %s[1-9] Select  [r] Refresh  [c] Clear  [h] Hide  [q] Quit%s\n", KDIM, KNRM);
    fflush(stdout);
}