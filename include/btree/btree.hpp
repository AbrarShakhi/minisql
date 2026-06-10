#pragma once
#include <functional>
#include <optional>
#include <vector>

#include "storage/buffer_pool.hpp"

namespace minisql {

// Disk-resident B+ tree keyed on uint64_t rowids.
// All data records live in leaves; leaves are linked for sequential scans.
// Nodes are stored as pages managed by the BufferPool.
class BTree {
public:
  // Pass INVALID_PAGE_ID for root_page_id when the tree is brand new.
  BTree(BufferPool &bp, PageId root_page_id) noexcept;
  ~BTree() = default;

  BTree(const BTree &) = delete;
  BTree &operator=(const BTree &) = delete;

  [[nodiscard]] PageId root_page_id() const noexcept { return root_; }

  // Returns false if the key already exists.
  bool insert(uint64_t key, const std::vector<uint8_t> &value);

  // Returns nullopt if the key is not found.
  [[nodiscard]] std::optional<std::vector<uint8_t>> search(uint64_t key) const;

  // Returns false if the key is not found.
  bool remove(uint64_t key);

  // Visit every (key, value) pair with lo <= key <= hi in ascending key order.
  void
  range_scan(uint64_t lo, uint64_t hi,
             const std::function<void(uint64_t, const std::vector<uint8_t> &)>
                 &cb) const;

  // Visit every (key, value) pair in ascending key order.
  void scan_all(
      const std::function<void(uint64_t, const std::vector<uint8_t> &)> &cb)
      const;

private:
  // Returns a pinned pointer to the leaf that should contain key.
  [[nodiscard]] Page *find_leaf(uint64_t key) const;

  // Collect and sort all cells from a leaf into a vector, reinitialise leaf,
  // reinsert half. Fills push_up_key and new_leaf_id.
  void split_leaf(Page *leaf, uint64_t &push_up_key, PageId &new_leaf_id);
  void split_internal(Page *node, uint64_t &push_up_key, PageId &new_node_id);

  void insert_into_parent(Page *left, uint64_t key, Page *right);
  void insert_into_new_root(Page *left, uint64_t key, Page *right);

  void rebalance_after_delete(Page *leaf);
  void merge_leaves(Page *left, Page *right, Page *parent, uint32_t sep_idx);
  void merge_internals(Page *left, Page *right, Page *parent, uint32_t sep_idx);
  void steal_left_leaf(Page *node, Page *sib, Page *parent, uint32_t sep_idx);
  void steal_right_leaf(Page *node, Page *sib, Page *parent, uint32_t sep_idx);
  void remove_key_from_internal(Page *node, uint32_t key_idx);

  // Returns pinned sibling page and sets sep_idx to the parent separator index.
  // Returns nullptr when no such sibling exists.
  [[nodiscard]] Page *left_sibling(const Page *node, uint32_t &sep_idx) const;
  [[nodiscard]] Page *right_sibling(const Page *node, uint32_t &sep_idx) const;

  // Allocate a new page via the buffer pool. Caller must unpin it.
  [[nodiscard]] Page *alloc_page(PageId &out_id);

  // Returns the leftmost leaf page (pinned). Returns nullptr if tree is empty.
  [[nodiscard]] Page *leftmost_leaf() const;

  BufferPool &bp_;
  PageId root_;
};

} // namespace minisql
