include(CMakeParseArguments)

find_program(CLANG_TIDY_EXE 
    NAMES 
        clang-tidy
        clang-tidy-4.0
        clang-tidy-3.9
        clang-tidy-3.8
        clang-tidy-3.7
        clang-tidy-3.6
        clang-tidy-3.5
    PATHS
        /usr/local/opt/llvm/bin
)

function(find_clang_tidy_version VAR)
    execute_process(COMMAND ${CLANG_TIDY_EXE} -version OUTPUT_VARIABLE VERSION_OUTPUT)
    separate_arguments(VERSION_OUTPUT_LIST UNIX_COMMAND "${VERSION_OUTPUT}")
    list(FIND VERSION_OUTPUT_LIST "version" VERSION_INDEX)
    math(EXPR VERSION_INDEX "${VERSION_INDEX} + 1")
    list(GET VERSION_OUTPUT_LIST ${VERSION_INDEX} VERSION)
    set(${VAR} ${VERSION} PARENT_SCOPE)
endfunction()

if( NOT CLANG_TIDY_EXE )
    message( STATUS "Clang tidy not found" )
    set(CLANG_TIDY_VERSION "0.0")
else()
    find_clang_tidy_version(CLANG_TIDY_VERSION)
    message( STATUS "Clang tidy found: ${CLANG_TIDY_VERSION}")
endif()



set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

macro(enable_clang_tidy)
    set(options ANALYZE_TEMPORARY_DTORS)
    set(oneValueArgs HEADER_FILTER)
    set(multiValueArgs CHECKS ERRORS EXTRA_ARGS)

    cmake_parse_arguments(PARSE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    string(REPLACE ";" "," CLANG_TIDY_CHECKS "${PARSE_CHECKS}")
    string(REPLACE ";" "," CLANG_TIDY_ERRORS "${PARSE_ERRORS}")
    string(REPLACE ";" " " CLANG_TIDY_EXTRA_ARGS "${PARSE_EXTRA_ARGS}")
    
    message(STATUS "Clang tidy checks: ${CLANG_TIDY_CHECKS}")

    if (${PARSE_ANALYZE_TEMPORARY_DTORS})
        set(CLANG_TIDY_ANALYZE_TEMPORARY_DTORS "-analyze-temporary-dtors")
    endif()

    if (${CLANG_TIDY_VERSION} VERSION_LESS "3.9.0")
        set(CLANG_TIDY_ERRORS_ARG "")
    else()
        set(CLANG_TIDY_ERRORS_ARG "-warnings-as-errors='${CLANG_TIDY_ERRORS}'")
    endif()

    if(PARSE_HEADER_FILTER)
        string(REPLACE "$" "$$" CLANG_TIDY_HEADER_FILTER "${PARSE_HEADER_FILTER}")
    else()
        set(CLANG_TIDY_HEADER_FILTER ".*")
    endif()

    set(CLANG_TIDY_COMMAND 
        ${CLANG_TIDY_EXE} 
        -p ${CMAKE_BINARY_DIR} 
        -checks='${CLANG_TIDY_CHECKS}'
        ${CLANG_TIDY_ERRORS_ARG}
        -extra-arg='${CLANG_TIDY_EXTRA_ARGS}'
        ${CLANG_TIDY_ANALYZE_TEMPORARY_DTORS}
        -header-filter='${CLANG_TIDY_HEADER_FILTER}'
    )
    add_custom_target(tidy)
endmacro()

function(clang_tidy_check TARGET)
    get_target_property(SOURCES ${TARGET} SOURCES)
    add_custom_target(tidy-${TARGET}
        COMMAND ${CLANG_TIDY_COMMAND} ${SOURCES}
        # TODO: Use generator expressions instead
        # COMMAND ${CLANG_TIDY_COMMAND} $<TARGET_PROPERTY:${TARGET},SOURCES>
        # COMMAND ${CLANG_TIDY_COMMAND} $<JOIN:$<TARGET_PROPERTY:${TARGET},SOURCES>, >
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "clang-tidy: Running clang-tidy on target ${TARGET}..."
    )
    add_dependencies(tidy tidy-${TARGET})
endfunction()

