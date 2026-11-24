include(ExternalProject)

FetchContent_Declare(libjpeg_turbo
    URL https://codeload.github.com/libjpeg-turbo/libjpeg-turbo/tar.gz/refs/tags/3.2.1
    URL_HASH SHA256=6f5819c51649bce2571a23b1b0cd41eae2f57fc3f8908c7ec9e7047efb65ca13
    SOURCE_SUBDIR mizuki-external)
FetchContent_Declare(libwebp
    URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
    URL_HASH SHA256=e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
    EXCLUDE_FROM_ALL)
FetchContent_Declare(picosha2
    URL https://codeload.github.com/okdshin/PicoSHA2/tar.gz/refs/tags/v1.0.1
    URL_HASH SHA256=9983136544234e573fe07cc1a22fdf978ad7979043e7be2e9082f6d4991ff8a8
    SOURCE_SUBDIR mizuki-header-only)

function(mizuki_add_image_dependencies)
    set(BUILD_SHARED_LIBS OFF)
    foreach(tool ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO WEBPMUX EXTRAS)
        set(WEBP_BUILD_${tool} OFF)
    endforeach()
    set(WEBP_BUILD_LIBWEBPMUX ON)
    FetchContent_MakeAvailable(libwebp libjpeg_turbo picosha2)

    # Upstream requires libjpeg-turbo to be configured as a standalone project.
    set(jpeg_binary_dir "${CMAKE_BINARY_DIR}/jpeg-build")
    set(jpeg_library_dir "${jpeg_binary_dir}")
    if(CMAKE_CONFIGURATION_TYPES)
        string(APPEND jpeg_library_dir "/$<CONFIG>")
    endif()
    if(MSVC)
        set(jpeg_library_name turbojpeg-static)
    else()
        set(jpeg_library_name turbojpeg)
    endif()
    set(jpeg_library "${jpeg_library_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}${jpeg_library_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(jpeg_crt_dll ON)
    if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(jpeg_crt_dll OFF)
    endif()
    ExternalProject_Add(mizuki_jpeg_build
        SOURCE_DIR "${libjpeg_turbo_SOURCE_DIR}"
        BINARY_DIR "${jpeg_binary_dir}"
        CMAKE_ARGS
            "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
            "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}"
            "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
            "-DWITH_CRT_DLL:BOOL=${jpeg_crt_dll}"
            -DENABLE_SHARED:BOOL=OFF
            -DENABLE_STATIC:BOOL=ON
            -DWITH_TURBOJPEG:BOOL=ON
            -DWITH_TOOLS:BOOL=OFF
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG> --target turbojpeg-static
        BUILD_BYPRODUCTS "${jpeg_library}"
        INSTALL_COMMAND "")
    file(MAKE_DIRECTORY "${jpeg_binary_dir}")
    add_library(mizuki_jpeg STATIC IMPORTED GLOBAL)
    set_target_properties(mizuki_jpeg PROPERTIES
        IMPORTED_LOCATION "${jpeg_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${libjpeg_turbo_SOURCE_DIR}/src;${jpeg_binary_dir};${libjpeg_turbo_SOURCE_DIR}/src/spng/zlib")
    if(CMAKE_CONFIGURATION_TYPES)
        foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
            string(TOUPPER "${config}" config_upper)
            set_property(TARGET mizuki_jpeg PROPERTY IMPORTED_LOCATION_${config_upper}
                "${jpeg_binary_dir}/${config}/${CMAKE_STATIC_LIBRARY_PREFIX}${jpeg_library_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        endforeach()
    endif()
    add_dependencies(mizuki_jpeg mizuki_jpeg_build)

    add_library(mizuki_sha256 INTERFACE)
    target_include_directories(mizuki_sha256 SYSTEM INTERFACE "${picosha2_SOURCE_DIR}")
endfunction()

mizuki_add_image_dependencies()
