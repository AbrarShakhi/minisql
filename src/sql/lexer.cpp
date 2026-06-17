#include "sql/lexer.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace minisql {

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"CREATE", TokenType::KW_CREATE}, {"TABLE", TokenType::KW_TABLE},
    {"DROP", TokenType::KW_DROP},     {"INSERT", TokenType::KW_INSERT},
    {"INTO", TokenType::KW_INTO},     {"VALUES", TokenType::KW_VALUES},
    {"SELECT", TokenType::KW_SELECT}, {"FROM", TokenType::KW_FROM},
    {"WHERE", TokenType::KW_WHERE},   {"DELETE", TokenType::KW_DELETE},
    {"UPDATE", TokenType::KW_UPDATE}, {"SET", TokenType::KW_SET},
    {"ORDER", TokenType::KW_ORDER},   {"BY", TokenType::KW_BY},
    {"ASC", TokenType::KW_ASC},       {"DESC", TokenType::KW_DESC},
    {"LIMIT", TokenType::KW_LIMIT},   {"OFFSET", TokenType::KW_OFFSET},
    {"AND", TokenType::KW_AND},       {"OR", TokenType::KW_OR},
    {"NOT", TokenType::KW_NOT},       {"NULL", TokenType::KW_NULL},
    {"IS", TokenType::KW_IS},         {"INTEGER", TokenType::KW_INTEGER},
    {"INT", TokenType::KW_INTEGER},   {"REAL", TokenType::KW_REAL},
    {"FLOAT", TokenType::KW_REAL},    {"TEXT", TokenType::KW_TEXT},
    {"VARCHAR", TokenType::KW_TEXT},  {"BLOB", TokenType::KW_BLOB},
};

bool Token::is_keyword() const noexcept {
  return type >= TokenType::KW_CREATE && type <= TokenType::KW_BLOB;
}
bool Token::is_type_keyword() const noexcept {
  return type == TokenType::KW_INTEGER || type == TokenType::KW_REAL ||
         type == TokenType::KW_TEXT || type == TokenType::KW_BLOB;
}
bool Token::is_comparison_op() const noexcept {
  return type >= TokenType::EQ && type <= TokenType::GTE;
}

// ── Lexer
// ─────────────────────────────────────────────────────────────────────

Lexer::Lexer(std::string src) : src_(std::move(src)) {}

std::vector<Token> Lexer::tokenise() {
  std::vector<Token> tokens;
  while (true) {
    skip_whitespace_and_comments();
    if (at_end()) {
      tokens.push_back({TokenType::END_OF_FILE, "", line_});
      break;
    }
    tokens.push_back(next_token());
  }
  return tokens;
}

bool Lexer::at_end() const noexcept { return pos_ >= src_.size(); }
char Lexer::peek() const noexcept { return at_end() ? '\0' : src_[pos_]; }
char Lexer::peek2() const noexcept {
  return pos_ + 1 >= src_.size() ? '\0' : src_[pos_ + 1];
}
char Lexer::advance() noexcept {
  char c = src_[pos_++];
  if (c == '\n')
    ++line_;
  return c;
}

void Lexer::skip_whitespace_and_comments() {
  while (!at_end()) {
    char c = peek();
    if (std::isspace(static_cast<unsigned char>(c))) {
      advance();
      continue;
    }
    // Single-line comment: -- ...
    if (c == '-' && peek2() == '-') {
      while (!at_end() && peek() != '\n')
        advance();
      continue;
    }
    break;
  }
}

Token Lexer::next_token() {
  int saved_line = line_;
  char c = peek();

  if (std::isdigit(static_cast<unsigned char>(c)) ||
      (c == '-' && std::isdigit(static_cast<unsigned char>(peek2())))) {
    return read_number();
  }
  if (c == '\'' || c == '"')
    return read_string();
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
    return read_ident_or_keyword();

  advance();
  switch (c) {
  case '(':
    return {TokenType::LPAREN, "(", saved_line};
  case ')':
    return {TokenType::RPAREN, ")", saved_line};
  case ',':
    return {TokenType::COMMA, ",", saved_line};
  case ';':
    return {TokenType::SEMICOLON, ";", saved_line};
  case '.':
    return {TokenType::DOT, ".", saved_line};
  case '*':
    return {TokenType::STAR, "*", saved_line};
  case '=':
    return {TokenType::EQ, "=", saved_line};
  case '!':
    if (peek() == '=') {
      advance();
      return {TokenType::NEQ, "!=", saved_line};
    }
    break;
  case '<':
    if (peek() == '=') {
      advance();
      return {TokenType::LTE, "<=", saved_line};
    }
    if (peek() == '>') {
      advance();
      return {TokenType::NEQ, "<>", saved_line};
    }
    return {TokenType::LT, "<", saved_line};
  case '>':
    if (peek() == '=') {
      advance();
      return {TokenType::GTE, ">=", saved_line};
    }
    return {TokenType::GT, ">", saved_line};
  default:
    break;
  }
  return {TokenType::UNKNOWN, std::string(1, c), saved_line};
}

Token Lexer::read_number() {
  int saved_line = line_;
  bool is_real = false;
  std::string val;
  if (peek() == '-')
    val += advance();
  while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
    val += advance();
  if (!at_end() && peek() == '.') {
    is_real = true;
    val += advance();
    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
      val += advance();
  }
  return {is_real ? TokenType::REAL_LIT : TokenType::INTEGER_LIT, val,
          saved_line};
}

Token Lexer::read_string() {
  int saved_line = line_;
  char quote = advance(); // consume opening quote
  std::string val;
  while (!at_end() && peek() != quote) {
    char c = advance();
    if (c == '\\' && !at_end())
      val += advance(); // simple escape
    else
      val += c;
  }
  if (!at_end())
    advance(); // consume closing quote
  return {TokenType::STRING_LIT, val, saved_line};
}

Token Lexer::read_ident_or_keyword() {
  int saved_line = line_;
  std::string val;
  while (!at_end() &&
         (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
    val += advance();
  }
  std::string upper = val;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  auto it = KEYWORDS.find(upper);
  if (it != KEYWORDS.end())
    return {it->second, upper, saved_line};
  return {TokenType::IDENT, val, saved_line};
}

} // namespace minisql
