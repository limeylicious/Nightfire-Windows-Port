/* loopback_rec: records what the default speakers play (WASAPI loopback) to a 16-bit
 * stereo WAV for a fixed time, so a run on the emulated sound chip can be compared with
 * a run on native sound without anyone listening.
 *
 * Usage: loopback_rec out.wav seconds
 * Build: cl /O2 loopback_rec.c ole32.lib */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator_, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator_, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient_, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioCaptureClient_, 0xC8ADBD64, 0xE71E, 0x48A0, 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

static void hdr(FILE *f, uint32_t rate, uint32_t bytes)
{
    uint32_t riff = 36 + bytes, fl = 16, avg = rate * 4; uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fl, 4, 1, f);
    fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&avg, 4, 1, f);
    fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    fseek(f, 0, SEEK_END);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: loopback_rec out.wav seconds\n"); return 1; }
    double secs = atof(argv[2]);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator *en = NULL; IMMDevice *dev = NULL; IAudioClient *ac = NULL; IAudioCaptureClient *cc = NULL;
    WAVEFORMATEX *wf = NULL;
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator_, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator_, (void **)&en))) return 2;
    if (FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eConsole, &dev))) return 3;
    if (FAILED(IMMDevice_Activate(dev, &IID_IAudioClient_, CLSCTX_ALL, NULL, (void **)&ac))) return 4;
    IAudioClient_GetMixFormat(ac, &wf);
    int is_float = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(&((WAVEFORMATEXTENSIBLE *)wf)->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_));
    if (FAILED(IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, wf, NULL))) return 5;
    if (FAILED(IAudioClient_GetService(ac, &IID_IAudioCaptureClient_, (void **)&cc))) return 6;
    FILE *f = fopen(argv[1], "wb"); if (!f) return 7;
    uint32_t bytes = 0; hdr(f, wf->nSamplesPerSec, 0);
    fprintf(stderr, "loopback: %u Hz, %u ch, %u bits, %s\n", wf->nSamplesPerSec, wf->nChannels, wf->wBitsPerSample, is_float ? "float" : "int");
    IAudioClient_Start(ac);
    ULONGLONG t0 = GetTickCount64(), last = 0;
    uint64_t frames_total = 0;
    while (GetTickCount64() - t0 < (ULONGLONG)(secs * 1000)) {
        Sleep(10);
        UINT32 n = 0;
        while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(cc, &n)) && n) {
            BYTE *data; UINT32 frames; DWORD flags;
            if (FAILED(IAudioCaptureClient_GetBuffer(cc, &data, &frames, &flags, NULL, NULL))) break;
            for (UINT32 i = 0; i < frames; i++) {
                int16_t o[2];
                for (int c = 0; c < 2; c++) {
                    int src = c < wf->nChannels ? c : 0;
                    float v = 0.0f;
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                        if (is_float) v = ((float *)data)[i * wf->nChannels + src];
                        else if (wf->wBitsPerSample == 16) v = ((int16_t *)data)[i * wf->nChannels + src] / 32768.0f;
                        else if (wf->wBitsPerSample == 32) v = ((int32_t *)data)[i * wf->nChannels + src] / 2147483648.0f;
                    }
                    v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
                    o[c] = (int16_t)(v * 32767.0f);
                }
                fwrite(o, 2, 2, f);
            }
            bytes += frames * 4; frames_total += frames;
            IAudioCaptureClient_ReleaseBuffer(cc, frames);
        }
        if (GetTickCount64() - last > 2000) { hdr(f, wf->nSamplesPerSec, bytes); fflush(f); last = GetTickCount64(); }
    }
    IAudioClient_Stop(ac);
    hdr(f, wf->nSamplesPerSec, bytes); fclose(f);
    fprintf(stderr, "loopback: %llu frames written\n", (unsigned long long)frames_total);
    return 0;
}
