#include <iostream>
#include <chrono>

// windows includes
#include <Windows.h>
#include <Tlhelp32.h>

// requests and json
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

// remove the console window
// #pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

// media Commands for spotify
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

void waitForWindow(LPCSTR windowName , int waitTime)
{
    // waits for the user to open spotify
    HWND handle = FindWindowA(NULL, windowName);
    while (!handle)
    {
        handle = FindWindowA(NULL, windowName);
        Sleep(waitTime);
    }
}

void ChangeSpotifyState(SPOTIFY_COMMANDS spotify_command)
{
    // thanks to this guy --> "https://github.com/moobsmc/spotify-controls/blob/master/Spotify.cpp"

    // get the handle for spotify
    HWND Window_Handle = FindWindow("Chrome_WidgetWin_0", NULL);
    // send the message
    SendMessage(Window_Handle, WM_APPCOMMAND, 0, spotify_command);
}

void bypass_ad(nlohmann::json& config_j)
{
    /// get spotify window state ////

    // waits for spotify to change the window name then gets a window handle
    waitForWindow("Advertisement", 50);
    HWND temp_handle = FindWindowA(NULL, "Advertisement");

    // declares and defines variables that are going to receive the window "stats"
    WINDOWPLACEMENT wd = { sizeof(WINDOWPLACEMENT) };
    UINT nCurShow;

    // gets the window "stats" and saves them
    GetWindowPlacement(temp_handle, &wd);
    nCurShow = wd.showCmd;

    /// kill spotify ////////////////

    // killSpotify();
    // terminate spotify sending a SIGTERM
    try
    {
        system("taskkill /IM spotify.exe");
    } catch(const std::exception&) {}
    Sleep(config_j["timings"]["timeToWaitAfterClosingSpotify"]);
    // Sleep(1000);

    /// start spotify ///////////////

    ShellExecuteA(NULL, "open", "C:\\Users\\Titan\\AppData\\Roaming\\Spotify\\Spotify.exe", NULL, NULL, SW_SHOWMAXIMIZED);

    // waits for the app
    waitForWindow("Spotify Free", 50);
    HWND handle = FindWindowA(NULL, "Spotify Free");

    Sleep(config_j["timings"]["timeToWaitAfterSpotifySpotifyOpened"]);
    // Sleep(200);

    /// forces focus to the spotify window //////
    // props to this guy -> "https://stackoverflow.com/questions/19136365/win32-setforegroundwindow-not-working-all-the-time"

    /// THE FOLLOWING METHOD TO SET THE FOREGROUND WINDOW WILL NOT WORK IF THERE IS AN ACTIVE MENU (LIKE THE START MENU)
    /// BUT SetForegroundWindow NEVER REALLY WORKS IN PRACTISE ANYWAY, SO THIS IS BETTER
    // if GetForegroundWindow returns NULL it will just simply not do anything
    HWND foreground_hwnd = GetForegroundWindow();

    // get the process id of the current foreground window process
    DWORD windowThreadProcessId = GetWindowThreadProcessId(foreground_hwnd, LPDWORD(0));
    DWORD currentThreadId = GetCurrentThreadId();

    // attaches the current running process with the foreground window process
    AttachThreadInput(windowThreadProcessId, currentThreadId, true);
    BringWindowToTop(handle); // brings the specified window to the top of the Z order -> win32 api documentation
    ShowWindow(handle, SW_SHOWMAXIMIZED);

    Sleep(config_j["timings"]["timeToWaitAfterSettingSpotifyAsTheForegroundWindow"]);
    // Sleep(200);

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
    // Sleep(100);
    ChangeSpotifyState(SPOTIFY_PLAYPAUSE);

    /// give focus back to the 'original' window ///

    // bring back the focus to 'original' foreground window
    BringWindowToTop(foreground_hwnd);
    ShowWindow(foreground_hwnd, SW_SHOW);

    // detaches the current running process and the new foreground window process
    AttachThreadInput(windowThreadProcessId, currentThreadId, false);
}

int main(int argc, char** argv)
{
    // read the JSON config file
    std::ifstream config_f("config.json");
    nlohmann::json config_j;
    config_f >> config_j;

    // starts the loop that checks the current playing information
    while (true)
    {
        // waits for the user to open spotify
        HWND handle = FindWindowA(NULL, "Advertisement");
        if (handle)
        {
            // spotify is playing na ad
            bypass_ad(config_j);
        }

        // wait for 2 seconds
        Sleep(2000);
    }

    return 0;
}