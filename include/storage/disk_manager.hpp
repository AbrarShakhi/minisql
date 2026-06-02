#pragma once
#include <cstdint>
#include <fstream>
#include <string>

namespace minisql {

// Handles raw byte I/O between memory buffers and the on-disk database file.
// Each page is PAGE_SIZE bytes; page N starts at byte offset N * PAGE_SIZE.
class DiskManager {
public:
  explicit DiskManager(const std::string &path);
  ~DiskManager();

  DiskManager(const DiskManager &) = delete;
  DiskManager &operator=(const DiskManager &) = delete;

  void read_page(uint32_t id, uint8_t *buf);
  void write_page(uint32_t id, const uint8_t *buf);
  uint32_t allocate_page();

  [[nodiscard]] uint32_t num_pages() const noexcept { return num_pages_; }
  [[nodiscard]] bool is_open() const noexcept { return file_.is_open(); }

private:
  std::string path_;
  std::fstream file_;
  uint32_t num_pages_{0};
};

} // namespace minisql
