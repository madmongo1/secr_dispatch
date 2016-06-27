add_subdirectory(http)
add_subdirectory(api)
add_subdirectory(asioex)

add_sources(
    CMakeLists.txt
    config.hpp
    ownership.hpp
    polymorphic_stream.hpp
    buffered_stream.hpp
    fake_stream.hpp
    stream.hpp
    string_view.hpp
)
