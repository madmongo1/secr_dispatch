
project (dispatch CXX)

sanity_require(LIBRARY protobuf VERSION any)
sanity_require(LIBRARY openssl VERSION any)
sanity_require(LIBRARY boost VERSION any COMPONENTS log system)

set (PROTO_PROTOS exception.proto secr_dispatch_test.proto secr_dispatch_http.proto)


protobuf_configure_files (CPP 
							CPP_HEADERS PROTO_HDRS
							CPP_SOURCES PROTO_SRCS
							FILES ${PROTO_PROTOS})

set(interface_name "dispatch")
set(proto_lib "secr_dispatch_proto")

set_property (SOURCE ${PROTO_PROTOS} PROPERTY HEADER_FILE_ONLY ON)
add_library(${proto_lib} ${PROTO_HDRS} ${PROTO_SRCS} ${PROTO_PROTOS})
target_link_libraries(${proto_lib} sanity::protobuf)

# install the .h files into /include/secr/dispatch
# install the .proto files into /share/secr/dispatch

set (binstall_h_path "${CMAKE_CURRENT_BINARY_DIR}/include/secr/${interface_name}")
set (binstall_proto_path "${CMAKE_CURRENT_BINARY_DIR}/share/secr/${interface_name}")
add_custom_command (TARGET ${proto_lib} 
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory 
                         ${binstall_h_path})
foreach (_h ${PROTO_HDRS})
    add_custom_command (TARGET ${proto_lib} 
    					POST_BUILD
        				COMMAND ${CMAKE_COMMAND} -E copy 
        					"${_h}" 
        					${binstall_h_path})
endforeach ()

add_custom_command (TARGET ${proto_lib} 
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory 
                         ${binstall_proto_path})
foreach (_proto ${PROTO_PROTOS})
    add_custom_command (TARGET ${proto_lib} 
    					POST_BUILD
        				COMMAND ${CMAKE_COMMAND} -E copy 
        					"${CMAKE_CURRENT_SOURCE_DIR}/${_proto}"
        					 ${binstall_proto_path})
endforeach ()

target_include_directories(${proto_lib}
    SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>)

#
# build utility project
#

interface_tree(include)
source_tree(src)
test_tree(tests)

####
# Create targets
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/src")
add_library(secr_dispatch ${INTERFACE_FILES} ${SOURCE_FILES})

target_link_libraries(secr_dispatch 
                        PUBLIC  http_parser
                                secr_dispatch_proto
                                ValueLib_debug
                                ValueLib_data
                                ValueLib_stdext
                                sanity::openssl
                                boost::log
                                boost::system)

target_include_directories(secr_dispatch
    SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    )

#
# test
#

sanity_require(LIBRARY gtest VERSION any)
sanity_require(LIBRARY boost VERSION any COMPONENTS thread system)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/tests")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/tests")
add_executable(secr_dispatch_tests ${TEST_FILES})
target_link_libraries(secr_dispatch_tests secr_dispatch sanity::gtest::main boost::thread boost::system)



add_test(AllTestsInSecrDispatch secr_dispatch_tests)


