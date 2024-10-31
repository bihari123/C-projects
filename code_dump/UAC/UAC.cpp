#include <Windows.h>
#include <UserEnv.h>
#include <Sddl.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "userenv.lib")

// Function to get an existing AppContainer profile
HRESULT GetAppContainerProfile(
    PCWSTR pszAppContainerName,
    PSID* ppAppContainerSid)
{
    DWORD dwLength = SECURITY_MAX_SID_SIZE;
    *ppAppContainerSid = (PSID)HeapAlloc(GetProcessHeap(), 0, dwLength);

    if (*ppAppContainerSid == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT hr = DeriveAppContainerSidFromAppContainerName(pszAppContainerName, ppAppContainerSid);

    if (FAILED(hr))
    {
        HeapFree(GetProcessHeap(), 0, *ppAppContainerSid);
        *ppAppContainerSid = nullptr;
        return hr;
    }

    return S_OK;
}

// Function to create an AppContainer profile
HRESULT CreateMyAppContainerProfile(
    PCWSTR pszAppContainerName,
    PCWSTR pszDisplayName,
    PCWSTR pszDescription,
    PSID_AND_ATTRIBUTES pCapabilities,
    DWORD dwCapabilityCount,
    PSID* ppAppContainerSid)
{
    HRESULT hr = S_OK;
    *ppAppContainerSid = nullptr;

    hr = CreateAppContainerProfile(pszAppContainerName, pszDisplayName, pszDescription, pCapabilities, dwCapabilityCount, ppAppContainerSid);

    if (FAILED(hr))
    {
        std::wcout << L"CreateAppContainerProfile failed with error: " << hr << std::endl;
    }

    return hr;
}

// Function to launch the application in AppContainer
bool LaunchAppInAppContainer(const std::wstring& appPath, const std::wstring& appContainerName)
{
    PSID appContainerSid = nullptr;

    // Check if the AppContainer profile already exists
    HRESULT hr = GetAppContainerProfile(appContainerName.c_str(), &appContainerSid);

    if (FAILED(hr))
    {
        // Create a new AppContainer profile
        hr = CreateMyAppContainerProfile(appContainerName.c_str(), appContainerName.c_str(), L"AppContainer Profile", nullptr, 0, &appContainerSid);

        if (FAILED(hr) || appContainerSid == nullptr)
        {
            std::wcout << L"Failed to create AppContainer profile. Error: " << hr << std::endl;
            return false;
        }
    }

    // Set up the SECURITY_CAPABILITIES structure
    SECURITY_CAPABILITIES securityCapabilities = {};
    securityCapabilities.AppContainerSid = appContainerSid;

    // Convert the appPath to LPWSTR
    LPWSTR appPathCStr = const_cast<LPWSTR>(appPath.c_str());

    // Set up the STARTUPINFOEX structure
    STARTUPINFOEX startupInfoEx = {};
    startupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    startupInfoEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attributeSize);

    if (startupInfoEx.lpAttributeList == nullptr)
    {
        std::wcout << L"HeapAlloc failed." << std::endl;
        return false;
    }

    if (!InitializeProcThreadAttributeList(startupInfoEx.lpAttributeList, 1, 0, &attributeSize))
    {
        std::wcout << L"InitializeProcThreadAttributeList failed." << std::endl;
        return false;
    }

    if (!UpdateProcThreadAttribute(startupInfoEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &securityCapabilities, sizeof(SECURITY_CAPABILITIES), nullptr, nullptr))
    {
        std::wcout << L"UpdateProcThreadAttribute failed." << std::endl;
        return false;
    }

    PROCESS_INFORMATION processInfo = {};

    if (!CreateProcessAsUser(
        nullptr,
        appPathCStr,
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &startupInfoEx.StartupInfo,
        &processInfo))
    {
        std::wcout << L"CreateProcessAsUser failed with error: " << GetLastError() << std::endl;

        // Clean up
        DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, startupInfoEx.lpAttributeList);
        FreeSid(appContainerSid);

        return false;
    }

    std::wcout << L"CreateProcessAsUser succeeded. Process ID: " << processInfo.dwProcessId << std::endl;

    // Wait for the process to complete
    WaitForSingleObject(processInfo.hProcess, INFINITE);

    // Clean up
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    DeleteProcThreadAttributeList(startupInfoEx.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, startupInfoEx.lpAttributeList);
    FreeSid(appContainerSid);

    return true;
}

int main()
{
    std::wstring appPath = L"C:\\Program Files (x86)\\Epic Games\\Launcher\\Portal\\Binaries\\Win32\\EpicGamesLauncher.exe";
    std::wstring appContainerName = L"YourAppContainerName";

    if (LaunchAppInAppContainer(appPath, appContainerName))
    {
        std::wcout << L"Application launched successfully in AppContainer." << std::endl;
    }
    else
    {
        std::wcout << L"Failed to launch application in AppContainer." << std::endl;
    }

    return 0;
}
