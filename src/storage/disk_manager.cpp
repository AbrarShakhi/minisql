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
}

DiskManager::~DiskManager() {
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void DiskManager::read_page(uint32_t id, uint8_t *buf) {}

void DiskManager::write_page(uint32_t id, const uint8_t *buf) {}

uint32_t DiskManager::allocate_page() {}

} // namespace minisql
