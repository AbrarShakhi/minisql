#pragma once
#include "storage/disk_manager.hpp"

namespace minisql {

// LRU buffer pool manager. Maintains a fixed set of frames in RAM.
// Callers pin a page before use and unpin it when done.
// Dirty pages are flushed to disk on eviction or explicit flush.
class BufferPool {
public:
  explicit BufferPool(DiskManager &dm, size_t capacity = BUFFER_POOL_SIZE);
  ~BufferPool();

  BufferPool(const BufferPool &) = delete;
  BufferPool &operator=(const BufferPool &) = delete;
};

} // namespace minisql
