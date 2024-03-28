#include <iostream>
#include <new>
#include <stack>
template <typename T, size_t PoolSize> class MemoryPool {
public:
  MemoryPool() : pool(new T[PoolSize]), freeIndices() {
    for (size_t i = 0; i < PoolSize; ++i) {
      freeIndices.push(i);
    }
  }
  ~MemoryPool() { delete[] pool; }

  T *allocate() {
    if (freeIndices.empty()) {
      throw std::bad_alloc();
    }
    size_t index = freeIndices.top();
    freeIndices.pop();
    return &pool[index];
  }

  void deallocate(T *ptr) {
    size_t index = ptr - pool;
    freeIndices.push(index);
  }

private:
  T *pool;
  std::stack<size_t> freeIndices;
};

struct SmallObject {
  int data[2];
  SmallObject() { std::cout << "SmallObject constructed\n"; }
  ~SmallObject() { std::cout << "SmallObject destructed\n"; }
};

int main() {
  MemoryPool<SmallObject, 10> pool;
  auto obj1 = pool.allocate();
  new (obj1) SmallObject();
  pool.deallocate(obj1);
}
