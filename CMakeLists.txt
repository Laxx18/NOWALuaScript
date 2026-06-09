cmake_minimum_required(VERSION 3.16)

project(NOWALuaScript VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prompt the user to specify TARGET_PATH
set(TARGET_PATH "" CACHE PATH "Specify the target folder path for the project")

if(NOT TARGET_PATH)
    message(FATAL_ERROR "TARGET_PATH is not set. Please set it using -DTARGET_PATH=/path/to/your/folder")
endif()

message(STATUS "TARGET_PATH is set to: ${TARGET_PATH}")
add_definitions(-DTARGET_PATH="${TARGET_PATH}")

find_package(Qt6 REQUIRED COMPONENTS Quick Concurrent)

# ---------------------------------------------------------------------------
# Auto-detect Qt bin directory from the Qt6 CMake package location.
# Qt6_DIR is typically:  C:/Qt/6.8.x/<kit>/lib/cmake/Qt6
# We walk up three levels to reach the kit root, then append /bin.
# ---------------------------------------------------------------------------
get_filename_component(QT_KIT_ROOT "${Qt6_DIR}" DIRECTORY)   # .../lib/cmake
get_filename_component(QT_KIT_ROOT "${QT_KIT_ROOT}" DIRECTORY) # .../lib
get_filename_component(QT_KIT_ROOT "${QT_KIT_ROOT}" DIRECTORY) # .../llvm-mingw_64 (kit root)
set(QT_BIN_DIR_DETECTED "${QT_KIT_ROOT}/bin")

set(QT_BIN_DIR "${QT_BIN_DIR_DETECTED}" CACHE STRING "Path to the Qt bin directory (auto-detected)")
message(STATUS "QT_BIN_DIR: ${QT_BIN_DIR}")

# ---------------------------------------------------------------------------
# Auto-detect the LLVM-MinGW toolchain bin directory.
# The toolchain lives in <Qt install root>/Tools/llvm-mingw*/bin.
# We go two more levels up from the kit root to reach the Qt install root
# (e.g. C:/Qt), then glob for the llvm-mingw folder inside Tools/.
# ---------------------------------------------------------------------------
get_filename_component(QT_VERSION_ROOT "${QT_KIT_ROOT}" DIRECTORY)   # C:/Qt/6.8.x
get_filename_component(QT_INSTALL_ROOT "${QT_VERSION_ROOT}" DIRECTORY) # C:/Qt

file(GLOB LLVM_DIRS "${QT_INSTALL_ROOT}/Tools/llvm-mingw*")

if(LLVM_DIRS)
    list(GET LLVM_DIRS 0 LLVM_FIRST_DIR)
    set(LLVM_BIN_DIR_DETECTED "${LLVM_FIRST_DIR}/bin")
else()
    set(LLVM_BIN_DIR_DETECTED "")
    message(WARNING "Could not auto-detect LLVM-MinGW toolchain under ${QT_INSTALL_ROOT}/Tools/. "
                    "Set LLVM_BIN_DIR manually in CMake cache.")
endif()

set(LLVM_BIN_DIR "${LLVM_BIN_DIR_DETECTED}" CACHE STRING "Path to LLVM-MinGW toolchain bin dir (auto-detected)")
message(STATUS "LLVM_BIN_DIR: ${LLVM_BIN_DIR}")

# ---------------------------------------------------------------------------

set(LUA_LIBRARIES "${CMAKE_SOURCE_DIR}/lua-5_2/lib/lua.lib")
set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lua-5_2/include")

qt_standard_project_setup()

qt_add_executable(NOWALuaScript
    main.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/qml)

qt_add_resources(NOWALuaScript "qml"
    PREFIX
        "/"
    FILES
        qml_files/DetailsArea.qml
        qml_files/LuaEditor.qml
        qml_files/Main.qml
        qml_files/MainMenu.qml
        qml_files/SearchDialog.qml
        qml_files/AboutDialog.qml
        qml_files/IntelliSenseContextMenu.qml
        qml_files/MatchedFunctionContextMenu.qml
)

qt_add_qml_module(NOWALuaScript
    URI NOWALuaScript
    VERSION 1.0
    SOURCES
        luascriptcontroller.h luascriptcontroller.cpp
        luascriptqmladapter.h luascriptqmladapter.cpp
        appcommunicator.h appcommunicator.cpp
        backend/luascript.h backend/luascript.cpp
        model/luaeditormodelitem.h model/luaeditormodelitem.cpp
        model/luaeditormodel.h model/luaeditormodel.cpp
        model/apimodel.h model/apimodel.cpp
        model/matchclassworker.h model/matchclassworker.cpp
        qml/luaeditorqml.h qml/luaeditorqml.cpp
        qml/luahighlighter.h qml/luahighlighter.cpp
        luascriptadapter.h luascriptadapter.cpp
        RESOURCES resources.qrc
)

set_target_properties(NOWALuaScript PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include_directories(${LUA_INCLUDE_DIR})

target_link_libraries(NOWALuaScript
    PRIVATE Qt6::Quick Qt6::Concurrent
    ${LUA_LIBRARIES}
)

include(GNUInstallDirs)
install(TARGETS NOWALuaScript
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

target_compile_definitions(NOWALuaScript PRIVATE
    SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
    $<$<NOT:$<CONFIG:Release>>:QT_DEBUG_OUTPUT>
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")

    if(WIN32)
        # Collect all DLLs from the correct Qt kit bin folder
        file(GLOB DLL_FILES "${QT_BIN_DIR}/*.dll")

        set(COPY_COMMANDS)
        foreach(DLL_FILE ${DLL_FILES})
            list(APPEND COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_FILE}" "${CMAKE_BINARY_DIR}/bin"
                COMMAND ${CMAKE_COMMAND} -E echo "----->Copy DLL_FILE ${DLL_FILE} to ${CMAKE_BINARY_DIR}/bin"
            )
        endforeach()

        add_custom_command(TARGET NOWALuaScript POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying DLL files for Release build..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E echo "--->Create directory ${CMAKE_BINARY_DIR}/bin"
            ${COPY_COMMANDS}

            # Lua third-party DLL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/lua-5_2/bin/Lua.dll" "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E echo "----->Copy Lua.dll to ${CMAKE_BINARY_DIR}/bin"

            # LLVM runtime DLLs — required by Qt 6.8 LLVM-MinGW toolchain
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LLVM_BIN_DIR}/libc++.dll" "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E echo "----->Copy libc++.dll from ${LLVM_BIN_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LLVM_BIN_DIR}/libunwind.dll" "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E echo "----->Copy libunwind.dll from ${LLVM_BIN_DIR}"

            # Copy the built exe to bin
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:NOWALuaScript> "${CMAKE_BINARY_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E echo "----->Copy TARGET_FILE $<TARGET_FILE:NOWALuaScript> to ${CMAKE_BINARY_DIR}/bin"
            COMMENT "Copying necessary Qt libraries and executable to the output directory"

            # windeployqt picks up remaining Qt plugins and QML imports
            COMMAND ${QT_BIN_DIR}/windeployqt --dir ${CMAKE_BINARY_DIR}/bin NOWALuaScript.exe --qmldir ${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} -E echo "----->windeployqt done"
            COMMENT "Deploying Qt DLLs and QML files using windeployqt"
        )

        # Copy everything to the final TARGET_PATH deployment folder
        set(SOURCE_DIR "${CMAKE_BINARY_DIR}/bin")
        set(DESTINATION_DIR "${TARGET_PATH}/NOWALuaScript/bin")

        add_custom_command(
            TARGET NOWALuaScript
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DESTINATION_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_DIR} ${DESTINATION_DIR}
            COMMENT "Copying all files to ${DESTINATION_DIR}"
        )

    elseif(APPLE)
        add_custom_command(TARGET NOWALuaScript POST_BUILD
            COMMAND ${QT_BIN_DIR}/macdeployqt $<TARGET_BUNDLE_DIR:NOWALuaScript> -qmldir=${CMAKE_SOURCE_DIR}/qml
            COMMENT "Deploying Qt frameworks using macdeployqt"
        )
    endif()

endif()

# Platform-specific app icon
if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/NOWALuaScript.rc")
    target_sources(NOWALuaScript PRIVATE ${APP_ICON_RESOURCE})
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "NOWALuaScript.icns")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/NOWALuaScript.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(NOWALuaScript PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/NOWALuaScript.icns")
else()
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/NOWALuaScript.png" DESTINATION share/icons/hicolor/48x48/apps)
endif()
