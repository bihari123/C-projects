#include <iostream>
#include <type_traits>

template <typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
process(const T &value) {
  std::cout << "Processing integarte type " << value << std::endl;
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, void>::type
process(const T &vallue) {
  std::cout << "Processing non integral type" << std::endl;
}

int main() {
  process(10);
  process(3.14);
  return 0;
}
