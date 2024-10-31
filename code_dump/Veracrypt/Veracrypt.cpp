#include <windows.h>
#include <iostream>
#include <string>

int main() {
    // Command to run VeraCrypt with the specified parameters
    std::wstring command = LR"(C:\Program Files\VeraCrypt\VeraCrypt.exe /volume "C:\Users\thaku\Desktop\mySensitiveData\MyVolume.hc" /letter x /password tarun123 /quit /silent)";

    // Initialize startup info and process info structures
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create the process
    if (!CreateProcess(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Error creating process: " << GetLastError() << std::endl;
        return 1;
    }

    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
