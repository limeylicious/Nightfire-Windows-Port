/* Reuse owned scratch capacity, never guest data. All callers are inside the
 * synchronous consumer's existing scanout lock; no pointer escapes a draw's
 * completed GPU interval. Large exceptional requests keep ordinary allocation
 * behavior. The three original slots retain at most 48MiB until process exit. The
 * additional shader memo slot234 retains 128KiB; entries reset every draw. */
#ifndef DRIVING_STORAGE228_H
#define DRIVING_STORAGE228_H
#include <stdlib.h>
#include <stddef.h>
typedef struct DrivingStorage228 {void *bytes;size_t capacity;} DrivingStorage228;
static void *driving_storage228(DrivingStorage228 *s,size_t bytes)
{
 const size_t limit=16u*1024*1024;
 if(!bytes)return NULL;
 if(bytes>limit)return malloc(bytes);
 if(bytes>s->capacity){
  size_t capacity=(bytes+65535)&~(size_t)65535;
  void *next=malloc(capacity);if(!next)return NULL;
  free(s->bytes);s->bytes=next;s->capacity=capacity;
 }
 return s->bytes;
}
static void driving_storage_release228(DrivingStorage228 *s,void *bytes)
{if(bytes!=s->bytes)free(bytes);}
#endif
