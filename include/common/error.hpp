#pragma once
#include <stdexcept>

namespace minisql {

struct DatabaseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct StorageError : DatabaseError {
  using DatabaseError::DatabaseError;
};
struct ParseError : DatabaseError {
  using DatabaseError::DatabaseError;
};
struct ExecutionError : DatabaseError {
  using DatabaseError::DatabaseError;
};
struct SchemaError : DatabaseError {
  using DatabaseError::DatabaseError;
};
struct NotFoundError : DatabaseError {
  using DatabaseError::DatabaseError;
};

} // namespace minisql
