/* "PC Options" and "PC Graphics" pages in the game's own front-end menu (NF_OVERLAY=1
 * only; native-driving/ingame-menu/BUILD.md and MENU-DATA.md).
 *
 * Codename menu > Options > Audio/Video gets a "PC Options" button (a copy of the
 * "Credits" row) that opens a new page 0x40000061 with two buttons: "PC Graphics"
 * (also a Credits copy) and "Key Binds" (a copy of "Restore Defaults"; it opens the
 * overlay's Key Binds screen, lean_binds.inc nf_overlay_open_binds). PC Graphics
 * opens a new page 0x40000060, built at load time from
 * copies of the Audio/Video page's own records: rows with left/right pickers for
 * Aspect ratio, Resolution (the sizes for the chosen aspect ratio, refilled when it
 * changes), Brightness, Scaling, Display mode, Smooth Motion (in-between frames, next
 * start), Uncapped Frame Rate and Frame Rate Cap (pictures shown only; the game's own
 * 60 Hz update is not touched; both need Smooth Motion running). The values are
 * the F10 overlay's settings (lean_overlay.inc nf_overlay_get/set: same live
 * settings, same save file). Nothing on disk changes: the menu blob is extended
 * in guest memory when the front end loads.
 *
 * The main menu's "Multiplayer" opens a new page with "Local" and "Online" (section
 * "Multiplayer: Local / Online" at the end of this file).
 *
 * Generated routines replaced (scripts/pcg_menu_rename.py renames the originals to
 * orig_sub_...); without NF_OVERLAY=1 each simply calls its original:
 *   0x092B50 MenuManager_Load(size, blob)        extended front-end blob
 *   0x08E320 Handler_HandleMessage(ctx, obj, msg, a4, a5)   page 0x40000060
 *   0x06D460 Txt_BindLabel(id, ...)              text for string group 0x7F
 *   0x0959E0 Page_SetHelpText(page)              description line for our controls
 *   0x095A80 Page_Update(manager, page)          mouse in the menus
 *   0x092C40 Manager_SendMessage(manager, msg, p1, p2)   page changes for Local / Online
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PCG_TEST                 /* PCG_TEST: native-driving/ingame-menu/tools/pcg_splice_test.c */
#include "nd3d_api.h"
#endif

void orig_sub_00092B50(void);
void orig_sub_0008E320(void);
void orig_sub_0006D460(void);
void sub_00072820(void);   /* __Menu_Send(ctx, control id, msg, p1, p2), cdecl */
void sub_00092C40(void);   /* Manager_SendMessage(manager, msg, p1, p2), cdecl */
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);

#define AV_PAGE      0x40000031u
#define PCG_PAGE     0x40000060u
#define PCG_BUTTON   0x100002F0u
#define PCG_ROW0     0x100002F1u   /* rows 0x100002F1..F8 */
#define PCG_APPLY    0x100002FAu   /* buttons, copies of "Restore Defaults" (0x100002F7/F8 before the frame rate rows) */
#define PCG_OK       0x100002FBu
#define PCO_PAGE     0x40000061u   /* PC Options */
#define PCO_GRAPHICS 0x100002FCu
#define PCO_KEYS     0x100002FDu
#define TXT_PCO_TITLE    0x7F000005u
#define TXT_PCO_GRAPHICS 0x7F000006u
#define TXT_PCO_KEYS     0x7F000007u
#define AV_DEFAULTS  0x100001A3u
#define TXT_APPLY    0x7F000003u
#define TXT_OK       0x7F000004u
#define AV_CREDITS   0x10000138u
#define AV_TRAILER   0x10000222u
#define AV_FIRST     0x10000135u   /* the Audio/Video page's first control (its intro script focuses it) */
#define AV_ROW       0x10000136u   /* "Subtitles" row + picker: the template for the new rows */
#define AV_DECOR     0x10000001u
#define TXT_BUTTON   0x7F000001u
#define TXT_TITLE    0x7F000002u
#define TXT_ROW0     0x7F000010u
#define FLAG_AVSKIP  0x25ED78u      /* set by the Audio/Video page for buttons that change page by script */
/* Multiplayer: Local / Online (the section at the end of this file) */
#define MAIN_PAGE    0x40000002u   /* main menu: NightFire, Multiplayer, Codenames */
#define MAIN_BUTTON  0x10000002u   /* "NightFire": the template for the new buttons */
#define MAIN_FADE    0x100000EDu   /* the main page's full-screen fade (the template for the new pages' fades) */
#define MAIN_DESC    0x10000225u   /* the main page's description box */
#define JOIN_PAGE    0x40000019u   /* Join Game: each player joins and picks a codename */
#define SCEN_PAGE    0x4000001Au   /* Select Scenario */
#define CHAR_PAGE    0x40000051u   /* Choose Team / Choose Character */
#define OPTS_PAGE    0x40000012u   /* Scenario Options */
#define MPM_PAGE     0x40000062u   /* new: Multiplayer (Local, Online) */
#define ONL_PAGE     0x40000063u   /* new: Online (Host Game, Find Games, Join by IP Address) */
#define MPM_LOCAL    0x10000300u
#define MPM_ONLINE   0x10000301u
#define ONL_HOST     0x10000302u
#define ONL_FIND     0x10000303u
#define ONL_IP       0x10000304u
#define MPM_FADE     0x10000305u   /* each new page's own fade: messages to an id reach every object with that */
#define ONL_FADE     0x10000306u   /* id (0x072820), and 0x39 answers with the last one, so a copy of 0x100000ED */
                                   /* would take the main page's fade-in and leave the main menu black */
#define TXT_MP0      0x7F000020u   /* 0x7F000020.. : mp_text[] */
#define TXT_GAME_MULTIPLAYER 0x149u   /* the game's own "Multiplayer" */
#define TXT_GAME_HELP_BACK   0x218u   /* the game's own "~A Select  ~V Scroll  ~B Back" */

static int pcg_on(void)
{
    static int on = -1;
    if (on < 0) { const char *v = getenv("NF_OVERLAY"); on = v && v[0] == '1'; }
    return on;
}

/* ------------------------------------------------------------ guest strings */
static uint32_t gstr(const char *s)
{
    size_t n = strlen(s) + 1;
    uint32_t p = xbox_HeapAlloc((uint32_t)(n < 64 ? 64 : n), 4);
    if (p) memcpy((void *)ND3D_GPTR(p), s, n);
    return p;
}
static void gstr_set(uint32_t p, const char *s)   /* rewrite a 64-byte guest string in place */
{
    if (!p) return;
    char *d = (char *)ND3D_GPTR(p);
    strncpy(d, s, 63); d[63] = 0;
}

#define ROWS 8   /* row r is overlay setting r (nf_overlay_get/set) */
#define ROW_SMOOTH 5
#define ROW_CAP 7
static const char *const row_label[ROWS] = { "Aspect Ratio", "Resolution", "Brightness", "Scaling", "Display Mode",
                                             "Smooth Motion", "Uncapped Frame Rate", "Frame Rate Cap" };
#define RES_MAX 12   /* Resolution items: written from the overlay's list (nf_overlay_res_name) */
#define ITEM_MAX 16  /* items per row (the frame rate cap list has 13) */
static const char *const row_items[ROWS][5] = {
    { "Stretch", "4:3", "16:9" },
    { 0 },
    { "Off", "Low", "Medium", "High" },
    { "Smooth", "Sharp" },
    { "Windowed", "Fullscreen", "Borderless" },
    { "Off", "On" },
    { "Off", "On" },
    { 0 },   /* from the overlay's list (nf_overlay_cap_name) */
};
static int row_count[ROWS] = { 3, 0, 4, 2, 3, 2, 2, 0 };
/* Description line shown while a control is highlighted (Page_SetHelpText). */
#define HELP_BUTTON "PC graphics settings and key binds"
#define HELP_GRAPHICS "Adjust resolution, brightness and sharpness settings"
#define HELP_KEYS "Choose the keys and mouse buttons for each action"
static const char *const row_help[ROWS] = {
    "Choose the aspect ratio. 16:9 needs a restart.",
    "Choose the resolution the game is drawn at.",
    "Brighten the picture.",
    "Smooth or sharp scaling.",
    "Play in a window, fullscreen, or borderless fullscreen.",
    "Adds in-between frames. Restart to apply. Can slow the game.",
    "Shows as many frames as the PC can. Needs Smooth Motion.",
    "Limits the frames shown each second. Needs Smooth Motion.",
};
int nf_overlay_cap_name(int i, char *out, int n);
void nf_overlay_open_binds(void);   /* lean_binds.inc: the Key Binds screen */
static uint32_t g_items[ROWS][ITEM_MAX], g_labels[ROWS], g_button, g_title, g_empty, g_help_button, g_help[ROWS];
static uint32_t g_apply, g_ok, g_help_apply, g_help_ok;
static uint32_t g_pco_title, g_pco_graphics, g_pco_keys, g_help_graphics, g_help_keys;
/* Multiplayer: Local / Online. Text 0x7F000020 + i; description line for controls MPM_LOCAL + i. */
#define MP_TXT_N 6
static const char *const mp_text[MP_TXT_N] = { "Local", "Online", "Host Game", "Find Games", "Join by IP Address", "Online" };
#define TXT_ONL_TITLE (TXT_MP0 + 5)
#define MP_HELP_N 5
static const char *const mp_help[MP_HELP_N] = {
    "Play on this PC. Up to four players share the screen.",
    "Play with other PCs. Split-screen is only for players on this PC.",
    "Set up the scenario and map. For now the game runs on this PC only.",
    "Coming soon: a list of games with their map, mode, players and ping.",
    "Coming soon: join a game by typing the host's IP address.",
};
static uint32_t g_mp_txt[MP_TXT_N], g_mp_help[MP_HELP_N];

static void strings_init(void)
{
    if (g_empty) return;
    g_empty = gstr("");
    g_button = gstr("PC Options");
    g_title = gstr("PC Graphics");
    g_help_button = gstr(HELP_BUTTON);
    g_pco_title = gstr("PC Options"); g_pco_graphics = gstr("PC Graphics"); g_pco_keys = gstr("Key Binds");
    g_help_graphics = gstr(HELP_GRAPHICS); g_help_keys = gstr(HELP_KEYS);
    g_apply = gstr("Apply"); g_ok = gstr("OK");
    g_help_apply = gstr("Save these settings.");
    g_help_ok = gstr("Save these settings and return to PC Options.");
    for (int i = 0; i < MP_TXT_N; i++) g_mp_txt[i] = gstr(mp_text[i]);
    for (int i = 0; i < MP_HELP_N; i++) g_mp_help[i] = gstr(mp_help[i]);
    for (int r = 0; r < ROWS; r++) {
        g_labels[r] = gstr(row_label[r]);
        g_help[r] = gstr(row_help[r]);
        if (r == 1) { for (int i = 0; i < RES_MAX; i++) g_items[r][i] = gstr(""); continue; }   /* 64-byte slots */
        if (r == ROW_CAP) {   /* "Match Screen", "30 FPS", ... */
            char b[64]; int n = 0;
            while (n < ITEM_MAX && nf_overlay_cap_name(n, b, sizeof b)) { g_items[r][n] = gstr(b); n++; }
            if (!n) g_items[r][n++] = gstr("Match Screen");
            row_count[r] = n; continue;
        }
        for (int i = 0; i < row_count[r]; i++) g_items[r][i] = gstr(row_items[r][i]);
    }
}

/* ------------------------------------------------------------ Txt_BindLabel */
int nf_overlay_get(int which);
void nf_overlay_set(int which, int v);
int nf_overlay_res_count(void);
int nf_overlay_res_name(int i, char *out, int n);

/* The Resolution row's items for the current aspect ratio. Returns 1 when a name changed. */
static int res_items(void)
{
    int n = nf_overlay_res_count(), changed = 0;
    if (n > RES_MAX) n = RES_MAX;
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++) {
        char b[64];
        if (!nf_overlay_res_name(i, b, sizeof b)) snprintf(b, sizeof b, "?");
        if (g_items[1][i] && strcmp((const char *)ND3D_GPTR(g_items[1][i]), b)) { gstr_set(g_items[1][i], b); changed = 1; }
    }
    if (row_count[1] != n) { row_count[1] = n; changed = 1; }
    return changed;
}
int nf_overlay_widescreen_told(void);
int nf_overlay_smooth_told(void);

static void shape_hint(void)   /* "Aspect Ratio (restart to apply)" while the game runs with the other choice; same for Smooth Motion */
{
    int s = nf_overlay_get(0), told = nf_overlay_widescreen_told();
    char b[64];
    snprintf(b, sizeof b, "%s%s", row_label[0], (s == 2) != (told != 0) ? " (restart to apply)" : "");
    gstr_set(g_labels[0], b);
    s = nf_overlay_get(ROW_SMOOTH); told = nf_overlay_smooth_told();
    snprintf(b, sizeof b, "%s%s", row_label[ROW_SMOOTH], told >= 0 && (s != 0) != (told != 0) ? " (restart to apply)" : "");
    gstr_set(g_labels[ROW_SMOOTH], b);
}

void sub_0006D460(void)
{
    uint32_t id = ARG(1);
    if (!pcg_on() || (id >> 24) != 0x7Fu) { orig_sub_0006D460(); return; }
    strings_init();
    uint32_t p = g_empty;
    if (id == TXT_BUTTON) p = g_button;
    else if (id == TXT_TITLE) p = g_title;
    else if (id == TXT_APPLY) p = g_apply;
    else if (id == TXT_OK) p = g_ok;
    else if (id == TXT_PCO_TITLE) p = g_pco_title;
    else if (id == TXT_PCO_GRAPHICS) p = g_pco_graphics;
    else if (id == TXT_PCO_KEYS) p = g_pco_keys;
    else if (id >= TXT_ROW0 && id < TXT_ROW0 + ROWS) { if (id == TXT_ROW0 || id == TXT_ROW0 + ROW_SMOOTH) shape_hint(); p = g_labels[id - TXT_ROW0]; }
    else if (id >= TXT_MP0 && id < TXT_MP0 + MP_TXT_N) p = g_mp_txt[id - TXT_MP0];
    RET(p, 0);
}

/* ------------------------------------------------------------ blob splice */
/* Record grammar (MenuManager_Create 0x093960): i32 tag, then a fixed body. */
static int body_size(int32_t tag)
{
    switch (tag) {
    case -2: return 4;  case -3: return 2;  case -4: return 14; case -5: return 32;
    case -16: return 17; case -7: return 30; case -10: return 9; case -8: return 32;
    case -9: return 8;  case -11: return 14; case -12: return 13; case -13: return 9;
    case -14: return 0;
    }
    return -1;
}
typedef struct { uint32_t off, size; int32_t tag; } Rec;
#define MAXREC 16384
static Rec recs[MAXREC];
static unsigned nrec;
static const uint8_t *B;   /* the original blob (host pointer) */

static int32_t ri32(uint32_t o) { int32_t v; memcpy(&v, B + o, 4); return v; }
static uint32_t ru32(uint32_t o) { uint32_t v; memcpy(&v, B + o, 4); return v; }
static int walk(uint32_t limit)
{
    uint32_t o = 4; nrec = 0;
    while (o + 4 <= limit && nrec < MAXREC) {
        int32_t t = ri32(o); int bs = body_size(t);
        if (bs < 0) return 0;
        recs[nrec].off = o; recs[nrec].tag = t; recs[nrec].size = 4 + (uint32_t)bs; nrec++;
        o += 4 + (uint32_t)bs;
        if (t == -14) return 1;
    }
    return 0;
}
/* A group: a "define" record and everything after it up to the next define or block. */
typedef struct { unsigned first, last; uint32_t id; uint8_t type; } Group;
static unsigned group_end(unsigned i, unsigned limit)
{
    unsigned j = i + 1;
    while (j < limit && recs[j].tag != -7 && recs[j].tag != -16 && recs[j].tag != -14) j++;
    return j;   /* exclusive */
}
static uint32_t group_label(Group g)   /* the msg 0x18 string id, or 0 */
{
    for (unsigned k = g.first; k < g.last; k++)
        if (recs[k].tag == -10 && B[recs[k].off + 4] == 0x18) return ru32(recs[k].off + 4 + 5);
    return 0;
}
static uint32_t group_image(Group g)   /* the msg 0x24 image id, or 0 */
{
    for (unsigned k = g.first; k < g.last; k++)
        if (recs[k].tag == -10 && B[recs[k].off + 4] == 0x24) return ru32(recs[k].off + 4 + 1);
    return 0;
}

static uint8_t *O; static uint32_t olen, ocap;
static void put(const void *p, uint32_t n) { if (olen + n <= ocap) memcpy(O + olen, p, n); olen += n; }
static void put32(uint32_t at, uint32_t v) { if (at + 4 <= ocap) memcpy(O + at, &v, 4); }
static void put16(uint32_t at, uint16_t v) { if (at + 2 <= ocap) memcpy(O + at, &v, 2); }
static uint32_t get32(uint32_t at) { uint32_t v = 0; if (at + 4 <= ocap) memcpy(&v, O + at, 4); return v; }

/* Copy a group, patching: id (-1 keep), y (0 keep), label (0 keep), page target for
 * "go to page" script messages (0 keep), focus target (0 keep). */
static void put_group(Group g, uint32_t id, uint16_t y, uint32_t label, uint32_t goto_page, uint32_t focus)
{
    for (unsigned k = g.first; k < g.last; k++) {
        uint32_t at = olen, b = at + 4;
        put(B + recs[k].off, recs[k].size);
        switch (recs[k].tag) {
        case -7:  if (id != 0xFFFFFFFFu) put32(b, id); if (y) put16(b + 8, y); break;
        case -11: if (y) put16(b + 4, y); break;
        case -10: if (label && O[b] == 0x18) put32(b + 5, label); break;
        case -12: if (goto_page && O[b + 4] == 0x44) put32(b + 5, goto_page);
                  if (focus && O[b + 4] == 0x22) put32(b + 5, focus); break;
        }
    }
}

/* The groups of one page's block; *blk = its block record. 0 when the page is missing or not as expected. */
static unsigned page_groups(uint32_t page, Group *g, unsigned max, unsigned *blk)
{
    unsigned b = 0, e, n = 0;
    while (b < nrec && !(recs[b].tag == -16 && ru32(recs[b].off + 4) == page)) b++;
    if (b + 1 >= nrec || recs[b + 1].tag != -7) return 0;
    e = b + 1;
    while (e < nrec && recs[e].tag != -16 && recs[e].tag != -14) e++;
    if (recs[e - 1].off + recs[e - 1].size != recs[b].off + recs[b].size + ru32(recs[b].off + 4 + 13)) return 0;
    for (unsigned i = b + 1; i < e && n < max; ) {
        g[n].first = i; g[n].last = group_end(i, e);
        g[n].id = ru32(recs[i].off + 4); g[n].type = B[recs[i].off + 8];
        i = g[n].last; n++;
    }
    *blk = b;
    return n;
}
static int find_group(const Group *g, unsigned n, uint32_t id, uint32_t label)   /* label 0: any */
{
    for (unsigned k = 0; k < n; k++)
        if (g[k].id == id && (!label || group_label(g[k]) == label)) return (int)k;
    return -1;
}

/* The Multiplayer page (Local, Online) and the Online page (Host Game, Find Games, Join by
 * IP Address): copies of the main page (its fade, description box and help line, buttons
 * copied from "NightFire" 32 apart from y 262 as there), with a title copied from Join Game.
 * The help line says "~A Select  ~V Scroll  ~B Back". Returns 0, adding nothing, when the
 * main page or Join Game is not as expected. */
static int g_mp_ok;   /* the two pages are in the menu: the page changes below may use them */
static int mp_pages(void)
{
    Group mg[32], jg[64]; unsigned mb = 0, jb = 0;
    unsigned nm = page_groups(MAIN_PAGE, mg, 32, &mb), nj = page_groups(JOIN_PAGE, jg, 64, &jb);
    int gpage = find_group(mg, nm, MAIN_PAGE, 0), gbutton = find_group(mg, nm, MAIN_BUTTON, 0),
        gfade = find_group(mg, nm, MAIN_FADE, 0), gdesc = find_group(mg, nm, MAIN_DESC, 0),
        ghelp = find_group(mg, nm, AV_DECOR, 0x54E), gtitle = find_group(jg, nj, AV_DECOR, 0x29E);
    if (!nm || !nj || gpage < 0 || gbutton < 0 || gfade < 0 || gdesc < 0 || ghelp < 0 || gtitle < 0) return 0;
    static const uint32_t pages[2] = { MPM_PAGE, ONL_PAGE }, titles[2] = { TXT_GAME_MULTIPLAYER, TXT_ONL_TITLE };
    static const uint32_t ids[2][3] = { { MPM_LOCAL, MPM_ONLINE, 0 }, { ONL_HOST, ONL_FIND, ONL_IP } };
    static const unsigned count[2] = { 2, 3 };
    for (int p = 0; p < 2; p++) {
        uint32_t blk = olen; put(B + recs[mb].off, recs[mb].size); put32(blk + 4, pages[p]);
        put_group(mg[gpage], pages[p], 0, 0, 0, ids[p][0]);   /* intro script focuses the first button */
        put_group(jg[gtitle], 0xFFFFFFFFu, 0, titles[p], 0, 0);
        for (unsigned i = 0; i < count[p]; i++)              /* text 0x7F000020 + (id - MPM_LOCAL) */
            put_group(mg[gbutton], ids[p][i], (uint16_t)(262 + 32 * i), TXT_MP0 + (ids[p][i] - MPM_LOCAL), 0, 0);
        put_group(mg[gfade], p ? ONL_FADE : MPM_FADE, 0, 0, 0, 0);
        put_group(mg[gdesc], 0xFFFFFFFFu, 0, 0, 0, 0);
        put_group(mg[ghelp], 0xFFFFFFFFu, 0, TXT_GAME_HELP_BACK, 0, 0);
        put32(blk + 4 + 13, olen - (blk + recs[mb].size));
    }
    return 1;
}

static uint32_t g_blob, g_blob_cap;   /* the extended copy (guest) */
static uint32_t extend_blob(uint32_t src, uint32_t size_hint)
{
    uint32_t limit = (size_hint > 0x1000 && size_hint < 0x400000) ? size_hint : 0x100000;
    B = (const uint8_t *)ND3D_GPTR(src);
    if (ri32(0) != -6 || !walk(limit)) return 0;
    unsigned av = nrec, avend = 0, endrec = nrec - 1;
    for (unsigned i = 0; i < nrec; i++) {
        if (recs[i].tag == -16 && (ru32(recs[i].off + 4) == PCG_PAGE || ru32(recs[i].off + 4) == PCO_PAGE ||
                                   ru32(recs[i].off + 4) == MPM_PAGE)) return 0;   /* already extended */
        if (recs[i].tag == -16 && ru32(recs[i].off + 4) == AV_PAGE) av = i;
    }
    if (av == nrec) return 0;                 /* not the front end */
    avend = av + 1;
    while (avend < nrec && recs[avend].tag != -16 && recs[avend].tag != -14) avend++;
    if (recs[avend - 1].off + recs[avend - 1].size != recs[av].off + recs[av].size + ru32(recs[av].off + 4 + 13)) return 0;

    /* groups of the Audio/Video block */
    Group grp[256]; unsigned ng = 0;
    for (unsigned i = av + 1; i < avend && ng < 256; ) {
        if (recs[i].tag != -7) { i++; continue; }
        Group g; g.first = i; g.last = group_end(i, avend);
        g.id = ru32(recs[i].off + 4); g.type = B[recs[i].off + 8];
        grp[ng++] = g; i = g.last;
    }
    int gpage = -1, gcredits = -1, gtrailer = -1, grow = -1, gpick = -1, gdefaults = -1;
    for (unsigned k = 0; k < ng; k++) {
        if (grp[k].id == AV_DEFAULTS && gdefaults < 0) gdefaults = (int)k;
        if (grp[k].id == AV_PAGE) gpage = (int)k;
        else if (grp[k].id == AV_CREDITS && gcredits < 0) gcredits = (int)k;
        else if (grp[k].id == AV_TRAILER && gtrailer < 0) gtrailer = (int)k;
        else if (grp[k].id == AV_ROW && grp[k].type == 5 && grow < 0) grow = (int)k;
        else if (grp[k].id == AV_ROW && grp[k].type == 9 && gpick < 0) gpick = (int)k;
    }
    if (gpage < 0 || gcredits < 0 || grow < 0 || gpick < 0 || gdefaults < 0) return 0;
    if (recs[grp[0].first].off != recs[av + 1].off) return 0;   /* records before the first define: not expected */

    uint32_t blen = recs[endrec].off + recs[endrec].size;
    uint32_t need = blen + 0x4000 + 64;
    if (!g_blob || g_blob_cap < need) { g_blob = xbox_HeapAlloc(need, 16); g_blob_cap = g_blob ? need : 0; }
    if (!g_blob) return 0;
    O = (uint8_t *)ND3D_GPTR(g_blob); ocap = g_blob_cap; olen = 0;

    /* 1. everything before the Audio/Video block, its block record (length patched below) */
    put(B, recs[av].off);
    uint32_t avblk = olen; put(B + recs[av].off, recs[av].size);
    /* 2. its groups, with the "PC Options" button (a copy of Credits) after the trailer button.
     * On screen it sits at y 0xD5, centred in the gap between Multiplayer Split Screen (0xAA)
     * and Screen Adjust (0x100) left by the two rows the page hides (Speaker, Widescreen);
     * at 0x170 it covered the description box (0x163-0x18B). Up/Down are expected to follow
     * screen position (cursor-driven menus; inferred, the first run confirms). */
    int after = gtrailer >= 0 ? gtrailer : (int)ng - 1;
    for (unsigned k = 0; k < ng; k++) {
        put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);
        if ((int)k == after)
            put_group(grp[gcredits], PCG_BUTTON, 0xD5, TXT_BUTTON, PCO_PAGE, 0);
    }
    put32(avblk + 4 + 13, olen - (avblk + recs[av].size));
    /* 3. the rest of the blob up to the end record */
    put(B + recs[avend].off, recs[endrec].off - recs[avend].off);
    /* 4. the new page: block + page (intro script focuses the first row) + frame/title/help + rows */
    uint32_t nblk = olen; put(B + recs[av].off, recs[av].size); put32(nblk + 4, PCG_PAGE);
    put_group(grp[gpage], PCG_PAGE, 0, 0, 0, PCG_ROW0);
    for (unsigned k = 0; k < ng; k++) {
        uint32_t lab = group_label(grp[k]), img = group_image(grp[k]);
        if (grp[k].id == AV_DECOR && lab == 0x2BB) put_group(grp[k], 0xFFFFFFFFu, 0, TXT_TITLE, 0, 0);   /* "AV Options" title */
        else if (grp[k].id == AV_DECOR && lab == 0x217) put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);    /* button help line */
        else if ((grp[k].id == AV_DECOR && img && img != 0x3000042) || grp[k].id == 0x10000224u || grp[k].id == 0x10000225u)
            put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);                                                    /* frames and pictures */
    }
    /* Rows 28 apart as on the Audio/Video page (86..282), then Apply and OK at the next two
     * steps; OK ends at 354, above the description box (355), like that page's last row. */
    for (unsigned r = 0; r < ROWS; r++) {
        uint16_t y = (uint16_t)(0x56 + 0x1C * r);
        put_group(grp[grow], PCG_ROW0 + r, y, TXT_ROW0 + r, 0, 0);
        put_group(grp[gpick], PCG_ROW0 + r, y, 0, 0, 0);
    }
    put_group(grp[gdefaults], PCG_APPLY, (uint16_t)(0x56 + 0x1C * ROWS), TXT_APPLY, 0, 0);        /* "Apply" 0x136 (0xE4 with five rows) */
    put_group(grp[gdefaults], PCG_OK, (uint16_t)(0x56 + 0x1C * (ROWS + 1)), TXT_OK, 0, 0);       /* "OK" 0x152 (0x100 with five rows) */
    put32(nblk + 4 + 13, olen - (nblk + recs[av].size));
    /* 5. the PC Options page: block + page (intro script focuses PC Graphics) + frame/title/help + two buttons */
    uint32_t pblk = olen; put(B + recs[av].off, recs[av].size); put32(pblk + 4, PCO_PAGE);
    put_group(grp[gpage], PCO_PAGE, 0, 0, 0, PCO_GRAPHICS);
    for (unsigned k = 0; k < ng; k++) {
        uint32_t lab = group_label(grp[k]), img = group_image(grp[k]);
        if (grp[k].id == AV_DECOR && lab == 0x2BB) put_group(grp[k], 0xFFFFFFFFu, 0, TXT_PCO_TITLE, 0, 0);
        else if (grp[k].id == AV_DECOR && lab == 0x217) put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);
        else if ((grp[k].id == AV_DECOR && img && img != 0x3000042) || grp[k].id == 0x10000224u || grp[k].id == 0x10000225u)
            put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);
    }
    put_group(grp[gcredits], PCO_GRAPHICS, 0x56, TXT_PCO_GRAPHICS, PCG_PAGE, 0);   /* its script opens PC Graphics */
    put_group(grp[gdefaults], PCO_KEYS, 0x56 + 0x1C, TXT_PCO_KEYS, 0, 0);         /* handled below: opens Key Binds */
    put32(pblk + 4 + 13, olen - (pblk + recs[av].size));
    /* 6. the Multiplayer (Local, Online) and Online pages */
    int mp = mp_pages();
    /* 7. the end record and anything after it */
    put(B + recs[endrec].off, recs[endrec].size);
    if (olen > ocap) return 0;
    g_mp_ok = mp;
    fprintf(stderr, "[NF-OVERLAY] PC Options and PC Graphics pages added to the front-end menu (%u -> %u bytes)\n", blen, olen);
    fprintf(stderr, mp ? "[NF-MP] Multiplayer (Local, Online) and Online pages added\n"
                       : "[NF-MP] main page or Join Game not as expected: Multiplayer opens Join Game as before\n");
    return g_blob;
}

void sub_00092B50(void)
{
    uint32_t size = ARG(1), blob = ARG(2);
    orig_sub_00092B50();                     /* stores the pointer at 0x25FB08 and returns */
    if (!pcg_on() || !blob) return;
    uint32_t ext = extend_blob(blob, size);
    if (ext) G32(0x25FB08) = ext;
}

/* ------------------------------------------------------------ description line */
/* Page_SetHelpText(page) 0x0959E0: the game looks the highlighted control up in a
 * fixed table (0x17D640, control id + value -> string id) and sends the text to
 * the help control (0x25FC18) with message 0x18. Our controls are not in that
 * table, so they get their text here; everything else goes to the original. */
void orig_sub_000959E0(void);
void sub_00072630(void);   /* __Menu_SendMessage(control, msg, p1, p2), cdecl */
void sub_000959E0(void)
{
    if (!pcg_on()) { orig_sub_000959E0(); return; }
    uint32_t page = ARG(1), help = G32(0x25FC18u);
    uint32_t focus = (page && help) ? G32(page + 0xACu) : 0;
    uint32_t id = focus ? G32(focus + 0x18u) : 0, text = 0;
    if (id == PCG_BUTTON || id == PCG_APPLY || id == PCG_OK || id == PCO_GRAPHICS || id == PCO_KEYS || (id >= PCG_ROW0 && id < PCG_ROW0 + ROWS)) {
        strings_init();
        text = id == PCG_BUTTON ? g_help_button : id == PCG_APPLY ? g_help_apply : id == PCG_OK ? g_help_ok :
               id == PCO_GRAPHICS ? g_help_graphics : id == PCO_KEYS ? g_help_keys : g_help[id - PCG_ROW0];
    }
    else if (id >= MPM_LOCAL && id < MPM_LOCAL + MP_HELP_N) { strings_init(); text = g_mp_help[id - MPM_LOCAL]; }
    if (!text) { orig_sub_000959E0(); return; }
    nd3d_call_cdecl(sub_00072630, 4, help, 0x18u, text, 0u);
    RET(0, 0);
}

/* ------------------------------------------------------------ page handler */
/* Router arguments (Handler_HandleMessage 0x08E320): ctx (0 = the current page;
 * the Audio/Video page passes the same value to __Menu_Send), the object whose id
 * is routed, the message, a4, a5. Page messages seen on this page (log of
 * 2026-10-09): 0x51/0x4D when the menu is built, 0x4C open (a5 = previous page),
 * 0x50 every frame, 0x4B a button pressed (a5 = the button). Picker messages:
 * 0x51 build, 0x49 highlighted, 0x4E Left, 0x4F Right, 0x4B A, 0x5E. */
static int g_open, g_shown[ROWS];
static int mp_route(uint32_t ctx, uint32_t id, uint32_t msg, uint32_t a5);   /* Multiplayer: Local / Online */
static int mp_char(uint32_t ctx, uint32_t obj, uint32_t id, uint32_t msg, uint32_t a4, uint32_t a5);   /* Online: no health handicap */

static uint32_t menu_send(uint32_t ctx, uint32_t id, uint32_t msg, uint32_t p1, uint32_t p2)
{
    return nd3d_call_cdecl(sub_00072820, 5, ctx, id, msg, p1, p2);
}
static void row_fill(uint32_t ctx, int r)
{
    uint32_t id = PCG_ROW0 + (uint32_t)r;
    int v = nf_overlay_get(r); if (v < 0 || v >= row_count[r]) v = 0;
    menu_send(ctx, id, 0x17, 0, 0);                                   /* clear */
    for (int i = 0; i < row_count[r]; i++) menu_send(ctx, id, 0x10, g_items[r][i], (uint32_t)i);   /* add item, value i */
    menu_send(ctx, id, 0x1E, (uint32_t)v, 0);                         /* select */
    g_shown[r] = v;
}
static void page_fill(uint32_t ctx)
{
    strings_init(); shape_hint(); res_items();
    for (int r = 0; r < ROWS; r++) row_fill(ctx, r);
    fprintf(stderr, "[NF-OVERLAY] PC Graphics page filled: aspect %d resolution %d brightness %d scaling %d display %d smooth %d uncapped %d cap %d\n",
            g_shown[0], g_shown[1], g_shown[2], g_shown[3], g_shown[4], g_shown[5], g_shown[6], g_shown[7]);
}
/* Read every picker; save what changed. why: "live", "Apply", "OK". */
static void page_read(uint32_t ctx, const char *why)
{
    static uint32_t last[ROWS]; static unsigned nlog; static int last_init;
    if (!last_init) { memset(last, 0xFF, sizeof last); last_init = 1; }
    int v[ROWS], changed = 0;
    for (int r = 0; r < ROWS; r++) {
        uint32_t x = menu_send(ctx, PCG_ROW0 + (uint32_t)r, 0x35, 0, 0);   /* the picker's value */
        if (x != last[r] && nlog < 120) { nlog++; fprintf(stderr, "[NF-OVERLAY] PC Graphics row %d reads %08X (%s)\n", r, x, why); }
        last[r] = x;
        v[r] = ((int)x >= 0 && (int)x < row_count[r]) ? (int)x : g_shown[r];
        if (v[r] != g_shown[r]) {
            g_shown[r] = v[r]; nf_overlay_set(r, v[r]); changed = 1;
            if (r == 0) {   /* new aspect ratio: its own sizes in the Resolution row (the overlay picked the nearest one) */
                res_items(); row_fill(ctx, 1); last[1] = (uint32_t)g_shown[1]; v[1] = g_shown[1]; r++;   /* row 1 was just refilled: not read this pass */
                fprintf(stderr, "[NF-OVERLAY] PC Graphics aspect ratio %d: %d sizes, resolution %d\n", v[0], row_count[1], v[1]);
            }
        }
    }
    if (changed) shape_hint();
    if (g_shown[1] == row_count[1] - 1) res_items();   /* Match Window shows the size it draws (string rewritten in place) */
    if (strcmp(why, "live"))
        fprintf(stderr, "[NF-OVERLAY] PC Graphics %s: aspect %d resolution %d brightness %d scaling %d display %d smooth %d uncapped %d cap %d (saved)\n",
                why, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

void sub_0008E320(void)
{
    if (!pcg_on()) { orig_sub_0008E320(); return; }
    uint32_t ctx = ARG(1), obj = ARG(2), msg = ARG(3), a5 = ARG(5);
    uint32_t id = obj ? G32(obj + 0x18) : 0;
    if (id == PCG_PAGE) {
        static int logged;
        if (msg != 0x50 && logged++ < 40) fprintf(stderr, "[NF-OVERLAY] PC Graphics page message %X (a5 %08X)\n", msg, a5);
        if (msg == 0x4C) { g_open = 1; page_fill(ctx); }
        else if (msg == 0x4B && a5) {
            uint32_t b = G32(a5 + 0x18);
            if (b == PCG_APPLY) page_read(ctx, "applied");
            else if (b == PCG_OK) {
                page_read(ctx, "OK");
                g_open = 0;
                nd3d_call_cdecl(sub_00092C40, 4, 0x25F1D0u, 0x5Fu, 0u, 0u);   /* back, as B and the Audio/Video page do */
            }
        }
        else if (g_open && msg == 0x50) page_read(ctx, "live");
        RET(1, 0);
    }
    if (id == PCO_PAGE) {   /* PC Options: PC Graphics changes page by its own script; Key Binds opens the overlay screen */
        static int logged;
        if (msg != 0x50 && logged++ < 40) fprintf(stderr, "[NF-OVERLAY] PC Options page message %X (a5 %08X)\n", msg, a5);
        if (msg == 0x4C) g_open = 0;   /* back from PC Graphics */
        if (msg == 0x4B && a5 && G32(a5 + 0x18) == PCO_KEYS) {
            fprintf(stderr, "[NF-OVERLAY] PC Options: Key Binds\n");
            nf_overlay_open_binds();
        }
        RET(1, 0);
    }
    if (mp_route(ctx, id, msg, a5)) { RET(1, 0); }
    if (id == AV_PAGE && msg == 0x4B && a5 && G32(a5 + 0x18) == PCG_BUTTON) {   /* as "Credits": the button's script changes page */
        G8(FLAG_AVSKIP) = 1;
        RET(1, 0);
    }
    if (mp_char(ctx, obj, id, msg, ARG(4), a5)) return;   /* runs the game's handler itself */
    if ((id >> 28) == 4u && id != PCG_PAGE && msg == 0x4C) g_open = 0;   /* another page opened */
    orig_sub_0008E320();
    /* 0x49 = a picker's value changed (Left/Right). (0x4E/0x4F are gaining/losing
     * focus: Page_Update sends them; first read as Left/Right, corrected 2026-10-09.) */
    if (g_open && id >= PCG_ROW0 && id < PCG_ROW0 + ROWS && msg == 0x49)
        page_read(ctx, "live");
}

/* ------------------------------------------------------------ mouse in menus */
/* The menu is cursor-driven: each player's menu state (0x25F1D0, +0x94) has a
 * cursor point (+0x24 x, +0x26 y, menu units); Menu_GetControl 0x073370 makes the
 * control under it the focus and Page_Update 0x095A80 re-centres it on that
 * control. So the mouse moves that point (only over a control, only when moved),
 * and clicks / wheel become short pad presses (fb_present.c nightfire_menu_press).
 * Menu mode = a current page was updated in the last 250 ms (nf_menu_active, read
 * by the window thread to leave the pointer free). */
void orig_sub_00095A80(void);
int nightfire_menu_mouse_take(int *x, int *y, int *w, int *h, int *left, int *right, int *wheel);
void nightfire_menu_press(unsigned buttons);
int nf_overlay_client_to_game(int x, int y, int w, int h, int *gx, int *gy);
static volatile ULONGLONG g_menu_tick;

int nf_menu_active(void)
{
    return pcg_on() && g_menu_tick && GetTickCount64() - g_menu_tick < 250;
}

/* Same test as Menu_GetControl; smallest control wins. *targets = how many
 * controls the pointer could select on this page at all (0: the page is driven
 * by Up/Down only, e.g. the Codename / Options wheels). */
static uint32_t hit_test_n(uint32_t page, int gx, int gy, int *targets)
{
    uint32_t list = G32(page + 0x94u), best = 0; int best_area = 0x7FFFFFFF, n_ok = 0;
    if (targets) *targets = 0;
    if (!list) return 0;
    int rx = gx - (int16_t)G16(page + 0x70u), ry = gy - (int16_t)G16(page + 0x72u);
    for (uint32_t c = list - G32(page + 0xA0u) * 4u, n = 0; c && n < 512; c = G32(c), n++) {
        int x = (int16_t)G16(c + 0x70u), y = (int16_t)G16(c + 0x72u), w = (int16_t)G16(c + 0x74u), h = (int16_t)G16(c + 0x76u);
        if (G8(c + 0x7Au) || G8(c + 0x7Bu) == 0xD) continue;
        if (w > 0 && h > 0) n_ok++;
        if (rx > x && rx < x + w && ry > y && ry < y + h && w * h < best_area) { best = c; best_area = w * h; }
    }
    if (targets) *targets = n_ok;
    return best;
}

/* Diagnostics (first pointer use on each page): the mapped point, the page origin
 * and every control in the page's list with its rectangle and flags. */
static void dump_page(uint32_t page, int in, int gx, int gy)
{
    static uint32_t done[32]; static unsigned ndone;
    uint32_t pid = G32(page + 0x18u), list = G32(page + 0x94u);
    for (unsigned i = 0; i < ndone; i++) if (done[i] == pid) return;
    if (ndone >= 32) return;
    done[ndone++] = pid;
    fprintf(stderr, "[NF-MOUSE] page 0x%08X: pointer %s at menu %d,%d; page origin %d,%d; controls:\n", pid,
            in ? "in picture" : "outside picture", gx, gy, (int16_t)G16(page + 0x70u), (int16_t)G16(page + 0x72u));
    if (!list) { fprintf(stderr, "[NF-MOUSE]   (no control list)\n"); return; }
    for (uint32_t c = list - G32(page + 0xA0u) * 4u, n = 0; c && n < 64; c = G32(c), n++)
        fprintf(stderr, "[NF-MOUSE]   0x%08X type %X at %d,%d size %dx%d flag7A %u%s\n", G32(c + 0x18u), G8(c + 0x7Bu),
                (int16_t)G16(c + 0x70u), (int16_t)G16(c + 0x72u), (int16_t)G16(c + 0x74u), (int16_t)G16(c + 0x76u),
                G8(c + 0x7Au), c == G32(page + 0xACu) ? " (focus)" : "");
}

static void mouse_step(uint32_t ms, uint32_t page)
{
    int x, y, w, h, l, r, wh, gx = 0, gy = 0;
    int moved = nightfire_menu_mouse_take(&x, &y, &w, &h, &l, &r, &wh);
    if (!moved && !l && !r && !wh) return;
    int in = nf_overlay_client_to_game(x, y, w, h, &gx, &gy), targets = 0;
    uint32_t hit = hit_test_n(page, in ? gx : -30000, in ? gy : -30000, &targets), pid = G32(page + 0x18u);
    dump_page(page, in, gx, gy);
    uint32_t cid = hit ? G32(hit + 0x18u) : 0;
    static uint32_t logged_page; static unsigned nlog;
    if (hit && pid != logged_page && nlog < 60) {
        logged_page = pid; nlog++;
        fprintf(stderr, "[NF-MOUSE] page 0x%08X control 0x%08X under cursor (menu %d,%d)\n", pid, cid, gx, gy);
    }
    if (moved && hit) {
        uint32_t cur = G32(ms + 0x94u);
        if (cur) { G16(cur + 0x24u) = (uint16_t)gx; G16(cur + 0x26u) = (uint16_t)gy; }
    }
    /* Left click = A on the control under the pointer; on pages with nothing the
     * pointer can select (Up/Down-only pages such as the wheels) A on the current item. */
    if (l && (hit || (in && !targets))) {
        nightfire_menu_press(0x10000u);
        if (nlog++ < 200) fprintf(stderr, "[NF-MOUSE] click A on %s 0x%08X (page 0x%08X)\n", hit ? "control" : "the current item (page has no pointer targets), control", cid, pid);
    } else if (l && nlog++ < 200)
        fprintf(stderr, "[NF-MOUSE] click ignored: no control under pointer (menu %d,%d, %s, page 0x%08X, %d targets)\n", gx, gy, in ? "in picture" : "outside picture", pid, targets);
    if (r) { nightfire_menu_press(0x20000u); if (nlog++ < 200) fprintf(stderr, "[NF-MOUSE] right click B (page 0x%08X)\n", pid); }
    if (wh) {
        uint32_t f = G32(page + 0xACu); uint8_t t = f ? G8(f + 0x7Bu) : 0;
        int side = (t == 9 || t == 0xA);   /* picker or slider: Left/Right */
        unsigned up = side ? 8u : 1u, down = side ? 4u : 2u;
        for (int k = 0; k < (wh > 0 ? wh : -wh) && k < 4; k++) nightfire_menu_press(wh > 0 ? up : down);
        if (nlog++ < 200) fprintf(stderr, "[NF-MOUSE] wheel %d -> %s (focused type %X)\n", wh, side ? (wh > 0 ? "Right" : "Left") : (wh > 0 ? "Up" : "Down"), t);
    }
}

void sub_00095A80(void)
{
    if (!pcg_on()) { orig_sub_00095A80(); return; }
    uint32_t ms = ARG(1), page = ARG(2);
    if (ms && page && G32(ms + 0x1BCu) == page) {
        static uint32_t last; uint32_t pid = G32(page + 0x18u);
        if (pid != last) { last = pid; fprintf(stderr, "[NF-MOUSE] menu page 0x%08X on screen (mouse active)\n", pid); }
        g_menu_tick = GetTickCount64();
        mouse_step(ms, page);
    } else if (ms && page && G32(ms + 0x1C0u) == page) {   /* the second page slot (pop-ups?): diagnostics only */
        static uint32_t last2; uint32_t pid = G32(page + 0x18u);
        if (pid != last2) { last2 = pid; fprintf(stderr, "[NF-MOUSE] second page 0x%08X updated (pointer not used there yet)\n", pid); }
    }
    orig_sub_00095A80();
}


/* ------------------------------------------------------------ Multiplayer: Local / Online */
/* The owner's plan (2026-10-09): Multiplayer opens a submenu. Local = the usual multiplayer
 * (Join Game with its split-screen entries, then the game's own setup pages). Online = the
 * same Join Game screen, then character selection, then the Online page: Host Game, Find
 * Games, Join by IP Address. Host Game continues with the game's own Select Scenario, Select
 * Map and Scenario Options pages; until the network step exists that match runs on this PC
 * only. Find Games and Join by IP Address only describe what they will do.
 *
 * Every page change reaches Manager_SendMessage 0x092C40 as message 0x44 (p1 = the page):
 * directly, through __Menu_SendMessage 0x072630 (manager type), or from the delayed-message
 * queue 0x073240 (fired by 0x0732F0 through 0x072630). Back (B) reopens the previous page the
 * same way with p2 = 5 or 13 (0x0933D4); those are never changed. The native 0x092C40 below
 * changes p1 only for:
 *   main page -> Join Game                           Multiplayer page (always)
 *   Online: Join Game -> Select Scenario             Choose Character
 *   Online: Choose Character -> Scenario Options     Online page (before Host Game)
 *   Online, hosting: Select Map -> Choose Character  Scenario Options when the scenario has no
 *            teams (the character was chosen already; team scenarios still choose a team)
 * Pages that look at the page they were opened from (a5 of message 0x4C) get the value the
 * usual route gives: Join Game opened from our page = from the main page (fade in, players
 * reset); Scenario Options after the skipped Choose Character = from Choose Character. */
void orig_sub_00092C40(void);
void sub_00073240(void);   /* queue a message: (frames, target, msg, p1, p2), cdecl */
void sub_00095DE0(void);   /* start an animation: (control, description, frames, 0), cdecl */
static int g_mp;           /* 0 none yet, 1 Local, 2 Online */
static int g_mp_host;      /* Online: Host Game pressed */
static int g_mp_skip;      /* the next Scenario Options opening replaces Choose Character */
static uint32_t g_fade;    /* guest description for 0x095DE0: +0xC 0xE1 (fade), +0x10 alpha */

static uint32_t mp_cur_page(uint32_t ms)
{
    uint32_t p = ms ? G32(ms + 0x1BCu) : 0;
    return p ? G32(p + 0x18u) : 0;
}
static void mp_fade(uint32_t ctx, uint32_t fade, uint32_t alpha)   /* the page's full-screen fade, 15 frames */
{
    uint32_t c = menu_send(ctx, fade, 0x39, 0, 0);
    if (!g_fade) g_fade = xbox_HeapAlloc(32, 4);
    if (!c || !g_fade) return;
    G32(g_fade + 0xCu) = 0xE1u; G32(g_fade + 0x10u) = alpha; G32(g_fade + 0x14u) = 0;
    nd3d_call_cdecl(sub_00095DE0, 4, c, g_fade, 0xFu, 0u);
}

void sub_00092C40(void)
{
    if (pcg_on() && g_mp_ok && ARG(2) == 0x44u && ARG(4) != 5u && ARG(4) != 13u) {
        uint32_t ms = ARG(1), to = ARG(3), from = mp_cur_page(ms), nto = to;
        if (from == MAIN_PAGE && to == JOIN_PAGE) nto = MPM_PAGE;
        else if (g_mp == 2 && !g_mp_host && from == JOIN_PAGE && to == SCEN_PAGE) nto = CHAR_PAGE;
        else if (g_mp == 2 && !g_mp_host && from == CHAR_PAGE && to == OPTS_PAGE) nto = ONL_PAGE;
        else if (g_mp == 2 && g_mp_host && to == CHAR_PAGE && !(G32(0x26003Cu) & 0x20000000u)) {
            nto = OPTS_PAGE; ARG(4) = 0; g_mp_skip = 1;   /* p2 as Choose Character sends it */
        }
        if (nto != to) {
            static unsigned nlog;
            if (nlog++ < 60) fprintf(stderr, "[NF-MP] page 0x%08X -> 0x%08X becomes 0x%08X (%s)\n", from, to, nto,
                                     g_mp == 2 ? (g_mp_host ? "online, hosting" : "online") : "multiplayer menu");
            ARG(3) = nto;
        }
    }
    orig_sub_00092C40();
}

/* Online: no health handicap (the owner, 2026-10-10: nobody joins with extra health).
 * Choose Character 0x40000051 has a picker 0x100001A1 and a Ready box 0x1000022A for each
 * player (+0x20 = the player). Each player's step is at 0x245698 + 4 * player: 3 = character,
 * 4 = health handicap (the picker lists -75 % .. +100 %, C_RBMPSETUP 0x088720), 5 = ready.
 * The handicap is kept at 0x25FE64 + 0x30 * player (0 = normal; added to the health at spawn).
 * Online, A on a character goes on to Ready with normal health (the picker set to 0, then
 * the same press again, as a second A would confirm it), and B on Ready goes back to the
 * characters (the same press again on the picker, as a second B would leave the handicap
 * list, C_RBMPFINISH 0x088530). Whatever happens on the way, a player at Ready has normal
 * health, and opening the Online or Scenario Options page sets all four to normal. */
#define CHAR_PICKER    0x100001A1u
#define CHAR_READY     0x1000022Au
#define MP_STEP(p)     (0x245698u + 4u * (p))
#define MP_HANDICAP(p) (0x25FE64u + 0x30u * (p))
void sub_000728F0(void);   /* send to one player's copy of a control: (ctx, id, player, msg, p1, p2), cdecl */

static void mp_no_handicaps(void)
{
    for (uint32_t p = 0; p < 4; p++) G32(MP_HANDICAP(p)) = 0;
}
static int mp_char(uint32_t ctx, uint32_t obj, uint32_t id, uint32_t msg, uint32_t a4, uint32_t a5)
{
    if (!g_mp_ok || g_mp != 2 || !obj || (id != CHAR_PICKER && id != CHAR_READY)) return 0;
    uint32_t pl = G8(obj + 0x20u) & 3u, before = G32(MP_STEP(pl));
    orig_sub_0008E320();
    uint32_t ret = g_eax, after = G32(MP_STEP(pl));
    static unsigned nlog;
    if (id == CHAR_PICKER && before == 3 && after == 4) {
        nd3d_call_cdecl(sub_00072630, 4, obj, 0x1Eu, 0u, 0u);       /* select normal health */
        nd3d_call_cdecl(sub_0008E320, 5, ctx, obj, msg, a4, a5);     /* and confirm it */
        if (nlog++ < 40) fprintf(stderr, "[NF-MP] Online: player %u chose a character, no health handicap (step %u)\n", pl + 1, G32(MP_STEP(pl)));
    } else if (id == CHAR_READY && before == 5 && after == 4) {
        uint32_t pick = nd3d_call_cdecl(sub_000728F0, 6, ctx, CHAR_PICKER, pl, 0x39u, 0u, 0u);
        if (pick) nd3d_call_cdecl(sub_0008E320, 5, ctx, pick, msg, a4, a5);   /* past the handicap list */
        if (nlog++ < 40) fprintf(stderr, "[NF-MP] Online: player %u back to the characters (step %u)\n", pl + 1, G32(MP_STEP(pl)));
    }
    if (G32(MP_STEP(pl)) == 5 && G32(MP_HANDICAP(pl))) {
        if (nlog++ < 40) fprintf(stderr, "[NF-MP] Online: player %u handicap %d set to normal\n", pl + 1, (int)G32(MP_HANDICAP(pl)));
        G32(MP_HANDICAP(pl)) = 0;
    }
    g_eax = ret;
    return 1;
}

/* Router messages for the two pages, and the a5 fixes. Returns 1 when the message was ours. */
static int mp_route(uint32_t ctx, uint32_t id, uint32_t msg, uint32_t a5)
{
    if (!g_mp_ok) return 0;
    if (id == MAIN_PAGE && msg == 0x4C) { g_mp = 0; g_mp_host = 0; g_mp_skip = 0; return 0; }
    if (id == JOIN_PAGE && msg == 0x4C && a5 == MPM_PAGE) { ARG(5) = MAIN_PAGE; return 0; }
    if (id == OPTS_PAGE && msg == 0x4C && g_mp == 2) mp_no_handicaps();
    if (id == OPTS_PAGE && msg == 0x4C && g_mp_skip) { g_mp_skip = 0; ARG(5) = CHAR_PAGE; return 0; }
    if (id != MPM_PAGE && id != ONL_PAGE) return 0;
    uint32_t ms = 0x25F1D0u + (ctx & 0xFFu) * 0x1D8u;   /* this player's menu manager, as the game's handlers */
    uint32_t fade = id == MPM_PAGE ? MPM_FADE : ONL_FADE;
    static unsigned nlog;
    if (msg != 0x50 && nlog++ < 60) fprintf(stderr, "[NF-MP] %s page message %X (a5 %08X)\n", id == MPM_PAGE ? "Multiplayer" : "Online", msg, a5);
    if (msg == 0x4C) {   /* opened: as the main page does, fade in from black and menu input back after 15 frames */
        if (id == MPM_PAGE) g_mp = 0;
        else mp_no_handicaps();
        g_mp_host = 0; g_mp_skip = 0;
        menu_send(ctx, fade, 0x1A, 0xFFu, 0);
        nd3d_call_cdecl(sub_00072630, 4, ms, 0x68u, 1u, 0u);
        nd3d_call_cdecl(sub_00073240, 5, 0xFu, ms, 0x68u, 0u, 0u);
        menu_send(ctx, MAIN_DESC, 0x75, 0x64u, 0);
        mp_fade(ctx, fade, 0);
    } else if (msg == 0x4B && a5) {
        uint32_t b = G32(a5 + 0x18u);
        if (b == MPM_LOCAL || b == MPM_ONLINE) {   /* as the main page's Multiplayer: fade out, then Join Game */
            g_mp = b == MPM_LOCAL ? 1 : 2;
            fprintf(stderr, "[NF-MP] %s\n", g_mp == 1 ? "Local" : "Online");
            nd3d_call_cdecl(sub_00072630, 4, ms, 0x68u, 1u, 0u);
            mp_fade(ctx, fade, 0xFFu);
            nd3d_call_cdecl(sub_00073240, 5, 0x13u, ms, 0x44u, JOIN_PAGE, 0u);
        } else if (b == ONL_HOST) {   /* as Join Game opens Select Scenario */
            g_mp_host = 1;
            fprintf(stderr, "[NF-MP] Online: Host Game (the match runs on this PC until the network step)\n");
            nd3d_call_cdecl(sub_00092C40, 4, ms, 0x44u, SCEN_PAGE, 0u);
        } else if (b == ONL_FIND || b == ONL_IP)
            fprintf(stderr, "[NF-MP] Online: %s is not active yet\n", b == ONL_FIND ? "Find Games" : "Join by IP Address");
    }
    return 1;
}
