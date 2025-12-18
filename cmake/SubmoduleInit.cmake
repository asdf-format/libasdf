function(submodule_init)
    foreach (path IN ITEMS ${ARGV0})
        message("Update submodule: ${path}")
        execute_process(
                COMMAND "git config get submodule.${path}.ignore"
                RESULT_VARIABLE ignored
        )
        execute_process(
                COMMAND "cd ${CMAKE_SOURCE_DIR} && git submodule update --init ${path}"
        )
        execute_process(
                COMMAND "cd ${CMAKE_SOURCE_DIR} && git config submodule.${path}.ignore untracked"
        )
    endforeach()
endfunction()
