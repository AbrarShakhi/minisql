#pragma once
#include <fstream>
#include <string>

#include "storage/page.hpp"

namespace minisql {

// Handles raw byte I/O between memory buffers and the on-disk database file.
// Each page is PAGE_SIZE bytes; page N starts at byte offset N * PAGE_SIZE.
class DiskManager {
public:
  explicit DiskManager(const std::string &path);
  ~DiskManager();

  DiskManager(const DiskManager &) = delete;
  DiskManager &operator=(const DiskManager &) = delete;

  void read_page(PageId id, uint8_t *buf);
  void write_page(PageId id, const uint8_t *buf);
  PageId allocate_page();

  [[nodiscard]] uint32_t num_pages() const noexcept { return num_pages_; }
  [[nodiscard]] bool is_open() const noexcept { return file_.is_open(); }

private:
  std::string path_;
  std::fstream file_;
  uint32_t num_pages_{0};
};

} // namespace minisql
