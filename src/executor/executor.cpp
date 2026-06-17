#include "executor/executor.hpp"
#include "common/error.hpp"
#include "executor/row.hpp"
#include <algorithm>
#include <stdexcept>

namespace minisql {

Executor::Executor(Catalog &catalog, BufferPool &bp)
    : catalog_(catalog), bp_(bp) {}

ResultSet Executor::execute(const Stmt &stmt) {
  switch (stmt.kind) {
  case StmtKind::CREATE_TABLE:
    return exec_create_table(stmt);
  case StmtKind::DROP_TABLE:
    return exec_drop_table(stmt);
  case StmtKind::INSERT:
    return exec_insert(stmt);
  case StmtKind::SELECT:
    return exec_select(stmt);
  case StmtKind::DELETE:
    return exec_delete(stmt);
  case StmtKind::UPDATE:
    return exec_update(stmt);
  }
  throw ExecutionError("Unknown statement kind");
}

// ── DDL
// ───────────────────────────────────────────────────────────────────────

ResultSet Executor::exec_create_table(const Stmt &s) {
  if (catalog_.table_exists(s.table_name))
    throw ExecutionError("Table already exists: " + s.table_name);

  std::vector<Column> cols;
  for (const auto &cd : s.col_defs) {
    cols.emplace_back(cd.name, cd.type, cd.nullable);
  }
  Schema schema(cols);

  // Allocate a root page for the new B+ tree.
  PageId root_id{};
  Page *root_page = bp_.new_page(root_id);
  if (!root_page)
    throw StorageError("Cannot allocate page for new table");
  BTreeNode root_node(root_page);
  root_node.init_leaf();
  bp_.unpin_page(root_id, true);

  catalog_.create_table(s.table_name, schema, root_id);

  ResultSet rs;
  return rs;
}

ResultSet Executor::exec_drop_table(const Stmt &s) {
  if (!catalog_.table_exists(s.table_name))
    throw ExecutionError("Table not found: " + s.table_name);
  trees_.erase(s.table_name);
  catalog_.drop_table(s.table_name);
  ResultSet rs;
  return rs;
}

// ── DML: INSERT
// ───────────────────────────────────────────────────────────────

ResultSet Executor::exec_insert(const Stmt &s) {
  if (!catalog_.table_exists(s.table_name))
    throw ExecutionError("Table not found: " + s.table_name);

  const TableMeta &meta = catalog_.get_table(s.table_name);
  const Schema &schema = meta.schema;
  size_t ncols = schema.col_count();

  BTree &tree = get_or_open_tree(s.table_name);

  for (const auto &raw_row : s.ins_rows) {
    Row row;
    if (s.ins_cols.empty()) {
      // Positional insert.
      if (raw_row.size() != ncols)
        throw ExecutionError(
            "INSERT: value count (" + std::to_string(raw_row.size()) +
            ") does not match column count (" + std::to_string(ncols) + ")");
      row = raw_row;
    } else {
      // Named-column insert.
      row = make_null_row(ncols);
      if (s.ins_cols.size() != raw_row.size())
        throw ExecutionError("INSERT: column list / value list size mismatch");
      for (size_t i = 0; i < s.ins_cols.size(); ++i) {
        auto idx = schema.index_of(s.ins_cols[i]);
        if (!idx)
          throw SchemaError("Unknown column: " + s.ins_cols[i]);
        row[*idx] = raw_row[i];
      }
    }

    // NOT NULL check.
    for (size_t i = 0; i < ncols; ++i) {
      if (!schema.column(i).nullable && is_null(row[i]))
        throw ExecutionError("NOT NULL constraint violated for column: " +
                             schema.column(i).name);
    }

    uint64_t rowid = catalog_.next_row_id(s.table_name);
    auto buf = serialise_row(row);
    if (!tree.insert(rowid, buf))
      throw ExecutionError("INSERT: duplicate rowid " + std::to_string(rowid));
  }
  bp_.flush_all();
  catalog_.flush();
  ResultSet rs;
  return rs;
}

// ── DML: SELECT
// ───────────────────────────────────────────────────────────────

ResultSet Executor::exec_select(const Stmt &s) {
  if (!catalog_.table_exists(s.table_name))
    throw ExecutionError("Table not found: " + s.table_name);

  const TableMeta &meta = catalog_.get_table(s.table_name);
  const Schema &schema = meta.schema;

  // Determine output columns.
  std::vector<size_t> col_indices;
  std::vector<std::string> col_names_out;

  if (s.select_star) {
    for (size_t i = 0; i < schema.col_count(); ++i) {
      col_indices.push_back(i);
      col_names_out.push_back(schema.column(i).name);
    }
  } else {
    for (const auto &name : s.select_cols) {
      auto idx = schema.index_of(name);
      if (!idx)
        throw SchemaError("Unknown column: " + name);
      col_indices.push_back(*idx);
      col_names_out.push_back(name);
    }
  }

  ResultSet rs(col_names_out);
  BTree &tree = get_or_open_tree(s.table_name);
  size_t ncols = schema.col_count();

  // Collect all matching rows.
  std::vector<std::pair<uint64_t, Row>> collected;
  tree.scan_all([&](uint64_t rowid, const std::vector<uint8_t> &buf) {
    Row row = deserialise_row(buf.data(), buf.size(), ncols);
    if (s.where_expr && !eval_predicate(*s.where_expr, row, schema))
      return;
    collected.emplace_back(rowid, std::move(row));
  });

  // ORDER BY.
  if (!s.order_by_col.empty()) {
    auto order_idx = schema.index_of(s.order_by_col);
    if (!order_idx)
      throw SchemaError("ORDER BY: unknown column: " + s.order_by_col);
    bool asc = s.order_asc;
    size_t oi = *order_idx;
    std::stable_sort(collected.begin(), collected.end(),
                     [&](const auto &a, const auto &b) {
                       int cmp = compare_values(a.second[oi], b.second[oi]);
                       return asc ? cmp < 0 : cmp > 0;
                     });
  }

  // OFFSET / LIMIT.
  size_t start = 0, end = collected.size();
  if (s.offset)
    start = static_cast<size_t>(std::max<int64_t>(0, *s.offset));
  if (s.limit)
    end = std::min(end,
                   start + static_cast<size_t>(std::max<int64_t>(0, *s.limit)));

  for (size_t i = start; i < end && i < collected.size(); ++i) {
    const Row &row = collected[i].second;
    Row out;
    for (size_t ci : col_indices) {
      out.push_back(row[ci]);
    }
    rs.add_row(std::move(out));
  }
  return rs;
}

// ── DML: DELETE
// ───────────────────────────────────────────────────────────────

ResultSet Executor::exec_delete(const Stmt &s) {
  if (!catalog_.table_exists(s.table_name))
    throw ExecutionError("Table not found: " + s.table_name);

  const TableMeta &meta = catalog_.get_table(s.table_name);
  const Schema &schema = meta.schema;
  size_t ncols = schema.col_count();
  BTree &tree = get_or_open_tree(s.table_name);

  std::vector<uint64_t> to_delete;
  tree.scan_all([&](uint64_t rowid, const std::vector<uint8_t> &buf) {
    Row row = deserialise_row(buf.data(), buf.size(), ncols);
    if (!s.where_expr || eval_predicate(*s.where_expr, row, schema)) {
      to_delete.push_back(rowid);
    }
  });
  for (uint64_t rowid : to_delete) {
    tree.remove(rowid);
  }
  bp_.flush_all();
  ResultSet rs;
  return rs;
}

// ── DML: UPDATE
// ───────────────────────────────────────────────────────────────

ResultSet Executor::exec_update(const Stmt &s) {
  if (!catalog_.table_exists(s.table_name))
    throw ExecutionError("Table not found: " + s.table_name);

  const TableMeta &meta = catalog_.get_table(s.table_name);
  const Schema &schema = meta.schema;
  size_t ncols = schema.col_count();
  BTree &tree = get_or_open_tree(s.table_name);

  // Validate SET column names.
  for (const auto &ap : s.update_sets) {
    if (!schema.index_of(ap.col))
      throw SchemaError("UPDATE: unknown column: " + ap.col);
  }

  std::vector<std::pair<uint64_t, Row>> updates;
  tree.scan_all([&](uint64_t rowid, const std::vector<uint8_t> &buf) {
    Row row = deserialise_row(buf.data(), buf.size(), ncols);
    if (!s.where_expr || eval_predicate(*s.where_expr, row, schema)) {
      for (const auto &ap : s.update_sets) {
        auto idx = schema.index_of(ap.col);
        row[*idx] = ap.val;
      }
      updates.emplace_back(rowid, std::move(row));
    }
  });

  for (auto &[rowid, row] : updates) {
    tree.remove(rowid);
    auto buf = serialise_row(row);
    tree.insert(rowid, buf);
  }
  bp_.flush_all();
  ResultSet rs;
  return rs;
}

// ── Expression evaluator
// ──────────────────────────────────────────────────────

Value Executor::eval_expr(const Expr &expr, const Row &row,
                          const Schema &schema) const {
  switch (expr.kind) {
  case ExprKind::LITERAL:
    return expr.literal_val;

  case ExprKind::COLUMN: {
    auto idx = schema.index_of(expr.col_name);
    if (!idx)
      throw ExecutionError("Unknown column: " + expr.col_name);
    return row[*idx];
  }

  case ExprKind::BINARY: {
    if (expr.bin_op == BinOp::AND) {
      Value lv = eval_expr(*expr.left, row, schema);
      if (is_null(lv))
        return NullVal{};
      bool lb =
          std::holds_alternative<Integer>(lv) && std::get<Integer>(lv) != 0;
      if (!lb)
        return Integer{0};
      Value rv = eval_expr(*expr.right, row, schema);
      if (is_null(rv))
        return NullVal{};
      bool rb =
          std::holds_alternative<Integer>(rv) && std::get<Integer>(rv) != 0;
      return Integer{rb ? 1 : 0};
    }
    if (expr.bin_op == BinOp::OR) {
      Value lv = eval_expr(*expr.left, row, schema);
      bool lb =
          std::holds_alternative<Integer>(lv) && std::get<Integer>(lv) != 0;
      if (lb)
        return Integer{1};
      Value rv = eval_expr(*expr.right, row, schema);
      bool rb =
          std::holds_alternative<Integer>(rv) && std::get<Integer>(rv) != 0;
      return Integer{rb ? 1 : 0};
    }
    Value lv = eval_expr(*expr.left, row, schema);
    Value rv = eval_expr(*expr.right, row, schema);
    if (is_null(lv) || is_null(rv))
      return NullVal{};
    int cmp = compare_values(lv, rv);
    bool result{};
    switch (expr.bin_op) {
    case BinOp::EQ:
      result = (cmp == 0);
      break;
    case BinOp::NEQ:
      result = (cmp != 0);
      break;
    case BinOp::LT:
      result = (cmp < 0);
      break;
    case BinOp::GT:
      result = (cmp > 0);
      break;
    case BinOp::LTE:
      result = (cmp <= 0);
      break;
    case BinOp::GTE:
      result = (cmp >= 0);
      break;
    default:
      result = false;
      break;
    }
    return Integer{result ? 1 : 0};
  }

  case ExprKind::UNARY: {
    Value v = eval_expr(*expr.operand, row, schema);
    if (is_null(v))
      return NullVal{};
    bool b = std::holds_alternative<Integer>(v) && std::get<Integer>(v) != 0;
    return Integer{b ? 0 : 1};
  }
  }
  return NullVal{};
}

bool Executor::eval_predicate(const Expr &expr, const Row &row,
                              const Schema &schema) const {
  Value v = eval_expr(expr, row, schema);
  if (is_null(v))
    return false;
  if (std::holds_alternative<Integer>(v))
    return std::get<Integer>(v) != 0;
  if (std::holds_alternative<Real>(v))
    return std::get<Real>(v) != 0.0;
  return false;
}

BTree &Executor::get_or_open_tree(const std::string &table_name) {
  auto it = trees_.find(table_name);
  if (it != trees_.end())
    return *it->second;

  const TableMeta &meta = catalog_.get_table(table_name);
  auto tree = std::make_unique<BTree>(bp_, meta.root_page_id);
  trees_[table_name] = std::move(tree);
  return *trees_[table_name];
}

} // namespace minisql
