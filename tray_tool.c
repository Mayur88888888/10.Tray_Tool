#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_ICON_ID 1001
#define TRAY_MENU_ID_BASE 2000
#define MAX_MENU_ITEMS 1000
#define SETTINGS_FILE "settings.txt"


int UseCustomIcon = 0;
char IconFile[MAX_PATH] = "trayicon.ico";

int ShowBalloonOnStart = 1;
char BalloonTitle[128] = "Tray Tool Loaded";
char BalloonMessage[256] = "Right click tray icon to open menu";

int ShowExit = 1;
int ShowReload = 1;
int ShowPreferences = 1;
int ShowAbout = 1;

int MaxMenuItems = 1000;
int RecursiveFolders = 1;

char LogFile[MAX_PATH] = "tray_tool.log";


char TOOL_FOLDER[MAX_PATH] = "C:\\Tools";
char TOOL_NAME[128] = "Tray Tool";
char AUTHOR[128] = "OpenAI User";
COLORREF BG_COLOR = RGB(255,255,255);
COLORREF TEXT_COLOR = RGB(0,0,0);

NOTIFYICONDATA nid;
HMENU hTrayMenu;
char *batFiles[MAX_MENU_ITEMS];
int fileCount = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- Settings Parser ---
void LoadSettings() {
    FILE *fp = fopen(SETTINGS_FILE, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || strlen(line) < 3) continue;

        // Basic string values
        if (sscanf(line, "ToolFolder=%[^\n]", TOOL_FOLDER)) continue;
        if (sscanf(line, "ToolName=%[^\n]", TOOL_NAME)) continue;
        if (sscanf(line, "Author=%[^\n]", AUTHOR)) continue;

        // Colors (r,g,b)
        int r, g, b;
        if (sscanf(line, "BackgroundColor=%d,%d,%d", &r, &g, &b)) { BG_COLOR = RGB(r,g,b); continue; }
        if (sscanf(line, "TextColor=%d,%d,%d", &r, &g, &b)) { TEXT_COLOR = RGB(r,g,b); continue; }
        if (sscanf(line, "MenuHoverColor=%d,%d,%d", &r, &g, &b)) { /* store as needed */ continue; }
        if (sscanf(line, "MenuDisabledColor=%d,%d,%d", &r, &g, &b)) { /* store as needed */ continue; }

        // Boolean / int values
        if (sscanf(line, "UseCustomIcon=%d", &UseCustomIcon)) continue;
        if (sscanf(line, "ShowBalloonOnStart=%d", &ShowBalloonOnStart)) continue;
        if (sscanf(line, "ShowExit=%d", &ShowExit)) continue;
        if (sscanf(line, "ShowReload=%d", &ShowReload)) continue;
        if (sscanf(line, "ShowPreferences=%d", &ShowPreferences)) continue;
        if (sscanf(line, "ShowAbout=%d", &ShowAbout)) continue;
        if (sscanf(line, "MaxMenuItems=%d", &MaxMenuItems)) continue;
        if (sscanf(line, "RecursiveFolders=%d", &RecursiveFolders)) continue;

        // Strings
        if (sscanf(line, "IconFile=%[^\n]", IconFile)) continue;
        if (sscanf(line, "BalloonTitle=%[^\n]", BalloonTitle)) continue;
        if (sscanf(line, "BalloonMessage=%[^\n]", BalloonMessage)) continue;
        if (sscanf(line, "LogFile=%[^\n]", LogFile)) continue;
    }
    fclose(fp);
}


// --- Tray Icon ---
void AddTrayIcon(HWND hwnd, const char* tipMessage) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strncpy(nid.szTip, TOOL_NAME, sizeof(nid.szTip)-1);
    strncpy(nid.szInfo, tipMessage, sizeof(nid.szInfo)-1);
    strncpy(nid.szInfoTitle, TOOL_NAME, sizeof(nid.szInfoTitle)-1);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void FreeBatFiles() {
    for (int i = 0; i < fileCount; i++) {
        free(batFiles[i]);
    }
    fileCount = 0;
}

// --- Menu Builder ---
void AddBatchFilesToMenu(HMENU parentMenu, const char *folderPath) {
    WIN32_FIND_DATA findFileData;
    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s\\*", folderPath);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0) continue;

        char fullPath[MAX_PATH];
        snprintf(fullPath, MAX_PATH, "%s\\%s", folderPath, findFileData.cFileName);

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            HMENU subMenu = CreatePopupMenu();
            AppendMenu(parentMenu, MF_POPUP, (UINT_PTR)subMenu, findFileData.cFileName);
            AddBatchFilesToMenu(subMenu, fullPath);
        } else {
            char *ext = PathFindExtension(findFileData.cFileName);
            if (_stricmp(ext, ".bat") == 0 || _stricmp(ext, ".cmd") == 0) {
                if (fileCount < MAX_MENU_ITEMS) {
                    batFiles[fileCount] = _strdup(fullPath);
                    AppendMenu(parentMenu, MF_STRING, TRAY_MENU_ID_BASE + fileCount, findFileData.cFileName);
                    fileCount++;
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);
}

void ExecuteBatch(const char *filepath) {
    char command[1024];
    snprintf(command, sizeof(command), "/c \"%s\"", filepath);
    ShellExecute(NULL, "open", "cmd.exe", command, NULL, SW_HIDE);
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    FreeBatFiles();

    hTrayMenu = CreatePopupMenu();
    AddBatchFilesToMenu(hTrayMenu, TOOL_FOLDER);

    AppendMenu(hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hTrayMenu, MF_STRING, 9001, "Preferences");
    AppendMenu(hTrayMenu, MF_STRING, 9002, "About");
    AppendMenu(hTrayMenu, MF_STRING, 9003, "Reload");
    AppendMenu(hTrayMenu, MF_STRING, 9999, "Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hTrayMenu);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadSettings();

    const char CLASS_NAME[] = "TrayAppWindowClass";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, TOOL_NAME, 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    AddTrayIcon(hwnd, "Right-click the icon to access tools");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RemoveTrayIcon();
    FreeBatFiles();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON && lParam == WM_RBUTTONUP) {
        ShowContextMenu(hwnd);
    } else if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        if (id >= TRAY_MENU_ID_BASE && id < TRAY_MENU_ID_BASE + fileCount) {
            ExecuteBatch(batFiles[id - TRAY_MENU_ID_BASE]);
        } else if (id == 9001) {
            ShellExecute(NULL, "open", "notepad.exe", SETTINGS_FILE, NULL, SW_SHOW);
        } else if (id == 9002) {
            char info[256];
            snprintf(info, sizeof(info), "%s\nCreated by: %s", TOOL_NAME, AUTHOR);
            MessageBox(NULL, info, "About", MB_OK | MB_ICONINFORMATION);
        } else if (id == 9003) {
            LoadSettings();
            AddTrayIcon(hwnd, "Settings reloaded.");
        } else if (id == 9999) {
            PostQuitMessage(0);
        }
    } else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
