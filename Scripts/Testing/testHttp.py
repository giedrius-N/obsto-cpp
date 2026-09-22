import http.client
import sys

HOST = "127.0.0.1"
PORT = 8000


def send_http_request(method: str, path: str, body: str = None):
    try:
        conn = http.client.HTTPConnection(HOST, PORT, timeout=5.0)
        headers = {}
        
        if body is not None:
            headers["Content-Length"] = str(len(body.encode('utf-8')))

        print(f"Executing: {method} {path}")
        conn.request(method, path, body=body, headers=headers)
        
        response = conn.getresponse()
        raw_body = response.read()

        print(f"--- Status Code: {response.status} {response.reason} ---")
        print("--- Response Headers ---")
        for header, value in response.getheaders():
            print(f"{header}: {value}")
            
        print("\n--- Raw Response Body ---")
        print(raw_body)
        print("\n--- Decoded Body ---")
        print(raw_body.decode('utf-8', errors='replace'))
        
        conn.close()

    except ConnectionRefusedError:
        print(f"[!] Error: Could not connect to {HOST}:{PORT}.")
        print("    Please verify that your server is running.")
        sys.exit(1)
    except TimeoutError:
        print(f"[!] Error: Connection to {HOST}:{PORT} timed out after 5 seconds.")
        sys.exit(1)
    except Exception as e:
        print(f"[!] Request failed: {e}")
        sys.exit(1)


def print_usage():
    print("Usage: python testHttp.py <command> [args...]")
    print("\nAvailable commands:")
    print("  -list             Lists all stored object paths (GET /)")
    print("  -get <path>       Retrieves object at path (GET /<path>)")
    print("  -put <path> <val> Stores value at path (PUT /<path>)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    flag = sys.argv[1].lower()

    if flag == "-list":
        send_http_request("GET", "/")

    elif flag == "-get":
        if len(sys.argv) < 3:
            print("[!] Error: Missing path argument for -get")
            print("Usage: python testHttp.py -get <path>")
            sys.exit(1)
        path = sys.argv[2] if sys.argv[2].startswith("/") else f"/{sys.argv[2]}"
        send_http_request("GET", path)

    elif flag == "-put":
        if len(sys.argv) < 4:
            print("[!] Error: Missing path or value argument for -put")
            print("Usage: python testHttp.py -put <path> <value>")
            sys.exit(1)
        path = sys.argv[2] if sys.argv[2].startswith("/") else f"/{sys.argv[2]}"
        body = sys.argv[3]
        send_http_request("PUT", path, body=body)

    else:
        print(f"[!] Unknown flag: '{flag}'")
        print_usage()
        sys.exit(1)
