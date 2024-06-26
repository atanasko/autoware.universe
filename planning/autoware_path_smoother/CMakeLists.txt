cmake_minimum_required(VERSION 3.14)
project(autoware_path_smoother)

find_package(autoware_cmake REQUIRED)
autoware_package()

find_package(Eigen3 REQUIRED)

ament_auto_add_library(autoware_path_smoother SHARED
  DIRECTORY src
)

target_include_directories(autoware_path_smoother
  SYSTEM PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)

# register node
rclcpp_components_register_node(autoware_path_smoother
  PLUGIN "autoware::path_smoother::ElasticBandSmoother"
  EXECUTABLE elastic_band_smoother
)

if(BUILD_TESTING)
  ament_add_ros_isolated_gtest(test_${PROJECT_NAME}
    test/test_${PROJECT_NAME}_node_interface.cpp
  )
  target_link_libraries(test_${PROJECT_NAME}
    ${PROJECT_NAME}
  )
endif()

ament_auto_package(
  INSTALL_TO_SHARE
    launch
    config
)

install(PROGRAMS
  scripts/calculation_time_plotter.py
  DESTINATION lib/${PROJECT_NAME}
)
