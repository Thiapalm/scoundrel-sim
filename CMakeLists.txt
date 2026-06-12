cmake_minimum_required(VERSION 3.16)
project(ScoundrelSim)

# Assuming ScoundrelCore is in the parent directory for isolated development
set(BUILD_AS_SUBMODULE ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/../scoundrel-core scoundrel-core)

add_executable(ScoundrelSimulator src/BalanceSimulator.cpp)

target_link_libraries(ScoundrelSimulator PRIVATE ScoundrelCore)

target_include_directories(ScoundrelSimulator PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
