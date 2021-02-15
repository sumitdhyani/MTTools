set (ALL_HEADERS ../ThreadPool.hpp
../TaskScheduler.hpp
../TaskThrottlers.hpp
../Timer.hpp
../WorkerThread.hpp)


set (ALL_SOURCES MTToolsTest.cpp
ThrottlingTests.cpp
ThreadPoolTests.cpp
TimerTests.cpp)

set (ALL_HEADERS_AND_SOURCES ${ALL_HEADERS} ${ALL_SOURCES})

project(MTToolsTest)
add_executable("${PROJECT_NAME}" "${ALL_HEADERS_AND_SOURCES}")
add_dependencies("${PROJECT_NAME}" MTTools)
target_include_directories("${PROJECT_NAME}" PUBLIC  "$ENV{BOOST_ROOT}" "$ENV{GTEST_ROOT}/googletest/include" ..)
target_link_directories("${PROJECT_NAME}" PUBLIC "$ENV{BOOST_ROOT}/stage/lib" "$ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE}" "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}" "${CMAKE_SOURCE_DIR}/${CMAKE_BUILD_TYPE}")
target_link_libraries("${PROJECT_NAME}" "${GTEST_LIBS}" )
