#pragma once

#include <memory>
#include <type_traits>
#include <utility>

template <typename T, typename Key, typename Value>
concept CacheLike = requires(T t, const Key k, const Value v, Value out) {
  { t.get(k, out) } -> std::convertible_to<bool>;
  t.put(k, v);
  { t.contains(k) } -> std::convertible_to<bool>;
};

template <typename Key, typename Value> class CacheWrapper {
public:
  struct CacheConcept {
    virtual ~CacheConcept() = default;

    virtual bool get(const Key &key, Value &out) const = 0;
    virtual void put(const Key &key, const Value &value) = 0;
    virtual bool contains(const Key &key) const = 0;
  };

  template <typename T> struct CacheModel final : CacheConcept {
    T obj;

    template <typename... Args>
    explicit CacheModel(Args &&...args) : obj(std::forward<Args>(args)...) {}

    bool get(const Key &key, Value &out) const override {
      return obj.get(key, out);
    }
    void put(const Key &key, const Value &value) override {
      obj.put(key, value);
    }
    bool contains(const Key &key) const override { return obj.contains(key); }
  };

  // Исключаем сам CacheWrapper из дедукции, чтобы не конфликтовать
  // с move-конструктором
  template <CacheLike<Key, Value> T,
            typename = std::enable_if_t<
                !std::is_same_v<std::decay_t<T>, CacheWrapper>>>
  explicit CacheWrapper(T &&cache)
      : self_(std::make_unique<CacheModel<std::decay_t<T>>>(
            std::forward<T>(cache))) {}

  // Для кэшей, которые дорого мувать или надо конструировать in-place
  template <CacheLike<Key, Value> T, typename... Args>
  explicit CacheWrapper(std::in_place_type_t<T>, Args &&...args)
      : self_(std::make_unique<CacheModel<T>>(std::forward<Args>(args)...)) {}

  // Публичный API обёртки
  bool get(const Key &key, Value &out) const { return self_->get(key, out); }
  void put(const Key &key, const Value &value) { self_->put(key, value); }
  bool contains(const Key &key) const { return self_->contains(key); }

  CacheWrapper(CacheWrapper &&) noexcept = default;
  CacheWrapper &operator=(CacheWrapper &&) noexcept = default;
  CacheWrapper(const CacheWrapper &) = delete;
  CacheWrapper &operator=(const CacheWrapper &) = delete;

private:
  std::unique_ptr<CacheConcept> self_;
};
