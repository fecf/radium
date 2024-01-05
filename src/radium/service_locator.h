#pragma once

#include <map>
#include <memory>
#include <type_traits>

class ServiceLocator {
 public:
  template <typename T>
  static void Provide(T* instance, int tag) {
    std::shared_ptr<T> sp(instance);
    size_t hash = typeid(T).hash_code();
    ServiceLocator::map_.emplace(
        std::make_pair(hash, tag), std::static_pointer_cast<void>(sp));
  }

  template <typename T>
  static void Provide(T* instance) {
    return Provide<T>(instance, -1);
  }

  template <typename T>
  static T* Get(int tag) {
    size_t hash = typeid(T).hash_code();
    assert(ServiceLocator::map_.contains({hash, tag}));
    T* ret = static_cast<T*>(ServiceLocator::map_.at({hash, tag}).get());
    return ret;
  }

  template <typename T>
  static T* Get() {
    return Get<T>(-1);
  }

  static void Clear() { map_.clear(); }

 private:
  static std::map<std::pair<size_t, int>, std::shared_ptr<void>> map_;
};