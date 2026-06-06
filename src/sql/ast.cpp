#include "sql/ast.hpp"
#include <utility>

namespace minisql {

ExprPtr Expr::make_literal(Value v) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::LITERAL;
  e->literal_val = std::move(v);
  return e;
}

ExprPtr Expr::make_column(std::string name) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::COLUMN;
  e->col_name = std::move(name);
  return e;
}

ExprPtr Expr::make_binary(BinOp op, ExprPtr l, ExprPtr r) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::BINARY;
  e->bin_op = op;
  e->left = std::move(l);
  e->right = std::move(r);
  return e;
}

ExprPtr Expr::make_not(ExprPtr operand) {
  auto e = std::make_unique<Expr>();
  e->kind = ExprKind::UNARY;
  e->operand = std::move(operand);
  return e;
}

} // namespace minisql
