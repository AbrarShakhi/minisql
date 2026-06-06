#pragma once
#include "btree/btree_node.hpp"
#include "storage/buffer_pool.hpp"
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace minisql {

class BTree {
public:
  BTree(BufferPool &bp, PageId root_page_id) noexcept;
  ~BTree() = default;

  BTree(const BTree &) = delete;
  BTree &operator=(const BTree &) = delete;

  [[nodiscard]] PageId root_page_id() const noexcept { return root_; }

  bool insert(uint64_t key, const std::vector<uint8_t> &value);

  [[nodiscard]] std::optional<std::vector<uint8_t>> search(uint64_t key) const;

  bool remove(uint64_t key);

  void
  range_scan(uint64_t lo, uint64_t hi,
             const std::function<void(uint64_t, const std::vector<uint8_t> &)>
                 &cb) const;

  void scan_all(
      const std::function<void(uint64_t, const std::vector<uint8_t> &)> &cb)
      const;

private:
  [[nodiscard]] Page *alloc_page(PageId &out_id);

  BufferPool &bp_;
  PageId root_;
};

} // namespace minisql
