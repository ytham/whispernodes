project(whisper_client LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)
set(BOOST_ROOT /usr/local/Cellar/boost/1.67.0_1)
set(BOOST_INCLUDEDIR /usr/local/Cellar/boost/1.67.0_1/include)
set(BOOST_LIBRARYDIR /usr/local/Cellar/boost/1.67.0_1/lib)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_DEBUG 1)
find_package(Boost
  1.67.0
  REQUIRED
  COMPONENTS
  program_options
)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(whisper_client
  main.cpp
  Node.cpp
)

target_include_directories(whisper_client
  PUBLIC
  .
)

target_link_libraries(whisper_client
  Boost::program_options
)
