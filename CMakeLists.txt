cmake_minimum_required(VERSION 3.25)
project("Detect My Jiggler")

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
    main.cpp
    globals.cpp
    known_devices.cpp
    device_utils.cpp
    detection.cpp
    ui.cpp
    live_chart.cpp
    resource.rc
)
add_executable(DetectMyJiggler WIN32 ${SOURCE_FILES})

if (WIN32)
    # Link necessary Windows libraries
    # comctl32: Common Controls
    # uxtheme: UI Themes
    # hid: Human Interface Device
    # msimg32: GradientFill and other GDI functions
    target_link_libraries(DetectMyJiggler comctl32 uxtheme hid msimg32)

    if (MSVC)
        # Disable automatic manifest generation to avoid duplicate manifest error
        # We provide our own manifest in resource.rc
        target_link_options(DetectMyJiggler PRIVATE /MANIFEST:NO)
    endif()

    if (MINGW)
        # Static linking for MinGW to produce a standalone executable
        target_link_options(DetectMyJiggler PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()
