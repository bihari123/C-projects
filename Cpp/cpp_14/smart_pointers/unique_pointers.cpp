#include <iostream>
#include <memory>
using namespace std;
class Chocolate {
public:
  Chocolate() { std::cout << "Chocolate has been acquired\n"; }
  ~Chocolate() { std::cout << "Chocolate has been destroyed\n"; }
};
int main() {
  std::unique_ptr<Chocolate> myWhiteChocolcate{new Chocolate{}};

  std::unique_ptr<Chocolate> mydarkChocolate{}; // start as nullptr
                                                //
  std::cout << "myWhiteChocolate is"
            << (static_cast<bool>(myWhiteChocolcate) ? "not null\n" : "null\n");

  std::cout << "myDarkChocolate is "
            << (static_cast<bool>(mydarkChocolate) ? "not null\n" : "null\n");

  // we can't do
  // mydarkChocolate = myWhiteChocolcate
  // as the copy assignment is disabled
  // So let's do it the right way by calling the std::move()
  mydarkChocolate =
      std::move(myWhiteChocolcate); // myDarkChocolate assumes the Ownership,
                                    // myWhiteChocolate is set to null

  std::cout << "Ownership tranferred\n";

  std::cout << "myWhiteChocolate is "
            << (static_cast<bool>(myWhiteChocolcate) ? "not null\n" : "null\n");
  std::cout << "myDarkChocolate is "
            << (static_cast<bool>(mydarkChocolate) ? "not null\n" : "null\n");
}
