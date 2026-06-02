#pragma once
#include "common/config.hpp"
#include <array>
#include <cstdint>

namespace minisql {

using PageId = uint32_t;
using FrameId = uint32_t;

class Page {
public:
  Page() noexcept = default;
  ~Page() = default;

  Page(const Page &) = delete;
  Page &operator=(const Page &) = delete;
  Page(Page &&) noexcept = default;
  Page &operator=(Page &&) noexcept = default;

  [[nodiscard]] PageId page_id() const noexcept { return page_id_; }
  [[nodiscard]] bool is_dirty() const noexcept { return dirty_; }
  [[nodiscard]] int pin_count() const noexcept { return pins_; }

  [[nodiscard]] uint8_t *data() noexcept { return data_.data(); }
  [[nodiscard]] const uint8_t *data() const noexcept { return data_.data(); }

  void set_page_id(PageId id) noexcept { page_id_ = id; }
  void mark_dirty() noexcept { dirty_ = true; }
  void clear_dirty() noexcept { dirty_ = false; }
  void pin() noexcept { ++pins_; }
  void unpin() noexcept {
    if (pins_ > 0)
      --pins_;
  }
  void reset() noexcept;

private:
  PageId page_id_{INVALID_PAGE_ID};
  bool dirty_{false};
  int pins_{0};
  std::array<uint8_t, PAGE_SIZE> data_{};
};

} // namespace minisql
