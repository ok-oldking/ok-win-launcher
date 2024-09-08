#include <iostream>
#include <windows.h>

// Add this line to set the subsystem to Windows and specify the entry point
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")

int main()
{
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi)); // Initialize PROCESS_INFORMATION

    // Set the flags to hide the console window
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Command to execute (modifiable string)
    wchar_t command[] = L".\\python\\launcher_env\\Scripts\\pythonw.exe .\\launcher.py";

    // Create the process
    if (CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // Wait for the process to complete
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        // Handle error
        MessageBoxW(NULL, L"Failed to create process", L"Error", MB_OK);
    }

    return 0;
}
