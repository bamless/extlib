add_executable(example example.c)
target_link_libraries(example PRIVATE extvector extstring extmap)

# Enable link-time optimization if supported
if(LTO)
    set_target_properties(example PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()