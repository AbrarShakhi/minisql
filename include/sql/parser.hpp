#pragma once
#include "sql/ast.hpp"
#include "sql/token.hpp"
#include <memory>
#include <vector>

namespace minisql {

// Recursive-descent parser.  Consumes the token list produced by Lexer and
// returns a single Stmt (one SQL statement per call).
// Throws ParseError for syntax violations.
class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  [[nodiscard]] std::unique_ptr<Stmt> parse();

private:
  // ── Statement parsers ────────────────────────────────────────────────────
  std::unique_ptr<Stmt> parse_create_table();
  std::unique_ptr<Stmt> parse_drop_table();
  std::unique_ptr<Stmt> parse_insert();
  std::unique_ptr<Stmt> parse_select();
  std::unique_ptr<Stmt> parse_delete();
  std::unique_ptr<Stmt> parse_update();

  // ── Expression parsers (precedence climbing) ─────────────────────────────
  ExprPtr parse_expr();
  ExprPtr parse_or();
  ExprPtr parse_and();
  ExprPtr parse_not();
  ExprPtr parse_comparison();
  ExprPtr parse_primary();

  // ── Value / type helpers ─────────────────────────────────────────────────
  Value parse_literal_value();
  DataType parse_type_keyword();
  ColumnDef parse_column_def();

  // ── Token stream helpers ─────────────────────────────────────────────────
  [[nodiscard]] const Token &peek() const noexcept;
  [[nodiscard]] const Token &peek2() const noexcept;
  const Token &advance();
  const Token &expect(TokenType t);
  bool match(TokenType t);
  [[nodiscard]] bool at_end() const noexcept;

  std::vector<Token> tokens_;
  size_t pos_{0};
};

} // namespace minisql
