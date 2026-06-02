#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "common/error.hpp"
#include "repl/repl.hpp"

namespace minisql {

// ── Pimpl body
// ────────────────────────────────────────────────────────────────

struct Repl::Impl {
  std::string db_path;

  explicit Impl(const std::string &path) : db_path(path) {}
};

// ── Repl ─────────────────────────────────────────────────────────────────────

Repl::Repl(const std::string &db_path)
    : impl_(std::make_unique<Impl>(db_path)) {}

Repl::~Repl() = default;

void Repl::run() {
  print_banner();
  std::string line, pending;

  while (std::getline(std::cin, line)) {
    print_prompt();

    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
      line.pop_back();

    if (line.empty())
      continue;

    if (line[0] == '.') {
      if (handle_meta_command(line))
        continue;
      break; // .exit
    }

    if (!pending.empty())
      pending += ' ';
    pending += line;

    if (pending.back() == ';') {
      handle_sql(pending);
      pending.clear();
    }
  }
}

bool Repl::handle_meta_command(const std::string &line) {
  std::istringstream iss(line);
  std::string cmd;
  iss >> cmd;

  if (cmd == ".exit" || cmd == ".quit") {
    std::cout << "Goodbye.\n";
    return false; // signal run() to stop
  }
  if (cmd == ".help") {
    std::cout << "Meta-commands:\n"
                 "  .help             Show this message\n"
                 "  .exit / .quit     Exit minisql\n\n";
    return true;
  }
  std::cout << "Unknown command: " << cmd << "  (type .help for help)\n";
  return true;
}

void Repl::handle_sql(const std::string &sql) {
  try {
    // TODO: Handle SQL
  } catch (const DatabaseError &e) {
    std::cerr << "Error: " << e.what() << '\n';
  } catch (const std::exception &e) {
    std::cerr << "Unexpected error: " << e.what() << '\n';
  }
}

void Repl::print_banner() const {
  std::cout << "minisql v1.0 – a SQLite-inspired database engine\n"
               "Type .help for help, .exit to quit.\n\n";
}

void Repl::print_prompt() const { std::cout << "minisql> " << std::flush; }

} // namespace minisql
