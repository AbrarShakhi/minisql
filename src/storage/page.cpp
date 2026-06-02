#include "storage/page.hpp"
#include <cstring>

namespace minisql {

void Page::reset() noexcept {
  page_id_ = INVALID_PAGE_ID;
  dirty_ = false;
  pins_ = 0;
  data_.fill(0);
}

} // namespace minisql
