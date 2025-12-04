cmake_minimum_required (VERSION 3.18)
project(GPR924-CompGraph)

set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_TOOLCHAIN_FILE "C:/Users/forna/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake")


# Recherche des dépendances OpenGL
find_package(OpenGL REQUIRED)


include(cmake/data.cmake)
include(cmake/shaders.cmake)

add_subdirectory(externals/GPR924-Engine)

file(GLOB_RECURSE SRC_FILE src/*.cc include/*.h)

add_library(CompGraphLib STATIC ${SRC_FILE})
target_include_directories(CompGraphLib PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR}/third_party)  # Ajoutez ce chemin
target_link_libraries(CompGraphLib PUBLIC common)



checkshaders("${CMAKE_SOURCE_DIR}" CompGraphLib)
copydata("${CMAKE_SOURCE_DIR}" CompGraphLib)

# Création manuelle de TOUS les exécutables

# Sample 1: Triangle avec VAO vide
add_executable(triangle_empty_vao main/main_triangle_empty_vao.cc)
target_link_libraries(triangle_empty_vao PUBLIC CompGraphLib)

# Sample 2: Triangle avec VBO
add_executable(triangle_vbo main/main_triangle_vbo.cc)
target_link_libraries(triangle_vbo PUBLIC CompGraphLib)

# Sample 3: Quad avec VAO et VBO
add_executable(quad_vbo main/main_quad_vbo.cc)
target_link_libraries(quad_vbo PUBLIC CompGraphLib)

# Sample 4: Quad avec VAO et VBO+EBO
 add_executable(quad_ebo main/main_quad_ebo.cc
         main/main_quad_texture_stb.cc
         main/shader.cc
         main/main_sample_cube.cc)
 target_link_libraries(quad_ebo PUBLIC CompGraphLib)

# Sample 5: Quad avec Uniform
 add_executable(quad_uniform main/main_quad_uniform.cc)
 target_link_libraries(quad_uniform PUBLIC CompGraphLib)

# Sample 6: Quad avec Texture rotation
 add_executable(quad_texture main/main_quad_texture.cc)
 target_link_libraries(quad_texture PUBLIC CompGraphLib)

# Sample 7: Quad avec Texture stb no rotation
add_executable(quad_texture_stb main/main_quad_texture_stb.cc)
target_link_libraries(quad_texture PUBLIC CompGraphLib)

# Sample 8: Cube avec Texture rotation
add_executable(cube_texture main/main_sample_cube.cc)
target_link_libraries(cube_texture PUBLIC CompGraphLib)

# Option 2: Boucle automatique pour les autres fichiers main (sauf ceux déjà créés)

# Liste des fichiers main déjà traités manuellement
#set(MANUALLY_CREATED
#        main/main_triangle_empty_vao.cc
#        main/main_triangle_vbo.cc
#)

# Récupère tous les fichiers main
file(GLOB ALL_MAIN_FILES main/*.cc)

foreach(main_file ${ALL_MAIN_FILES})
    # Vérifie si le fichier n'a pas déjà été traité manuellement
    list(FIND MANUALLY_CREATED ${main_file} _index)
    if(${_index} EQUAL -1)
        # Crée l'exécutable pour ce fichier
        get_filename_component(target ${main_file} NAME_WE)
        message(STATUS "Creating executable ${target} from ${main_file}")

        add_executable(${target} ${main_file}
                include/Shader.h)
        target_link_libraries(${target} PUBLIC CompGraphLib)
    endif()
endforeach()
function(copydata SRC_DIR TARGET_NAME)

    # Copier les textures
    file(GLOB_RECURSE TEXTURE_FILES "${SRC_DIR}/textures/*.jpg" "${SRC_DIR}/textures/*.png")
    foreach(texture_file ${TEXTURE_FILES})
        file(RELATIVE_PATH RELATIVE_PATH "${SRC_DIR}" ${texture_file})
        get_filename_component(RELATIVE_DIR ${RELATIVE_PATH} DIRECTORY)

        add_custom_command(
                TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${RELATIVE_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy ${texture_file} "${CMAKE_BINARY_DIR}/${RELATIVE_PATH}"
                COMMENT "Copying texture: ${RELATIVE_PATH}"
        )
    endforeach()
endfunction()






