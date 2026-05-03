#include <shared/game/EquipmentCache.h>
#include <sstream>
#include <string>

namespace Firelands::EquipmentCache {

std::string Serialize(std::vector<PlayerCreateVisualItem> const &items) {
  std::ostringstream out;
  bool first = true;
  for (PlayerCreateVisualItem const &item : items) {
    if (item.slot >= 23)
      continue;
    if (!first)
      out << ';';
    first = false;
    out << static_cast<uint32>(item.slot) << ':' << static_cast<uint32>(item.invType)
        << ':' << item.displayId << ':' << item.displayEnchantId;
  }
  return out.str();
}

VisualArray Parse(std::string const &serialized) {
  VisualArray parsed{};
  if (serialized.empty())
    return parsed;

  std::stringstream rows(serialized);
  std::string row;
  while (std::getline(rows, row, ';')) {
    if (row.empty())
      continue;

    std::stringstream cols(row);
    std::string col;
    std::array<uint32, 4> values{};
    size_t idx = 0;
    while (std::getline(cols, col, ':') && idx < values.size()) {
      try {
        values[idx++] = static_cast<uint32>(std::stoul(col));
      } catch (...) {
        idx = 0;
        break;
      }
    }

    if (idx != values.size())
      continue;
    if (values[0] >= parsed.size())
      continue;

    auto &slot = parsed[values[0]];
    slot.invType = static_cast<uint8>(values[1]);
    slot.displayId = values[2];
    slot.displayEnchantId = values[3];
  }

  return parsed;
}

std::string SerializeVisualArray(VisualArray const &arr) {
  std::vector<PlayerCreateVisualItem> items;
  items.reserve(arr.size());
  for (size_t i = 0; i < arr.size(); ++i) {
    if (arr[i].displayId == 0 && arr[i].invType == 0)
      continue;
    PlayerCreateVisualItem row{};
    row.slot = static_cast<uint8>(i);
    row.invType = arr[i].invType;
    row.displayId = arr[i].displayId;
    row.displayEnchantId = arr[i].displayEnchantId;
    items.push_back(row);
  }
  return Serialize(items);
}

} // namespace Firelands::EquipmentCache
