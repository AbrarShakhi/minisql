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
  std::string src_;
  size_t pos_{0};
  int line_{1};
};

} // namespace minisql
