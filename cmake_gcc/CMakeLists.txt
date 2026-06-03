# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	breathsense_mg21
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_breathsense_mg21 target,
# which contains the content of the SLC project
include(breathsense_mg21.cmake)

add_executable(breathsense_mg21
    # Add additional sources here
)

target_include_directories(breathsense_mg21 PUBLIC
    # Add additional include paths here
)

target_compile_definitions(breathsense_mg21 PUBLIC
    # Add additional macros here
)

target_compile_options(breathsense_mg21 PUBLIC
    # Set additional compiler flags here
)

target_link_options(breathsense_mg21 PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(breathsense_mg21 PRIVATE
    slc
)

# Include managed project content if available
include(breathsense_mg21_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(breathsense_mg21 PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET breathsense_mg21
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:breathsense_mg21>" "$<TARGET_FILE_DIR:breathsense_mg21>/$<TARGET_FILE_BASE_NAME:breathsense_mg21>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:breathsense_mg21>" "$<TARGET_FILE_DIR:breathsense_mg21>/$<TARGET_FILE_BASE_NAME:breathsense_mg21>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:breathsense_mg21>" "$<TARGET_FILE_DIR:breathsense_mg21>/$<TARGET_FILE_BASE_NAME:breathsense_mg21>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET breathsense_mg21
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()