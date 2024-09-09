#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <regex>
#include <vector>

// Add this line to set the subsystem to Windows and specify the entry point
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")

std::string getAbsolutePath(const std::string& relativePath) {
    char fullPath[MAX_PATH];
    if (_fullpath(fullPath, relativePath.c_str(), MAX_PATH) != NULL) {
        return std::string(fullPath);
    }
    else {
        MessageBoxW(NULL, L"Failed to get absolute path", L"Error", MB_OK);
        return relativePath; // Return the original path if conversion fails
    }
}

void modifyVenvCfg(const std::string& envDir) {
    std::string absEnvDir = getAbsolutePath(envDir);
    std::string pythonDir = absEnvDir.substr(0, absEnvDir.find_last_of("\\/"));
    std::string filePath = absEnvDir + "\\pyvenv.cfg";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        MessageBoxW(NULL, L"Failed to open pyvenv.cfg file", L"Error", MB_OK);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        MessageBoxW(NULL, L"Failed to open pyvenv.cfg file for writing", L"Error", MB_OK);
        return;
    }

    std::regex homeRegex(R"(home\s*=\s*.*)");
    std::regex executableRegex(R"(executable\s*=\s*.*)");
    std::regex commandRegex(R"(command\s*=\s*.*)");

    content = std::regex_replace(content, homeRegex, "home = " + pythonDir);
    content = std::regex_replace(content, executableRegex, "executable = " + pythonDir + "\\python.exe");
    content = std::regex_replace(content, commandRegex, "command = " + pythonDir + "\\python.exe -m venv " + absEnvDir);

    outFile << content;
    outFile.close();
}

std::string readLauncherVersion(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        MessageBoxW(NULL, L"Failed to open JSON file", L"Error", MB_OK);
        return "0.0.1"; // Default version if file read fails
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::regex versionRegex(R"(\"launcher_version\"\s*:\s*"([^"]+)\")");
    std::smatch match;
    if (std::regex_search(content, match, versionRegex)) {
        return match[1].str();
    }
    else {
        MessageBoxW(NULL, L"Failed to find launcher_version in JSON file", L"Error", MB_OK);
        return "0.0.1"; // Default version if regex search fails
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
    std::string launcherVersion = readLauncherVersion(".\\configs\\launcher.json");

    // Modify the pyvenv.cfg file
    modifyVenvCfg(".\\python\\launcher_env");

    // Command to execute (modifiable string)
    std::wstring command = L".\\python\\launcher_env\\Scripts\\python.exe .\\repo\\" + std::wstring(launcherVersion.begin(), launcherVersion.end()) + L"\\launcher.py";

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
        // Wait for the process to complete or timeout after 10 seconds
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);
        DWORD exitCode = 0;
        //if (waitResult == WAIT_TIMEOUT) {
        //    TerminateProcess(pi.hProcess, 1);
        //    MessageBoxW(NULL, L"Process timed out", L"Error", MB_OK);
        //}
        //else {
        //    // Check the exit code of the process
        //    
        //}
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

        if (exitCode != 0 and exitCode != 259) {
            MessageBoxW(NULL, wstdoutStr.c_str(), L"Process Output (Error)", MB_OK);
        }        

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        // Handle error
        std::wcerr << L"Failed to create process" << std::endl;
        std::wcerr << L"Command: " << command << std::endl;
        MessageBoxW(NULL, L"Failed to create process", L"Error", MB_OK);
    }

    CloseHandle(hStdOutRead);
    CloseHandle(hStdOutWrite);

    return 0;
}
