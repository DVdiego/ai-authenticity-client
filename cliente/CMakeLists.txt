cmake_minimum_required(VERSION 3.16)

project(cliente VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/assets/appcliente.icns")

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Network)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appcliente
    main.cpp
    src/app/AppController.cpp
    src/engines/EngineFactory.cpp
    src/engines/OnDeviceEngine.cpp
    src/engines/OnDeviceNativeEngine.cpp
    src/engines/ApiEngine.cpp
    ../../../ai-authenticity-mobile/native/core/cpp/src/runtime_api.cpp
    ${APP_ICON_MACOS}
)

set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

target_include_directories(appcliente PRIVATE
    src
    /opt/homebrew/opt/onnxruntime/include
    ../../../ai-authenticity-mobile/native/core/cpp/include
)

qt_add_resources(appcliente "appcliente_assets"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        assets/appicon-256.png
)

qt_add_qml_module(appcliente
    URI cliente
    QML_FILES
        Main.qml
)

set_target_properties(appcliente PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appcliente
    MACOSX_BUNDLE_ICON_FILE appcliente.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appcliente
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Network
        ${OpenCV_LIBS}
        /opt/homebrew/opt/onnxruntime/lib/libonnxruntime.dylib
)

include(GNUInstallDirs)
install(TARGETS appcliente
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
