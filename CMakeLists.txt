if(NOT CMAKE_BUILD_TYPE )
        set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32 AND NOT CMAKE_GENERATOR_PLATFORM)
        set(CMAKE_GENERATOR_PLATFORM x64)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(GTEST_LIB_SUFFIX d)
else()
        set(GTEST_LIB_SUFFIX "")
endif()

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
	set(CMAKE_CXX_STANDARD 17)
	cmake_minimum_required(VERSION 2.8.9)
	if(WIN32)
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
		set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
	else()
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
		set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
	endif()
endif()

if(WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(GTEST_LIBS "gtest${GTEST_LIB_SUFFIX}"
    "gtest_main${GTEST_LIB_SUFFIX}")
else()
	set(GTEST_LIBS "gtest${GTEST_LIB_SUFFIX}"
	"gtest_main${GTEST_LIB_SUFFIX}"
	pthread)
endif()

if(NOT TARGET CommonUtils)
	execute_process(COMMAND git clone https://github.com/sumitdhyani/CommonUtils.git "${CMAKE_SOURCE_DIR}/CommonUtils")
endif()

set(ALL_SOURCES ConditionVariable.hpp
TaskScheduler.hpp
TaskThrottlers.hpp
Timer.hpp
ConsumerThread.hpp
ThreadPool.hpp
WorkerThread.hpp)


project(MTTools)
add_library("${PROJECT_NAME}" STATIC  "${ALL_SOURCES}")
add_dependencies("${PROJECT_NAME}" CommonUtils)
#Needed for quaified paths, for projects which have external dependencies
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_SOURCE_DIR}")
set_target_properties("${PROJECT_NAME}" PROPERTIES LINKER_LANGUAGE CXX)

add_subdirectory("UnitTests")
