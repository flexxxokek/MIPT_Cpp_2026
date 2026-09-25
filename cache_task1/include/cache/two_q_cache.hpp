#pragma once
// 2Q cache (Johnson & Shasha, 1994).
//
// Three lists are maintained:
//   A1in  - FIFO queue of recently-inserted, "cold" resident entries.
//   A1out - FIFO ghost queue: keys recently evicted from A1in (no values).
//   Am    - LRU queue of "hot" resident entries (promoted / re-referenced).
//
// A brand-new key always enters A1in. If it is evicted from A1in before
// being referenced again, its key (not its value) moves to the A1out ghost
// list. A key that is referenced again while its ghost is still in A1out is
// considered "proven interesting" and is promoted straight into Am. Keys
// that are hit while still resident in Am are moved to the MRU end of Am.
//
// A1in and Am together never hold more than `capacity` real entries; A1out
// is a ghost list bounded independently and consumes no cache slots.

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace cache {

template <typename Key, typename Value> class TwoQCache {
public:
  // kin/kout are fractions of `capacity` used to size A1in and the A1out
  // ghost list, respectively. Defaults follow the values suggested in the
  // original 2Q paper.
  explicit TwoQCache(std::size_t capacity, double kin = 0.25, double kout = 0.5)
      : capacity_(capacity), a1in_capacity_(std::max<std::size_t>(
                                 1, static_cast<std::size_t>(capacity * kin))),
        a1out_capacity_(std::max<std::size_t>(
            1, static_cast<std::size_t>(capacity * kout))) {}

  bool get(const Key &key, Value &out) {
    if (auto it = am_map_.find(key); it != am_map_.end()) {
      out = it->second->value;
      am_list_.splice(am_list_.begin(), am_list_, it->second);
      am_map_[key] = am_list_.begin();
      return true;
    }
    if (auto it = a1in_map_.find(key); it != a1in_map_.end()) {
      out = it->second->value;
      return true; // A1in stays strict FIFO: no reordering on hit.
    }
    return false; // Either truly absent, or only a key-less ghost in A1out.
  }

  void put(const Key &key, const Value &value) {
    if (capacity_ == 0) {
      return;
    }

    if (auto it = am_map_.find(key); it != am_map_.end()) {
      it->second->value = value;
      am_list_.splice(am_list_.begin(), am_list_, it->second);
      am_map_[key] = am_list_.begin();
      return;
    }
    if (auto it = a1in_map_.find(key); it != a1in_map_.end()) {
      it->second->value = value;
      return;
    }
    if (auto it = a1out_map_.find(key); it != a1out_map_.end()) {
      a1out_list_.erase(it->second);
      a1out_map_.erase(it);
      insertAm(key, value);
      return;
    }
    insertA1in(key, value);
  }

  bool contains(const Key &key) const {
    return am_map_.contains(key) || a1in_map_.contains(key);
  }
  std::size_t size() const { return am_list_.size() + a1in_list_.size(); }
  std::size_t capacity() const { return capacity_; }

private:
  struct Node {
    Key key;
    Value value;
  };
  using List = std::list<Node>;
  using ListIt = typename List::iterator;

  void insertA1in(const Key &key, const Value &value) {
    if (realSize() >= capacity_) {
      evictReal();
    }
    a1in_list_.push_front(Node{key, value});
    a1in_map_[key] = a1in_list_.begin();
    if (a1in_list_.size() > a1in_capacity_) {
      migrateOldestA1inToGhost();
    }
  }

  void insertAm(const Key &key, const Value &value) {
    if (realSize() >= capacity_) {
      evictReal();
    }
    am_list_.push_front(Node{key, value});
    am_map_[key] = am_list_.begin();
  }

  void migrateOldestA1inToGhost() {
    if (a1in_list_.empty()) {
      return;
    }
    const Key victim = a1in_list_.back().key;
    a1in_list_.pop_back();
    a1in_map_.erase(victim);

    a1out_list_.push_front(victim);
    a1out_map_[victim] = a1out_list_.begin();
    if (a1out_list_.size() > a1out_capacity_) {
      const Key ghost = a1out_list_.back();
      a1out_list_.pop_back();
      a1out_map_.erase(ghost);
    }
  }

  // Evicts one real (value-holding) entry to make room, preferring A1in.
  void evictReal() {
    if (!a1in_list_.empty()) {
      migrateOldestA1inToGhost();
    } else if (!am_list_.empty()) {
      const Key victim = am_list_.back().key;
      am_list_.pop_back();
      am_map_.erase(victim);
    }
  }

  std::size_t realSize() const { return a1in_list_.size() + am_list_.size(); }

  std::size_t capacity_;
  std::size_t a1in_capacity_;
  std::size_t a1out_capacity_;

  List a1in_list_;
  std::unordered_map<Key, ListIt> a1in_map_;

  List am_list_;
  std::unordered_map<Key, ListIt> am_map_;

  std::list<Key> a1out_list_;
  std::unordered_map<Key, typename std::list<Key>::iterator> a1out_map_;
};

} // namespace cache
