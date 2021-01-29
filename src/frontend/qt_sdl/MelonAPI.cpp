#ifdef __WIN32__
#define DLL extern "C" __declspec(dllexport)
#else
#define DLL extern "C"
#endif

#include <cstring>
#include <memory.h>
#include <stdio.h>

#include "../../Config.h"
#include "../../GPU.h"
#include "../../NDS.h"
#include "../../NDSCart_SRAMManager.h"
#include "../../Platform.h"
#include "../../Savestate.h"
#include "../../SPI.h"
#include "../../SPU.h"
#include "../../types.h"

// This method is used by Platform.cpp, so we must include it here or make modifications there. Here's easier.
void emuStop() { }
// This one too, but this should definitely not ever be called.
void* oglGetProcAddress(const char* proc)
{
	throw "OpenGL is not supported by MelonAPI.";
}

bool inited = false;

char blankStr[] = { '\0' };
u8* loadedROM;
s32 loadedROMSize;
bool directBoot = true;

u32 lastFrameButtons = 0;

DLL void ResetCounters();

DLL void Deinit()
{
    Platform::DeInit();
    NDS::DeInit();
	inited = false;
}

DLL bool Init()
{
    FILE* conout = fopen("CONOUT$", "w");
    if (conout)
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    if (inited)
    {
        printf("MelonDS is already inited. De-initing before init.\n");
        Deinit();
    }

    Platform::Init(0, NULL);

    if (!NDS::Init())
    {
        printf("Failed to init NDS.");
        return false;
    }
    GPU::InitRenderer(0);

    ResetCounters();

    // This will be temporary, used until MelonAPI exposes config options.
    GPU::RenderSettings renderSettings;
    renderSettings.Soft_Threaded = true;
    GPU::SetRenderSettings(0, renderSettings);
    strcpy(Config::BIOS7Path, "melon/bios7.bin");
    strcpy(Config::BIOS9Path, "melon/bios9.bin");
    strcpy(Config::FirmwarePath, "melon/firmware.bin");

	inited = true;
	return true;
}

DLL void LoadROM(u8* file, s32 fileSize)
{
    if (loadedROM) delete[] loadedROM;
    loadedROM = new u8[fileSize];
    loadedROMSize = fileSize;
    memcpy(loadedROM, file, fileSize);

    NDS::LoadROM(file, fileSize, blankStr, directBoot);
}

DLL void ResetCounters()
{
    NDS::NumFrames = 0;
    NDS::NumLagFrames = 0;
}
DLL int GetFrameCount() { return NDS::NumFrames; }
DLL bool IsLagFrame() { return NDS::LagFrameFlag; }
DLL int GetLagFrameCount() { return NDS::NumLagFrames; }

DLL void SetFrameCount(u32 count) { NDS::NumFrames = count; }
DLL void SetIsLagFrame(bool isLag) { NDS::LagFrameFlag = isLag; }
DLL void SetLagFrameCount(u32 count) { NDS::NumLagFrames = count; }

DLL void FrameAdvance(u32 buttons, u8 touchX, u8 touchY)
{
    if (!inited) return;

    if (buttons & 0x2000)
        NDS::TouchScreen(touchX, touchY);
    else
        NDS::ReleaseScreen();
        
    NDS::SetKeyMask(~buttons & 0xFFF); // 12 buttons
    if (buttons & 0x4000)
        NDS::SetLidClosed(false);
    else if (buttons & 0x8000)
        NDS::SetLidClosed(true);

    if (buttons & 0x10000 && !(lastFrameButtons & 0x10000))
    {
        if (NDS::Running)
            NDS::Stop();
        else if (loadedROM)
        {
            // we want to keep any firmware settings the system has made
            bool overrideConfig = Config::FirmwareOverrideSettings;
            Config::FirmwareOverrideSettings = false;
            NDS::LoadROM(loadedROM, loadedROMSize, blankStr, directBoot);
            Config::FirmwareOverrideSettings = overrideConfig;
        }
    }

    NDS::RunFrame();
    lastFrameButtons = buttons;
}

DLL s32* GetTopScreenBuffer() { return (s32*)GPU::Framebuffer[GPU::FrontBuffer][0]; }
DLL s32* GetBottomScreenBuffer() { return (s32*)GPU::Framebuffer[GPU::FrontBuffer][1]; }
// Length in pixels.
DLL s32 GetScreenBufferSize() { return 256 * 192; }

s32 sampleCount = -1;
DLL int GetSampleCount()
{
    return sampleCount = SPU::GetOutputSize();
}
DLL void GetSamples(s16* data, s32 count)
{
    if (count != sampleCount)
        throw "sample count mismatch; call GetSampleCount first";
    if (SPU::ReadOutput(data, count) != count)
        throw "sample count was less than expected";
    sampleCount = -1;
}
DLL void DiscardSamples()
{
    SPU::DrainOutput();
}

// null samples is the same as giving 0 sample count
DLL void SetMicSamples(s16* samples, s32 count)
{
    NDS::MicInputFrame(samples, count);
}


DLL void SetOverrideFirmwareSettings(bool override)
{
    Config::FirmwareOverrideSettings = override;
}
// Lengths are character count. 2 bytes per character.
DLL void SetFirmwareSettings(char* username, s32 usernameLength, char* message, s32 messageLength,
    u8 language, u8 color, u8 month, u8 day)
{
    if (usernameLength > 10)
        usernameLength = 10;
    if (messageLength > 26)
        messageLength = 26;
    
    memcpy(Config::FirmwareUsername, username, usernameLength * 2);
    memset(Config::FirmwareUsername + usernameLength * 2, 0, sizeof(Config::FirmwareUsername) - usernameLength * 2);

    memcpy(Config::FirmwareMessage, message, messageLength * 2);
    memset(Config::FirmwareMessage + messageLength * 2, 0, sizeof(Config::FirmwareMessage) - messageLength * 2);

    Config::FirmwareLanguage = language;
    Config::FirmwareFavouriteColour = color;
    Config::FirmwareBirthdayMonth = month;
    Config::FirmwareBirthdayDay = day;
}
DLL void EraseUserSettings(u8* firmwareData, s32 firmwareLength)
{
    // make sure we have a valid firmware length
    if (SPI_Firmware::FixFirmwareLength((u32)firmwareLength) == firmwareLength)
    {
        u32 mask = firmwareLength - 1;
        u32 userdata = 0x7FE00 & mask;
        memset(firmwareData + userdata, 0xFF, 0x200);
    }
    // if not, do nothing
    // the intended use case is to calculate a hash of the firmware to compare against the hash of a known good one
    // in the case we have a bad length the hash will not match, which is what we want
}

DLL void SetDirectBoot(bool value) { directBoot = value; }

DLL void SetTimeAtBoot(u32 value) { Config::TimeAtBoot = value; }

DLL bool UseSavestate(u8* data, s32 len)
{
    Savestate* state = new Savestate(data, len);
    if (!state->Error)
        NDS::DoSavestate(state);
    bool error = state->Error;
    delete state;
    return !error;
}

Savestate* _loadedState;
u8* stateData;
s32 stateLength = -1;
DLL int GetSavestateSize()
{
    if (_loadedState) delete _loadedState;

    _loadedState = new Savestate(NULL, true);
    NDS::DoSavestate(_loadedState);
    stateData = _loadedState->GetData();
    stateLength = _loadedState->GetDataLength();

    return stateLength;
}
DLL void GetSavestateData(u8* data, s32 size)
{
    if (size != stateLength)
        throw "size mismatch; call GetSavestateSize first";
    if (stateData)
    {
        memcpy(data, stateData, stateLength);
        delete _loadedState;
        _loadedState = NULL;
        stateData = NULL;
        stateLength = -1;
    }
}

DLL s32 GetSRAMLength()
{
    return (s32)NDSCart_SRAMManager::SecondaryBufferLength;
}
DLL void GetSRAM(u8* dst, s32 size)
{
    if (!inited) return;

    if (size != NDSCart_SRAMManager::SecondaryBufferLength)
        throw "SRAM size mismatch; call GetSRAMLength first";
    NDSCart_SRAMManager::FlushSecondaryBuffer(dst, size);
}
DLL void SetSRAM(u8* src, s32 size)
{
    if (!inited) return;

    if (size != NDSCart_SRAMManager::SecondaryBufferLength)
        throw "SRAM size mismatch; call GetSRAMLength first";
    NDSCart_SRAMManager::UpdateBuffer(src, size);
}
DLL bool IsSRAMModified()
{
    return NDSCart_SRAMManager::NeedsFlush();
}
