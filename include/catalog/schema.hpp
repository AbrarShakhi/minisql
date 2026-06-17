#pragma once
#include "catalog/column.hpp"
#include <optional>
#include <string>
#include <vector>

namespace minisql {

// Describes the structure of a single table (ordered set of columns).
class Schema {
public:
  Schema() = default;
  explicit Schema(std::vector<Column> cols);

  void add_column(Column col);

  [[nodiscard]] const std::vector<Column> &columns() const noexcept {
    return cols_;
  }
  [[nodiscard]] size_t col_count() const noexcept { return cols_.size(); }
  [[nodiscard]] const Column &column(size_t i) const { return cols_.at(i); }

  // Returns the zero-based index of column name, or nullopt.
  [[nodiscard]] std::optional<size_t>
  index_of(const std::string &name) const noexcept;

  // Serialise / deserialise to/from a byte buffer (used in catalog file).
  [[nodiscard]] std::vector<uint8_t> serialise() const;
  static Schema deserialise(const uint8_t *buf, size_t len);

private:
  std::vector<Column> cols_;
};

} // namespace minisql
