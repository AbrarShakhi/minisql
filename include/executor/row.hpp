#pragma once
#include "catalog/schema.hpp"
#include "common/types.hpp"
#include <cstdint>
#include <vector>

namespace minisql {

// A single data row: ordered values aligned to a Schema.
using Row = std::vector<Value>;

// Serialise a Row to a byte buffer suitable for storing in a B+ tree leaf cell.
[[nodiscard]] std::vector<uint8_t> serialise_row(const Row &row);

// Deserialise a byte buffer back into a Row (must know column count).
[[nodiscard]] Row deserialise_row(const uint8_t *buf, size_t len,
                                  size_t col_count);

// Build a Row where all values are NullVal.
[[nodiscard]] Row make_null_row(size_t col_count);

} // namespace minisql
