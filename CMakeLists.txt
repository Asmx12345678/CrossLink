cmake_minimum_required(VERSION 3.16)
project(CrossLink VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CROSSLINK_BUILD_TESTS "Build CrossLink tests" ON)

add_library(crosslink_phy
    src/phy.cpp
    src/ofdm.cpp
)
target_include_directories(crosslink_phy PUBLIC include)
target_compile_options(crosslink_phy PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_executable(crosslink-link apps/link_sim.cpp)
target_link_libraries(crosslink-link PRIVATE crosslink_phy)

add_executable(crosslink-ofdm apps/ofdm_sim.cpp)
target_link_libraries(crosslink-ofdm PRIVATE crosslink_phy)

add_executable(crosslink-amc apps/adaptive_mcs.cpp)
target_link_libraries(crosslink-amc PRIVATE crosslink_phy)

if(CROSSLINK_BUILD_TESTS)
    enable_testing()
    add_executable(crosslink-phy-tests tests/test_phy.cpp)
    target_link_libraries(crosslink-phy-tests PRIVATE crosslink_phy)
    add_test(NAME phy COMMAND crosslink-phy-tests)
endif()

install(TARGETS crosslink-link crosslink-ofdm crosslink-amc RUNTIME DESTINATION bin)
