#pragma once
#include "sql/token.hpp"
#include <string>
#include <vector>

namespace minisql {

// Converts a SQL string into a flat token list.
// Throws ParseError on unrecognised characters.
class Lexer {
public:
  explicit Lexer(std::string src);

  // Tokenise the entire input at once.
  [[nodiscard]] std::vector<Token> tokenise();

private:
  void skip_whitespace_and_comments();
  Token next_token();
  Token read_number();
  Token read_string();
  Token read_ident_or_keyword();
  char peek() const noexcept;
  char peek2() const noexcept;
  char advance() noexcept;
  [[nodiscard]] bool at_end() const noexcept;

  std::string src_;
  size_t pos_{0};
  int line_{1};
};

} // namespace minisql
