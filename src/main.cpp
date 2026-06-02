#include <cstdlib>
#include <iostream>
#include <string>

#include "repl/repl.hpp"

int main(int argc, char *argv[]) {
  std::string db_path = "minisql_data";
  if (argc >= 2) {
    db_path = argv[1];
  }

  try {
    minisql::Repl repl(db_path);
    repl.run();
  } catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
