#include "common/types.hpp"
#include "common/error.hpp"
#include <algorithm>
#include <cmath>

namespace minisql {

bool is_null(const Value &v) noexcept {
  return std::holds_alternative<NullVal>(v);
}

DataType type_of(const Value &v) noexcept {
  return std::visit(
      [](auto &&arg) -> DataType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NullVal>)
          return DataType::NULLVAL;
        if constexpr (std::is_same_v<T, Integer>)
          return DataType::INTEGER;
        if constexpr (std::is_same_v<T, Real>)
          return DataType::REAL;
        if constexpr (std::is_same_v<T, Text>)
          return DataType::TEXT;
        if constexpr (std::is_same_v<T, Blob>)
          return DataType::BLOB;
        return DataType::NULLVAL;
      },
      v);
}

std::string to_string(DataType dt) {
  switch (dt) {
  case DataType::INTEGER:
    return "INTEGER";
  case DataType::REAL:
    return "REAL";
  case DataType::TEXT:
    return "TEXT";
  case DataType::BLOB:
    return "BLOB";
  case DataType::NULLVAL:
    return "NULL";
  }
  return "UNKNOWN";
}

std::string to_string(const Value &v) {
  return std::visit(
      [](auto &&arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NullVal>)
          return "NULL";
        if constexpr (std::is_same_v<T, Integer>)
          return std::to_string(arg);
        if constexpr (std::is_same_v<T, Real>)
          return std::to_string(arg);
        if constexpr (std::is_same_v<T, Text>)
          return arg;
        if constexpr (std::is_same_v<T, Blob>) {
          std::string s = "X'";
          constexpr char hex[] = "0123456789ABCDEF";
          for (uint8_t b : arg) {
            s += hex[b >> 4];
            s += hex[b & 0xF];
          }
          s += '\'';
          return s;
        }
        return "";
      },
      v);
}

DataType dt_from_string(const std::string &s) {
  std::string up = s;
  std::transform(up.begin(), up.end(), up.begin(), ::toupper);
  if (up == "INTEGER" || up == "INT" || up == "BIGINT")
    return DataType::INTEGER;
  if (up == "REAL" || up == "FLOAT" || up == "DOUBLE")
    return DataType::REAL;
  if (up == "TEXT" || up == "VARCHAR" || up == "CHAR")
    return DataType::TEXT;
  if (up == "BLOB")
    return DataType::BLOB;
  throw SchemaError("Unknown data type: " + s);
}

int compare_values(const Value &a, const Value &b) {
  if (is_null(a) && is_null(b))
    return 0;
  if (is_null(a))
    return -1;
  if (is_null(b))
    return 1;

  // Numeric coercion: INTEGER ↔ REAL
  if (std::holds_alternative<Integer>(a) && std::holds_alternative<Real>(b)) {
    double da = static_cast<double>(std::get<Integer>(a));
    double db = std::get<Real>(b);
    return (da < db) ? -1 : (da > db) ? 1 : 0;
  }
  if (std::holds_alternative<Real>(a) && std::holds_alternative<Integer>(b)) {
    double da = std::get<Real>(a);
    double db = static_cast<double>(std::get<Integer>(b));
    return (da < db) ? -1 : (da > db) ? 1 : 0;
  }

  return std::visit(
      [&b](auto &&av) -> int {
        using TA = std::decay_t<decltype(av)>;
        if (!std::holds_alternative<TA>(b))
          throw ExecutionError("Cannot compare values of different types");
        const auto &bv = std::get<TA>(b);
        if constexpr (std::is_same_v<TA, NullVal>)
          return 0;
        else if (av < bv)
          return -1;
        else if (av > bv)
          return 1;
        else
          return 0;
      },
      a);
}

} // namespace minisql
