/* nf_net.c - netplay core for the native port (online plan step 4).
 *
 * Lockstep: every PC runs the whole match and only controller states travel.
 * One PC hosts and up to three guests join it. Guests talk only to the host
 * (a star), so only the host needs a port others can reach. For each game
 * input frame the host builds the agreed state of all four game ports from
 * every PC's presses and sends it to the guests; no PC runs frame F before it
 * holds the agreed states for F. Presses read at frame F are used at frame
 * F + delay, which hides the round trip to the host.
 *
 * Packets carry every frame the other side has not confirmed yet (up to 16,
 * plus the newest 16 when more are missing), so a lost packet needs no resend
 * of its own: the next one covers it. Each side reports its latest state hash
 * (nf_net_hash) in every packet; a difference stops the match with a message
 * in both logs. Ping is measured twice a second, and the host sets everyone's
 * input delay from the slowest ping (the delay changes which frame a press
 * lands on, never what the copies agree on, so changing it is safe).
 *
 * A PC that sends nothing for NF_NET_DROP seconds (default 20) is dropped by
 * the host: from the first frame the host lacks, that PC's players stand still
 * on every copy (a bot takes over later, plan step 11). Guests that lose the
 * host stop the match.
 *
 * The lobby (NF_NET=lobby) is a small window where the player picks a role:
 * it lists the matches hosted on the local network (a broadcast "who is
 * hosting?" once a second, answered by every host, with each match's players,
 * ping and whether this PC can join it), joins one of them or a typed address,
 * or hosts a match. The host sees who has joined and starts the match when
 * everyone is in (2 to 4 PCs); everyone in it sees the same list.
 *
 * Switches (netplay is off unless NF_NET is set):
 *   NF_NET=lobby | host | join   lobby window, or a fixed role with no window
 *   NF_NET_ADDR=address[:port]   join: the host (default port 47610)
 *   NF_NET_PORT=n                host and lobby: the match's UDP port (default 47610);
 *                                join: local port (default any)
 *   NF_NET_FIND=address,...      lobby: more places to look for matches besides the
 *                                local network (e.g. Tailscale addresses)
 *   NF_NET_LOBBY_AUTO=...        test only: host:N (host, start at N PCs), join (the first
 *                                match found), join:ADDR, leave:S (back to the list after
 *                                S s), quit:S (close the lobby after S s); comma-separated
 *   NF_NET_PLAYERS=n             host: PCs in the match, host included (default 2)
 *   NF_NET_LOCAL=n               players on this PC (default 1; the lobby can change it)
 *   NF_NET_DELAY=n               host: least input delay in frames (default 2 = 33 ms); the host
 *                                raises it to cover the slowest ping and lowers it again slowly
 *   NF_NET_NAME=text             this PC's name for the others (default the computer name,
 *                                or in the lobby the name typed there last time)
 *   NF_NET_DROP=s                seconds of silence before a PC is dropped (default 20)
 *   NF_NET_LOG=file              log file (default stderr)
 *   NF_NET_SIM=delay=ms,jitter=ms,loss=percent,cut=at_s:len_s
 *                                test only: delay/jitter/loss on this PC's sends; cut = no
 *                                packets in or out from at_s for len_s seconds
 *
 * The lobby remembers the name, players and last typed address in
 * %LOCALAPPDATA%\NightfirePC\online.ini.
 *
 * Uses only Winsock and plain Windows controls (nothing to install). Written
 * from the Winsock and Win32 documentation. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nf_net.h"

#define NET_MAGIC    0x314E464Eu          /* "NFN1" */
#define NET_PROTO    1u
#define NET_DEFPORT  47610u
#define NET_W        256u                 /* frames kept */
#define NET_BATCH    16u                  /* frames in one packet */
#define NET_MAXPKT   1400
#define NET_HASHES   16
#define NET_NOHASH   0xFFFFFFFFu
#define NET_FRAME    (NF_NET_PORTS * NF_NET_PAD)
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

enum { T_HELLO = 1, T_WELCOME, T_REJECT, T_START, T_INPUT, T_FRAMES, T_PING, T_PONG, T_BYE, T_STOP,
       T_FIND, T_HERE, T_LOBBY };
enum { R_FULL = 1, R_PROTO, R_BUILD, R_GAME, R_SETTINGS, R_STARTED, R_PLAYERS };
static const char *const k_reason[] = { "", "the match is full", "a different netplay version",
    "a different build of the game", "different game files", "different game settings",
    "the match has already started", "too many players on that PC" };
static const char *const k_short[] = { "open", "full", "other version", "other build",
    "other game files", "other settings", "started", "too many players" };
#define ROLE_BROWSE 3                    /* lobby: looking for matches, not in one yet */

typedef struct { uint32_t frame; uint64_t h; int set; } hash_t;

typedef struct {
    int used;
    struct sockaddr_in addr;
    char name[17];
    unsigned base, nlocal;
    uint32_t nonce;
    LONG64 last_heard, last_send, last_ping, last_start;
    int rtt_ms; double srtt, rttvar;
    double adv;                          /* frames this PC runs ahead of that one (smoothed) */
    int adv_set;
    hash_t hashes[NET_HASHES]; unsigned nhash;
    /* host side, per guest (and the host's own presses in peer 0) */
    uint32_t in_next;                    /* presses held for every frame below this */
    uint32_t in_stamp[NET_W];            /* frame + 1 held in each slot */
    uint8_t in[NET_W][NF_NET_PORTS][NF_NET_PAD];
    uint32_t agreed_acked;               /* the guest holds every agreed frame below this */
    int started_ack, dropped;
    uint32_t drop_from;
} peer_t;

static int s_role;                       /* 0 off, 1 host, 2 guest, 3 lobby looking for matches */
static volatile int s_state;             /* 0 joining, 1 running, 2 ended, -1 failed */
static SOCKET s_sock = INVALID_SOCKET;
static CRITICAL_SECTION s_cs;
static HANDLE s_ev, s_thread;
static volatile LONG s_stop;
static FILE *s_log;
static LONG64 s_qpf, s_t0;
static uint64_t s_build, s_game, s_settings;
static uint32_t s_session, s_delay = 2, s_min_delay = 2, s_nonce;
static LONG64 s_delay_t, s_start_at;
static unsigned s_need = 2, s_nlocal = 1;
static LONG64 s_drop_us = 20000000;
static char s_name[17];
static peer_t s_peer[NF_NET_PEERS];      /* host: 0 = this PC; guest: 0 = the host, others for display */
static int s_me = -1;                    /* this PC's index in the host's list */
static unsigned s_my_base;
static uint32_t s_agreed_next, s_agreed_stamp[NET_W];
static uint8_t s_agreed[NET_W][NF_NET_PORTS][NF_NET_PAD];
static uint32_t s_my_next, s_host_has;   /* guest: own presses submitted / held by the host */
static uint8_t s_my_in[NET_W][NF_NET_PORTS][NF_NET_PAD];
static hash_t s_my_hash[NET_HASHES]; static unsigned s_my_nhash;
static uint32_t s_last_hf = NET_NOHASH; static uint64_t s_last_h;
static LONG64 s_join_t, s_hello_t, s_input_t, s_wait_log_t;
static char s_status[200];
static uint32_t s_wait_frame; static LONG64 s_wait_since; static int s_waiting;
static uint32_t s_frames, s_waits; static LONG64 s_wait_us, s_wait_max;
static uint32_t s_cur_frame; static LONG64 s_cur_t; static int s_ran;   /* the frame the game is on, and since when */
static LONG64 s_pace_total;
/* the lobby */
typedef struct {
    int used; uint32_t session; struct sockaddr_in addr; char name[17];
    unsigned pcs, max, players, state, why; int ping_ms; LONG64 seen;
} found_t;
static int s_lobby;                      /* NF_NET=lobby */
static volatile LONG s_cancelled;        /* the player closed the lobby */
static unsigned s_port = NET_DEFPORT;    /* the match's port (host listens, lobby looks there) */
static found_t s_found[16];
static struct { int used; char name[17]; unsigned nlocal; int ping_ms; } s_lob[NF_NET_PEERS];   /* guest: the host's list */
static int s_lob_starting;
static struct sockaddr_in s_targets[24]; static int s_ntargets;
static char s_my_addrs[96];              /* this PC's addresses, for the host to pass on */
static LONG64 s_find_t, s_lobby_t;
static SOCKET s_old_sock = INVALID_SOCKET;   /* closed by the network thread once it has let go */

/* ---- clock and log ---------------------------------------------------------- */
static LONG64 now_us(void)
{
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (c.QuadPart - s_t0) * 1000000 / s_qpf;
}
static void say(const char *fmt, ...)
{
    FILE *f = s_log ? s_log : stderr; va_list a; va_start(a, fmt);
    fprintf(f, "[NF-NET %8.3f] ", now_us() / 1e6); vfprintf(f, fmt, a); fputc('\n', f); fflush(f);
    va_end(a);
}

/* ---- packet writing / reading (little-endian) ------------------------------- */
typedef struct { uint8_t p[NET_MAXPKT]; int n, bad; } wbuf;
typedef struct { const uint8_t *p; int n, pos, bad; } rbuf;
static void w8(wbuf *b, uint32_t v) { if (b->n >= NET_MAXPKT) { b->bad = 1; return; } b->p[b->n++] = (uint8_t)v; }
static void w16(wbuf *b, uint32_t v) { w8(b, v); w8(b, v >> 8); }
static void w32(wbuf *b, uint32_t v) { w16(b, v); w16(b, v >> 16); }
static void w64(wbuf *b, uint64_t v) { w32(b, (uint32_t)v); w32(b, (uint32_t)(v >> 32)); }
static void wmem(wbuf *b, const void *s, int n) { if (b->n + n > NET_MAXPKT) { b->bad = 1; return; } memcpy(b->p + b->n, s, n); b->n += n; }
static uint32_t r8(rbuf *b) { if (b->pos >= b->n) { b->bad = 1; return 0; } return b->p[b->pos++]; }
static uint32_t r16(rbuf *b) { uint32_t v = r8(b); return v | r8(b) << 8; }
static uint32_t r32(rbuf *b) { uint32_t v = r16(b); return v | r16(b) << 16; }
static uint64_t r64(rbuf *b) { uint64_t v = r32(b); return v | (uint64_t)r32(b) << 32; }
static void rmem(rbuf *b, void *d, int n) { if (b->pos + n > b->n) { b->bad = 1; memset(d, 0, n); return; } memcpy(d, b->p + b->pos, n); b->pos += n; }
static void head(wbuf *b, int type) { b->n = b->bad = 0; w32(b, NET_MAGIC); w8(b, type); w8(b, s_me < 0 ? 0xFF : s_me); w16(b, 0); w32(b, s_session); }

/* ---- sending, with the optional simulated bad connection --------------------- */
static struct { int on, delay, jitter, loss; LONG64 cut_at, cut_len; uint32_t rng; } s_sim;
typedef struct { LONG64 due; struct sockaddr_in to; int n; uint8_t d[NET_MAXPKT]; } simpkt;
static simpkt s_simq[512]; static int s_simn;
static uint32_t sim_rnd(void) { uint32_t x = s_sim.rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return s_sim.rng = x; }
static int sim_cut(void) { LONG64 t = now_us(); return s_sim.cut_len > 0 && t >= s_sim.cut_at && t < s_sim.cut_at + s_sim.cut_len; }
static void raw_send(const void *d, int n, const struct sockaddr_in *to) { sendto(s_sock, (const char *)d, n, 0, (const struct sockaddr *)to, sizeof *to); }
static void send_to(wbuf *b, const struct sockaddr_in *to)
{
    if (b->bad) { say("packet too big (type %u), not sent", b->p[4]); return; }
    if (!s_sim.on) { raw_send(b->p, b->n, to); return; }
    if (sim_cut()) return;
    if (s_sim.loss && (int)(sim_rnd() % 10000u) < s_sim.loss * 100) return;
    if (s_simn >= (int)(sizeof s_simq / sizeof s_simq[0])) return;
    simpkt *q = &s_simq[s_simn++];
    q->due = now_us() + s_sim.delay * 1000ll + (s_sim.jitter ? (LONG64)(sim_rnd() % (unsigned)(s_sim.jitter * 1000 + 1)) : 0);
    q->to = *to; q->n = b->n; memcpy(q->d, b->p, b->n);
}
static void sim_flush(void)
{
    LONG64 t = now_us();
    for (int i = 0; i < s_simn; )
        if (s_simq[i].due <= t) { raw_send(s_simq[i].d, s_simq[i].n, &s_simq[i].to); s_simq[i] = s_simq[--s_simn]; }
        else i++;
}
static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b)
{ return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port; }
static const char *addr_text(const struct sockaddr_in *a)
{
    static char t[4][32]; static int k; char *s = t[k++ & 3]; const uint8_t *ip = (const uint8_t *)&a->sin_addr;
    snprintf(s, 32, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], ntohs(a->sin_port)); return s;
}

/* ---- ending the session ------------------------------------------------------ */
static void end_session(const char *fmt, ...)
{
    if (s_state == 2) return;
    va_list a; va_start(a, fmt); vsnprintf(s_status, sizeof s_status, fmt, a); va_end(a);
    s_state = 2; say("SESSION ENDED: %s", s_status); SetEvent(s_ev);
}
static void send_stop(uint32_t frame)
{
    wbuf b; head(&b, T_STOP); w32(&b, frame);
    for (int r = 0; r < 3; r++)
        for (int i = 0; i < NF_NET_PEERS; i++)
            if (s_peer[i].used && i != s_me && (s_role == 2 ? i == 0 : !s_peer[i].dropped)) send_to(&b, &s_peer[i].addr);
}

/* ---- state hashes ------------------------------------------------------------ */
static void hash_compare(int i)
{
    peer_t *p = &s_peer[i];
    for (unsigned a = 0; a < NET_HASHES; a++) {
        if (!p->hashes[a].set) continue;
        for (unsigned m = 0; m < NET_HASHES; m++) {
            if (!s_my_hash[m].set || s_my_hash[m].frame != p->hashes[a].frame) continue;
            if (s_my_hash[m].h != p->hashes[a].h) {
                uint32_t f = p->hashes[a].frame;
                say("DESYNC at frame %u: this PC %016llx, %s %016llx", f, (unsigned long long)s_my_hash[m].h, p->name, (unsigned long long)p->hashes[a].h);
                send_stop(f);
                end_session("Desync at frame %u with %s: the copies of the match no longer agree, so it stopped. Keep this log and theirs.", f, p->name);
                return;
            }
            p->hashes[a].set = 0;      /* compared */
            break;
        }
    }
}
static void peer_hash(int i, uint32_t f, uint64_t h)
{
    peer_t *p = &s_peer[i];
    if (f == NET_NOHASH) return;
    for (unsigned a = 0; a < NET_HASHES; a++) if (p->hashes[a].set && p->hashes[a].frame == f) return;
    hash_t *e = &p->hashes[p->nhash++ % NET_HASHES]; e->frame = f; e->h = h; e->set = 1;
    hash_compare(i);
}

/* ---- time sync ----------------------------------------------------------------
 * Every packet says which frame the sender is on and how long it has been on it.
 * With the ping that gives where the other PC is now; a PC that runs ahead of
 * the others slows down a little (nf_net_pace_us) instead of stopping to wait. */
#define NET_FRAME_US 16666.667
static double my_pos(LONG64 t) { return s_ran ? s_cur_frame + (t - s_cur_t) / NET_FRAME_US : 0; }
static void w_pos(wbuf *b)
{
    LONG64 age = s_ran ? now_us() - s_cur_t : 0;
    w32(b, s_ran ? s_cur_frame : 0xFFFFFFFFu); w32(b, (uint32_t)(age > 0x7FFFFFFF ? 0x7FFFFFFF : age));
}
static void r_pos(peer_t *p, uint32_t frame, uint32_t age_us)
{
    if (frame == 0xFFFFFFFFu || !s_ran || p->srtt <= 0) return;
    LONG64 t = now_us();
    double there = frame + (age_us + p->srtt * 500.0) / NET_FRAME_US;
    double adv = my_pos(t) - there;
    if (adv > 600 || adv < -600) return;
    if (!p->adv_set) { p->adv = adv; p->adv_set = 1; } else p->adv += (adv - p->adv) / 16;
}

/* ---- host ------------------------------------------------------------------ */
static void host_frames_pkt(peer_t *g, uint32_t first, uint32_t n)
{
    wbuf b; head(&b, T_FRAMES);
    w32(&b, g->in_next); w32(&b, first); w8(&b, n); w8(&b, s_delay); w32(&b, s_last_hf); w64(&b, s_last_h); w_pos(&b);
    for (uint32_t k = 0; k < n; k++) wmem(&b, s_agreed[(first + k) % NET_W], NET_FRAME);
    send_to(&b, &g->addr); g->last_send = now_us();
}
static void host_send_frames(peer_t *g)
{
    uint32_t a = g->agreed_acked, n = s_agreed_next - a;
    if (n <= NET_BATCH) host_frames_pkt(g, a, n);
    else { host_frames_pkt(g, a, NET_BATCH); host_frames_pkt(g, s_agreed_next - NET_BATCH, NET_BATCH); }
}
static void host_pick_delay(LONG64 t);
static int gone_at(const peer_t *p, uint32_t f) { return p->dropped && f >= p->drop_from; }
static void host_advance(void)
{
    int moved = 0;
    for (;;) {
        uint32_t f = s_agreed_next, slot = f % NET_W;
        int ready = 1;
        for (int i = 0; i < NF_NET_PEERS; i++)
            if (s_peer[i].used && !gone_at(&s_peer[i], f) && s_peer[i].in_next <= f) ready = 0;
        if (!ready) break;
        memset(s_agreed[slot], 0, NET_FRAME);
        for (int i = 0; i < NF_NET_PEERS; i++) {
            peer_t *p = &s_peer[i];
            if (!p->used || gone_at(p, f)) continue;
            for (unsigned k = 0; k < p->nlocal && p->base + k < NF_NET_PORTS; k++)
                memcpy(s_agreed[slot][p->base + k], p->in[slot][k], NF_NET_PAD);
        }
        s_agreed_stamp[slot] = f + 1; s_agreed_next = f + 1; moved = 1;
    }
    if (!moved) return;
    SetEvent(s_ev);
    for (int i = 1; i < NF_NET_PEERS; i++)
        if (s_peer[i].used && !s_peer[i].dropped && s_peer[i].started_ack) host_send_frames(&s_peer[i]);
}
static void host_start_pkt(peer_t *g)
{
    wbuf b; head(&b, T_START); w8(&b, s_delay);
    int n = 0; for (int i = 0; i < NF_NET_PEERS; i++) n += s_peer[i].used;
    w8(&b, n);
    for (int i = 0; i < NF_NET_PEERS; i++) {
        if (!s_peer[i].used) continue;
        w8(&b, i); w8(&b, s_peer[i].base); w8(&b, s_peer[i].nlocal); wmem(&b, s_peer[i].name, 16);
    }
    send_to(&b, &g->addr); g->last_start = now_us();
}
static void host_start(void)
{
    unsigned base = 0, n = 0;
    host_pick_delay(now_us());     /* before the first frames are set up: they depend on the delay */
    for (int i = 0; i < NF_NET_PEERS; i++) {
        peer_t *p = &s_peer[i];
        if (!p->used) continue;
        p->base = base; base += p->nlocal; n++;
        p->in_next = s_delay; p->agreed_acked = 0; p->last_heard = now_us();
    }
    s_my_base = s_peer[0].base;
    s_state = 1;
    say("match starting: %u PCs, %u players, input delay %u frames", n, base, s_delay);
    for (int i = 0; i < NF_NET_PEERS; i++)
        if (s_peer[i].used) say("  %s: ports %u-%u%s", s_peer[i].name, s_peer[i].base + 1, s_peer[i].base + s_peer[i].nlocal, i == 0 ? " (host)" : "");
    for (int i = 1; i < NF_NET_PEERS; i++) if (s_peer[i].used) host_start_pkt(&s_peer[i]);
    host_advance();          /* the first `delay` frames: everyone neutral */
    SetEvent(s_ev);
}
static void host_reject(const struct sockaddr_in *to, uint32_t nonce, int reason)
{
    wbuf b; head(&b, T_REJECT); w32(&b, nonce); w8(&b, reason);
    send_to(&b, to);
}
static void host_welcome(int i)
{
    wbuf b; head(&b, T_WELCOME); w32(&b, s_peer[i].nonce); w8(&b, i); w8(&b, s_delay);
    int now = 0; for (int k = 0; k < NF_NET_PEERS; k++) now += s_peer[k].used;
    w8(&b, s_need); w8(&b, now);
    send_to(&b, &s_peer[i].addr);
}
static void host_hello(rbuf *r, const struct sockaddr_in *from)
{
    uint32_t proto = r16(r), nl = r8(r); r8(r);
    uint64_t build = r64(r), game = r64(r), settings = r64(r);
    uint32_t nonce = r32(r); char name[17] = {0}; rmem(r, name, 16);
    if (r->bad) return;
    for (int i = 1; i < NF_NET_PEERS; i++)
        if (s_peer[i].used && same_addr(&s_peer[i].addr, from)) {
            if (s_peer[i].nonce == nonce) { s_peer[i].last_heard = now_us(); host_welcome(i); return; }
            if (s_state == 0) { say("%s rejoined from %s", s_peer[i].name, addr_text(from)); s_peer[i].used = 0; break; }
        }
    unsigned ports = 0, pcs = 0;
    for (int i = 0; i < NF_NET_PEERS; i++) if (s_peer[i].used) { ports += s_peer[i].nlocal; pcs++; }
    int why = proto != NET_PROTO ? R_PROTO : build != s_build ? R_BUILD : game != s_game ? R_GAME
            : settings != s_settings ? R_SETTINGS : s_state != 0 ? R_STARTED : (nl < 1 || nl > NF_NET_PORTS) ? R_PLAYERS
            : (pcs >= s_need || pcs >= NF_NET_PEERS || ports + nl > NF_NET_PORTS) ? R_FULL : 0;
    if (why) { say("turned away %s (%s): %s", name, addr_text(from), k_reason[why]); host_reject(from, nonce, why); return; }
    int i = 1; while (s_peer[i].used) i++;
    peer_t *p = &s_peer[i];
    memset(p, 0, sizeof *p);
    p->used = 1; p->addr = *from; memcpy(p->name, name, 16); p->name[16] = 0; p->nlocal = nl; p->nonce = nonce;
    p->last_heard = now_us();
    say("%s joined from %s with %u player%s (%u of %u PCs)", p->name, addr_text(from), nl, nl == 1 ? "" : "s", pcs + 1, s_need);
    host_welcome(i);
    /* a few pings first, to pick the delay; in the lobby the host presses Start instead */
    if (!s_lobby && pcs + 1 >= s_need && !s_start_at) s_start_at = now_us() + 700000;
}
static unsigned used_pcs(unsigned *players)
{
    unsigned n = 0, pl = 0;
    for (int i = 0; i < NF_NET_PEERS; i++) if (s_peer[i].used && !s_peer[i].dropped) { n++; pl += s_peer[i].nlocal; }
    if (players) *players = pl;
    return n;
}
/* Someone looking for matches: say what this one is and whether they can join it. */
static void host_found(rbuf *r, const struct sockaddr_in *from)
{
    uint32_t proto = r16(r); uint64_t ts = r64(r), build = r64(r), game = r64(r), settings = r64(r);
    if (r->bad) return;
    unsigned players, pcs = used_pcs(&players), max = s_lobby ? NF_NET_PEERS : s_need;
    int why = proto != NET_PROTO ? R_PROTO : build != s_build ? R_BUILD : game != s_game ? R_GAME
            : settings != s_settings ? R_SETTINGS : (s_state != 0 || s_start_at) ? R_STARTED
            : (pcs >= max || players >= NF_NET_PORTS) ? R_FULL : 0;
    wbuf b; head(&b, T_HERE); w16(&b, NET_PROTO); w64(&b, ts); wmem(&b, s_name, 16);
    w8(&b, pcs); w8(&b, max); w8(&b, players); w8(&b, s_state == 0 ? (s_start_at ? 1 : 0) : 2); w8(&b, why);
    send_to(&b, from);
}
/* Lobby: the host tells everyone in the match who else is in it. */
static void host_lobby_pkt(void)
{
    wbuf b; head(&b, T_LOBBY); w8(&b, s_start_at ? 1 : 0);
    unsigned n = 0; for (int i = 0; i < NF_NET_PEERS; i++) n += s_peer[i].used;
    w8(&b, n);
    for (int i = 0; i < NF_NET_PEERS; i++) {
        if (!s_peer[i].used) continue;
        w8(&b, i); w8(&b, s_peer[i].nlocal); w16(&b, i == 0 ? 0 : s_peer[i].rtt_ms); wmem(&b, s_peer[i].name, 16);
    }
    for (int i = 1; i < NF_NET_PEERS; i++) if (s_peer[i].used) send_to(&b, &s_peer[i].addr);
}
static void host_input(peer_t *g, int gi, rbuf *r)
{
    uint32_t ack = r32(r), first = r32(r), n = r8(r), nl = r8(r), hf = r32(r); uint64_t h = r64(r);
    uint32_t pf = r32(r), pa = r32(r);
    if (r->bad || nl != g->nlocal || n > NET_BATCH) return;
    g->started_ack = 1;
    r_pos(g, pf, pa);
    if (ack > g->agreed_acked && ack <= s_agreed_next) g->agreed_acked = ack;
    uint8_t st[NF_NET_PORTS][NF_NET_PAD];
    for (uint32_t k = 0; k < n; k++) {
        uint32_t f = first + k;
        rmem(r, st, nl * NF_NET_PAD);
        if (r->bad) return;
        if (g->dropped || f < g->in_next || f >= s_agreed_next + NET_W - NET_BATCH) continue;
        memcpy(g->in[f % NET_W], st, nl * NF_NET_PAD); g->in_stamp[f % NET_W] = f + 1;
    }
    while (g->in_stamp[g->in_next % NET_W] == g->in_next + 1) g->in_next++;
    peer_hash(gi, hf, h);
    host_advance();
}

/* ---- lobby: looking for matches -------------------------------------------- */
static void browse_find(LONG64 t)
{
    wbuf b; head(&b, T_FIND); w16(&b, NET_PROTO); w64(&b, (uint64_t)t); w64(&b, s_build); w64(&b, s_game); w64(&b, s_settings);
    for (int i = 0; i < s_ntargets; i++) send_to(&b, &s_targets[i]);
    s_find_t = t;
}
static void browse_here(rbuf *r, uint32_t session, const struct sockaddr_in *from, LONG64 t)
{
    uint32_t proto = r16(r); uint64_t ts = r64(r); char name[17] = {0}; rmem(r, name, 16);
    uint32_t pcs = r8(r), max = r8(r), players = r8(r), state = r8(r), why = r8(r);
    if (r->bad || proto != NET_PROTO || !session) return;
    if (why >= sizeof k_reason / sizeof k_reason[0]) why = R_PROTO;
    found_t *e = NULL, *spare = NULL;
    for (int i = 0; i < (int)(sizeof s_found / sizeof s_found[0]); i++) {
        if (s_found[i].used && s_found[i].session == session) { e = &s_found[i]; break; }
        if (!s_found[i].used && !spare) spare = &s_found[i];
    }
    if (!e) {
        if (!spare) return;
        e = spare; memset(e, 0, sizeof *e); e->used = 1; e->session = session; e->addr = *from;
        say("lobby: found %s's match at %s (%u of %u PCs, %s)", name, addr_text(from), pcs, max, why ? k_reason[why] : state ? "starting" : "open");
    } else if (from->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) e->addr = *from;   /* same PC: loopback always gets through */
    if (e->why != why && e->seen) say("lobby: %s's match is now %s", name, why ? k_short[why] : "open");
    memcpy(e->name, name, 16); e->name[16] = 0;
    e->pcs = pcs; e->max = max; e->players = players; e->state = state; e->why = why; e->seen = t;
    LONG64 rtt = t - (LONG64)ts;
    if (rtt >= 0 && rtt < 5000000) { int ms = (int)((rtt + 999) / 1000); e->ping_ms = e->ping_ms ? (e->ping_ms * 3 + ms + 2) / 4 : ms; }
}

/* ---- guest ------------------------------------------------------------------ */
static void guest_hello(void)
{
    wbuf b; head(&b, T_HELLO); w16(&b, NET_PROTO); w8(&b, s_nlocal); w8(&b, 0);
    w64(&b, s_build); w64(&b, s_game); w64(&b, s_settings); w32(&b, s_nonce); wmem(&b, s_name, 16);
    send_to(&b, &s_peer[0].addr); s_hello_t = now_us();
}
static void guest_input_pkt(uint32_t first, uint32_t n)
{
    wbuf b; head(&b, T_INPUT);
    w32(&b, s_agreed_next); w32(&b, first); w8(&b, n); w8(&b, s_nlocal); w32(&b, s_last_hf); w64(&b, s_last_h); w_pos(&b);
    for (uint32_t k = 0; k < n; k++) wmem(&b, s_my_in[(first + k) % NET_W], s_nlocal * NF_NET_PAD);
    send_to(&b, &s_peer[0].addr);
}
static void guest_send_input(void)
{
    uint32_t a = s_host_has, n = s_my_next - a;
    if (n <= NET_BATCH) guest_input_pkt(a, n);
    else { guest_input_pkt(a, NET_BATCH); guest_input_pkt(s_my_next - NET_BATCH, NET_BATCH); }
    s_input_t = now_us();
}
static void guest_frames(rbuf *r)
{
    uint32_t ack = r32(r), first = r32(r), n = r8(r), delay = r8(r);
    uint32_t hf = r32(r); uint64_t h = r64(r);
    uint32_t pf = r32(r), pa = r32(r);
    if (r->bad || n > NET_BATCH) return;
    r_pos(&s_peer[0], pf, pa);
    if (ack > s_host_has && ack <= s_my_next) s_host_has = ack;
    if (delay >= 1 && delay <= 30 && delay != s_delay) { say("input delay %u frames (host's choice)", delay); s_delay = delay; }
    uint8_t st[NF_NET_PORTS][NF_NET_PAD];
    for (uint32_t k = 0; k < n; k++) {
        uint32_t f = first + k;
        rmem(r, st, NET_FRAME);
        if (r->bad) return;
        if (f < s_agreed_next || f >= s_agreed_next + NET_W) continue;
        memcpy(s_agreed[f % NET_W], st, NET_FRAME); s_agreed_stamp[f % NET_W] = f + 1;
    }
    uint32_t before = s_agreed_next;
    while (s_agreed_stamp[s_agreed_next % NET_W] == s_agreed_next + 1) s_agreed_next++;
    if (s_agreed_next != before) SetEvent(s_ev);
    peer_hash(0, hf, h);
}
static void guest_lobby(rbuf *r)
{
    uint32_t starting = r8(r), n = r8(r);
    if (r->bad || n > NF_NET_PEERS) return;
    memset(s_lob, 0, sizeof s_lob);
    for (uint32_t k = 0; k < n; k++) {
        uint32_t i = r8(r), nl = r8(r), ping = r16(r); char name[17] = {0}; rmem(r, name, 16);
        if (r->bad || i >= NF_NET_PEERS) return;
        s_lob[i].used = 1; s_lob[i].nlocal = nl; s_lob[i].ping_ms = (int)ping; memcpy(s_lob[i].name, name, 17);
    }
    if (starting && !s_lob_starting) say("the host is starting the match");
    s_lob_starting = (int)starting;
}
static void guest_start(rbuf *r)
{
    uint32_t delay = r8(r), n = r8(r);
    if (r->bad || n < 1 || n > NF_NET_PEERS) return;
    peer_t host = s_peer[0];
    for (int i = 1; i < NF_NET_PEERS; i++) s_peer[i].used = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t i = r8(r), base = r8(r), nl = r8(r); char name[17] = {0}; rmem(r, name, 16);
        if (r->bad || i >= NF_NET_PEERS) return;
        peer_t *p = &s_peer[i];
        if (i == 0) *p = host;
        p->used = 1; p->base = base; p->nlocal = nl; memcpy(p->name, name, 16); p->name[16] = 0;
    }
    if (s_me < 0 || !s_peer[s_me].used) return;
    s_delay = delay; s_my_base = s_peer[s_me].base;
    s_my_next = s_host_has = s_delay; s_agreed_next = 0;
    s_state = 1;
    say("match starting: input delay %u frames; this PC has ports %u-%u", s_delay, s_my_base + 1, s_my_base + s_nlocal);
    for (int i = 0; i < NF_NET_PEERS; i++)
        if (s_peer[i].used) say("  %s: ports %u-%u%s", s_peer[i].name, s_peer[i].base + 1, s_peer[i].base + s_peer[i].nlocal, i == 0 ? " (host)" : i == s_me ? " (this PC)" : "");
    guest_send_input();
    SetEvent(s_ev);
}

/* ---- ping and input delay ------------------------------------------------------ */
static void rtt_sample(peer_t *p, LONG64 us)
{
    double ms = us / 1000.0;
    if (ms < 0 || ms > 10000) return;
    if (p->srtt <= 0) { p->srtt = ms; p->rttvar = ms / 2; }
    else { double d = ms - p->srtt; p->rttvar += ((d < 0 ? -d : d) - p->rttvar) / 4; p->srtt += d / 8; }
    p->rtt_ms = (int)(p->srtt + 0.5); if (!p->rtt_ms) p->rtt_ms = 1;
}
/* Host: a guest's press must reach the host and come back inside the delay. */
static void host_pick_delay(LONG64 t)
{
    double need = 0;
    for (int i = 1; i < NF_NET_PEERS; i++) {
        const peer_t *g = &s_peer[i];
        if (g->used && !g->dropped && g->srtt > 0 && g->srtt + 2 * g->rttvar > need) need = g->srtt + 2 * g->rttvar;
    }
    unsigned want = (unsigned)((need + 8.0) * 60.0 / 1000.0) + 1;
    if (want < s_min_delay) want = s_min_delay;
    if (want > 30) want = 30;
    if (want > s_delay || (want + 1 < s_delay && t - s_delay_t > 2000000)) {
        unsigned d = want > s_delay ? want : s_delay - 1;
        say("input delay %u frames (%.0f ms) for a ping of %.0f ms", d, d * 1000.0 / 60, need);
        s_delay = d; s_delay_t = t;
    }
}

/* ---- incoming packets -------------------------------------------------------- */
static void on_packet(const uint8_t *d, int n, const struct sockaddr_in *from)
{
    rbuf r = { d, n, 0, 0 };
    if (r32(&r) != NET_MAGIC) return;
    uint32_t type = r8(&r), sender = r8(&r); r16(&r); uint32_t session = r32(&r);
    if (r.bad) return;
    LONG64 t = now_us();
    if (type == T_FIND) { if (s_role == 1) host_found(&r, from); return; }
    if (s_role == ROLE_BROWSE) { if (type == T_HERE) browse_here(&r, session, from, t); return; }
    if (s_role == 1) {
        if (type == T_HELLO) { host_hello(&r, from); return; }
        int gi = -1;
        for (int i = 1; i < NF_NET_PEERS; i++) if (s_peer[i].used && same_addr(&s_peer[i].addr, from)) gi = i;
        if (gi < 0 || session != s_session || sender != (uint32_t)gi) return;
        peer_t *g = &s_peer[gi];
        if (g->dropped) return;
        g->last_heard = t;
        switch (type) {
        case T_INPUT: if (s_state == 1) host_input(g, gi, &r); break;
        case T_PING: { uint64_t ts = r64(&r); if (r.bad) break; wbuf b; head(&b, T_PONG); w64(&b, ts); send_to(&b, from); break; }
        case T_PONG: { uint64_t ts = r64(&r); if (!r.bad) rtt_sample(g, t - (LONG64)ts); break; }
        case T_BYE:
            if (s_state == 0) { say("%s left before the start", g->name); g->used = 0; break; }
            g->dropped = 1; g->drop_from = g->in_next;
            say("%s left at frame %u: their players stand still from there", g->name, g->drop_from);
            host_advance(); break;
        case T_STOP: { uint32_t f = r32(&r); send_stop(f); end_session("%s reported a desync at frame %u: the match stopped. Keep this log and theirs.", g->name, f); break; }
        }
        return;
    }
    /* guest: only the host */
    if (!same_addr(&s_peer[0].addr, from)) return;
    s_peer[0].last_heard = t;
    if (type == T_WELCOME || type == T_REJECT) {
        uint32_t nonce = r32(&r);
        if (r.bad || nonce != s_nonce || s_state != 0) return;
        if (type == T_REJECT) {
            uint32_t why = r8(&r);
            snprintf(s_status, sizeof s_status, "The host turned this PC away: %s.", why < sizeof k_reason / sizeof k_reason[0] ? k_reason[why] : "no reason given");
            say("%s", s_status); s_state = -1; SetEvent(s_ev); return;
        }
        uint32_t me = r8(&r), delay = r8(&r), need = r8(&r), now = r8(&r);
        if (r.bad || me >= NF_NET_PEERS || me == 0) return;
        if (s_me < 0) say("joined %s as PC %u (%u of %u PCs so far); waiting for the start", addr_text(from), me + 1, now, need);
        s_me = (int)me; s_session = session; s_delay = delay;
        return;
    }
    if (s_me < 0 || session != s_session || sender != 0) return;
    switch (type) {
    case T_START: if (s_state == 0) guest_start(&r); break;
    case T_LOBBY: if (s_state == 0) guest_lobby(&r); break;
    case T_FRAMES: if (s_state == 1) guest_frames(&r); break;
    case T_PING: { uint64_t ts = r64(&r); if (r.bad) break; wbuf b; head(&b, T_PONG); w64(&b, ts); send_to(&b, from); break; }
    case T_PONG: { uint64_t ts = r64(&r); if (!r.bad) rtt_sample(&s_peer[0], t - (LONG64)ts); break; }
    case T_BYE: end_session("The host left the match."); break;
    case T_STOP: { uint32_t f = r32(&r); end_session("The host reported a desync at frame %u: the match stopped. Keep this log and theirs.", f); break; }
    }
}

/* ---- timers -------------------------------------------------------------------- */
static void ping(peer_t *p, LONG64 t)
{
    wbuf b; head(&b, T_PING); w64(&b, (uint64_t)t); send_to(&b, &p->addr); p->last_ping = t;
}
static void timers(void)
{
    LONG64 t = now_us();
    if (s_role == ROLE_BROWSE) {
        if (t - s_find_t > 1000000) browse_find(t);
        for (int i = 0; i < (int)(sizeof s_found / sizeof s_found[0]); i++)
            if (s_found[i].used && t - s_found[i].seen > 3500000) { say("lobby: %s's match is gone", s_found[i].name); s_found[i].used = 0; }
        return;
    }
    if (s_role == 2) {
        if (s_state == 0 && s_me < 0) {
            if (t - s_hello_t > 250000) guest_hello();
            if (t - s_join_t > (s_lobby ? 10000000 : 30000000)) {
                snprintf(s_status, sizeof s_status, "No answer from the host at %s.", addr_text(&s_peer[0].addr));
                say("%s", s_status); s_state = -1; SetEvent(s_ev);
            }
            return;
        }
        if (s_state != 0 && s_state != 1) return;
        if (t - s_peer[0].last_ping > 500000) ping(&s_peer[0], t);
        if (s_state == 1 && t - s_input_t > 20000) guest_send_input();
        if (t - s_peer[0].last_heard > s_drop_us)
            end_session("Lost the host: nothing heard for %lld s, so the match stopped.", s_drop_us / 1000000);
        return;
    }
    if (s_state != 0 && s_state != 1) return;
    if (s_state == 1) host_pick_delay(t);
    if (s_state == 0 && s_start_at && t >= s_start_at) {
        int n = 0; for (int i = 0; i < NF_NET_PEERS; i++) n += s_peer[i].used;
        if (n >= (int)s_need) host_start();
        else {
            s_start_at = 0;
            if (s_lobby) { s_need = NF_NET_PEERS; say("lobby: start called off, a PC left"); }
        }
    }
    if (s_state == 0 && t - s_lobby_t > 300000) { host_lobby_pkt(); s_lobby_t = t; }
    for (int i = 1; i < NF_NET_PEERS; i++) {
        peer_t *g = &s_peer[i];
        if (!g->used || g->dropped) continue;
        if (t - g->last_ping > 500000) ping(g, t);
        if (s_state == 1) {
            if (!g->started_ack) { if (t - g->last_start > 100000) host_start_pkt(g); }
            else if (t - g->last_send > 20000) host_send_frames(g);
        }
        if (t - g->last_heard > s_drop_us) {
            if (s_state == 0) { say("%s went quiet before the start and was removed", g->name); g->used = 0; continue; }
            g->dropped = 1; g->drop_from = g->in_next;
            say("%s dropped: nothing heard for %lld s; their players stand still from frame %u", g->name, s_drop_us / 1000000, g->drop_from);
            host_advance();
        }
    }
    if (s_state == 0 && t - s_wait_log_t > 10000000) {
        int n = 0; for (int i = 0; i < NF_NET_PEERS; i++) n += s_peer[i].used;
        say("waiting for players: %d of %u PCs", n, s_need); s_wait_log_t = t;
    }
}
static DWORD WINAPI net_thread(void *arg)
{
    (void)arg;
    uint8_t buf[2048];
    while (!s_stop) {
        fd_set rs; FD_ZERO(&rs); FD_SET(s_sock, &rs);
        struct timeval tv = { 0, 2000 };
        select(0, &rs, NULL, NULL, &tv);
        EnterCriticalSection(&s_cs);
        if (s_old_sock != INVALID_SOCKET) { closesocket(s_old_sock); s_old_sock = INVALID_SOCKET; }
        for (;;) {
            struct sockaddr_in from; int fl = sizeof from;
            int n = recvfrom(s_sock, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
            if (n < 0) { int e = WSAGetLastError(); if (e == WSAECONNRESET || e == WSAEMSGSIZE) continue; break; }
            if (s_sim.on && sim_cut()) continue;
            on_packet(buf, n, &from);
        }
        timers();
        if (s_sim.on) sim_flush();
        LeaveCriticalSection(&s_cs);
    }
    return 0;
}

/* ---- public ------------------------------------------------------------------ */
void nf_net_set_ids(uint64_t build, uint64_t game, uint64_t settings) { s_build = build; s_game = game; s_settings = settings; }
int nf_net_on(void) { return s_role && s_state == 1; }
uint32_t nf_net_delay(void) { return s_delay; }
void nf_net_local_ports(unsigned *first_port, unsigned *count) { if (first_port) *first_port = s_my_base; if (count) *count = s_role ? s_nlocal : 0; }

static unsigned env_u(const char *name, unsigned def, unsigned lo, unsigned hi)
{
    const char *v = getenv(name); if (!v || !*v) return def;
    unsigned long x = strtoul(v, NULL, 10);
    return x < lo ? lo : x > hi ? hi : (unsigned)x;
}
static void parse_sim(const char *v)
{
    s_sim.on = 1; s_sim.rng = GetCurrentProcessId() * 2654435761u | 1;
    for (const char *p = v; p && *p; ) {
        if (!strncmp(p, "delay=", 6)) s_sim.delay = atoi(p + 6);
        else if (!strncmp(p, "jitter=", 7)) s_sim.jitter = atoi(p + 7);
        else if (!strncmp(p, "loss=", 5)) s_sim.loss = atoi(p + 5);
        else if (!strncmp(p, "cut=", 4)) { double a = 0, l = 0; sscanf(p + 4, "%lf:%lf", &a, &l); s_sim.cut_at = (LONG64)(a * 1e6); s_sim.cut_len = (LONG64)(l * 1e6); }
        p = strchr(p, ','); if (p) p++;
    }
}

/* "name-or-ip[:port]" to an IPv4 address; 0 if it can't be found. */
static int parse_addr(const char *text, unsigned defport, struct sockaddr_in *out)
{
    char host[256], port[16]; snprintf(port, sizeof port, "%u", defport);
    while (*text == ' ' || *text == '\t') text++;
    snprintf(host, sizeof host, "%s", text);
    char *e = host + strlen(host); while (e > host && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    char *colon = strrchr(host, ':'); if (colon) { *colon = 0; snprintf(port, sizeof port, "%s", colon + 1); }
    if (!host[0]) return 0;
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, port, &hints, &res) || !res) return 0;
    memcpy(out, res->ai_addr, sizeof *out); freeaddrinfo(res);
    return 1;
}
static uint32_t new_nonce(void)
{
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    uint32_t n = (uint32_t)(c.QuadPart ^ (c.QuadPart >> 32)) * 2654435761u ^ GetCurrentProcessId() ^ GetTickCount();
    return n ? n : 1;
}
/* A non-blocking UDP socket on `port` (0 = any), able to send broadcasts. */
static SOCKET open_sock(unsigned port, int *err)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { *err = WSAGetLastError(); return s; }
    u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
    BOOL off = FALSE, on = TRUE; DWORD got = 0; WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof off, NULL, 0, &got, NULL, NULL);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on);
    struct sockaddr_in me; memset(&me, 0, sizeof me); me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY); me.sin_port = htons((u_short)port);
    if (bind(s, (struct sockaddr *)&me, sizeof me)) { *err = WSAGetLastError(); closesocket(s); return INVALID_SOCKET; }
    return s;
}
static int lobby_run(void);

int nf_net_init(void)
{
    static int done; static int result;
    if (done) return result;
    done = 1;
    const char *role = getenv("NF_NET");
    if (!role || !*role || !strcmp(role, "0")) return result = 0;
    LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); s_qpf = f.QuadPart ? f.QuadPart : 1; s_t0 = c.QuadPart;
    const char *lf = getenv("NF_NET_LOG"); if (lf && *lf) s_log = fopen(lf, "w");
    if (!_stricmp(role, "host")) s_role = 1;
    else if (!_stricmp(role, "join")) s_role = 2;
    else if (!_stricmp(role, "lobby")) { s_role = ROLE_BROWSE; s_lobby = 1; }
    else { say("NF_NET=%s not understood (lobby, host or join); netplay off", role); return result = -1; }
    s_need = env_u("NF_NET_PLAYERS", 2, 1, NF_NET_PEERS);
    s_nlocal = env_u("NF_NET_LOCAL", 1, 1, NF_NET_PORTS);
    s_delay = s_min_delay = env_u("NF_NET_DELAY", 2, 1, 30);
    s_drop_us = (LONG64)env_u("NF_NET_DROP", 20, 2, 600) * 1000000;
    const char *nm = getenv("NF_NET_NAME");
    if (nm && *nm) snprintf(s_name, sizeof s_name, "%.16s", nm);
    else { char cn[64]; DWORD cl = sizeof cn; snprintf(s_name, sizeof s_name, "%.16s", GetComputerNameA(cn, &cl) ? cn : "PC"); }
    const char *sim = getenv("NF_NET_SIM"); if (sim && *sim) parse_sim(sim);
    s_nonce = new_nonce();

    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd)) { say("Winsock did not start"); return result = -1; }
    s_port = env_u("NF_NET_PORT", NET_DEFPORT, 1, 65535);
    unsigned port = s_role == 1 ? s_port : s_role == 2 ? env_u("NF_NET_PORT", 0, 0, 65535) : 0;
    int err = 0;
    s_sock = open_sock(port, &err);
    if (s_sock == INVALID_SOCKET) { say("could not use UDP port %u (%d)", port, err); return result = -1; }

    memset(s_peer, 0, sizeof s_peer);
    if (s_role == ROLE_BROWSE) {
        InitializeCriticalSection(&s_cs);
        s_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
        return result = lobby_run();
    }
    if (s_role == 2) {
        const char *a = getenv("NF_NET_ADDR");
        if (!a || !*a) { say("NF_NET=join needs NF_NET_ADDR (the host's address)"); return result = -1; }
        if (!parse_addr(a, NET_DEFPORT, &s_peer[0].addr)) { say("could not find the host %s", a); return result = -1; }
        s_peer[0].used = 1; strcpy(s_peer[0].name, "host");
        s_me = -1; s_join_t = now_us(); s_hello_t = -1000000;
        say("joining %s as %s with %u player%s", addr_text(&s_peer[0].addr), s_name, s_nlocal, s_nlocal == 1 ? "" : "s");
    } else {
        s_me = 0; s_session = s_nonce;
        s_peer[0].used = 1; s_peer[0].nlocal = s_nlocal; memcpy(s_peer[0].name, s_name, 17);
        say("hosting on UDP port %u for %u PCs; this PC has %u player%s; input delay %u frames", port, s_need, s_nlocal, s_nlocal == 1 ? "" : "s", s_delay);
    }
    InitializeCriticalSection(&s_cs);
    s_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (s_role == 1 && s_need == 1) { EnterCriticalSection(&s_cs); host_start(); LeaveCriticalSection(&s_cs); }
    s_thread = CreateThread(NULL, 0, net_thread, NULL, 0, NULL);
    if (!s_thread) { say("no network thread"); return result = -1; }
    while (s_state == 0) WaitForSingleObject(s_ev, 100);
    return result = s_state == 1 ? 1 : -1;
}

static void fallback(const uint8_t local[][NF_NET_PAD], uint8_t out[NF_NET_PORTS][NF_NET_PAD])
{
    memset(out, 0, NET_FRAME);
    for (unsigned k = 0; k < s_nlocal && s_my_base + k < NF_NET_PORTS; k++) memcpy(out[s_my_base + k], local[k], NF_NET_PAD);
}
static void waiting_for(uint32_t frame, char *t, size_t n)
{
    t[0] = 0;
    if (s_role == 2) { snprintf(t, n, "the host"); return; }
    for (int i = 1; i < NF_NET_PEERS; i++) {
        peer_t *p = &s_peer[i];
        if (p->used && !gone_at(p, frame) && p->in_next <= frame) {
            size_t l = strlen(t); snprintf(t + l, n - l, "%s%s", l ? ", " : "", p->name);
        }
    }
}

int nf_net_frame(uint32_t frame, const uint8_t local[][NF_NET_PAD], uint8_t out[NF_NET_PORTS][NF_NET_PAD])
{
    if (!s_role || s_state < 1) { if (s_role) fallback(local, out); return 0; }
    EnterCriticalSection(&s_cs);
    if (s_state != 1) { fallback(local, out); LeaveCriticalSection(&s_cs); return 0; }
    /* this PC's presses, used `delay` frames from now (frames the game skipped get the same) */
    s_cur_frame = frame; s_cur_t = now_us(); s_ran = 1;
    uint32_t target = frame + s_delay;
    if (s_role == 1) {
        peer_t *h = &s_peer[0];
        for (; h->in_next <= target; h->in_next++) {
            uint32_t slot = h->in_next % NET_W;
            memset(h->in[slot], 0, NET_FRAME);
            for (unsigned k = 0; k < s_nlocal; k++) memcpy(h->in[slot][k], local[k], NF_NET_PAD);
            h->in_stamp[slot] = h->in_next + 1;
        }
        host_advance();
    } else {
        int sent = 0;
        for (; s_my_next <= target; s_my_next++) {
            memset(s_my_in[s_my_next % NET_W], 0, NET_FRAME);
            for (unsigned k = 0; k < s_nlocal; k++) memcpy(s_my_in[s_my_next % NET_W][k], local[k], NF_NET_PAD);
            sent = 1;
        }
        if (sent) guest_send_input();
    }
    /* every PC's presses for this frame */
    s_frames++;
    if (s_agreed_next <= frame) {
        LONG64 t0 = now_us(); int logged = 0;
        s_waiting = 1; s_wait_frame = frame; s_wait_since = t0;
        while (s_state == 1 && s_agreed_next <= frame) {
            LeaveCriticalSection(&s_cs);
            WaitForSingleObject(s_ev, 5);
            EnterCriticalSection(&s_cs);
            if (!logged && now_us() - t0 > 1000000) {
                char who[96]; waiting_for(frame, who, sizeof who);
                say("waiting at frame %u for %s", frame, who); logged = 1;
            }
        }
        LONG64 w = now_us() - t0;
        s_waiting = 0; if (w > 2000) s_waits++; s_wait_us += w; if (w > s_wait_max) s_wait_max = w;
        if (logged) say("going again after %.1f s", w / 1e6);
    }
    if (s_state != 1) { fallback(local, out); LeaveCriticalSection(&s_cs); return 0; }
    memcpy(out, s_agreed[frame % NET_W], NET_FRAME);
    LeaveCriticalSection(&s_cs);
    return 1;
}

int nf_net_pace_us(void)
{
    if (!s_role || s_state != 1) return 0;
    EnterCriticalSection(&s_cs);
    double adv = -1e9;
    if (s_role == 2) { if (s_peer[0].adv_set) adv = s_peer[0].adv; }
    else for (int i = 1; i < NF_NET_PEERS; i++)
        if (s_peer[i].used && !s_peer[i].dropped && s_peer[i].adv_set && s_peer[i].adv > adv) adv = s_peer[i].adv;
    int us = adv > 0.5 ? (int)(adv * 1000) : 0;      /* 1 ms a frame per frame ahead, at most 2 ms */
    if (us > 2000) us = 2000;
    s_pace_total += us;
    LeaveCriticalSection(&s_cs);
    return us;
}

void nf_net_hash(uint32_t frame, uint64_t hash)
{
    if (!s_role || s_state != 1) return;
    EnterCriticalSection(&s_cs);
    hash_t *e = &s_my_hash[s_my_nhash++ % NET_HASHES]; e->frame = frame; e->h = hash; e->set = 1;
    s_last_hf = frame; s_last_h = hash;
    for (int i = 0; i < NF_NET_PEERS; i++)
        if (i != s_me && s_peer[i].used && (s_role == 1 || i == 0)) hash_compare(i);
    LeaveCriticalSection(&s_cs);
}

int nf_net_peers(void)
{
    int n = 0; for (int i = 0; i < NF_NET_PEERS; i++) n += s_peer[i].used; return s_role ? n : 0;
}
int nf_net_peer(int i, char name[17], unsigned *first_port, unsigned *count, int *ping_ms, int *dropped)
{
    if (!s_role || i < 0 || i >= NF_NET_PEERS || !s_peer[i].used) return 0;
    EnterCriticalSection(&s_cs);
    peer_t *p = &s_peer[i];
    if (name) { memcpy(name, p->name, 16); name[16] = 0; }
    if (first_port) *first_port = p->base;
    if (count) *count = p->nlocal;
    if (ping_ms) *ping_ms = i == s_me ? 0 : (s_role == 1 || i == 0) ? p->rtt_ms : -1;
    if (dropped) *dropped = p->dropped;
    LeaveCriticalSection(&s_cs);
    return 1;
}
const char *nf_net_status(void)
{
    static char t[200];
    if (!s_role) return "";
    if (s_state == 2 || s_state == -1) return s_status;
    if (s_state == 1 && s_waiting) {
        LONG64 w = now_us() - s_wait_since;
        if (w > 1000000) {
            EnterCriticalSection(&s_cs);
            char who[96]; waiting_for(s_wait_frame, who, sizeof who);
            LeaveCriticalSection(&s_cs);
            snprintf(t, sizeof t, "Waiting for %s (%lld s)", who, w / 1000000); return t;
        }
    }
    return "";
}

int nf_net_joined(void) { return s_role == 1 || (s_role == 2 && s_me > 0); }
int nf_net_cancelled(void) { return s_cancelled != 0; }

void nf_net_leave(void)
{
    if (!s_role || s_sock == INVALID_SOCKET || !s_ev) return;
    EnterCriticalSection(&s_cs);
    if (s_state == 0 || s_state == 1) {
        wbuf b; head(&b, T_BYE); w8(&b, 0);
        for (int r = 0; r < 3; r++)
            for (int i = 0; i < NF_NET_PEERS; i++)
                if (s_peer[i].used && i != s_me && (s_role == 2 ? i == 0 : !s_peer[i].dropped)) raw_send(b.p, b.n, &s_peer[i].addr);
        if (s_state == 0) { snprintf(s_status, sizeof s_status, "You left before the match started."); say("%s", s_status); s_state = -1; SetEvent(s_ev); }
        else end_session("You left the match.");
    }
    LeaveCriticalSection(&s_cs);
}

void nf_net_shutdown(void)
{
    if (!s_role || s_sock == INVALID_SOCKET) return;
    if (s_thread) {
        /* let the others get what they still need from this PC (up to 1.5 s) */
        LONG64 t0 = now_us();
        EnterCriticalSection(&s_cs);
        while (s_state == 1 && now_us() - t0 < 1500000) {
            int done = 1;
            if (s_role == 2) done = s_host_has >= s_my_next;
            else for (int i = 1; i < NF_NET_PEERS; i++)
                if (s_peer[i].used && !s_peer[i].dropped && s_peer[i].started_ack && s_peer[i].agreed_acked < s_agreed_next) done = 0;
            if (done) break;
            LeaveCriticalSection(&s_cs); Sleep(5); EnterCriticalSection(&s_cs);
        }
        wbuf b; head(&b, T_BYE); w8(&b, 0);
        for (int r = 0; r < 3; r++)
            for (int i = 0; i < NF_NET_PEERS; i++)
                if (s_peer[i].used && i != s_me && (s_role == 2 ? i == 0 : !s_peer[i].dropped)) raw_send(b.p, b.n, &s_peer[i].addr);
        if (s_ran)
            say("finished: %u frames, waited %u times (%.1f s in all, longest %.0f ms), slowed %.1f s in all, ping %d ms",
                s_frames, s_waits, s_wait_us / 1e6, s_wait_max / 1e3, s_pace_total / 1e6, s_role == 2 ? s_peer[0].rtt_ms : s_peer[1].rtt_ms);
        LeaveCriticalSection(&s_cs);
        InterlockedExchange(&s_stop, 1);
        WaitForSingleObject(s_thread, 1000); CloseHandle(s_thread); s_thread = NULL;
    }
    if (s_old_sock != INVALID_SOCKET) { closesocket(s_old_sock); s_old_sock = INVALID_SOCKET; }
    closesocket(s_sock); s_sock = INVALID_SOCKET; WSACleanup();
    if (s_log) { fclose(s_log); s_log = NULL; }
}

/* ---- the lobby window (NF_NET=lobby) ------------------------------------------
 * The window has its own thread, so the game's windows are left alone; the
 * game thread waits in nf_net_init until a match has started or the player
 * closed the lobby. Everything the window shows is read under s_cs from what
 * the network thread keeps up to date. */
enum { ID_NAME = 100, ID_LOCAL, ID_LIST, ID_JOIN, ID_HOST, ID_START, ID_BACK, ID_ADDR, ID_JOINADDR, ID_QUIT,
       ID_STATUS, ID_L_NAME, ID_L_LOCAL, ID_L_LIST, ID_L_ADDR, ID_COL0, ID_COL1, ID_COL2, ID_COL3, ID_LAST };
static HWND s_lw, s_lc[ID_LAST - ID_NAME];
#define LC(id) s_lc[(id) - ID_NAME]
static HFONT s_lfont;
static int s_dpi = 96, s_row_h = 20;
static const int k_col[4] = { 6, 166, 244, 300 };   /* list columns at 96 dpi */
static int s_lobby_result;
static char s_note[240];                 /* the last thing that went wrong, until the next action */
static int s_view = -1;                  /* 0 looking for matches, 1 hosting, 2 joining or joined */
static uint32_t s_row_key[16]; static unsigned char s_row_ok[16]; static int s_rows;
static char s_rows_text[16 * 160];
static char s_ini[MAX_PATH];
static int s_name_env;
static struct { int on, host_at, hosted, join, joins, quit_s, leave_s, addr_done; char addr[128]; LONG64 t0; } s_auto;

static int px(int v) { return MulDiv(v, s_dpi, 96); }   /* 96-dpi layout units to pixels */

/* Where to look: the whole local network, this PC, and NF_NET_FIND. */
static void add_target(uint32_t ip, unsigned port)
{
    for (int i = 0; i < s_ntargets; i++) if (s_targets[i].sin_addr.s_addr == ip && s_targets[i].sin_port == htons((u_short)port)) return;
    if (s_ntargets >= (int)(sizeof s_targets / sizeof s_targets[0])) return;
    struct sockaddr_in *a = &s_targets[s_ntargets++];
    memset(a, 0, sizeof *a); a->sin_family = AF_INET; a->sin_addr.s_addr = ip; a->sin_port = htons((u_short)port);
}
static void find_targets(void)
{
    add_target(htonl(INADDR_BROADCAST), s_port);
    add_target(htonl(INADDR_LOOPBACK), s_port);
    INTERFACE_INFO ii[32]; DWORD got = 0; int na = 0;
    if (!WSAIoctl(s_sock, SIO_GET_INTERFACE_LIST, NULL, 0, ii, sizeof ii, &got, NULL, NULL))
        for (DWORD k = 0; k < got / sizeof ii[0]; k++) {
            ULONG fl = ii[k].iiFlags;
            uint32_t a = ii[k].iiAddress.AddressIn.sin_addr.s_addr, m = ii[k].iiNetmask.AddressIn.sin_addr.s_addr;
            const uint8_t *ip = (const uint8_t *)&a;
            if (!(fl & IFF_UP) || (fl & IFF_LOOPBACK) || !a) continue;
            if (na < 3 && !(ip[0] == 169 && ip[1] == 254)) {      /* no self-assigned addresses */
                size_t l = strlen(s_my_addrs);
                snprintf(s_my_addrs + l, sizeof s_my_addrs - l, "%s%u.%u.%u.%u", na ? ", " : "", ip[0], ip[1], ip[2], ip[3]); na++;
            }
            if ((fl & IFF_BROADCAST) && m && m != 0xFFFFFFFFu) add_target((a & m) | ~m, s_port);   /* that network's broadcast */
        }
    const char *more = getenv("NF_NET_FIND");
    while (more && *more) {
        char one[160]; size_t n = strcspn(more, ",; ");
        if (n && n < sizeof one) {
            memcpy(one, more, n); one[n] = 0;
            struct sockaddr_in a;
            if (parse_addr(one, s_port, &a)) add_target(a.sin_addr.s_addr, ntohs(a.sin_port));
            else say("lobby: NF_NET_FIND: could not find %s", one);
        }
        more += n; while (*more == ',' || *more == ';' || *more == ' ') more++;
    }
}

/* The lobby's actions (window thread). */
static void swap_sock(SOCKET s)          /* call outside s_cs */
{
    for (int k = 0; k < 100 && s_old_sock != INVALID_SOCKET; k++) Sleep(5);
    EnterCriticalSection(&s_cs); s_old_sock = s_sock; s_sock = s; LeaveCriticalSection(&s_cs);
}
static void lobby_save(const char *key, const char *value) { if (s_ini[0]) WritePrivateProfileStringA("lobby", key, value, s_ini); }
static void lobby_take_inputs(void)
{
    if (!s_lw) return;
    char t[64]; GetWindowTextA(LC(ID_NAME), t, sizeof t);
    for (char *c = t; *c; c++) if ((unsigned char)*c < 32) *c = ' ';
    char *b = t; while (*b == ' ') b++;
    char *e = b + strlen(b); while (e > b && e[-1] == ' ') *--e = 0;
    LRESULT sel = SendMessageA(LC(ID_LOCAL), CB_GETCURSEL, 0, 0);
    EnterCriticalSection(&s_cs);
    if (*b) snprintf(s_name, sizeof s_name, "%.16s", b);
    if (sel >= 0 && sel < NF_NET_PORTS) s_nlocal = (unsigned)sel + 1;
    LeaveCriticalSection(&s_cs);
    if (!s_name_env && *b) lobby_save("name", s_name);
    char n[8]; snprintf(n, sizeof n, "%u", s_nlocal); lobby_save("players", n);
}
static void lobby_host(void)
{
    int err = 0;
    for (int k = 0; k < 100 && s_old_sock != INVALID_SOCKET; k++) Sleep(5);
    SOCKET s = open_sock(s_port, &err);
    if (s == INVALID_SOCKET) {
        if (err == WSAEADDRINUSE) snprintf(s_note, sizeof s_note, "Could not host: UDP port %u is already in use on this PC (is another copy hosting?).", s_port);
        else snprintf(s_note, sizeof s_note, "Could not host: UDP port %u could not be opened (error %d).", s_port, err);
        say("lobby: %s", s_note); return;
    }
    swap_sock(s);
    EnterCriticalSection(&s_cs);
    s_nonce = new_nonce(); s_session = s_nonce;
    s_role = 1; s_state = 0; s_me = 0; s_need = NF_NET_PEERS; s_start_at = 0; s_lobby_t = 0;
    memset(s_peer, 0, sizeof s_peer); memset(s_found, 0, sizeof s_found);
    s_peer[0].used = 1; s_peer[0].nlocal = s_nlocal; memcpy(s_peer[0].name, s_name, 17);
    s_note[0] = 0;
    say("lobby: hosting a match on UDP port %u as %s with %u player%s (this PC: %s)", s_port, s_name, s_nlocal, s_nlocal == 1 ? "" : "s", s_my_addrs[0] ? s_my_addrs : "no network address found");
    LeaveCriticalSection(&s_cs);
}
static void lobby_join(const struct sockaddr_in *to, const char *name)
{
    EnterCriticalSection(&s_cs);
    memset(s_peer, 0, sizeof s_peer); memset(s_lob, 0, sizeof s_lob); s_lob_starting = 0; memset(s_found, 0, sizeof s_found);
    s_peer[0].used = 1; s_peer[0].addr = *to; snprintf(s_peer[0].name, sizeof s_peer[0].name, "%.16s", name && *name ? name : "host");
    s_nonce = new_nonce(); s_session = 0;
    s_role = 2; s_state = 0; s_me = -1; s_join_t = now_us(); s_hello_t = -1000000;
    s_note[0] = 0;
    say("lobby: joining %s at %s as %s with %u player%s", s_peer[0].name, addr_text(to), s_name, s_nlocal, s_nlocal == 1 ? "" : "s");
    LeaveCriticalSection(&s_cs);
}
static void lobby_back(const char *why)  /* from a match (hosting, joining or joined) back to the list */
{
    int err = 0; SOCKET s = INVALID_SOCKET;
    if (s_role == 1) { for (int k = 0; k < 100 && s_old_sock != INVALID_SOCKET; k++) Sleep(5); s = open_sock(0, &err); }
    EnterCriticalSection(&s_cs);
    if (s_state == 0) {
        wbuf b; head(&b, T_BYE); w8(&b, 0);
        for (int r = 0; r < 3; r++)
            for (int i = 0; i < NF_NET_PEERS; i++)
                if (s_peer[i].used && i != s_me && (s_role == 2 ? i == 0 : 1)) raw_send(b.p, b.n, &s_peer[i].addr);
    }
    if (s != INVALID_SOCKET) { s_old_sock = s_sock; s_sock = s; }
    s_role = ROLE_BROWSE; s_state = 0; s_me = -1; s_session = 0; s_start_at = 0; s_need = NF_NET_PEERS;
    memset(s_peer, 0, sizeof s_peer); memset(s_lob, 0, sizeof s_lob); s_lob_starting = 0;
    s_find_t = -10000000;
    if (why && *why) { snprintf(s_note, sizeof s_note, "%s", why); say("lobby: back to the list: %s", why); }
    else { s_note[0] = 0; say("lobby: back to the list"); }
    LeaveCriticalSection(&s_cs);
}
static void lobby_start(void)
{
    EnterCriticalSection(&s_cs);
    unsigned pcs = used_pcs(NULL);
    if (s_role == 1 && s_state == 0 && pcs >= 2 && !s_start_at) {
        s_need = pcs; s_start_at = now_us() + 700000;     /* a few more pings first, to pick the delay */
        say("lobby: starting the match with %u PCs", pcs);
    }
    LeaveCriticalSection(&s_cs);
}
static void lobby_quit(void)
{
    if (s_cancelled) return;
    say("lobby: closed by the player");
    InterlockedExchange(&s_cancelled, 1);
    nf_net_leave();
}
static void join_selected(void)
{
    if (!s_lw || s_view != 0) return;
    int sel = (int)SendMessageA(LC(ID_LIST), LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_rows) return;
    struct sockaddr_in to; char name[17] = ""; int ok = 0, have = 0; unsigned why = 0;
    EnterCriticalSection(&s_cs);
    for (int i = 0; i < (int)(sizeof s_found / sizeof s_found[0]); i++)
        if (s_found[i].used && s_found[i].session == s_row_key[sel]) {
            to = s_found[i].addr; memcpy(name, s_found[i].name, 17); why = s_found[i].why; ok = !why && !s_found[i].state; have = 1;
        }
    LeaveCriticalSection(&s_cs);
    if (!have) return;
    if (!ok) { snprintf(s_note, sizeof s_note, "You can't join %s's match: %s.", name, why ? k_reason[why] : "it is starting"); return; }
    lobby_take_inputs();
    lobby_join(&to, name);
}
static void join_address(void)
{
    if (!s_lw) return;
    char t[128]; GetWindowTextA(LC(ID_ADDR), t, sizeof t);
    struct sockaddr_in to;
    if (!t[0]) { snprintf(s_note, sizeof s_note, "Type the host's address first (their IP address or PC name)."); return; }
    if (!parse_addr(t, s_port, &to)) { snprintf(s_note, sizeof s_note, "Could not find \"%.60s\". Type the host's IP address or PC name (add :port if it is not %u).", t, s_port); return; }
    lobby_save("address", t);
    lobby_take_inputs();
    lobby_join(&to, t);
}
/* NF_NET_LOBBY_AUTO: test runs press the buttons themselves. */
static void parse_auto(void)
{
    const char *a = getenv("NF_NET_LOBBY_AUTO"), *p;
    if (!a || !*a) return;
    s_auto.on = 1; s_auto.t0 = now_us();
    if ((p = strstr(a, "host:"))) s_auto.host_at = atoi(p + 5); else if (strstr(a, "host")) s_auto.host_at = 2;
    if ((p = strstr(a, "join:"))) snprintf(s_auto.addr, sizeof s_auto.addr, "%.*s", (int)strcspn(p + 5, ","), p + 5);
    else if (strstr(a, "join")) s_auto.join = 1;
    if ((p = strstr(a, "quit:"))) s_auto.quit_s = atoi(p + 5);
    if ((p = strstr(a, "leave:"))) s_auto.leave_s = atoi(p + 6);
    say("lobby (test): NF_NET_LOBBY_AUTO=%s", a);
}
static void lobby_auto(void)
{
    if (!s_auto.on) return;
    LONG64 t = now_us() - s_auto.t0;
    if (s_auto.quit_s && t > s_auto.quit_s * 1000000ll) { s_auto.quit_s = 0; lobby_quit(); return; }
    if (s_auto.leave_s && t > s_auto.leave_s * 1000000ll && (s_role == 1 || s_role == 2)) { s_auto.leave_s = 0; lobby_back(NULL); return; }
    if (s_role == ROLE_BROWSE) {
        if (s_auto.host_at && !s_auto.hosted) { s_auto.hosted = 1; lobby_host(); return; }
        if (s_auto.addr[0] && !s_auto.addr_done) {
            struct sockaddr_in to; s_auto.addr_done = 1;
            if (parse_addr(s_auto.addr, s_port, &to)) lobby_join(&to, s_auto.addr); else say("lobby (test): could not find %s", s_auto.addr);
            return;
        }
        if (s_auto.join && s_auto.joins < 3) {
            struct sockaddr_in to; char name[17]; int have = 0;
            EnterCriticalSection(&s_cs);
            for (int i = 0; i < (int)(sizeof s_found / sizeof s_found[0]) && !have; i++)
                if (s_found[i].used && !s_found[i].why && !s_found[i].state) { to = s_found[i].addr; memcpy(name, s_found[i].name, 17); have = 1; }
            LeaveCriticalSection(&s_cs);
            if (have) { s_auto.joins++; lobby_join(&to, name); }
        }
        return;
    }
    if (s_role == 1 && s_auto.host_at) {
        EnterCriticalSection(&s_cs);
        int go = s_state == 0 && !s_start_at && (int)used_pcs(NULL) >= s_auto.host_at;
        LeaveCriticalSection(&s_cs);
        if (go) lobby_start();
    }
}

/* The window. */
static void set_text(int id, const char *t)
{
    char cur[700]; GetWindowTextA(LC(id), cur, sizeof cur);
    if (strcmp(cur, t)) SetWindowTextA(LC(id), t);
}
static void show(int id, int on)
{
    int vis = (GetWindowLongA(LC(id), GWL_STYLE) & WS_VISIBLE) != 0;
    if (vis != !!on) ShowWindow(LC(id), on ? SW_SHOWNA : SW_HIDE);
}
static void ping_text(char *t, size_t n, int ms) { if (ms > 0) snprintf(t, n, "%d ms", ms); else snprintf(t, n, "-"); }
static void lobby_ui(void)
{
    static char rows[16][160]; uint32_t key[16]; unsigned char ok[16]; int n = 0;
    char status[700], pt[16], mine[40] = "";
    int view, can_start = 0, sel = (int)SendMessageA(LC(ID_LIST), LB_GETCURSEL, 0, 0);
    uint32_t sel_key = sel >= 0 && sel < s_rows ? s_row_key[sel] : 0;
    status[0] = 0;
    EnterCriticalSection(&s_cs);
    view = s_role == ROLE_BROWSE ? 0 : s_role == 1 ? 1 : 2;
    if (s_note[0]) snprintf(status, sizeof status, "%s\n", s_note);
    size_t l = strlen(status);
    if (view == 0) {
        int sel_why = -1; char sel_name[17] = "";
        for (int i = 0; i < (int)(sizeof s_found / sizeof s_found[0]) && n < 16; i++) {
            const found_t *e = &s_found[i];
            if (!e->used) continue;
            ping_text(pt, sizeof pt, e->ping_ms);
            snprintf(rows[n], sizeof rows[n], "%s\t%u of %u PCs\t%s\t%s", e->name, e->pcs, e->max, pt, e->why ? k_short[e->why] : e->state ? "starting" : "open");
            key[n] = e->session; ok[n] = !e->why && !e->state;
            if (e->session == sel_key) { sel_why = e->why ? (int)e->why : e->state ? 100 : 0; memcpy(sel_name, e->name, 17); }
            n++;
        }
        if (sel_why > 0 && !s_note[0])
            snprintf(status + l, sizeof status - l, "You can't join %s's match: %s.%s", sel_name, sel_why == 100 ? "it is starting" : k_reason[sel_why],
                     sel_why == R_SETTINGS ? " Every PC needs the same launcher and game settings." : sel_why == R_BUILD ? " Every PC needs the same build of the game." : "");
        else if (n)
            snprintf(status + l, sizeof status - l, "Pick a match and press Join (or double-click it). To host your own match, press Host a match.");
        else
            snprintf(status + l, sizeof status - l, "Looking for matches on your network (UDP port %u)... none yet.\nTo host one, press Host a match. If the host is not on your network, type their address below.", s_port);
    } else if (view == 1) {
        unsigned pcs = used_pcs(NULL);
        for (int i = 0; i < NF_NET_PEERS && n < 16; i++) {
            const peer_t *p = &s_peer[i];
            if (!p->used) continue;
            ping_text(pt, sizeof pt, i == 0 ? 0 : p->rtt_ms);
            snprintf(rows[n], sizeof rows[n], "%s\t%u player%s\t%s\t%s", p->name, p->nlocal, p->nlocal == 1 ? "" : "s", pt, i == 0 ? "host (you)" : "");
            key[n] = (uint32_t)i + 1; ok[n] = 1; n++;
        }
        can_start = s_state == 0 && pcs >= 2 && !s_start_at;
        snprintf(status + l, sizeof status - l, "Hosting on UDP port %u. PCs on your network see this match in their list; others join with this PC's address%s%s%s.\n%s",
                 s_port, s_my_addrs[0] ? " (" : "", s_my_addrs, s_my_addrs[0] ? ")" : "",
                 s_start_at ? "Starting the match..." : pcs < 2 ? "Waiting for someone to join. If Windows asks, let Nightfire through the firewall."
                 : "Press Start match when everyone is in (up to 4 PCs).");
    } else {
        if (s_me < 0) {
            snprintf(rows[n], sizeof rows[n], "%s\t\t\thost", s_peer[0].name); key[n] = 1; ok[n] = 1; n++;
            snprintf(status + l, sizeof status - l, "Joining %s at %s... (%lld s)", s_peer[0].name, addr_text(&s_peer[0].addr), (now_us() - s_join_t) / 1000000);
        } else {
            for (int i = 0; i < NF_NET_PEERS && n < 16; i++) {
                if (!s_lob[i].used) continue;
                ping_text(pt, sizeof pt, i == 0 ? s_peer[0].rtt_ms : i == s_me ? 0 : s_lob[i].ping_ms);
                snprintf(rows[n], sizeof rows[n], "%s\t%u player%s\t%s\t%s", s_lob[i].name, s_lob[i].nlocal, s_lob[i].nlocal == 1 ? "" : "s", pt,
                         i == 0 ? "host" : i == s_me ? "you" : "");
                key[n] = (uint32_t)i + 1; ok[n] = 1; n++;
            }
            if (!n) { snprintf(rows[n], sizeof rows[n], "%s\t\t\thost", s_peer[0].name); key[n] = 1; ok[n] = 1; n++; }
            snprintf(mine, sizeof mine, "%s", s_peer[0].name);
            snprintf(status + l, sizeof status - l, s_lob_starting ? "%s is starting the match..." : "You are in %s's match. Waiting for the host to press Start match.", mine);
        }
    }
    LeaveCriticalSection(&s_cs);

    if (view != s_view) {
        s_view = view;
        show(ID_JOIN, view == 0); show(ID_HOST, view == 0); show(ID_L_ADDR, view == 0); show(ID_ADDR, view == 0); show(ID_JOINADDR, view == 0);
        show(ID_START, view == 1); show(ID_BACK, view != 0);
        set_text(ID_BACK, view == 1 ? "Stop hosting" : "Leave");
        EnableWindow(LC(ID_NAME), view == 0); EnableWindow(LC(ID_LOCAL), view == 0);
        set_text(ID_L_LIST, view == 0 ? "Matches found:" : "Players in this match:");
        set_text(ID_COL0, view == 0 ? "Host" : "PC"); set_text(ID_COL1, view == 0 ? "PCs" : "Players");
        set_text(ID_COL2, "Ping"); set_text(ID_COL3, view == 0 ? "Can join?" : "");
        s_rows_text[0] = 1;                  /* refill the list */
        if (view != 0) SetFocus(LC(view == 1 ? ID_START : ID_BACK)); else SetFocus(LC(ID_LIST));
    }
    char all[sizeof s_rows_text]; all[0] = 0;
    for (int i = 0; i < n; i++) { size_t m = strlen(all); snprintf(all + m, sizeof all - m, "%s\n", rows[i]); }
    if (strcmp(all, s_rows_text)) {
        HWND lb = LC(ID_LIST);
        SendMessageA(lb, WM_SETREDRAW, FALSE, 0);
        SendMessageA(lb, LB_RESETCONTENT, 0, 0);
        int keep = -1;
        for (int i = 0; i < n; i++) {
            SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)rows[i]);
            s_row_key[i] = key[i]; s_row_ok[i] = ok[i];
            if (view == 0 && key[i] == sel_key) keep = i;
        }
        s_rows = n;
        for (int i = 0; view == 0 && keep < 0 && i < n; i++) if (ok[i]) keep = i;   /* nothing picked: the first one this PC can join */
        if (keep >= 0) SendMessageA(lb, LB_SETCURSEL, keep, 0);
        SendMessageA(lb, WM_SETREDRAW, TRUE, 0); InvalidateRect(lb, NULL, TRUE);
        strcpy(s_rows_text, all);
    }
    sel = (int)SendMessageA(LC(ID_LIST), LB_GETCURSEL, 0, 0);
    EnableWindow(LC(ID_JOIN), view == 0 && sel >= 0 && sel < s_rows && s_row_ok[sel]);
    EnableWindow(LC(ID_START), can_start);
    set_text(ID_STATUS, status);
}
static void lobby_draw(const DRAWITEMSTRUCT *d)
{
    if (d->CtlType != ODT_LISTBOX || (int)d->itemID < 0) return;
    char t[200] = "";
    if (SendMessageA(d->hwndItem, LB_GETTEXTLEN, d->itemID, 0) < (LRESULT)sizeof t) SendMessageA(d->hwndItem, LB_GETTEXT, d->itemID, (LPARAM)t);
    int sel = (d->itemState & ODS_SELECTED) != 0, grey = (int)d->itemID < s_rows && !s_row_ok[d->itemID];
    FillRect(d->hDC, &d->rcItem, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    SetTextColor(d->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : grey ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT));
    SetBkMode(d->hDC, TRANSPARENT);
    HGDIOBJ old = SelectObject(d->hDC, s_lfont);
    const char *p = t;
    for (int c = 0; c < 4 && p; c++) {
        const char *tab = strchr(p, '\t'); int len = tab ? (int)(tab - p) : (int)strlen(p);
        RECT r = d->rcItem; r.left += px(k_col[c]); r.right = c < 3 ? d->rcItem.left + px(k_col[c + 1]) - px(6) : d->rcItem.right - px(4);
        DrawTextA(d->hDC, p, len, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        p = tab ? tab + 1 : NULL;
    }
    SelectObject(d->hDC, old);
    if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
}
static void lobby_command(int id, int code)
{
    switch (id) {
    case ID_LIST:
        if (code == LBN_DBLCLK) join_selected();
        else if (code == LBN_SELCHANGE) { s_note[0] = 0; lobby_ui(); }
        return;
    case IDOK: if (s_view == 0) { if (GetFocus() == LC(ID_ADDR)) join_address(); else join_selected(); } break;
    case ID_JOIN: join_selected(); break;
    case ID_JOINADDR: join_address(); break;
    case ID_HOST: lobby_take_inputs(); lobby_host(); break;
    case ID_START: lobby_start(); break;
    case ID_BACK: lobby_back(NULL); break;
    case ID_QUIT: lobby_quit(); return;
    default: return;
    }
    lobby_ui();
}
static LRESULT CALLBACK lobby_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_COMMAND: if (s_lw) lobby_command(LOWORD(w), HIWORD(w)); return 0;
    case WM_CLOSE: lobby_quit(); return 0;
    case WM_MEASUREITEM: ((MEASUREITEMSTRUCT *)l)->itemHeight = (UINT)s_row_h; return TRUE;
    case WM_DRAWITEM: lobby_draw((const DRAWITEMSTRUCT *)l); return TRUE;
    }
    return DefWindowProcA(h, m, w, l);
}
static HWND lobby_ctl(HWND parent, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(!strcmp(cls, "EDIT") || !strcmp(cls, "LISTBOX") ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             px(x), px(y), px(w), px(h), parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    if (c) SendMessageA(c, WM_SETFONT, (WPARAM)s_lfont, TRUE);
    if (id >= ID_NAME && id < ID_LAST) LC(id) = c;
    return c;
}
static HWND lobby_window(void)
{
    HDC dc = GetDC(NULL);
    if (dc) { s_dpi = GetDeviceCaps(dc, LOGPIXELSY); if (s_dpi < 96) s_dpi = 96; }
    NONCLIENTMETRICSA ncm; memset(&ncm, 0, sizeof ncm); ncm.cbSize = sizeof ncm;
    if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0)) s_lfont = CreateFontIndirectA(&ncm.lfMessageFont);
    if (!s_lfont) s_lfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (dc) {
        TEXTMETRICA tm; HGDIOBJ o = SelectObject(dc, s_lfont);
        if (GetTextMetricsA(dc, &tm)) s_row_h = tm.tmHeight + px(6);
        SelectObject(dc, o); ReleaseDC(NULL, dc);
    }
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = lobby_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "NightfireOnlineLobby";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassA(&wc);
    DWORD st = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, ex = WS_EX_TOPMOST;
    RECT r = { 0, 0, px(560), px(452) };
    AdjustWindowRectEx(&r, st, FALSE, ex);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    RECT wa; if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0)) { wa.left = wa.top = 0; wa.right = GetSystemMetrics(SM_CXSCREEN); wa.bottom = GetSystemMetrics(SM_CYSCREEN); }
    int x = wa.left + (wa.right - wa.left - ww) / 2, y = wa.top + (wa.bottom - wa.top - wh) / 3;
    HWND other = FindWindowA("NightfireOnlineLobby", NULL);     /* two copies on one PC: side by side */
    if (other) {
        RECT o; GetWindowRect(other, &o);
        int gap = px(8), pair = (o.right - o.left) + gap + ww;
        if (pair <= wa.right - wa.left) {
            int left = wa.left + (wa.right - wa.left - pair) / 2;
            SetWindowPos(other, NULL, left, o.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            x = left + (o.right - o.left) + gap; y = o.top;
        } else { x = o.left + px(40); y = o.top + px(40); }
    }
    char title[120]; const char *lb = getenv("NF_WINDOW_LABEL");
    snprintf(title, sizeof title, "Nightfire Online%s%s", lb && *lb ? " - " : "", lb && *lb ? lb : "");
    HWND w = CreateWindowExA(ex, wc.lpszClassName, title, st, x, y, ww, wh, NULL, NULL, wc.hInstance, NULL);
    if (!w) return NULL;
    lobby_ctl(w, "STATIC", "Your name:", SS_LEFT | SS_CENTERIMAGE, 14, 14, 84, 24, ID_L_NAME);
    lobby_ctl(w, "EDIT", s_name, ES_AUTOHSCROLL | WS_TABSTOP, 100, 14, 170, 24, ID_NAME);
    SendMessageA(LC(ID_NAME), EM_LIMITTEXT, 16, 0);
    lobby_ctl(w, "STATIC", "Players on this PC:", SS_RIGHT | SS_CENTERIMAGE, 280, 14, 160, 24, ID_L_LOCAL);
    lobby_ctl(w, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 448, 14, 98, 150, ID_LOCAL);
    for (int i = 1; i <= NF_NET_PORTS; i++) { char t[4]; snprintf(t, sizeof t, "%d", i); SendMessageA(LC(ID_LOCAL), CB_ADDSTRING, 0, (LPARAM)t); }
    SendMessageA(LC(ID_LOCAL), CB_SETCURSEL, s_nlocal - 1, 0);
    lobby_ctl(w, "STATIC", "Matches found:", SS_LEFT, 14, 54, 300, 20, ID_L_LIST);
    for (int c = 0; c < 4; c++) lobby_ctl(w, "STATIC", "", SS_LEFT, 16 + k_col[c], 76, (c < 3 ? k_col[c + 1] - k_col[c] : 404 - k_col[c]) - 4, 18, ID_COL0 + c);
    lobby_ctl(w, "LISTBOX", "", LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, 14, 96, 410, 196, ID_LIST);
    lobby_ctl(w, "BUTTON", "Join", BS_PUSHBUTTON | WS_TABSTOP, 436, 96, 110, 30, ID_JOIN);
    lobby_ctl(w, "BUTTON", "Host a match", BS_PUSHBUTTON | WS_TABSTOP, 436, 134, 110, 30, ID_HOST);
    lobby_ctl(w, "BUTTON", "Start match", BS_PUSHBUTTON | WS_TABSTOP, 436, 96, 110, 30, ID_START);
    lobby_ctl(w, "BUTTON", "Leave", BS_PUSHBUTTON | WS_TABSTOP, 436, 134, 110, 30, ID_BACK);
    lobby_ctl(w, "STATIC", "Or join by address:", SS_LEFT | SS_CENTERIMAGE, 14, 304, 130, 24, ID_L_ADDR);
    lobby_ctl(w, "EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, 146, 304, 278, 24, ID_ADDR);
    SendMessageA(LC(ID_ADDR), EM_LIMITTEXT, 120, 0);
    lobby_ctl(w, "BUTTON", "Join address", BS_PUSHBUTTON | WS_TABSTOP, 436, 302, 110, 30, ID_JOINADDR);
    lobby_ctl(w, "STATIC", "", SS_LEFT | SS_NOPREFIX, 14, 344, 532, 62, ID_STATUS);
    lobby_ctl(w, "BUTTON", "Quit", BS_PUSHBUTTON | WS_TABSTOP, 436, 410, 110, 30, ID_QUIT);
    if (s_ini[0]) { char a[128]; GetPrivateProfileStringA("lobby", "address", "", a, sizeof a, s_ini); SetWindowTextA(LC(ID_ADDR), a); }
    ShowWindow(LC(ID_START), SW_HIDE); ShowWindow(LC(ID_BACK), SW_HIDE);
    s_lw = w;
    lobby_ui();
    ShowWindow(w, SW_SHOWNORMAL); SetForegroundWindow(w);
    SetFocus(LC(ID_LIST));
    return w;
}
static DWORD WINAPI lobby_thread(LPVOID arg)
{
    (void)arg;
    lobby_window();
    if (!s_lw && !s_auto.on) {
        EnterCriticalSection(&s_cs);
        snprintf(s_status, sizeof s_status, "The online lobby window could not open."); say("lobby: %s", s_status); s_state = -1;
        LeaveCriticalSection(&s_cs);
        s_lobby_result = -1; return 0;
    }
    LONG64 ui_t = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (s_lw && IsDialogMessageA(s_lw, &msg)) continue;
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        lobby_auto();
        int role, state; char why[200] = "";
        EnterCriticalSection(&s_cs);
        role = s_role; state = s_state;
        if (state == -1 || state == 2) snprintf(why, sizeof why, "%s", s_status);
        LeaveCriticalSection(&s_cs);
        if (s_cancelled) { s_lobby_result = -1; break; }
        if ((role == 1 || role == 2) && state == 1) { s_lobby_result = 1; break; }
        if ((role == 1 || role == 2) && (state == -1 || state == 2)) lobby_back(why);
        LONG64 t = now_us();
        if (s_lw && t - ui_t > 200000) { lobby_ui(); ui_t = t; }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
    }
    if (s_lw) { DestroyWindow(s_lw); s_lw = NULL; }
    say("lobby: %s", s_lobby_result == 1 ? (s_role == 1 ? "match started (this PC hosts it)" : "match started") : "closed");
    return 0;
}
static int lobby_run(void)
{
    char base[MAX_PATH]; DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
    if (n && n < sizeof base) {
        snprintf(s_ini, sizeof s_ini, "%s\\NightfirePC", base); CreateDirectoryA(s_ini, NULL);
        size_t l = strlen(s_ini); snprintf(s_ini + l, sizeof s_ini - l, "\\online.ini");
    }
    const char *nm = getenv("NF_NET_NAME"); s_name_env = nm && *nm;
    if (!s_name_env && s_ini[0]) { char t[40]; GetPrivateProfileStringA("lobby", "name", "", t, sizeof t, s_ini); if (t[0]) snprintf(s_name, sizeof s_name, "%.16s", t); }
    const char *lc = getenv("NF_NET_LOCAL");
    if (!(lc && *lc) && s_ini[0]) { UINT k = GetPrivateProfileIntA("lobby", "players", 0, s_ini); if (k >= 1 && k <= NF_NET_PORTS) s_nlocal = k; }
    find_targets();
    s_me = -1; s_need = NF_NET_PEERS; s_find_t = -10000000;
    say("lobby: open as %s with %u player%s; looking for matches on UDP port %u in %d places; this PC: %s", s_name, s_nlocal, s_nlocal == 1 ? "" : "s",
        s_port, s_ntargets, s_my_addrs[0] ? s_my_addrs : "no network address found");
    for (int i = 0; i < s_ntargets; i++) say("lobby:   looking at %s", addr_text(&s_targets[i]));
    parse_auto();
    s_thread = CreateThread(NULL, 0, net_thread, NULL, 0, NULL);
    if (!s_thread) { say("no network thread"); return -1; }
    HANDLE ui = CreateThread(NULL, 0, lobby_thread, NULL, 0, NULL);
    if (!ui) { say("no lobby thread"); return -1; }
    WaitForSingleObject(ui, INFINITE); CloseHandle(ui);
    return s_lobby_result;
}
