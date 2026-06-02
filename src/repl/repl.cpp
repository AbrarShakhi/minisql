#include <iostream>
#include <memory>
#include <sstream>
#include <string>

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
  std::string line, pending;

  while (true) {

    if (!std::getline(std::cin, line))
      break;

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

    // Accumulate multi-line SQL until we see a semicolon.
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
  std::cout << "Unknown command: " << cmd << "  (type .help for help)\n";
  return true;
}

void Repl::handle_sql(const std::string &sql) {}

} // namespace minisql
