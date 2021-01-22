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
#include "../../Platform.h"
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
            NDS::LoadROM(loadedROM, loadedROMSize, blankStr, directBoot);
    }

    NDS::RunFrame();
    lastFrameButtons = buttons;
}

DLL s32* GetTopScreenBuffer() { return (s32*)GPU::Framebuffer[GPU::FrontBuffer][0]; }
DLL s32* GetBottomScreenBuffer() { return (s32*)GPU::Framebuffer[GPU::FrontBuffer][1]; }
// Length in pixels.
DLL s32 GetScreenBufferSize() { return 256 * 192; }

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
