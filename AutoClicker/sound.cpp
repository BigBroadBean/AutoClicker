#include "sound.h"
#include <Windows.h>
#include <mmsystem.h>
#include <thread>

#pragma comment(lib, "winmm.lib")

void PlayToggleSound(bool enabled)
{
    std::thread([enabled]() {
        if (enabled)
            PlaySound(TEXT("SystemStart"), NULL, SND_ALIAS | SND_ASYNC);
        else
            PlaySound(TEXT("SystemExit"), NULL, SND_ALIAS | SND_ASYNC);
    }).detach();
}