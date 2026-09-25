#pragma once

#include <cstddef>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../cache/arc_cache.hpp"
#include "../cache/lfu_cache.hpp"
#include "../cache/lirs_cache.hpp"
#include "../cache/two_q_cache.hpp"
#include "../cachesystem/cache_wrapper.hpp"

template <typename Key, typename Value> class CacheSystem {
public:
  using Cache = CacheWrapper<Key, Value>;

  // Формат ввода (по одной строке на уровень кэша):
  //   <количество уровней>
  //   <тип> <ёмкость>
  //   <тип> <ёмкость>
  //   ...
  explicit CacheSystem(std::istream &in) {
    std::size_t levels{};
    if (!(in >> levels)) {
      throw std::invalid_argument("CacheSystem: failed to read levels count");
    }

    cascade_.reserve(levels);

    std::string cacheId;
    std::size_t capacity{};
    for (std::size_t i = 0; i < levels; ++i) {
      if (!(in >> cacheId >> capacity)) {
        throw std::invalid_argument("CacheSystem: malformed input at level " +
                                    std::to_string(i));
      }
      cascade_.push_back(makeCache(cacheId, capacity));
    }
  }

  // Ищет значение по всей цепочке уровней.
  // При попадании на уровне i "поднимает" значение во все уровни 0..i-1.
  bool get(const Key &key, Value &out) {
    for (std::size_t i = 0; i < cascade_.size(); ++i) {
      if (cascade_[i].get(key, out)) {
        promote(key, out, i);
        return true;
      }
    }
    return false;
  }

  // Кладёт значение во все уровни каскада (inclusive-политика).
  // Если нужна exclusive-политика — кладите только в cascade_.front().
  void put(const Key &key, const Value &value) {
    for (auto &cache : cascade_) {
      cache.put(key, value);
    }
  }

  bool contains(const Key &key) const {
    for (const auto &cache : cascade_) {
      if (cache.contains(key))
        return true;
    }
    return false;
  }

  std::size_t levels() const noexcept { return cascade_.size(); }

private:
  static Cache makeCache(std::string_view name, std::size_t capacity) {
    if (name == "ARC") {
      return Cache(std::in_place_type<cache::ARCCache<Key, Value>>, capacity);
    } else if (name == "LFU") {
      return Cache(std::in_place_type<cache::LFUCache<Key, Value>>, capacity);
    } else if (name == "LIRS") {
      return Cache(std::in_place_type<cache::LIRSCache<Key, Value>>, capacity);
    } else if (name == "2Q") {
      return Cache(std::in_place_type<cache::TwoQCache<Key, Value>>, capacity);
    } else {
      throw std::invalid_argument("Unknown cache type: " + std::string(name));
    }
  }

  // Проталкивает найденное значение в уровни выше того, где произошло
  // попадание.
  void promote(const Key &key, const Value &value, std::size_t hitLevel) {
    for (std::size_t i = 0; i < hitLevel; ++i) {
      cascade_[i].put(key, value);
    }
  }

  std::vector<Cache> cascade_;
};
