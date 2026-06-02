#pragma once
#include <memory>
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

private:
  bool handle_meta_command(const std::string &line);
  void handle_sql(const std::string &sql);
  void print_banner() const;
  void print_prompt() const;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace minisql
