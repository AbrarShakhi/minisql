#pragma once
#include "btree/btree.hpp"
#include "catalog/catalog.hpp"
#include "executor/result_set.hpp"
#include "sql/ast.hpp"
#include "storage/buffer_pool.hpp"
#include <memory>
#include <string>

namespace minisql {

// Walks an AST Stmt and carries out the requested operation against the
// live catalog + B+ trees.  One Executor is shared for the entire session.
class Executor {
public:
  Executor(Catalog &catalog, BufferPool &bp);

  // Returns a ResultSet (may be empty for DDL/DML).
  // Throws ExecutionError / SchemaError on semantic problems.
  [[nodiscard]] ResultSet execute(const Stmt &stmt);

  // Accessors used by the REPL for meta-commands.
  [[nodiscard]] Catalog &catalog() noexcept { return catalog_; }
  [[nodiscard]] BufferPool &buffer_pool() noexcept { return bp_; }

private:
  ResultSet exec_create_table(const Stmt &s);
  ResultSet exec_drop_table(const Stmt &s);
  ResultSet exec_insert(const Stmt &s);
  ResultSet exec_select(const Stmt &s);
  ResultSet exec_delete(const Stmt &s);
  ResultSet exec_update(const Stmt &s);

  // Evaluate an expression against a row+schema.  Returns NullVal on NULL.
  [[nodiscard]] Value eval_expr(const Expr &expr, const Row &row,
                                const Schema &schema) const;

  // Returns true if the expression evaluates to a truthy (non-null, non-zero)
  // value.
  [[nodiscard]] bool eval_predicate(const Expr &expr, const Row &row,
                                    const Schema &schema) const;

  // Get or create the BTree for a table (lazy).
  BTree &get_or_open_tree(const std::string &table_name);

  Catalog &catalog_;
  BufferPool &bp_;

  // Open B+ trees keyed by table name (created on first access per session).
  std::unordered_map<std::string, std::unique_ptr<BTree>> trees_;
};

} // namespace minisql
