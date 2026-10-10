/* Key binds (NF_OVERLAY=1; nf_binds.h): the store both games share and the Key
 * Binds screen, drawn over the game like the F10 menu. Included by lean_overlay.inc
 * (this file is the same in nightfire-driving-native and nightfire-port-native).
 *
 * The screen opens from Action's Options > Audio/Video > PC Options > Key Binds
 * (nf_overlay_open_binds, pcg_menu.c) or from the F10 menu's "Key binds" row in
 * either game. Two lists, On foot (Action) and Driving, two keys per action.
 * Keyboard: Up/Down pick an action, Left/Right key 1 or key 2, Enter changes it
 * (then press the new key or mouse button; Esc cancels), Delete clears it, Tab
 * shows the other list, Esc goes back. Mouse: pointing selects, a click changes
 * the key under the pointer, the wheel scrolls, right click goes back.
 * A key another action already uses opens a box: Remove from that action, Swap,
 * Use both (both actions on one key) or Cancel. Esc, F1, F5, F6, F9, F10, `, Alt,
 * the Windows keys and Print Screen are kept for their own jobs and can't be bound.
 * Every change is saved at once to %LOCALAPPDATA%\NightfirePC\key-binds.ini; each
 * game re-reads that file when the other one changed it.
 *
 * The page and its key pictures (dark glossy keycaps with a gold rim and legend,
 * a mouse with the meant button in gold) are drawn into a bitmap with GDI and
 * plain pixel code, re-drawn only when something on it changes. */
#include "nf_binds.h"

typedef struct { const char *id, *name, *help; unsigned char def[2]; } NfbAction;
#define NFB_MAX 24
static const NfbAction nfb_foot[NFA_COUNT] = {
    {"forward", "Move forward", "Walk forward.", {'W', 0}},
    {"back", "Move back", "Walk backwards.", {'S', 0}},
    {"left", "Strafe left", "Step to the left.", {'A', 0}},
    {"right", "Strafe right", "Step to the right.", {'D', 0}},
    {"fire", "Fire", "Fire the weapon in your hand.", {VK_LBUTTON, 0}},
    {"aim", "Aim / zoom", "Aim, or zoom in with a scope.", {VK_RBUTTON, 'T'}},
    {"use", "Use / reload", "Use, open, pick up and reload: one button on the Xbox.", {'E', 'R'}},
    {"jump", "Jump", "Jump.", {VK_SPACE, 0}},
    {"crouch", "Crouch", "Crouch.", {VK_CONTROL, 'C'}},
    {"altfire", "Alternate fire", "The weapon's second fire mode.", {'F', 0}},
    {"weapon", "Next weapon", "Switch to the next weapon.", {'Q', 0}},
    {"gadget", "Next gadget", "Switch to the next gadget.", {'G', 0}},
    {"objectives", "Objectives", "Show the mission objectives (Xbox Back).", {VK_TAB, 0}},
    {"pause", "Pause", "Pause the game (Xbox Start).", {VK_RETURN, 0}},
    {"select", "Menu: select", "Select in menus (Xbox A; also A while playing).", {'Z', 0}},
    {"menuback", "Menu: back", "Go back in menus (Xbox B; also B while playing).", {'X', 0}},
    {"dup", "D-pad up", "Move in menus (Xbox D-pad).", {VK_UP, 0}},
    {"ddown", "D-pad down", "Move in menus (Xbox D-pad).", {VK_DOWN, 0}},
    {"dleft", "D-pad left", "Move in menus (Xbox D-pad).", {VK_LEFT, 0}},
    {"dright", "D-pad right", "Move in menus (Xbox D-pad).", {VK_RIGHT, 0}},
};
/* Driving: one row per pad input. The help lines come from the game's own control
 * files (data\Control\drivecfg.def for the car, povcfg0.def for the sniper scenes). */
static const NfbAction nfb_drive[NFD_COUNT] = {
    {"steerleft", "Steer left", "Left stick left: steer the car.", {'A', 0}},
    {"steerright", "Steer right", "Left stick right: steer the car.", {'D', 0}},
    {"stickup", "Left stick up", "Left stick up.", {'W', 0}},
    {"stickdown", "Left stick down", "Left stick down.", {'S', 0}},
    {"rt", "Accelerate / fire", "Right trigger. Car: accelerate. Sniper: fire.", {VK_LBUTTON, 0}},
    {"lt", "Brake / zoom", "Left trigger. Car: brake. Sniper: zoom.", {VK_RBUTTON, 'T'}},
    {"a", "Fire car weapon", "Xbox A. Car: fire the chosen weapon.", {'E', 'R'}},
    {"b", "Handbrake", "Xbox B. Car: handbrake. Sniper: change weapon.", {'Q', 0}},
    {"x", "Gadget / reload", "Xbox X. Car: gadget. Sniper: reload.", {VK_CONTROL, 'C'}},
    {"y", "Camera", "Xbox Y. Car: change camera.", {VK_SPACE, 0}},
    {"white", "Car weapon up", "Xbox White: change the car weapon.", {'F', 0}},
    {"black", "Car weapon down", "Xbox Black: change the car weapon the other way.", {'G', 0}},
    {"lsclick", "Left stick click", "Left stick click.", {VK_SHIFT, 0}},
    {"rsclick", "Right stick click", "Right stick click.", {'V', 0}},
    {"back", "Back button", "Xbox Back. Sniper: resume driving.", {VK_TAB, 0}},
    {"start", "Pause", "Pause the game (Xbox Start).", {VK_RETURN, 0}},
    {"select", "Menu: select", "Select in menus (Xbox A; also the car weapon).", {'Z', 0}},
    {"menuback", "Menu: back", "Go back in menus (Xbox B; also the handbrake).", {'X', 0}},
    {"dup", "D-pad up", "Menus. Sniper: zoom in.", {VK_UP, 0}},
    {"ddown", "D-pad down", "Menus. Sniper: zoom out.", {VK_DOWN, 0}},
    {"dleft", "D-pad left", "Menus. Also changes weapon.", {VK_LEFT, 0}},
    {"dright", "D-pad right", "Menus. Also changes weapon.", {VK_RIGHT, 0}},
};
static const char *const nfb_tab_names[NFB_GAMES] = {"On foot", "Driving"};
static const char *const nfb_file_prefix[NFB_GAMES] = {"foot", "drive"};
#if defined(DRIVING_LEAN_RENDERER)
#define NFB_THIS_GAME NFB_DRIVING
#else
#define NFB_THIS_GAME NFB_ACTION
#endif
static const NfbAction *nfb_list(int g, int *n)
{
    if (g == NFB_DRIVING) { *n = NFD_COUNT; return nfb_drive; }
    *n = NFA_COUNT; return nfb_foot;
}

/* ------------------------------------------------------------ keys */
static unsigned nfb_grave_vk(void) { return MapVirtualKeyA(0x29, MAPVK_VSC_TO_VK); }   /* the key left of 1, on this layout */
/* Keys kept for their own jobs: the reason, or NULL when the key can be bound. */
static const char *nfb_reserved(unsigned c)
{
    switch (c) {
    case VK_ESCAPE: return "Esc is kept for going back and freeing the mouse.";
    case VK_F1: return "F1 is kept for capturing and freeing the mouse.";
    case VK_F5: case VK_F6: return "F5 and F6 are kept for moving the keyboard between split-screen players.";
    case VK_F9: return "F9 is kept for saving a mark.";
    case VK_F10: return "F10 is kept for the settings menu.";
    case VK_MENU: case VK_LMENU: case VK_RMENU: return "Alt is kept for Windows.";
    case VK_LWIN: case VK_RWIN: return "The Windows key is kept for Windows.";
    case VK_SNAPSHOT: return "Print Screen is kept for Windows.";
    }
    if (c && c == nfb_grave_vk()) return "The ` key is kept for the settings menu.";
    return NULL;
}
/* A key's name: full ("Left mouse", "Page Up") or short for a keycap ("PgUp"). */
static void nfb_key_name(unsigned c, char *o, size_t n, int keycap)
{
    static const struct { unsigned char vk; const char *full, *cap; } names[] = {
        {VK_LBUTTON, "Left mouse", "Left"}, {VK_RBUTTON, "Right mouse", "Right"}, {VK_MBUTTON, "Middle mouse", "Middle"},
        {VK_XBUTTON1, "Mouse 4", "Mouse 4"}, {VK_XBUTTON2, "Mouse 5", "Mouse 5"},
        {NFB_WHEEL_UP, "Wheel up", "Wheel up"}, {NFB_WHEEL_DOWN, "Wheel down", "Wheel down"},
        {VK_SPACE, "Space", "Space"}, {VK_RETURN, "Enter", "Enter"}, {VK_TAB, "Tab", "Tab"}, {VK_SHIFT, "Shift", "Shift"},
        {VK_CONTROL, "Ctrl", "Ctrl"}, {VK_BACK, "Backspace", "Bksp"}, {VK_DELETE, "Delete", "Del"}, {VK_INSERT, "Insert", "Ins"},
        {VK_HOME, "Home", "Home"}, {VK_END, "End", "End"}, {VK_PRIOR, "Page Up", "PgUp"}, {VK_NEXT, "Page Down", "PgDn"},
        {VK_UP, "Up arrow", "Up"}, {VK_DOWN, "Down arrow", "Down"}, {VK_LEFT, "Left arrow", "Left"}, {VK_RIGHT, "Right arrow", "Right"},
        {VK_CAPITAL, "Caps Lock", "Caps"}, {VK_NUMLOCK, "Num Lock", "NumLk"}, {VK_SCROLL, "Scroll Lock", "ScrLk"},
        {VK_PAUSE, "Pause", "Pause"}, {VK_APPS, "Menu key", "Menu"}, {VK_ESCAPE, "Esc", "Esc"},
        {VK_MULTIPLY, "Num *", "Num *"}, {VK_ADD, "Num +", "Num +"}, {VK_SUBTRACT, "Num -", "Num -"},
        {VK_DECIMAL, "Num .", "Num ."}, {VK_DIVIDE, "Num /", "Num /"}, {VK_CLEAR, "Num 5", "Num 5"},
    };
    if (!c) { snprintf(o, n, "-"); return; }
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++)
        if (names[i].vk == c) { snprintf(o, n, "%s", keycap ? names[i].cap : names[i].full); return; }
    if (c >= VK_NUMPAD0 && c <= VK_NUMPAD9) { snprintf(o, n, "Num %u", c - VK_NUMPAD0); return; }
    if (c >= VK_F1 && c <= VK_F24) { snprintf(o, n, "F%u", c - VK_F1 + 1); return; }
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) { snprintf(o, n, "%c", (char)c); return; }
    {   /* other keys: the character this layout gives (letters follow each player's layout) */
        UINT ch = MapVirtualKeyA(c, MAPVK_VK_TO_CHAR) & 0x7FFFu;
        if (ch > 32 && ch < 256) { snprintf(o, n, "%c", (char)ch); return; }
        UINT sc = MapVirtualKeyA(c, MAPVK_VK_TO_VSC);
        char k[64];
        if (sc && GetKeyNameTextA((LONG)(sc << 16), k, sizeof k) > 0) { snprintf(o, n, "%s", k); return; }
    }
    snprintf(o, n, "Key %02X", c);
}
static int nfb_is_mouse(unsigned c)
{
    return c == VK_LBUTTON || c == VK_RBUTTON || c == VK_MBUTTON || c == VK_XBUTTON1 || c == VK_XBUTTON2 ||
           c == NFB_WHEEL_UP || c == NFB_WHEEL_DOWN;
}

/* ------------------------------------------------------------ the store */
static volatile unsigned char nfb_map[NFB_GAMES][NFB_MAX][2];
static SRWLOCK nfb_lock = SRWLOCK_INIT;                 /* file reads and writes, changes from the screen */
static volatile LONG nfb_loaded, nfb_version;           /* nfb_version: anything on the screen changed */
static FILETIME nfb_file_time; static ULONGLONG nfb_next_check;

static void nfb_path(char *out, size_t n)
{
    const char *base = getenv("LOCALAPPDATA");
    snprintf(out, n, "%s\\NightfirePC", base && *base ? base : ".");
    CreateDirectoryA(out, NULL);
    size_t l = strlen(out); snprintf(out + l, n - l, "\\key-binds.ini");
}
static int nfb_file_stamp(FILETIME *t)
{
    char path[MAX_PATH]; nfb_path(path, sizeof path);
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a)) return 0;
    *t = a.ftLastWriteTime; return 1;
}
static void nfb_defaults(int g)
{
    int n; const NfbAction *L = nfb_list(g, &n);
    for (int a = 0; a < n; a++) { nfb_map[g][a][0] = L[a].def[0]; nfb_map[g][a][1] = L[a].def[1]; }
}
static void nfb_read_file(void)   /* under nfb_lock */
{
    for (int g = 0; g < NFB_GAMES; g++) nfb_defaults(g);
    char path[MAX_PATH]; nfb_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    unsigned read = 0;
    if (f) {
        char line[160];
        while (fgets(line, sizeof line, f)) {
            char *eq = strchr(line, '='), *dot = strchr(line, '.');
            if (!eq || !dot || dot > eq || line[0] == '#') continue;
            *eq = 0; *dot = 0;
            unsigned k0 = 0, k1 = 0;
            if (sscanf(eq + 1, "%x,%x", &k0, &k1) < 1) continue;
            for (int g = 0; g < NFB_GAMES; g++) {
                if (strcmp(line, nfb_file_prefix[g])) continue;
                int n; const NfbAction *L = nfb_list(g, &n);
                for (int a = 0; a < n; a++) if (!strcmp(dot + 1, L[a].id)) {
                    nfb_map[g][a][0] = (unsigned char)(k0 < 256 && !nfb_reserved(k0) ? k0 : 0);
                    nfb_map[g][a][1] = (unsigned char)(k1 < 256 && !nfb_reserved(k1) ? k1 : 0);
                    read++;
                }
            }
        }
        fclose(f);
    }
    if (!nfb_file_stamp(&nfb_file_time)) memset(&nfb_file_time, 0, sizeof nfb_file_time);
    InterlockedIncrement(&nfb_version);
    LOG("[NF-BINDS] key binds %s %s (%u actions set there; the rest are the defaults)\n", f ? "read from" : "default; will be saved to", path, read);
}
static void nfb_write_file(void)   /* under nfb_lock */
{
    char path[MAX_PATH]; nfb_path(path, sizeof path);
    FILE *f = fopen(path, "w");
    if (!f) { LOG("[NF-BINDS] could not save %s\n", path); return; }
    fprintf(f, "# Nightfire PC key binds (the Key Binds page writes this file).\n"
               "# list.action=key1,key2 as Windows key codes in hex; 0 = no key.\n");
    for (int g = 0; g < NFB_GAMES; g++) {
        int n; const NfbAction *L = nfb_list(g, &n);
        for (int a = 0; a < n; a++) {
            char k0[32], k1[32];
            nfb_key_name(nfb_map[g][a][0], k0, sizeof k0, 0); nfb_key_name(nfb_map[g][a][1], k1, sizeof k1, 0);
            fprintf(f, "%s.%s=%02X,%02X ; %s: %s, %s\n", nfb_file_prefix[g], L[a].id, nfb_map[g][a][0], nfb_map[g][a][1], L[a].name, k0, k1);
        }
    }
    fclose(f);
    nfb_file_stamp(&nfb_file_time);
}
static void nfb_load(void)
{
    if (nfb_loaded) return;
    AcquireSRWLockExclusive(&nfb_lock);
    if (!nfb_loaded) { nfb_read_file(); nfb_next_check = GetTickCount64() + 1000; InterlockedExchange(&nfb_loaded, 1); }
    ReleaseSRWLockExclusive(&nfb_lock);
}
int nf_binds_on(void)
{
    if (!nfo_enabled()) return 0;
    nfb_load();
    ULONGLONG now = GetTickCount64();
    /* The other game may have changed the file: look once a second, never waiting
     * on the screen's lock (this runs on the game's input path). */
    if (now >= nfb_next_check && TryAcquireSRWLockExclusive(&nfb_lock)) {
        nfb_next_check = now + 1000;
        FILETIME t;
        if (nfb_file_stamp(&t) && CompareFileTime(&t, &nfb_file_time)) nfb_read_file();
        ReleaseSRWLockExclusive(&nfb_lock);
    }
    return 1;
}
void nf_binds_get(int game, int action, unsigned char out[2])
{
    out[0] = out[1] = 0;
    if (game < 0 || game >= NFB_GAMES || action < 0 || action >= NFB_MAX) return;
    nfb_load();
    out[0] = nfb_map[game][action][0]; out[1] = nfb_map[game][action][1];
}
/* Mouse wheel: each notch is a press of about 60 ms with 40 ms between presses. */
static SRWLOCK nfb_wheel_lock = SRWLOCK_INIT;
static int nfb_wheel_queue[2], nfb_wheel_dir = -1;
static ULONGLONG nfb_wheel_until, nfb_wheel_gap;
void nf_binds_wheel(int notches)
{
    if (!notches) return;
    AcquireSRWLockExclusive(&nfb_wheel_lock);
    int *q = &nfb_wheel_queue[notches > 0 ? 0 : 1];
    *q += notches > 0 ? notches : -notches; if (*q > 4) *q = 4;
    ReleaseSRWLockExclusive(&nfb_wheel_lock);
}
int nf_binds_wheel_down(int code)
{
    AcquireSRWLockExclusive(&nfb_wheel_lock);
    ULONGLONG now = GetTickCount64();
    if (nfb_wheel_dir >= 0 && now >= nfb_wheel_until) { nfb_wheel_dir = -1; nfb_wheel_gap = now + 40; }
    if (nfb_wheel_dir < 0 && now >= nfb_wheel_gap) {
        for (int d = 0; d < 2; d++) if (nfb_wheel_queue[d]) { nfb_wheel_queue[d]--; nfb_wheel_dir = d; nfb_wheel_until = now + 60; break; }
    }
    int down = nfb_wheel_dir >= 0 && nfb_wheel_dir == (code == NFB_WHEEL_DOWN);
    ReleaseSRWLockExclusive(&nfb_wheel_lock);
    return down;
}

/* ------------------------------------------------------------ the screen: state */
enum { NFB_BROWSE, NFB_LISTEN, NFB_CLASH, NFB_RESET };
enum { NFB_OPT_REMOVE, NFB_OPT_SWAP, NFB_OPT_BOTH, NFB_OPT_CANCEL, NFB_OPT_RESET };
#define NFB_VIS 11                       /* rows shown at once */
static volatile LONG nfb_tab, nfb_sel, nfb_slot, nfb_top, nfb_mode, nfb_choice, nfb_from_menu;
/* Under nfb_lock: the clash being asked about and the message line. */
static unsigned char nfb_pending, nfb_old;
static int nfb_clash_a, nfb_clash_s;
static char nfb_msg[160]; static ULONGLONG nfb_msg_until;

static int nfb_rows(int tab) { int n; nfb_list(tab, &n); return n + 1; }   /* the actions, then "Reset to defaults" */
static int nfb_listening(void) { return nfb_mode == NFB_LISTEN; }
static void nfb_bump(void) { InterlockedIncrement(&nfb_version); }
static void nfb_say(const char *s)
{
    AcquireSRWLockExclusive(&nfb_lock);
    snprintf(nfb_msg, sizeof nfb_msg, "%s", s); nfb_msg_until = GetTickCount64() + 3500;
    ReleaseSRWLockExclusive(&nfb_lock);
    nfb_bump();
}
static void nfb_show(LONG sel)   /* scroll so row sel is on screen */
{
    LONG top = nfb_top;
    if (sel < top) top = sel;
    if (sel >= top + NFB_VIS) top = sel - NFB_VIS + 1;
    InterlockedExchange(&nfb_top, top < 0 ? 0 : top);
}
static void nfb_open(int from_menu)
{
    nfb_load();
    AcquireSRWLockExclusive(&nfb_lock); nfb_read_file(); nfb_msg_until = 0; ReleaseSRWLockExclusive(&nfb_lock);
    InterlockedExchange(&nfb_tab, NFB_THIS_GAME); InterlockedExchange(&nfb_sel, 0); InterlockedExchange(&nfb_slot, 0);
    InterlockedExchange(&nfb_top, 0); InterlockedExchange(&nfb_mode, NFB_BROWSE); InterlockedExchange(&nfb_from_menu, from_menu);
    InterlockedExchange(&nfo_page, 1);
    nfb_bump();
    LOG("[NF-BINDS] Key Binds screen opened (%s)\n", from_menu ? "PC Options" : "F10 menu");
}
void nf_overlay_open_binds(void)
{
    if (!nfo_enabled()) return;
    nfb_open(1);
    InterlockedExchange(&nf_overlay_menu_open, 1);
}
static void nfb_close(void)   /* Esc / right click: back to where it was opened from */
{
    InterlockedExchange(&nfb_mode, NFB_BROWSE);
    InterlockedExchange(&nfo_page, 0);
    if (nfb_from_menu) InterlockedExchange(&nf_overlay_menu_open, 0);
    LOG("[NF-BINDS] Key Binds screen closed\n");
}
/* Another action (not `a`) in list g using key c: 1 and where, else 0. */
static int nfb_find(int g, int a, unsigned c, int *fa, int *fs)
{
    int n; nfb_list(g, &n);
    for (int b = 0; b < n; b++) if (b != a) for (int s = 0; s < 2; s++)
        if (nfb_map[g][b][s] == c) { *fa = b; *fs = s; return 1; }
    return 0;
}
static void nfb_log_change(int g, int a, const char *why)
{
    int n; const NfbAction *L = nfb_list(g, &n);
    char k0[32], k1[32]; nfb_key_name(nfb_map[g][a][0], k0, sizeof k0, 0); nfb_key_name(nfb_map[g][a][1], k1, sizeof k1, 0);
    LOG("[NF-BINDS] %s %s: %s, %s (%s; saved)\n", nfb_tab_names[g], L[a].name, k0, k1, why);
}
/* The new key for the selected slot (window thread, listening). */
static void nfb_bind(unsigned code)
{
    const char *r = nfb_reserved(code);
    if (r) { nfb_say(r); return; }              /* keep listening */
    int g = (int)nfb_tab, a = (int)nfb_sel, s = (int)nfb_slot, n, fa, fs;
    nfb_list(g, &n);
    if (a < 0 || a >= n) { InterlockedExchange(&nfb_mode, NFB_BROWSE); return; }
    AcquireSRWLockExclusive(&nfb_lock);
    unsigned char old = nfb_map[g][a][s];
    if (old == code) { ReleaseSRWLockExclusive(&nfb_lock); InterlockedExchange(&nfb_mode, NFB_BROWSE); nfb_bump(); return; }
    if (nfb_map[g][a][!s] == code) {             /* already this action's other key: the two keys swap places */
        nfb_map[g][a][!s] = old; nfb_map[g][a][s] = (unsigned char)code;
        nfb_write_file(); nfb_log_change(g, a, "keys swapped");
    } else if (nfb_find(g, a, code, &fa, &fs)) { /* another action has it: ask */
        nfb_pending = (unsigned char)code; nfb_old = old; nfb_clash_a = fa; nfb_clash_s = fs;
        ReleaseSRWLockExclusive(&nfb_lock);
        InterlockedExchange(&nfb_choice, 0); InterlockedExchange(&nfb_mode, NFB_CLASH); nfb_bump();
        return;
    } else {
        nfb_map[g][a][s] = (unsigned char)code;
        nfb_write_file(); nfb_log_change(g, a, "new key");
    }
    ReleaseSRWLockExclusive(&nfb_lock);
    InterlockedExchange(&nfb_mode, NFB_BROWSE); nfb_bump();
}
/* The clash box's choices: Remove, Swap (only when the slot had a key), Use both, Cancel. */
static int nfb_options(int *o)
{
    int n = 0;
    if (nfb_mode == NFB_RESET) { o[n++] = NFB_OPT_RESET; o[n++] = NFB_OPT_CANCEL; return n; }
    o[n++] = NFB_OPT_REMOVE;
    if (nfb_old && nfb_old != nfb_pending) o[n++] = NFB_OPT_SWAP;
    o[n++] = NFB_OPT_BOTH; o[n++] = NFB_OPT_CANCEL;
    return n;
}
static void nfb_pick(int opt)
{
    int g = (int)nfb_tab, a = (int)nfb_sel, s = (int)nfb_slot, again = 0;
    AcquireSRWLockExclusive(&nfb_lock);
    if (opt == NFB_OPT_RESET) {
        nfb_defaults(g); nfb_write_file();
        LOG("[NF-BINDS] %s keys reset to the defaults (saved)\n", nfb_tab_names[g]);
    } else if (opt != NFB_OPT_CANCEL) {
        int b = nfb_clash_a, t = nfb_clash_s;
        if (opt == NFB_OPT_REMOVE) nfb_map[g][b][t] = 0;
        else if (opt == NFB_OPT_SWAP) nfb_map[g][b][t] = nfb_old;
        nfb_map[g][a][s] = nfb_pending;
        nfb_write_file();
        nfb_log_change(g, a, opt == NFB_OPT_BOTH ? "use both" : opt == NFB_OPT_SWAP ? "swapped" : "removed from the other action");
        nfb_log_change(g, b, "the other action");
        /* Still on a third action (an earlier "Use both"): ask about that one too. */
        int fa, fs;
        if (opt != NFB_OPT_BOTH && nfb_find(g, a, nfb_pending, &fa, &fs) && fa != b) {
            nfb_old = 0; nfb_clash_a = fa; nfb_clash_s = fs; again = 1;
        }
    }
    ReleaseSRWLockExclusive(&nfb_lock);
    InterlockedExchange(&nfb_choice, 0);
    InterlockedExchange(&nfb_mode, again ? NFB_CLASH : NFB_BROWSE);
    nfb_bump();
}

/* Hit areas of the drawn page (present thread writes, window thread reads). */
enum { NFB_HIT_TAB = 1, NFB_HIT_SLOT, NFB_HIT_ROW, NFB_HIT_OPT };
typedef struct { int x, y, w, h, kind, a, b; } NfbHit;
#define NFB_HITS 96
static SRWLOCK nfb_hit_lock = SRWLOCK_INIT;
static NfbHit nfb_hits[NFB_HITS]; static int nfb_nhits, nfb_org_x, nfb_org_y, nfb_drawn_w, nfb_drawn_h;
static int nfb_hit(HWND wnd, int cx, int cy, NfbHit *out)   /* client pixels; later areas win (slots over rows) */
{
    int found = 0;
    RECT r;   /* the picture's size can lag the window's for a frame: scale to it */
    AcquireSRWLockShared(&nfb_hit_lock);
    if (GetClientRect(wnd, &r) && r.right > 0 && r.bottom > 0 && nfb_drawn_w > 0 && nfb_drawn_h > 0) {
        cx = (int)((long long)cx * nfb_drawn_w / r.right); cy = (int)((long long)cy * nfb_drawn_h / r.bottom);
    }
    int x = cx - nfb_org_x, y = cy - nfb_org_y;
    for (int i = nfb_nhits - 1; i >= 0; i--) {
        NfbHit *h = &nfb_hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) { *out = *h; found = 1; break; }
    }
    ReleaseSRWLockShared(&nfb_hit_lock);
    return found;
}

/* Window thread: every message while the screen is up. Returns 1 when used (the
 * game must not see it), 0 to pass it on (key and button releases, so nothing
 * stays held in the game). */
static int nfb_eat(UINT m)
{
    return !(m == WM_KEYUP || m == WM_LBUTTONUP || m == WM_RBUTTONUP || m == WM_MBUTTONUP || m == WM_XBUTTONUP) &&
           (m == WM_KEYDOWN || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP || m == WM_CHAR || m == WM_SYSCHAR ||
            m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN ||
            m == WM_XBUTTONDOWN || m == WM_MOUSEWHEEL || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDBLCLK ||
            m == WM_MBUTTONDBLCLK || m == WM_XBUTTONDBLCLK || m == WM_INPUT);
}
static int nfb_key(HWND h, UINT m, WPARAM w, LPARAM l)
{
    LONG mode = nfb_mode;
    int down = (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && !(l & (1 << 30));
    NfbHit hit; int over = 0;
    if (m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MOUSEWHEEL)
        over = nfb_hit(h, (short)LOWORD(l), (short)HIWORD(l), &hit);
    if (m == WM_MOUSEWHEEL) {   /* wheel positions are screen pixels */
        POINT p = {(short)LOWORD(l), (short)HIWORD(l)}; ScreenToClient(h, &p);
        over = nfb_hit(h, p.x, p.y, &hit);
    }
    if (mode == NFB_LISTEN) {
        unsigned code = 0;
        if (down) code = (unsigned)w & 0xFF;
        else if (m == WM_LBUTTONDOWN) code = VK_LBUTTON;
        else if (m == WM_RBUTTONDOWN) code = VK_RBUTTON;
        else if (m == WM_MBUTTONDOWN) code = VK_MBUTTON;
        else if (m == WM_XBUTTONDOWN) code = GET_XBUTTON_WPARAM(w) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        else if (m == WM_MOUSEWHEEL) code = GET_WHEEL_DELTA_WPARAM(w) > 0 ? NFB_WHEEL_UP : NFB_WHEEL_DOWN;
        if (code == VK_ESCAPE) { InterlockedExchange(&nfb_mode, NFB_BROWSE); nfb_bump(); }
        else if (code && code != VK_PROCESSKEY && code != 0xFF) nfb_bind(code);
        return nfb_eat(m);
    }
    if (mode == NFB_CLASH || mode == NFB_RESET) {
        int o[5], n = nfb_options(o); LONG c = nfb_choice;
        if (down) switch (w) {
        case VK_UP: InterlockedExchange(&nfb_choice, (c + n - 1) % n); nfb_bump(); break;
        case VK_DOWN: case VK_TAB: InterlockedExchange(&nfb_choice, (c + 1) % n); nfb_bump(); break;
        case VK_RETURN: case VK_SPACE: nfb_pick(o[c >= 0 && c < n ? c : 0]); break;
        case VK_ESCAPE: nfb_pick(NFB_OPT_CANCEL); break;
        }
        if (over && hit.kind == NFB_HIT_OPT && hit.a >= 0 && hit.a < n) {
            if (m == WM_MOUSEMOVE && c != hit.a) { InterlockedExchange(&nfb_choice, hit.a); nfb_bump(); }
            if (m == WM_LBUTTONDOWN) nfb_pick(o[hit.a]);
        }
        if (m == WM_RBUTTONDOWN) nfb_pick(NFB_OPT_CANCEL);
        return nfb_eat(m);
    }
    /* browsing */
    int rows = nfb_rows((int)nfb_tab), n = rows - 1;
    LONG sel = nfb_sel;
    if (down) switch (w) {
    case VK_UP: sel = (sel + rows - 1) % rows; InterlockedExchange(&nfb_sel, sel); nfb_show(sel); nfb_bump(); break;
    case VK_DOWN: sel = (sel + 1) % rows; InterlockedExchange(&nfb_sel, sel); nfb_show(sel); nfb_bump(); break;
    case VK_LEFT: case VK_RIGHT: InterlockedExchange(&nfb_slot, !nfb_slot); nfb_bump(); break;
    case VK_TAB: case VK_PRIOR: case VK_NEXT:
        InterlockedExchange(&nfb_tab, !nfb_tab); InterlockedExchange(&nfb_sel, 0); InterlockedExchange(&nfb_top, 0); nfb_bump(); break;
    case VK_RETURN: case VK_SPACE:
        if (sel >= n) { InterlockedExchange(&nfb_choice, 1); InterlockedExchange(&nfb_mode, NFB_RESET); }   /* Cancel highlighted first */
        else InterlockedExchange(&nfb_mode, NFB_LISTEN);
        nfb_bump(); break;
    case VK_DELETE: case VK_BACK:
        if (sel < n) {
            int g = (int)nfb_tab;
            AcquireSRWLockExclusive(&nfb_lock);
            if (nfb_map[g][sel][nfb_slot]) { nfb_map[g][sel][nfb_slot] = 0; nfb_write_file(); nfb_log_change(g, (int)sel, "cleared"); }
            ReleaseSRWLockExclusive(&nfb_lock);
            nfb_bump();
        }
        break;
    case VK_ESCAPE: nfb_close(); break;
    }
    if (over && (m == WM_MOUSEMOVE || m == WM_LBUTTONDOWN)) {
        if ((hit.kind == NFB_HIT_SLOT || hit.kind == NFB_HIT_ROW) && (hit.a != sel || (hit.kind == NFB_HIT_SLOT && hit.b != nfb_slot))) {
            InterlockedExchange(&nfb_sel, hit.a);
            if (hit.kind == NFB_HIT_SLOT) InterlockedExchange(&nfb_slot, hit.b);
            nfb_bump();
        }
        if (m == WM_LBUTTONDOWN) {
            if (hit.kind == NFB_HIT_TAB && hit.a != nfb_tab) {
                InterlockedExchange(&nfb_tab, hit.a); InterlockedExchange(&nfb_sel, 0); InterlockedExchange(&nfb_top, 0); nfb_bump();
            } else if (hit.kind == NFB_HIT_SLOT) { InterlockedExchange(&nfb_mode, NFB_LISTEN); nfb_bump(); }
            else if (hit.kind == NFB_HIT_ROW && hit.a >= n) { InterlockedExchange(&nfb_choice, 1); InterlockedExchange(&nfb_mode, NFB_RESET); nfb_bump(); }
        }
    }
    if (m == WM_MOUSEWHEEL) {
        LONG top = nfb_top - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA, most = rows - NFB_VIS;
        if (top > most) top = most;
        if (top < 0) top = 0;
        if (top != nfb_top) { InterlockedExchange(&nfb_top, top); nfb_bump(); }
    }
    if (m == WM_RBUTTONDOWN) nfb_close();
    return nfb_eat(m);
}

/* ------------------------------------------------------------ the screen: drawing */
typedef struct {
    HDC dc; uint32_t *px; int w, h, flush;
    float f;                                         /* text size unit, pixels */
    HFONT name, bold, head, help, title, dlg, leg1, leg2;
} NfbCtx;
typedef struct { int r, g, b; } NfbRgb;
static const NfbRgb NFB_GOLD_HI = {236, 196, 92}, NFB_GOLD_LO = {150, 104, 18}, NFB_RIM = {150, 120, 60};
static float nfb_clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static void nfb_flush(NfbCtx *x) { if (x->flush) { GdiFlush(); x->flush = 0; } }
static void nfb_put(NfbCtx *x, int px, int py, NfbRgb c, float a)
{
    if (px < 0 || py < 0 || px >= x->w || py >= x->h || a <= 0) return;
    if (a > 1) a = 1;
    uint32_t *p = &x->px[py * x->w + px];
    int r = (int)((*p >> 16) & 255), g = (int)((*p >> 8) & 255), b = (int)(*p & 255);
    r += (int)((c.r - r) * a); g += (int)((c.g - g) * a); b += (int)((c.b - b) * a);
    *p = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static NfbRgb nfb_mix(NfbRgb a, NfbRgb b, float t)
{
    NfbRgb o = {a.r + (int)((b.r - a.r) * t), a.g + (int)((b.g - a.g) * t), a.b + (int)((b.b - a.b) * t)};
    return o;
}
static void nfb_fill(NfbCtx *x, float x0, float y0, float w, float h, NfbRgb c, float a)
{
    nfb_flush(x);
    for (int py = (int)y0; py < (int)(y0 + h); py++) for (int px = (int)x0; px < (int)(x0 + w); px++) nfb_put(x, px, py, c, a);
}
static void nfb_frame(NfbCtx *x, float x0, float y0, float w, float h, int t, NfbRgb c)
{
    nfb_fill(x, x0, y0, w, (float)t, c, 1); nfb_fill(x, x0, y0 + h - t, w, (float)t, c, 1);
    nfb_fill(x, x0, y0, (float)t, h, c, 1); nfb_fill(x, x0 + w - t, y0, (float)t, h, c, 1);
}
/* Signed distance from a point to a rounded rectangle (negative inside). */
static float nfb_sd(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    float hx = (x1 - x0) * 0.5f - r, hy = (y1 - y0) * 0.5f - r;
    float qx = fabsf(px - (x0 + x1) * 0.5f) - hx, qy = fabsf(py - (y0 + y1) * 0.5f) - hy;
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
}
/* A glossy rounded shape (the game's button look): top-to-bottom gradient, a rim,
 * a soft highlight over the upper half. Optional clip: only inside the rounded
 * rectangle clip[0..4] (x0,y0,x1,y1,r), shrunk by clip_in. */
static void nfb_gem(NfbCtx *x, float x0, float y0, float x1, float y1, float r, NfbRgb top, NfbRgb bot, NfbRgb rim,
                    float rimw, float hi, const float *clip, float clip_in)
{
    nfb_flush(x);
    float ex = (x0 + x1) * 0.5f, ey = y0 + (y1 - y0) * 0.3f, rx = (x1 - x0) * 0.38f, ry = (y1 - y0) * 0.25f;
    for (int py = (int)floorf(y0); py < (int)ceilf(y1); py++) for (int px = (int)floorf(x0); px < (int)ceilf(x1); px++) {
        float cx = px + 0.5f, cy = py + 0.5f;
        float sd = nfb_sd(cx, cy, x0, y0, x1, y1, r), cov = nfb_clamp01(0.5f - sd);
        if (clip) cov *= nfb_clamp01(0.5f - (nfb_sd(cx, cy, clip[0], clip[1], clip[2], clip[3], clip[4]) + clip_in));
        if (cov <= 0) continue;
        NfbRgb c = nfb_mix(top, bot, nfb_clamp01((cy - y0) / (y1 - y0)));
        if (rimw > 0) c = nfb_mix(c, rim, nfb_clamp01(rimw + 0.5f + sd));
        float e = ((cx - ex) / rx) * ((cx - ex) / rx) + ((cy - ey) / ry) * ((cy - ey) / ry);
        if (hi > 0 && e < 1 && -sd > rimw) { NfbRgb wht = {255, 255, 255}; c = nfb_mix(c, wht, hi * (1 - e)); }
        nfb_put(x, px, py, c, cov);
    }
}
static void nfb_tri(NfbCtx *x, float ax, float ay, float bx, float by, float cx, float cy, NfbRgb c)
{
    nfb_flush(x);
    float minx = fminf(ax, fminf(bx, cx)), maxx = fmaxf(ax, fmaxf(bx, cx)), miny = fminf(ay, fminf(by, cy)), maxy = fmaxf(ay, fmaxf(by, cy));
    float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area == 0) return;
    for (int py = (int)floorf(miny); py <= (int)ceilf(maxy); py++) for (int px = (int)floorf(minx); px <= (int)ceilf(maxx); px++) {
        int in = 0;
        for (int s = 0; s < 9; s++) {   /* 3x3 samples */
            float sx = px + (s % 3 + 0.5f) / 3, sy = py + (s / 3 + 0.5f) / 3;
            float w0 = ((bx - sx) * (cy - sy) - (by - sy) * (cx - sx)) / area, w1 = ((cx - sx) * (ay - sy) - (cy - sy) * (ax - sx)) / area;
            if (w0 >= 0 && w1 >= 0 && w0 + w1 <= 1) in++;
        }
        nfb_put(x, px, py, c, in / 9.0f);
    }
}
static void nfb_text(NfbCtx *x, HFONT f, float x0, float y0, float w, float h, const char *s, NfbRgb c, UINT fmt, int shadow)
{
    SelectObject(x->dc, f);
    RECT r = {(LONG)x0, (LONG)y0, (LONG)(x0 + w), (LONG)(y0 + h)};
    if (shadow) { RECT q = r; OffsetRect(&q, shadow, shadow); SetTextColor(x->dc, RGB(0, 0, 0)); DrawTextA(x->dc, s, -1, &q, fmt | DT_NOPREFIX); }
    SetTextColor(x->dc, RGB(c.r, c.g, c.b));
    DrawTextA(x->dc, s, -1, &r, fmt | DT_NOPREFIX);
    x->flush = 1;
}
static int nfb_textw(NfbCtx *x, HFONT f, const char *s)
{
    SIZE z = {0, 0}; SelectObject(x->dc, f); GetTextExtentPoint32A(x->dc, s, (int)strlen(s), &z); return z.cx;
}
/* A keyboard key: dark glossy rounded square, gold rim, light gold legend (or an arrow). */
static int nfb_keycap(NfbCtx *x, unsigned code, float px, float py, float h)
{
    char lab[32]; nfb_key_name(code, lab, sizeof lab, 1);
    int arrow = code == VK_UP || code == VK_DOWN || code == VK_LEFT || code == VK_RIGHT;
    HFONT lf = (strlen(lab) == 1) ? x->leg1 : x->leg2;
    float w = h * 0.92f;
    if (!arrow) { float tw = (float)nfb_textw(x, lf, lab) + h * 0.55f; if (tw > w) w = tw; }
    float r = h / 5, ins = h / 9;
    NfbRgb t1 = {70, 72, 80}, b1 = {8, 8, 10}, t2 = {58, 60, 68}, b2 = {22, 22, 26}, rim2 = {40, 40, 46};
    nfb_gem(x, px, py, px + w, py + h, r, t1, b1, NFB_RIM, fmaxf(1, h / 14), 0.27f, NULL, 0);
    nfb_gem(x, px + ins, py + ins * 0.7f, px + w - ins, py + h - ins * 1.5f, r * 0.7f, t2, b2, rim2, 1, 0.23f, NULL, 0);
    if (arrow) {
        float cx = px + w / 2, cy = py + h * 0.45f, s = h * 0.15f;
        if (code == VK_UP) nfb_tri(x, cx - s, cy + s * 0.6f, cx + s, cy + s * 0.6f, cx, cy - s * 1.0f, NFB_GOLD_HI);
        else if (code == VK_DOWN) nfb_tri(x, cx - s, cy - s * 0.6f, cx + s, cy - s * 0.6f, cx, cy + s * 1.0f, NFB_GOLD_HI);
        else if (code == VK_LEFT) nfb_tri(x, cx + s * 0.6f, cy - s, cx + s * 0.6f, cy + s, cx - s * 1.0f, cy, NFB_GOLD_HI);
        else nfb_tri(x, cx - s * 0.6f, cy - s, cx - s * 0.6f, cy + s, cx + s * 1.0f, cy, NFB_GOLD_HI);
    } else {
        NfbRgb leg = {232, 212, 168};
        nfb_text(x, lf, px, py - h * 0.05f, w, h * 0.92f, lab, leg, DT_CENTER | DT_VCENTER | DT_SINGLELINE, h >= 24 ? 2 : 1);
    }
    return (int)ceilf(w);
}
/* A mouse: dark glossy body, the meant button (or wheel, or side button) in gold. */
static int nfb_mouse(NfbCtx *x, unsigned code, float px, float py, float h)
{
    float w = h * 0.72f, x1 = px + w, y1 = py + h, rb = w / 2 - 1;
    NfbRgb t1 = {70, 72, 80}, b1 = {8, 8, 10}, grey_t = {110, 110, 118}, grey_b = {40, 40, 46}, dark = {20, 20, 20}, gold_rim = {110, 76, 10};
    float rimw = fmaxf(1, h / 18), body[5] = {px, py, x1, y1, rb};
    nfb_gem(x, px, py, x1, y1, rb, t1, b1, NFB_RIM, rimw, 0.2f, NULL, 0);
    float split = py + h * 0.44f, gap = fmaxf(1, w / 40), cxm = px + w / 2;
    if (code == VK_LBUTTON) nfb_gem(x, px - rb, py - rb, cxm - gap, split, 1, NFB_GOLD_HI, NFB_GOLD_LO, gold_rim, 0, 0.35f, body, rimw);
    if (code == VK_RBUTTON) nfb_gem(x, cxm + gap, py - rb, x1 + rb, split, 1, NFB_GOLD_HI, NFB_GOLD_LO, gold_rim, 0, 0.35f, body, rimw);
    nfb_fill(x, cxm - gap / 2, py + rimw, gap, split - py - rimw, NFB_RIM, 1);                 /* button split */
    nfb_fill(x, px + w * 0.1f, split - gap / 2, w * 0.8f, gap, NFB_RIM, 1);
    int lit = code == VK_MBUTTON || code == NFB_WHEEL_UP || code == NFB_WHEEL_DOWN;
    float ww = fmaxf(3, w / 6), wh = h * 0.2f, wx = cxm - ww / 2, wy = py + h * 0.1f;
    nfb_gem(x, wx, wy, wx + ww, wy + wh, ww / 2, lit ? NFB_GOLD_HI : grey_t, lit ? NFB_GOLD_LO : grey_b, dark, 1, 0.3f, NULL, 0);
    if (code == NFB_WHEEL_UP || code == NFB_WHEEL_DOWN) {   /* an arrow beside the wheel */
        float ax = px + w * 0.8f, s = w * 0.12f, ay = wy + wh * (code == NFB_WHEEL_UP ? 0.3f : 0.7f);
        if (code == NFB_WHEEL_UP) nfb_tri(x, ax - s, ay + s * 0.6f, ax + s, ay + s * 0.6f, ax, ay - s, NFB_GOLD_HI);
        else nfb_tri(x, ax - s, ay - s * 0.6f, ax + s, ay - s * 0.6f, ax, ay + s, NFB_GOLD_HI);
    }
    if (code == VK_XBUTTON1 || code == VK_XBUTTON2) {   /* side buttons on the left: 5 (forward) above 4 (back) */
        float sx0 = px + rimw * 0.5f, sx1 = px + w * 0.24f, sh = h * 0.1f;
        for (int k = 0; k < 2; k++) {
            float sy = py + h * (k ? 0.64f : 0.50f);
            int on = (k == 0) == (code == VK_XBUTTON2);
            nfb_gem(x, sx0, sy, sx1, sy + sh, sh / 2, on ? NFB_GOLD_HI : grey_t, on ? NFB_GOLD_LO : grey_b, dark, 1, 0.3f, NULL, 0);
        }
    }
    return (int)ceilf(w);
}
/* The picture for a bound key: a keycap, or a mouse with a short word after it. */
static int nfb_picture(NfbCtx *x, unsigned code, float px, float py, float h)
{
    if (!nfb_is_mouse(code)) return nfb_keycap(x, code, px, py, h);
    int w = nfb_mouse(x, code, px, py, h);
    char lab[32]; nfb_key_name(code, lab, sizeof lab, 1);
    NfbRgb leg = {232, 212, 168};
    int tw = nfb_textw(x, x->help, lab);
    nfb_text(x, x->help, px + w + h * 0.25f, py, (float)tw + 4, h, lab, leg, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
    return w + (int)(h * 0.25f) + tw;
}
/* A row of prompts: [key] word, centred at y. */
static void nfb_prompts(NfbCtx *x, const unsigned *keys, const char *const *words, int n, float cy, float kh)
{
    float total = 0, gap = x->f * 1.1f; int kw[6];
    for (int i = 0; i < n; i++) {
        char lab[32]; nfb_key_name(keys[i], lab, sizeof lab, 1);
        float w = kh * 0.92f, tw = (float)nfb_textw(x, strlen(lab) == 1 ? x->leg1 : x->leg2, lab) + kh * 0.55f;
        kw[i] = (int)ceilf(tw > w ? tw : w);
        total += kw[i] + x->f * 0.35f + nfb_textw(x, x->name, words[i]) + (i ? gap : 0);
    }
    float px = (x->w - total) / 2;
    NfbRgb word = {226, 210, 176};
    for (int i = 0; i < n; i++) {
        if (i) px += gap;
        nfb_keycap(x, keys[i], px, cy - kh / 2, kh); px += kw[i] + x->f * 0.35f;
        int tw = nfb_textw(x, x->name, words[i]);
        nfb_text(x, x->name, px, cy - x->f, (float)tw + 4, x->f * 2, words[i], word, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 1);
        px += tw;
    }
}
static void nfb_line(NfbCtx *x, float y, float t)   /* the orange divider, fading to the sides */
{
    NfbRgb c = {232, 142, 52};
    nfb_flush(x);
    for (int py = (int)y; py < (int)(y + t); py++) for (int px = 0; px < x->w; px++) {
        float d = fabsf(px - x->w * 0.5f) / (x->w * 0.5f);
        nfb_put(x, px, py, c, 0.9f * (1 - d * d));
    }
}

static NfoPanel nfb_panel;
static NfbHit nfb_new_hits[NFB_HITS]; static int nfb_new_n;
static void nfb_add_hit(float x0, float y0, float w, float h, int kind, int a, int b)
{
    if (nfb_new_n >= NFB_HITS) return;
    NfbHit t = {(int)x0, (int)y0, (int)w, (int)h, kind, a, b}; nfb_new_hits[nfb_new_n++] = t;
}
/* Draws the whole page into a bitmap of pw x ph and uploads it. */
static int nfb_render(int fu, int pw, int ph, int tab, int sel, int slot, int top, int mode, int choice, int blink, const char *msg)
{
    HDC dc = CreateCompatibleDC(NULL); if (!dc) return 0;
    BITMAPINFO bi = {0}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = pw; bi.bmiHeader.biHeight = -ph;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL; HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); return 0; }
    HGDIOBJ oldb = SelectObject(dc, bmp);
    SetBkMode(dc, TRANSPARENT);
    float f = (float)fu, rh = f * 1.75f, kh = f * 1.25f;
    NfbCtx X = {0}; NfbCtx *x = &X;
    x->dc = dc; x->px = (uint32_t *)bits; x->w = pw; x->h = ph; x->f = f;
#define NFB_FONT(px, wt, it) CreateFontA(-(int)(px), 0, 0, 0, wt, it, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI")
    x->name = NFB_FONT(f, FW_SEMIBOLD, 0); x->bold = NFB_FONT(f, FW_BOLD, 0); x->head = NFB_FONT(f * 0.85f, FW_BOLD, 0);
    x->help = NFB_FONT(f * 0.9f, FW_SEMIBOLD, 0); x->title = NFB_FONT(f * 1.9f, FW_BOLD, 1); x->dlg = NFB_FONT(f * 1.05f, FW_BOLD, 0);
    x->leg1 = NFB_FONT(kh * 0.6f, FW_BOLD, 0); x->leg2 = NFB_FONT(kh * 0.4f, FW_BOLD, 0);
#undef NFB_FONT
    nfb_new_n = 0;
    {   /* background: dark red-brown with an orange glow top left, like the game's menus */
        NfbRgb t0 = {56, 22, 10}, t1 = {18, 6, 3};
        for (int py = 0; py < ph; py++) {
            NfbRgb base = nfb_mix(t0, t1, (float)py / ph);
            for (int px = 0; px < pw; px++) {
                float dx = (px - pw * 0.22f) / (pw * 0.7f), dy = (py - ph * 0.12f) / (ph * 0.6f), d = 1 - sqrtf(dx * dx + dy * dy);
                int k = d > 0 ? (int)(d * d * 70) : 0;
                int r = base.r + k, g = base.g + k * 2 / 5, b = base.b + k / 10;
                x->px[py * pw + px] = 0xFF000000u | ((uint32_t)(r > 255 ? 255 : r) << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
        int t = fu >= 24 ? 2 : 1; NfbRgb edge = {214, 128, 44}, in = {90, 36, 12};
        nfb_frame(x, 0, 0, (float)pw, (float)ph, t, edge); nfb_frame(x, (float)t, (float)t, (float)(pw - 2 * t), (float)(ph - 2 * t), 1, in);
    }
    NfbRgb beige = {234, 214, 170}, muted = {170, 140, 100}, head = {196, 154, 72}, name = {214, 194, 154}, name_sel = {255, 242, 216};
    NfbRgb sel_fill = {118, 48, 16}, sel_edge = {226, 160, 70}, slot_edge = {250, 214, 120};
    int bt = fu >= 24 ? 2 : 1;
    nfb_text(x, x->title, f * 1.4f, f * 0.5f, f * 16, f * 2.4f, "Key Binds", beige, DT_LEFT | DT_TOP | DT_SINGLELINE, fu >= 20 ? 2 : 1);
    nfb_text(x, x->help, f * 16, f * 1.25f, pw - f * 17.4f, f * 1.4f, "Changes are saved at once", muted, DT_RIGHT | DT_TOP | DT_SINGLELINE, 0);
    nfb_line(x, f * 3.0f, (float)bt);
    /* tabs */
    for (int t = 0; t < NFB_GAMES; t++) {
        float tx = f * (1.4f + t * 7.6f), ty = f * 3.5f, tw = f * 7.0f, th = f * 1.7f;
        if (t == tab) { nfb_fill(x, tx, ty, tw, th, sel_fill, 1); nfb_frame(x, tx, ty, tw, th, bt, sel_edge); }
        NfbRgb tc = {250, 236, 206}, td = {176, 150, 116};
        nfb_text(x, x->bold, tx, ty, tw, th, nfb_tab_names[t], t == tab ? tc : td, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 1);
        if (mode == NFB_BROWSE || mode == NFB_LISTEN) nfb_add_hit(tx, ty, tw, th, NFB_HIT_TAB, t, 0);
    }
    float col[2] = {f * 17.5f, f * 26.5f}, y0 = f * 7.0f;
    nfb_text(x, x->head, f * 1.8f, f * 5.5f, f * 12, f * 1.3f, "Action", head, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
    nfb_text(x, x->head, col[0], f * 5.5f, f * 8, f * 1.3f, "Key 1", head, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
    nfb_text(x, x->head, col[1], f * 5.5f, f * 8, f * 1.3f, "Key 2", head, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
    int n; const NfbAction *L = nfb_list(tab, &n);
    int rows = n + 1;
    for (int i = 0; i < NFB_VIS && top + i < rows; i++) {
        int r = top + i; float ry = y0 + i * rh;
        if (r == sel) { nfb_fill(x, f, ry, pw - 2 * f, rh, sel_fill, 1); nfb_frame(x, f, ry, pw - 2 * f, rh, bt, sel_edge); }
        if (mode == NFB_BROWSE || mode == NFB_LISTEN) nfb_add_hit(f, ry, pw - 2 * f, rh, NFB_HIT_ROW, r, 0);
        if (r >= n) {   /* Reset to defaults */
            char hint[64]; snprintf(hint, sizeof hint, "Every %s key back as it was", nfb_tab_names[tab]);
            nfb_text(x, x->name, f * 1.8f, ry, f * 15, rh, "Reset to defaults", r == sel ? name_sel : name, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 1);
            nfb_text(x, x->help, col[0], ry, pw - col[0] - f * 1.5f, rh, hint, muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, 0);
            continue;
        }
        nfb_text(x, x->name, f * 1.8f, ry, col[0] - f * 2.4f, rh, L[r].name, r == sel ? name_sel : name, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, 1);
        for (int s = 0; s < 2; s++) {
            float sx = col[s], sy = ry + (rh - kh) / 2;
            if (r == sel && s == slot) nfb_frame(x, sx - f * 0.35f, ry + f * 0.12f, f * 8.4f, rh - f * 0.24f, bt, slot_edge);
            if (mode == NFB_BROWSE || mode == NFB_LISTEN) nfb_add_hit(sx - f * 0.35f, ry, f * 8.4f, rh, NFB_HIT_SLOT, r, s);
            unsigned code = nfb_map[tab][r][s];
            if (mode == NFB_LISTEN && r == sel && s == slot) {
                NfbRgb on = {250, 214, 120}, off = {150, 110, 50};
                nfb_text(x, x->bold, sx, ry, f * 8, rh, "Press a key", blink ? off : on, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 1);
            } else if (code) nfb_picture(x, code, sx, sy, kh);
            else nfb_text(x, x->name, sx, ry, f * 8, rh, "-", muted, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
        }
    }
    if (top > 0) nfb_tri(x, pw - f * 1.9f, y0 - f * 0.25f, pw - f * 1.1f, y0 - f * 0.25f, pw - f * 1.5f, y0 - f * 0.85f, sel_edge);
    if (top + NFB_VIS < rows) { float by = y0 + NFB_VIS * rh; nfb_tri(x, pw - f * 1.9f, by + f * 0.2f, pw - f * 1.1f, by + f * 0.2f, pw - f * 1.5f, by + f * 0.8f, sel_edge); }
    {   /* the line under the list: a message, what to do now, or the highlighted action's description */
        char line[200]; NfbRgb gold = {236, 196, 92};
        if (msg && *msg) snprintf(line, sizeof line, "%s", msg);
        else if (mode == NFB_LISTEN && sel < n) snprintf(line, sizeof line, "Press the new key or mouse button for %s. Esc cancels.", L[sel].name);
        else if (sel < n) snprintf(line, sizeof line, "%s", L[sel].help);
        else snprintf(line, sizeof line, "Puts every %s action back to its usual keys.", nfb_tab_names[tab]);
        nfb_text(x, x->help, f * 1.8f, f * 26.6f, pw - f * 3.6f, f * 1.4f, line, gold, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, 0);
    }
    nfb_line(x, f * 28.2f, (float)bt);
    if (mode == NFB_BROWSE) {
        static const unsigned k[4] = {VK_RETURN, VK_DELETE, VK_TAB, VK_ESCAPE};
        static const char *const wd[4] = {"Change", "Clear", "Other list", "Back"};
        nfb_prompts(x, k, wd, 4, f * 29.6f, f * 1.2f);
    } else if (mode == NFB_LISTEN) {
        static const unsigned k[1] = {VK_ESCAPE}; static const char *const wd[1] = {"Cancel"};
        nfb_prompts(x, k, wd, 1, f * 29.6f, f * 1.2f);
    } else {
        static const unsigned k[2] = {VK_RETURN, VK_ESCAPE}; static const char *const wd[2] = {"Select", "Cancel"};
        nfb_prompts(x, k, wd, 2, f * 29.6f, f * 1.2f);
    }
    if (mode == NFB_CLASH || mode == NFB_RESET) {
        nfb_flush(x);
        for (int i = 0; i < pw * ph; i++) {   /* dim the page under the box */
            uint32_t p = x->px[i];
            x->px[i] = 0xFF000000u | ((((p >> 16) & 255) * 3 / 5) << 16) | ((((p >> 8) & 255) * 3 / 5) << 8) | ((p & 255) * 3 / 5);
        }
        int o[5], no; char opt[5][96], help[5][200], title[160], kname[32], oldn[32];
        unsigned char pend; unsigned char old; int ca, cs;
        AcquireSRWLockShared(&nfb_lock); pend = nfb_pending; old = nfb_old; ca = nfb_clash_a; cs = nfb_clash_s; ReleaseSRWLockShared(&nfb_lock);
        no = nfb_options(o);
        nfb_key_name(pend, kname, sizeof kname, 0); nfb_key_name(old, oldn, sizeof oldn, 0);
        const char *an = (sel >= 0 && sel < n) ? L[sel].name : "", *bn = (ca >= 0 && ca < n) ? L[ca].name : "";
        unsigned char other = (ca >= 0 && ca < n) ? nfb_map[tab][ca][!cs] : 0;
        char othern[32]; nfb_key_name(other, othern, sizeof othern, 0);
        for (int i = 0; i < no; i++) switch (o[i]) {
        case NFB_OPT_REMOVE:
            snprintf(opt[i], sizeof opt[i], "Remove from %s", bn);
            if (other) snprintf(help[i], sizeof help[i], "%s will be for %s only. %s keeps %s.", kname, an, bn, othern);
            else snprintf(help[i], sizeof help[i], "%s will be for %s only. %s will have no key until you set one.", kname, an, bn);
            break;
        case NFB_OPT_SWAP:
            snprintf(opt[i], sizeof opt[i], "Swap: %s gets %s", bn, oldn);
            snprintf(help[i], sizeof help[i], "%s will be for %s, and %s for %s.", kname, an, oldn, bn);
            break;
        case NFB_OPT_BOTH:
            snprintf(opt[i], sizeof opt[i], "Use both");
            snprintf(help[i], sizeof help[i], "%s will do %s and %s at the same time.", kname, an, bn);
            break;
        case NFB_OPT_RESET:
            snprintf(opt[i], sizeof opt[i], "Reset");
            snprintf(help[i], sizeof help[i], "Every %s action gets its usual keys back.", nfb_tab_names[tab]);
            break;
        default:
            snprintf(opt[i], sizeof opt[i], "Cancel");
            snprintf(help[i], sizeof help[i], "Nothing changes.");
        }
        float bw = f * 27, bh = f * (1.0f + 1.9f + 1.5f + no * 1.65f + 0.4f + 2.6f + 0.6f), bx = (pw - bw) / 2, by = (ph - bh) / 2 - f;
        NfbRgb bg0 = {50, 18, 7}, bg1 = {28, 9, 4}, edge = {234, 172, 74}, white = {250, 240, 220}, q = {204, 184, 150};
        nfb_gem(x, bx, by, bx + bw, by + bh, f * 0.3f, bg0, bg1, edge, (float)bt, 0, NULL, 0);
        float ty = by + f * 1.0f;
        if (mode == NFB_CLASH) {
            int kw = nfb_picture(x, pend, bx + f * 1.2f, ty + (f * 1.9f - kh) / 2, kh);
            snprintf(title, sizeof title, "is already bound to %s.", bn);
            nfb_text(x, x->dlg, bx + f * 1.2f + kw + f * 0.5f, ty, bw - f * 2.4f - kw, f * 1.9f, title, white, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, 1);
            nfb_text(x, x->help, bx + f * 1.2f, ty + f * 1.9f, bw - f * 2.4f, f * 1.5f, "What should happen?", q, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
        } else {
            snprintf(title, sizeof title, "Reset the %s keys?", nfb_tab_names[tab]);
            nfb_text(x, x->dlg, bx + f * 1.2f, ty, bw - f * 2.4f, f * 1.9f, title, white, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 1);
            nfb_text(x, x->help, bx + f * 1.2f, ty + f * 1.9f, bw - f * 2.4f, f * 1.5f, "Your changes to this list will be lost.", q, DT_LEFT | DT_VCENTER | DT_SINGLELINE, 0);
        }
        float oy = ty + f * 3.4f;
        for (int i = 0; i < no; i++) {
            float yy = oy + i * f * 1.65f;
            if (i == choice) { nfb_fill(x, bx + f * 0.7f, yy, bw - f * 1.4f, f * 1.55f, sel_fill, 1); nfb_frame(x, bx + f * 0.7f, yy, bw - f * 1.4f, f * 1.55f, bt, sel_edge); }
            nfb_text(x, x->name, bx + f * 1.5f, yy, bw - f * 3, f * 1.55f, opt[i], i == choice ? name_sel : name, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, 1);
            nfb_add_hit(bx + f * 0.7f, yy, bw - f * 1.4f, f * 1.55f, NFB_HIT_OPT, i, 0);
        }
        NfbRgb gold = {236, 196, 92};
        int c = choice >= 0 && choice < no ? choice : 0;
        nfb_text(x, x->help, bx + f * 1.2f, oy + no * f * 1.65f + f * 0.4f, bw - f * 2.4f, f * 2.6f, help[c], gold, DT_LEFT | DT_TOP | DT_WORDBREAK, 0);
    }
    GdiFlush();
    int ok = 0;
    {
        if (nfb_panel.srv) ID3D11ShaderResourceView_Release(nfb_panel.srv);
        if (nfb_panel.tex) ID3D11Texture2D_Release(nfb_panel.tex);
        nfb_panel.srv = NULL; nfb_panel.tex = NULL;
        D3D11_TEXTURE2D_DESC td = {(UINT)pw, (UINT)ph, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE, 0, 0};
        D3D11_SUBRESOURCE_DATA sd = {bits, (UINT)pw * 4, 0};
        ok = SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, &sd, &nfb_panel.tex)) &&
             SUCCEEDED(ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)nfb_panel.tex, NULL, &nfb_panel.srv));
        if (ok) { nfb_panel.w = (unsigned)pw; nfb_panel.h = (unsigned)ph; }
    }
    HFONT fonts[8] = {x->name, x->bold, x->head, x->help, x->title, x->dlg, x->leg1, x->leg2};
    SelectObject(dc, oldb); DeleteObject(bmp);
    for (int i = 0; i < 8; i++) if (fonts[i]) DeleteObject(fonts[i]);
    DeleteDC(dc);
    if (ok) {
        AcquireSRWLockExclusive(&nfb_hit_lock);
        memcpy(nfb_hits, nfb_new_hits, sizeof nfb_hits); nfb_nhits = nfb_new_n;
        ReleaseSRWLockExclusive(&nfb_hit_lock);
    }
    return ok;
}
/* Present thread: the page, centred, re-drawn only when something on it changed. */
static void nfb_draw(ID3D11DeviceContext *c, float W, float H)
{
    int fu = (int)(H / 40); if (fu < 12) fu = 12;
    int pw = 36 * fu, ph = 31 * fu;
    while ((pw > W - 8 || ph > H - 8) && fu > 8) { fu--; pw = 36 * fu; ph = 31 * fu; }
    int mode = (int)nfb_mode, blink = mode == NFB_LISTEN ? (int)((GetTickCount64() / 500) & 1) : 0;
    char msg[160] = "";
    AcquireSRWLockShared(&nfb_lock);
    if (GetTickCount64() < nfb_msg_until) snprintf(msg, sizeof msg, "%s", nfb_msg);
    ReleaseSRWLockShared(&nfb_lock);
    int tab = (int)nfb_tab, sel = (int)nfb_sel, rows = nfb_rows(tab);
    if (sel < 0 || sel >= rows) sel = 0;
    char key[400];
    snprintf(key, sizeof key, "%d|%d|%d|%d|%d|%ld|%ld|%d|%ld|%ld|%d|%s", fu, pw, ph, tab, sel, nfb_slot, nfb_top, mode, nfb_choice, nfb_version, blink, msg);
    if (!nfb_panel.srv || strcmp(key, nfb_panel.key)) {
        if (nfb_render(fu, pw, ph, tab, sel, (int)nfb_slot, (int)nfb_top, mode, (int)nfb_choice, blink, msg)) snprintf(nfb_panel.key, sizeof nfb_panel.key, "%s", key);
        else nfb_panel.key[0] = 0;
    }
    float x = floorf((W - nfb_panel.w) / 2), y = floorf((H - nfb_panel.h) / 2);
    AcquireSRWLockExclusive(&nfb_hit_lock); nfb_org_x = (int)x; nfb_org_y = (int)y; nfb_drawn_w = (int)W; nfb_drawn_h = (int)H; ReleaseSRWLockExclusive(&nfb_hit_lock);
    nfo_draw_panel(c, &nfb_panel, x, y, W, H, 0.96f);
}
