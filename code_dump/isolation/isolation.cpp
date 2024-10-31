#include <windows.h>
#include <sddl.h>
#include <iostream>

void CreateSandbox() {
    // Create a job object
    HANDLE hJob = CreateJobObject(nullptr, nullptr);
    if (hJob == nullptr) {
        std::cerr << "Failed to create job object. Error: " << GetLastError() << std::endl;
        return;
    }

    // Configure job object to restrict the process
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
    jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
        std::cerr << "Failed to set job information. Error: " << GetLastError() << std::endl;
        CloseHandle(hJob);
        return;
    }

    // Create restricted token
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &hToken)) {
        std::cerr << "Failed to open process token. Error: " << GetLastError() << std::endl;
        CloseHandle(hJob);
        return;
    }

    HANDLE hRestrictedToken;
    if (!CreateRestrictedToken(hToken, DISABLE_MAX_PRIVILEGE, 0, nullptr, 0, nullptr, 0, nullptr, &hRestrictedToken)) {
        std::cerr << "Failed to create restricted token. Error: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        CloseHandle(hJob);
        return;
    }

    // Define security attributes
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    // Create restricted security descriptor
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
        "D:(D;;GA;;;BA)(A;;GA;;;WD)", // DACL: Deny all access to built-in administrators and everyone
        SDDL_REVISION_1,
        &sd,
        nullptr)) {
        std::cerr << "Failed to create security descriptor. Error: " << GetLastError() << std::endl;
        CloseHandle(hRestrictedToken);
        CloseHandle(hToken);
        CloseHandle(hJob);
        return;
    }

    sa.lpSecurityDescriptor = sd;

    // Create the process in the restricted environment
    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    if (!CreateProcessAsUser(
        hRestrictedToken,       // Restricted token
        "C:\\Program Files (x86)\\Epic Games\\Launcher\\Portal\\Binaries\\Win32\\EpicGamesLauncher.exe",      // Path to the executable
        nullptr,                 // Command line arguments
        &sa,                     // Process security attributes
        &sa,                     // Thread security attributes
        TRUE,                    // Inherit handles
        CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB | CREATE_NEW_CONSOLE, // Creation flags
        nullptr,                 // Environment
        nullptr,                 // Current directory
        &si,                     // Startup info
        &pi                      // Process information
    )) {
        std::cerr << "Failed to create process. Error: " << GetLastError() << std::endl;
        LocalFree(sd);
        CloseHandle(hRestrictedToken);
        CloseHandle(hToken);
        CloseHandle(hJob);
        return;
    }

    // Assign the process to the job object
    if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
        std::cerr << "Failed to assign process to job object. Error: " << GetLastError() << std::endl;
        TerminateProcess(pi.hProcess, 1);
    }

    // Resume the process
    if (ResumeThread(pi.hThread) == -1) {
        std::cerr << "Failed to resume thread. Error: " << GetLastError() << std::endl;
        TerminateProcess(pi.hProcess, 1);
    }

    // Wait for the process to exit
    WaitForSingleObject(pi.hProcess, INFINITE);
    Sleep(10000000);

    // Clean up handles and security descriptor
    LocalFree(sd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRestrictedToken);
    CloseHandle(hToken);
    CloseHandle(hJob);
}

int main() {
    CreateSandbox();
    return 0;
}
