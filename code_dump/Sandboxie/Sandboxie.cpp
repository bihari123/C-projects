#include <windows.h>
#include <iostream>
#include <string>

int main() {
    // Define the paths to the executables
    const char* sandboxiePath = "C:\\Program Files\\Sandboxie-Plus\\Start.exe";
    const char* hypersomniaPath = "C:\\Users\\thaku\\Downloads\\Hypersomnia-for-Windows\\hypersomnia\\Hypersomnia.exe";
    const char* supertuxPath = "C:\\Program Files\\SuperTux\\bin\\supertux2.exe";
    const char* boxOption = "/box:Test";

    // Ask the user to enter a number
    int choice;
    std::cout << "Enter 1 to run Hypersomnia or 2 to run SuperTux: ";
    std::cin >> choice;

    // Determine the path based on user input
    const char* selectedPath;
    if (choice == 1) {
        selectedPath = hypersomniaPath;
    }
    else if (choice == 2) {
        selectedPath = supertuxPath;
    }
    else {
        std::cerr << "Invalid choice. Exiting.\n";
        return 1;
    }

    // Construct the command line
    std::string commandLine = std::string("\"") + sandboxiePath + "\" " + boxOption + " \"" + selectedPath + "\"";

    // Initialize the STARTUPINFO and PROCESS_INFORMATION structures
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create the process
    if (!CreateProcessA(
        nullptr,          // No module name (use command line)
        &commandLine[0],  // Command line
        nullptr,          // Process handle not inheritable
        nullptr,          // Thread handle not inheritable
        FALSE,            // Set handle inheritance to FALSE
        0,                // No creation flags
        nullptr,          // Use parent's environment block
        nullptr,          // Use parent's starting directory 
        &si,              // Pointer to STARTUPINFO structure
        &pi)              // Pointer to PROCESS_INFORMATION structure
        ) {
        std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
        return 1;
    }

    // Wait until the child process exits
    DWORD result;
    do {
        result = WaitForSingleObject(pi.hProcess, 5000); // Check every 5 seconds
        if (result == WAIT_FAILED) {
            std::cerr << "WaitForSingleObject failed (" << GetLastError() << ").\n";
            return 1;
        }
    } while (result != WAIT_OBJECT_0);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
