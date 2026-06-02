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

} // namespace minisql
