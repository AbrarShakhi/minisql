#pragma once
#include <string>

namespace minisql {

// Interactive read-eval-print loop.
// Owns the DiskManager, BufferPool, Catalog, and Executor.
class Repl {
public:
  explicit Repl(const std::string &db_path);
  ~Repl();

  Repl(const Repl &) = delete;
  Repl &operator=(const Repl &) = delete;

  void run();
};

} // namespace minisql
