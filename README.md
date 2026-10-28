# ROG-Map

ROS 2 component and mapping library used by the LASER UAV stack and SUPER.

```bash
ros2 launch rog_map run_rog_map.launch.py uav_name:=uav1 use_sim_time:=true
```

Select a configuration with `rog_map_config:=default.yaml` or `ogm_map.yaml`.
Input topics are relative to the UAV namespace.

`rog_map.ros_callback.target_frame` defaults to the first valid odometry frame.
If set explicitly, it must match the odometry frame. Point clouds are transformed
at their message timestamp using TF before insertion. A dedicated TF listener
allows clouds to wait up to 50 ms of ROS time for a slightly delayed transform.
Clouds without a matching transform and odometry with a different frame are
rejected. All map clouds and markers use that same frame; ROG-Map does not
broadcast a transform to `fcu`.

SUPER links the exported `rog_map_lib` library and includes
`rog_map_ros/rog_map_ros.hpp`. It embeds the map, so a separate mapping component
is only needed when running ROG-Map independently.

```bash
colcon build --packages-select rog_map --cmake-args -DBUILD_TESTING=ON
colcon test --packages-select rog_map --event-handlers console_direct+
```
