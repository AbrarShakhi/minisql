#pragma once
#include <cstdint>
#include <vector>

#include "common/config.hpp"
#include "storage/page.hpp"

namespace minisql {

enum class NodeType : uint8_t { INTERNAL = 0, LEAF = 1 };

// Wraps a raw Page and provides typed accessors for B+ tree node fields.
// Does NOT own the page; the BufferPool owns it.
//
// Internal node page layout (offset in bytes):
//   [0]       uint8_t  node_type
//   [1-4]     uint32_t num_keys
//   [5-8]     uint32_t parent_page_id
//   Interleaved child/key pairs starting at offset 9:
//   child[0](4) key[0](8) child[1](4) key[1](8) … key[n-1](8) child[n](4)
//
// Leaf node page layout:
//   [0]       uint8_t  node_type
//   [1-4]     uint32_t num_cells
//   [5-8]     uint32_t parent_page_id
//   [9-12]    uint32_t next_leaf_page_id
//   [13-16]   uint32_t free_end  (heap grows ← from free_end toward slot array)
//   [17+]     slot array: num_cells × uint32_t (page offset of each cell)
//             … free space …
//             cell heap: [key(8)][val_len(4)][val_bytes(val_len)]
class BTreeNode {
public:
  explicit BTreeNode(Page *page) noexcept : page_(page) {}

  // ── Common ──────────────────────────────────────────────────────────────
  [[nodiscard]] NodeType node_type() const noexcept;
  [[nodiscard]] uint32_t num_keys() const noexcept;
  [[nodiscard]] PageId parent_id() const noexcept;
  [[nodiscard]] bool is_leaf() const noexcept;
  [[nodiscard]] Page *raw_page() const noexcept { return page_; }
  [[nodiscard]] PageId page_id() const noexcept { return page_->page_id(); }

  void set_node_type(NodeType t) noexcept;
  void set_num_keys(uint32_t n) noexcept;
  void set_parent_id(PageId pid) noexcept;

  // ── Internal node ────────────────────────────────────────────────────────
  [[nodiscard]] uint64_t internal_key(uint32_t i) const noexcept;
  [[nodiscard]] PageId internal_child(uint32_t i) const noexcept;

  void set_internal_key(uint32_t i, uint64_t key) noexcept;
  void set_internal_child(uint32_t i, PageId pid) noexcept;

  // Insert (key, right_child) at logical position i, shifting right.
  void internal_insert(uint32_t i, uint64_t key, PageId right_child) noexcept;

  [[nodiscard]] bool internal_is_full() const noexcept;

  // Initialise a fresh internal node.
  void init_internal(PageId parent = INVALID_PAGE_ID) noexcept;

  // ── Leaf node ────────────────────────────────────────────────────────────
  [[nodiscard]] PageId next_leaf_id() const noexcept;
  [[nodiscard]] uint64_t leaf_key(uint32_t i) const noexcept;
  [[nodiscard]] std::vector<uint8_t> leaf_value(uint32_t i) const;

  void set_next_leaf_id(PageId pid) noexcept;

  // Returns false if there is not enough free space.
  bool leaf_insert(uint64_t key, const std::vector<uint8_t> &val);

  // Remove cell at logical index i (does not compact the heap).
  void leaf_delete(uint32_t i);

  // True if the page can accommodate a cell with val_len bytes.
  [[nodiscard]] bool leaf_has_space(size_t val_len) const noexcept;

  // Find sorted index for key (binary search). Returns num_keys if key > all.
  [[nodiscard]] uint32_t leaf_lower_bound(uint64_t key) const noexcept;

  // Initialise a fresh leaf node.
  void init_leaf(PageId parent = INVALID_PAGE_ID,
                 PageId next = INVALID_PAGE_ID) noexcept;

private:
  // Internal layout helpers ─────────────────────────────────────────────
  static constexpr size_t INT_OFF_CHILD0 =
      INTERNAL_HEADER; // offset of child[0]
  [[nodiscard]] uint8_t *int_child_ptr(uint32_t i) noexcept;
  [[nodiscard]] uint8_t *int_key_ptr(uint32_t i) noexcept;

  // Leaf layout helpers ─────────────────────────────────────────────────
  static constexpr size_t LEAF_OFF_NEXT = 9;
  static constexpr size_t LEAF_OFF_FREE_END = 13;
  static constexpr size_t LEAF_OFF_SLOTS = LEAF_HEADER; // 17

  [[nodiscard]] uint32_t leaf_slot(uint32_t i) const noexcept;
  void set_leaf_slot(uint32_t i, uint32_t off) noexcept;
  [[nodiscard]] uint32_t leaf_free_end() const noexcept;
  void set_leaf_free_end(uint32_t end) noexcept;

  Page *page_;
};

} // namespace minisql
