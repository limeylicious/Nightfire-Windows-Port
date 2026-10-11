/* nf_net.h - netplay core (online plan step 4): lockstep presses over UDP.
 * See nf_net.c for how it works and the switches. */
#pragma once
#include <stdint.h>

#define NF_NET_PORTS 4     /* game controller ports */
#define NF_NET_PAD   18    /* bytes of one port's state (as nightfire_input.c builds it) */
#define NF_NET_PEERS 4     /* PCs in a match, host included */

/* Ids every PC in a match must share: the build, the game files (XBE checksum)
 * and the settings that change play. Call before nf_net_init. */
void nf_net_set_ids(uint64_t build, uint64_t game, uint64_t settings);

/* Reads the NF_NET switches. 0 = netplay off; 1 = the match has started (this
 * blocks until every PC has joined); -1 = could not start (logged). */
int nf_net_init(void);
int nf_net_on(void);

/* This PC's players use game ports first_port .. first_port + count - 1. */
void nf_net_local_ports(unsigned *first_port, unsigned *count);

/* Input frame `frame` (0, 1, 2, ... in order): `local` holds this PC's players'
 * states read now (local[0] = this PC's first player); they are used at frame
 * frame + delay. Fills `out` with every port's agreed state for `frame`, waiting
 * for the other PCs when needed. Returns 1, or 0 once the session has ended
 * (host left, this PC was dropped, or a desync); `out` then holds `local` on
 * this PC's ports and neutral states elsewhere. */
int nf_net_frame(uint32_t frame, const uint8_t local[][NF_NET_PAD], uint8_t out[NF_NET_PORTS][NF_NET_PAD]);

/* Extra microseconds the game should wait before its next frame (0 normally,
 * at most 2000): a PC running ahead of the others slows down a little so that
 * nobody has to stop and wait. Call once per frame, from the frame pacer. */
int nf_net_pace_us(void);

/* A state hash for `frame` (same frames on every PC, e.g. every 60th): compared
 * with the others; a difference ends the session with a desync message. */
void nf_net_hash(uint32_t frame, uint64_t hash);

/* Lobby / status information. */
int nf_net_peers(void);                                  /* PCs in the session */
int nf_net_peer(int i, char name[17], unsigned *first_port, unsigned *count, int *ping_ms, int *dropped);
const char *nf_net_status(void);                         /* "" while all is well */
uint32_t nf_net_delay(void);
int nf_net_joined(void);                                 /* guest: the host has let this PC in */

/* This PC leaves: the others are told at once (its players stand still there),
 * and nf_net_init / nf_net_frame return as for an ended session. */
void nf_net_leave(void);

void nf_net_shutdown(void);
