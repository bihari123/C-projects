#include <cstddef>
#include <iostream>
class Buffer {
private:
  int *data;
  std::size_t size;

public:
  Buffer(std::size_t size) : size(size), data(new int[size]) {
    std::cout << "Buffer of size " << size << " allocate";
  }
  ~Buffer() {
    delete[] data;
    std::cout << "Buffer deallocate\n";
  }

  // Move constructor
  Buffer(Buffer &&other) noexcept : data(other.data), size(other.size) {
    other.data = nullptr;
    other.size = 0;
    std::cout << "Buffer moved\n";
  }

  // copy constructor (deleted to emphasize on move semantics)
  Buffer(const Buffer &) = delete;

  // assignment operator (deleted for brevity)
  Buffer &operator=(const Buffer &) = delete;
  Buffer &operator=(Buffer &&) = delete;
};

int main() {
  Buffer a(10);
  Buffer b(std::move(a)); // move a to b
  return 0;
}
