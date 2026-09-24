#ifndef NIGHTFIRE_FLIGHT_RECORDER116_H
#define NIGHTFIRE_FLIGHT_RECORDER116_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* Caller supplies already-validated game data; this module never reads guest RAM
 * or OS keyboard state. Initially opt-in via explicit enable(). 4096 input polls
 * is about 68 seconds at60 polls/sec, not a guaranteed duration at other rates. */
#define NF_FLIGHT116_CAPACITY 4096u
#define NF_FLIGHT116_MARKER_BYTES 40u
enum nf_flight116_result { NF_FLIGHT116_OK=0, NF_FLIGHT116_DISABLED=1,
    NF_FLIGHT116_BUSY=2, NF_FLIGHT116_INVALID=3, NF_FLIGHT116_IO_ERROR=4 };
enum nf_flight116_metadata { NF_FLIGHT116_LEVEL=1u, NF_FLIGHT116_POSITION=2u,
    NF_FLIGHT116_YAW=4u, NF_FLIGHT116_HEALTH=8u, NF_FLIGHT116_GAME_TICK=16u };
enum nf_flight116_event { NF_FLIGHT116_INPUT=1u, NF_FLIGHT116_FRAME=2u,
    NF_FLIGHT116_MARKER=3u };
typedef struct nf_flight116_record {
    uint64_t sequence; /* Assigned by append; chronological insertion order. */
    uint64_t frame, time_ms, game_tick;
    uint32_t buttons, level, metadata, event_kind;
    int16_t axes[4]; /* LX,LY,RX,RY from final merged game packet. */
    uint8_t triggers[2]; /* LT,RT. */
    uint8_t analog_buttons[6]; /* A,B,X,Y,black,white (original Xbox values). */
    float position[3], yaw, health;
    char marker[NF_FLIGHT116_MARKER_BYTES]; /* Truncated to39 bytes + NUL. */
} nf_flight116_record;
typedef struct nf_flight116_info {
    uint64_t total_records, overwritten;
    uint32_t count, copied, capacity, enabled;
} nf_flight116_info;

/* Enable/reset once per development run. Directory must already exist and be
 * an absolute drive path supplied/validated by the launcher; no mkdir/uploads.
 * Enable/disable are initialization/normal-control APIs, not fault handlers. */
int nf_flight116_enable(const wchar_t *output_directory);
void nf_flight116_disable(void);
int nf_flight116_append(const nf_flight116_record *record);
/* Nonblocking lock acquisition. Copies newest min(capacity,count) records in
 * chronological order. info.count is total retained, info.copied copied count. */
int nf_flight116_snapshot(nf_flight116_record *out, size_t capacity, nf_flight116_info *info);
/* Best effort normal diagnostic-stop/close dump; never aborts/throws/exits.
 * Returns BUSY immediately if either recorder lock is unavailable, including
 * same-thread reentrancy. No allocation, no waiting for recorder locks. Disk I/O
 * can still block/fail: this is NOT universal native-crash capture. Appends may
 * continue after the consistent snapshot; later records are outside that dump.
 * Writes a new nightfire-history116-PID-serial.jsonl; never overwrites a file.
 * Caller must preserve its original stop/exit regardless of this return value. */
int nf_flight116_flush(const char *reason);

#ifdef NIGHTFIRE_FLIGHT116_TEST
void nf_flight116_test_hold_ring(void);
void nf_flight116_test_release_ring(void);
void nf_flight116_test_hold_flush(void);
void nf_flight116_test_release_flush(void);
#endif
#endif
