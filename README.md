# ROG-Map

`rog_map` builds a local three-dimensional occupancy map from point clouds and odometry. It provides obstacle inflation, frontier extraction and an optional Euclidean signed distance field (ESDF) for planning and spatial queries.

The LASER UAV System (LUS) uses ROG-Map with SUPER. The package exports the `rog_map_lib` C++ library and the `rog_map::RunRogMap` ROS 2 component. SUPER embeds the library; the component runs mapping independently.

## Build and run

From a ROS 2 workspace with the package dependencies installed:

```bash
colcon build --packages-up-to rog_map
source install/setup.bash
```

Start the component in its own container:

```bash
ros2 launch rog_map run_rog_map.launch.py uav_name:=uav1 use_sim_time:=true
```

The launch creates `/uav1/run_rog_map` in a multithreaded component container. Start the odometry, point-cloud source and required TF publishers separately. A SUPER planning process already has its own map and does not need this additional component.

### Launch arguments

| Argument | Default | Meaning |
| --- | --- | --- |
| `uav_name` | `UAV_NAME`, or `uav1` | Component namespace. |
| `rog_map_config` | `default.yaml` | Configuration filename under the installed package's `config/` directory. |
| `use_sim_time` | `true` when `REAL_UAV=false` | Uses the ROS simulation clock. |
| `standalone` | `true` | Starts a new component container. Set `false` to load into an existing container. |
| `container_name` | Empty | Target container when `standalone=false`. |
| `log_level` | `info` | Logging level passed to the standalone container. |

For RViz, set `UAV_NAME` to the same vehicle and run:

```bash
export UAV_NAME=uav1
ros2 launch rog_map rviz.launch.py
```

The visualization helper uses `UAV_NAME` to adapt the supplied RViz configuration.

## Interfaces

With `uav_name:=uav1` and [config/default.yaml](config/default.yaml):

| Direction | Topic | Type | Purpose |
| --- | --- | --- | --- |
| Input | `/uav1/livox/lidar` | `sensor_msgs/msg/PointCloud2` | Point cloud to insert into the map. |
| Input | `/uav1/estimation_manager/estimation` | `nav_msgs/msg/Odometry` | Vehicle pose and map coordinate frame. |
| Output | `/uav1/rog_map/occ` | `sensor_msgs/msg/PointCloud2` | Occupied cells. |
| Output | `/uav1/rog_map/inf_occ` | `sensor_msgs/msg/PointCloud2` | Inflated occupied cells. |
| Output | `/uav1/rog_map/unk`, `/uav1/rog_map/inf_unk` | `sensor_msgs/msg/PointCloud2` | Unknown and inflated unknown cells, when enabled. |
| Output | `/uav1/rog_map/frontier` | `sensor_msgs/msg/PointCloud2` | Frontier cells, when extraction is enabled. |
| Output | `/uav1/rog_map/esdf` | `sensor_msgs/msg/PointCloud2` | ESDF visualization, when enabled. |
| Output | `/uav1/rog_map/map_bound` | `visualization_msgs/msg/MarkerArray` | Map bounds and visualization markers. |

Input topics come from the selected YAML. Relative names resolve inside the UAV namespace; absolute names are used directly. Sensor subscriptions and visualization publishers use best-effort QoS with a depth of one. The visualization clouds depend on their enable flags and connected subscribers. Map queries are exposed through the C++ library, rather than ROS services.

### Frames and timing

`rog_map.ros_callback.target_frame` defaults to an empty string. The first valid odometry message then selects the map frame. An explicit value must match the odometry message's `header.frame_id`; subsequent odometry in a different frame is rejected.

Each cloud is transformed into that frame at its message timestamp before insertion. The TF lookup can wait up to 50 ms of ROS time for a delayed transform. Clouds are skipped when odometry is unavailable, has timed out or the required transform cannot be found.

Map clouds and markers use the selected map frame. `rog_map.visualization.frame_id` does not override it when ROS callbacks are active. ROG-Map does not publish an odometry-to-FCU transform; the state-estimation or sensor setup must provide the required TF relationships. Use the same clock across sensors, mapping and planning.

## Configuration

The YAML files are loaded through `config_path` by the component. They use a top-level `rog_map` key, without a `ros__parameters` wrapper. `use_sim_time` remains a ROS parameter supplied by the launch.

### Supplied profiles

| Setting | `default.yaml` | `ogm_map.yaml` |
| --- | --- | --- |
| Cloud input | `livox/lidar` | `livox_up/lidar` |
| Odometry input | `estimation_manager/estimation` | `estimation_manager/estimation` |
| Voxel and inflation resolution | `0.15` m | `0.25` m |
| Map dimensions | `[20, 20, 20]` m | `[25, 25, 5]` m |
| Occupied-cell inflation | `1` step | `2` steps |
| Sliding map | Disabled | Enabled, `0.1` m threshold |
| Frontier extraction | Enabled | Disabled |
| Point sampling stride | `15` | `1` |
| Visualization rate | `10` Hz | `2` Hz |
| Raycasting and ESDF | Disabled | Disabled |

Select the second profile with `rog_map_config:=ogm_map.yaml`. Check its sensor topic and map bounds against your setup.

### Map and sensor settings

All parameter paths below are relative to `rog_map`.

| Parameter | Meaning |
| --- | --- |
| `resolution`, `inflation_resolution` | Occupancy and inflated-map voxel sizes in metres. |
| `map_size` | Local map dimensions in metres. |
| `inflation_step` | Number of neighboring-cell steps used to inflate occupied cells. |
| `unk_inflation_en`, `unk_inflation_step` | Enables and sets inflation of unknown cells. |
| `map_sliding.enable`, `map_sliding.threshold` | Moves the local map with the vehicle after the configured displacement in metres. |
| `fix_map_origin` | Map origin when sliding is disabled. |
| `virtual_ground_height`, `virtual_ceil_height` | Vertical limits in metres; cells outside them are treated as occupied. |
| `frontier_extraction_en` | Enables frontier-cell extraction. |
| `intensity_thresh` | Positive values enable a minimum-intensity filter; zero or negative values disable it. |
| `point_filt_num` | Retains every nth input point; values below 1 are clamped to 1. This is a point-sampling stride, not a cloud-message rate limit. |
| `ros_callback.enable` | Enables ROS odometry/cloud subscriptions and map updates. |
| `ros_callback.cloud_topic`, `ros_callback.odom_topic` | Sensor input names. |
| `ros_callback.target_frame` | Map frame; empty selects the first valid odometry frame. |
| `ros_callback.odom_timeout` | Maximum time since the last accepted odometry callback, in seconds. Both supplied profiles use `2.0`. |
| `load_pcd_en`, `pcd_name` | Enables loading a map from a PCD file and selects its path. |

### Raycasting and distance fields

| Parameter | Meaning |
| --- | --- |
| `raycasting.enable` | Enables free-space updates along rays. With it disabled, the map maintains occupied endpoints and leaves other space unknown. |
| `raycasting.batch_update_size` | Number of input frames batched for occupancy updates. |
| `raycasting.local_update_box`, `raycasting.ray_range` | Update-region dimensions and ray-distance bounds in metres. |
| `raycasting.p_hit`, `raycasting.p_miss` | Occupancy-update probabilities for hits and misses. |
| `raycasting.p_min`, `raycasting.p_max` | Probability bounds. |
| `raycasting.p_occ`, `raycasting.p_free` | Occupied/free classification thresholds. |
| `raycasting.unk_thresh` | Unknown-cell proportion used by the inflated map. |
| `esdf.enable`, `esdf.resolution`, `esdf.local_update_box` | Enables the distance field and sets its resolution and update region in metres. |

### Visualization

`visualization.enable` controls cloud visualization, `visualization.time_rate` sets its timer rate in Hz and `visualization.range` selects the region around the vehicle. `visualization.pub_unknown_map_en` enables unknown-cell output; inflated unknown output also requires `unk_inflation_en`.

`visualization.frame_rate` and `visualization.use_dynamic_reconfigure` are parsed from the YAML but do not provide frame-triggered updates or a dynamic-reconfigure interface in this ROS 2 wrapper. Set the visualization timer and edit configuration before startup.

## C++ integration

Include `<rog_map_ros/rog_map_ros.hpp>` and construct `rog_map::ROGMapROS` with an existing ROS node and a configuration-file path. Link the exported library:

```cmake
find_package(rog_map REQUIRED)
target_link_libraries(your_target rog_map_lib)
```

The library provides occupancy, inflated-occupancy, line-of-sight and nearest-cell queries. When feeding the map directly, set `ros_callback.enable: false` and call `updateMap()` with the cloud and vehicle pose in a common frame.

SUPER uses this embedded form and loads its mapping settings from the SUPER configuration. Changing a standalone ROG-Map profile does not change the map inside SUPER.

## Checks

The package includes `test_ros_frames`, covering cloud transforms, odometry-frame selection and delayed TF handling. To build and run its checks from the workspace root:

```bash
colcon build --packages-select rog_map --cmake-args -DBUILD_TESTING=ON
colcon test --packages-select rog_map --event-handlers console_direct+
```
