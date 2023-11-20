
add_executable(map map.cpp)

# add dependency - fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(map PRIVATE fmt::fmt)

#find_package(ctre::ctre CONFIG REQUIRED)
