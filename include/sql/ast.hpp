#pragma once
#include "common/types.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace minisql {

// ── Expression nodes ─────────────────────────────────────────────────────────

enum class ExprKind {
  LITERAL, // a Value constant
  COLUMN,  // column reference
  BINARY,  // binary op (AND, OR, =, <, …)
  UNARY,   // NOT expr
};

enum class BinOp {
  EQ,
  NEQ,
  LT,
  GT,
  LTE,
  GTE,
  AND,
  OR,
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
  ExprKind kind;

  // LITERAL
  Value literal_val;

  // COLUMN
  std::string col_name;

  // BINARY
  BinOp bin_op{};
  ExprPtr left;
  ExprPtr right;

  // UNARY (NOT)
  ExprPtr operand;

  Expr() = default;
  Expr(const Expr &) = delete;
  Expr &operator=(const Expr &) = delete;
  Expr(Expr &&) = default;
  Expr &operator=(Expr &&) = default;

  static ExprPtr make_literal(Value v);
  static ExprPtr make_column(std::string name);
  static ExprPtr make_binary(BinOp op, ExprPtr l, ExprPtr r);
  static ExprPtr make_not(ExprPtr operand);
};

// ── Statement nodes
// ───────────────────────────────────────────────────────────

enum class StmtKind {
  CREATE_TABLE,
  DROP_TABLE,
  INSERT,
  SELECT,
  DELETE,
  UPDATE,
};

struct ColumnDef {
  std::string name;
  DataType type{DataType::INTEGER};
  bool nullable{true};
};

struct AssignPair {
  std::string col;
  Value val;
};

struct Stmt {
  StmtKind kind;

  // CREATE TABLE / DROP TABLE
  std::string table_name;
  std::vector<ColumnDef> col_defs; // CREATE TABLE

  // INSERT
  std::vector<std::string> ins_cols;        // optional column list
  std::vector<std::vector<Value>> ins_rows; // one inner vector per VALUES row

  // SELECT
  bool select_star{false};
  std::vector<std::string> select_cols;
  ExprPtr where_expr; // nullptr = no WHERE
  std::string order_by_col;
  bool order_asc{true};
  std::optional<int64_t> limit;
  std::optional<int64_t> offset;

  // UPDATE
  std::vector<AssignPair> update_sets;

  // DELETE shares table_name + where_expr

  Stmt() = default;
  Stmt(const Stmt &) = delete;
  Stmt &operator=(const Stmt &) = delete;
  Stmt(Stmt &&) = default;
  Stmt &operator=(Stmt &&) = default;
};

} // namespace minisql
