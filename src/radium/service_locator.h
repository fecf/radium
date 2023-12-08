#pragma once

#include <map>
#include <type_traits>
#include <memory>

class ServiceLocator {
 public:
  template <typename T> static void provide(T* instance, int tag) {
    std::shared_ptr<T> sp(instance);
    size_t hash = typeid(T).hash_code();
    ServiceLocator::map.emplace(std::make_pair(hash, tag), std::static_pointer_cast<void>(sp));
  }

  template <typename T> static void provide(T* instance) {
    return provide<T>(instance, -1);
  }

  template <typename T> static T* get(int tag) {
    size_t hash = typeid(T).hash_code();
    assert(ServiceLocator::map.contains({hash, tag}));
    T* ret = static_cast<T*>(ServiceLocator::map.at({hash, tag}).get());
    return ret;
  }

  template <typename T> static T* get() {
    return get<T>(-1);
  }

  static std::map<std::pair<size_t, int>, std::shared_ptr<void>> map;
};