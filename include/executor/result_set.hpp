#pragma once
#include "catalog/schema.hpp"
#include "executor/row.hpp"
#include <string>
#include <vector>

namespace minisql {

// Holds the output rows of a query together with column metadata.
class ResultSet {
public:
  ResultSet() = default;
  explicit ResultSet(std::vector<std::string> col_names);

  void add_row(Row row);

  [[nodiscard]] const std::vector<std::string> &col_names() const noexcept {
    return col_names_;
  }
  [[nodiscard]] const std::vector<Row> &rows() const noexcept { return rows_; }
  [[nodiscard]] size_t row_count() const noexcept { return rows_.size(); }
  [[nodiscard]] size_t col_count() const noexcept { return col_names_.size(); }
  [[nodiscard]] bool empty() const noexcept { return rows_.empty(); }

  // Pretty-print as an ASCII table to stdout.
  void print() const;

private:
  std::vector<std::string> col_names_;
  std::vector<Row> rows_;
};

} // namespace minisql
