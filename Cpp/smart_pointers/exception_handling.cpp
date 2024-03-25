#include <iostream>

class DivideByZeroEx {};
class NegativeNum {};

double calculate_mpg(int miles, int gallons) {
  if (gallons == 0) {
    throw DivideByZeroEx();
  }
  if (miles < 0 || gallons < 0)
    throw NegativeNum();

  return static_cast<double>(miles) / gallons;
}

int main() {
  int miles{};
  int gallons{};

  double miles_per_gallons{};

  std::cout << "Enter the distance you drove in miles ";
  std::cin >> miles;

  std::cout << "enter the number of gallons you had";
  std::cin >> gallons;

  try {
    miles_per_gallons = calculate_mpg(miles, gallons);
    std::cout << "Result: your car uses" << miles_per_gallons << " per miles"
              << std::endl;
  } catch (const DivideByZeroEx &ex) {
    std::cerr << "Error! 0 gallons are not an option" << std::endl;
  } catch (const NegativeNum &ex) {
    std::cerr << "Error ! You cannot use negative values" << std::endl;
  }
  std::cout << "Thank You and goodbye" << std::endl;
}
