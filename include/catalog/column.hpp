#pragma once
#include "common/types.hpp"
#include <string>

namespace minisql {

struct Column {
  std::string name;
  DataType type{DataType::INTEGER};
  bool nullable{true};

  Column() = default;
  Column(std::string n, DataType t, bool null = true)
      : name(std::move(n)), type(t), nullable(null) {}

  [[nodiscard]] bool operator==(const Column &o) const noexcept {
    return name == o.name && type == o.type && nullable == o.nullable;
  }
};

} // namespace minisql
