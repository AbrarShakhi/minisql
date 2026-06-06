#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace minisql {

enum class DataType : uint8_t {
  INTEGER = 0,
  REAL = 1,
  TEXT = 2,
  BLOB = 3,
  NULLVAL = 4,
};

using NullVal = std::monostate;
using Integer = int64_t;
using Real = double;
using Text = std::string;
using Blob = std::vector<uint8_t>;

using Value = std::variant<NullVal, Integer, Real, Text, Blob>;

[[nodiscard]] bool is_null(const Value &v) noexcept;
[[nodiscard]] DataType type_of(const Value &v) noexcept;
[[nodiscard]] std::string to_string(DataType dt);
[[nodiscard]] std::string to_string(const Value &v);
[[nodiscard]] DataType dt_from_string(const std::string &s);

// Returns negative / 0 / positive. Throws if types are incomparable.
[[nodiscard]] int compare_values(const Value &a, const Value &b);

} // namespace minisql
