#include "sql/parser.hpp"
#include "common/error.hpp"
#include <sstream>
#include <stdexcept>

namespace minisql {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// ── Public entry point
// ────────────────────────────────────────────────────────

std::unique_ptr<Stmt> Parser::parse() {
  if (at_end())
    throw ParseError("Empty input");

  const Token &t = peek();
  std::unique_ptr<Stmt> stmt;

  if (t.is(TokenType::KW_CREATE))
    stmt = parse_create_table();
  else if (t.is(TokenType::KW_DROP))
    stmt = parse_drop_table();
  else if (t.is(TokenType::KW_INSERT))
    stmt = parse_insert();
  else if (t.is(TokenType::KW_SELECT))
    stmt = parse_select();
  else if (t.is(TokenType::KW_DELETE))
    stmt = parse_delete();
  else if (t.is(TokenType::KW_UPDATE))
    stmt = parse_update();
  else {
    throw ParseError("Unexpected token '" + t.value + "' at line " +
                     std::to_string(t.line));
  }

  // Consume optional trailing semicolon.
  match(TokenType::SEMICOLON);
  return stmt;
}

// ── Statement parsers
// ─────────────────────────────────────────────────────────

std::unique_ptr<Stmt> Parser::parse_create_table() {
  expect(TokenType::KW_CREATE);
  expect(TokenType::KW_TABLE);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::CREATE_TABLE;
  stmt->table_name = expect(TokenType::IDENT).value;
  expect(TokenType::LPAREN);
  do {
    stmt->col_defs.push_back(parse_column_def());
  } while (match(TokenType::COMMA));
  expect(TokenType::RPAREN);
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_drop_table() {
  expect(TokenType::KW_DROP);
  expect(TokenType::KW_TABLE);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::DROP_TABLE;
  stmt->table_name = expect(TokenType::IDENT).value;
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_insert() {
  expect(TokenType::KW_INSERT);
  expect(TokenType::KW_INTO);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::INSERT;
  stmt->table_name = expect(TokenType::IDENT).value;

  // Optional column list.
  if (peek().is(TokenType::LPAREN) && !peek2().is(TokenType::STRING_LIT) &&
      !peek2().is(TokenType::INTEGER_LIT) && !peek2().is(TokenType::REAL_LIT) &&
      !peek2().is(TokenType::KW_NULL)) {
    // Peek2 is an IDENT → column list
    if (peek2().is(TokenType::IDENT)) {
      expect(TokenType::LPAREN);
      do {
        stmt->ins_cols.push_back(expect(TokenType::IDENT).value);
      } while (match(TokenType::COMMA));
      expect(TokenType::RPAREN);
    }
  }

  expect(TokenType::KW_VALUES);

  // One or more value rows: VALUES (...), (...)
  do {
    expect(TokenType::LPAREN);
    std::vector<Value> row;
    do {
      row.push_back(parse_literal_value());
    } while (match(TokenType::COMMA));
    expect(TokenType::RPAREN);
    stmt->ins_rows.push_back(std::move(row));
  } while (match(TokenType::COMMA));

  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_select() {
  expect(TokenType::KW_SELECT);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::SELECT;

  if (match(TokenType::STAR)) {
    stmt->select_star = true;
  } else {
    do {
      stmt->select_cols.push_back(expect(TokenType::IDENT).value);
    } while (match(TokenType::COMMA));
  }

  expect(TokenType::KW_FROM);
  stmt->table_name = expect(TokenType::IDENT).value;

  if (match(TokenType::KW_WHERE)) {
    stmt->where_expr = parse_expr();
  }
  if (match(TokenType::KW_ORDER)) {
    expect(TokenType::KW_BY);
    stmt->order_by_col = expect(TokenType::IDENT).value;
    if (match(TokenType::KW_DESC))
      stmt->order_asc = false;
    else
      match(TokenType::KW_ASC);
  }
  if (match(TokenType::KW_LIMIT)) {
    stmt->limit = std::get<Integer>(parse_literal_value());
  }
  if (match(TokenType::KW_OFFSET)) {
    stmt->offset = std::get<Integer>(parse_literal_value());
  }
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_delete() {
  expect(TokenType::KW_DELETE);
  expect(TokenType::KW_FROM);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::DELETE;
  stmt->table_name = expect(TokenType::IDENT).value;
  if (match(TokenType::KW_WHERE)) {
    stmt->where_expr = parse_expr();
  }
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_update() {
  expect(TokenType::KW_UPDATE);
  auto stmt = std::make_unique<Stmt>();
  stmt->kind = StmtKind::UPDATE;
  stmt->table_name = expect(TokenType::IDENT).value;
  expect(TokenType::KW_SET);
  do {
    AssignPair ap;
    ap.col = expect(TokenType::IDENT).value;
    expect(TokenType::EQ);
    ap.val = parse_literal_value();
    stmt->update_sets.push_back(std::move(ap));
  } while (match(TokenType::COMMA));
  if (match(TokenType::KW_WHERE)) {
    stmt->where_expr = parse_expr();
  }
  return stmt;
}

// ── Expression parsers
// ────────────────────────────────────────────────────────

ExprPtr Parser::parse_expr() { return parse_or(); }

ExprPtr Parser::parse_or() {
  auto left = parse_and();
  while (match(TokenType::KW_OR)) {
    auto right = parse_and();
    left = Expr::make_binary(BinOp::OR, std::move(left), std::move(right));
  }
  return left;
}

ExprPtr Parser::parse_and() {
  auto left = parse_not();
  while (match(TokenType::KW_AND)) {
    auto right = parse_not();
    left = Expr::make_binary(BinOp::AND, std::move(left), std::move(right));
  }
  return left;
}

ExprPtr Parser::parse_not() {
  if (match(TokenType::KW_NOT)) {
    return Expr::make_not(parse_not());
  }
  return parse_comparison();
}

ExprPtr Parser::parse_comparison() {
  auto left = parse_primary();

  // IS NULL / IS NOT NULL
  if (match(TokenType::KW_IS)) {
    bool negate = match(TokenType::KW_NOT);
    expect(TokenType::KW_NULL);
    auto null_lit = Expr::make_literal(NullVal{});
    auto eq =
        Expr::make_binary(BinOp::EQ, std::move(left), std::move(null_lit));
    if (negate)
      return Expr::make_not(std::move(eq));
    return eq;
  }

  if (!peek().is_comparison_op())
    return left;

  BinOp op{};
  switch (advance().type) {
  case TokenType::EQ:
    op = BinOp::EQ;
    break;
  case TokenType::NEQ:
    op = BinOp::NEQ;
    break;
  case TokenType::LT:
    op = BinOp::LT;
    break;
  case TokenType::GT:
    op = BinOp::GT;
    break;
  case TokenType::LTE:
    op = BinOp::LTE;
    break;
  case TokenType::GTE:
    op = BinOp::GTE;
    break;
  default:
    throw ParseError("Expected comparison operator");
  }
  auto right = parse_primary();
  return Expr::make_binary(op, std::move(left), std::move(right));
}

ExprPtr Parser::parse_primary() {
  const Token &t = peek();

  if (t.is(TokenType::LPAREN)) {
    advance();
    auto e = parse_expr();
    expect(TokenType::RPAREN);
    return e;
  }
  if (t.is(TokenType::IDENT)) {
    advance();
    return Expr::make_column(t.value);
  }
  // Literal value
  return Expr::make_literal(parse_literal_value());
}

// ── Helpers
// ───────────────────────────────────────────────────────────────────

Value Parser::parse_literal_value() {
  const Token &t = peek();
  if (t.is(TokenType::INTEGER_LIT)) {
    advance();
    return Integer{std::stoll(t.value)};
  }
  if (t.is(TokenType::REAL_LIT)) {
    advance();
    return Real{std::stod(t.value)};
  }
  if (t.is(TokenType::STRING_LIT)) {
    advance();
    return Text{t.value};
  }
  if (t.is(TokenType::KW_NULL)) {
    advance();
    return NullVal{};
  }
  throw ParseError("Expected a literal value, got '" + t.value + "' at line " +
                   std::to_string(t.line));
}

DataType Parser::parse_type_keyword() {
  const Token &t = advance();
  switch (t.type) {
  case TokenType::KW_INTEGER:
    return DataType::INTEGER;
  case TokenType::KW_REAL:
    return DataType::REAL;
  case TokenType::KW_TEXT:
    return DataType::TEXT;
  case TokenType::KW_BLOB:
    return DataType::BLOB;
  default:
    throw ParseError("Expected type keyword, got '" + t.value + "'");
  }
}

ColumnDef Parser::parse_column_def() {
  ColumnDef cd;
  cd.name = expect(TokenType::IDENT).value;
  cd.type = parse_type_keyword();
  // Optional NOT NULL
  cd.nullable = true;
  if (peek().is(TokenType::KW_NOT)) {
    advance();
    expect(TokenType::KW_NULL);
    cd.nullable = false;
  }
  return cd;
}

const Token &Parser::peek() const noexcept {
  static Token eof{TokenType::END_OF_FILE, "", 0};
  if (pos_ >= tokens_.size())
    return eof;
  return tokens_[pos_];
}
const Token &Parser::peek2() const noexcept {
  static Token eof{TokenType::END_OF_FILE, "", 0};
  if (pos_ + 1 >= tokens_.size())
    return eof;
  return tokens_[pos_ + 1];
}
const Token &Parser::advance() {
  if (!at_end())
    ++pos_;
  return tokens_[pos_ - 1];
}
const Token &Parser::expect(TokenType t) {
  if (peek().type != t) {
    std::string expected;
    switch (t) {
    case TokenType::IDENT:
      expected = "identifier";
      break;
    case TokenType::LPAREN:
      expected = "'('";
      break;
    case TokenType::RPAREN:
      expected = "')'";
      break;
    case TokenType::COMMA:
      expected = "','";
      break;
    case TokenType::SEMICOLON:
      expected = "';'";
      break;
    case TokenType::EQ:
      expected = "'='";
      break;
    case TokenType::KW_NULL:
      expected = "NULL";
      break;
    case TokenType::KW_FROM:
      expected = "FROM";
      break;
    case TokenType::KW_INTO:
      expected = "INTO";
      break;
    case TokenType::KW_VALUES:
      expected = "VALUES";
      break;
    case TokenType::KW_TABLE:
      expected = "TABLE";
      break;
    case TokenType::KW_SET:
      expected = "SET";
      break;
    case TokenType::KW_BY:
      expected = "BY";
      break;
    default:
      expected = "token";
      break;
    }
    throw ParseError("Expected " + expected + " but got '" + peek().value +
                     "' at line " + std::to_string(peek().line));
  }
  return advance();
}
bool Parser::match(TokenType t) {
  if (peek().type == t) {
    advance();
    return true;
  }
  return false;
}
bool Parser::at_end() const noexcept {
  return pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::END_OF_FILE;
}

} // namespace minisql
