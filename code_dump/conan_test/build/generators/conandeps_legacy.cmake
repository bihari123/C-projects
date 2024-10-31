message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(fmt)
find_package(Boost)
find_package(Eigen3)
find_package(nlohmann_json)

set(CONANDEPS_LEGACY  fmt::fmt  boost::boost  Eigen3::Eigen  nlohmann_json::nlohmann_json )