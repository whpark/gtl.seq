
add_executable(map main.cpp)

# add dependency - fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(map PRIVATE fmt::fmt)
