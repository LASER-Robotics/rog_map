import os

import launch

from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, PythonExpression

from launch_ros.actions import ComposableNodeContainer, LoadComposableNodes
from launch_ros.descriptions import ComposableNode
from launch_ros.substitutions import FindPackageShare


def launch_setup(context: launch.LaunchContext, ld):
    namespace = 'run_rog_map'

    # #{ uav_name

    uav_name = LaunchConfiguration('uav_name')

    ld.add_action(DeclareLaunchArgument(
        'uav_name',
        default_value=os.getenv('UAV_NAME', 'uav1'),
        description='Top-level namespace.'
    ))

    # #}

    # #{ standalone

    standalone = LaunchConfiguration('standalone')

    ld.add_action(DeclareLaunchArgument(
        'standalone',
        default_value='true',
        description='Whether to start as a container or load into an existing container.'
    ))

    # #}

    # #{ container_name

    container_name = LaunchConfiguration('container_name')

    ld.add_action(DeclareLaunchArgument(
        'container_name',
        default_value='',
        description='Name of an existing container to load into (if standalone is false)'
    ))

    # #}

    # #{ use_sim_time

    use_sim_time = LaunchConfiguration('use_sim_time')

    ld.add_action(DeclareLaunchArgument(
        'use_sim_time',
        default_value=PythonExpression(['"', os.getenv('REAL_UAV', 'true'), '" == "false"']),
        description='Whether use the simulation time.'
    ))

    # #}

    # #{ log_level

    log_level = LaunchConfiguration('log_level')

    ld.add_action(DeclareLaunchArgument(
        'log_level',
        default_value='info',
        description='Log level.'
    ))

    # #}

    # #{ rog_map_config

    rog_map_config = LaunchConfiguration('rog_map_config')

    ld.add_action(DeclareLaunchArgument(
        'rog_map_config',
        default_value='default.yaml',
        description='Name of the RogMap configuration file.'
    ))

    # #}

    # #{ run node

    run_rog_map_test_node = ComposableNode(

        package='rog_map',
        plugin='rog_map::RunRogMap',
        namespace=uav_name,
        name='run_rog_map',

        parameters=[
            {'uav_name': uav_name},
            {'use_sim_time': use_sim_time},
            {'config_path': PathJoinSubstitution([
                FindPackageShare('rog_map'),
                'config',
                rog_map_config
            ])}
        ],

    )

    load_into_existing = LoadComposableNodes(
        target_container=container_name,
        composable_node_descriptions=[run_rog_map_test_node],
        condition=UnlessCondition(standalone)
    )

    ld.add_action(load_into_existing)

    # #}

    # #{ standalone container

    standalone_container = ComposableNodeContainer(
        namespace=uav_name,
        name=namespace+'_container',
        package='rclcpp_components',
        executable='component_container_mt',
        output='screen',
        arguments=['--ros-args', '--log-level', log_level],
        composable_node_descriptions=[run_rog_map_test_node],
        condition=IfCondition(standalone)
    )

    ld.add_action(standalone_container)

    # #}


def generate_launch_description():
    ld = launch.LaunchDescription()

    # #{ opaque function

    ld.add_action(
        OpaqueFunction(function=launch_setup, args=[ld])
    )

    # #}

    return ld
