#include <iostream>
#include <memory>

template <typename T, size_t N> class FixedArray {
public:
  using Allocator = std::allocator<T>;

private:
  Allocator alloc;
  T *data;
  size_t size;

public:
  FixedArray() : data(alloc.allocate(N)), size(N) {
    for (size_t i = 0; i < N; ++i) {
      alloc.construct(data + i);
    }
    std::cout << "Array of size " << N << " allocated\n";
  }
  ~FixedArray() {
    for (size_t i = 0; i < N; ++i) {
      alloc.destroy(data + 1);
    }
    alloc.deallocate(data, N);
    std::cout << "Array deallocated. \n";
  }
  T &operator[](size_t index) { return data[index]; }
  // copy and move constructors/assignment operators ommited for brevity
};
int main() {
  FixedArray<int, 5> array;
  for (int i = 0; i < 5; ++i) {
    array[i] = i * 10;
  }
  return 0;
}
