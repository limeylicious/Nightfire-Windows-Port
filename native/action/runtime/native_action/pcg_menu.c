/* "PC Graphics" page in the game's own front-end menu (NF_OVERLAY=1 only;
 * native-driving/ingame-menu/BUILD.md and MENU-DATA.md).
 *
 * Codename menu > Options > Audio/Video gets a "PC Graphics" button (a copy of
 * the "Credits" row) that opens a new page 0x40000060, built at load time from
 * copies of the Audio/Video page's own records: four rows with left/right
 * pickers for Picture shape, Resolution, Brightness, Scaling and Display mode. The values are
 * the F10 overlay's settings (lean_overlay.inc nf_overlay_get/set: same live
 * settings, same save file). Nothing on disk changes: the menu blob is extended
 * in guest memory when the front end loads.
 *
 * Four generated routines are replaced (scripts/pcg_menu_rename.py renames the
 * originals to orig_sub_...); without NF_OVERLAY=1 each simply calls its original:
 *   0x092B50 MenuManager_Load(size, blob)        extended front-end blob
 *   0x08E320 Handler_HandleMessage(ctx, obj, msg, a4, a5)   page 0x40000060
 *   0x06D460 Txt_BindLabel(id, ...)              text for string group 0x7F
 *   0x0959E0 Page_SetHelpText(page)              description line for our controls
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
#define PCG_ROW0     0x100002F1u   /* rows 0x100002F1..F5 */
#define PCG_APPLY    0x100002F7u   /* buttons, copies of "Restore Defaults" (0x100002F5/F6 before the Display Mode row) */
#define PCG_OK       0x100002F8u
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

#define ROWS 5
static const char *const row_label[ROWS] = { "Picture Shape", "Resolution", "Brightness", "Scaling", "Display Mode" };
static const char *const row_items[ROWS][5] = {
    { "Stretch", "Keep 4:3", "Widescreen 16:9" },
    { "Original", "2x", "3x", "4x", "Match Window" },
    { "Off", "Low", "Medium", "High" },
    { "Smooth", "Sharp" },
    { "Windowed", "Fullscreen", "Borderless" },
};
static const int row_count[ROWS] = { 3, 5, 4, 2, 3 };
/* Description line shown while a control is highlighted (Page_SetHelpText). */
#define HELP_BUTTON "Adjust resolution, brightness and sharpness settings"
static const char *const row_help[ROWS] = {
    "Choose the picture shape. Widescreen needs a restart.",
    "Draw the game at a higher resolution.",
    "Brighten the picture.",
    "Smooth or sharp scaling.",
    "Play in a window, fullscreen, or borderless fullscreen.",
};
static uint32_t g_items[ROWS][5], g_labels[ROWS], g_button, g_title, g_empty, g_help_button, g_help[ROWS];
static uint32_t g_apply, g_ok, g_help_apply, g_help_ok;

static void strings_init(void)
{
    if (g_empty) return;
    g_empty = gstr("");
    g_button = gstr("PC Graphics");
    g_title = gstr("PC Graphics");
    g_help_button = gstr(HELP_BUTTON);
    g_apply = gstr("Apply"); g_ok = gstr("OK");
    g_help_apply = gstr("Save these settings.");
    g_help_ok = gstr("Save these settings and return to AV Options.");
    for (int r = 0; r < ROWS; r++) {
        g_labels[r] = gstr(row_label[r]);
        g_help[r] = gstr(row_help[r]);
        for (int i = 0; i < row_count[r]; i++) g_items[r][i] = gstr(row_items[r][i]);
    }
}

/* ------------------------------------------------------------ Txt_BindLabel */
int nf_overlay_get(int which);
void nf_overlay_set(int which, int v);
int nf_overlay_widescreen_told(void);

static void shape_hint(void)   /* "Picture Shape (restart to apply)" while the game runs with the other choice */
{
    int s = nf_overlay_get(0), told = nf_overlay_widescreen_told();
    char b[64];
    snprintf(b, sizeof b, "%s%s", row_label[0], (s == 2) != (told != 0) ? " (restart to apply)" : "");
    gstr_set(g_labels[0], b);
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
    else if (id >= TXT_ROW0 && id < TXT_ROW0 + ROWS) { if (id == TXT_ROW0) shape_hint(); p = g_labels[id - TXT_ROW0]; }
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

static uint32_t g_blob, g_blob_cap;   /* the extended copy (guest) */
static uint32_t extend_blob(uint32_t src, uint32_t size_hint)
{
    uint32_t limit = (size_hint > 0x1000 && size_hint < 0x400000) ? size_hint : 0x100000;
    B = (const uint8_t *)ND3D_GPTR(src);
    if (ri32(0) != -6 || !walk(limit)) return 0;
    unsigned av = nrec, avend = 0, endrec = nrec - 1;
    for (unsigned i = 0; i < nrec; i++) {
        if (recs[i].tag == -16 && ru32(recs[i].off + 4) == PCG_PAGE) return 0;   /* already extended */
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
    /* 2. its groups, with the "PC Graphics" button (a copy of Credits) after the trailer button */
    int after = gtrailer >= 0 ? gtrailer : (int)ng - 1;
    for (unsigned k = 0; k < ng; k++) {
        put_group(grp[k], 0xFFFFFFFFu, 0, 0, 0, 0);
        if ((int)k == after)
            put_group(grp[gcredits], PCG_BUTTON, 0x170, TXT_BUTTON, PCG_PAGE, 0);
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
    for (unsigned r = 0; r < ROWS; r++) {
        uint16_t y = (uint16_t)(0x56 + 0x1C * r);
        put_group(grp[grow], PCG_ROW0 + r, y, TXT_ROW0 + r, 0, 0);
        put_group(grp[gpick], PCG_ROW0 + r, y, 0, 0, 0);
    }
    put_group(grp[gdefaults], PCG_APPLY, 0xE4, TXT_APPLY, 0, 0);   /* "Apply" (0xD4 with four rows) */
    put_group(grp[gdefaults], PCG_OK, 0x100, TXT_OK, 0, 0);        /* "OK" (0xF0 with four rows) */
    put32(nblk + 4 + 13, olen - (nblk + recs[av].size));
    /* 5. the end record and anything after it */
    put(B + recs[endrec].off, recs[endrec].size);
    if (olen > ocap) return 0;
    fprintf(stderr, "[NF-OVERLAY] PC Graphics page added to the front-end menu (%u -> %u bytes)\n", blen, olen);
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
    if (id == PCG_BUTTON || id == PCG_APPLY || id == PCG_OK || (id >= PCG_ROW0 && id < PCG_ROW0 + ROWS)) {
        strings_init();
        text = id == PCG_BUTTON ? g_help_button : id == PCG_APPLY ? g_help_apply : id == PCG_OK ? g_help_ok : g_help[id - PCG_ROW0];
    }
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

static uint32_t menu_send(uint32_t ctx, uint32_t id, uint32_t msg, uint32_t p1, uint32_t p2)
{
    return nd3d_call_cdecl(sub_00072820, 5, ctx, id, msg, p1, p2);
}
static void page_fill(uint32_t ctx)
{
    strings_init(); shape_hint();
    for (int r = 0; r < ROWS; r++) {
        uint32_t id = PCG_ROW0 + (uint32_t)r;
        int v = nf_overlay_get(r); if (v < 0 || v >= row_count[r]) v = 0;
        menu_send(ctx, id, 0x17, 0, 0);                                   /* clear */
        for (int i = 0; i < row_count[r]; i++) menu_send(ctx, id, 0x10, g_items[r][i], (uint32_t)i);   /* add item, value i */
        menu_send(ctx, id, 0x1E, (uint32_t)v, 0);                         /* select */
        g_shown[r] = v;
    }
    fprintf(stderr, "[NF-OVERLAY] PC Graphics page filled: shape %d resolution %d brightness %d scaling %d display %d\n",
            g_shown[0], g_shown[1], g_shown[2], g_shown[3], g_shown[4]);
}
/* Read every picker; save what changed. why: "live", "Apply", "OK". */
static void page_read(uint32_t ctx, const char *why)
{
    static uint32_t last[ROWS] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu }; static unsigned nlog;
    int v[ROWS], changed = 0;
    for (int r = 0; r < ROWS; r++) {
        uint32_t x = menu_send(ctx, PCG_ROW0 + (uint32_t)r, 0x35, 0, 0);   /* the picker's value */
        if (x != last[r] && nlog < 120) { nlog++; fprintf(stderr, "[NF-OVERLAY] PC Graphics row %d reads %08X (%s)\n", r, x, why); }
        last[r] = x;
        v[r] = ((int)x >= 0 && (int)x < row_count[r]) ? (int)x : g_shown[r];
        if (v[r] != g_shown[r]) { g_shown[r] = v[r]; nf_overlay_set(r, v[r]); changed = 1; }
    }
    if (changed && v[0] >= 0) shape_hint();
    if (strcmp(why, "live"))
        fprintf(stderr, "[NF-OVERLAY] PC Graphics %s: shape %d resolution %d brightness %d scaling %d display %d (saved)\n", why, v[0], v[1], v[2], v[3], v[4]);
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
    if (id == AV_PAGE && msg == 0x4B && a5 && G32(a5 + 0x18) == PCG_BUTTON) {   /* as "Credits": the button's script changes page */
        G8(FLAG_AVSKIP) = 1;
        RET(1, 0);
    }
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

