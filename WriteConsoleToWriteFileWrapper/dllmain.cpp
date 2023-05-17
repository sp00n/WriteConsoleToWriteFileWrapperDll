#include "pch.h"
#include <chrono>
#include <ctime>
#include <stdio.h>
#include <string>
#include <windows.h>
#include "detours.h"


// Create a file handle to write to
//HANDLE hFile = CreateFile(L"test.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
// FILE_FLAG_NO_BUFFERING, FILE_FLAG_WRITE_THROUGH
HANDLE hFile;
std::wstring g_logFileName;


// Store the original WriteConsoleW function
static BOOL(WINAPI * originalWriteConsoleW)(HANDLE, const VOID*, DWORD, LPDWORD, LPVOID) = WriteConsoleW;


// Hook WriteConsoleW with a call to WriteFile
BOOL WINAPI HookedWriteConsoleW(HANDLE hConsoleOutput, const VOID* lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
    /*
    // Get the current system time
    auto currentTime = std::chrono::system_clock::now();

    // Convert the system time to a time_t object
    std::time_t currentTime_t = std::chrono::system_clock::to_time_t(currentTime);

    // Convert the time_t object to a string representation
    char timestamp[100];
    tm localTime;
    if (localtime_s(&localTime, &currentTime_t) == 0)
    {
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &localTime);
    }
    else
    {
        // Error handling for localtime_s failure
        // You can choose to skip the timestamp in case of failure or handle it differently
        // For simplicity, we will use an empty string as the timestamp in case of failure
        timestamp[0] = '\0';
    }

    // Convert the timestamp to a wide string
    std::wstring wideTimestamp(timestamp, timestamp + strlen(timestamp));
    */
    

    DWORD bytesWritten;
    DWORD bytesWrittenToFile;
    auto bytesToWrite = sizeof(WCHAR) * wcslen((LPCWSTR)lpBuffer);

    // Remove color codes from the UTF-16 string
    // If we don't remove them, the text after converting to UTF-8 will be garbled
    // The colors in the console window somehow still survive this process
    std::wstring filteredText;
    bool isColorCode = false;

    for (auto i = 0; i < bytesToWrite / sizeof(WCHAR); i++)
    {
        if (((WCHAR*)lpBuffer)[i] == L'\033')
        {
            // Found the beginning of a color code
            isColorCode = true;
        }
        else if (isColorCode)
        {
            // Skip the color code
            if (((WCHAR*)lpBuffer)[i] == L'm')
            {
                isColorCode = false;
            }
        }
        else
        {
            // Append non-color code characters to the filtered text
            filteredText += ((WCHAR*)lpBuffer)[i];
        }
    }

    // This may become a buffer overflow, but the text has to be really long for that (exceeding MAX_INT 32bit)
    int filteredTextLength = static_cast<int>(filteredText.length());

    // Convert UTF-16 filtered string to UTF-8
    auto bufferSize = WideCharToMultiByte(CP_UTF8, 0, filteredText.c_str(), filteredTextLength, NULL, 0, NULL, NULL);
    char* utf8Buffer = new char[bufferSize];
    auto bytesToBuffer = WideCharToMultiByte(CP_UTF8, 0, filteredText.c_str(), filteredTextLength, utf8Buffer, bufferSize, NULL, NULL);

    // Write the filtered UTF-8 string using WriteFile
    DWORD filteredBytesToWrite = bufferSize;
    BOOL result = WriteFile(hConsoleOutput, utf8Buffer, filteredBytesToWrite, &bytesWritten, NULL);
    
    // Write to the file
    WriteFile(hFile, utf8Buffer, filteredBytesToWrite, &bytesWrittenToFile, NULL);
    FlushFileBuffers(hFile);

    // Clean up allocated memory
    delete[] utf8Buffer;


    // WriteFile() returns number of bytes written, but WriteConsoleW() has to return the number of characters written.
    if (bytesWritten)
    {
        *lpNumberOfCharsWritten = bytesWritten / sizeof(WCHAR);
    }

    return result;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    LONG error;

    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();

        /*printf(DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
            " Starting.\n");
        fflush(stdout);*/

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)originalWriteConsoleW, HookedWriteConsoleW);
        error = DetourTransactionCommit();

        if (error == NO_ERROR) {
            /*printf(DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                " Detoured WriteConsoleW().\n");*/
        }
        else {
            /*printf(DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                " Error detouring WriteConsoleW(): %ld\n", error);
            fflush(stdout);*/
        }

        // Get the command line for our log file name
        LPWSTR commandLine = GetCommandLineW();

        // Convert the command line to a wide string
        std::wstring dllArgument(commandLine);

        // Search for the "/dlllog:" argument and extract the file name
        std::wstring argPrefix = L"/dlllog:";
        size_t argIndex = dllArgument.find(argPrefix);
        if (argIndex != std::wstring::npos) {
            g_logFileName = dllArgument.substr(argIndex + argPrefix.length());
            
        }
        else {
            g_logFileName = L"defaultLogFile.txt";
        }

        hFile = CreateFile(g_logFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)originalWriteConsoleW, HookedWriteConsoleW);
        error = DetourTransactionCommit();

        /*printf(DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
            " Removed WriteConsoleW() (result=%ld)\n", error);
        fflush(stdout);*/

        CloseHandle(hFile);
    }
    return TRUE;
}