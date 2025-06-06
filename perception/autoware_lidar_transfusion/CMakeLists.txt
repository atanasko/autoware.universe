cmake_minimum_required(VERSION 3.14)
project(autoware_lidar_transfusion)

find_package(autoware_cmake REQUIRED)
autoware_package()

add_compile_options(-Wno-deprecated-declarations)

option(CUDA_VERBOSE "Verbose output of CUDA modules" OFF)

# set flags for CUDA availability
option(CUDA_AVAIL "CUDA available" OFF)
find_package(CUDA)
if(CUDA_FOUND)
  find_library(CUBLAS_LIBRARIES cublas HINTS
    ${CUDA_TOOLKIT_ROOT_DIR}/lib64
    ${CUDA_TOOLKIT_ROOT_DIR}/lib
  )
  if(CUDA_VERBOSE)
    message("CUDA is available!")
    message("CUDA Libs: ${CUDA_LIBRARIES}")
    message("CUDA Headers: ${CUDA_INCLUDE_DIRS}")
  endif()
  # Note: cublas_device was depreciated in CUDA version 9.2
  #       https://forums.developer.nvidia.com/t/where-can-i-find-libcublas-device-so-or-libcublas-device-a/67251/4
  #       In LibTorch, CUDA_cublas_device_LIBRARY is used.
  unset(CUDA_cublas_device_LIBRARY CACHE)
  set(CUDA_AVAIL ON)
else()
  message("CUDA NOT FOUND")
  set(CUDA_AVAIL OFF)
endif()

# set flags for TensorRT availability
option(TRT_AVAIL "TensorRT available" OFF)
# try to find the tensorRT modules
find_library(NVINFER nvinfer)
find_library(NVONNXPARSER nvonnxparser)
if(NVINFER AND NVONNXPARSER)
  if(CUDA_VERBOSE)
    message("TensorRT is available!")
    message("NVINFER: ${NVINFER}")
    message("NVONNXPARSER: ${NVONNXPARSER}")
  endif()
  set(TRT_AVAIL ON)
else()
  message("TensorRT is NOT Available")
  set(TRT_AVAIL OFF)
endif()

# set flags for CUDNN availability
option(CUDNN_AVAIL "CUDNN available" OFF)
# try to find the CUDNN module
find_library(CUDNN_LIBRARY
NAMES libcudnn.so${__cudnn_ver_suffix} libcudnn${__cudnn_ver_suffix}.dylib ${__cudnn_lib_win_name}
PATHS $ENV{LD_LIBRARY_PATH} ${__libpath_cudart} ${CUDNN_ROOT_DIR} ${PC_CUDNN_LIBRARY_DIRS} ${CMAKE_INSTALL_PREFIX}
PATH_SUFFIXES lib lib64 bin
DOC "CUDNN library."
)
if(CUDNN_LIBRARY)
  if(CUDA_VERBOSE)
    message(STATUS "CUDNN is available!")
    message(STATUS "CUDNN_LIBRARY: ${CUDNN_LIBRARY}")
  endif()
  set(CUDNN_AVAIL ON)
else()
  message("CUDNN is NOT Available")
  set(CUDNN_AVAIL OFF)
endif()

if(TRT_AVAIL AND CUDA_AVAIL AND CUDNN_AVAIL)
  find_package(ament_cmake_auto REQUIRED)
  ament_auto_find_build_dependencies()

  include_directories(
    include
    ${CUDA_INCLUDE_DIRS}
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CUDA_FLAGS ${CMAKE_CUDA_FLAGS} "-g -G")
    set(CUDA_NVCC_FLAGS "-g -G")
  endif()

  ament_auto_add_library(${PROJECT_NAME}_lib SHARED
    lib/detection_class_remapper.cpp
    lib/postprocess/non_maximum_suppression.cpp
    lib/preprocess/voxel_generator.cpp
    lib/preprocess/pointcloud_densification.cpp
    lib/ros_utils.cpp
    lib/transfusion_trt.cpp
  )

  # cSpell:ignore gencode
  list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_75,code=sm_75")
  list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_86,code=sm_86")
  list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_87,code=sm_87")
  list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_89,code=sm_89")
  # NOTE(knzo25): PTX support for newer GPUs until we can compile directly
  list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_89,code=compute_89")
  # TODO(knzo25): enable when the driver supports it
  # list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_120,code=sm_120")

  cuda_add_library(${PROJECT_NAME}_cuda_lib SHARED
    lib/postprocess/circle_nms_kernel.cu
    lib/postprocess/postprocess_kernel.cu
    lib/preprocess/preprocess_kernel.cu
  )

  target_link_libraries(${PROJECT_NAME}_lib
    ${NVINFER}
    ${NVONNXPARSER}
    ${CUDA_LIBRARIES}
    ${CUBLAS_LIBRARIES}
    ${CUDA_curand_LIBRARY}
    ${CUDNN_LIBRARY}
    ${PROJECT_NAME}_cuda_lib
  )

  target_include_directories(${PROJECT_NAME}_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )

  # To suppress unknown-pragmas error. The root-cause is CUB library in CUDA 11.6.
  # This issue was fixed by https://github.com/NVIDIA/cub/commit/7d608bf1dc14553e2fb219eabeed80b76621b6fe
  target_include_directories(${PROJECT_NAME}_lib
    SYSTEM PUBLIC
    ${CUDA_INCLUDE_DIRS}
  )

  ament_auto_add_library(${PROJECT_NAME}_component SHARED
    src/lidar_transfusion_node.cpp
  )

  target_link_libraries(${PROJECT_NAME}_component
    ${PROJECT_NAME}_lib
  )

  rclcpp_components_register_node(${PROJECT_NAME}_component
    PLUGIN "autoware::lidar_transfusion::LidarTransfusionNode"
    EXECUTABLE ${PROJECT_NAME}_node
  )

  install(
    TARGETS ${PROJECT_NAME}_cuda_lib
    DESTINATION lib
  )

  if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()

    find_package(ament_cmake_gtest REQUIRED)
    ament_auto_add_gtest(test_detection_class_remapper
      test/test_detection_class_remapper.cpp
    )
    ament_auto_add_gtest(test_ros_utils
      test/test_ros_utils.cpp
    )
    ament_auto_add_gtest(test_nms
      test/test_nms.cpp
    )

    # Temporary disabled, tracked by:
    # https://github.com/autowarefoundation/autoware_universe/issues/7724
    # ament_auto_add_gtest(test_voxel_generator
    #   test/test_voxel_generator.cpp
    # )

    # # preprocess kernel test
    # add_executable(test_preprocess_kernel
    #   test/test_preprocess_kernel.cpp
    # )
    # target_include_directories(test_preprocess_kernel PUBLIC
    #   ${test_preprocess_kernel_SOURCE_DIR}
    # )
    # target_link_libraries(test_preprocess_kernel
    #   ${PROJECT_NAME}_cuda_lib
    #   gtest
    #   gtest_main
    # )
    # ament_add_test(test_preprocess_kernel
    #   GENERATE_RESULT_FOR_RETURN_CODE_ZERO
    #   COMMAND "$<TARGET_FILE:test_preprocess_kernel>"
    # )

    # # postprocess kernel test
    # add_executable(test_postprocess_kernel
    #   test/test_postprocess_kernel.cpp
    # )
    # target_include_directories(test_postprocess_kernel PUBLIC
    #   ${test_postprocess_kernel_SOURCE_DIR}
    # )
    # target_link_libraries(test_postprocess_kernel
    #   ${PROJECT_NAME}_cuda_lib
    #   gtest
    #   gtest_main
    # )
    # ament_add_test(test_postprocess_kernel
    #   GENERATE_RESULT_FOR_RETURN_CODE_ZERO
    #   COMMAND "$<TARGET_FILE:test_postprocess_kernel>"
    # )

  endif()

  ament_auto_package(
    INSTALL_TO_SHARE
      launch
      config
  )

else()
  find_package(ament_cmake_auto REQUIRED)
  ament_auto_find_build_dependencies()

  ament_auto_package(
    INSTALL_TO_SHARE
      launch
  )
endif()
