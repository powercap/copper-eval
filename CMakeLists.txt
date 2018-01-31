cmake_minimum_required(VERSION 2.8.5)

project(copper-eval)
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# Currently we only support UNIX systems
include(GNUInstallDirs)


# Dependencies

set(RAPLCAP_IMPL raplcap-msr CACHE STRING "The RAPLCap implementation to use")

# Use OpenMP for timing
find_package(OpenMP REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HBSC REQUIRED heartbeats-simple-classic)
pkg_check_modules(COPPER REQUIRED copper)
pkg_check_modules(RAPLCAP REQUIRED ${RAPLCAP_IMPL}>=0.2.0)
pkg_check_modules(POET REQUIRED poet>=2.0.0)
pkg_check_modules(CPUFREQ REQUIRED cpufreq-bindings)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${HBSC_CFLAGS_OTHER} ${COPPER_CFLAGS_OTHER} ${RAPLCAP_CFLAGS_OTHER} ${POET_CFLAGS_OTHER} ${CPUFREQ_CFLAGS_OTHER}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -Wall -std=c99")

include_directories(${PROJECT_SOURCE_DIR}/inc
                    ${HBSC_INCLUDE_DIRS}
                    ${COPPER_INCLUDE_DIRS}
                    ${RAPLCAP_INCLUDE_DIRS}
                    ${POET_INCLUDE_DIRS}
                    ${CPUFREQ_INCLUDE_DIRS})

set(DYNAMIC_LINKS -L${HBSC_LIBDIR} ${HBSC_LIBRARIES}
                  -L${COPPER_LIBDIR} ${COPPER_LIBRARIES}
                  -L${RAPLCAP_LIBDIR} ${RAPLCAP_LIBRARIES}
                  -L${POET_LIBDIR} ${POET_LIBRARIES}
                  -L${CPUFREQ_LIBDIR} ${CPUFREQ_LIBRARIES})

set(STATIC_LINKS -L${HBSC_LIBDIR} ${HBSC_STATIC_LIBRARIES}
                 -L${COPPER_LIBDIR} ${COPPER_STATIC_LIBRARIES}
                 -L${RAPLCAP_LIBDIR} ${RAPLCAP_STATIC_LIBRARIES}
                 -L${POET_LIBDIR} ${POET_STATIC_LIBRARIES}
                 -L${CPUFREQ_LIBDIR} ${CPUFREQ_STATIC_LIBRARIES})


# Libraries

add_library(${PROJECT_NAME} src/copper-eval.c
                            src/copper-eval-util.c
                            src/copper-eval-copper.c
                            src/copper-eval-raplcap.c
                            src/copper-eval-poet.c
                            src/copper-eval-cpufreq-bindings.c
                            src/copper-eval-psc.c src/psc/psc.c src/psc/psc-util.c
                            src/copper-eval-dsc.c src/dsc/dsc.c src/dsc/dsc-util.c)
target_link_libraries(${PROJECT_NAME} ${DYNAMIC_LINKS})
if(BUILD_SHARED_LIBS)
  set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION}
                                                   SOVERSION ${VERSION_MAJOR})
endif()


# Binaries

add_executable(copper-eval-example src/copper-eval-example.c)
target_link_libraries(copper-eval-example ${PROJECT_NAME} ${STATIC_LINKS})

add_executable(overhead-eval src/overhead-eval.c)
target_link_libraries(overhead-eval ${PROJECT_NAME} ${CPUFREQ_STATIC_LIBRARIES})

add_executable(simulate-characterized-app src/simulate-characterized-app.c)
target_link_libraries(simulate-characterized-app ${PROJECT_NAME} ${CPUFREQ_STATIC_LIBRARIES})


# Tests

# we don't setup CTests because the test actually requires root privileges to run
add_executable(copper-eval-test test/copper-eval-test.c)
target_link_libraries(copper-eval-test ${PROJECT_NAME} ${STATIC_LINKS})


# pkg-config

set(PKG_CONFIG_EXEC_PREFIX "\${prefix}")
set(PKG_CONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")

set(PKG_CONFIG_NAME "${PROJECT_NAME}")
set(PKG_CONFIG_DESCRIPTION "CoPPer evaluation utilities")
set(PKG_CONFIG_REQUIRES_PRIVATE "heartbeats-simple-classic >= 0.1.0, copper >= 0.1.0, ${RAPLCAP_IMPL} >= 0.1.0, poet >= 2.0.0, cpufreq-bindings >= 0.1.0")
set(PKG_CONFIG_LIBS "-L\${libdir} -l${PROJECT_NAME}")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
)


# Install

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES inc/copper-eval.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


# Uninstall

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
  @ONLY
)

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
