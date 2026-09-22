# obsto

```text
   ██████╗ ██████╗ ███████╗████████╗ ██████╗
  ██╔═══██╗██╔══██╗██╔════╝╚══██╔══╝██╔═══██╗
  ██║   ██║██████╔╝███████╗   ██║   ██║   ██║
  ██║   ██║██╔══██╗╚════██║   ██║   ██║   ██║
  ╚██████╔╝██████╔╝███████║   ██║   ╚██████╔╝
   ╚═════╝ ╚═════╝ ╚══════╝   ╚═╝    ╚═════╝
```

**obsto** is a simple C++ object storage service, built to store arbitrary objects (text, binary files, images) under a string key/path.

The same storage engine can be accessed through two protocols on the same port:

```text
                         ┌─────────────────┐
                         │      OBSTO      │
                         │  Object Storage │
                         └────────┬────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │                           │
               HTTP/1.1                       RESP
                    │                           │
             ┌──────┴──────┐             ┌──────┴──────┐
             │ curl / HTTP │             │ redis-cli   │
             │   clients   │             │   clients   │
             └─────────────┘             └─────────────┘
```

## Protocols supported

### HTTP/1.1

- `GET /` - list all stored object paths, separated by newline.
- `GET /{path}` - retrieve the object stored at `{path}`.
- `PUT /{path}` - store (or overwrite) the object at `{path}`, using the request body as the object's contents.
- `DELETE /{path}` - remove the object at `{path}`.

Examples with `curl`:
```bash
curl -T myfile.png http://localhost:8000/myfile.png     # store
curl http://localhost:8000/myfile.png -o out.png        # retrieve
curl -X DELETE http://localhost:8000/myfile.png         # delete
curl http://localhost:8000/                             # list all keys
```

### RESP (Redis Serialization Protocol)

Only the following commands are implemented - **not the full Redis command set**:

- `PING` - connectivity check, replies `PONG` (or echoes an argument if given).
- `KEYS *` - list all stored object paths.
- `GET {path}` - retrieve the object at `{path}`.
- `SET {path} {object}` - store (or overwrite) the object at `{path}`.
- `SETNX {path} {object}` - store the object only if `{path}` doesn't already exist: replies `1` if stored, `0` if it already existed.
- `DEL {path}` - remove the object at `{path}`.

Examples `redis-cli`:
```bash
redis-cli -h localhost -p 8000
> PING
> SET mykey "hello"
> GET mykey
> KEYS *
> DEL mykey
```

## Object storage behavior

- Objects are persisted to disk, and the on-disk index is rebuilt automatically on server startup, so previously stored objects survive a restart.
- Large uploads and downloads are streamed in chunks rather than held entirely in memory, so an object larger than available RAM doesn't crash the process.
- Total stored capacity is bounded by a configurable limit, once reached, new writes are rejected rather than growing without bound.
- Malformed, truncated, or otherwise malicious input on either protocol is rejected without crashing the server or leaving connections hung.

## Building & running with Docker

Build the image from the repository root:

```bash
docker build -t obsto .
```

Run the server with port `8000` exposed and persistent object storage:

```bash
docker run -d --name obsto -p 8000:8000 -v obsto-data:/data obsto
```

The service is available at `http://localhost:8000`. For example:

```bash
curl -X PUT --data "hello" http://localhost:8000/test.txt
curl http://localhost:8000/test.txt
```

The `obsto-data` volume keeps stored objects after the container is removed.

To change the port or storage capacity, pass arguments after the image name:

```bash
docker run -d --name obsto -p 9000:9000 -v obsto-data:/data obsto --port 9000 --capacity 500 --path /data
```

## Building & running (Windows)

### Prerequisites

Download and install:
- Conan 2.x
- CMake 3.15 or newer
- A C++20 capable compiler (e.g. GCC 10+)
- (Optional, for static analysis only) `clang-tidy`

---

Make sure `cmake`, `conan`, your compiler, and (if used) `clang-tidy` are available on `PATH`.

- Use `Scripts\buildObsto.cmd` to configure and build (pass `-release` or `-debug` for a specific build type, or `-q` for a quick build).
- `Scripts\runObsto.cmd` to run the server. Any arguments are forwarded to the executable, and it also accepts an optional `-debug`/`-release` flag itself to pick which build to run (defaults to `-debug`).

Available options:

| Flag | Short | Description | Default |
|---|---|---|---|
| `--port` | `-p` | Server port | `8000` |
| `--capacity` | `-c` | Storage capacity in MB | `200` |
| `--path` | `-P` | Storage path | `C:\develop\obsto` |
| `--help` | `-h` | Show help message | - |

Example:
```bash
Scripts\runObsto.cmd --port 8080 --capacity 500 --path C:\data\storage
Scripts\runObsto.cmd -p 8080 -c 500 -P C:\data\storage
```

## Tests

- Use `Scripts\buildTests.cmd` to build the test executable.
- `Scripts\runTests.cmd` to run it. Any extra arguments passed to `runTests.cmd` are forwarded to the GTest binary, so flags like `--gtest_filter=*Test.* --gtest_repeat=2` work as expected.

**The most important test suite is `TestIntegration`** (`Tests/IntgrTests/TestIntegration.cpp`). While most other test files exercise individual classes in isolation (parsers, storage classes, protocol handlers against mocks), `TestIntegration` uses real instance of the server - real sockets, real disk-backed storage, real `Server`/`Session` networking - and drives it exactly as an actual client would.

Run only this suite with:
```bash
Scripts\runTests.cmd --gtest_filter=*TestIntegration*
```

## Static code analysis

- Use `Scripts\runClangtidy.cmd` to run static code analysis on the `App` and `Core` project files. Configuration and enabled checks are defined in `.clang-tidy` at the repository root.

## Known Limitations

obsto currently operates as an unauthenticated service over plaintext connections.

Planned future work includes:

- TLS / encrypted transport
- Authentication
- Access control
- Additional storage and deployment options

---

## License

Distributed under the MIT License. See `LICENSE` for more information.
