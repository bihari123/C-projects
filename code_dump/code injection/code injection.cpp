#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>

// Define the constants we need
#define ThreadQuerySetWin32StartAddress 9

// Define the function prototype for NtQueryInformationThread
typedef NTSTATUS(WINAPI* PNtQueryInformationThread)(
    HANDLE ThreadHandle,
    ULONG ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
    );

// Function to get process name from process ID
std::wstring GetProcessName(DWORD processId) {
    WCHAR szProcessName[MAX_PATH] = L"<unknown>";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

    if (hProcess != NULL) {
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(WCHAR));
        }
    }
    CloseHandle(hProcess);
    return szProcessName;
}

// Callback function for thread creation events
VOID CALLBACK ThreadCreationCallback(
    PVOID lpParameter,
    BOOLEAN TimerOrWaitFired
) {
    PVOID startAddress = NULL;
    HANDLE hThread = *(PHANDLE)lpParameter;
    DWORD threadId = GetThreadId(hThread);
    DWORD processId = GetProcessIdOfThread(hThread);

    // Dynamically load NtQueryInformationThread
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    PNtQueryInformationThread NtQueryInformationThread = (PNtQueryInformationThread)GetProcAddress(hNtDll, "NtQueryInformationThread");

    if (NtQueryInformationThread) {
        // Get the start address of the thread
        NTSTATUS status = NtQueryInformationThread(hThread, ThreadQuerySetWin32StartAddress, &startAddress, sizeof(startAddress), NULL);
        if (status != 0) {
            std::wcout << L"Failed to query thread information. Status: " << status << std::endl;
        }
    }

    std::wcout << L"New thread created in process: " << GetProcessName(processId)
        << L" (PID: " << processId << L")" << std::endl;
    std::wcout << L"Thread ID: " << threadId << L", Start Address: " << startAddress << std::endl;

    // Check if the thread start address is outside the process's address space
    if (startAddress) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(GetCurrentProcess(), startAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_FREE || mbi.Type == MEM_PRIVATE) {
                std::wcout << L"WARNING: Possible code injection detected!" << std::endl;
            }
        }
    }
}

int main() {
    HANDLE hThreadEvent;

    // Create a thread creation event
    hThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hThreadEvent == NULL) {
        std::cerr << "Failed to create event" << std::endl;
        return 1;
    }

    // Register for thread creation notifications
    HANDLE hThreadReg;
    if (!RegisterWaitForSingleObject(&hThreadReg, hThreadEvent, ThreadCreationCallback,
        &hThreadEvent, INFINITE, WT_EXECUTEDEFAULT)) {
        std::cerr << "Failed to register for thread creation events" << std::endl;
        CloseHandle(hThreadEvent);
        return 1;
    }

    std::wcout << L"Monitoring for code injection attempts..." << std::endl;

    // Main loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    UnregisterWait(hThreadReg);
    CloseHandle(hThreadEvent);

    return 0;
}