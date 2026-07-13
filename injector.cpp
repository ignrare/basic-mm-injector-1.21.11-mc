#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <psapi.h>

typedef HINSTANCE(WINAPI* LoadLibraryA_t)(LPCSTR);
typedef FARPROC(WINAPI* GetProcAddress_t)(HMODULE, LPCSTR);
typedef HANDLE(WINAPI* CreateRemoteThread_t)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef LPVOID(WINAPI* VirtualAllocEx_t)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL(WINAPI* WriteProcessMemory_t)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
typedef BOOL(WINAPI* CloseHandle_t)(HANDLE);

void setColor(int textColor, int bgColor) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, (bgColor << 4) | textColor);
}

void printColored(const std::string& text, int textColor, int bgColor) {
    setColor(textColor, bgColor);
    std::cout << text << std::endl;  
    setColor(7, 0);  
}


bool isMinecraftRunning() {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        printColored("Failed to create snapshot.", 12, 0);
        return false;
    }

    if (Process32First(hSnapshot, &pe32)) {
        do {
            
            if (wcscmp(pe32.szExeFile, L"javaw.exe") == 0 || wcscmp(pe32.szExeFile, L"Feather Launcher.exe") == 0) {
                CloseHandle(hSnapshot);
                return true;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return false;
}


std::vector<BYTE> readDLL(const std::string& dllPath) {
    std::ifstream file(dllPath, std::ios::binary);
    if (!file) {
        printColored("Failed to open DLL file.", 12, 0);
        return {};
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<BYTE> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        printColored("DLL file read successfully.", 10, 0);
    }
    else {
        printColored("Failed to read DLL file.", 12, 0);
    }

    file.close();
    return buffer;
}


std::vector<BYTE> encryptDLL(const std::vector<BYTE>& dllBuffer) {
    std::vector<BYTE> encrypted(dllBuffer);
    for (size_t i = 0; i < encrypted.size(); ++i) {
        encrypted[i] ^= 0xAA;  
    }
    return encrypted;
}

std::vector<BYTE> decryptDLL(const std::vector<BYTE>& encryptedDLL) {
    std::vector<BYTE> decrypted(encryptedDLL);
    for (size_t i = 0; i < decrypted.size(); ++i) {
        decrypted[i] ^= 0xAA;  
    }
    return decrypted;
}


void injectDLL(DWORD processId, const std::vector<BYTE>& dllBuffer) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        printColored("Failed to open process.", 12, 0);
        return;
    }

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    if (hKernel32 == NULL) {
        printColored("Failed to get kernel32.dll module handle.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    LoadLibraryA_t pLoadLibraryA = (LoadLibraryA_t)GetProcAddress(hKernel32, "LoadLibraryA");
    if (pLoadLibraryA == NULL) {
        printColored("Failed to get address of LoadLibraryA.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    VirtualAllocEx_t pVirtualAllocEx = (VirtualAllocEx_t)GetProcAddress(hKernel32, "VirtualAllocEx");
    if (pVirtualAllocEx == NULL) {
        printColored("Failed to get address of VirtualAllocEx.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    WriteProcessMemory_t pWriteProcessMemory = (WriteProcessMemory_t)GetProcAddress(hKernel32, "WriteProcessMemory");
    if (pWriteProcessMemory == NULL) {
        printColored("Failed to get address of WriteProcessMemory.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    CreateRemoteThread_t pCreateRemoteThread = (CreateRemoteThread_t)GetProcAddress(hKernel32, "CreateRemoteThread");
    if (pCreateRemoteThread == NULL) {
        printColored("Failed to get address of CreateRemoteThread.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    CloseHandle_t pCloseHandle = (CloseHandle_t)GetProcAddress(hKernel32, "CloseHandle");
    if (pCloseHandle == NULL) {
        printColored("Failed to get address of CloseHandle.", 12, 0);
        CloseHandle(hProcess);
        return;
    }

    
    LPVOID allocMem = pVirtualAllocEx(hProcess, NULL, dllBuffer.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (allocMem == NULL) {
        printColored("Failed to allocate memory in target process.", 12, 0);
        pCloseHandle(hProcess);
        return;
    }

    
    if (!pWriteProcessMemory(hProcess, allocMem, dllBuffer.data(), dllBuffer.size(), NULL)) {
        printColored("Failed to write DLL to target process memory.", 12, 0);
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        pCloseHandle(hProcess);
        return;
    }

    
    HANDLE hThread = pCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryA, allocMem, 0, NULL);
    if (hThread == NULL) {
        printColored("Failed to create remote thread.", 12, 0);
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        pCloseHandle(hProcess);
        return;
    }

    WaitForSingleObject(hThread, INFINITE); 
    pCloseHandle(hThread);
    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    pCloseHandle(hProcess);
    printColored("DLL injected successfully!", 10, 0);
}

int main() {
    const std::string dllPath = "C:\path\to\cheat.dll";  // change this to ur cheat dll file path
    printColored("Waiting for Minecraft to start...", 10, 0);

   
    while (!isMinecraftRunning()) {
        Sleep(1000);  
    }

    printColored("Minecraft detected! Reading DLL file...", 10, 0);
    std::vector<BYTE> dllBuffer = readDLL(dllPath);
    if (dllBuffer.empty()) {
        return 1;  
    }

    
    dllBuffer = encryptDLL(dllBuffer);

    printColored("Waiting 5 seconds before injecting to avoid detection...", 10, 0);
    Sleep(5000);  

    DWORD processId = 0;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        printColored("Failed to create snapshot.", 12, 0);
        return 1;
    }

    if (Process32First(hSnapshot, &pe32)) {
        do {
            
            if (wcscmp(pe32.szExeFile, L"javaw.exe") == 0 || wcscmp(pe32.szExeFile, L"Feather Launcher.exe") == 0) {
                processId = pe32.th32ProcessID;
                break;  
                break; 
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (processId == 0) {
        printColored("Minecraft process not found", 12, 0);
        return 1;  
    }

    printColored("Injecting DLL into Minecraft process", 10, 0);
    injectDLL(processId, dllBuffer);

    return 0;
}
