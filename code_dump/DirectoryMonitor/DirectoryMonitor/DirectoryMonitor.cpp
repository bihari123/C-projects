#include <algorithm>
#include <atomic>
#include <iostream>
#include <lmcons.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <winnt.h>
#define BUFFER_SIZE 4096

#include <iostream>

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

std::atomic<bool> shouldStop(false);

std::vector<std::wstring> programFilesTree;

std::vector<std::wstring> GetProgramFilesSubfolders() {
  std::vector<std::wstring> subfolders;
  std::vector<std::wstring> paths = {L"C:\\Program Files",
                                     L"C:\\Program Files (x86)"};

  for (const auto &path : paths) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW((path + L"\\*").c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            (wcscmp(findFileData.cFileName, L".") != 0) &&
            (wcscmp(findFileData.cFileName, L"..") != 0)) {
          std::wstring folderName = findFileData.cFileName;
          std::transform(folderName.begin(), folderName.end(),
                         folderName.begin(), ::towlower);
          if (!(folderName.find(L"microsoft") != std::wstring::npos ||
                folderName.find(L"windows") != std::wstring::npos ||
                folderName.find(L"nvidia") != std::wstring::npos)) {
            subfolders.push_back(folderName);
          }
        }
      } while (FindNextFileW(hFind, &findFileData) != 0);
      FindClose(hFind);
    } else {
      std::wcerr << L"Failed to access directory: " << path << L". Error code: "
                 << GetLastError() << std::endl;
    }
  }

  return subfolders;
}

// Global variable to store the user directories
std::vector<std::string> globalUserDirectories;

std::vector<std::wstring> WhiteListedFolders = {
    L"Nvidia", L"Microsoft", L"Google",   L"Epic Games",
    L"EA",     L"Chrome",    L"Fortnite",

};

bool IsScript(const std::wstring &fileName) {
  std::wstring ext = fileName.substr(fileName.find_last_of(L".") + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
  return (ext == L"bat" || ext == L"ps1" || ext == L"cmd" || ext == L"vbs" ||
          ext == L"js");
}

bool IsExecutable(const std::wstring &fileName) {
  std::wstring ext = fileName.substr(fileName.find_last_of(L".") + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
  return (ext == L"exe" || ext == L"dll" || ext == L"sys");
}

std::wstring GetActionString(DWORD action) {
  switch (action) {
  case FILE_ACTION_ADDED:
    return L"Added";
  case FILE_ACTION_REMOVED:
    return L"Removed";
  case FILE_ACTION_MODIFIED:
    return L"Modified";
  case FILE_ACTION_RENAMED_OLD_NAME:
    return L"Renamed (Old Name)";
  case FILE_ACTION_RENAMED_NEW_NAME:
    return L"Renamed (New Name)";
  default:
    return L"Unknown Action";
  }
}

bool ShouldIgnorePath(const std::wstring &path) {
  std::wstring lowercasePath = path;
  std::transform(lowercasePath.begin(), lowercasePath.end(),
                 lowercasePath.begin(), ::towlower);

  // Check for general terms to ignore
  const std::vector<std::wstring> ignoreTerms = {
      L"microsoft", L"updatestore", L"game", L"nvidia", L"log", L"packages",
      L"epic games", L"fortnite", L".log", L".db", L".txt", L"dat", L"appdata",
      L"network", // we are not monitoring network changes. That is taken care
                  // with another module
      L"cookies", L"prefetch"

  };

  for (const auto &term : ignoreTerms) {
    if (lowercasePath.find(term) != std::wstring::npos) {
      return true;
    }
  }

  return false;
}
void NotifyEvent(const std::wstring &eventType, const std::wstring &path,
                 const std::wstring &fileName) {
  std::wcout << L"[" << eventType << L"] " << path << L"\\" << fileName
             << std::endl;
}
bool changeInForbiddenDir(const std::wstring &target) {

  return std::find(globalUserDirectories.begin(), globalUserDirectories.end(),
                   target) != globalUserDirectories.end();
}

bool potentialUnauthorizedSoftInstall() {
  std::vector<std::wstring> currentFolders = GetProgramFilesSubfolders();

  std::vector<std::wstring> newFolders;
  for (const auto &folder : currentFolders) {
    if (std::find(programFilesTree.begin(), programFilesTree.end(), folder) ==
        programFilesTree.end()) {
      newFolders.push_back(folder);
    }
  }

  if (!newFolders.empty()) {
    // notify
    std::wcout << L"New folder(s) detected:" << std::endl;
    for (const auto &newFolder : newFolders) {
      std::wcout << newFolder << std::endl;
      programFilesTree.push_back(newFolder);
    }
    return true;
  }

  return false;
}

void AnalyzeAndNotify(const std::wstring &path, const std::wstring &fileName,
                      DWORD action) {
  std::wstring fullPath = path + L"\\" + fileName;

  if (changeInForbiddenDir(fullPath)) {
    // TODO: notify
    // ALERT: extreme
  }

  if (likely(ShouldIgnorePath(fullPath))) {
    return; // Ignore this path
  }
  std::wstring lowercasePath = path;
  std::wstring lowercaseFileName = fileName;
  std::transform(lowercasePath.begin(), lowercasePath.end(),
                 lowercasePath.begin(), ::towlower);
  std::transform(lowercaseFileName.begin(), lowercaseFileName.end(),
                 lowercaseFileName.begin(), ::towlower);

  switch (action) {

  case FILE_ACTION_MODIFIED:
    // TODO:
    //  if it is a executable, then is it running
    return;

  case FILE_ACTION_ADDED:
    // is a new executable added to the system32 folder
    if ((lowercaseFileName.find(L"system32") != std::wstring::npos) &&
        IsExecutable(lowercaseFileName)) {
      // TODO:
      // check the tlsh hash of the exe and compare it with th open threat
      // database
      // notify
    } else if ((lowercaseFileName.find(L"program files") !=
                std::wstring::npos) &&
               potentialUnauthorizedSoftInstall()) { // is it a new folder in
                                                     // program files

      // TODO:
      // notify
    }

    return;
  case FILE_ACTION_RENAMED_NEW_NAME:
    return;
  case FILE_ACTION_REMOVED:
    return;
  }

  /*
if (likely(action == FILE_ACTION_MODIFIED)) {
  return;
} else if (lowercaseFileName.find(L"program files") != std::wstring::npos &&
           (action == FILE_ACTION_ADDED ||
            action == FILE_ACTION_RENAMED_NEW_NAME)) {

  NotifyEvent(L"Potential Unauthorized Software Installation", path,
              fileName);
} else if (lowercaseFileName.find(L"programdata") != std::wstring::npos &&
           action ==)

  if (lowercasePath == L"c:\\" && action == FILE_ACTION_ADDED) {

    size_t backslashCount =
        std::count(fileName.begin(), fileName.end(), L'\\');
    if (backslashCount == 0) {
      NotifyEvent(L"New Item Created in Drive Root", path, fileName);
    }
  }
  */
  std::wcout << L" the path is " << lowercasePath << " " << lowercaseFileName
             << " " << GetActionString(action) << std::endl;

  /* if (changeInForbiddenDir(fullPath)) {
       // extremelu unsafe
   }*/

  if ((lowercasePath == L"c:\\program files" ||
       lowercasePath == L"c:\\program files (x86)") &&
      (action == FILE_ACTION_ADDED || action == FILE_ACTION_RENAMED_NEW_NAME)) {
    NotifyEvent(L"Potential Unauthorized Software Installation", path,
                fileName);
  } else if (lowercasePath == L"c:\\programdata" &&
             (action == FILE_ACTION_MODIFIED || action == FILE_ACTION_ADDED)) {
    NotifyEvent(L"Potential System-wide Configuration Change", path, fileName);
  } else if ((lowercasePath.find(L"\\temp") != std::wstring::npos ||
              lowercasePath.find(L"\\tmp") != std::wstring::npos) &&
             (action == FILE_ACTION_ADDED || action == FILE_ACTION_MODIFIED)) {
    if (IsScript(lowercaseFileName)) {
      NotifyEvent(L"Suspicious Activity in Temp Directory", path, fileName);
    }
  }

  else if (lowercasePath.find(L"\\system32\\drivers") != std::wstring::npos &&
           (action == FILE_ACTION_ADDED || action == FILE_ACTION_MODIFIED)) {
    NotifyEvent(L"Potential Malicious Driver Activity", path, fileName);
  } else if (lowercasePath.find(L"\\system32\\spool\\drivers") !=
                 std::wstring::npos &&
             (action == FILE_ACTION_ADDED || action == FILE_ACTION_MODIFIED)) {
    NotifyEvent(L"Potential Printer Driver Exploitation", path, fileName);
  }
}

void MonitorDirectory(const std::wstring &path) {
  HANDLE hDirectory = CreateFileW(
      path.c_str(), FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

  if (hDirectory == INVALID_HANDLE_VALUE) {
    std::wcerr << L"Error opening directory: " << path << L" (Error: "
               << GetLastError() << L")" << std::endl;
    return;
  }

  char buffer[BUFFER_SIZE] = {0};
  DWORD bytesReturned;
  OVERLAPPED overlapped = {0};
  overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  while (!shouldStop) {
    BOOL success = ReadDirectoryChangesW(
        hDirectory, buffer, BUFFER_SIZE, TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY,
        &bytesReturned, &overlapped, NULL);

    if (!success) {
      DWORD error = GetLastError();
      if (error != ERROR_IO_PENDING) {
        std::wcerr << L"Error reading directory changes: " << error
                   << std::endl;
        break;
      }
    }

    DWORD waitStatus =
        WaitForSingleObject(overlapped.hEvent, 1000); // Wait for 1 second

    if (waitStatus == WAIT_OBJECT_0) {
      if (!GetOverlappedResult(hDirectory, &overlapped, &bytesReturned,
                               FALSE)) {
        std::wcerr << L"Error in GetOverlappedResult: " << GetLastError()
                   << std::endl;
        break;
      }

      if (bytesReturned == 0) {
        continue;
      }

      FILE_NOTIFY_INFORMATION *pNotify;
      size_t offset = 0;

      do {
        pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(&buffer[offset]);
        std::wstring fileName(pNotify->FileName,
                              pNotify->FileNameLength / sizeof(WCHAR));

        AnalyzeAndNotify(path, fileName, pNotify->Action);
        offset += pNotify->NextEntryOffset;
      } while (pNotify->NextEntryOffset && offset < bytesReturned);

      // Reset event for next wait operation
      ResetEvent(overlapped.hEvent);
    } else if (waitStatus == WAIT_TIMEOUT) {
      // Timeout occurred, loop will continue
      continue;
    } else {
      std::wcerr << L"Wait failed: " << GetLastError() << std::endl;
      break;
    }
  }

  CloseHandle(overlapped.hEvent);
  CloseHandle(hDirectory);
}

std::string getCurrentUsername() {
  char username[UNLEN + 1];
  DWORD username_len = UNLEN + 1;
  if (GetUserNameA(username, &username_len)) {
    return std::string(username);
  }
  return "";
}

std::vector<std::string> getUserDirectories() {
  std::string username = getCurrentUsername();
  std::string baseDir = "C:\\Users\\" + username + "\\";

  std::vector<std::string> userDirectories = {
      baseDir + "Documents", baseDir + "Pictures", baseDir + "Music",
      baseDir + "Videos",
      baseDir + "OneDrive" // If OneDrive is set up
  };

  return userDirectories;
}

void initGlobalUsrDirs() {
  // Move the returned vector into the global variable
  globalUserDirectories = std::move(getUserDirectories());
}
void initProgramDirTree() {
  programFilesTree = std::move(GetProgramFilesSubfolders());
}
int main() {
  initProgramDirTree();
  initGlobalUsrDirs();
  std::vector<std::wstring> foldersToMonitor = {
      /*
        L"C:\\Windows",
        L"C:\\Windows\\System32",
        L"C:\\Program Files (x86)",
        L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp",
        L"C:\\Users\\thaku\\Desktop"
    */
      L"C:\\"};

  /*
   L"C:\\Windows",
  L"C:\\Windows\\System32",
  L"C:\\Windows\\SysWOW64",
  L"C:\\Program Files",
  L"C:\\Program Files (x86)",
  L"C:\\ProgramData",
  L"C:\\Users",
  L"C:\\Users\\Public",
  L"C:\\Windows\\Temp",
  L"C:\\",
  L"C:\\Windows\\System32\\drivers\\etc",
  L"C:\\Windows\\System32\\config",
  L"C:\\Windows\\System32\\spool\\drivers"
  */

  std::vector<std::thread> threads;

  for (const auto &folder : foldersToMonitor) {
    threads.emplace_back([folder]() {
      std::wcout << L"Monitoring: " << folder << std::endl;
      MonitorDirectory(folder);
    });
  }

  std::cout << "Press Enter to stop monitoring..." << std::endl;
  std::cin.get();
  shouldStop = true;

  for (auto &thread : threads) {
    thread.join();
  }

  return 0;
}
