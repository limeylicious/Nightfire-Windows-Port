/* Integer Z24S8 helpers. Caller validates the backing surface and coordinates.
 * Floating-point depth, stencil tests and polygon offset are not implemented.
 */
#ifndef NIGHTFIRE_DEPTH_H
#define NIGHTFIRE_DEPTH_H
#include <stdint.h>
#include <math.h>
static int nf_depth_compare(unsigned func,uint32_t incoming,uint32_t stored) {
 switch(func){
 case 0x200:return 0;case 0x201:return incoming<stored;case 0x202:return incoming==stored;
 case 0x203:return incoming<=stored;case 0x204:return incoming>stored;case 0x205:return incoming!=stored;
 case 0x206:return incoming>=stored;case 0x207:return 1;default:return 0;
 }
}
static int nf_depth_encode(float z,uint32_t *value) {
 if(!isfinite(z) || z<0 || z>16777215.0f)return 0;
 /* Cast after range validation. Sub-unit hardware precision is not modeled. */
 *value=(uint32_t)z;return 1;
}
static uint32_t nf_depth_write(uint32_t previous,uint32_t z) {
 return (z<<8)|(previous&255);
}
static uint32_t nf_depth_clear(uint32_t previous,uint32_t clear,unsigned mask) {
 uint32_t bits=((mask&1)?0xffffff00u:0)|((mask&2)?255u:0);
 return (previous&~bits)|(clear&bits);
}
#endif
