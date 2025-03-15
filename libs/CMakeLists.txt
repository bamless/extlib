# Add static libraries, one per file. Use interface libraries for header only

add_library(extassert INTERFACE ${PROJECT_SOURCE_DIR}/include/extlib/ext_assert.h)
target_include_directories(extassert
    INTERFACE
        $<INSTALL_INTERFACE:include/extlib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/extlib>
)

add_library(extvector INTERFACE  ${PROJECT_SOURCE_DIR}/include/extlib/ext_vector.h)
target_link_libraries(extvector INTERFACE extassert)
target_include_directories(extvector
    INTERFACE
        $<INSTALL_INTERFACE:include/extlib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/extlib>
)

add_library(extstring STATIC ext_string.c ${PROJECT_SOURCE_DIR}/include/extlib/ext_string.h)
target_link_libraries(extstring INTERFACE extvector PRIVATE extassert)
target_include_directories(extstring
    PUBLIC
        $<INSTALL_INTERFACE:include/extlib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/extlib>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include/extlib
)

add_library(extmap STATIC ext_map.c ${PROJECT_SOURCE_DIR}/include/extlib/ext_map.h)
target_link_libraries(extmap PRIVATE extassert)
target_include_directories(extmap
    PUBLIC
        $<INSTALL_INTERFACE:include/extlib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/extlib>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include/extlib
)

# Enable link-time optimization if supported
if(LTO)
    set_target_properties(extstring PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_target_properties(extmap    PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Install
install(TARGETS extassert extvector extstring extmap
    EXPORT  extlib-export
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install header files
install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include
    PATTERN "*.h.in" EXCLUDE
)
