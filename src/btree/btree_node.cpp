#include "btree/btree_node.hpp"

#include <cassert>
#include <cstring>

namespace minisql {

// ── Helpers: little-endian read/write ────────────────────────────────────────

namespace {

template <typename T> T read_le(const uint8_t *p) noexcept {
  T val{};
  std::memcpy(&val, p, sizeof(T));
  return val;
}

template <typename T> void write_le(uint8_t *p, T val) noexcept {
  std::memcpy(p, &val, sizeof(T));
}

} // namespace

// ── Common
// ────────────────────────────────────────────────────────────────────

NodeType BTreeNode::node_type() const noexcept {
  return static_cast<NodeType>(page_->data()[0]);
}
uint32_t BTreeNode::num_keys() const noexcept {
  return read_le<uint32_t>(page_->data() + 1);
}
PageId BTreeNode::parent_id() const noexcept {
  return read_le<uint32_t>(page_->data() + 5);
}
bool BTreeNode::is_leaf() const noexcept {
  return node_type() == NodeType::LEAF;
}

void BTreeNode::set_node_type(NodeType t) noexcept {
  page_->data()[0] = static_cast<uint8_t>(t);
  page_->mark_dirty();
}
void BTreeNode::set_num_keys(uint32_t n) noexcept {
  write_le<uint32_t>(page_->data() + 1, n);
  page_->mark_dirty();
}
void BTreeNode::set_parent_id(PageId pid) noexcept {
  write_le<uint32_t>(page_->data() + 5, pid);
  page_->mark_dirty();
}

// ── Internal node
// ─────────────────────────────────────────────────────────────

// Layout at offset INT_OFF_CHILD0 = 9:
//   child[0](4) key[0](8) child[1](4) key[1](8) ... child[n-1](4) key[n-1](8)
//   child[n](4) child[i] at 9 + i*12 key[i]   at 9 + i*12 + 4

uint8_t *BTreeNode::int_child_ptr(uint32_t i) noexcept {
  return page_->data() + INT_OFF_CHILD0 + static_cast<size_t>(i) * 12;
}
uint8_t *BTreeNode::int_key_ptr(uint32_t i) noexcept {
  return page_->data() + INT_OFF_CHILD0 + static_cast<size_t>(i) * 12 + 4;
}

uint64_t BTreeNode::internal_key(uint32_t i) const noexcept {
  uint8_t *p = const_cast<BTreeNode *>(this)->int_key_ptr(i);
  return read_le<uint64_t>(p);
}
PageId BTreeNode::internal_child(uint32_t i) const noexcept {
  uint8_t *p = const_cast<BTreeNode *>(this)->int_child_ptr(i);
  return read_le<uint32_t>(p);
}

void BTreeNode::set_internal_key(uint32_t i, uint64_t key) noexcept {
  write_le<uint64_t>(int_key_ptr(i), key);
  page_->mark_dirty();
}
void BTreeNode::set_internal_child(uint32_t i, PageId pid) noexcept {
  write_le<uint32_t>(int_child_ptr(i), pid);
  page_->mark_dirty();
}

void BTreeNode::internal_insert(uint32_t i, uint64_t key,
                                PageId right_child) noexcept {
  uint32_t n = num_keys();
  // Shift existing keys and children right from position i.
  for (uint32_t j = n; j > i; --j) {
    set_internal_key(j, internal_key(j - 1));
    set_internal_child(j + 1, internal_child(j));
  }
  set_internal_key(i, key);
  set_internal_child(i + 1, right_child);
  set_num_keys(n + 1);
}

bool BTreeNode::internal_is_full() const noexcept {
  return num_keys() >= INTERNAL_MAX_KEYS;
}

void BTreeNode::init_internal(PageId parent) noexcept {
  std::memset(page_->data(), 0, PAGE_SIZE);
  set_node_type(NodeType::INTERNAL);
  set_num_keys(0);
  set_parent_id(parent);
  page_->mark_dirty();
}

// ── Leaf node
// ─────────────────────────────────────────────────────────────────

PageId BTreeNode::next_leaf_id() const noexcept {
  return read_le<uint32_t>(page_->data() + LEAF_OFF_NEXT);
}
void BTreeNode::set_next_leaf_id(PageId pid) noexcept {
  write_le<uint32_t>(page_->data() + LEAF_OFF_NEXT, pid);
  page_->mark_dirty();
}

uint32_t BTreeNode::leaf_free_end() const noexcept {
  return read_le<uint32_t>(page_->data() + LEAF_OFF_FREE_END);
}
void BTreeNode::set_leaf_free_end(uint32_t end) noexcept {
  write_le<uint32_t>(page_->data() + LEAF_OFF_FREE_END, end);
}

uint32_t BTreeNode::leaf_slot(uint32_t i) const noexcept {
  return read_le<uint32_t>(page_->data() + LEAF_OFF_SLOTS +
                           static_cast<size_t>(i) * LEAF_SLOT_SZ);
}
void BTreeNode::set_leaf_slot(uint32_t i, uint32_t offset) noexcept {
  write_le<uint32_t>(page_->data() + LEAF_OFF_SLOTS +
                         static_cast<size_t>(i) * LEAF_SLOT_SZ,
                     offset);
}

uint64_t BTreeNode::leaf_key(uint32_t i) const noexcept {
  uint32_t off = leaf_slot(i);
  return read_le<uint64_t>(page_->data() + off);
}
std::vector<uint8_t> BTreeNode::leaf_value(uint32_t i) const {
  uint32_t off = leaf_slot(i);
  uint32_t val_len = read_le<uint32_t>(page_->data() + off + 8);
  const uint8_t *start = page_->data() + off + 12;
  return {start, start + val_len};
}

bool BTreeNode::leaf_has_space(size_t val_len) const noexcept {
  uint32_t n = num_keys();
  size_t slot_end = LEAF_OFF_SLOTS + (n + 1) * LEAF_SLOT_SZ;
  size_t cell_sz = LEAF_CELL_HDR + val_len;
  return (slot_end + cell_sz) <= leaf_free_end();
}

uint32_t BTreeNode::leaf_lower_bound(uint64_t key) const noexcept {
  uint32_t lo = 0, hi = num_keys();
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (leaf_key(mid) < key)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

bool BTreeNode::leaf_insert(uint64_t key, const std::vector<uint8_t> &val) {
  if (!leaf_has_space(val.size()))
    return false;

  uint32_t n = num_keys();
  uint32_t pos = leaf_lower_bound(key);

  // Write cell at new heap position.
  uint32_t cell_sz = static_cast<uint32_t>(LEAF_CELL_HDR + val.size());
  uint32_t cell_off = leaf_free_end() - cell_sz;
  uint8_t *cell = page_->data() + cell_off;
  write_le<uint64_t>(cell, key);
  write_le<uint32_t>(cell + 8, static_cast<uint32_t>(val.size()));
  std::memcpy(cell + 12, val.data(), val.size());

  // Shift slot array right from pos onwards.
  for (uint32_t i = n; i > pos; --i) {
    set_leaf_slot(i, leaf_slot(i - 1));
  }
  set_leaf_slot(pos, cell_off);

  set_leaf_free_end(cell_off);
  set_num_keys(n + 1);
  page_->mark_dirty();
  return true;
}

void BTreeNode::leaf_delete(uint32_t i) {
  uint32_t n = num_keys();
  // Shift slot array left (the cell data is left as dead space, compacted on
  // split).
  for (uint32_t j = i; j + 1 < n; ++j) {
    set_leaf_slot(j, leaf_slot(j + 1));
  }
  set_num_keys(n - 1);
  page_->mark_dirty();
}

void BTreeNode::init_leaf(PageId parent, PageId next) noexcept {
  std::memset(page_->data(), 0, PAGE_SIZE);
  set_node_type(NodeType::LEAF);
  set_num_keys(0);
  set_parent_id(parent);
  set_next_leaf_id(next);
  set_leaf_free_end(PAGE_SIZE); // heap starts at the very end
  page_->mark_dirty();
}

} // namespace minisql
