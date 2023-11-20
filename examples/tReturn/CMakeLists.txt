
add_executable(tReturn tReturn.cpp)

# add dependency - fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(tReturn PRIVATE fmt::fmt)

#find_package(ctre::ctre CONFIG REQUIRED)
