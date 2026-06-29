# Query Processing

This document explains exactly what happens between the moment you press Enter
and the moment results appear on screen.  Every stage is covered — the Lexer,
the Parser, the Executor, and the result printer.

---

## Table of Contents

1. [The Four-Stage Pipeline](#1-the-four-stage-pipeline)
2. [Stage 1 — Lexer](#2-stage-1--lexer)
3. [Stage 2 — Parser](#3-stage-2--parser)
4. [Stage 3 — Executor](#4-stage-3--executor)
   - 4.1 CREATE TABLE
   - 4.2 DROP TABLE
   - 4.3 INSERT
   - 4.4 SELECT
   - 4.5 UPDATE
   - 4.6 DELETE
5. [Expression Evaluation](#5-expression-evaluation)
6. [Stage 4 — Result Printer](#6-stage-4--result-printer)
7. [End-to-End Walkthrough](#7-end-to-end-walkthrough)
8. [Error Handling](#8-error-handling)

---

## 1. The Four-Stage Pipeline

Every SQL statement travels through four stages in sequence.

```
 ┌───────────────────────────────────┐
 │  User types a SQL string          │
 └──────────────┬────────────────────┘
                │  raw string
                ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 1 — LEXER  (src/sql/lexer.cpp)                │
 │  Splits the string into a flat list of tokens.       │
 └──────────────┬───────────────────────────────────────┘
                │  vector<Token>
                ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 2 — PARSER  (src/sql/parser.cpp)              │
 │  Builds an Abstract Syntax Tree (AST) from tokens.   │
 └──────────────┬───────────────────────────────────────┘
                │  unique_ptr<Stmt>
                ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 3 — EXECUTOR  (src/executor/executor.cpp)     │
 │  Walks the AST, calls the B+ tree and catalog,       │
 │  evaluates WHERE predicates, collects results.       │
 └──────────────┬───────────────────────────────────────┘
                │  ResultSet
                ▼
 ┌──────────────────────────────────────────────────────┐
 │  Stage 4 — RESULT PRINTER  (src/executor/result_set) │
 │  Formats and prints an ASCII table.                  │
 └──────────────────────────────────────────────────────┘
```

---

## 2. Stage 1 — Lexer

**Source file:** `src/sql/lexer.cpp`
**Input:** a raw SQL string
**Output:** `std::vector<Token>`

The Lexer reads the input string one character at a time and groups characters
into **tokens** — the smallest meaningful units of SQL.

Each `Token` has:
- A **type** (e.g. `KW_SELECT`, `IDENT`, `INTEGER_LIT`, `GT`)
- A **value** — the raw text of that token
- A **line number** — for error messages

### What the Lexer recognises

| Input | Token type | Token value |
|---|---|---|
| `SELECT` (case-insensitive) | `KW_SELECT` | `"SELECT"` |
| `WHERE` | `KW_WHERE` | `"WHERE"` |
| `AND` | `KW_AND` | `"AND"` |
| `NULL` | `KW_NULL` | `"NULL"` |
| `INTEGER` / `INT` / `BIGINT` | `KW_INTEGER` | `"INTEGER"` |
| `users` | `IDENT` | `"users"` |
| `age` | `IDENT` | `"age"` |
| `42` | `INTEGER_LIT` | `"42"` |
| `-7` | `INTEGER_LIT` | `"-7"` |
| `3.14` | `REAL_LIT` | `"3.14"` |
| `'Alice'` | `STRING_LIT` | `"Alice"` |
| `=` | `EQ` | `"="` |
| `!=` or `<>` | `NEQ` | `"!="` |
| `<=` | `LTE` | `"<="` |
| `(` | `LPAREN` | `"("` |
| `,` | `COMMA` | `","` |
| `*` | `STAR` | `"*"` |
| `;` | `SEMICOLON` | `";"` |
| `-- comment` | *(discarded)* | — |
| whitespace | *(discarded)* | — |

### Example

**Input:** `SELECT name FROM users WHERE age > 30;`

**Output token list:**

```
KW_SELECT   "SELECT"
IDENT       "name"
KW_FROM     "FROM"
IDENT       "users"
KW_WHERE    "WHERE"
IDENT       "age"
GT          ">"
INTEGER_LIT "30"
SEMICOLON   ";"
END_OF_FILE ""
```

### String literals

Single-quoted strings are read character-by-character.  The opening and
closing `'` are consumed and not included in the token value.  A `\\`
followed by any character is treated as an escape sequence (the backslash
is dropped).

### Case normalisation

All identifier characters are preserved as-is (case-sensitive).
All keyword characters are uppercased before looking up in the keyword table,
so `select`, `SELECT`, and `SeLeCt` all produce `KW_SELECT`.

---

## 3. Stage 2 — Parser

**Source file:** `src/sql/parser.cpp`
**Input:** `std::vector<Token>`
**Output:** `std::unique_ptr<Stmt>`

The Parser consumes the token list and builds an **Abstract Syntax Tree
(AST)**.  It uses the **recursive descent** technique: each grammar rule is
a C++ function that may call other functions for nested rules.

### The Stmt node

`Stmt` is the root of every AST.  It has a `StmtKind` field that identifies
which SQL statement it represents, and separate fields for every possible
clause (table name, column list, WHERE expression, ORDER BY, LIMIT, etc.).

```
StmtKind values:
  CREATE_TABLE
  DROP_TABLE
  INSERT
  SELECT
  UPDATE
  DELETE
```

### The Expr node

Expressions (WHERE, IS NULL, comparisons) are represented as a tree of
`Expr` nodes.

```
ExprKind values:
  LITERAL   — a constant Value (integer, real, text, null)
  COLUMN    — a column reference (holds the column name string)
  BINARY    — a binary operation (holds BinOp + left Expr + right Expr)
  UNARY     — NOT (holds one child Expr)
```

`BinOp` values:  `EQ  NEQ  LT  GT  LTE  GTE  AND  OR`

### Call tree for a SELECT statement

```
parse()
 └─ parse_select()
     ├─ consume KW_SELECT
     ├─ if STAR  → select_star = true
     │  else     → parse column names until no more commas
     ├─ consume KW_FROM
     ├─ read table_name (IDENT)
     ├─ if KW_WHERE
     │   └─ parse_expr()
     │       └─ parse_or()
     │           └─ parse_and()
     │               └─ parse_not()
     │                   └─ parse_comparison()
     │                       ├─ parse_primary() → left operand
     │                       ├─ consume comparison operator
     │                       └─ parse_primary() → right operand
     ├─ if KW_ORDER → consume KW_BY, read column, check ASC/DESC
     ├─ if KW_LIMIT  → parse integer literal
     ├─ if KW_OFFSET → parse integer literal
     └─ return Stmt
```

### Expression precedence in the parser

The precedence rules are encoded directly in the call hierarchy:

```
parse_expr()      ← entry point
  parse_or()      ← handles OR (lowest precedence)
    parse_and()   ← handles AND
      parse_not() ← handles NOT
        parse_comparison()  ← handles =, !=, <, >, <=, >=, IS
          parse_primary()   ← handles literals, column refs, ( expr )
```

Higher precedence = deeper in the call stack = evaluated first.

### AST example: `WHERE age > 28 AND name = 'Alice'`

```
BINARY(AND)
├── BINARY(GT)
│   ├── COLUMN("age")
│   └── LITERAL(Integer{28})
└── BINARY(EQ)
    ├── COLUMN("name")
    └── LITERAL(Text{"Alice"})
```

### AST example: `WHERE NOT (active = 0)`

```
UNARY(NOT)
└── BINARY(EQ)
    ├── COLUMN("active")
    └── LITERAL(Integer{0})
```

---

## 4. Stage 3 — Executor

**Source file:** `src/executor/executor.cpp`
**Input:** `const Stmt&`
**Output:** `ResultSet`

The Executor dispatches to the correct handler based on `Stmt::kind`, then
carries out the operation against the live catalog and B+ trees.

---

### 4.1 CREATE TABLE

```
1. Validate: table_name must not already exist in catalog
2. Validate: column definitions are well-formed
3. Call BufferPool::new_page() → get a new page ID
4. Initialise that page as an empty B+ tree leaf node
5. Unpin the page (mark dirty so it is written to disk)
6. Call Catalog::create_table(name, schema, root_page_id)
7. Catalog writes its .cat file to disk
8. Return empty ResultSet
```

---

### 4.2 DROP TABLE

```
1. Validate: table_name must exist in catalog
2. Remove the open BTree object from the executor's tree cache
3. Call Catalog::drop_table(name)
4. Catalog writes its .cat file to disk
5. Return empty ResultSet

Note: the pages in the .db file are NOT reclaimed (known limitation)
```

---

### 4.3 INSERT

```
1. Look up table_name in catalog; get schema and column list
2. For each value row in ins_rows:
   a. If ins_cols is empty (positional insert):
        validate value count == column count
        row = ins_rows[i] directly
      Else (named-column insert):
        validate ins_cols.size() == value_row.size()
        build row = make_null_row(ncols)
        for each (col_name, value) pair:
          look up column index in schema
          row[index] = value
   b. For each column: if NOT NULL and value is NULL → raise ExecutionError
   c. rowid = catalog.next_row_id(table_name)   ← increments counter
   d. buf   = serialise_row(row)                ← byte vector
   e. tree.insert(rowid, buf)
      ├── find_leaf(rowid)      ← walk internal nodes
      ├── if leaf has space → leaf_insert(rowid, buf)
      └── if leaf is full  → split_leaf, insert_into_parent (recursive)
3. BufferPool::flush_all()   ← write dirty pages to disk
4. Catalog::flush()          ← update next_row_id on disk
5. Return empty ResultSet
```

---

### 4.4 SELECT

```
1. Look up table_name in catalog; get schema
2. Determine projection:
     SELECT *       → all column indices [0, 1, 2, ...]
     SELECT col,..  → look up each name → get indices; error if unknown

3. Collect rows:
   tree.scan_all(callback):
     ├─ find leftmost leaf  (walk left-child pointers from root)
     └─ while leaf != null:
          for each cell in leaf (slot array order = rowid order):
            key = leaf_key(i)
            buf = leaf_value(i)
            row = deserialise_row(buf, col_count)
            if WHERE present:
              if eval_predicate(where_expr, row, schema) == false → skip
            else:
              add (key, row) to collected list
          next_leaf = leaf.next_leaf_page_id
          unpin current leaf; fetch next leaf

4. ORDER BY:
   if order_by_col is set:
     idx = schema.index_of(order_by_col)
     std::stable_sort(collected, compare by collected[i].row[idx])
     direction = ASC or DESC

5. OFFSET / LIMIT:
   start = max(0, offset)
   end   = min(collected.size(), start + limit)
   slice = collected[start .. end)

6. Project:
   for each (rowid, row) in slice:
     out_row = [row[col_indices[0]], row[col_indices[1]], ...]
     result_set.add_row(out_row)

7. Return ResultSet
```

---

### 4.5 UPDATE

```
1. Look up table_name; get schema
2. Validate: every SET column name exists in schema
3. Scan all rows with scan_all:
     for each row:
       if WHERE passes (or no WHERE):
         apply SET assignments: row[col_idx] = new_value
         add (rowid, new_row) to updates list
4. For each (rowid, new_row) in updates:
     tree.remove(rowid)           ← delete old cell from leaf
     buf = serialise_row(new_row)
     tree.insert(rowid, buf)      ← re-insert with same rowid
5. BufferPool::flush_all()
6. Return empty ResultSet
```

---

### 4.6 DELETE

```
1. Look up table_name; get schema
2. Scan all rows with scan_all:
     for each row:
       if WHERE passes (or no WHERE):
         add rowid to to_delete list
3. For each rowid in to_delete:
     tree.remove(rowid)
       ├── find_leaf(rowid)
       ├── binary search for rowid in slot array → position pos
       ├── shift slot array left: slot[pos..n-1] = slot[pos+1..n]
       └── num_cells -= 1; mark page dirty
4. BufferPool::flush_all()
5. Return empty ResultSet
```

---

## 5. Expression Evaluation

**Function:** `Executor::eval_expr(expr, row, schema)`

The evaluator recursively walks the `Expr` AST and returns a `Value`.

```
eval_expr(LITERAL):
  return expr.literal_val         ← the constant stored in the node

eval_expr(COLUMN "age"):
  idx = schema.index_of("age")    ← look up column position
  return row[idx]                 ← fetch the value from the current row

eval_expr(BINARY EQ, left, right):
  lv = eval_expr(left)
  rv = eval_expr(right)
  if lv is NULL or rv is NULL → return NULL
  cmp = compare_values(lv, rv)
  return Integer{cmp == 0 ? 1 : 0}

eval_expr(BINARY AND, left, right):
  lv = eval_expr(left)
  if lv is NULL → return NULL     ← short-circuit NULL
  lb = (lv is Integer and Integer != 0)
  if !lb → return Integer{0}      ← short-circuit false
  rv = eval_expr(right)
  if rv is NULL → return NULL
  rb = (rv is Integer and Integer != 0)
  return Integer{rb ? 1 : 0}

eval_expr(BINARY OR, left, right):
  lv = eval_expr(left)
  lb = (lv is Integer and Integer != 0)
  if lb → return Integer{1}       ← short-circuit true
  rv = eval_expr(right)
  rb = (rv is Integer and Integer != 0)
  return Integer{rb ? 1 : 0}

eval_expr(UNARY NOT, operand):
  v = eval_expr(operand)
  if v is NULL → return NULL
  b = (v is Integer and Integer != 0)
  return Integer{b ? 0 : 1}
```

**eval_predicate** calls `eval_expr` and converts the result to bool:

```
eval_predicate(expr, row, schema):
  v = eval_expr(expr, row, schema)
  if v is NULL    → false
  if v is Integer → Integer != 0
  if v is Real    → Real != 0.0
  otherwise       → false
```

### NULL propagation rules

| Expression | Result |
|---|---|
| `NULL = NULL` | NULL (not true) |
| `NULL > 5` | NULL (not true) |
| `NULL AND true` | NULL (not true) |
| `NULL OR true` | true |
| `NULL OR false` | false (since OR short-circuits on true) |
| `IS NULL` of a NULL value | true (special-cased in parser) |
| `IS NOT NULL` of a NULL value | false |

---

## 6. Stage 4 — Result Printer

**Source file:** `src/executor/result_set.cpp`

After the Executor returns a `ResultSet`, the REPL calls `result_set.print()`.

The printer:
1. Computes the display width of each column = max of (header length, longest value string).
2. Prints a top border line.
3. Prints the header row.
4. Prints a separator line.
5. Prints each data row.
6. Prints a bottom border line.
7. Prints the row count.

```
+----+---------+-----+
| id | name    | age |
+----+---------+-----+
| 1  | Alice   | 31  |
| 3  | Charlie | 35  |
+----+---------+-----+
2 row(s)
```

For DDL and DML statements (CREATE TABLE, DROP TABLE, INSERT, UPDATE, DELETE),
the result set has no columns and no rows.  The printer prints nothing — the
REPL simply shows the next prompt.

---

## 7. End-to-End Walkthrough

**Query:** `SELECT name FROM users WHERE age > 28 ORDER BY name ASC LIMIT 2;`

**Table state:**

```
rowid | id | name    | age
------+----+---------+----
  1   |  1 | Alice   | 31
  2   |  2 | Bob     | 25
  3   |  3 | Charlie | 35
```

---

### Lexer output (14 tokens)

```
KW_SELECT   INTEGER_LIT are not present here
IDENT(name) KW_FROM IDENT(users)
KW_WHERE IDENT(age) GT INTEGER_LIT(28)
KW_ORDER KW_BY IDENT(name) KW_ASC
KW_LIMIT INTEGER_LIT(2)
```

---

### Parser output (Stmt)

```
Stmt {
  kind        = SELECT
  table_name  = "users"
  select_cols = ["name"]
  where_expr  = BINARY(GT,
                  COLUMN("age"),
                  LITERAL(Integer{28}))
  order_by    = "name"
  order_asc   = true
  limit       = 2
}
```

---

### Executor: scan_all

Walk B+ tree leaves in rowid order:

```
rowid=1  → Row{1, "Alice", 31}
           eval WHERE: BINARY(GT, 31, 28) → cmp=1 > 0 → true  → keep
rowid=2  → Row{2, "Bob", 25}
           eval WHERE: BINARY(GT, 25, 28) → cmp=-1 > 0 → false → discard
rowid=3  → Row{3, "Charlie", 35}
           eval WHERE: BINARY(GT, 35, 28) → cmp=1 > 0 → true  → keep

Collected: [(1, Alice/31), (3, Charlie/35)]
```

---

### Executor: ORDER BY

Sort by `name` ASC:
```
"Alice" < "Charlie"  → already sorted
Result: [(1, Alice), (3, Charlie)]
```

---

### Executor: LIMIT

Take at most 2 rows → both rows qualify.

---

### Executor: projection

Select only `name` column (index 1):
```
ResultSet columns: ["name"]
ResultSet rows:    [["Alice"], ["Charlie"]]
```

---

### Printer output

```
+---------+
| name    |
+---------+
| Alice   |
| Charlie |
+---------+
2 row(s)
```

---

## 8. Error Handling

Errors are represented as C++ exceptions thrown anywhere in the pipeline and
caught in the REPL's `handle_sql()` function.

```cpp
// In repl.cpp:
try {
    auto tokens = Lexer(sql).tokenise();
    auto stmt   = Parser(tokens).parse();
    auto result = executor.execute(*stmt);
    result.print();
} catch (const DatabaseError& e) {
    std::cerr << "Error: " << e.what() << '\n';
}
```

The exception is printed to `stderr` and the REPL continues.  **A bad query
never crashes the engine.**

### Exception hierarchy

```
DatabaseError
├── StorageError    (disk I/O failures, buffer pool full)
├── ParseError      (bad SQL syntax)
├── ExecutionError  (semantic errors: table not found, type mismatch, etc.)
├── SchemaError     (unknown column name, bad type keyword)
└── NotFoundError   (table looked up but does not exist)
```

### Where each exception is thrown

| Stage | Exception | Condition |
|---|---|---|
| Lexer | `ParseError` | Unrecognised character |
| Parser | `ParseError` | Unexpected token, missing keyword |
| Executor | `ExecutionError` | Table not found, value count mismatch |
| Executor | `SchemaError` | Unknown column name |
| Executor | `ExecutionError` | NOT NULL violation |
| Executor | `ExecutionError` | Cross-type comparison |
| BTree | `StorageError` | Buffer pool exhausted |
| DiskManager | `StorageError` | fstream read/write failure |
