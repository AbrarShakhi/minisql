# Limitations

This document is an honest, complete list of everything minisql does not
support, the consequences of each gap, and (where applicable) how to work
around it today.

---

## Table of Contents

1. [SQL features not supported](#1-sql-features-not-supported)
2. [Storage and durability](#2-storage-and-durability)
3. [Performance](#3-performance)
4. [Identifier and size limits](#4-identifier-and-size-limits)
5. [Crash safety](#5-crash-safety)

---

## 1. SQL Features Not Supported

### JOIN

minisql has no `JOIN` keyword.  You cannot query across two tables in a
single statement.

**Workaround:** run two separate `SELECT` queries and combine the results in
your application code.

```sql
-- Not supported:
SELECT e.name, d.name
FROM employees e
JOIN departments d ON e.dept_id = d.id;

-- Workaround: run separately
SELECT id, name FROM departments;
SELECT name, dept_id FROM employees;
-- Then match dept_id to id in your application.
```

---

### Subqueries

Nested `SELECT` inside a `WHERE` or `FROM` clause is not supported.

```sql
-- Not supported:
SELECT * FROM employees WHERE salary > (SELECT AVG(salary) FROM employees);
SELECT * FROM (SELECT name FROM users WHERE active = 1) AS sub;
```

---

### Aggregate functions

`COUNT`, `SUM`, `AVG`, `MIN`, `MAX` are not implemented.

```sql
-- Not supported:
SELECT COUNT(*) FROM users;
SELECT MAX(salary) FROM employees;
SELECT AVG(grade) FROM students WHERE active = 1;
```

**Workaround:** fetch all rows and compute aggregates in your application code.

---

### GROUP BY / HAVING

```sql
-- Not supported:
SELECT dept_id, COUNT(*) FROM employees GROUP BY dept_id;
SELECT dept_id, AVG(salary) FROM employees GROUP BY dept_id HAVING AVG(salary) > 80000;
```

---

### DISTINCT

```sql
-- Not supported:
SELECT DISTINCT dept_id FROM employees;
```

**Workaround:** fetch all rows and deduplicate in your application code.

---

### IN ( ... )

```sql
-- Not supported:
SELECT * FROM users WHERE id IN (1, 3, 5);
```

**Workaround:** use `OR`:

```sql
SELECT * FROM users WHERE id = 1 OR id = 3 OR id = 5;
```

---

### BETWEEN

```sql
-- Not supported:
SELECT * FROM products WHERE price BETWEEN 5.0 AND 20.0;
```

**Workaround:**

```sql
SELECT * FROM products WHERE price >= 5.0 AND price <= 20.0;
```

---

### LIKE / pattern matching

```sql
-- Not supported:
SELECT * FROM users WHERE name LIKE 'Ali%';
```

**Workaround:** fetch all rows and filter with `std::string::find` in your
application code.

---

### Arithmetic in expressions

You cannot do arithmetic in `WHERE`, `SELECT`, or `SET`.

```sql
-- Not supported in WHERE:
SELECT * FROM products WHERE price * 0.9 < 10.0;

-- Not supported in SET:
UPDATE employees SET salary = salary * 1.1 WHERE dept_id = 1;

-- Not supported in SELECT:
SELECT price * quantity AS total FROM order_lines;
```

**Workaround:** compute the value in your application and pass the result:

```sql
-- If salary is 90000 and you want +10%:
UPDATE employees SET salary = 99000.0 WHERE id = 1;
```

---

### Column aliases

```sql
-- Not supported:
SELECT name AS employee_name, salary AS pay FROM employees;
```

---

### Secondary indexes

minisql has no `CREATE INDEX`.  Every `SELECT` with a `WHERE` clause
performs a **full table scan** — it visits every row in the B+ tree.

This is fast enough for small tables but becomes slow as row count grows.

---

### Transactions

No `BEGIN`, `COMMIT`, or `ROLLBACK`.  Every statement auto-commits
immediately and cannot be undone.

```sql
-- Not supported:
BEGIN;
DELETE FROM users WHERE id = 1;
ROLLBACK;  -- can't undo
```

---

### Constraints

| Constraint | Supported? |
|---|---|
| `NOT NULL` | Yes |
| `UNIQUE` | No |
| `PRIMARY KEY` syntax | No (the hidden rowid is the only primary key) |
| `FOREIGN KEY` | No |
| `CHECK (expr)` | No |
| `DEFAULT value` | No (omitted columns always default to NULL) |

---

### ALTER TABLE

You cannot add, remove, or rename columns after a table is created.

```sql
-- Not supported:
ALTER TABLE users ADD COLUMN email TEXT;
ALTER TABLE users DROP COLUMN age;
ALTER TABLE users RENAME TO accounts;
```

**Workaround:**
1. Create a new table with the desired schema.
2. Insert rows: `INSERT INTO new_table SELECT ... FROM old_table;`
   (only works if the columns match — no JOIN or computed columns).
3. Drop the old table.

---

### Multi-statement input

You cannot send two SQL statements separated by `;` in one go.

```
-- Not supported:
minisql> INSERT INTO users VALUES (1, 'Alice', 30); SELECT * FROM users;
```

Send one statement at a time.  Piping a file with one statement per line
works because the REPL accumulates input per line until it sees a `;`.

---

### Scientific notation in real literals

```sql
-- Not supported:
INSERT INTO data VALUES (1.5e10);
INSERT INTO data VALUES (6.022e23);
```

**Workaround:** write the full decimal:

```sql
INSERT INTO data VALUES (15000000000.0);
```

---

### Hexadecimal integer literals

```sql
-- Not supported:
INSERT INTO flags VALUES (0xFF);
```

**Workaround:** use the decimal equivalent:

```sql
INSERT INTO flags VALUES (255);
```

---

## 2. Storage and Durability

### Page reclamation after DROP TABLE

When you `DROP TABLE`, the B+ tree pages that held that table's data are
**not freed** inside the `.db` file.  The file does not shrink.  Those bytes
remain on disk as dead space.

**Impact:** repeated create-and-drop cycles will grow the `.db` file without
bound.

**Workaround:** if the `.db` file has grown too large, recreate the database
from scratch (export data, delete both files, reimport).

---

### Dead space from DELETE and UPDATE

Deleted and updated cells leave dead bytes in the leaf pages.  Dead space is
reclaimed only when the page fills up and must split (the split rebuilds the
page from scratch using only live cells).

**Impact:** a table that has many updates/deletes may use more pages than a
freshly-loaded table of the same data.

---

### No overflow pages

A single serialised row must fit entirely within one page (4096 bytes minus
the leaf header and slot overhead).  The maximum usable payload per cell is
approximately **4059 bytes**.

If you store a `TEXT` or `BLOB` value longer than ~4 KB, the insert will
fail.

**Workaround:** keep individual field values under a few kilobytes.

---

### Single-user access

Only one process should open the same `.db` / `.cat` pair at a time.  There
is no file locking.  Two concurrent processes will corrupt each other's data.

---

## 3. Performance

### Full table scan for every SELECT

Without secondary indexes, every `SELECT … WHERE …` must visit every row.
For a table with N rows, this is O(N) — it does not get faster with larger
tables.

**Impact:**

| Rows | Approximate scan time |
|---|---|
| 1,000 | Negligible |
| 100,000 | Noticeable |
| 1,000,000 | Slow |

---

### ORDER BY is always in-memory

All rows matching the `WHERE` predicate are collected into a `std::vector`,
then sorted with `std::stable_sort`.  For large result sets, this uses
significant RAM.

---

### No query planner

minisql has no query optimiser.  It always does the same thing regardless
of what the query looks like.  There is no cost estimation, no index
selection, no reordering of predicates.

---

### Buffer pool size is fixed

The buffer pool holds 128 pages (512 KB) in RAM.  This is enough for small
databases.  For large databases with many concurrent page accesses, the pool
will evict and reload pages frequently (cache thrashing).

---

## 4. Identifier and Size Limits

| Item | Limit |
|---|---|
| Table name length | 64 bytes |
| Column name length | 64 bytes |
| Text / BLOB field length | ~4059 bytes (must fit in one page) |
| Columns per table | No hard limit; practically ~30–40 before catalog page overflows |
| Tables per database | No hard limit; practically hundreds before catalog page overflows |
| Rows per table | No hard limit; limited only by disk space |
| Max rowid | 2^64 − 1 (18,446,744,073,709,551,615) |

---

## 5. Crash Safety

### No write-ahead log (WAL)

minisql does not implement a Write-Ahead Log.  Dirty pages are held in the
buffer pool and flushed to disk in bulk on `.exit` and after each DML
statement.

**If the process is killed between a write and a flush** (e.g. `kill -9`,
power loss, OS crash), the `.db` file may be partially written.  There is no
recovery mechanism.

### No checksums

Page contents are not checksummed.  Disk errors or partial writes are
silently ignored.  Corrupt data will be read back and may cause incorrect
query results or runtime errors.

### Safe shutdown procedure

Always use `.exit` or `.quit` to close minisql.  This ensures:
1. All dirty buffer pool pages are flushed to disk.
2. The catalog (with updated rowid counters) is written.
3. The file streams are properly closed.

Never kill the process with `kill -9` or close the terminal abruptly while
the database is open.
