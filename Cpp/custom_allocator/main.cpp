#include <cstddef>
#include <iostream>
#include <vector>

template <typename T> class SimpleAllocator {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;

  SimpleAllocator() = default;

  template <class U>
  constexpr SimpleAllocator(const SimpleAllocator<U> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    auto ptr = static_cast<T *>(malloc(n * sizeof(T)));
    std::cout << "Allocating " << n * sizeof(T) << " byte\n";
    return ptr;
  }
  void deallocate(T *p, std::size_t n) noexcept {
    free(p);
    std::cout << "Deallocating " << n * sizeof(T) << " bytes\n";
  }
};

int main() {
  std::vector<int, SimpleAllocator<int>> vec(SimpleAllocator<int>{});
  vec.push_back(1);
  vec.push_back(2);
  return 0;
}
