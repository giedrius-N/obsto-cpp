FROM gcc:14-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake python3-pip \
    && python3 -m pip install --break-system-packages conan \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN conan profile detect --force \
    && conan install . \
        --output-folder=build \
        --build=missing \
        -s build_type=Release \
    && cmake -S . -B build \
        -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

FROM gcc:14-bookworm AS runtime

WORKDIR /app
COPY --from=builder /src/build/obsto ./obsto
COPY --from=builder /src/build/bin/libobsto_core.so ./libobsto_core.so

ENV LD_LIBRARY_PATH=/app

RUN mkdir /data

VOLUME ["/data"]
EXPOSE 8000

ENTRYPOINT ["/app/obsto"]
CMD ["--port", "8000", "--capacity", "200", "--path", "/data"]