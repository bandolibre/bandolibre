cmake_minimum_required(VERSION 3.22)

# Host unit tests for the HAL-free modules. A separate project from the
# firmware's CMakeLists.txt: that one is bound to the ARM toolchain, these are
# built with the host compiler. Run through `just test`. A new HAL-free module
# gets one add_host_test() line here.

project(main-g474-host-tests C CXX)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/..)

# add_host_test(<name> <sources...>): build <name> from the sources (relative
# to the firmware directory) and register it with ctest.
function(add_host_test name)
    list(TRANSFORM ARGN PREPEND ${FW}/)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE
        ${FW}/app
        ${FW}/properties
        ${FW}/../common/console
        ${FW}/../third_party/gsl/include)
    target_link_libraries(${name} PRIVATE m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_host_test(one_euro_filter_test app/one_euro_filter_test.c)
add_host_test(hysteresis_test app/hysteresis_test.c)
add_host_test(properties_test
    properties/properties_test.c
    properties/properties.c
    properties/properties_console.c
    properties/property_persist.cc
    properties/property_store.cc
    properties/property_flash_fake.cc)
add_host_test(property_store_test
    properties/property_store.cc
    properties/property_store_test.cc)
add_host_test(bellow_classify_test app/bellow_classify.c app/bellow_classify_test.c)
add_host_test(bellow_curve_test app/bellow_curve.c app/bellow_curve_test.c)
add_host_test(keyboard_layout_test app/keyboard_layout.c app/keyboard_layout_test.c)
add_host_test(fixed_format_test app/fixed_format.c app/fixed_format_test.c)
