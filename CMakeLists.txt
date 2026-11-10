cmake_minimum_required(VERSION 3.16)
project(saturn_bt_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_executable(saturn_bt src/saturn_bt.cpp)
target_link_libraries(saturn_bt PRIVATE PkgConfig::SYSTEMD)

enable_testing()

add_executable(test_bridge tests/test_bridge.cpp)
target_include_directories(test_bridge PRIVATE src)
add_test(NAME bridge COMMAND test_bridge)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME mcp COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_saturn_mcp.py)
  set_tests_properties(mcp PROPERTIES ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
endif()

# sudo cmake --install build: the program to /usr/local/bin, the service to where systemd finds it
include(GNUInstallDirs)
install(TARGETS saturn_bt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(systemd/saturn-bt@.service.in saturn-bt@.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/saturn-bt@.service DESTINATION lib/systemd/system)
