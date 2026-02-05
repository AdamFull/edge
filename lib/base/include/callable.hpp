#ifndef EDGE_CALLABLE_H
#define EDGE_CALLABLE_H

#include <type_traits>
#include <utility>

#include "allocator.hpp"

namespace edge {
template <typename T>
struct callable_traits : callable_traits<decltype(&T::operator())> {};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...) const> {
  using signature = R(Args...);
  using return_type = R;
};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...)> {
  using signature = R(Args...);
  using return_type = R;
};

template <typename R, typename... Args> struct callable_traits<R (*)(Args...)> {
  using signature = R(Args...);
  using return_type = R;
};

template <typename> struct Callable;

template <typename R, typename... Args> struct Callable<R(Args...)> {
  R invoke(Args... args) const {
    return invoke_fn(data, std::forward<Args>(args)...);
  }

  void destroy(const NotNull<const Allocator *> alloc) {
    if (data) {
      alloc->deallocate(data);
    }
    invoke_fn = nullptr;
    data = nullptr;
  }

  bool is_valid() const { return invoke_fn != nullptr; }

  R (*invoke_fn)(void *data, Args... args) = nullptr;
  void *data = nullptr;
};

template <typename R, typename... Args>
Callable<R(Args...)>
callable_create_from_func(const NotNull<const Allocator *> alloc, R (*fn)(Args...)) {
  using FnPtr = R (*)(Args...);

  FnPtr *stored = alloc->allocate<FnPtr>(fn);

  Callable<R(Args...)> result;
  result.invoke_fn = [](void *data, Args... args) -> R {
    FnPtr fn_ptr = *static_cast<FnPtr *>(data);
    return fn_ptr(std::forward<Args>(args)...);
  };
  result.data = stored;
  return result;
}

template <typename F>
auto callable_create_from_lambda(const NotNull<const Allocator *> alloc,
                                        F &&functor) {
  using FType = std::decay_t<F>;
  using Sig = callable_traits<FType>::signature;

  auto *stored = alloc->allocate<FType>(std::forward<F>(functor));

  return [=]<typename R, typename... Args>(R (*)(Args...)) {
    Callable<R(Args...)> result;
    result.invoke_fn = [](void *data, Args... args) -> R {
      FType *fn = static_cast<FType *>(data);
      return (*fn)(std::forward<Args>(args)...);
    };
    result.data = stored;
    return result;
  }(static_cast<Sig *>(nullptr));
}
} // namespace edge

#endif