/*510 private diagnostics. No guest pointer is read by this interface. */
#ifndef DRIVING_DIAG510_H
#define DRIVING_DIAG510_H
#include <stdint.h>
#include <stddef.h>
extern int driving_text510_on,driving_stall510_on;
enum {D510_ATTEMPT=1,D510_ROUTE,D510_DRAW,D510_GLYPH,D510_PIXEL,D510_ATLAS,
 D510_RESOLVE,D510_CACHE,D510_PRESENT,D510_CLEAR,D510_UNKNOWN,D510_END,D510_IMMEDIATE,D510_ATTRIBUTES};
enum {P510_FONT=1,P510_SPRITE,P510_BATCH,P510_GEOMETRY,P510_GENERIC,P510_RESOLVE,P510_CLEAR,P510_BLIT};
typedef struct D510Record {
 volatile long ready;uint32_t type,tid,bytes;
 uint64_t qpc,epoch,command,attempt,serial,reserved;
 unsigned char payload[448];
} D510Record;
/* Integers/floats are little endian; no embedded pointers are dereferenced by a parser. */
typedef struct D510Draw {
 uint32_t path,lane,target,count,profile,raw_format,raw_texture,flags;
 float bounds[4],minimum[4],maximum[4],mean[4];
 uint32_t state[20]; /* blend/enabled/src/dst,alpha/enabled/func/ref,depth/enabled/func/write,mask,cull,etc */
 uint64_t texture_identity[4];uint32_t texture_format[4],filter[4],address[4];
 uint64_t atlas_hash;
} D510Draw;
typedef struct D510Glyph {uint32_t lane,index,target,reserved;float position[4][4],uv[4][4],color[4][4];uint64_t atlas_hash;} D510Glyph;
typedef struct D510Atlas {uint64_t hash;uint32_t offset,total;unsigned char data[432];} D510Atlas;
typedef struct D510Resolve {uint32_t source,destination,family,count;float attributes[3][16];} D510Resolve;
typedef struct D510Link {uint64_t epoch;uint32_t physical,version,index,reserved;} D510Link;
void driving_diag510_init(void);
void driving_diag510_terminal(const char*);
void driving_diag510_register(const char*);
void driving_diag510_beat(const char*);
void driving_diag510_wait(unsigned,const uint32_t*,unsigned,int,int64_t,unsigned);
void driving_diag510_wait_phase(unsigned);
void driving_diag510_command(unsigned,unsigned);
uint64_t driving_diag510_attempt(void);
uint64_t driving_diag510_override(uint64_t);
void driving_diag510_route(unsigned,unsigned,unsigned,unsigned);
void driving_diag510_emit(unsigned,const void*,size_t);
void driving_diag510_raw_attempt(unsigned,const uint32_t*);
void driving_diag510_clear(unsigned,uint32_t,const uint32_t*,unsigned);
uint64_t driving_diag510_atlas(const unsigned char*,size_t);
uint64_t driving_diag510_resolve(uint32_t,uint32_t,unsigned,const float*,unsigned);
uint64_t driving_diag510_epoch(void);
uint64_t driving_diag510_completed(void);
void driving_diag510_cache(uint32_t,unsigned,uint64_t);
void driving_diag510_select(uint64_t,unsigned);
void driving_diag510_present(unsigned,uint32_t);
/* Material helpers live after the original hardware types, outside this header. */
#endif
