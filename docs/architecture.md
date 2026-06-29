# Architecture

This document gives the high-level picture: the module layout, how the layers
interact, and which design principles are applied and why.

---

## Table of Contents

1. [Bird's-Eye View](#1-birds-eye-view)
2. [Module Map](#2-module-map)
3. [Layer Diagram](#3-layer-diagram)
4. [Data Flow Diagram](#4-data-flow-diagram)
5. [Key Classes and Their Responsibilities](#5-key-classes-and-their-responsibilities)
6. [SOLID Principles](#6-solid-principles)
7. [Memory Management](#7-memory-management)
8. [File Layout](#8-file-layout)

---

## 1. Bird's-Eye View

minisql is structured in six horizontal layers.  Each layer knows only about
the layer(s) below it.  No layer ever calls upward.

```
┌────────────────────────────────────────────────────────────────┐
│  REPL layer          repl/repl.cpp                             │
│  Interactive shell — reads stdin, dispatches to executor       │
├────────────────────────────────────────────────────────────────┤
│  SQL layer           sql/lexer.cpp  sql/parser.cpp             │
│  Text → tokens → AST                                           │
├────────────────────────────────────────────────────────────────┤
│  Executor layer      executor/executor.cpp                     │
│  AST → result set; evaluates WHERE; calls B+ tree              │
├────────────────────────────────────────────────────────────────┤
│  Catalog layer       catalog/catalog.cpp                       │
│  Table schemas, root page IDs, rowid counters                  │
├────────────────────────────────────────────────────────────────┤
│  B+ Tree layer       btree/btree.cpp  btree/btree_node.cpp     │
│  Balanced tree of pages; insert / search / delete / scan       │
├────────────────────────────────────────────────────────────────┤
│  Storage layer       storage/buffer_pool.cpp                   │
│                      storage/disk_manager.cpp                  │
│  Page cache (LRU) and raw disk I/O                             │
└────────────────────────────────────────────────────────────────┘
```

---

## 2. Module Map

Every directory has one responsibility.  Every file in that directory has
one responsibility within it.

```
minisql/
│
├── include/                  Public headers (one per class)
│   ├── common/
│   │   ├── config.hpp        Compile-time constants (PAGE_SIZE, BUFFER_POOL_SIZE, …)
│   │   ├── types.hpp         Value variant type; to_string, compare_values helpers
│   │   └── error.hpp         Exception hierarchy (DatabaseError and five sub-types)
│   │
│   ├── storage/
│   │   ├── page.hpp          One 4096-byte frame: data, page_id, dirty flag, pin count
│   │   ├── disk_manager.hpp  File I/O: read_page, write_page, allocate_page
│   │   └── buffer_pool.hpp   LRU page cache: fetch_page, new_page, unpin_page, flush_all
│   │
│   ├── btree/
│   │   ├── btree_node.hpp    Typed read/write over a raw Page buffer (no ownership)
│   │   └── btree.hpp         B+ tree: insert, search, remove, scan_all, range_scan
│   │
│   ├── catalog/
│   │   ├── column.hpp        Column{name, DataType, nullable} — plain data struct
│   │   ├── schema.hpp        Ordered column set + binary serialise/deserialise
│   │   └── catalog.hpp       map<name,TableMeta>; persists to .cat on every change
│   │
│   ├── sql/
│   │   ├── token.hpp         TokenType enum + Token{type, value, line}
│   │   ├── lexer.hpp         string → vector<Token>
│   │   ├── ast.hpp           Expr and Stmt AST node types + factory methods
│   │   └── parser.hpp        vector<Token> → unique_ptr<Stmt> (recursive descent)
│   │
│   ├── executor/
│   │   ├── row.hpp           Row = vector<Value>; serialise_row / deserialise_row
│   │   ├── result_set.hpp    ResultSet: column names + rows + ASCII table printer
│   │   └── executor.hpp      Stmt → ResultSet; owns B+ trees; evaluates expressions
│   │
│   └── repl/
│       └── repl.hpp          REPL loop + Pimpl; owns all sub-systems
│
├── src/                      Implementations (one per header above)
│   ├── common/types.cpp
│   ├── storage/{page,disk_manager,buffer_pool}.cpp
│   ├── btree/{btree_node,btree}.cpp
│   ├── catalog/{column,schema,catalog}.cpp
│   ├── sql/{lexer,ast,parser}.cpp
│   ├── executor/{row,result_set,executor}.cpp
│   ├── repl/repl.cpp
│   └── main.cpp              Entry point — parse argv, build Repl, call run()
│
├── docs/                     Documentation (this directory)
└── CMakeLists.txt            Build system
```

---

## 3. Layer Diagram

Dependencies flow strictly downward.  An arrow means "knows about / uses".

```
main.cpp
   │
   ▼
Repl
   │
   ├──► Lexer  ──► Token, Lexer
   │
   ├──► Parser ──► Lexer output, Expr/Stmt AST
   │
   └──► Executor
           │
           ├──► Catalog   ──► Schema, Column, TableMeta
           │
           ├──► BTree     ──► BTreeNode
           │       │
           │       └──► BufferPool ──► Page
           │                   │
           │                   └──► DiskManager
           │
           ├──► Row serialisation (row.cpp)
           │
           └──► ResultSet printer
```

Nothing in the storage layer knows about SQL.
Nothing in the SQL layer knows about pages or B+ trees.
The executor is the only component that bridges both worlds.

---

## 4. Data Flow Diagram

```
User input (stdin)
       │
       │  raw SQL string
       ▼
  ┌──────────┐
  │  Lexer   │  string → vector<Token>
  └────┬─────┘
       │
       ▼
  ┌──────────┐
  │  Parser  │  tokens → unique_ptr<Stmt>
  └────┬─────┘
       │
       ▼
  ┌──────────────────────────┐
  │       Executor           │
  │                          │
  │  ┌────────────────────┐  │
  │  │  eval_expr / WHERE │  │
  │  └────────────────────┘  │
  │  ┌────────────────────┐  │
  │  │  row serialise /   │  │
  │  │  deserialise       │  │
  │  └────────────────────┘  │
  └────────┬─────────────────┘
           │
     ┌─────┴──────┐
     │            │
     ▼            ▼
 Catalog        BTree
 (.cat file)    │
                ▼
           BufferPool
           (RAM cache)
                │
                ▼
           DiskManager
           (.db file)
```

Read path (SELECT):

```
Executor ──scan_all──► BTree
  BTree   ──fetch_page──► BufferPool
    BufferPool ──(cache hit)──► return page from RAM
    BufferPool ──(cache miss)──► DiskManager.read_page() → load from .db
  BTree reads cells via BTreeNode accessors
  Executor deserialises rows, evaluates WHERE, collects results
```

Write path (INSERT):

```
Executor ──insert──► BTree
  BTree ──find_leaf──► BufferPool (fetch root, walk to leaf)
  BTree ──leaf_insert──► BTreeNode.leaf_insert()
    If leaf full: BTree splits, allocates new page, updates parent
  BTree unpins all touched pages (dirty=true)
  Executor ──flush_all──► BufferPool → DiskManager.write_page() for each dirty page
  Executor ──flush──► Catalog → writes .cat file
```

---

## 5. Key Classes and Their Responsibilities

### DiskManager

Single responsibility: **raw page I/O**.

Knows nothing about B+ trees or SQL.  Knows only the database file path,
the total number of pages, and how to seek-and-read / seek-and-write one
page at a time.

```
read_page(id, buf)    → seeks to id×4096, reads PAGE_SIZE bytes
write_page(id, buf)   → seeks to id×4096, writes PAGE_SIZE bytes; extends file
allocate_page()       → writes a zero page at the end; returns the new page ID
```

### BufferPool

Single responsibility: **keeping frequently-used pages in RAM**.

Owns an array of 128 `Page` frames.  Implements LRU eviction.  Knows nothing
about B+ trees.  Tracks dirty pages so it knows which ones need flushing.

### Page

Single responsibility: **a 4096-byte buffer with bookkeeping metadata**.

Holds the raw bytes, a page ID, a dirty flag, and a pin count.  No logic
beyond these fields.

### BTreeNode

Single responsibility: **typed access to a page's B+ tree fields**.

Does not own a page — it wraps a `Page*` borrowed from the buffer pool.
Provides methods like `leaf_key(i)`, `leaf_insert(key, val)`,
`internal_child(i)`.  All reads and writes go through this class so the
page layout is never manually calculated elsewhere.

### BTree

Single responsibility: **the balanced tree algorithms**.

Implements `insert`, `search`, `remove`, `scan_all`, `range_scan`.  Uses
`BTreeNode` to access page contents.  Uses `BufferPool` to fetch and allocate
pages.  Decides when to split or merge.

### Schema

Single responsibility: **table column structure**.

An ordered list of `Column` objects.  Provides `index_of(name)` and
binary serialise/deserialise for the catalog file.

### Catalog

Single responsibility: **the in-memory registry of all tables**.

Maps table names to `TableMeta{schema, root_page_id, next_row_id}`.
Serialises itself to the `.cat` file whenever metadata changes.

### Lexer

Single responsibility: **turning a string into tokens**.

Reads one character at a time, produces `Token` objects.  Knows nothing
about grammar.

### Parser

Single responsibility: **turning tokens into an AST**.

Recursive descent.  Knows the SQL grammar.  Knows nothing about table
schemas or B+ trees.

### Executor

Single responsibility: **executing a Stmt against live data**.

The only component that connects the SQL layer to the storage layer.
Receives a `Stmt`, calls `Catalog` and `BTree`, returns a `ResultSet`.

### Repl

Single responsibility: **the interactive shell**.

Owns all other sub-systems (via `Pimpl`).  Reads lines from stdin, handles
meta-commands, drives the Lexer → Parser → Executor pipeline, calls the
result printer.

---

## 6. SOLID Principles

### Single Responsibility Principle (SRP)

Every file does exactly one thing.  `disk_manager.cpp` only does file I/O.
`lexer.cpp` only tokenises.  `btree_node.cpp` only decodes page bytes.
`result_set.cpp` only formats output.  No file mixes two concerns.

When you need to change how pages are read from disk, you change only
`disk_manager.cpp` — nothing else.  When you need to add a new SQL clause,
you change only `parser.cpp` and `executor.cpp`.

### Open/Closed Principle (OCP)

The Executor dispatches on `StmtKind`.  Adding a new SQL statement means
adding a new `StmtKind` value and a new `exec_*` method — no existing methods
change.  The Lexer's keyword table is a data structure, so new keywords can
be added without changing the tokenisation logic.

### Liskov Substitution Principle (LSP)

All exception types extend `DatabaseError`.  A caller that catches
`DatabaseError` handles all error sub-types correctly without knowing which
sub-type was thrown.

### Interface Segregation Principle (ISP)

`BTreeNode` exposes its internal-node methods (`internal_key`, `internal_child`,
`internal_insert`) and leaf-node methods (`leaf_key`, `leaf_value`,
`leaf_insert`, `leaf_delete`) as separate groups.  Code that works with
internal nodes never calls leaf methods, and vice versa.

### Dependency Inversion Principle (DIP)

`BTree` does not depend on `DiskManager` directly.  It depends on
`BufferPool`, which is the higher-level abstraction.  `Executor` does not
open files.  It depends on `Catalog` and `BufferPool` objects injected by
the REPL.  This makes it straightforward to test any component in isolation
by supplying a mock or in-memory alternative.

---

## 7. Memory Management

minisql uses RAII throughout and avoids raw `new`/`delete`.

| Component | Ownership model |
|---|---|
| `BufferPool::frames_` | `vector<unique_ptr<Page>>` — pool owns all pages |
| `BTree` objects | `unordered_map<string, unique_ptr<BTree>>` in Executor |
| `Catalog`, `BufferPool`, `DiskManager` | `unique_ptr` inside `Repl::Impl` |
| `Stmt` and `Expr` AST | `unique_ptr<Stmt>` / `unique_ptr<Expr>` — ownership is clear |
| `ResultSet` rows | `vector<Row>` value semantics — no manual allocation |

**No raw `new` or `delete` anywhere in the codebase.**

Page objects are never copied — `Page` has its copy constructor deleted.
Move semantics are used where needed (e.g. moving `Token` lists into the
`Parser`).

---

## 8. File Layout

```
minisql/
├── CMakeLists.txt           CMake build configuration
├── docs/                    Documentation
│   ├── README.md            Index and quick start
│   ├── sql-reference.md     Full SQL syntax reference
│   ├── storage.md           On-disk format and B+ tree internals
│   ├── query-processing.md  Lexer → Parser → Executor pipeline
│   ├── architecture.md      This file
│   ├── tutorials.md         Step-by-step guides
│   └── limitations.md       Known gaps and constraints
├── include/                 Headers (no .cpp code)
└── src/                     Implementation files
```

The build system produces two targets:

```
libminisql_lib.a    static library containing all modules
minisql             the executable (links against the library)
```

`main.cpp` is kept deliberately minimal.  It parses `argv`, constructs a
`Repl`, and calls `run()`.  All logic lives in the library.
