# test_connection.py
import socket
import sys

def test_ports():
    print("="*60)
    print("LEVERUP C2 - CONNECTION TEST")
    print("="*60)
    
    # Test Web Interface
    print("\n[1] Testing Web Interface (Port 5000)...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        result = sock.connect_ex(('127.0.0.1', 5000))
        sock.close()
        
        if result == 0:
            print("   ✓ Web Interface: RUNNING")
            print("      URL: http://127.0.0.1:5000")
        else:
            print("   ✗ Web Interface: NOT RUNNING")
            print("      Start with: python app.py")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    # Test C2 Server Port
    print("\n[2] Testing C2 Server Port (Port 8080)...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        result = sock.connect_ex(('127.0.0.1', 8080))
        sock.close()
        
        if result == 0:
            print("   ✓ C2 Server Port: LISTENING")
        else:
            print("   ✗ C2 Server Port: NOT LISTENING")
            print("      Start server from web interface")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print("\n" + "="*60)
    print("SOLUTION:")
    print("1. Run: python app.py")
    print("2. Open: http://127.0.0.1:5000")
    print("3. Click 'Start C2' -> Enter: IP: 0.0.0.0, Port: 8080")
    print("4. COMPILE bot with YOUR ACTUAL IP (not 192.168.1.6)")
    print("5. Run bot.exe")
    print("="*60)

if __name__ == "__main__":
    test_ports()