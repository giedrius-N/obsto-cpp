import socket
import sys

COMMAND_MAP = {
    "-ping": "*1\r\n$4\r\nPING\r\n",
    "-set": "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$20\r\nhello object storage\r\n",
    "-get": "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n",
    "-keys": "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n",
    "-setnx": "*3\r\n$5\r\nSETNX\r\n$5\r\nmykey\r\n$20\r\nhello object storage\r\n"
}

def send_resp_command(command_str: str, host: str = "127.0.0.1", port: int = 8000):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client:
            client.settimeout(5.0)
            client.connect((host, port))
            
            client.sendall(command_str.encode('utf-8'))
            response = client.recv(4096)
            
            print("--- Raw Response Bytes ---")
            print(response)
            print("\n--- Decoded String Response ---")
            print(response.decode('utf-8', errors='replace'))

    except ConnectionRefusedError:
        print(f"[!] Error: Could not connect to {host}:{port}.")
        print("    Please verify that your RESP server is running and listening on port 8000.")
        sys.exit(1)
    except socket.timeout:
        print(f"[!] Error: Connection to {host}:{port} timed out after 5 seconds.")
        sys.exit(1)
    except socket.error as e:
        print(f"[!] Socket error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python testResp.py <flag>")
        print("Available flags:", ", ".join(COMMAND_MAP.keys()))
        sys.exit(1)

    flag = sys.argv[1].lower()

    if flag in COMMAND_MAP:
        print(f"Executing hardcoded command for flag '{flag}'...")
        send_resp_command(COMMAND_MAP[flag])
    else:
        print(f"[!] Unknown flag: '{flag}'")
        print("Available flags:", ", ".join(COMMAND_MAP.keys()))
        sys.exit(1)
