cmake_minimum_required(VERSION 3.9)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(test_lib STATIC
	src/loot_generator.h
	src/loot_generator.cpp
	src/collision_detector.h
	src/collision_detector.cpp
	src/geometry.h
)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/app.h
	src/player_tokens.h
	src/player_tokens.cpp
	src/players.h
	src/players.cpp
	src/tagged.h
	src/geometry.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/logging_handler.h
	src/api_request_handler.cpp
	src/api_request_handler.h
	src/static_request_handler.cpp
	src/static_request_handler.h
	src/state_serialization.h
	src/state_serialization.cpp
	src/utils.h
	src/utils.cpp
)

#target_include_directories(game_server PRIVATE CONAN_PKG::boost)
#target_link_libraries(game_server PRIVATE Threads::Threads)
target_link_libraries(game_server
    PRIVATE
        CONAN_PKG::boost
        Threads::Threads
	test_lib
)

add_executable(tests
	tests/loot_generator_tests.cpp
	tests/collision-detector-tests.cpp
)

target_link_libraries(tests PRIVATE CONAN_PKG::catch2 test_lib)
