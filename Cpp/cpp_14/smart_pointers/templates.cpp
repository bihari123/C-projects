#include <iostream>
using namespace std;
template <typename T> T IsLarger(T arg1, T arg2) { return (arg1 > arg2); }
#define FRIENDLY_BOOL(x) ((x) ? " is " : " isn't ")

// template with more than one data type
template <typename T, typename U> auto max(T x, U y) { return (x > y) ? x : y; }

int main() {
  int x = 100, y = 20;
  cout << x << FRIENDLY_BOOL(IsLarger(x, y)) << "larger than "
       << " y " << endl;

  char a = 'a', b = 'b';
  cout << a << FRIENDLY_BOOL(IsLarger(a, a)) << "larger than "
       << " b" << endl;
}
