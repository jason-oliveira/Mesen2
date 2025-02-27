#include "pch.h"
#include "PlatformUtilities.h"

#ifdef _WIN32
#include <Windows.h>
#endif

bool PlatformUtilities::_highResTimerEnabled = false;

void PlatformUtilities::DisableScreensaver()
{
	//Prevent screensaver/etc from starting while using the emulator
	//DirectInput devices apparently do not always count as user input
	#ifdef _WIN32
	SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
	#endif
}

void PlatformUtilities::EnableScreensaver()
{
	#ifdef _WIN32
	SetThreadExecutionState(ES_CONTINUOUS);
	#endif
}

void PlatformUtilities::EnableHighResolutionTimer()
{
	#ifdef _WIN32
	//Request a 1ms timer resolution on Windows while a game is running
	if(!_highResTimerEnabled) {
		timeBeginPeriod(1);
		_highResTimerEnabled = true;
	}
	#endif
}

void PlatformUtilities::RestoreTimerResolution()
{
	#ifdef _WIN32
	if(_highResTimerEnabled) {
		timeEndPeriod(1);
		_highResTimerEnabled = false;
	}
	#endif
}