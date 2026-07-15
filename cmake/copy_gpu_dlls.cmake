# CMake 脚本: 将 GPU DLL 从 SRC 复制到 DST（文件不存在则静默跳过）
# 用法: cmake -DSRC=... -DDST=... -P copy_gpu_dlls.cmake

set(GPU_DLLS
    cudart64_110.dll  cudart64_118.dll
    cublas64_11.dll   cublas64_12.dll
    cublasLt64_11.dll cublasLt64_12.dll
    cudnn64_8.dll
    cudnn_adv_infer64_8.dll
    cudnn_cnn_infer64_8.dll
    cudnn_ops_infer64_8.dll
    cusparse64_11.dll cusparse64_12.dll
    cusolver64_11.dll cusolver64_12.dll
)

foreach(dll ${GPU_DLLS})
    if(EXISTS "${SRC}/${dll}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${SRC}/${dll}" "${DST}")
        message(STATUS "GPU DLL copied: ${dll}")
    endif()
endforeach()
