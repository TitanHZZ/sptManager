/*
 * This file belongs to sptManager.
 *
 * This file only contains source code built for the windows platform...
 */

// basic platform check for no error reports on linux
#ifdef WIN32

// windows stuff - especially COM interface and WMI
// #include <windows.h>
// #include <iostream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Audiopolicy.h>
#include <comdef.h>
#include <Psapi.h>
#include <atlbase.h>
#include <chrono>
#include <fstream>

// json
#include <nlohmann/json.hpp>

// remove the console window
// #pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#define CHECK_HR(hr) if (FAILED(hr)) { /*std::cout << "error" << std::endl;*/ return 1; }

_COM_SMARTPTR_TYPEDEF(IMMDevice, __uuidof(IMMDevice));
_COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioSessionManager2, __uuidof(IAudioSessionManager2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionManager2, __uuidof(IAudioSessionManager2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionEnumerator, __uuidof(IAudioSessionEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioSessionControl2, __uuidof(IAudioSessionControl2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionControl, __uuidof(IAudioSessionControl));
_COM_SMARTPTR_TYPEDEF(ISimpleAudioVolume, __uuidof(ISimpleAudioVolume));

// remove the console window
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

// media commands for spotify
enum SPOTIFY_COMMANDS
{
    SPOTIFY_MUTE = 524288,
    SPOTIFY_VOLUMEDOWN = 589824,
    SPOTIFY_VOLUMEUP = 655360,
    SPOTIFY_NEXT = 720896,
    SPOTIFY_PREV = 786432,
    SPOTIFY_STOP = 851968,
    SPOTIFY_PLAYPAUSE = 917504
};

IAudioSessionManager2Ptr CreateSessionManager()
{
    HRESULT hr = S_OK;
    IMMDevicePtr pDevice;
    IMMDeviceEnumeratorPtr pEnumerator;
    IAudioSessionManager2Ptr pSessionManager;

    // Create the device enumerator.
    CHECK_HR(hr = CoCreateInstance( __uuidof(MMDeviceEnumerator) , NULL , CLSCTX_ALL ,  __uuidof(IMMDeviceEnumerator), (void**) &pEnumerator ));

    // Get the default audio device.
    CHECK_HR(hr = pEnumerator->GetDefaultAudioEndpoint( eRender , eConsole , &pDevice ));

    // Get the session manager.
    CHECK_HR(hr = pDevice->Activate( __uuidof(IAudioSessionManager2) , CLSCTX_ALL , NULL, (void**) &pSessionManager ));

    return pSessionManager;
}

bool checkAudioOutputAndOpenWindow(CComQIPtr<IAudioMeterInformation> pMeterInformation) {
    // Get current peak volume output.
    bool retVal = false;
    FLOAT fPeakValue;
    pMeterInformation->GetPeakValue(&fPeakValue);

    // Make sure that there is something playing.
    if (fPeakValue > 0)
    {
        // Try to get handle(s) on both possible window names that spotify uses when playing an AD.
        // Note that "Spotify Free" is also used when the audio player is just paused.
        // Thats why we check if there is something playing.
        HWND sptNormalWindowHandle = FindWindowA(NULL, "Spotify Free");
        HWND sptADWindowHandle = FindWindowA(NULL, "Advertisement");

        // Check if one of the 'windows' is open.
        retVal = sptNormalWindowHandle != NULL || sptADWindowHandle != NULL;
        CloseHandle(sptNormalWindowHandle);
        CloseHandle(sptADWindowHandle);
        return retVal;
    }

    return retVal;
}

bool checkAd(IAudioSessionManager2Ptr mgr) {
    // code based on this stackoverflow answer
    // --> https://stackoverflow.com/a/32425536

    IAudioSessionEnumeratorPtr enumerator;
    if (FAILED( mgr->GetSessionEnumerator(&enumerator) ))
    {
        return false;
    }

    int sessionCount;
    if (FAILED( enumerator->GetCount(&sessionCount) ))
    {
        return false;
    }

    for (int i = 0; i < sessionCount; i++)
    {
        IAudioSessionControlPtr control;
        if (FAILED( enumerator->GetSession(i, &control) ))
        {
            return false;
        }
        
        IAudioSessionControl2Ptr control2;
        if (FAILED( control->QueryInterface(__uuidof(IAudioSessionControl2), (void**) &control2) ))
        {
            return false;
        }

        DWORD processId;
        if (FAILED( control2->GetProcessId(&processId) ))
        {
            return false;
        }

        // get a handle to the current audio device process
        HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (process_handle)
        {
            // get process information
            TCHAR procName[MAX_PATH];
            GetProcessImageFileName(process_handle, procName, MAX_PATH);
            std::string procNameStr = static_cast<std::string>(procName);

            if (procNameStr.find("Spotify") != std::string::npos)
            {
                // 'pMeterInformation' wil be used to get the current peak volume output
                CComQIPtr<IAudioMeterInformation> pMeterInformation = control2;
                DWORD nMask;
                pMeterInformation->QueryHardwareSupport(&nMask);

                // loop for 1 second
                // make sure that the changes in window name and peak audio volume is not just the user
                // playing or pausing the music
                bool adPlaying = true;
                auto start = std::chrono::steady_clock::now();
                for (auto now = start; now < start + std::chrono::seconds{1}; now = std::chrono::steady_clock::now()) {
                    adPlaying &= checkAudioOutputAndOpenWindow(pMeterInformation);
                    Sleep(50);
                }
                return adPlaying;
            }
        }
        CloseHandle(process_handle);
    }
    return false;
}

void waitForWindow(LPCSTR windowName , int waitTime)
{
    // waits for a specific window to open
    HWND handle = FindWindowA(NULL, windowName);
    while (!handle)
    {
        handle = FindWindowA(NULL, windowName);
        Sleep(waitTime);
    }
    CloseHandle(handle);
}

void ChangeSpotifyState(SPOTIFY_COMMANDS spotify_command)
{
    // based on this --> "https://github.com/moobsmc/spotify-controls/blob/master/Spotify.cpp"

    // get the handle for spotify
    HWND Window_Handle = FindWindow("Chrome_WidgetWin_0", NULL);
    // send the message
    SendMessage(Window_Handle, WM_APPCOMMAND, 0, spotify_command);

    // basic clean up
    CloseHandle(Window_Handle);
}

void bypass_ad(nlohmann::json& config_j, HWND& spt_handle)
{
    /// get spotify window state ////
    // declares and defines variables that are going to receive the window "stats"
    WINDOWPLACEMENT wd = { sizeof(WINDOWPLACEMENT) };
    UINT nCurShow;

    // gets the window state and saves them
    GetWindowPlacement(spt_handle, &wd);
    nCurShow = wd.showCmd;

    /// kill spotify ////////////////
    // terminate spotify sending a SIGTERM
    system("taskkill /IM spotify.exe > temp.log 2>&1");
    Sleep(config_j["timings"]["timeToWaitAfterClosingSpotify"]);

    /// start spotify ///////////////
    const std::string sptDir = static_cast<std::string>(config_j["generalConfiguration"]["SpotifyInstallationDir"]) + "Spotify.exe";
    ShellExecuteA(NULL, "open", sptDir.c_str(), NULL, NULL, SW_SHOWMAXIMIZED);

    // wait for spotify
    waitForWindow("Spotify Free", 50);
    HWND handle = FindWindowA(NULL, "Spotify Free");

    Sleep(config_j["timings"]["timeToWaitAfterSpotifySpotifyOpened"]);

    /// 'forces' focus to the spotify window //////
    // based on this -> "https://stackoverflow.com/questions/19136365/win32-setforegroundwindow-not-working-all-the-time"
    HWND foreground_hwnd = GetForegroundWindow();

    // get the process id of the current foreground window process
    DWORD windowThreadProcessId = GetWindowThreadProcessId(foreground_hwnd, LPDWORD(0));
    DWORD currentThreadId = GetCurrentThreadId();

    // attaches the current running process with the foreground window process
    AttachThreadInput(windowThreadProcessId, currentThreadId, true);
    BringWindowToTop(handle); // brings the specified window to the top of the Z order -> win32 api documentation
    ShowWindow(handle, SW_SHOWMAXIMIZED);
    SetForegroundWindow(handle); // might help sometimes

    Sleep(config_j["timings"]["timeToWaitAfterSettingSpotifyAsTheForegroundWindow"]);

    /// minimize or restore spotify if needed ///
    if (config_j["generalConfiguration"]["respectLastSpotifyWindowState"])
    {
        // set spotify window state to be the same it was before being closed
        ShowWindow(handle, nCurShow);
    }
    else if (config_j["generalConfiguration"]["minimizeSpotify"])
    {
        // minimize the spotify window
        ShowWindow(handle, SW_MINIMIZE);
    }

    /// play spotify media //////////
    Sleep(config_j["timings"]["timeToWaitBeforePlayingTheMediaInSpotify"]);
    ChangeSpotifyState(SPOTIFY_PLAYPAUSE);

    /// bring back the focus to 'original' foreground window ///
    BringWindowToTop(foreground_hwnd);
    ShowWindow(foreground_hwnd, SW_SHOW);

    // detaches the current running process and the new foreground window process
    AttachThreadInput(windowThreadProcessId, currentThreadId, false);

    // basic clean up
    CloseHandle(handle);
}

int main (int argc, char *argv[]) {
    // initialize the COM interface for use with WMI
    CoInitialize(NULL);
    IAudioSessionManager2Ptr mgr = CreateSessionManager();
    if (!mgr) {
        // std::cout << "ERROR: could not get the session manager" << std::endl;
        return 1;
    }

    // read the JSON config file
    std::ifstream config_f("config.json");
    nlohmann::json config_j = nlohmann::json::parse(config_f);

    while (true)
    {
        HWND sptNormalWindowHandle = FindWindowA(NULL, "Spotify Free");
        HWND sptADWindowHandle = FindWindowA(NULL, "Advertisement");

        if (sptADWindowHandle)
        {
            // std::cout << "---- SPOTIFY AD DETECTED ----" << std::endl;
            bypass_ad(config_j, sptADWindowHandle);
        } else if (sptNormalWindowHandle) {
            if (checkAd(mgr))
            {
                // std::cout << "---- SPOTIFY AD DETECTED ----" << std::endl;
                bypass_ad(config_j, sptNormalWindowHandle);
            }
        }

        CloseHandle(sptNormalWindowHandle);
        CloseHandle(sptADWindowHandle);

        Sleep(1000);
    }

    // basic clean up
    CoUninitialize();
    return 0;
}

#endif // ifdef WIN32
