cmake_minimum_required (VERSION 3.13)

link_libraries(
    Boost::iostreams
    Boost::system
    ouinet::base
)

add_executable(test_file_io
    "test_file_io.cpp"
    "../src/util/file_io.cpp")
