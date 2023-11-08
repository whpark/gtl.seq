# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("gtl.seq")

include_directories ("include")

#add C++20
set(CMAKE_CXX_STANDARD 20)

# Include sub-projects.
add_subdirectory ("examples")
