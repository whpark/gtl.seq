
add_executable(basic main.cpp)

# add dependency - fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(basic PRIVATE fmt::fmt)

#find_package(ctre::ctre CONFIG REQUIRED)
