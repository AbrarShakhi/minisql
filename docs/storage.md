# Storage Internals

This document explains exactly how minisql stores data on disk — the two file
formats, the page layout, the B+ tree structure, and the buffer pool that
keeps recently-used pages in RAM.

---

## Table of Contents

1. [The Two Files](#1-the-two-files)
2. [The Catalog File (.cat)](#2-the-catalog-file-cat)
3. [The Database File (.db) and Pages](#3-the-database-file-db-and-pages)
4. [The B+ Tree](#4-the-b-tree)
5. [Internal Node Page Layout](#5-internal-node-page-layout)
6. [Leaf Node Page Layout — Slotted Page](#6-leaf-node-page-layout--slotted-page)
7. [Row Serialisation Format](#7-row-serialisation-format)
8. [Dead Space and Fragmentation](#8-dead-space-and-fragmentation)
9. [The Buffer Pool — LRU Cache](#9-the-buffer-pool--lru-cache)
10. [What Happens on .exit](#10-what-happens-on-exit)

---

## 1. The Two Files

Every database is stored in exactly two binary files side by side.

```
mydb.db     B+ tree page file   (all row data)
mydb.cat    catalog file        (table schemas, metadata)
```

Both are created automatically the first time you open a database name that
does not exist yet.

They are always used together.  Deleting or replacing one without the other
will corrupt the database.

---

## 2. The Catalog File (.cat)

The catalog is a compact binary file that answers the question:
**"what tables exist, and what do they look like?"**

It contains no row data.  It stores:
- Table names
- Column names, types, and nullable flags
- Which page in the `.db` file is the root of each table's B+ tree
- The next auto-increment rowid for each table

### Binary format

```
Offset   Size  Field
──────────────────────────────────────────────────────────────────
0        4     Magic number: bytes 49 4E 49 4D  (="MINI" little-endian)
4        4     Version: 1
8        4     Number of tables

For each table (repeated table_count times):
  [0]    1     Length of table name  (N bytes)
  [1]    N     Table name  (no null terminator)
  [N+1]  4     root_page_id  ← which page in .db holds the B+ tree root
  [N+5]  8     next_row_id   ← auto-increment counter; next INSERT gets this value
  [N+13] 2     Number of columns  (C)

  For each column (repeated C times):
    [0]  1     Length of column name  (M bytes)
    [1]  M     Column name
    [M+1]1     Data type  (0=INTEGER  1=REAL  2=TEXT  3=BLOB  4=NULL)
    [M+2]1     Nullable   (0=NOT NULL  1=nullable)
```

All multi-byte integers are **little-endian** (least-significant byte first).

### Annotated example

After `CREATE TABLE users (id INTEGER NOT NULL, name TEXT NOT NULL, age INTEGER)` and
three inserts, the catalog looks like this on disk:

```
49 4E 49 4D          magic ("MINI" reversed)
01 00 00 00          version = 1
01 00 00 00          1 table

05                   name length = 5
75 73 65 72 73       "users"
00 00 00 00          root_page_id = 0   (page 0 in .db)
04 00 00 00 00 00 00 00   next_row_id = 4   (next INSERT gets rowid 4)
03 00                3 columns

02                   col name length = 2
69 64                "id"
00                   type = INTEGER
00                   NOT NULL

04                   col name length = 4
6E 61 6D 65         "name"
02                   type = TEXT
00                   NOT NULL

03                   col name length = 3
61 67 65             "age"
00                   type = INTEGER
01                   nullable
```

### When it is updated

The catalog file is rewritten to disk after every:
- `CREATE TABLE`
- `DROP TABLE`
- `INSERT` (to update `next_row_id`)
- `UPDATE`
- `DELETE`
- `.exit`

It is fully loaded into RAM at startup and kept there for the whole session.

---

## 3. The Database File (.db) and Pages

The `.db` file is a flat binary file divided into **fixed-size 4096-byte
pages**.  Think of it as an array of pages:

```
File on disk:
┌─────────────┬─────────────┬─────────────┬─────────────┐
│   Page  0   │   Page  1   │   Page  2   │   Page  3   │ ...
│  4096 bytes │  4096 bytes │  4096 bytes │  4096 bytes │
└─────────────┴─────────────┴─────────────┴─────────────┘
 byte 0        byte 4096     byte 8192     byte 12288
```

**Addressing:** Page number `N` starts at byte offset `N × 4096` in the file.

**Page IDs:** The `DiskManager` assigns page IDs sequentially starting at 0.
The first `CREATE TABLE` creates page 0.  Every B+ tree split allocates a new
page at the end of the file.  Page IDs are stored in the catalog and in the
B+ tree nodes themselves (as child pointers).

**Page count:** The total number of pages = file size ÷ 4096.

**Each page is one of two things:**
- An **internal node** (B+ tree navigation page — no row data)
- A **leaf node** (B+ tree data page — holds actual rows)

The very first byte of every page is a `node_type` field:
```
0  →  INTERNAL node
1  →  LEAF node
```

---

## 4. The B+ Tree

Each table has its own independent **B+ tree**, rooted at the page ID stored
in the catalog.

### What is a B+ tree?

A B+ tree is a self-balancing search tree with three key properties:

1. **All data lives in the leaves.**  Internal nodes hold only separator keys
   and pointers to children — they contain no row data.

2. **Leaves are linked.**  Every leaf holds a pointer to the next leaf in
   sorted order, forming a singly-linked list.  This makes range scans
   (`ORDER BY`, scanning all rows) very efficient.

3. **The tree always stays balanced.**  When a page fills up, it splits into
   two pages and pushes a separator key up to the parent.  When the root
   splits, a new root page is created.  Every leaf is always at the same
   depth from the root.

### Visual example

```
                    ┌────────────────────────┐
                    │   Internal node        │
                    │   keys: [50, 100]      │
                    └───┬────────┬────────┬──┘
                        │        │        │
           ┌────────────┘        │        └────────────┐
           ▼                     ▼                     ▼
    ┌────────────┐        ┌────────────┐        ┌────────────┐
    │ Leaf page  │──next──│ Leaf page  │──next──│ Leaf page  │──next──►NULL
    │ rows 1-49  │        │ rows 50-99 │        │ rows 100+  │
    └────────────┘        └────────────┘        └────────────┘
```

### The B+ tree key (rowid)

The key used to order and look up rows is the **rowid** — a hidden
`uint64_t` auto-increment counter maintained per table in the catalog.

```
First INSERT into a table  → rowid = 1
Second INSERT              → rowid = 2
Third INSERT               → rowid = 3
...
```

The rowid is separate from any `id` column you declare yourself.  Your `id`
column is just another value stored inside the row — the rowid is the B+ tree
key that decides where the row physically lives on disk.

### Capacity

| Node type | Max keys per page | Max children / cells |
|---|---|---|
| Internal | 340 | 341 |
| Leaf | depends on row size | ~200 for typical rows |

A two-level B+ tree (root + leaves) can hold `341 × 200 ≈ 68,000` rows.
A three-level tree can hold `341² × 200 ≈ 23 million` rows.

### B+ tree operations

| SQL statement | B+ tree operation |
|---|---|
| `INSERT` | Walk to correct leaf, insert cell; split leaf if full, propagate up |
| `SELECT` | Walk internal nodes to reach the leftmost matching leaf, then scan forward through the linked list |
| `DELETE` | Walk to leaf, remove the slot entry from the slot array |
| `UPDATE` | DELETE old rowid + INSERT same rowid with new values |

---

## 5. Internal Node Page Layout

An internal node acts as a **navigation guide** — it stores separator keys
and pointers (page IDs) to child pages.  It stores no row data.

```
Byte range   Size   Field
──────────────────────────────────────────────
0            1      node_type = 0  (INTERNAL)
1–4          4      num_keys    (N = number of separator keys stored)
5–8          4      parent_page_id  (0xFFFFFFFF if this is the root)
```

After the header, keys and child pointers alternate:

```
9–12         4      child[0]    ← page ID of leftmost child
13–20        8      key[0]      ← uint64_t separator key
21–24        4      child[1]
25–32        8      key[1]
...
9 + N×12    4      child[N]    ← page ID of rightmost child
```

**Navigation rule:**  To find which child to follow for search key K:

```
if K < key[0]          → go to child[0]
if key[i-1] ≤ K < key[i]  → go to child[i]
if K ≥ key[N-1]        → go to child[N]
```

**Capacity calculation:**

```
Page size              = 4096 bytes
Header                 =    9 bytes
Extra child pointer    =    4 bytes
Available for keys     = 4096 - 9 - 4 = 4083 bytes
Bytes per (key+child)  = 8 + 4 = 12 bytes
Maximum keys           = 4083 / 12 = 340
Maximum children       = 341
```

**When an internal node fills up:**

The node is split in the same way as a leaf: the middle key is pushed up to
the parent, and the keys/children are divided between the original node and a
new node.  If the root is split, a new root page is allocated.

---

## 6. Leaf Node Page Layout — Slotted Page

A leaf node stores **actual row data** using the **slotted page** format.

The slotted page design allows cells of variable size to be packed into a
fixed-size page efficiently, without wasting space or requiring all rows to be
the same size.

### Page structure

```
┌──────────────────────────────────────────────────────┐  byte 0
│  HEADER  (17 bytes total)                            │
│  [0]      node_type = 1  (LEAF)                      │
│  [1–4]    num_cells                                  │
│  [5–8]    parent_page_id   (0xFFFFFFFF = root)       │
│  [9–12]   next_leaf_page_id (0xFFFFFFFF = last leaf) │
│  [13–16]  free_end  ← byte offset where heap starts  │
├──────────────────────────────────────────────────────┤  byte 17
│  SLOT ARRAY  (grows →, 4 bytes per slot)             │
│  slot[0] = byte offset of cell 0                     │
│  slot[1] = byte offset of cell 1                     │
│  slot[2] = byte offset of cell 2                     │
│  ...                                                 │
├──────────────────────────────────────────────────────┤
│                                                      │
│            F R E E   S P A C E                       │
│                                                      │
├──────────────────────────────────────────────────────┤
│  CELL HEAP  (grows ←, packed from the right side)    │
│  ...                                                 │
│  cell[1]:  key(8 bytes) val_len(4 bytes) val_data    │
│  cell[0]:  key(8 bytes) val_len(4 bytes) val_data    │
└──────────────────────────────────────────────────────┘  byte 4095
```

### How the two regions meet

The **slot array** grows to the right (higher addresses) starting at byte 17.
The **cell heap** grows to the left (lower addresses) starting at `free_end`
(initially 4096).

When you insert a new cell:
1. `free_end` is decremented by the size of the new cell.
2. The cell is written at the new `free_end` position.
3. A new slot entry pointing to `free_end` is appended to the slot array.
4. `num_cells` is incremented.

The page is **full** when:
```
slot_array_end  =  17 + num_cells × 4
cell needed     =  4 (new slot) + 8 (key) + 4 (val_len) + len(row bytes)
full when:  slot_array_end + cell_needed  >  free_end
```

### Slot array ordering

Slots are kept in **ascending rowid order** within the slot array.  This means
the slot array itself is sorted, so a binary search can find any rowid in
O(log n) without scanning the entire cell heap.

### Reading a cell

1. Read `slot[i]` → get byte offset `off`.
2. Read 8 bytes at `off` → `rowid` (uint64_t, little-endian).
3. Read 4 bytes at `off + 8` → `val_len` (uint32_t, little-endian).
4. Read `val_len` bytes starting at `off + 12` → serialised row data.

### Annotated hex example

For a page holding two rows (Alice rowid=1, Charlie rowid=3):

```
Hex                             Meaning
────────────────────────────────────────────────────────────
01                              node_type = 1  (LEAF)
02 00 00 00                     num_cells = 2
FF FF FF FF                     parent_page_id = INVALID (root)
FF FF FF FF                     next_leaf_page_id = INVALID (last)
60 0F 00 00                     free_end = 0x0F60 = 3936

[slot array at byte 17]
60 0F 00 00                     slot[0] = 3936  (cell 0 is at byte 3936)
88 0F 00 00                     slot[1] = 3976  (cell 1 is at byte 3976)

[free space: bytes 25 – 3935]

[cell heap at byte 3936]
── cell 0 at byte 3936 ──
01 00 00 00 00 00 00 00         rowid = 1
1C 00 00 00                     val_len = 28
00 01 00 00 00 00 00 00 00 00   col[0] INTEGER tag + value 1
02 05 00 00 00 41 6C 69 63 65  col[1] TEXT tag + len 5 + "Alice"
00 1F 00 00 00 00 00 00 00      col[2] INTEGER tag + value 31

── cell 1 at byte 3976 ──
03 00 00 00 00 00 00 00         rowid = 3
1E 00 00 00                     val_len = 30
00 03 00 00 00 00 00 00 00      col[0] INTEGER 3
02 07 00 00 00 43 68 61 72 ...  col[1] TEXT "Charlie"
00 23 00 00 00 00 00 00 00      col[2] INTEGER 35
```

---

## 7. Row Serialisation Format

Each row is serialised into a compact byte sequence stored as the `val_data`
portion of a leaf cell.

### Format

For each column, in declaration order:

```
[1 byte]   type tag
           0 = INTEGER
           1 = REAL
           2 = TEXT
           3 = BLOB
           4 = NULL

Then:
  If INTEGER:  8 bytes, little-endian int64_t
  If REAL:     8 bytes, IEEE 754 double
  If TEXT:     4 bytes little-endian uint32_t (string length)
               then that many bytes of UTF-8 text
  If BLOB:     4 bytes little-endian uint32_t (byte count)
               then that many raw bytes
  If NULL:     nothing — the type tag alone is sufficient
```

### Byte-by-byte example: `(1, 'Alice', 31)`

Schema: `id INTEGER NOT NULL, name TEXT NOT NULL, age INTEGER`

```
byte 0:     00         ← INTEGER tag for 'id'
bytes 1–8:  01 00 00 00 00 00 00 00   ← int64 value 1

byte 9:     02         ← TEXT tag for 'name'
bytes 10–13:05 00 00 00   ← string length = 5
bytes 14–18:41 6C 69 63 65   ← "Alice"

byte 19:    00         ← INTEGER tag for 'age'
bytes 20–27:1F 00 00 00 00 00 00 00   ← int64 value 31 (0x1F)

Total: 28 bytes  (stored as val_len in the cell header)
```

### Byte-by-byte example: `(4, 'Dave', NULL)`

```
byte 0:     00  01 00 00 00 00 00 00 00 00   ← INTEGER 4
byte 10:    02  04 00 00 00 44 61 76 65      ← TEXT "Dave"
byte 19:    04                               ← NULL (no extra bytes)

Total: 20 bytes
```

---

## 8. Dead Space and Fragmentation

When a row is **deleted**, its slot entry is removed from the slot array (by
shifting all later slots left), but the cell bytes themselves are **not erased
from the heap**.  The freed bytes become invisible dead space.

When a row is **updated**, it is deleted and re-inserted.  The old cell bytes
become dead space; the new cell is allocated at the current `free_end`.

This means `free_end` can only ever decrease, and pages accumulate dead
space over time.

```
Before delete (3 cells):
  slot: [A] [B] [C]
  heap: ... [C_data] [B_data] [A_data]

After DELETE where slot B:
  slot: [A] [C]                   ← B removed from slot array
  heap: ... [C_data] [B_data] [A_data]   ← B_data still here (dead space)
```

**When is dead space reclaimed?**

When a leaf node fills up and must split, all live cells are collected
(via the slot array, which only points to live cells), the page is
reinitialised to empty, and the live cells are re-inserted.  Dead bytes are
discarded at that point.

Dead space has no effect on query correctness — queries only follow slot
array pointers.  It does reduce the usable capacity of a page before a split.

---

## 9. The Buffer Pool — LRU Cache

Reading from or writing to disk for every page access would be extremely
slow.  The `BufferPool` keeps a cache of recently-used pages in RAM,
dramatically reducing disk I/O.

### Frames

The buffer pool manages a fixed number of **frames** — slots in RAM, each
exactly 4096 bytes.

```
Default capacity: 128 frames
RAM used:         128 × 4096 = 524,288 bytes (512 KB)
```

### Page table

A hash map tracks which page ID is in which frame:

```
page_table:  { page_id → frame_id }
```

### LRU eviction

The pool maintains a **doubly-linked list** sorted from least-recently-used
(LRU, head) to most-recently-used (MRU, tail).  Every time a page is accessed,
it moves to the tail.  When a new page must be loaded and no free frames exist,
the page at the head is evicted.

```
LRU list: [page 5] ← [page 2] ← [page 8] ← [page 1]
                                              ↑ most recently used
           ↑ evict this one first (if unpinned)
```

### Pin count

While a page is in use (inside BTreeNode read/write operations), it is
**pinned** (pin count > 0).  Pinned pages are never evicted.

The caller must call `unpin_page(id, dirty)` when done:
- `dirty = true`  → the page was modified; write it to disk before eviction.
- `dirty = false` → the page was only read; no write needed.

### Fetch lifecycle

```
fetch_page(id):
  1. Is id in page_table?
       Yes → move frame to MRU end of LRU list, increment pin count, return page
       No  → continue

  2. Is there a free frame?
       Yes → use it
       No  → evict the LRU unpinned frame
             (if evicted frame is dirty → write to disk first)

  3. Read page id from disk into the chosen frame
  4. Add to page_table, add to MRU end of LRU list
  5. Pin (pin count = 1), return page
```

### flush_all

Called on `.exit` and after every DML statement.  Writes every dirty frame
to disk without evicting them.

---

## 10. What Happens on .exit

When you type `.exit` or `.quit`, or send EOF (Ctrl+D), the following happens
in order:

1. **BufferPool::flush_all()** — every dirty page is written to the `.db` file.
2. **Catalog::flush()** — the in-memory catalog (with up-to-date rowid
   counters) is written to the `.cat` file.
3. The fstream for the `.db` file is closed.
4. The program exits.

If you kill the process without `.exit`, dirty pages in the buffer pool are
lost.  The catalog may also show a stale `next_row_id`.  This can leave the
database in an inconsistent state.  See [limitations.md](limitations.md) for
details.
