
function(copydata main_folder exe_name)
    file(GLOB_RECURSE DATA_FILES
            "${main_folder}/data/*.json"
            "${main_folder}/data/*.png"
            "${main_folder}/data/*.jpg"
            "${main_folder}/data/*.cube"
            "${main_folder}/data/*.bmp"
            "${main_folder}/data/*.hdr"
            "${main_folder}/data/*.obj"
            "${main_folder}/data/*.mtl"
            "${main_folder}/data/*.gltf"
    )
    foreach(DATA ${DATA_FILES})
        get_filename_component(FILE_NAME ${DATA} NAME)
        get_filename_component(PATH_NAME ${DATA} DIRECTORY)
        get_filename_component(EXTENSION ${DATA} EXT)
        file(RELATIVE_PATH PATH_NAME "${main_folder}" ${PATH_NAME})
        #MESSAGE("Data PATH: ${PATH_NAME} NAME: ${FILE_NAME} EXTENSION: ${EXTENSION}")
        set(DATA_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PATH_NAME}/${FILE_NAME}")

        if("${EXTENSION}" STREQUAL ".py")
            source_group("Script Files\\${RELATIVE_PATH}" FILES "${DATA}")
        endif()

        if("${EXTENSION}" STREQUAL ".lua")
            source_group("Script Files\\${RELATIVE_PATH}" FILES "${DATA}")
        endif()
        #MESSAGE("Data OUT PATH: ${DATA_OUTPUT}")
        add_custom_command(
                OUTPUT ${DATA_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E copy
                ${main_folder}/${PATH_NAME}/${FILE_NAME}
                ${DATA_OUTPUT}
                DEPENDS ${DATA})
        list(APPEND Data_OUTPUT_FILES ${DATA_OUTPUT})
    endforeach(DATA)

    set(DATA_TARGET "${exe_name}_Data")
    MESSAGE(${DATA_TARGET})

    add_custom_target(
            "${exe_name}_DATA"
            DEPENDS ${DATA_FILES} ${Data_OUTPUT_FILES}
    )
    add_dependencies(${exe_name} "${exe_name}_DATA")
endfunction()