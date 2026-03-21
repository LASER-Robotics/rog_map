cmake_minimum_required(VERSION 3.5)
project(rog_map)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "-O3 -Wall -g -fPIC")
add_compile_options(-DQT_NO_VERSION_TAGGING)
add_compile_options(-Werror=unused-variable)
add_compile_options(-Werror=return-type)

# reduce warning
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0054 NEW)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "Suppress developer warnings")

add_definitions(-DROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

# Define the voxelize and raycasting method
add_definitions(-DORIGIN_AT_CORNER)

set(DEPENDENCIES
  ament_cmake
  rclcpp
  rclcpp_components
  std_msgs
  nav_msgs
  geometry_msgs
  visualization_msgs
  pcl_conversions
  tf2_ros
  tf2_sensor_msgs
)

foreach(DEP IN LISTS DEPENDENCIES)
  find_package(${DEP} REQUIRED)
endforeach()

set(includes
  include
  ${Eigen_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  )

set(THIRDPARTY_LIBRARIES
  ${PCL_LIBRARIES}
  yaml-cpp
  )


include_directories(
  ${includes}
  )

link_directories(
  ${PCL_LIBRARY_DIRS}
  )

add_definitions(
  ${PCL_DEFINITIONS}
  )

list(APPEND LIBRARY_SOURCES
  src/rog_map/counter_map.cpp
  src/rog_map/esdf_map.cpp
  src/rog_map/inf_map.cpp
  src/rog_map/prob_map.cpp
  src/rog_map/rog_map.cpp
  src/rog_map/sliding_map.cpp
  )

file(GLOB_RECURSE LIBRARY_HEADERS CONFIGURE_DEPENDS include/*.h include/*.hpp include/*.cpp)

add_library(rog_map_lib SHARED
  ${LIBRARY_SOURCES}
  ${LIBRARY_HEADERS}
  )

ament_target_dependencies(rog_map_lib
  ${DEPENDENCIES}
  )

target_link_libraries(rog_map_lib
  ${THIRDPARTY_LIBRARIES}
  )

install(TARGETS rog_map_lib
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include
  )

ament_export_include_directories(include)
ament_export_libraries(rog_map_lib)

# Add composable node
add_library(run_rog_map SHARED
  src/run_rog_map.cpp
  )

ament_target_dependencies(run_rog_map ${DEPENDENCIES})

target_link_libraries(run_rog_map
  rog_map_lib
  ${THIRDPARTY_LIBRARIES}
  )

# Register component
rclcpp_components_register_nodes(run_rog_map PLUGIN "rog_map::RunRogMap")

install(TARGETS run_rog_map
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  )

install(
  DIRECTORY
  launch
  config
  rviz
  DESTINATION share/${PROJECT_NAME}
  )

install(
  DIRECTORY scripts
  USE_SOURCE_PERMISSIONS
  DESTINATION share/${PROJECT_NAME}
  )

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(${DEPENDENCIES})

ament_package()
