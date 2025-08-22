#!/bin/bash

PACKAGE_PATH=$(ros2 pkg prefix --share rog_map)

cp $PACKAGE_PATH/rviz/default.rviz /tmp/default.rviz

sed -i "s/uav[0-9]/$UAV_NAME/g" /tmp/default.rviz
