#pragma once
#include "storage/disk_manager.hpp"
#include "storage/page.hpp"
#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

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

  // Fetch a page from disk (or cache). Increments pin count.
  // Returns nullptr if pool is exhausted (all frames pinned).
  [[nodiscard]] Page *fetch_page(PageId id);

  // Allocate a brand-new page on disk. Increments pin count.
  // Returns nullptr if pool is exhausted.
  [[nodiscard]] Page *new_page(PageId &out_id);

  // Decrement pin count. If dirty == true, marks the page dirty.
  void unpin_page(PageId id, bool dirty);

  // Write a single dirty page to disk immediately.
  void flush_page(PageId id);

  // Write all dirty pages to disk.
  void flush_all();

private:
  FrameId evict(); // returns INVALID_FRAME_ID (UINT32_MAX) if all pinned
  void touch(FrameId fid); // move frame to MRU end of LRU list

  DiskManager &dm_;
  size_t capacity_;

  std::vector<std::unique_ptr<Page>> frames_;
  std::unordered_map<PageId, FrameId> page_table_;
  std::list<FrameId> lru_list_;
  std::unordered_map<FrameId, std::list<FrameId>::iterator> lru_pos_;
  std::list<FrameId> free_list_;
};

/**
  TODO: Right now lru_list_ is using LinkedList.\
  Accessing the LRU list might be slower.\
  So, Also need a Queue like data structure to pop oldest and app recent.
  Needs continues Circuler Queue datastrusture.
*/

} // namespace minisql
