#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <lmcons.h>
#include <thread>

std::wstring GetActionString(DWORD action) {
    switch (action) {
    case FILE_ACTION_ADDED: return L"Added";
    case FILE_ACTION_REMOVED: return L"Removed";
    case FILE_ACTION_MODIFIED: return L"Modified";
    case FILE_ACTION_RENAMED_OLD_NAME: return L"Renamed (Old Name)";
    case FILE_ACTION_RENAMED_NEW_NAME: return L"Renamed (New Name)";
    default: return L"Unknown Action";
    }
}

void MonitorDirectory(const std::wstring& path) {
    HANDLE dirHandle = CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (dirHandle == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open directory: " << path << L". Error: " << GetLastError() << std::endl;
        return;
    }

    std::wcout << L"Monitoring " << path << L" for changes..." << std::endl;

    while (true) {
        BYTE buffer[4096];
        DWORD bytesReturned;
        OVERLAPPED overlapped = { 0 };

        if (ReadDirectoryChangesW(
            dirHandle,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned,
            &overlapped,
            NULL
        )) {
            WaitForSingleObject(dirHandle, INFINITE);

            FILE_NOTIFY_INFORMATION* notification = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            do {
                std::wstring fileName(notification->FileName, notification->FileNameLength / sizeof(WCHAR));
                std::wstring action = GetActionString(notification->Action);
                std::wcout << L"Path: " << path << L" - Action: " << action << L" - File: " << fileName << std::endl;

                notification = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(notification) + notification->NextEntryOffset
                    );
            } while (notification->NextEntryOffset != 0);
        }
        else {
            std::wcerr << L"Failed to read directory changes for " << path << L". Error: " << GetLastError() << std::endl;
            break;
        }
    }

    CloseHandle(dirHandle);
}

std::wstring GetCurrentUsername() {
    wchar_t username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    if (GetUserNameW(username, &username_len)) {
        return std::wstring(username);
    }
    return L"Unknown";
}

int main() {
    std::wstring username = GetCurrentUsername();
    std::wstring appDataPath = L"C:\\Users\\" + username + L"\\AppData";

    std::vector<std::wstring> pathsToMonitor = {
        L"C:\\Windows",
        L"C:\\Windows\\System32",
       // L"C:\\Windows\\SysWOW64",
        L"C:\\Windows\\boot",
        L"C:\\Program Files",
        L"C:\\Program Files (x86)",
        L"C:\\ProgramData",
        appDataPath,
        L"C:\\Windows\\Temp",
        L"C:\\Windows\\WinSxS"
    };

    std::vector<std::thread> monitorThreads;

    for (const auto& path : pathsToMonitor) {
        monitorThreads.emplace_back(MonitorDirectory, path);
    }

    for (auto& thread : monitorThreads) {
        thread.join();
    }

    return 0;
}