#pragma once
#include <cstddef>
#include <cstdint>

namespace minisql {

inline constexpr uint32_t PAGE_SIZE = 4096;
inline constexpr uint32_t INVALID_PAGE_ID = UINT32_MAX;
inline constexpr size_t BUFFER_POOL_SIZE = 128;
inline constexpr uint32_t MAX_NAME_LEN = 64;
inline constexpr uint32_t MAX_VARCHAR_LEN = 255;
inline constexpr uint32_t MAGIC_NUMBER = 0x4D494E49U; // "MINI"
inline constexpr uint32_t DB_VERSION = 1U;
inline constexpr uint32_t CATALOG_PAGE_ID = 0U;

} // namespace minisql
