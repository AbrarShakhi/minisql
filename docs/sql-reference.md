# SQL Reference

This document covers every SQL feature supported by minisql — syntax, rules,
examples, and what happens when something goes wrong.

---

## Table of Contents

1. [Data Types](#1-data-types)
2. [Literals](#2-literals)
3. [NULL](#3-null)
4. [Identifiers](#4-identifiers)
5. [CREATE TABLE](#5-create-table)
6. [DROP TABLE](#6-drop-table)
7. [INSERT](#7-insert)
8. [SELECT](#8-select)
   - 8.1 Column projection
   - 8.2 WHERE clause
   - 8.3 ORDER BY
   - 8.4 LIMIT and OFFSET
9. [UPDATE](#9-update)
10. [DELETE](#10-delete)
11. [Expressions](#11-expressions)
    - 11.1 Comparison operators
    - 11.2 Boolean operators
    - 11.3 NULL tests
    - 11.4 Operator precedence
12. [Meta-Commands](#12-meta-commands)
13. [Statement terminator](#13-statement-terminator)
14. [Comments](#14-comments)

---

## 1. Data Types

minisql has four storage types.  Every column must declare one of them.

```
INTEGER    64-bit signed integer  (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)
REAL       64-bit IEEE 754 floating-point  (double precision)
TEXT       Variable-length UTF-8 string
BLOB       Variable-length raw bytes
```

**Accepted aliases**

| You write | Stored as |
|---|---|
| `INTEGER`, `INT`, `BIGINT` | `INTEGER` |
| `REAL`, `FLOAT`, `DOUBLE` | `REAL` |
| `TEXT`, `VARCHAR`, `CHAR` | `TEXT` |
| `BLOB` | `BLOB` |

**Type coercion in comparisons**

When you compare an `INTEGER` value with a `REAL` value, the integer is
silently promoted to a double before the comparison.  Any other cross-type
comparison (e.g. `INTEGER` vs `TEXT`) raises an `ExecutionError`.

---

## 2. Literals

Literals are constant values written directly in SQL.

**Integer literal**

Any sequence of digits, optionally preceded by `-`.

```sql
42
-7
0
1000000
```

**Real literal**

Digits with a decimal point.

```sql
3.14
-0.5
100.0
```

**Text literal**

Surrounded by single quotes.  A literal single quote inside a string is
written as two single quotes `''`.

```sql
'hello'
'it''s a fine day'
'minisql'
''          -- empty string
```

**NULL literal**

```sql
NULL
```

---

## 3. NULL

`NULL` represents a missing or unknown value.

- Any column is nullable by default.
- Mark a column non-nullable with `NOT NULL` in `CREATE TABLE`.
- Arithmetic and comparisons with `NULL` always produce `NULL`,
  which is treated as **false** in a `WHERE` clause.
- Test for `NULL` with `IS NULL` or `IS NOT NULL` — never with `= NULL`.

```sql
-- These WHERE tests never match any row:
WHERE age  = NULL      -- wrong
WHERE age != NULL      -- wrong

-- Correct:
WHERE age IS NULL
WHERE age IS NOT NULL
```

---

## 4. Identifiers

Table names and column names are identifiers.

- May contain letters, digits, and underscores.
- Must start with a letter or underscore.
- **Case-sensitive**: `Users` and `users` are different tables.
- Maximum length: 64 characters.

---

## 5. CREATE TABLE

Creates a new, empty table.

### Syntax

```sql
CREATE TABLE table_name (
    column_name  type  [NOT NULL],
    column_name  type  [NOT NULL],
    ...
);
```

### Rules

- `table_name` must not already exist in the database.
- At least one column is required.
- Column names must be unique within the table.
- `NOT NULL` means that column can never hold a `NULL` value.
  Inserting `NULL` or omitting it in named-column `INSERT` raises an error.
- Every table receives a hidden internal `rowid` (`uint64_t`, auto-incremented
  from 1).  This is the B+ tree key and is invisible to SQL.

### Examples

```sql
-- Minimal
CREATE TABLE messages (body TEXT);

-- Typical e-commerce table
CREATE TABLE products (
    id       INTEGER NOT NULL,
    name     TEXT    NOT NULL,
    price    REAL    NOT NULL,
    stock    INTEGER,
    notes    TEXT
);

-- All four types
CREATE TABLE demo (
    count    INTEGER,
    measure  REAL,
    label    TEXT,
    raw      BLOB
);
```

### Errors

```
ExecutionError: Table already exists: products
```

---

## 6. DROP TABLE

Deletes a table and removes its schema from the catalog.

> The pages used by the table's B+ tree in the `.db` file are **not reclaimed**
> in this version — see [limitations.md](limitations.md).

### Syntax

```sql
DROP TABLE table_name;
```

### Example

```sql
DROP TABLE messages;
```

### Errors

```
ExecutionError: Table not found: messages
```

---

## 7. INSERT

Adds one or more rows to a table.

### Syntax — positional

Values are matched to columns in the order they were declared.

```sql
INSERT INTO table_name VALUES (val, val, ...), (val, val, ...), ...;
```

### Syntax — named columns

Only the listed columns receive values; all others default to `NULL`.

```sql
INSERT INTO table_name (col, col, ...) VALUES (val, val, ...), ...;
```

### Rules

- Positional: value count must exactly match column count.
- Named: column list and value list must be the same length.
- Attempting to store `NULL` into a `NOT NULL` column raises an error.
- Each row receives the next available `rowid` from the catalog counter
  (separate from any `id` column you declare yourself).
- Multiple value rows in one `INSERT` are supported.

### Examples

```sql
-- Single row, positional
INSERT INTO products VALUES (1, 'Widget', 9.99, 100, NULL);

-- Multiple rows in one statement
INSERT INTO products VALUES
    (2, 'Gadget', 19.99, 50, 'New arrival'),
    (3, 'Doohickey', 4.99, 200, NULL);

-- Named columns — stock and notes default to NULL
INSERT INTO products (id, name, price) VALUES (4, 'Thingamajig', 2.49);

-- Explicit NULL in a nullable column
INSERT INTO products VALUES (5, 'Whatsit', 0.99, NULL, NULL);
```

### Errors

```
ExecutionError: Table not found: products
ExecutionError: INSERT: value count (2) does not match column count (5)
ExecutionError: INSERT: column list / value list size mismatch
ExecutionError: NOT NULL constraint violated for column: name
```

---

## 8. SELECT

Reads rows from a table.

### Full syntax

```sql
SELECT  *  |  col, col, ...
FROM    table_name
[WHERE  expression]
[ORDER BY  col  [ASC | DESC]]
[LIMIT  n]
[OFFSET m];
```

---

### 8.1 Column projection

**Select all columns**

```sql
SELECT * FROM products;
```

Columns appear in the order they were declared in `CREATE TABLE`.

**Select specific columns**

```sql
SELECT name, price FROM products;
SELECT id, name FROM users;
```

Only the named columns appear in the result.  The order you list them
determines the output order.

---

### 8.2 WHERE clause

Filters rows.  Only rows where the expression evaluates to a non-null,
non-zero value are included in the result.

```sql
SELECT * FROM products WHERE price < 10.0;
SELECT * FROM users   WHERE age >= 18 AND age < 65;
SELECT * FROM users   WHERE name = 'Alice' OR name = 'Bob';
SELECT * FROM products WHERE stock IS NULL;
SELECT * FROM products WHERE stock IS NOT NULL AND price > 5.0;
SELECT * FROM users   WHERE NOT (active = 0);
```

See [section 11](#11-expressions) for the full expression reference.

---

### 8.3 ORDER BY

Sorts the result set.

```sql
-- Ascending (default when ASC/DESC is omitted)
SELECT * FROM products ORDER BY price;
SELECT * FROM products ORDER BY price ASC;

-- Descending
SELECT * FROM products ORDER BY price DESC;

-- Sort text alphabetically
SELECT * FROM users ORDER BY name ASC;
```

- `NULL` values sort **before** non-null values in ascending order.
- Only one column is supported in `ORDER BY`.

---

### 8.4 LIMIT and OFFSET

Control how many rows are returned.

```sql
-- At most 5 rows
SELECT * FROM products LIMIT 5;

-- Skip the first 10 rows, then take at most 10
SELECT * FROM products ORDER BY id ASC LIMIT 10 OFFSET 10;
```

`OFFSET` is only meaningful when combined with `ORDER BY`; without a
deterministic order, the rows you skip are unpredictable.

**Pagination pattern** — page size 20, page number P (0-indexed):

```sql
SELECT * FROM products ORDER BY id ASC LIMIT 20 OFFSET <P*20>;
```

---

## 9. UPDATE

Modifies column values in existing rows.

### Syntax

```sql
UPDATE table_name
SET  col = val, col = val, ...
[WHERE expression];
```

- Without `WHERE`, every row in the table is updated.
- Multiple `SET` assignments are separated by commas.
- The right-hand side of each `=` must be a **literal value** (`INTEGER`,
  `REAL`, `TEXT`, or `NULL`) — not a column reference or expression.

### Examples

```sql
-- Change one value for one row
UPDATE users SET age = 31 WHERE id = 1;

-- Change multiple columns at once
UPDATE products SET price = 9.49, stock = 150 WHERE id = 2;

-- Set a column to NULL
UPDATE users SET age = NULL WHERE id = 4;

-- Update every row (no WHERE)
UPDATE products SET stock = 0;
```

### How it works internally

UPDATE is implemented as **delete + re-insert** at the B+ tree level:

1. Scan all rows; collect those matching the `WHERE` predicate.
2. Delete each matching rowid from the B+ tree.
3. Re-insert with the same rowid and the new column values.

The rowid is preserved, so the row stays in the same logical position.

### Errors

```
ExecutionError: Table not found: users
SchemaError: UPDATE: unknown column: email
```

---

## 10. DELETE

Removes rows from a table.

### Syntax

```sql
DELETE FROM table_name [WHERE expression];
```

- Without `WHERE`, all rows are deleted (the table itself remains).

### Examples

```sql
-- Delete one row
DELETE FROM users WHERE id = 3;

-- Delete with compound condition
DELETE FROM products WHERE stock = 0 AND price < 1.0;

-- Delete rows where a column is NULL
DELETE FROM users WHERE age IS NULL;

-- Delete everything
DELETE FROM logs;
```

### Errors

```
ExecutionError: Table not found: logs
```

---

## 11. Expressions

Expressions appear after `WHERE` and on the right-hand side of `SET`.

An expression evaluates to a `Value` — one of `INTEGER`, `REAL`, `TEXT`,
`BLOB`, or `NULL`.

---

### 11.1 Comparison operators

| Operator | Meaning |
|---|---|
| `=` | Equal |
| `!=` or `<>` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Both sides of a comparison can be a column reference, a literal, or a
sub-expression wrapped in parentheses.

```sql
age = 30
price < 10.0
name != 'Bob'
score >= 3.5
```

---

### 11.2 Boolean operators

| Operator | Meaning |
|---|---|
| `AND` | Both sides must be true |
| `OR` | At least one side must be true |
| `NOT` | Inverts a boolean result |

```sql
age > 18 AND age < 65
name = 'Alice' OR name = 'Bob'
NOT (active = 0)
(a = 1 OR b = 2) AND c = 3
```

---

### 11.3 NULL tests

| Expression | Matches when |
|---|---|
| `col IS NULL` | The column value is NULL |
| `col IS NOT NULL` | The column value is not NULL |

These are the **only** correct ways to test for NULL.

```sql
WHERE notes IS NULL
WHERE notes IS NOT NULL
```

---

### 11.4 Operator precedence

From highest priority to lowest:

```
1. ( )               parentheses — override everything
2. NOT
3. = != <> < > <= >= IS IS NOT   comparisons
4. AND
5. OR
```

**Examples of precedence**

```sql
-- AND binds tighter than OR, so this is: (a=1 AND b=2) OR c=3
WHERE a = 1 AND b = 2 OR c = 3

-- Use parentheses to change the meaning:
WHERE a = 1 AND (b = 2 OR c = 3)

-- NOT applies to the immediate comparison:
WHERE NOT active = 0          -- same as: active != 0
WHERE NOT (a = 1 OR b = 2)   -- neither a=1 nor b=2
```

---

## 12. Meta-Commands

Meta-commands start with `.` and are handled by the REPL, not by the SQL
parser.  They do not need a semicolon.

| Command | What it does |
|---|---|
| `.help` | List all commands |
| `.tables` | List every table in the current database |
| `.schema` | Print `CREATE TABLE` SQL for every table |
| `.schema name` | Print `CREATE TABLE` SQL for one table |
| `.exit` | Flush all data to disk and quit |
| `.quit` | Alias for `.exit` |

### Examples

```
minisql> .tables
users
products

minisql> .schema users
CREATE TABLE users (
  id INTEGER NOT NULL,
  name TEXT NOT NULL,
  age INTEGER
);

minisql> .schema
CREATE TABLE users (
  ...
);
CREATE TABLE products (
  ...
);
```

---

## 13. Statement terminator

The semicolon `;` at the end of a SQL statement is **optional** when you type
a single statement.  The REPL accumulates input lines and sends them to the
parser once it sees a `;`.

```sql
-- These two are equivalent:
SELECT * FROM users
SELECT * FROM users;
```

Multi-line statements work because the REPL keeps appending lines until it
sees a `;`:

```
minisql> SELECT *
minisql> FROM users
minisql> WHERE age > 18;
```

---

## 14. Comments

Single-line comments start with `--` and extend to the end of the line.
The Lexer discards them before tokenisation.

```sql
-- This is a comment
SELECT * FROM users;   -- inline comment

-- Create the main table
CREATE TABLE users (
    id   INTEGER NOT NULL,  -- primary identifier
    name TEXT    NOT NULL
);
```

Block comments (`/* ... */`) are **not** supported.
