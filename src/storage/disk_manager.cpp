#include "storage/disk_manager.hpp"
#include "common/error.hpp"
#include <cstring>

namespace minisql {

DiskManager::DiskManager(const std::string &path) : path_(path) {
  // Try to open existing file or create it if absent.
  file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
  if (!file_.is_open()) {
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary |
                         std::ios::trunc);
  }
  if (!file_.is_open()) {
    throw StorageError("Cannot open database file: " + path);
  }

  // Determine existing page count.
  file_.seekg(0, std::ios::end);
  std::streamoff size = file_.tellg();
  if (size < 0)
    throw StorageError("Cannot seek in database file: " + path);
  num_pages_ = static_cast<uint32_t>(size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void DiskManager::read_page(PageId id, uint8_t *buf) {
  if (id >= num_pages_) {
    // Return a zero-filled page for new pages that haven't been written yet.
    std::memset(buf, 0, PAGE_SIZE);
    return;
  }
  file_.seekg(static_cast<std::streamoff>(id) * PAGE_SIZE, std::ios::beg);
  if (!file_)
    throw StorageError("Seek error reading page " + std::to_string(id));
  file_.read(reinterpret_cast<char *>(buf), PAGE_SIZE);
  if (!file_)
    throw StorageError("Read error on page " + std::to_string(id));
}

void DiskManager::write_page(PageId id, const uint8_t *buf) {
  file_.seekp(static_cast<std::streamoff>(id) * PAGE_SIZE, std::ios::beg);
  if (!file_)
    throw StorageError("Seek error writing page " + std::to_string(id));
  file_.write(reinterpret_cast<const char *>(buf), PAGE_SIZE);
  if (!file_)
    throw StorageError("Write error on page " + std::to_string(id));
  file_.flush();
  if (id >= num_pages_)
    num_pages_ = id + 1;
}

PageId DiskManager::allocate_page() {
  PageId new_id = num_pages_;
  // Write a zero page to extend the file.
  uint8_t zero[PAGE_SIZE]{};
  write_page(new_id, zero);
  return new_id;
}

} // namespace minisql
