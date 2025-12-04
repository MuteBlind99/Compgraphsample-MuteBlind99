
function(checkshaders main_folder exe_name)
    if(MSVC)
        if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64" OR ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "ARM64")
            set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
        else()
            set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
        endif()
    elseif(UNIX)
        set(GLSL_VALIDATOR "glslangValidator")
    endif()

    file(GLOB_RECURSE GLSL_SOURCE_FILES
            "${main_folder}/data/*.frag"
            "${main_folder}/data/*.vert"
            "${main_folder}/data/*.comp"
    )
    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        get_filename_component(PATH_NAME ${GLSL} DIRECTORY)
        get_filename_component(EXTENSION ${GLSL} EXT)

        file(RELATIVE_PATH RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data/shaders" ${GLSL})
        get_filename_component(RELATIVE_PATH ${RELATIVE_PATH} DIRECTORY)

        source_group("Shader Files\\${RELATIVE_PATH}" FILES "${GLSL}")
        file(RELATIVE_PATH PATH_NAME "${main_folder}" ${PATH_NAME})
        #MESSAGE("GLSL PATH: ${PATH_NAME} NAME: ${FILE_NAME}")
        set(GLSL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PATH_NAME}/${FILE_NAME}")
        #MESSAGE("GLSL OUT PATH: ${GLSL_OUTPUT}")
        add_custom_command(
                OUTPUT ${GLSL_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E copy
                ${main_folder}/${PATH_NAME}/${FILE_NAME}
                ${GLSL_OUTPUT}
                COMMAND ${GLSL_VALIDATOR}  ${GLSL}
                DEPENDS ${GLSL})
        list(APPEND GLSL_OUTPUT_FILES ${GLSL_OUTPUT})
    endforeach(GLSL)

    set(SHADER_TARGET "${exe_name}_ShadersCheck")
    MESSAGE(${SHADER_TARGET})
    add_custom_target(
            "${exe_name}_ShadersCheck"
            DEPENDS ${GLSL_OUTPUT_FILES}
    )
    add_dependencies(${exe_name} "${exe_name}_ShadersCheck")
endfunction()