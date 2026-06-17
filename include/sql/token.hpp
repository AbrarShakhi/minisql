#pragma once
#include <string>

namespace minisql {

enum class TokenType {
  // Literals
  INTEGER_LIT, // 42
  REAL_LIT,    // 3.14
  STRING_LIT,  // 'hello'
  IDENT,       // table_name, column_name
  // Keywords
  KW_CREATE,
  KW_TABLE,
  KW_DROP,
  KW_INSERT,
  KW_INTO,
  KW_VALUES,
  KW_SELECT,
  KW_FROM,
  KW_WHERE,
  KW_DELETE,
  KW_UPDATE,
  KW_SET,
  KW_ORDER,
  KW_BY,
  KW_ASC,
  KW_DESC,
  KW_LIMIT,
  KW_OFFSET,
  KW_AND,
  KW_OR,
  KW_NOT,
  KW_NULL,
  KW_IS,
  KW_INTEGER,
  KW_REAL,
  KW_TEXT,
  KW_BLOB,
  KW_NOT_NULL, // treated as two tokens in parser, here for convenience
  // Punctuation
  LPAREN,
  RPAREN,
  COMMA,
  SEMICOLON,
  DOT,
  STAR,
  // Operators
  EQ,  // =
  NEQ, // != or <>
  LT,  // <
  GT,  // >
  LTE, // <=
  GTE, // >=
  // Special
  END_OF_FILE,
  UNKNOWN,
};

struct Token {
  TokenType type{TokenType::UNKNOWN};
  std::string value; // raw text (always populated)
  int line{1};

  [[nodiscard]] bool is(TokenType t) const noexcept { return type == t; }
  [[nodiscard]] bool is_keyword() const noexcept;
  [[nodiscard]] bool is_type_keyword() const noexcept;
  [[nodiscard]] bool is_comparison_op() const noexcept;
};

} // namespace minisql
