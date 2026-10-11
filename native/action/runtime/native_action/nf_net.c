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
 * Switches (netplay is off unless NF_NET is set):
 *   NF_NET=host | join           role
 *   NF_NET_ADDR=address[:port]   join: the host (default port 47610)
 *   NF_NET_PORT=n                host: UDP port to listen on (default 47610); join: local port (default any)
 *   NF_NET_PLAYERS=n             host: PCs in the match, host included (default 2)
 *   NF_NET_LOCAL=n               players on this PC (default 1)
 *   NF_NET_DELAY=n               host: least input delay in frames (default 2 = 33 ms); the host
 *                                raises it to cover the slowest ping and lowers it again slowly
 *   NF_NET_NAME=text             this PC's name for the others (default the computer name)
 *   NF_NET_DROP=s                seconds of silence before a PC is dropped (default 20)
 *   NF_NET_LOG=file              log file (default stderr)
 *   NF_NET_SIM=delay=ms,jitter=ms,loss=percent,cut=at_s:len_s
 *                                test only: delay/jitter/loss on this PC's sends; cut = no
 *                                packets in or out from at_s for len_s seconds
 *
 * Uses only Winsock (nothing to install). Written from the Winsock documentation. */
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

enum { T_HELLO = 1, T_WELCOME, T_REJECT, T_START, T_INPUT, T_FRAMES, T_PING, T_PONG, T_BYE, T_STOP };
enum { R_FULL = 1, R_PROTO, R_BUILD, R_GAME, R_SETTINGS, R_STARTED, R_PLAYERS };
static const char *const k_reason[] = { "", "the match is full", "a different netplay version",
    "a different build of the game", "different game files", "different game settings",
    "the match has already started", "too many players on that PC" };

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

static int s_role;                       /* 0 off, 1 host, 2 guest */
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
    if (pcs + 1 >= s_need && !s_start_at) s_start_at = now_us() + 700000;   /* a few pings first, to pick the delay */
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
    if (s_role == 2) {
        if (s_state == 0 && s_me < 0) {
            if (t - s_hello_t > 250000) guest_hello();
            if (t - s_join_t > 30000000) {
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
        if (n >= (int)s_need) host_start(); else s_start_at = 0;
    }
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
    else { say("NF_NET=%s not understood (host or join); netplay off", role); return result = -1; }
    s_need = env_u("NF_NET_PLAYERS", 2, 1, NF_NET_PEERS);
    s_nlocal = env_u("NF_NET_LOCAL", 1, 1, NF_NET_PORTS);
    s_delay = s_min_delay = env_u("NF_NET_DELAY", 2, 1, 30);
    s_drop_us = (LONG64)env_u("NF_NET_DROP", 20, 2, 600) * 1000000;
    const char *nm = getenv("NF_NET_NAME");
    if (nm && *nm) snprintf(s_name, sizeof s_name, "%.16s", nm);
    else { char cn[64]; DWORD cl = sizeof cn; snprintf(s_name, sizeof s_name, "%.16s", GetComputerNameA(cn, &cl) ? cn : "PC"); }
    const char *sim = getenv("NF_NET_SIM"); if (sim && *sim) parse_sim(sim);
    s_nonce = (uint32_t)(c.QuadPart ^ (c.QuadPart >> 32)) * 2654435761u ^ GetCurrentProcessId() ^ GetTickCount();
    if (!s_nonce) s_nonce = 1;

    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd)) { say("Winsock did not start"); return result = -1; }
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock == INVALID_SOCKET) { say("no UDP socket (%d)", WSAGetLastError()); return result = -1; }
    u_long nb = 1; ioctlsocket(s_sock, FIONBIO, &nb);
    BOOL off = FALSE; DWORD got = 0; WSAIoctl(s_sock, SIO_UDP_CONNRESET, &off, sizeof off, NULL, 0, &got, NULL, NULL);
    struct sockaddr_in me; memset(&me, 0, sizeof me); me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port = htons((u_short)env_u("NF_NET_PORT", s_role == 1 ? NET_DEFPORT : 0, 0, 65535));
    if (bind(s_sock, (struct sockaddr *)&me, sizeof me)) { say("could not use UDP port %u (%d)", ntohs(me.sin_port), WSAGetLastError()); return result = -1; }

    memset(s_peer, 0, sizeof s_peer);
    if (s_role == 2) {
        const char *a = getenv("NF_NET_ADDR");
        if (!a || !*a) { say("NF_NET=join needs NF_NET_ADDR (the host's address)"); return result = -1; }
        char hostname[256]; char port[16]; snprintf(port, sizeof port, "%u", NET_DEFPORT);
        strncpy(hostname, a, sizeof hostname - 1); hostname[sizeof hostname - 1] = 0;
        char *colon = strrchr(hostname, ':'); if (colon) { *colon = 0; strncpy(port, colon + 1, sizeof port - 1); port[sizeof port - 1] = 0; }
        struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(hostname, port, &hints, &res) || !res) { say("could not find the host %s", a); return result = -1; }
        memcpy(&s_peer[0].addr, res->ai_addr, sizeof s_peer[0].addr); freeaddrinfo(res);
        s_peer[0].used = 1; strcpy(s_peer[0].name, "host");
        s_me = -1; s_join_t = now_us(); s_hello_t = -1000000;
        say("joining %s as %s with %u player%s", addr_text(&s_peer[0].addr), s_name, s_nlocal, s_nlocal == 1 ? "" : "s");
    } else {
        s_me = 0; s_session = s_nonce;
        s_peer[0].used = 1; s_peer[0].nlocal = s_nlocal; memcpy(s_peer[0].name, s_name, 17);
        say("hosting on UDP port %u for %u PCs; this PC has %u player%s; input delay %u frames", ntohs(me.sin_port), s_need, s_nlocal, s_nlocal == 1 ? "" : "s", s_delay);
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
        say("finished: %u frames, waited %u times (%.1f s in all, longest %.0f ms), slowed %.1f s in all, ping %d ms",
            s_frames, s_waits, s_wait_us / 1e6, s_wait_max / 1e3, s_pace_total / 1e6, s_role == 2 ? s_peer[0].rtt_ms : s_peer[1].rtt_ms);
        LeaveCriticalSection(&s_cs);
        InterlockedExchange(&s_stop, 1);
        WaitForSingleObject(s_thread, 1000); CloseHandle(s_thread); s_thread = NULL;
    }
    closesocket(s_sock); s_sock = INVALID_SOCKET; WSACleanup();
    if (s_log) { fclose(s_log); s_log = NULL; }
}
