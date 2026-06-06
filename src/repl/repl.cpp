#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "common/error.hpp"
#include "repl/repl.hpp"
#include "storage/buffer_pool.hpp"
#include "storage/disk_manager.hpp"

namespace minisql {

// ── Pimpl body
// ────────────────────────────────────────────────────────────────

struct Repl::Impl {
  std::string db_path;
  std::unique_ptr<DiskManager> disk_mgr;
  std::unique_ptr<BufferPool> buf_pool;

  explicit Impl(const std::string &path)
      : db_path(path), disk_mgr(std::make_unique<DiskManager>(path + ".db")),
        buf_pool(std::make_unique<BufferPool>(*disk_mgr)) {}
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

  impl_->buf_pool->flush_all();
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
    std::cout
        << "Meta-commands:\n"
           "  .help             Show this message\n"
           "  .tables           List all tables\n"
           "  .schema [name]    Show schema of all tables or a specific one\n"
           "  .exit / .quit     Exit minisql\n\n"
           "SQL supported:\n"
           "  CREATE TABLE name (col type [NOT NULL], ...)\n"
           "  DROP TABLE name\n"
           "  INSERT INTO name [(cols)] VALUES (vals), ...\n"
           "  SELECT * | cols FROM name [WHERE expr] [ORDER BY col [ASC|DESC]] "
           "[LIMIT n [OFFSET m]]\n"
           "  DELETE FROM name [WHERE expr]\n"
           "  UPDATE name SET col=val,... [WHERE expr]\n"
           "  Types: INTEGER, REAL, TEXT, BLOB\n";
    return true;
  }
  if (cmd == ".tables")
    return false;
  if (cmd == ".schema")
    return false;

  std::cout << "Unknown command: " << cmd << "  (type .help for help)\n";
  return true;
}

void Repl::handle_sql(const std::string &sql) {
  try {

  } catch (const DatabaseError &e) {
    std::cerr << "Error: " << e.what() << '\n';
  } catch (const std::exception &e) {
    std::cerr << "Unexpected error: " << e.what() << '\n';
  }
}

void Repl::print_banner() const {
  std::cout << "minisql v.1 - a SQLite-inspired database engine\n"
               "Type .help for help, .exit to quit.\n\n";
}

void Repl::print_prompt() const { std::cout << "minisql> " << std::flush; }

} // namespace minisql
