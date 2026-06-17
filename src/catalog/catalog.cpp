#include "catalog/catalog.hpp"
#include "common/error.hpp"
#include <cstring>
#include <fstream>

namespace minisql {

// ── Serialisation helpers
// ─────────────────────────────────────────────────────

static void write_u8(std::ostream &os, uint8_t v) {
  os.put(static_cast<char>(v));
}
static void write_u16(std::ostream &os, uint16_t v) {
  write_u8(os, v & 0xFF);
  write_u8(os, (v >> 8) & 0xFF);
}
static void write_u32(std::ostream &os, uint32_t v) {
  write_u8(os, v & 0xFF);
  write_u8(os, (v >> 8) & 0xFF);
  write_u8(os, (v >> 16) & 0xFF);
  write_u8(os, (v >> 24) & 0xFF);
}
static void write_u64(std::ostream &os, uint64_t v) {
  for (int i = 0; i < 8; ++i)
    write_u8(os, static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}
static void write_str(std::ostream &os, const std::string &s) {
  write_u8(os, static_cast<uint8_t>(s.size()));
  os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

static uint8_t read_u8(std::istream &is) {
  return static_cast<uint8_t>(is.get());
}
static uint16_t read_u16(std::istream &is) {
  uint16_t lo = read_u8(is);
  uint16_t hi = read_u8(is);
  return static_cast<uint16_t>(lo | (hi << 8));
}
static uint32_t read_u32(std::istream &is) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
    v |= (static_cast<uint32_t>(read_u8(is)) << (8 * i));
  return v;
}
static uint64_t read_u64(std::istream &is) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i)
    v |= (static_cast<uint64_t>(read_u8(is)) << (8 * i));
  return v;
}
static std::string read_str(std::istream &is) {
  uint8_t len = read_u8(is);
  std::string s(len, '\0');
  is.read(s.data(), len);
  return s;
}

// ── Catalog ──────────────────────────────────────────────────────────────────

Catalog::Catalog(const std::string &cat_path) : cat_path_(cat_path) { load(); }

Catalog::~Catalog() {
  try {
    save();
  } catch (...) {
  }
}

bool Catalog::create_table(const std::string &name, const Schema &schema,
                           PageId root_page_id) {
  if (tables_.count(name))
    return false;
  tables_[name] = TableMeta{schema, root_page_id, 1};
  save();
  return true;
}

bool Catalog::drop_table(const std::string &name) {
  auto it = tables_.find(name);
  if (it == tables_.end())
    return false;
  tables_.erase(it);
  save();
  return true;
}

bool Catalog::table_exists(const std::string &name) const noexcept {
  return tables_.count(name) != 0;
}

TableMeta &Catalog::get_table(const std::string &name) {
  auto it = tables_.find(name);
  if (it == tables_.end())
    throw NotFoundError("Table not found: " + name);
  return it->second;
}

const TableMeta &Catalog::get_table(const std::string &name) const {
  auto it = tables_.find(name);
  if (it == tables_.end())
    throw NotFoundError("Table not found: " + name);
  return it->second;
}

std::vector<std::string> Catalog::table_names() const {
  std::vector<std::string> names;
  names.reserve(tables_.size());
  for (const auto &[k, _] : tables_)
    names.push_back(k);
  return names;
}

uint64_t Catalog::next_row_id(const std::string &table) {
  auto &meta = get_table(table);
  return meta.next_row_id++;
}

void Catalog::flush() const { save(); }

// ── Private
// ───────────────────────────────────────────────────────────────────

void Catalog::load() {
  std::ifstream ifs(cat_path_, std::ios::binary);
  if (!ifs.is_open())
    return; // fresh database, no catalog yet

  uint32_t magic = read_u32(ifs);
  uint32_t version = read_u32(ifs);
  if (magic != 0x4D494E49U)
    throw StorageError("Corrupt catalog file");
  (void)version;

  uint32_t table_count = read_u32(ifs);
  for (uint32_t t = 0; t < table_count; ++t) {
    std::string name = read_str(ifs);
    PageId root_pid = read_u32(ifs);
    uint64_t next_rowid = read_u64(ifs);

    uint16_t col_count = read_u16(ifs);
    std::vector<Column> cols;
    cols.reserve(col_count);
    for (uint16_t c = 0; c < col_count; ++c) {
      std::string col_name = read_str(ifs);
      DataType type = static_cast<DataType>(read_u8(ifs));
      bool nullable = (read_u8(ifs) != 0);
      cols.emplace_back(std::move(col_name), type, nullable);
    }
    tables_[name] = TableMeta{Schema(std::move(cols)), root_pid, next_rowid};
  }
}

void Catalog::save() const {
  std::ofstream ofs(cat_path_, std::ios::binary | std::ios::trunc);
  if (!ofs)
    throw StorageError("Cannot write catalog file: " + cat_path_);

  write_u32(ofs, 0x4D494E49U); // magic
  write_u32(ofs, 1U);          // version
  write_u32(ofs, static_cast<uint32_t>(tables_.size()));

  for (const auto &[name, meta] : tables_) {
    write_str(ofs, name);
    write_u32(ofs, meta.root_page_id);
    write_u64(ofs, meta.next_row_id);
    const auto &cols = meta.schema.columns();
    write_u16(ofs, static_cast<uint16_t>(cols.size()));
    for (const auto &c : cols) {
      write_str(ofs, c.name);
      write_u8(ofs, static_cast<uint8_t>(c.type));
      write_u8(ofs, c.nullable ? 1 : 0);
    }
  }
}

} // namespace minisql
