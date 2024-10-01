#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
#include <sstream>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")

std::wstring getAbsolutePath(const std::wstring& relativePath) {
    wchar_t fullPath[MAX_PATH];
    if (_wfullpath(fullPath, relativePath.c_str(), MAX_PATH) != NULL) {
        return std::wstring(fullPath);
    }
    else {
        MessageBoxW(NULL, L"Failed to get absolute path", L"Error", MB_OK);
        return relativePath; // Return the original path if conversion fails
    }
}

std::string WideStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring UTF8ToWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void modifyVenvCfg(const std::wstring& envDir) {
    std::wstring absEnvDir = getAbsolutePath(envDir);
    std::wstring pythonDir = absEnvDir.substr(0, absEnvDir.find_last_of(L"\\/"));
    std::wstring filePath = absEnvDir + L"\\pyvenv.cfg";

    // Open the file in UTF-8 mode
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        MessageBoxW(NULL, L"Failed to open pyvenv.cfg file", L"Error", MB_OK);
        return;
    }

    std::ostringstream contentStream;
    contentStream << file.rdbuf();
    std::string contentUTF8 = contentStream.str();
    file.close();

    std::wstring content = UTF8ToWideString(contentUTF8);

    // Modify the content using regex
    std::wregex homeRegex(LR"((\s*home\s*=\s*).*)");
    std::wregex executableRegex(LR"((\s*executable\s*=\s*).*)");
    std::wregex commandRegex(LR"((\s*command\s*=\s*).*)");

    content = std::regex_replace(content, homeRegex, L"$1" + pythonDir);
    content = std::regex_replace(content, executableRegex, L"$1" + pythonDir + L"\\python.exe");
    content = std::regex_replace(content, commandRegex, L"$1" + pythonDir + L"\\python.exe -m venv " + absEnvDir);

    // Convert the modified wide string back to UTF-8
    std::string contentModifiedUTF8 = WideStringToUTF8(content);

    // Write the modified content back to the file in UTF-8
    std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        MessageBoxW(NULL, L"Failed to open pyvenv.cfg file for writing", L"Error", MB_OK);
        return;
    }

    outFile.write(contentModifiedUTF8.c_str(), contentModifiedUTF8.size());
    outFile.close();
}


std::wstring readLauncherVersion(const std::wstring& filePath) {
    std::wifstream file(filePath);
    if (!file.is_open()) {
        MessageBoxW(NULL, L"Failed to open JSON file", L"Error", MB_OK);
        return L"0.0.1"; // Default version if file read fails
    }

    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    file.close();

    std::wregex versionRegex(LR"(\"launcher_version\"\s*:\s*"([^"]+)\")");
    std::wsmatch match;
    if (std::regex_search(content, match, versionRegex)) {
        return match[1].str();
    }
    else {
        MessageBoxW(NULL, L"Failed to find launcher_version in JSON file", L"Error", MB_OK);
        return L"0.0.1"; // Default version if regex search fails
    }
}

int main() {
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi)); // Initialize PROCESS_INFORMATION

    // Set the flags to hide the console window
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Read the launcher version from the JSON file
    std::wstring launcherVersion = readLauncherVersion(L".\\configs\\launcher.json");

    // Modify the pyvenv.cfg file
    modifyVenvCfg(L".\\python\\launcher_env");

    // Command to execute (modifiable string)
    std::wstring command = L".\\python\\launcher_env\\Scripts\\python.exe .\\repo\\" + launcherVersion + L"\\launcher.py";

    // Create pipes for capturing stdout
    HANDLE hStdOutRead, hStdOutWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    // Redirect stdout to the pipe
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;

    // Create the process
    if (CreateProcessW(NULL, &command[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // Wait for the process to complete or timeout after 30 seconds
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        // Read the stdout from the pipe
        DWORD bytesRead;
        CHAR buffer[4096];
        std::vector<CHAR> output;
        while (true) {
            DWORD bytesAvailable = 0;
            PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &bytesAvailable, NULL);
            if (bytesAvailable == 0) break;
            if (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                output.insert(output.end(), buffer, buffer + bytesRead);
            }
        }
        std::string stdoutStr(output.begin(), output.end());
        std::wstring wstdoutStr(stdoutStr.begin(), stdoutStr.end());

        if (exitCode != 0 && exitCode != 259) {
            MessageBoxW(NULL, wstdoutStr.c_str(), L"Process Output (Error)", MB_OK);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        // Handle error
        MessageBoxW(NULL, L"Failed to create process", L"Error", MB_OK);
    }

    CloseHandle(hStdOutRead);
    CloseHandle(hStdOutWrite);

    return 0;
}
