#include "executor/row.hpp"
#include "common/error.hpp"
#include <cstring>

namespace minisql {

// Serialisation format per value:
//   [type_tag: 1 byte]
//   If INTEGER: [int64_t: 8 bytes]
//   If REAL:    [double:  8 bytes]
//   If TEXT:    [len: 4 bytes][chars: len bytes]
//   If BLOB:    [len: 4 bytes][bytes: len bytes]
//   If NULL:    nothing extra

std::vector<uint8_t> serialise_row(const Row &row) {
  std::vector<uint8_t> buf;
  auto emit = [&](const void *p, size_t n) {
    const uint8_t *b = static_cast<const uint8_t *>(p);
    buf.insert(buf.end(), b, b + n);
  };
  auto emit_u32 = [&](uint32_t v) { emit(&v, 4); };

  for (const auto &val : row) {
    uint8_t tag = static_cast<uint8_t>(type_of(val));
    buf.push_back(tag);
    std::visit(
        [&](auto &&v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, Integer>) {
            emit(&v, 8);
          } else if constexpr (std::is_same_v<T, Real>) {
            emit(&v, 8);
          } else if constexpr (std::is_same_v<T, Text>) {
            uint32_t len = static_cast<uint32_t>(v.size());
            emit_u32(len);
            emit(v.data(), len);
          } else if constexpr (std::is_same_v<T, Blob>) {
            uint32_t len = static_cast<uint32_t>(v.size());
            emit_u32(len);
            emit(v.data(), len);
          }
          // NullVal: nothing extra
        },
        val);
  }
  return buf;
}

Row deserialise_row(const uint8_t *buf, size_t len, size_t col_count) {
  Row row;
  row.reserve(col_count);
  size_t pos = 0;

  auto read_u32 = [&]() -> uint32_t {
    if (pos + 4 > len)
      throw ExecutionError("Row buffer overrun");
    uint32_t v{};
    std::memcpy(&v, buf + pos, 4);
    pos += 4;
    return v;
  };

  for (size_t i = 0; i < col_count; ++i) {
    if (pos >= len)
      throw ExecutionError("Row buffer too short");
    DataType tag = static_cast<DataType>(buf[pos++]);
    switch (tag) {
    case DataType::INTEGER: {
      if (pos + 8 > len)
        throw ExecutionError("Row buffer overrun (INTEGER)");
      Integer v{};
      std::memcpy(&v, buf + pos, 8);
      pos += 8;
      row.emplace_back(v);
      break;
    }
    case DataType::REAL: {
      if (pos + 8 > len)
        throw ExecutionError("Row buffer overrun (REAL)");
      Real v{};
      std::memcpy(&v, buf + pos, 8);
      pos += 8;
      row.emplace_back(v);
      break;
    }
    case DataType::TEXT: {
      uint32_t sz = read_u32();
      if (pos + sz > len)
        throw ExecutionError("Row buffer overrun (TEXT)");
      Text s(reinterpret_cast<const char *>(buf + pos), sz);
      pos += sz;
      row.emplace_back(std::move(s));
      break;
    }
    case DataType::BLOB: {
      uint32_t sz = read_u32();
      if (pos + sz > len)
        throw ExecutionError("Row buffer overrun (BLOB)");
      Blob b(buf + pos, buf + pos + sz);
      pos += sz;
      row.emplace_back(std::move(b));
      break;
    }
    case DataType::NULLVAL:
      row.emplace_back(NullVal{});
      break;
    default:
      throw ExecutionError("Unknown type tag in row buffer");
    }
  }
  return row;
}

Row make_null_row(size_t col_count) { return Row(col_count, NullVal{}); }

} // namespace minisql
