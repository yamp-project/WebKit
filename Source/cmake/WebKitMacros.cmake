# This file is for macros that are used by multiple projects. If your macro is
# exclusively needed in only one subdirectory of Source (e.g. only needed by
# WebCore), then put it there instead.

macro(WEBKIT_COMPUTE_SOURCES _framework)
    set(_derivedSourcesPath ${${_framework}_DERIVED_SOURCES_DIR})

    foreach (_sourcesListFile IN LISTS ${_framework}_UNIFIED_SOURCE_LIST_FILES)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}" "${_derivedSourcesPath}/${_sourcesListFile}" COPYONLY)
      message(STATUS "Using source list file: ${_sourcesListFile}")

      list(APPEND _sourceListFileTruePaths "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}")
    endforeach ()

    set(gusb_args --derived-sources-path ${_derivedSourcesPath} --source-tree-path ${CMAKE_CURRENT_SOURCE_DIR})
    # Windows needs a larger bundle size because that helps keep WebCore.lib's size below the 4GB maximum in debug builds.
    if (MSVC AND ${_framework} STREQUAL "WebCore" AND ${_framework}_LIBRARY_TYPE STREQUAL "STATIC")
        list(APPEND gusb_args --max-bundle-size 16)
    endif ()

    if (ENABLE_UNIFIED_BUILDS)
        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            ${gusb_args}
            "--print-bundled-sources"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        foreach (_sourceFileTmp IN LISTS _outputTmp)
            set_source_files_properties(${_sourceFileTmp} PROPERTIES HEADER_FILE_ONLY ON)
            list(APPEND ${_framework}_HEADERS ${_sourceFileTmp})
        endforeach ()
        unset(_sourceFileTmp)

        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            ${gusb_args}
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE  _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
            message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        foreach (_file IN LISTS _outputTmp)
            if (_file MATCHES "\\.c$")
                list(APPEND ${_framework}_C_SOURCES ${_file})
            else ()
                list(APPEND ${_framework}_SOURCES ${_file})
            endif ()
        endforeach ()

        unset(_resultTmp)
        unset(_outputTmp)
    else ()
        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            ${gusb_args}
            "--print-all-sources"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        list(APPEND ${_framework}_SOURCES ${_outputTmp})
        unset(_resultTmp)
        unset(_outputTmp)
    endif ()
endmacro()

macro(WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS)
    set(_file ${CMAKE_CURRENT_SOURCE_DIR}/Platform${PORT}.cmake)
    if (EXISTS ${_file})
        message(STATUS "Using platform-specific CMakeLists: ${_file}")
        include(${_file})
    else ()
        message(STATUS "Platform-specific CMakeLists not found: ${_file}")
    endif ()
endmacro()

# Append the given dependencies to the source file
macro(WEBKIT_ADD_SOURCE_DEPENDENCIES _source _deps)
    set(_tmp)
    get_source_file_property(_tmp ${_source} OBJECT_DEPENDS)
    if (NOT _tmp)
        set(_tmp "")
    endif ()

    foreach (f ${_deps})
        list(APPEND _tmp "${f}")
    endforeach ()

    set_source_files_properties(${_source} PROPERTIES OBJECT_DEPENDS "${_tmp}")
    unset(_tmp)
endmacro()

macro(WEBKIT_FRAMEWORK_DECLARE _target)
    # add_library() without any source files triggers CMake warning
    # Addition of dummy "source" file does not result in any changes in generated build.ninja file
    add_library(${_target} ${${_target}_LIBRARY_TYPE} "${CMAKE_BINARY_DIR}/cmakeconfig.h")
endmacro()

macro(WEBKIT_LIBRARY_DECLARE _target)
    # add_library() without any source files triggers CMake warning
    # Addition of dummy "source" file does not result in any changes in generated build.ninja file
    add_library(${_target} ${${_target}_LIBRARY_TYPE} "${CMAKE_BINARY_DIR}/cmakeconfig.h")

    if (${_target}_LIBRARY_TYPE STREQUAL "OBJECT")
        list(APPEND ${_target}_INTERFACE_LIBRARIES $<TARGET_OBJECTS:${_target}>)
        if (TARGET ${_target}_c)
            list(APPEND ${_target}_INTERFACE_LIBRARIES $<TARGET_OBJECTS:${_target}_c>)
        endif ()
    endif ()
endmacro()

macro(WEBKIT_EXECUTABLE_DECLARE _target)
    add_executable(${_target} "${CMAKE_BINARY_DIR}/cmakeconfig.h")
endmacro()

# Private macro for setting the properties of a target.
macro(_WEBKIT_TARGET_SETUP _target _logical_name)
    target_include_directories(${_target} PUBLIC "$<BUILD_INTERFACE:${${_logical_name}_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} SYSTEM PRIVATE "$<BUILD_INTERFACE:${${_logical_name}_SYSTEM_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} PRIVATE "$<BUILD_INTERFACE:${${_logical_name}_PRIVATE_INCLUDE_DIRECTORIES}>")

    if (DEVELOPER_MODE_CXX_FLAGS)
        target_compile_options(${_target} PRIVATE ${DEVELOPER_MODE_CXX_FLAGS})
    endif ()

    target_compile_definitions(${_target} PRIVATE "BUILDING_${_logical_name}")
    if (${_logical_name}_DEFINITIONS)
        target_compile_definitions(${_target} PUBLIC ${${_logical_name}_DEFINITIONS})
    endif ()
    if (${_logical_name}_PRIVATE_DEFINITIONS)
        target_compile_definitions(${_target} PRIVATE ${${_logical_name}_PRIVATE_DEFINITIONS})
    endif ()

    if (${_logical_name}_COMPILE_OPTIONS)
        target_compile_options(${_target} PRIVATE ${${_logical_name}_COMPILE_OPTIONS})
    endif ()

    # Filter out missing targets from LIBRARIES
    if (${_logical_name}_LIBRARIES)
        set(_filtered_libs)
        foreach(_lib IN LISTS ${_logical_name}_LIBRARIES)
            if (_lib MATCHES "\\$<TARGET_OBJECTS:(.+)>")
                set(_target_name ${CMAKE_MATCH_1})
                if (TARGET ${_target_name})
                    list(APPEND _filtered_libs ${_lib})
                else()
                    message(STATUS "Skipping missing target object: ${_target_name}")
                endif()
            elseif (_lib MATCHES "::" AND NOT TARGET ${_lib})
                message(STATUS "Skipping missing imported target: ${_lib}")
            else()
                list(APPEND _filtered_libs ${_lib})
            endif()
        endforeach()
        if (_filtered_libs)
            target_link_libraries(${_target} PUBLIC ${_filtered_libs})
        endif()
    endif ()

    if (${_logical_name}_PRIVATE_LIBRARIES)
        set(_filtered_private_libs)
        foreach(_lib IN LISTS ${_logical_name}_PRIVATE_LIBRARIES)
            if (_lib MATCHES "\\$<TARGET_OBJECTS:(.+)>")
                set(_target_name ${CMAKE_MATCH_1})
                if (TARGET ${_target_name})
                    list(APPEND _filtered_private_libs ${_lib})
                else()
                    message(STATUS "Skipping missing target object: ${_target_name}")
                endif()
            elseif (_lib MATCHES "::" AND NOT TARGET ${_lib})
                message(STATUS "Skipping missing imported target: ${_lib}")
            else()
                list(APPEND _filtered_private_libs ${_lib})
            endif()
        endforeach()
        if (_filtered_private_libs)
            target_link_libraries(${_target} PRIVATE ${_filtered_private_libs})
        endif()
    endif ()

    if (${_logical_name}_DEPENDENCIES)
        add_dependencies(${_target} ${${_logical_name}_DEPENDENCIES})
    endif ()
endmacro()

macro(_WEBKIT_TARGET _target)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        if (${_target}_C_SOURCES)
            add_library(${_target}_c OBJECT)
            target_sources(${_target}_c PRIVATE ${${_target}_C_SOURCES})

            _WEBKIT_TARGET_SETUP(${_target}_c ${_target})

            set_target_properties(${_target}_c PROPERTIES C_STANDARD 17)
            list(APPEND ${_target}_PRIVATE_LIBRARIES ${_target}_c)
        endif ()

        target_sources(${_target} PRIVATE
            ${${_target}_HEADERS}
            ${${_target}_SOURCES}
        )

        _WEBKIT_TARGET_SETUP(${_target} ${_target})
    else ()
        target_sources(${_target} PRIVATE
            ${${_target}_HEADERS}
            ${${_target}_SOURCES}
            ${${_target}_C_SOURCES}
        )

        _WEBKIT_TARGET_SETUP(${_target} ${_target})
    endif ()
endmacro()

macro(_WEBKIT_TARGET_ANALYZE _target)
    if (ClangTidy_EXE)
        set(_clang_path_and_options
            ${ClangTidy_EXE}
            # Include all non system headers
            --header-filter=.*
        )
        set_target_properties(${_target} PROPERTIES
            C_CLANG_TIDY "${_clang_path_and_options}"
            CXX_CLANG_TIDY "${_clang_path_and_options}"
        )
    endif ()

    if (IWYU_EXE)
        set(_iwyu_path_and_options
            ${IWYU_EXE}
            # Suggests the more concise syntax introduced in C++17
            -Xiwyu --cxx17ns
            # Tells iwyu to always keep these includes
            -Xiwyu --keep=**/config.h
        )
        if (MSVC)
            list(APPEND _iwyu_path_and_options --driver-mode=cl)
        endif ()
        set_target_properties(${_target} PROPERTIES
            CXX_INCLUDE_WHAT_YOU_USE "${_iwyu_path_and_options}"
        )
    endif ()
endmacro()

function(_WEBKIT_TARGET_LINK_FRAMEWORK_INTO target_name framework _public_frameworks_var _private_frameworks_var)
    set_property(GLOBAL PROPERTY ${framework}_LINKED_INTO ${target_name})

    get_property(_framework_public_frameworks GLOBAL PROPERTY ${framework}_FRAMEWORKS)
    foreach (dependency IN LISTS ${_framework_public_frameworks})
        set(${_public_frameworks_var} "${${_public_frameworks_var}};${dependency}" PARENT_SCOPE)
    endforeach ()

    get_property(_framework_private_frameworks GLOBAL PROPERTY ${framework}_PRIVATE_FRAMEWORKS)
    foreach (dependency IN LISTS _framework_private_frameworks)
        set(${_private_frameworks_var} "${${_private_frameworks_var}};${dependency}" PARENT_SCOPE)
        _WEBKIT_TARGET_LINK_FRAMEWORK_INTO(${target_name} ${dependency} ${_public_frameworks_var} ${_private_frameworks_var})
    endforeach ()
endfunction()

macro(_WEBKIT_FRAMEWORK_LINK_FRAMEWORK _target_name)
    # Set the public libraries before modifying them when determining visibility.
    set_property(GLOBAL PROPERTY ${_target_name}_PUBLIC_LIBRARIES ${${_target_name}_LIBRARIES})

    set(_public_frameworks)
    set(_private_frameworks)

    foreach (framework IN LISTS ${_target_name}_FRAMEWORKS)
        get_property(_linked_into GLOBAL PROPERTY ${framework}_LINKED_INTO)
        if (_linked_into)
            list(APPEND _public_frameworks ${_linked_into})
        elseif (${framework}_LIBRARY_TYPE STREQUAL "SHARED")
            list(APPEND _public_frameworks ${framework})
        else ()
            list(APPEND _private_frameworks ${framework})
        endif ()
    endforeach ()

    # Recurse into the dependent frameworks
    if (_private_frameworks)
        list(REMOVE_DUPLICATES _private_frameworks)
    endif ()
    if (${_target_name}_LIBRARY_TYPE STREQUAL "SHARED")
        set_property(GLOBAL PROPERTY ${_target_name}_LINKED_INTO ${_target_name})
        foreach (framework IN LISTS _private_frameworks)
            _WEBKIT_TARGET_LINK_FRAMEWORK_INTO(${_target_name} ${framework} _public_frameworks _private_frameworks)
        endforeach ()
    endif ()

    # Add to the ${target_name}_LIBRARIES
    if (_public_frameworks)
        list(REMOVE_DUPLICATES _public_frameworks)
    endif ()
    foreach (framework IN LISTS _public_frameworks)
        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=231774
        if (APPLE)
            list(APPEND ${_target_name}_PRIVATE_LIBRARIES WebKit::${framework})
        else ()
            list(APPEND ${_target_name}_LIBRARIES WebKit::${framework})
        endif ()
    endforeach ()

    # Add to the ${target_name}_PRIVATE_LIBRARIES
    if (_private_frameworks)
        list(REMOVE_DUPLICATES _private_frameworks)
    endif ()
    foreach (framework IN LISTS _private_frameworks)
        if (${_target_name}_LIBRARY_TYPE STREQUAL "SHARED")
            get_property(_linked_libraries GLOBAL PROPERTY ${framework}_PUBLIC_LIBRARIES)
            list(APPEND ${_target_name}_INTERFACE_LIBRARIES
                ${_linked_libraries}
            )
            list(APPEND ${_target_name}_INTERFACE_INCLUDE_DIRECTORIES
                ${${framework}_FRAMEWORK_HEADERS_DIR}
                ${${framework}_PRIVATE_FRAMEWORK_HEADERS_DIR}
            )
            list(APPEND ${_target_name}_PRIVATE_LIBRARIES WebKit::${framework})
            if (${framework}_LIBRARY_TYPE STREQUAL "OBJECT")
                list(APPEND ${_target_name}_PRIVATE_LIBRARIES $<TARGET_OBJECTS:${framework}>)
                if (TARGET ${framework}_c)
                    list(APPEND ${_target_name}_PRIVATE_LIBRARIES $<TARGET_OBJECTS:${framework}_c>)
                endif ()
            endif ()
        else ()
            list(APPEND ${_target_name}_LIBRARIES WebKit::${framework})
        endif ()
    endforeach ()

    set_property(GLOBAL PROPERTY ${_target_name}_FRAMEWORKS ${_public_frameworks})
    set_property(GLOBAL PROPERTY ${_target_name}_PRIVATE_FRAMEWORKS ${_private_frameworks})
endmacro()

macro(_WEBKIT_TARGET_LINK_FRAMEWORK _target)
    foreach (framework IN LISTS ${_target}_FRAMEWORKS)
        get_property(_linked_into GLOBAL PROPERTY ${framework}_LINKED_INTO)

        # See if the target is linking a framework that the specified framework is already linked into
        if ((NOT _linked_into) OR (${framework} STREQUAL ${_linked_into}) OR (NOT ${_linked_into} IN_LIST ${_target}_FRAMEWORKS))
            list(APPEND ${_target}_PRIVATE_LIBRARIES WebKit::${framework})

            # The WebKit:: alias targets do not propagate OBJECT libraries so the
            # underyling library's objects are explicitly added to link properly
            if (TARGET ${framework} AND ${framework}_LIBRARY_TYPE STREQUAL "OBJECT")
                list(APPEND ${_target}_PRIVATE_LIBRARIES $<TARGET_OBJECTS:${framework}>)
                if (TARGET ${framework}_c)
                    list(APPEND ${_target}_PRIVATE_LIBRARIES $<TARGET_OBJECTS:${framework}_c>)
                endif ()
            endif ()
        endif ()
    endforeach ()
endmacro()

macro(_WEBKIT_LIBRARY_LINK_FRAMEWORK _target)
    # See if the library is SHARED and if so just link frameworks the same as executables
    if (${_target}_LIBRARY_TYPE STREQUAL SHARED)
        _WEBKIT_TARGET_LINK_FRAMEWORK(${_target})
    else ()
        # Include the framework headers but don't try and link the frameworks
        foreach (framework IN LISTS ${_target}_FRAMEWORKS)
            list(APPEND ${_target}_INCLUDE_DIRECTORIES
                ${${framework}_FRAMEWORK_HEADERS_DIR}
                ${${framework}_PRIVATE_FRAMEWORK_HEADERS_DIR}
            )
        endforeach ()
    endif ()
endmacro()

macro(_WEBKIT_TARGET_INTERFACE _target)
    add_library(${_target}_PostBuild INTERFACE)

    set(_filtered_interface_libs)
    foreach(_lib IN LISTS ${_target}_INTERFACE_LIBRARIES)
        if (_lib MATCHES "\\$<TARGET_OBJECTS:(.+)>")
            set(_target_name ${CMAKE_MATCH_1})
            if (TARGET ${_target_name})
                list(APPEND _filtered_interface_libs ${_lib})
            else()
                message(STATUS "Skipping non-existent target: ${_target_name}")
            endif()
        elseif (_lib MATCHES "^[A-Za-z0-9_]+::[A-Za-z0-9_]+$" OR TARGET ${_lib})
            if (TARGET ${_lib})
                list(APPEND _filtered_interface_libs ${_lib})
            else()
                message(STATUS "Skipping non-existent library: ${_lib}")
            endif()
        else()
            list(APPEND _filtered_interface_libs ${_lib})
        endif()
    endforeach()

    if (_filtered_interface_libs)
        target_link_libraries(${_target}_PostBuild INTERFACE ${_filtered_interface_libs})
    endif()

    target_include_directories(${_target}_PostBuild INTERFACE ${${_target}_INTERFACE_INCLUDE_DIRECTORIES})
    if (${_target}_INTERFACE_DEPENDENCIES)
        add_dependencies(${_target}_PostBuild ${${_target}_INTERFACE_DEPENDENCIES})
    endif ()
    if (NOT ${_target}_LIBRARY_TYPE STREQUAL "SHARED")
        target_compile_definitions(${_target}_PostBuild INTERFACE "STATICALLY_LINKED_WITH_${_target}")
    endif ()
    add_library(WebKit::${_target} ALIAS ${_target}_PostBuild)
endmacro()

macro(WEBKIT_FRAMEWORK _target)
    _WEBKIT_FRAMEWORK_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()

    if (${_target}_PRE_BUILD_COMMAND)
        add_custom_target(_${_target}_PreBuild COMMAND ${${_target}_PRE_BUILD_COMMAND} VERBATIM)
        add_dependencies(${_target} _${_target}_PreBuild)
    endif ()

    if (${_target}_POST_BUILD_COMMAND)
        add_custom_command(TARGET ${_target} POST_BUILD COMMAND ${${_target}_POST_BUILD_COMMAND} VERBATIM)
    endif ()

    if (APPLE AND NOT PORT STREQUAL "GTK" AND NOT ${${_target}_LIBRARY_TYPE} MATCHES STATIC)
        set_target_properties(${_target} PROPERTIES FRAMEWORK TRUE)
        install(TARGETS ${_target} FRAMEWORK DESTINATION ${LIB_INSTALL_DIR})
    endif ()

    _WEBKIT_TARGET_INTERFACE(${_target})
endmacro()

macro(WEBKIT_LIBRARY _target)
    _WEBKIT_LIBRARY_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()

    _WEBKIT_TARGET_INTERFACE(${_target})
endmacro()

macro(WEBKIT_EXECUTABLE _target)
    _WEBKIT_TARGET_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()
endmacro()

function(WEBKIT_COPY_FILES target_name)
    set(options FLATTENED)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)
    foreach (file IN LISTS files)
        if (IS_ABSOLUTE ${file})
            set(src_file ${file})
        else ()
            set(src_file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        endif ()
        if (opt_FLATTENED)
            get_filename_component(filename ${file} NAME)
            set(dst_file ${opt_DESTINATION}/${filename})
        else ()
            get_filename_component(file_dir ${file} DIRECTORY)
            file(MAKE_DIRECTORY ${opt_DESTINATION}/${file_dir})
            set(dst_file ${opt_DESTINATION}/${file})
        endif ()
        add_custom_command(OUTPUT ${dst_file}
            COMMAND ${CMAKE_COMMAND} -E copy ${src_file} ${dst_file}
            MAIN_DEPENDENCY ${file}
            VERBATIM
        )
        list(APPEND dst_files ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_files})
endfunction()

function(WEBKIT_SYMLINK_FILES target_name)
    set(options FLATTENED)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)
    file(MAKE_DIRECTORY ${opt_DESTINATION})
    foreach (file IN LISTS files)
        if (IS_ABSOLUTE ${file})
            set(src_file ${file})
        else ()
            set(src_file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        endif ()
        if (opt_FLATTENED)
            get_filename_component(filename ${file} NAME)
            set(dst_file ${opt_DESTINATION}/${filename})
        else ()
            get_filename_component(file_dir ${file} DIRECTORY)
            file(MAKE_DIRECTORY ${opt_DESTINATION}/${file_dir})
            set(dst_file ${opt_DESTINATION}/${file})
        endif ()
        add_custom_command(OUTPUT ${dst_file}
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${src_file} ${dst_file}
            MAIN_DEPENDENCY ${file}
            VERBATIM
        )
        list(APPEND dst_files ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_files})
endfunction()

# Helper macros for debugging CMake problems.
macro(WEBKIT_DEBUG_DUMP_COMMANDS)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endmacro()

macro(WEBKIT_DEBUG_DUMP_VARIABLES)
    set_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
       message(STATUS "${_variableName}=${${_variableName}}")
    endforeach ()
endmacro()

# Append the given flag to the target property.
# Builds on top of get_target_property() and set_target_properties()
macro(WEBKIT_ADD_TARGET_PROPERTIES _target _property _flags)
    get_target_property(_tmp ${_target} ${_property})
    if (NOT _tmp)
        set(_tmp "")
    endif (NOT _tmp)

    foreach (f ${_flags})
        set(_tmp "${_tmp} ${f}")
    endforeach (f ${_flags})

    set_target_properties(${_target} PROPERTIES ${_property} ${_tmp})
    unset(_tmp)
endmacro()

macro(WEBKIT_POPULATE_LIBRARY_VERSION library_name)
    if (NOT DEFINED ${library_name}_VERSION_MAJOR)
        set(${library_name}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION_MINOR)
        set(${library_name}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION_MICRO)
        set(${library_name}_VERSION_MICRO ${PROJECT_VERSION_MICRO})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION)
        set(${library_name}_VERSION ${PROJECT_VERSION})
    endif ()
endmacro()

macro(WEBKIT_CREATE_SYMLINK target src dest)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ln -sf ${src} ${dest}
        DEPENDS ${dest}
        COMMENT "Create symlink from ${src} to ${dest}")
endmacro()

macro(WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER _target _module_name _interop_module_path _output_header)
    if (SWIFT_REQUIRED)
        set_target_properties(${_target} PROPERTIES Swift_MODULE_NAME ${_module_name})
        # Ask swiftc where to find the header files which support C/C++ builds
        # Right now this macro is used only once; if it's used more often then
        # we should abstract this so it's executed only once.
        execute_process(
            COMMAND ${CMAKE_Swift_COMPILER} -print-target-info
            OUTPUT_VARIABLE _swift_target_info
        )
        string(JSON _swift_target_paths GET ${_swift_target_info} "paths")
        string(JSON _swift_runtime_resource_path GET ${_swift_target_paths} "runtimeResourcePath")
        target_include_directories(${_target} SYSTEM AFTER PRIVATE "${_swift_runtime_resource_path}")

        # Assemble arguments which need to be passed to swiftc.
        # Add WebKit's various feature flags as -D directives to the Swift compiler.
        GET_WEBKIT_CONFIG_VARIABLES(_swift_definitions)
        list(TRANSFORM _swift_definitions PREPEND "-D")
        set(_swift_options ${_swift_definitions})
        # Other options needed by Swift for C++ interop, including the location
        # of the modulemap and hader for WebKit's internal "APIs" which we
        # make available from C++ to Swift.
        list(APPEND _swift_options "-cxx-interoperability-mode=default" "-Xcc" "-std=c++2b" "-I${_interop_module_path}")
        # We'll use these options both for mainstream cmake invocations of swiftc (here)
        # and for our own invocation to output an interoperability .h file (later)
        list(TRANSFORM _swift_options PREPEND "$<$<COMPILE_LANGUAGE:Swift>:" OUTPUT_VARIABLE _swift_only_options)
        list(TRANSFORM _swift_only_options APPEND ">")
        target_compile_options(${_target} PRIVATE ${_swift_only_options})

        # cmake's Swift interop does not respect CMAKE_SHARED_LINKER_FLAGS, so let's pass
        # on those that we can.
        # rdar://155519819
        string(REPLACE " " ";" CMAKE_SHARED_LINKER_FLAGS_SPLIT "${CMAKE_SHARED_LINKER_FLAGS}")
        foreach (_flag IN ITEMS ${CMAKE_SHARED_LINKER_FLAGS_SPLIT})
            # We can only pass on -Wl flags.
            string(SUBSTRING ${_flag} 0 4 _prefix)
            if (${_prefix} STREQUAL "-Wl,")
                string(SUBSTRING ${_flag} 4 -1 _shorter_flag)
                # The following unfortunately deduplicates the -Xlinker
                # target_compile_options(${_target} PUBLIC "$<$<COMPILE_LANGUAGE:Swift>:-Xlinker>")
                target_compile_options(${_target} PUBLIC "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xlinker ${_shorter_flag}>")
            endif ()
        endforeach ()

        # Generate the header required for C++ to call into Swift.
        set(_swift_sources $<TARGET_PROPERTY:${_target},SOURCES>)
        set(_swift_sources $<FILTER:${_swift_sources},INCLUDE,\\.swift$>)

        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR include OUTPUT_VARIABLE _header_base_path)
        cmake_path(APPEND _header_base_path ${_output_header} OUTPUT_VARIABLE _header_path)
        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR "${_target}.emit-module.d" OUTPUT_VARIABLE _depfile_path)

        add_custom_command(
            OUTPUT ${_header_path}
            DEPENDS ${_swift_sources}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND
                ${CMAKE_Swift_COMPILER} -typecheck
                ${_swift_options}
                $<LIST:TRANSFORM,$<TARGET_PROPERTY:${_target},INCLUDE_DIRECTORIES>,PREPEND,-I>
                ${_swift_sources}
                -module-name WebKit
                -emit-clang-header-path ${_header_path}
                -emit-dependencies
            DEPFILE ${_depfile_path}
            COMMENT
                "Generating ${_target} C++ bindings to Swift at '${_header_path}'"
            COMMAND_EXPAND_LISTS)

        target_include_directories(${_target} PUBLIC ${_header_base_path})
        target_sources(${_target} PRIVATE ${_header_path})
    endif ()
endmacro()