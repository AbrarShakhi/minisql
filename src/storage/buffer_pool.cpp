#include "storage/buffer_pool.hpp"
#include <limits>

namespace minisql {

static constexpr FrameId INVALID_FRAME = std::numeric_limits<FrameId>::max();

BufferPool::BufferPool(DiskManager &dm, size_t capacity)
    : dm_(dm), capacity_(capacity) {
  frames_.reserve(capacity);
  for (size_t i = 0; i < capacity; ++i) {
    frames_.emplace_back(std::make_unique<Page>());
    free_list_.push_back(static_cast<FrameId>(i));
  }
}

BufferPool::~BufferPool() { flush_all(); }

Page *BufferPool::fetch_page(PageId id) {
  // Already in cache?
  auto it = page_table_.find(id);
  if (it != page_table_.end()) {
    FrameId fid = it->second;
    touch(fid);
    frames_[fid]->pin();
    return frames_[fid].get();
  }

  // Need a free or evictable frame.
  FrameId fid = INVALID_FRAME;
  if (!free_list_.empty()) {
    fid = free_list_.front();
    free_list_.pop_front();
  } else {
    fid = evict();
    if (fid == INVALID_FRAME)
      return nullptr; // all pinned
  }

  // Load from disk.
  Page *page = frames_[fid].get();
  page->reset();
  page->set_page_id(id);
  dm_.read_page(id, page->data());
  page_table_[id] = fid;
  lru_list_.push_back(fid);
  lru_pos_[fid] = std::prev(lru_list_.end());
  page->pin();
  return page;
}

Page *BufferPool::new_page(PageId &out_id) {
  FrameId fid = INVALID_FRAME;
  if (!free_list_.empty()) {
    fid = free_list_.front();
    free_list_.pop_front();
  } else {
    fid = evict();
    if (fid == INVALID_FRAME)
      return nullptr;
  }

  out_id = dm_.allocate_page();
  Page *page = frames_[fid].get();
  page->reset();
  page->set_page_id(out_id);
  page_table_[out_id] = fid;
  lru_list_.push_back(fid);
  lru_pos_[fid] = std::prev(lru_list_.end());
  page->pin();
  return page;
}

void BufferPool::unpin_page(PageId id, bool dirty) {
  auto it = page_table_.find(id);
  if (it == page_table_.end())
    return;
  Page *page = frames_[it->second].get();
  if (dirty)
    page->mark_dirty();
  page->unpin();
}

void BufferPool::flush_page(PageId id) {
  auto it = page_table_.find(id);
  if (it == page_table_.end())
    return;
  Page *page = frames_[it->second].get();
  if (page->is_dirty()) {
    dm_.write_page(id, page->data());
    page->clear_dirty();
  }
}

void BufferPool::flush_all() {
  for (auto &[id, fid] : page_table_) {
    Page *page = frames_[fid].get();
    if (page->is_dirty()) {
      dm_.write_page(id, page->data());
      page->clear_dirty();
    }
  }
}

// ── Private ──────────────────────────────────────────────────────────────────

FrameId BufferPool::evict() {
  for (auto it = lru_list_.begin(); it != lru_list_.end(); ++it) {
    FrameId fid = *it;
    Page *page = frames_[fid].get();
    if (page->pin_count() > 0)
      continue; // pinned, skip

    // Flush if dirty.
    if (page->is_dirty()) {
      dm_.write_page(page->page_id(), page->data());
      page->clear_dirty();
    }
    page_table_.erase(page->page_id());
    lru_pos_.erase(fid);
    lru_list_.erase(it);
    return fid;
  }
  return INVALID_FRAME; // all frames pinned
}

void BufferPool::touch(FrameId fid) {
  auto it = lru_pos_.find(fid);
  if (it != lru_pos_.end()) {
    lru_list_.erase(it->second);
  }
  lru_list_.push_back(fid);
  lru_pos_[fid] = std::prev(lru_list_.end());
}

} // namespace minisql
