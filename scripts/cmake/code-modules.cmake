# SETS EXECUTABLE TO OUTPUT TO A SPECIFIC DIRECTORY
function(target_output_directory executable directory)
    set_target_properties(${executable} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${directory}
    )
endfunction()

# ENABLES UNITY BUILDS ON TARGETS
function(target_enable_unity target)
    set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
    set_target_properties(${target} PROPERTIES UNITY_BUILD_BATCH_SIZE 8)
endfunction()

# DISABLES UNITY BUILD ON A SPECIFIC FILE
function(source_exclude_unity file)
    set_source_files_properties(${file} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endfunction()

# FUNCTION THAT DECLARES ALL THE DEFAULTS OF A MODULE
function(module_default_init module)

    file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS "public/*.hpp")
    file(GLOB_RECURSE private_sources CONFIGURE_DEPENDS "private/*.cpp")
    file(GLOB_RECURSE private_headers CONFIGURE_DEPENDS "private/*.hpp")

    target_sources(${module} PUBLIC ${public_files} PRIVATE ${private_headers} PRIVATE ${private_sources})
    target_include_directories(${module} PUBLIC "public" PRIVATE "private")

    if (ENABLE_PCH)
        target_link_libraries(${module} PRIVATE PCH)
        target_precompile_headers(${module} REUSE_FROM PCH)
    endif ()

    if (ENABLE_TESTS)

        file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "tests/*.cpp")

        if (test_sources)    
            target_sources(KSTests PRIVATE ${test_sources})
            target_link_libraries(KSTests PRIVATE ${module})
            target_include_directories(KSTests PRIVATE "private")
        endif ()
        
    endif ()

    if (ENABLE_UNITY)
        target_enable_unity(${module})
    endif ()

endfunction()

# FUNCTION THAT ADDS A MODULE TO THE MAIN ENGINE LIBRARY
function(target_add_module target module)
    message(STATUS "### - ${module} Module")
    add_subdirectory(${module})
    target_link_libraries(${target} INTERFACE ${module})
endfunction()