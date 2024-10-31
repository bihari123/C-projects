#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
void PrintError(const std::string& message) {
    std::cerr << message << " Error: " << GetLastError() << std::endl;
}


 
void ListRunningProcesses() {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        PrintError("CreateToolhelp32Snapshot failed.");
        return;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hProcessSnap, &pe32)) {
        PrintError("Process32First failed.");
        CloseHandle(hProcessSnap);
        return;
    }

    do {
        std::wcout << L"Process ID: " << pe32.th32ProcessID << L" | Executable: " << pe32.szExeFile << std::endl;
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
}

int main() {
    // Step 1: Create a Job Object
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) {
        PrintError("CreateJobObject failed.");
        return 1;
    }

    // Step 2: Set Job Object Limits (optional)
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = { 0 };
    jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
        PrintError("SetInformationJobObject failed.");
        CloseHandle(hJob);
        return 1;
    }

    // Step 3: Create a Process
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);

    const wchar_t* exePath = L"C:\\Windows\\System32\\notepad.exe";  // Using notepad.exe for testing
    wchar_t cmdLine[MAX_PATH];
    wcscpy_s(cmdLine, exePath);

    // Print the path to confirm
    std::wcout << L"Attempting to run: " << cmdLine << std::endl;

    if (!CreateProcessW(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,  // Start the process in a new console
        NULL,
        NULL,
        &si,
        &pi)) {
        PrintError("CreateProcess failed.");
        CloseHandle(hJob);
        return 1;
    }

    // Print process ID
    std::wcout << L"Process ID: " << pi.dwProcessId << std::endl;

    // Assign the process to the job object
    if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
        PrintError("AssignProcessToJobObject failed.");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hJob);
        return 1;
    }

    std::cout << "Process started successfully." << std::endl;

    // Set the wait timer to check the process status every 1 second (1000 milliseconds)
    const DWORD checkInterval = 1000;
    const DWORD waitTime = 600000;  // 10 minutes

    DWORD startTime = GetTickCount();
    DWORD exitCode;

    // Keep the main program running and monitor the child process
    while (true) {
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            if (exitCode == STILL_ACTIVE) {
                std::cout << "Process is still running." << std::endl;
            }
            else {
                std::cout << "Process has exited with code: " << exitCode << std::endl;
                break;
            }
        }
        else {
            PrintError("GetExitCodeProcess failed.");
            break;
        }

        if (GetTickCount() - startTime >= waitTime) {
            std::cout << "10 minutes have passed, process is still running." << std::endl;
            break;
        }

        Sleep(checkInterval);
    }
    int i = 0;
    while (true) {
        ListRunningProcesses();
        if (i > 3) {
            break;
    }
        i++;
    }
    // Wait for an additional 10 minutes before exiting (600,000 milliseconds)
    std::cout << "Waiting for 10 more minutes before exiting..." << std::endl;
    Sleep(600000);  // Sleep for 600,000 milliseconds (10 minutes)

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hJob);

    std::cout << "Main program exiting." << std::endl;
    return 0;
}
