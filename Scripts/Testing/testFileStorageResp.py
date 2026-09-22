import argparse
import socket
import sys


def build_resp_command(*parts: bytes) -> bytes:
    command = f"*{len(parts)}\r\n".encode()
    for part in parts:
        command += f"${len(part)}\r\n".encode() + part + b"\r\n"
    return command


def send_command(command: bytes, host: str = "localhost", port: int = 8000) -> bytes:
    with socket.create_connection((host, port), timeout=10.0) as s:
        s.sendall(command)
        response = b""
        s.settimeout(2.0)
        try:
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                response += chunk
        except socket.timeout:
            pass
        return response


def parse_bulk_string_reply(response: bytes) -> bytes:
    if not response.startswith(b"$"):
        raise ValueError(f"Expected a bulk string reply, got: {response[:50]!r}")

    header_end = response.find(b"\r\n")
    if header_end == -1:
        raise ValueError("Malformed reply: no header terminator found")

    length = int(response[1:header_end])
    if length == -1:
        return b""  # nil reply, e.g. key not found

    data_start = header_end + 2
    return response[data_start:data_start + length]


def cmd_set(args):
    with open(args.file, "rb") as f:
        data = f.read()

    command = build_resp_command(b"SET", args.key.encode(), data)
    response = send_command(command, args.host, args.port)
    print(response.decode(errors="replace"))


def cmd_setnx(args):
    with open(args.file, "rb") as f:
        data = f.read()

    command = build_resp_command(b"SETNX", args.key.encode(), data)
    response = send_command(command, args.host, args.port)
    print(response.decode(errors="replace"))


def cmd_get(args):
    command = build_resp_command(b"GET", args.key.encode())
    response = send_command(command, args.host, args.port)

    try:
        payload = parse_bulk_string_reply(response)
    except ValueError as e:
        print(f"[!] Could not parse reply: {e}")
        print(f"Raw reply: {response!r}")
        sys.exit(1)

    if not payload and response.startswith(b"$-1"):
        print(f"[!] No object found at key '{args.key}'")
        sys.exit(1)

    with open(args.path, "wb") as f:
        f.write(payload)

    print(f"Saved {len(payload)} bytes to {args.path}")


def cmd_keys(args):
    command = build_resp_command(b"KEYS", b"*")
    response = send_command(command, args.host, args.port)
    print(response.decode(errors="replace"))


def main():
    parser = argparse.ArgumentParser(description="Manual RESP test client for obsto")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8000, help="Server port (default: 8000)")

    subparsers = parser.add_subparsers(dest="command", required=True)

    # Standard command names without leading hyphens
    set_parser = subparsers.add_parser("set", help="Store a file's contents under a key")
    set_parser.add_argument("--key", required=True, help="Key/path to store under")
    set_parser.add_argument("--file", required=True, help="Path to the file to read and send")
    set_parser.set_defaults(func=cmd_set)

    setnx_parser = subparsers.add_parser("setnx", help="Store only if the key doesn't already exist")
    setnx_parser.add_argument("--key", required=True, help="Key/path to store under")
    setnx_parser.add_argument("--file", required=True, help="Path to the file to read and send")
    setnx_parser.set_defaults(func=cmd_setnx)

    get_parser = subparsers.add_parser("get", help="Retrieve a key's value and save it to a file")
    get_parser.add_argument("--key", required=True, help="Key/path to retrieve")
    get_parser.add_argument("--path", required=True, help="Where to save the retrieved bytes")
    get_parser.set_defaults(func=cmd_get)

    keys_parser = subparsers.add_parser("keys", help="List all stored keys")
    keys_parser.set_defaults(func=cmd_keys)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

