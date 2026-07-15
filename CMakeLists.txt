cmake_minimum_required(VERSION 3.16)
project(TurboOCR VERSION 0.3.0 LANGUAGES CXX)

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

# ---- MOC 自动处理 (替代手动的 moc_prebuild.bat) ----
set(CMAKE_AUTOMOC ON)

# ---- /MT: 静态 CRT (3rdparty 预编译库统一使用静态 CRT) ----
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- 默认 Release, 可被命令行 -DCMAKE_BUILD_TYPE=Debug 覆盖 ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- 默认 Qt6 路径 (可被命令行 -DCMAKE_PREFIX_PATH=... 覆盖) ----
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "F:/Develop/Qt6/6.8.3/msvc2022_64" CACHE PATH "Qt6 install prefix" FORCE)
endif()

# ============================================================
# Find Qt6
# ============================================================
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui TextToSpeech Sql Pdf HttpServer)

# 从 Qt6_DIR 反推 Qt6 安装根目录
# Qt6_DIR = <prefix>/lib/cmake/Qt6  →  回退 3 级得到 prefix
get_filename_component(QT6_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
message(STATUS "Qt6 root: ${QT6_ROOT}")

# ============================================================
# 路径 (项目相对路径, 不依赖绝对路径)
# ============================================================
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(TP   "${ROOT}/3rdparty")
set(OCR  "${ROOT}/ocr_engine")

# ============================================================
# OCR 引擎源码 (ocr_engine/src/)
# 逐文件显式列出, 确保 CMake 正确管理重名 .cc 文件
# ============================================================
set(OCR_SRC
    # -- api --
    ${OCR}/src/api/pipelines/ocr.cc
    # -- base --
    ${OCR}/src/base/base_batch_sampler.cc
    ${OCR}/src/base/base_pipeline.cc
    ${OCR}/src/base/base_predictor.cc
    # -- common --
    ${OCR}/src/common/image_batch_sampler.cc
    ${OCR}/src/common/processors.cc
    ${OCR}/src/common/static_infer.cc
    ${OCR}/src/common/thread_pool.cc
    # -- modules: image_classification --
    ${OCR}/src/modules/image_classification/predictor.cc
    ${OCR}/src/modules/image_classification/processors.cc
    ${OCR}/src/modules/image_classification/result.cc
    # -- modules: image_unwarping --
    ${OCR}/src/modules/image_unwarping/predictor.cc
    ${OCR}/src/modules/image_unwarping/processors.cc
    ${OCR}/src/modules/image_unwarping/result.cc
    # -- modules: text_detection --
    ${OCR}/src/modules/text_detection/predictor.cc
    ${OCR}/src/modules/text_detection/processors.cc
    ${OCR}/src/modules/text_detection/result.cc
    # -- modules: text_recognition --
    ${OCR}/src/modules/text_recognition/predictor.cc
    ${OCR}/src/modules/text_recognition/processors.cc
    ${OCR}/src/modules/text_recognition/result.cc
    # -- pipelines --
    ${OCR}/src/pipelines/doc_preprocessor/pipeline.cc
    ${OCR}/src/pipelines/doc_preprocessor/result.cc
    ${OCR}/src/pipelines/ocr/pipeline.cc
    ${OCR}/src/pipelines/ocr/result.cc
    # -- utils --
    ${OCR}/src/utils/args.cc
    ${OCR}/src/utils/ilogger.cc
    ${OCR}/src/utils/ini_config.cc
    ${OCR}/src/utils/mkldnn_blocklist.cc
    ${OCR}/src/utils/pp_option.cc
    ${OCR}/src/utils/utility.cc
    ${OCR}/src/utils/yaml_config.cc
)

# ============================================================
# TurboOCR 源码 (src/)
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

# Windows: 嵌入 exe 文件图标 (资源管理器显示)
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE "${ROOT}/resources/app.rc")
endif()

# ---- Windows 应用 (无控制台) + MultiByte 字符集 ----
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_CharacterSet "MultiByte"
    WIN32_EXECUTABLE TRUE
)
# 入口点: 使用 main() 而非 WinMain()
target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup)
# 非 VS 生成器默认无 UNICODE, 但 C 运行时可能隐含; 显式用 _MBCS 覆盖
target_compile_definitions(${PROJECT_NAME} PRIVATE _MBCS)

# ============================================================
# Include 目录 (target-scoped, 不污染全局)
# ============================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${ROOT}
    ${OCR}
    ${TP}/paddle_inference/include
    ${TP}/protobuf/include
    ${TP}/glog/include
    ${TP}/gflags/include
    ${TP}/xxhash/include
    ${TP}/yaml-cpp/include
    ${TP}/zlib/include
    ${TP}/mklml/include
    ${TP}/onednn/include
    ${TP}/nlohmann
    ${TP}/abseil/include
    ${TP}/clipper/include
    ${TP}/opencv/include
    ${TP}
)

# ============================================================
# ZXing 二维码/条形码 (预编译静态库, v142 工具集)
# 重新编译: cmake -B build_zx -T v142 ../3rdparty/zxing-cpp && cmake --build build_zx
# ============================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${TP}/zxing/include)
target_link_directories(${PROJECT_NAME} PRIVATE ${TP}/zxing/lib)

# ============================================================
# Link 目录
# ============================================================
target_link_directories(${PROJECT_NAME} PRIVATE
    ${TP}/paddle_inference/lib
    ${TP}/protobuf/lib
    ${TP}/glog/lib
    ${TP}/gflags/lib
    ${TP}/xxhash/lib
    ${TP}/yaml-cpp/lib
    ${TP}/zlib/lib
    ${TP}/mklml/lib
    ${TP}/onednn/lib
    ${TP}/abseil/lib
    ${TP}/clipper/lib
    ${TP}/opencv/lib
)

# ============================================================
# 编译选项 — 仅 MSVC 生效, 其他编译器跳过
# ============================================================
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/UUNICODE /U_UNICODE>
)

# ============================================================
# 预处理器定义
# ============================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    YAML_CPP_STATIC_DEFINE
    GOOGLE_GLOG_DLL_DECL=
    $<$<CONFIG:Release>:NDEBUG>
)

# ============================================================
# Link 库
# ============================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Qt6
    Qt6::Widgets Qt6::Core Qt6::Gui Qt6::TextToSpeech Qt6::Sql Qt6::Pdf Qt6::HttpServer
    # ZXing
    ${TP}/zxing/lib/ZXing.lib
    # 3rdparty — 使用绝对路径确保精确链接
    ${TP}/paddle_inference/lib/paddle_inference.lib
    ${TP}/protobuf/lib/libprotobuf.lib
    ${TP}/glog/lib/glog.lib
    ${TP}/gflags/lib/gflags_static.lib
    ${TP}/xxhash/lib/xxhash.lib
    ${TP}/yaml-cpp/lib/yaml-cpp.lib
    ${TP}/zlib/lib/zlib.lib
    ${TP}/mklml/lib/mklml.lib
    ${TP}/mklml/lib/libiomp5md.lib
    ${TP}/onednn/lib/mkldnn.lib
    ${TP}/abseil/lib/abseil_dll.lib
    ${TP}/clipper/lib/polyclipping.lib
    ${TP}/opencv/lib/opencv_world3410.lib
    # Windows 系统库
    shlwapi
    version
)

# ============================================================
# 后置构建: 复制 DLL + 插件 + 翻译 + 模型
# 使用 generator expressions 适配任意构建目录
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
    # ---- Paddle + 3rdparty DLLs ----
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/paddle_inference/lib/paddle_inference.dll" "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/paddle_inference/lib/common.dll"          "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/paddle_inference/lib/phi.dll"             "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/mklml/lib/mklml.dll"                      "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/mklml/lib/libiomp5md.dll"                 "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/paddle_inference/lib/mkldnn.dll"            "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/abseil/lib/abseil_dll.dll"                "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/clipper/lib/polyclipping.dll"              "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/opencv/lib/opencv_world3410.dll"           "${OUTDIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP}/opencv/lib/opencv_ffmpeg3410_64.dll"       "${OUTDIR}"
    # ---- Paddle GPU 依赖 (CUDA / cuDNN) — 有则打包，无则跳过 ----
    COMMAND powershell -NoProfile -Command "Get-ChildItem '${TP}/paddle_inference/lib/cud*','${TP}/paddle_inference/lib/cublas*' -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName '${OUTDIR}' -Verbose }"
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
    # ---- Qt 翻译 (全部语言, 除日语) ----
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
    # ---- MSVC 运行时 DLL (VS 2026 / v145 工具集: CRT + OpenMP) ----
    COMMAND powershell -NoProfile -Command "$vsBase = Resolve-Path 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/*/x64' -ErrorAction SilentlyContinue | Select-Object -First 1; if ($vsBase) { Copy-Item (Join-Path $vsBase 'Microsoft.VC145.CRT/*.dll') '${OUTDIR}' -Verbose; Copy-Item (Join-Path $vsBase 'Microsoft.VC145.OpenMP/*.dll') '${OUTDIR}' -Verbose } else { Write-Warning 'MSVC redist DLLs not found, please install VS 2026' }"
)

# ============================================================
# 打印配置摘要
# ============================================================
message(STATUS "========================================")
message(STATUS "TurboOCR v${PROJECT_VERSION}")
message(STATUS "Qt6:      ${QT6_ROOT}")
message(STATUS "Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Output:   ${OUTDIR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")
