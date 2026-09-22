#pragma once

#include <cstddef>

inline constexpr size_t kStreamingThreshold = 1024 * 1024; // 1 MB
inline constexpr size_t kReadChunkSize = 64 * 1024; // 64 KB
