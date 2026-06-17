#include "catalog/schema.hpp"
#include "common/error.hpp"
#include <cstring>

namespace minisql {

Schema::Schema(std::vector<Column> cols) : cols_(std::move(cols)) {}

void Schema::add_column(Column col) { cols_.push_back(std::move(col)); }

std::optional<size_t> Schema::index_of(const std::string &name) const noexcept {
  for (size_t i = 0; i < cols_.size(); ++i) {
    if (cols_[i].name == name)
      return i;
  }
  return std::nullopt;
}

// Binary serialisation format:
//   col_count(2) then for each column:
//     name_len(1) name(name_len) type(1) nullable(1)
std::vector<uint8_t> Schema::serialise() const {
  std::vector<uint8_t> buf;
  auto emit8 = [&](uint8_t v) { buf.push_back(v); };
  auto emit16 = [&](uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  };
  emit16(static_cast<uint16_t>(cols_.size()));
  for (const auto &c : cols_) {
    auto name_len = static_cast<uint8_t>(c.name.size());
    emit8(name_len);
    for (char ch : c.name)
      emit8(static_cast<uint8_t>(ch));
    emit8(static_cast<uint8_t>(c.type));
    emit8(c.nullable ? 1 : 0);
  }
  return buf;
}

Schema Schema::deserialise(const uint8_t *buf, size_t len) {
  if (len < 2)
    throw SchemaError("Schema buffer too small");
  size_t pos = 0;
  auto read8 = [&]() -> uint8_t {
    if (pos >= len)
      throw SchemaError("Schema buffer overrun");
    return buf[pos++];
  };
  auto read16 = [&]() -> uint16_t {
    uint16_t lo = read8();
    uint16_t hi = read8();
    return static_cast<uint16_t>(lo | (hi << 8));
  };

  uint16_t col_count = read16();
  std::vector<Column> cols;
  cols.reserve(col_count);
  for (uint16_t i = 0; i < col_count; ++i) {
    uint8_t name_len = read8();
    std::string name(name_len, '\0');
    for (uint8_t j = 0; j < name_len; ++j) {
      name[j] = static_cast<char>(read8());
    }
    DataType type = static_cast<DataType>(read8());
    bool nullable = (read8() != 0);
    cols.emplace_back(std::move(name), type, nullable);
  }
  return Schema(std::move(cols));
}

} // namespace minisql
