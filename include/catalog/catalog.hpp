#pragma once
#include "catalog/schema.hpp"
#include "storage/page.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace minisql {

struct TableMeta {
  Schema schema;
  PageId root_page_id{INVALID_PAGE_ID};
  uint64_t next_row_id{1};
};

// In-memory catalog loaded from / persisted to a sidecar `.cat` file.
// Owns no B+ tree pages; those are managed by the BufferPool.
class Catalog {
public:
  explicit Catalog(const std::string &cat_path);
  ~Catalog();

  Catalog(const Catalog &) = delete;
  Catalog &operator=(const Catalog &) = delete;

  // Returns false if a table with that name already exists.
  bool create_table(const std::string &name, const Schema &schema,
                    PageId root_page_id);

  // Returns false if the table does not exist.
  bool drop_table(const std::string &name);

  [[nodiscard]] bool table_exists(const std::string &name) const noexcept;
  [[nodiscard]] TableMeta &get_table(const std::string &name);
  [[nodiscard]] const TableMeta &get_table(const std::string &name) const;
  [[nodiscard]] std::vector<std::string> table_names() const;

  uint64_t next_row_id(const std::string &table);

  // Write all metadata to disk.
  void flush() const;

private:
  void load();
  void save() const;

  std::string cat_path_;
  std::unordered_map<std::string, TableMeta> tables_;
};

} // namespace minisql
