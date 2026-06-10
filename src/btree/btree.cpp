#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>

#include "btree/btree.hpp"
#include "btree/btree_node.hpp"
#include "common/error.hpp"

namespace minisql {

BTree::BTree(BufferPool &bp, PageId root_page_id) noexcept
    : bp_(bp), root_(root_page_id) {}

// ── Public interface
// ──────────────────────────────────────────────────────────

bool BTree::insert(uint64_t key, const std::vector<uint8_t> &value) {
  if (root_ == INVALID_PAGE_ID) {
    // First insertion: allocate root leaf.
    PageId rid{};
    Page *page = alloc_page(rid);
    BTreeNode node(page);
    node.init_leaf();
    node.leaf_insert(key, value);
    bp_.unpin_page(rid, true);
    root_ = rid;
    return true;
  }

  Page *leaf = find_leaf(key);
  BTreeNode leaf_node(leaf);

  // Duplicate key check.
  uint32_t pos = leaf_node.leaf_lower_bound(key);
  if (pos < leaf_node.num_keys() && leaf_node.leaf_key(pos) == key) {
    bp_.unpin_page(leaf->page_id(), false);
    return false;
  }

  if (leaf_node.leaf_has_space(value.size())) {
    leaf_node.leaf_insert(key, value);
    bp_.unpin_page(leaf->page_id(), true);
    return true;
  }

  // Need to split.
  uint64_t push_up_key{};
  PageId new_leaf_id{};
  // We need to do the insert, then split; collect all cells + new one.
  // Collect existing cells.
  uint32_t n = leaf_node.num_keys();
  std::vector<std::pair<uint64_t, std::vector<uint8_t>>> cells;
  cells.reserve(n + 1);
  for (uint32_t i = 0; i < n; ++i) {
    cells.emplace_back(leaf_node.leaf_key(i), leaf_node.leaf_value(i));
  }
  // Insert new cell in sorted position.
  cells.emplace_back(key, value);
  std::stable_sort(
      cells.begin(), cells.end(),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  // Split: left keeps [0, mid), right keeps [mid, end).
  uint32_t mid = static_cast<uint32_t>(cells.size() + 1) / 2;

  // Reinitialise original leaf with left half.
  PageId orig_next = leaf_node.next_leaf_id();
  PageId orig_pid = leaf->page_id();
  PageId par_pid = leaf_node.parent_id();
  leaf_node.init_leaf(par_pid, INVALID_PAGE_ID); // next set later
  for (uint32_t i = 0; i < mid; ++i) {
    leaf_node.leaf_insert(cells[i].first, cells[i].second);
  }

  // Allocate right leaf.
  Page *right_page = alloc_page(new_leaf_id);
  BTreeNode right_node(right_page);
  right_node.init_leaf(par_pid, orig_next);
  for (uint32_t i = mid; i < static_cast<uint32_t>(cells.size()); ++i) {
    right_node.leaf_insert(cells[i].first, cells[i].second);
  }
  leaf_node.set_next_leaf_id(new_leaf_id);

  push_up_key = right_node.leaf_key(0);

  // Update dirty state and unpin right; keep left pinned for
  // insert_into_parent.
  bp_.unpin_page(new_leaf_id, true);

  insert_into_parent(leaf, push_up_key, right_page);
  bp_.unpin_page(orig_pid, true);
  return true;
}

std::optional<std::vector<uint8_t>> BTree::search(uint64_t key) const {
  if (root_ == INVALID_PAGE_ID)
    return std::nullopt;
  Page *leaf = find_leaf(key);
  BTreeNode node(leaf);
  uint32_t pos = node.leaf_lower_bound(key);
  std::optional<std::vector<uint8_t>> result;
  if (pos < node.num_keys() && node.leaf_key(pos) == key) {
    result = node.leaf_value(pos);
  }
  bp_.unpin_page(leaf->page_id(), false);
  return result;
}

bool BTree::remove(uint64_t key) {
  if (root_ == INVALID_PAGE_ID)
    return false;
  Page *leaf = find_leaf(key);
  BTreeNode node(leaf);
  uint32_t pos = node.leaf_lower_bound(key);
  if (pos >= node.num_keys() || node.leaf_key(pos) != key) {
    bp_.unpin_page(leaf->page_id(), false);
    return false;
  }
  node.leaf_delete(pos);
  // Simple rebalance: if leaf empties and it is root, reset root.
  if (node.num_keys() == 0 && leaf->page_id() == root_) {
    root_ = INVALID_PAGE_ID;
  }
  bp_.unpin_page(leaf->page_id(), true);
  return true;
}

void BTree::range_scan(
    uint64_t lo, uint64_t hi,
    const std::function<void(uint64_t, const std::vector<uint8_t> &)> &cb)
    const {
  if (root_ == INVALID_PAGE_ID)
    return;
  Page *leaf = find_leaf(lo);
  while (leaf != nullptr) {
    BTreeNode node(leaf);
    uint32_t n = node.num_keys();
    PageId nxt = node.next_leaf_id();
    bool done = false;
    for (uint32_t i = 0; i < n; ++i) {
      uint64_t k = node.leaf_key(i);
      if (k < lo)
        continue;
      if (k > hi) {
        done = true;
        break;
      }
      cb(k, node.leaf_value(i));
    }
    bp_.unpin_page(leaf->page_id(), false);
    if (done || nxt == INVALID_PAGE_ID)
      break;
    leaf = bp_.fetch_page(nxt);
  }
}

void BTree::scan_all(
    const std::function<void(uint64_t, const std::vector<uint8_t> &)> &cb)
    const {
  Page *leaf = leftmost_leaf();
  while (leaf != nullptr) {
    BTreeNode node(leaf);
    uint32_t n = node.num_keys();
    PageId nxt = node.next_leaf_id();
    for (uint32_t i = 0; i < n; ++i) {
      cb(node.leaf_key(i), node.leaf_value(i));
    }
    bp_.unpin_page(leaf->page_id(), false);
    if (nxt == INVALID_PAGE_ID)
      break;
    leaf = bp_.fetch_page(nxt);
  }
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

Page *BTree::find_leaf(uint64_t key) const {
  Page *page = bp_.fetch_page(root_);
  while (page != nullptr) {
    BTreeNode node(page);
    if (node.is_leaf())
      return page;
    uint32_t n = node.num_keys();
    uint32_t i = 0;
    while (i < n && key >= node.internal_key(i))
      ++i;
    PageId child_id = node.internal_child(i);
    bp_.unpin_page(page->page_id(), false);
    page = bp_.fetch_page(child_id);
  }
  return nullptr;
}

void BTree::insert_into_parent(Page *left, uint64_t key, Page *right) {
  BTreeNode left_node(left);
  PageId par_id = left_node.parent_id();

  if (par_id == INVALID_PAGE_ID) {
    insert_into_new_root(left, key, right);
    return;
  }

  Page *parent = bp_.fetch_page(par_id);
  BTreeNode par_node(parent);

  // Find position of left child.
  uint32_t n = par_node.num_keys();
  uint32_t pos = 0;
  while (pos <= n && par_node.internal_child(pos) != left->page_id())
    ++pos;

  if (!par_node.internal_is_full()) {
    par_node.internal_insert(pos, key, right->page_id());
    BTreeNode right_node(right);
    right_node.set_parent_id(par_id);
    bp_.unpin_page(par_id, true);
    return;
  }

  // Parent is full → split parent.
  // Collect all keys and children from parent + new entry.
  uint32_t pn = par_node.num_keys();
  std::vector<uint64_t> keys;
  std::vector<PageId> children;
  keys.reserve(pn + 1);
  children.reserve(pn + 2);
  for (uint32_t i = 0; i <= pn; ++i) {
    children.push_back(par_node.internal_child(i));
    if (i < pn)
      keys.push_back(par_node.internal_key(i));
  }
  // Insert new key at pos.
  keys.insert(keys.begin() + pos, key);
  children.insert(children.begin() + pos + 1, right->page_id());

  // Middle key pushed up.
  uint32_t mid = static_cast<uint32_t>(keys.size()) / 2;
  uint64_t pushed_key = keys[mid];

  // Rebuild left (original parent).
  PageId orig_par_id = par_node.parent_id();
  par_node.init_internal(orig_par_id);
  for (uint32_t i = 0; i < mid; ++i) {
    par_node.set_internal_child(i, children[i]);
    par_node.set_internal_key(i, keys[i]);
  }
  par_node.set_internal_child(mid, children[mid]);
  par_node.set_num_keys(mid);

  // Build new right internal.
  PageId new_int_id{};
  Page *new_int_page = alloc_page(new_int_id);
  BTreeNode new_int(new_int_page);
  new_int.init_internal(INVALID_PAGE_ID); // parent set below
  for (uint32_t i = mid + 1; i < static_cast<uint32_t>(keys.size()); ++i) {
    uint32_t ni = i - (mid + 1);
    new_int.set_internal_child(ni, children[i]);
    new_int.set_internal_key(ni, keys[i]);
  }
  new_int.set_internal_child(static_cast<uint32_t>(keys.size()) - mid - 1,
                             children.back());
  new_int.set_num_keys(static_cast<uint32_t>(keys.size()) - mid - 1);

  // Update parent pointers of children that moved to new_int.
  for (uint32_t i = 0; i <= new_int.num_keys(); ++i) {
    PageId cid = new_int.internal_child(i);
    Page *cp = bp_.fetch_page(cid);
    BTreeNode cn(cp);
    cn.set_parent_id(new_int_id);
    bp_.unpin_page(cid, true);
  }
  // Also update right (the new leaf/internal we are inserting).
  BTreeNode right_node(right);
  right_node.set_parent_id(
      (right->page_id() == children[mid + 1] || // part of new_int
       std::find(children.begin() + mid + 1, children.end(),
                 right->page_id()) != children.end())
          ? new_int_id
          : par_id);

  bp_.unpin_page(new_int_id, true);
  insert_into_parent(parent, pushed_key, new_int_page);
  bp_.unpin_page(par_id, true);
}

void BTree::insert_into_new_root(Page *left, uint64_t key, Page *right) {
  PageId new_root_id{};
  Page *root_page = alloc_page(new_root_id);
  BTreeNode root_node(root_page);
  root_node.init_internal(INVALID_PAGE_ID);
  root_node.set_internal_child(0, left->page_id());
  root_node.set_internal_key(0, key);
  root_node.set_internal_child(1, right->page_id());
  root_node.set_num_keys(1);

  BTreeNode left_node(left);
  BTreeNode right_node(right);
  left_node.set_parent_id(new_root_id);
  right_node.set_parent_id(new_root_id);

  bp_.unpin_page(new_root_id, true);
  root_ = new_root_id;
}

Page *BTree::alloc_page(PageId &out_id) {
  Page *p = bp_.new_page(out_id);
  if (!p)
    throw StorageError("Buffer pool exhausted – cannot allocate new page");
  return p;
}

Page *BTree::leftmost_leaf() const {
  if (root_ == INVALID_PAGE_ID)
    return nullptr;
  Page *page = bp_.fetch_page(root_);
  while (page) {
    BTreeNode node(page);
    if (node.is_leaf())
      return page;
    PageId child = node.internal_child(0);
    bp_.unpin_page(page->page_id(), false);
    page = bp_.fetch_page(child);
  }
  return nullptr;
}

} // namespace minisql
