/* Key binds (NF_OVERLAY=1 only): which keys and mouse buttons do each action, for
 * both games, kept in one file next to the F10 settings:
 * %LOCALAPPDATA%\NightfirePC\key-binds.ini. The store and the Key Binds screen are
 * in lean_binds.inc (included by lean_overlay.inc; the same file in
 * nightfire-driving-native and nightfire-port-native). The input code asks it here:
 * Action fb_present.c, Driving driving_input224.c.
 *
 * Bind codes: 1-255 are Windows virtual-key codes (the mouse buttons are VK_LBUTTON 1,
 * VK_RBUTTON 2, VK_MBUTTON 4, VK_XBUTTON1 5, VK_XBUTTON2 6), NFB_WHEEL_UP and
 * NFB_WHEEL_DOWN are the mouse wheel (two codes Windows leaves unused), 0 = no key.
 * Without NF_OVERLAY=1 nf_binds_on() is 0 and both games keep their fixed keys. */
#ifndef NF_BINDS_H
#define NF_BINDS_H
#define NFB_WHEEL_UP   0x0E
#define NFB_WHEEL_DOWN 0x0F
enum { NFB_ACTION, NFB_DRIVING, NFB_GAMES };
/* On foot (Action). Defaults are the keys Action had before binds. */
enum { NFA_FORWARD, NFA_BACK, NFA_LEFT, NFA_RIGHT, NFA_FIRE, NFA_AIM, NFA_USE, NFA_JUMP, NFA_CROUCH,
       NFA_ALTFIRE, NFA_WEAPON, NFA_GADGET, NFA_OBJECTIVES, NFA_PAUSE, NFA_SELECT, NFA_MENUBACK,
       NFA_DUP, NFA_DDOWN, NFA_DLEFT, NFA_DRIGHT, NFA_COUNT };
/* Driving: one row per pad input (the game's own control files decide what each does).
 * Defaults are the keys Driving had before binds. */
enum { NFD_LEFT, NFD_RIGHT, NFD_UP, NFD_DOWN, NFD_RT, NFD_LT, NFD_A, NFD_B, NFD_X, NFD_Y,
       NFD_WHITE, NFD_BLACK, NFD_LS, NFD_RS, NFD_BACK, NFD_START, NFD_SELECT, NFD_MENUBACK,
       NFD_DUP, NFD_DDOWN, NFD_DLEFT, NFD_DRIGHT, NFD_COUNT };
/* 1 when the binds decide the keys (NF_OVERLAY=1). Call once per input sample:
 * it also picks up a key-binds file changed by the other game. */
int nf_binds_on(void);
/* The two codes bound to an action (0 = none). */
void nf_binds_get(int game, int action, unsigned char out[2]);
/* Mouse wheel: the window code adds notches (+ up, - down); each becomes a short
 * press of NFB_WHEEL_UP / NFB_WHEEL_DOWN, read with nf_binds_wheel_down. */
void nf_binds_wheel(int notches);
int nf_binds_wheel_down(int code);
/* Opens the Key Binds screen over the game (Action's PC Options page). */
void nf_overlay_open_binds(void);
#endif
