# Tutorials

Each tutorial is self-contained.  You can follow them in order or jump to
whichever is relevant.

---

## Tutorial 1 — Your First Database

**Goal:** create a database, add a table, insert rows, and query them.

### Step 1 — Start minisql

```bash
./build/minisql school
```

minisql creates two files — `school.db` and `school.cat` — and drops you
into the prompt:

```
minisql v1.0 – a SQLite-inspired database engine
Type .help for help, .exit to quit.

minisql>
```

### Step 2 — Create a table

```sql
CREATE TABLE students (
    id     INTEGER NOT NULL,
    name   TEXT    NOT NULL,
    grade  REAL,
    active INTEGER
);
```

minisql echoes a blank line and returns to the prompt.
The schema is saved to `school.cat`.

### Step 3 — Verify the table exists

```
minisql> .tables
students

minisql> .schema students
CREATE TABLE students (
  id INTEGER NOT NULL,
  name TEXT NOT NULL,
  grade REAL,
  active INTEGER
);
```

### Step 4 — Insert rows

```sql
INSERT INTO students VALUES (1, 'Alice',  3.9, 1);
INSERT INTO students VALUES (2, 'Bob',    3.5, 1);
INSERT INTO students VALUES (3, 'Carol',  3.7, 0);
INSERT INTO students VALUES (4, 'Dave',   NULL, 1);
```

### Step 5 — Read all rows

```sql
SELECT * FROM students;
```

```
+----+-------+-----+--------+
| id | name  | grade | active |
+----+-------+-------+--------+
| 1  | Alice | 3.900000 | 1  |
| 2  | Bob   | 3.500000 | 1  |
| 3  | Carol | 3.700000 | 0  |
| 4  | Dave  | NULL     | 1  |
+----+-------+-------+--------+
4 row(s)
```

### Step 6 — Exit and reopen

```
minisql> .exit
Goodbye.
```

```bash
./build/minisql school
```

```sql
SELECT * FROM students;
```

All four rows are still there.  The data survived the restart.

---

## Tutorial 2 — Filtering Rows with WHERE

**Goal:** use the `WHERE` clause to select specific rows.

Assuming the `students` table from Tutorial 1 is open.

### Simple comparisons

```sql
-- Students with a grade above 3.6
SELECT * FROM students WHERE grade > 3.6;

-- Exactly one student
SELECT * FROM students WHERE id = 1;

-- Students who are not active
SELECT * FROM students WHERE active = 0;
```

### Combining conditions with AND / OR

```sql
-- Active students with a high grade
SELECT name, grade FROM students WHERE active = 1 AND grade > 3.6;

-- Students named Alice or Bob
SELECT * FROM students WHERE name = 'Alice' OR name = 'Bob';

-- Active students OR high-grade students
SELECT * FROM students WHERE active = 1 OR grade > 3.8;
```

### Using NOT

```sql
-- Students who are not inactive
SELECT * FROM students WHERE NOT active = 0;

-- Everyone except Carol
SELECT * FROM students WHERE NOT name = 'Carol';
```

### Testing for NULL

```sql
-- Students with no grade recorded
SELECT * FROM students WHERE grade IS NULL;

-- Students who do have a grade
SELECT * FROM students WHERE grade IS NOT NULL;
```

> Never use `= NULL` — it will never match anything.

### Parentheses for grouping

```sql
-- Active students with either a high grade or a specific name
SELECT * FROM students
WHERE active = 1 AND (grade > 3.8 OR name = 'Bob');
```

---

## Tutorial 3 — Sorting, Limiting, and Pagination

**Goal:** control the order and quantity of results.

### ORDER BY

```sql
-- Alphabetical order
SELECT * FROM students ORDER BY name ASC;

-- Highest grade first
SELECT * FROM students ORDER BY grade DESC;

-- Combined with WHERE
SELECT name, grade
FROM students
WHERE active = 1
ORDER BY grade DESC;
```

### LIMIT — top N results

```sql
-- Single best student
SELECT name, grade FROM students ORDER BY grade DESC LIMIT 1;

-- Top 3
SELECT name, grade FROM students ORDER BY grade DESC LIMIT 3;
```

### OFFSET — skip rows

```sql
-- Skip the first 2 rows (the top 2 by grade), show the next 2
SELECT name, grade
FROM students
ORDER BY grade DESC
LIMIT 2 OFFSET 2;
```

### Pagination pattern

Page size = 2 rows, fetch page number P (0-indexed):

```sql
-- Page 0 (rows 1–2)
SELECT * FROM students ORDER BY id ASC LIMIT 2 OFFSET 0;

-- Page 1 (rows 3–4)
SELECT * FROM students ORDER BY id ASC LIMIT 2 OFFSET 2;

-- Page 2 (rows 5–6, empty if fewer than 5 rows)
SELECT * FROM students ORDER BY id ASC LIMIT 2 OFFSET 4;
```

---

## Tutorial 4 — Updating and Deleting Rows

**Goal:** modify and remove rows.

### UPDATE — change a single value

```sql
-- Alice got a better grade
UPDATE students SET grade = 4.0 WHERE id = 1;

SELECT * FROM students WHERE id = 1;
-- Shows: 1 | Alice | 4.000000 | 1
```

### UPDATE — change multiple columns at once

```sql
UPDATE students SET grade = 3.8, active = 0 WHERE name = 'Bob';
```

### UPDATE — change a column to NULL

```sql
UPDATE students SET grade = NULL WHERE id = 3;
```

### UPDATE — all rows (no WHERE)

```sql
-- Deactivate everyone
UPDATE students SET active = 0;
```

> No `WHERE` clause = every row is updated.  Double-check before running.

### DELETE — remove specific rows

```sql
-- Remove Dave
DELETE FROM students WHERE id = 4;

-- Remove all inactive students
DELETE FROM students WHERE active = 0;
```

### DELETE — remove all rows

```sql
DELETE FROM students;
-- Table is now empty, but still exists
SELECT * FROM students;   -- 0 row(s)
```

### DROP TABLE — remove the table entirely

```sql
DROP TABLE students;
.tables   -- students is gone
```

---

## Tutorial 5 — Working with Multiple Tables

minisql does not support `JOIN` yet, but you can run separate queries.

### Create related tables

```sql
CREATE TABLE departments (
    id   INTEGER NOT NULL,
    name TEXT    NOT NULL
);

CREATE TABLE employees (
    id      INTEGER NOT NULL,
    name    TEXT    NOT NULL,
    dept_id INTEGER,
    salary  REAL
);
```

### Insert data

```sql
INSERT INTO departments VALUES (1, 'Engineering');
INSERT INTO departments VALUES (2, 'Marketing');
INSERT INTO departments VALUES (3, 'Finance');

INSERT INTO employees VALUES (1, 'Alice', 1, 90000.0);
INSERT INTO employees VALUES (2, 'Bob',   1, 85000.0);
INSERT INTO employees VALUES (3, 'Carol', 2, 75000.0);
INSERT INTO employees VALUES (4, 'Dave',  3, 80000.0);
INSERT INTO employees VALUES (5, 'Eve',   1, 95000.0);
```

### Query one table at a time

```sql
-- All engineering employees
SELECT name, salary FROM employees WHERE dept_id = 1;

-- Employees earning over 80,000
SELECT name, salary FROM employees WHERE salary > 80000.0 ORDER BY salary DESC;

-- Employees with no department assigned
SELECT name FROM employees WHERE dept_id IS NULL;
```

### Cross-table workflows

```sql
-- Give all engineering employees a raise
UPDATE employees SET salary = salary * 1.1 WHERE dept_id = 1;
```

Wait — minisql does not support arithmetic expressions in `SET` (see
[limitations.md](limitations.md)).  You need to compute the value yourself:

```sql
-- Alice: 90000 × 1.1 = 99000
UPDATE employees SET salary = 99000.0 WHERE id = 1;

-- Bob: 85000 × 1.1 = 93500
UPDATE employees SET salary = 93500.0 WHERE id = 2;

-- Eve: 95000 × 1.1 = 104500
UPDATE employees SET salary = 104500.0 WHERE id = 5;
```

### Remove a whole department

```sql
-- Remove all marketing employees
DELETE FROM employees WHERE dept_id = 2;

-- Remove the department record
DELETE FROM departments WHERE id = 2;
```

---

## Tutorial 6 — Understanding the Storage Files

**Goal:** understand what is in `school.db` and `school.cat` after the
tutorials above.

### Check file sizes

```bash
ls -lh school.*
```

```
school.cat    ~80 bytes     (just schemas + metadata)
school.db     4096 bytes    (one 4096-byte page per table root so far)
```

### What is in school.cat?

The catalog stores:
- The name of each table ("students", "departments", "employees")
- For each table: which page in `school.db` is the B+ tree root
- The `next_row_id` counter (so the next `INSERT` gets the right rowid)
- All column definitions (name, type, nullable flag)

It is rewritten from scratch on every data change.

### What is in school.db?

Each table's data is stored in one or more 4096-byte pages.  With only a
handful of rows, each table fits in a single leaf page.

```
Page 0  → B+ tree root for "students"   (leaf node, holds student rows)
Page 1  → B+ tree root for "departments"
Page 2  → B+ tree root for "employees"
```

As you insert more rows, a leaf fills up and **splits**.  The split allocates
a new page, and the file grows by exactly 4096 bytes.

### What does a leaf page look like?

```
Bytes 0–16   Header
  byte 0:    node_type = 1  (LEAF)
  bytes 1-4: num_cells  (number of live rows in this page)
  bytes 5-8: parent_page_id  (INVALID if this is the root)
  bytes 9-12:next_leaf_page_id  (INVALID if this is the only/last leaf)
  bytes 13-16: free_end  (where the cell heap starts)

Bytes 17–...  Slot array  (4 bytes per row, sorted by rowid)
  slot[0] = offset of first row's cell
  slot[1] = offset of second row's cell
  ...

... free space ...

... cell heap (row data packed from the end of the page backward) ...
  cell: [rowid:8 bytes][val_len:4 bytes][serialised row:val_len bytes]
```

See [storage.md](storage.md) for the full annotated hex dump.

---

## Tutorial 7 — Data Types in Practice

### INTEGER

```sql
CREATE TABLE counters (n INTEGER);
INSERT INTO counters VALUES (0);
INSERT INTO counters VALUES (-1000000);
INSERT INTO counters VALUES (9223372036854775807);  -- max int64
SELECT * FROM counters;
```

### REAL

```sql
CREATE TABLE measurements (val REAL);
INSERT INTO measurements VALUES (3.14159265358979);
INSERT INTO measurements VALUES (-0.000001);
INSERT INTO measurements VALUES (1000000.5);
SELECT * FROM measurements WHERE val > 0.0;
```

### TEXT

```sql
CREATE TABLE words (word TEXT NOT NULL);
INSERT INTO words VALUES ('hello');
INSERT INTO words VALUES ('it''s a test');   -- escaped single quote
INSERT INTO words VALUES ('');               -- empty string
SELECT * FROM words ORDER BY word ASC;
```

### BLOB

BLOB values are stored as raw bytes.  In SQL you provide them as TEXT
literals; the engine stores the raw bytes of the UTF-8 string.

```sql
CREATE TABLE raw_data (payload BLOB);
INSERT INTO raw_data VALUES ('binary content here');
SELECT * FROM raw_data;
```

### NULL handling

```sql
CREATE TABLE nullable_demo (a INTEGER, b TEXT);
INSERT INTO nullable_demo VALUES (1, 'set');
INSERT INTO nullable_demo VALUES (NULL, 'set');
INSERT INTO nullable_demo VALUES (1, NULL);
INSERT INTO nullable_demo (a) VALUES (2);    -- b defaults to NULL

SELECT * FROM nullable_demo WHERE a IS NULL;
SELECT * FROM nullable_demo WHERE b IS NOT NULL;
```

---

## Tutorial 8 — Stress Test: Many Rows

Insert enough rows to trigger B+ tree page splits.

```sql
CREATE TABLE numbers (n INTEGER NOT NULL, label TEXT NOT NULL);
```

Paste all of this at once (the REPL accumulates input until it sees `;`):

```sql
INSERT INTO numbers VALUES
  (1,'one'),(2,'two'),(3,'three'),(4,'four'),(5,'five'),
  (6,'six'),(7,'seven'),(8,'eight'),(9,'nine'),(10,'ten'),
  (11,'eleven'),(12,'twelve'),(13,'thirteen'),(14,'fourteen'),(15,'fifteen'),
  (16,'sixteen'),(17,'seventeen'),(18,'eighteen'),(19,'nineteen'),(20,'twenty');
```

Or use a loop in your shell:

```bash
for i in $(seq 1 500); do
  echo "INSERT INTO numbers VALUES ($i, 'label_$i');" | ./build/minisql mydb
done
```

Then query:

```sql
SELECT * FROM numbers WHERE n > 490 ORDER BY n ASC;
SELECT * FROM numbers ORDER BY n DESC LIMIT 10;
```

After many inserts the `.db` file will be several pages large — the B+ tree
will have split and may have internal nodes in addition to leaves.  All
queries continue to work correctly.
