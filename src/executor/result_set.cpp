#include "executor/result_set.hpp"
#include "common/types.hpp"
#include <algorithm>
#include <iostream>
#include <string>

namespace minisql {

ResultSet::ResultSet(std::vector<std::string> col_names)
    : col_names_(std::move(col_names)) {}

void ResultSet::add_row(Row row) { rows_.push_back(std::move(row)); }

void ResultSet::print() const {
  if (col_names_.empty())
    return;

  // Compute column widths.
  std::vector<size_t> widths(col_names_.size());
  for (size_t c = 0; c < col_names_.size(); ++c) {
    widths[c] = col_names_[c].size();
  }
  for (const auto &row : rows_) {
    for (size_t c = 0; c < col_names_.size(); ++c) {
      if (c < row.size()) {
        widths[c] = std::max(widths[c], to_string(row[c]).size());
      }
    }
  }

  // Helper: print a horizontal divider.
  auto divider = [&]() {
    std::cout << '+';
    for (size_t c = 0; c < widths.size(); ++c) {
      std::cout << std::string(widths[c] + 2, '-') << '+';
    }
    std::cout << '\n';
  };

  // Print header.
  divider();
  std::cout << '|';
  for (size_t c = 0; c < col_names_.size(); ++c) {
    std::cout << ' ' << col_names_[c]
              << std::string(widths[c] - col_names_[c].size(), ' ') << " |";
  }
  std::cout << '\n';
  divider();

  // Print rows.
  for (const auto &row : rows_) {
    std::cout << '|';
    for (size_t c = 0; c < col_names_.size(); ++c) {
      std::string cell = (c < row.size()) ? to_string(row[c]) : "";
      std::cout << ' ' << cell << std::string(widths[c] - cell.size(), ' ')
                << " |";
    }
    std::cout << '\n';
  }
  divider();
  std::cout << rows_.size() << " row(s)\n";
}

} // namespace minisql
