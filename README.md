# minisql Documentation

minisql is a relational database engine written from scratch in C++17.
It stores data in a disk-resident B+ tree, parses SQL with a hand-written
recursive-descent parser, and manages disk pages with an LRU buffer pool.

---

## Contents

| File | What it covers |
|---|---|
| [sql-reference.md](docs/sql-reference.md) | Every SQL statement and clause with full syntax and examples |
| [storage.md](docs/storage.md) | How data is physically stored on disk (pages, B+ tree, file formats) |
| [query-processing.md](docs/query-processing.md) | How a SQL string becomes a result: Lexer → Parser → Executor |
| [architecture.md](docs/architecture.md) | High-level module map, data-flow diagram, design principles |
| [tutorials.md](docs/tutorials.md) | Step-by-step walkthroughs for common tasks |
| [limitations.md](docs/limitations.md) | Known gaps, constraints, and things that are not yet supported |

---

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run (creates mydb.db and mydb.cat in the current directory)
./build/minisql mydb
```

```
minisql v1.0
minisql> CREATE TABLE users (id INTEGER NOT NULL, name TEXT NOT NULL, age INTEGER);
minisql> INSERT INTO users VALUES (1, 'Alice', 30);
minisql> SELECT * FROM users;
+----+-------+-----+
| id | name  | age |
+----+-------+-----+
| 1  | Alice | 30  |
+----+-------+-----+
1 row(s)
minisql> .exit
```

---

## The two database files

Every database is stored as exactly two binary files:

```
mydb.db    raw B+ tree pages  (actual row data)
mydb.cat   catalog             (table names, schemas, metadata)
```

Both files are created automatically. See [storage.md](storage.md) for the
full on-disk format of each file.
