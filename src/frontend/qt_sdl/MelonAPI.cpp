#ifdef __WIN32__
#define DLL extern "C" __declspec(dllexport)
#else
#define DLL extern "C"
#endif

#include <stdio.h>

// This method is used by Platform.cpp, so we must include it here or make modifications there. Here's easier.
void emuStop() { }
// This one too, but this should definitely not ever be called.
void* oglGetProcAddress(const char* proc)
{
	throw "OpenGL is not supported by MelonAPI.";
}

bool inited = false;

DLL void Deinit()
{
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

    printf("Testing Melon print.");

    if (inited)
    {
        printf("MelonDS is already inited. De-initing before init.\n");
        Deinit();
    }

	inited = true;
	return true;
}
