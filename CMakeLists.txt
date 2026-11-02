cmake_minimum_required(VERSION 3.16)
project(TurboOCR VERSION 1.1.0 LANGUAGES CXX)

# ============================================================
# MSVC 工具集兼容性
# v142 (VS2019) 编译产物可被 v142/v143/v144 链接
# 若需高版本特性，命令行覆盖: cmake -B build -T v143
# ============================================================
if(NOT CMAKE_GENERATOR_TOOLSET AND MSVC)
    set(CMAKE_GENERATOR_TOOLSET "v142" CACHE STRING "MSVC toolset (v142 for max compatibility)")
endif()

# ============================================================
# C++ Standard
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- MOC 自动处理 ----
set(CMAKE_AUTOMOC ON)

# ---- /MT: 静态 CRT (3rdparty 预编译库统一使用静态 CRT) ----
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- 默认 Release ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Qt6 路径 (可被命令行 -DCMAKE_PREFIX_PATH=... 覆盖) ----
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "F:/Develop/Qt6/6.8.3/msvc2022_64" CACHE PATH "Qt6 install prefix" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui TextToSpeech Sql Pdf HttpServer)
get_filename_component(QT6_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
message(STATUS "Qt6 root: ${QT6_ROOT}")

# ============================================================
# 路径
# ============================================================
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(TP   "${ROOT}/3rdparty")
set(OCR  "${ROOT}/ocr_engine")

# ============================================================
# OCR 引擎源码
# ============================================================
set(OCR_SRC
    ${OCR}/src/api/pipelines/ocr.cc
    ${OCR}/src/base/base_batch_sampler.cc
    ${OCR}/src/base/base_pipeline.cc
    ${OCR}/src/base/base_predictor.cc
    ${OCR}/src/common/image_batch_sampler.cc
    ${OCR}/src/common/processors.cc
    ${OCR}/src/common/static_infer.cc
    ${OCR}/src/common/thread_pool.cc
    ${OCR}/src/modules/image_classification/predictor.cc
    ${OCR}/src/modules/image_classification/processors.cc
    ${OCR}/src/modules/image_classification/result.cc
    ${OCR}/src/modules/image_unwarping/predictor.cc
    ${OCR}/src/modules/image_unwarping/processors.cc
    ${OCR}/src/modules/image_unwarping/result.cc
    ${OCR}/src/modules/text_detection/predictor.cc
    ${OCR}/src/modules/text_detection/processors.cc
    ${OCR}/src/modules/text_detection/result.cc
    ${OCR}/src/modules/text_recognition/predictor.cc
    ${OCR}/src/modules/text_recognition/processors.cc
    ${OCR}/src/modules/text_recognition/result.cc
    ${OCR}/src/pipelines/doc_preprocessor/pipeline.cc
    ${OCR}/src/pipelines/doc_preprocessor/result.cc
    ${OCR}/src/pipelines/ocr/pipeline.cc
    ${OCR}/src/pipelines/ocr/result.cc
    ${OCR}/src/utils/args.cc
    ${OCR}/src/utils/ilogger.cc
    ${OCR}/src/utils/ini_config.cc
    ${OCR}/src/utils/mkldnn_blocklist.cc
    ${OCR}/src/utils/pp_option.cc
    ${OCR}/src/utils/utility.cc
    ${OCR}/src/utils/yaml_config.cc
)

# ============================================================
# TurboOCR 源码
# ============================================================
set(APP_SRC
    src/main.cpp
    src/main_window.cpp
    src/core/config_manager.cpp
    src/core/loading_overlay.cpp
    src/core/logger.cpp
    src/core/ocr_history.cpp
    src/core/pdf_render.cpp
    src/core/screen_capture.cpp
    src/core/theme.cpp
    src/core/i18n.cpp
    src/core/qrcode_scanner.cpp
    src/core/http_server.cpp
    src/core/gpu_checker.cpp
    src/ocr/engine.cpp
    src/pages/calligraphy_ocr_page.cpp
    src/pages/batch_page.cpp
    src/pages/settings_page.cpp
    src/pages/developer_page.cpp
    src/postprocess/parsers.cpp
    src/output/formatters.cpp
)

# ============================================================
# 可执行文件
# ============================================================
add_executable(${PROJECT_NAME} ${APP_SRC} ${OCR_SRC})

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE "${ROOT}/resources/app.rc")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_CharacterSet "MultiByte"
    WIN32_EXECUTABLE TRUE
)
target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup)
target_compile_definitions(${PROJECT_NAME} PRIVATE _MBCS)

# ============================================================
# Include 目录
# ============================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${ROOT}
    ${OCR}
    ${TP}/onnxruntime/include
    ${TP}/gflags/include
    ${TP}/yaml-cpp/include
    ${TP}/zlib/include
    ${TP}/nlohmann
    ${TP}/abseil/include
    ${TP}/clipper/include
    ${TP}/opencv/include
    ${TP}/zxing/include
    ${TP}
)

# ============================================================
# Link 目录
# ============================================================
target_link_directories(${PROJECT_NAME} PRIVATE
    ${TP}/onnxruntime/lib
    ${TP}/gflags/lib
    ${TP}/yaml-cpp/lib
    ${TP}/zlib/lib
    ${TP}/abseil/lib
    ${TP}/clipper/lib
    ${TP}/opencv/lib
    ${TP}/zxing/lib
)

# ============================================================
# 编译选项
# ============================================================
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/UUNICODE /U_UNICODE>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    YAML_CPP_STATIC_DEFINE
    $<$<CONFIG:Release>:NDEBUG>
)

# ============================================================
# Link 库
# ============================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui Qt6::TextToSpeech Qt6::Sql Qt6::Pdf Qt6::HttpServer
    ${TP}/zxing/lib/ZXing.lib
    ${TP}/onnxruntime/lib/onnxruntime.lib
    ${TP}/gflags/lib/gflags_static.lib
    ${TP}/yaml-cpp/lib/yaml-cpp.lib
    ${TP}/zlib/lib/zlib.lib
    ${TP}/abseil/lib/abseil_dll.lib
    ${TP}/clipper/lib/polyclipping.lib
    ${TP}/opencv/lib/opencv_world3410.lib
    d3d12
    dxgi
    shlwapi
    version
)

# ============================================================
# VC 运行时 DLL
# Qt 6.8 / ONNX Runtime 使用 VS2022+ 动态 CRT，优先选择最新可用的
# VC Redist（VC145 > VC143 > VC142），避免 vcruntime140_threads.dll 缺失。
# ============================================================
set(VC_REDIST_CANDIDATES
    "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC"
    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC"
    "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Redist/MSVC"
    "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Redist/MSVC"
    "D:/MSoftware/Visual Studio2019/2019/Community/VC/Redist/MSVC"
)
if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND VC_REDIST_CANDIDATES "${CMAKE_GENERATOR_INSTANCE}/VC/Redist/MSVC")
endif()

set(VC_REDIST_LATEST "")
set(VC_RUNTIME_DLLS "")

# 优先使用项目内自带的 VC145 运行时，保证构建机没有 VS2022/2026 时也能产出
# Qt 6.8 所需的 vcruntime140_threads.dll 等文件。
if(EXISTS "${TP}/vc_runtime/x64")
    file(GLOB VC_RUNTIME_DLLS "${TP}/vc_runtime/x64/*.dll")
    set(VC_REDIST_LATEST "${TP}/vc_runtime/x64")
endif()

# 没有自带运行时时，回退到本机 VS Redist（优先最新版本）。
if(NOT VC_RUNTIME_DLLS)
    foreach(cand IN LISTS VC_REDIST_CANDIDATES)
        if(EXISTS "${cand}")
            file(GLOB _vc_versions "${cand}/*")
            if(_vc_versions)
                list(SORT _vc_versions)
                list(REVERSE _vc_versions)
                foreach(ver IN LISTS _vc_versions)
                    file(GLOB _crt "${ver}/x64/Microsoft.VC*.CRT/*.dll")
                    if(_crt)
                        file(GLOB _omp "${ver}/x64/Microsoft.VC*.OpenMP/*.dll")
                        set(VC_RUNTIME_DLLS ${_crt} ${_omp})
                        set(VC_REDIST_LATEST "${ver}")
                        break()
                    endif()
                endforeach()
                if(VC_RUNTIME_DLLS)
                    break()
                endif()
            endif()
        endif()
    endforeach()
endif()

message(STATUS "VC runtime root: ${VC_REDIST_LATEST}")
message(STATUS "VC runtime DLLs: ${VC_RUNTIME_DLLS}")

# ============================================================
# 后置构建: 复制 DLL + 插件 + 翻译 + 模型
# ============================================================
set(OUTDIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # ---- Qt6 DLLs ----
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Core.dll"    "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Gui.dll"     "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Widgets.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Pdf.dll"     "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6TextToSpeech.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Multimedia.dll"   "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Network.dll"      "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Sql.dll"     "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6Svg.dll"     "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6HttpServer.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/bin/Qt6WebSockets.dll" "${OUTDIR}"
    # ---- ONNX Runtime + DirectML ----
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/onnxruntime/lib/onnxruntime.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/onnxruntime/lib/onnxruntime_providers_shared.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/directml/lib/DirectML.dll" "${OUTDIR}"
    # ---- 其他 3rdparty DLL ----
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/abseil/lib/abseil_dll.dll"      "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/clipper/lib/polyclipping.dll"   "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/opencv/lib/opencv_world3410.dll" "${OUTDIR}"
    # ---- Qt6 插件 ----
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/platforms/qwindows.dll" "${OUTDIR}/platforms/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/sqldrivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/sqldrivers/qsqlite.dll" "${OUTDIR}/sqldrivers/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/texttospeech"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/texttospeech/qtexttospeech_sapi.dll" "${OUTDIR}/texttospeech/"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/imageformats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/imageformats/qjpeg.dll" "${OUTDIR}/imageformats/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/imageformats/qgif.dll"  "${OUTDIR}/imageformats/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT6_ROOT}/plugins/imageformats/qico.dll"  "${OUTDIR}/imageformats/"
    # ---- Qt 翻译 ----
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT6_ROOT}/translations" "${OUTDIR}/translations"
    COMMAND ${CMAKE_COMMAND} -E remove "${OUTDIR}/translations/qt_ja.qm"
    COMMAND ${CMAKE_COMMAND} -E remove "${OUTDIR}/translations/qt_ja_JP.qm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT}/resources/translations/qtbase_zh_CN.qm" "${OUTDIR}/translations/"
    # ---- 图标 ----
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT}/resources/icon.png" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT}/resources/page_prev.png" "${OUTDIR}/icons/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT}/resources/page_next.png" "${OUTDIR}/icons/"
    # ---- OCR 模型 + 配置 ----
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROOT}/models" "${OUTDIR}/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${OCR}/configs" "${OUTDIR}/configs"
)

# ---- VC 运行时 ----
foreach(dll ${VC_RUNTIME_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "${OUTDIR}")
endforeach()

# ============================================================
# 打印配置摘要
# ============================================================
message(STATUS "========================================")
message(STATUS "TurboOCR v${PROJECT_VERSION} (ONNX Runtime + DirectML)")
message(STATUS "Qt6:      ${QT6_ROOT}")
message(STATUS "Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Output:   ${OUTDIR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")
