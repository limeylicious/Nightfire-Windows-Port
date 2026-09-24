#ifndef DRIVING_METHOD_TIMING327_H
#define DRIVING_METHOD_TIMING327_H
/* Optional serialized-consumer diagnostic. Does not defer a publication or
 * alter a guest command. Parent scopes subtract the same disjoint child
 * counters as TIMING250. Immediate-END is a subset, never an additive bucket. */
#ifdef DRIVING_TIMING250
static struct {
 uint64_t calls[6],inclusive[6],child[6],exclusive[6],invalid;
} method327;
static int method327_setting=-1;
static int method327_enabled(void){
 if(method327_setting<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_METHOD_TIMING327");method327_setting=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}
 return method327_setting&&dt250_enabled();
}
static uint64_t method327_children(void){
 return batch_time236.queue_ticks+batch_time236.fallback_ticks+batch_time236.flush_ticks+
  dt250.accepted_ticks+dt250.refused_ticks+dt250.software_ticks[0]+dt250.software_ticks[1]+dt250.software_ticks[2];
}
typedef struct {uint64_t start,child;} MethodScope327;
static MethodScope327 method327_begin(void){
 DWORD error=GetLastError();int crt=errno;
 MethodScope327 s={dt250_clock(),method327_children()};
 errno=crt;SetLastError(error);return s;
}
static void method327_end(unsigned group,MethodScope327 s){
 DWORD error=GetLastError();int crt=errno;
 uint64_t end=dt250_clock(),child=method327_children();
 if(end<s.start||child<s.child||end-s.start<child-s.child)method327.invalid++;
 else {method327.calls[group]++;method327.inclusive[group]+=end-s.start;
  method327.child[group]+=child-s.child;method327.exclusive[group]+=end-s.start-(child-s.child);}
 errno=crt;SetLastError(error);
}
static void method327_report(void){
 if(!method327_enabled()||(dt250.drains!=1&&dt250.drains%512))return;
 DWORD error=GetLastError();int crt=errno;LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);
 static const char*names[]={"end","clear","flip_request","flip_stall","other","immediate_end_subset"};
 for(unsigned i=0;i<6;i++)fprintf(stderr,"[METHOD327] drains=%llu group=%s calls=%llu inclusive_ticks=%llu child_ticks=%llu exclusive_ticks=%llu qpc_hz=%lld invalid=%llu diagnostic-only=1\n",
  (unsigned long long)dt250.drains,names[i],(unsigned long long)method327.calls[i],
  (unsigned long long)method327.inclusive[i],(unsigned long long)method327.child[i],
  (unsigned long long)method327.exclusive[i],(long long)hz.QuadPart,(unsigned long long)method327.invalid);
 errno=crt;SetLastError(error);
}
#endif
#endif
