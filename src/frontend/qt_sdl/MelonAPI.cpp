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

u8* loadedROM;
s32 loadedROMSize;
bool directBoot = true;

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

    NDS::LoadROM(file, fileSize, directBoot);
}
