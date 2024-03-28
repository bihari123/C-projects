#include <algorithm>
#include <iostream>

template <typename T> class myArray {
private:
  T *array;
  size_t length;

public:
  myArray(size_t len) : length(len), array(new T[len]) {
    std::cout << "Array of size " << len << "allocated \n";
  }
  ~myArray() {
    delete[] array;
    std::cout << "Array deallocated\n";
  }

  T &operator[](size_t index) { return array[index]; }
};

int main() {
  {
    myArray<int> thisArray(5);

    for (int i = 0; i < 5; i++) {
      thisArray[i] = i * 5;
    }
    return 0;
  }
}
