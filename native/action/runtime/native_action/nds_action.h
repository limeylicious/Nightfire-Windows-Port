/* Action native DirectSound (NIGHTFIRE_NATIVE_SOUND=1): entry points answered by
 * nds_action.c, reached from the call-site hook in nds_action_glue.c. */
#ifndef NDS_ACTION_H
#define NDS_ACTION_H
#include <stdint.h>
int nds_on(void);
int nds_override(uint32_t va);
/* Runs a guest stream callback (stream context, packet context, status) on the
 * calling guest thread, preserving that thread's guest registers. */
void nds_guest_callback(uint32_t fn, uint32_t stream_ctx, uint32_t packet_ctx, uint32_t status);

uint32_t nds_DirectSoundCreate(uint32_t guid, uint32_t ppds, uint32_t unk);
uint32_t nds_UseFullHRTF(void);
uint32_t nds_DownloadEffectsImage(uint32_t ds, uint32_t img, uint32_t size, uint32_t ploc, uint32_t ppdesc);
uint32_t nds_ReleaseDS(uint32_t ds);
uint32_t nds_SynchPlayback(uint32_t ds);
uint32_t nds_CommitDeferredSettings(uint32_t ds);
uint32_t nds_SetOrientation(uint32_t pargs, uint32_t apply);
uint32_t nds_ListenerSetPosition(uint32_t pargs, uint32_t apply);
uint32_t nds_ListenerSetVelocity(uint32_t pargs, uint32_t apply);
uint32_t nds_DoWork(void);
uint32_t nds_CreateSoundBuffer(uint32_t ds, uint32_t pdesc, uint32_t ppbuf, uint32_t unk);
uint32_t nds_ReleaseBuffer(uint32_t buf);
uint32_t nds_Play(uint32_t buf, uint32_t r1, uint32_t r2, uint32_t flags);
uint32_t nds_Stop(uint32_t buf);
uint32_t nds_SetBufferData(uint32_t buf, uint32_t data, uint32_t bytes);
uint32_t nds_SetLoopRegion(uint32_t buf, uint32_t start, uint32_t len);
uint32_t nds_SetCurrentPosition(uint32_t buf, uint32_t pos);
uint32_t nds_GetCurrentPosition(uint32_t buf, uint32_t pplay, uint32_t pwrite);
uint32_t nds_SetMixBins(uint32_t obj, uint32_t pmix);
uint32_t nds_SetMixBinVolumes(uint32_t buf, uint32_t pmix);
uint32_t nds_SetVolume(uint32_t obj, uint32_t mb);
uint32_t nds_SetHeadroom(uint32_t buf, uint32_t hr);
uint32_t nds_SetFrequency(uint32_t buf, uint32_t hz);
uint32_t nds_GetStatus(uint32_t buf, uint32_t pstatus);
uint32_t nds_Buffer3D(uint32_t buf, int kind, uint32_t pargs, uint32_t count, uint32_t apply);
uint32_t nds_SetI3DL2Source(uint32_t buf, uint32_t params, uint32_t apply);
uint32_t nds_CreateStream(uint32_t pdesc, uint32_t ppstream);
uint32_t nds_StreamPause(uint32_t s, uint32_t mode);
uint32_t nds_StreamAddRef(uint32_t s);
uint32_t nds_StreamRelease(uint32_t s);
uint32_t nds_StreamGetInfo(uint32_t s, uint32_t pinfo);
uint32_t nds_StreamGetStatus(uint32_t s, uint32_t pst);
uint32_t nds_StreamProcess(uint32_t s, uint32_t in, uint32_t outp);
uint32_t nds_StreamDiscontinuity(uint32_t s);
uint32_t nds_StreamFlush(uint32_t s);
#endif
