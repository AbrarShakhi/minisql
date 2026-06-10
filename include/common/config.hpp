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

// ── Internal B+ tree node page layout ──────────────────────────────────────
// [type(1)][num_keys(4)][parent(4)] then interleaved children/keys:
// child[0] key[0] child[1] key[1] ... child[n-1] key[n-1] child[n]
inline constexpr size_t INTERNAL_HEADER = 9;
inline constexpr size_t INTERNAL_KEY_SZ = 8;   // uint64_t rowid
inline constexpr size_t INTERNAL_CHILD_SZ = 4; // uint32_t page_id
inline constexpr size_t INTERNAL_ENTRY_SZ =
    INTERNAL_KEY_SZ + INTERNAL_CHILD_SZ; // 12
// max keys n: 9 + 4 + n*12 <= PAGE_SIZE  →  n = (4096-9-4)/12 = 340
inline constexpr uint32_t INTERNAL_MAX_KEYS =
    (PAGE_SIZE - INTERNAL_HEADER - INTERNAL_CHILD_SZ) / INTERNAL_ENTRY_SZ;

// ── Leaf B+ tree node page layout ──────────────────────────────────────────
// [type(1)][num_cells(4)][parent(4)][next_leaf(4)][free_end(4)]
// then slot array (num_cells × 4) then free gap then cell heap (grows ←)
// cell: [key(8)][val_len(4)][val_bytes(val_len)]
inline constexpr size_t LEAF_HEADER = 17;
inline constexpr size_t LEAF_SLOT_SZ = 4;   // uint32_t offset
inline constexpr size_t LEAF_CELL_HDR = 12; // key(8) + val_len(4)

} // namespace minisql
