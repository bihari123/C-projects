#include <iostream>
#include <type_traits>

template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
square(const T &x) {
  std::cout << "Integral sql=uare called " << std::endl;
  return x * x;
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
square(const T &x) {
  std::cout << "floating point square is called" << std::endl;
  return x * x;
}
