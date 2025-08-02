# MangoEditor - CMake বিল্ড কনফিগারেশন
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# প্রজেক্ট মেটাডাটা
project(MangoEditor
    VERSION 2.1.0
    DESCRIPTION "একটি আধুনিক ক্রস-প্ল্যাটফর্ম কোড এডিটর"
    LANGUAGES CXX
    HOMEPAGE_URL "https://mangosoft.org"
)

# বিল্ড টাইপ সেটআপ
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "বিল্ড টাইপ" FORCE)
endif()

# C++ স্ট্যান্ডার্ড সেটআপ
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# বিল্ড অপশন
option(BUILD_TESTING "টেস্ট বিল্ড সক্ষম করুন" ON)
option(ENABLE_PLUGINS "প্লাগইন সিস্টেম সক্ষম করুন" ON)
option(INSTALL_DEVELOPER "ডেভেলপার ফাইল ইন্সটল করুন" OFF)

# কম্পাইলার ফ্ল্যাগস
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -fPIC -fvisibility=hidden)
    if(APPLE)
        add_compile_options(-stdlib=libc++)
    endif()
endif()

# Qt ডিপেন্ডেন্সি
find_package(Qt5 5.15 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    LinguistTools
    Concurrent
)

# তৃতীয় পক্ষের লাইব্রেরি
add_subdirectory(thirdparty/scintilla)
add_subdirectory(thirdparty/cmark)

# রিসোর্স ফাইলস
qt5_add_resources(RESOURCE_FILES
    resources/icons.qrc
    resources/themes.qrc
    resources/translations.qrc
)

# মেইন টার্গেট
add_executable(MangoEditor
    src/main.cpp
    src/editor_core.cpp
    src/plugin_interface.cpp
    ${RESOURCE_FILES}
)

# প্ল্যাটফর্ম-স্পেসিফিক কনফিগারেশন
if(APPLE)
    # macOS বান্ডেল প্রোপার্টিজ
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.mangosoft.MangoEditor")
    set(MACOSX_BUNDLE_BUNDLE_NAME "MangoEditor")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}.${BUILD_NUMBER}")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2023-2024 MangoSoft")
    set(MACOSX_BUNDLE_ICON_FILE "app_icon.icns")
    set(MACOSX_DEPLOYMENT_TARGET "11.0")

    # প্লিস্ট ফাইল কনফিগার
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )

    # অতিরিক্ত রিসোর্স
    set(APP_ICON ${CMAKE_SOURCE_DIR}/resources/icons/mac/app_icon.icns)
    set(TEXTFILE_ICON ${CMAKE_SOURCE_DIR}/resources/icons/mac/textfile.icns)
    
    set_source_files_properties(${APP_ICON} ${TEXTFILE_ICON}
        PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
endif()

# টার্গেট প্রোপার্টিজ
set_target_properties(MangoEditor PROPERTIES
    OUTPUT_NAME "mangoeditor"
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    WIN32_EXECUTABLE TRUE
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# লিংকিং
target_link_libraries(MangoEditor PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Concurrent
    Scintilla
    cmark
)

if(UNIX AND NOT APPLE)
    target_link_libraries(MangoEditor PRIVATE
        pthread
        dl
    )
endif()

# ইন্সটলেশন
install(TARGETS MangoEditor
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY resources/
    DESTINATION share/MangoEditor
)

if(APPLE)
    install(FILES
        ${APP_ICON}
        ${TEXTFILE_ICON}
        DESTINATION ../Resources
    )
endif()

# ইনস্টলেশন পরবর্তী স্ক্রিপ্ট
include(GNUInstallDirs)
install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_INSTALL_PREFIX}/bin/mangoeditor
        ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/mango
    )
" COMPONENT runtime)

# প্যাকেজিং
set(CPACK_PACKAGE_NAME "MangoEditor")
set(CPACK_PACKAGE_VENDOR "MangoSoft")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "MangoEditor ${PROJECT_VERSION}")
    set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/cmake/DS_Store")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MODIFY_PATH ON)
else()
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt5-default, libc6, libqt5concurrent5")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
endif()

include(CPack)
