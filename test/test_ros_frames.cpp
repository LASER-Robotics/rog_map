#include <gtest/gtest.h>
#include <rog_map_ros/rog_map_ros.hpp>
#include <tf2_ros/static_transform_broadcaster.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

class MapFrames : public testing::Test {
protected:
    static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
    static void TearDownTestSuite() { rclcpp::shutdown(); }

    void SetUp() override {
        node = std::make_shared<rclcpp::Node>("map_frame_test", "/rog_map_test");
        executor.add_node(node);
        odom_pub = node->create_publisher<nav_msgs::msg::Odometry>("odom", 1);
        cloud_pub = node->create_publisher<sensor_msgs::msg::PointCloud2>("cloud", 1);
        config_path = std::filesystem::temp_directory_path() /
                      ("rog_map_frames_" + std::to_string(getpid()) + ".yaml");
    }

    void TearDown() override {
        executor.remove_node(node);
        map.reset();
        std::filesystem::remove(config_path);
    }

    void makeMap(const std::string &target_frame = "") {
        auto config = YAML::LoadFile(TEST_CONFIG_PATH);
        auto settings = config["rog_map"];
        settings["map_size"] = std::vector<double>{12, 12, 12};
        settings["resolution"] = 0.25;
        settings["inflation_resolution"] = 0.25;
        settings["frontier_extraction_en"] = false;
        settings["point_filt_num"] = 1;
        settings["visualization"]["time_rate"] = 100;
        settings["ros_callback"]["target_frame"] = target_frame;
        settings["ros_callback"]["odom_topic"] = "odom";
        settings["ros_callback"]["cloud_topic"] = "cloud";
        settings["ros_callback"]["odom_timeout"] = 5.0;
        std::ofstream(config_path) << config;
        map = std::make_shared<rog_map::ROGMapROS>(node, config_path.string());
        ASSERT_TRUE(spinUntil([&] {
            return odom_pub->get_subscription_count() && cloud_pub->get_subscription_count();
        }));
    }

    template <typename Predicate>
    bool spinUntil(Predicate ready, std::chrono::milliseconds timeout = 2s) {
        const auto end = std::chrono::steady_clock::now() + timeout;
        do {
            executor.spin_some();
            if (ready()) return true;
            std::this_thread::sleep_for(2ms);
        } while (std::chrono::steady_clock::now() < end);
        return ready();
    }

    void odometry(const std::string &frame = "uav7/odometry", double x = 0.0) {
        nav_msgs::msg::Odometry msg;
        msg.header.stamp = node->now();
        msg.header.frame_id = frame;
        msg.pose.pose.orientation.w = 1.0;
        msg.pose.pose.position.x = x;
        odom_pub->publish(msg);
    }

    void cloud(const std::string &frame) {
        rog_map::PointCloud points;
        pcl::PointXYZI point;
        point.x = 1.0f;
        point.y = point.z = 0.0f;
        point.intensity = 1.0f;
        points.push_back(point);
        sensor_msgs::msg::PointCloud2 msg;
        pcl::toROSMsg(points, msg);
        msg.header.stamp = node->now();
        msg.header.frame_id = frame;
        cloud_pub->publish(msg);
    }

    rclcpp::Node::SharedPtr node;
    rclcpp::executors::SingleThreadedExecutor executor;
    std::shared_ptr<rog_map::ROGMapROS> map;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_pub;
    std::filesystem::path config_path;
};

TEST_F(MapFrames, TransformsCoordinatesAndPublishesOdometryFrame) {
    makeMap();
    odometry();
    ASSERT_TRUE(spinUntil([&] { return map->getRobotState().rcv; }));

    tf2_ros::StaticTransformBroadcaster broadcaster(node);
    geometry_msgs::msg::TransformStamped transform;
    transform.header.stamp = node->now();
    transform.header.frame_id = "uav7/odometry";
    transform.child_frame_id = "uav7/lidar";
    transform.transform.translation.x = 2.125;
    transform.transform.translation.y = 0.125;
    transform.transform.translation.z = 0.125;
    transform.transform.rotation.z = std::sqrt(0.5);
    transform.transform.rotation.w = std::sqrt(0.5);
    broadcaster.sendTransform(transform);

    sensor_msgs::msg::PointCloud2::SharedPtr occupied;
    visualization_msgs::msg::MarkerArray::SharedPtr markers;
    auto occ_sub = node->create_subscription<sensor_msgs::msg::PointCloud2>(
        "rog_map/occ", rclcpp::SensorDataQoS(),
        [&](sensor_msgs::msg::PointCloud2::SharedPtr msg) { occupied = msg; });
    auto marker_sub = node->create_subscription<visualization_msgs::msg::MarkerArray>(
        "rog_map/map_bound", rclcpp::SensorDataQoS(),
        [&](visualization_msgs::msg::MarkerArray::SharedPtr msg) { markers = msg; });
    EXPECT_TRUE(spinUntil([&] {
        cloud("uav7/lidar");
        return map->isOccupied(rog_map::Vec3f(2.125, 1.125, 0.125)) && occupied && markers;
    }));
    EXPECT_TRUE(map->isOccupied(rog_map::Vec3f(2.125, 1.125, 0.125)));
    EXPECT_FALSE(map->isOccupied(rog_map::Vec3f(1, 0, 0)));
    ASSERT_TRUE(occupied);
    ASSERT_TRUE(markers);
    EXPECT_EQ(occupied->header.frame_id, "uav7/odometry");
    ASSERT_FALSE(markers->markers.empty());
    for (const auto &marker : markers->markers) {
        EXPECT_EQ(marker.header.frame_id, "uav7/odometry");
        EXPECT_NEAR(rclcpp::Time(marker.header.stamp).seconds(), node->now().seconds(), 2.0);
    }
}

TEST_F(MapFrames, AcceptsCloudAlreadyInOdometryFrame) {
    makeMap();
    odometry();
    ASSERT_TRUE(spinUntil([&] { return map->getRobotState().rcv; }));
    ASSERT_TRUE(spinUntil([&] {
        cloud("uav7/odometry");
        return map->isOccupied(rog_map::Vec3f(1, 0, 0));
    }));
}

TEST_F(MapFrames, RejectsMissingTransform) {
    makeMap();
    odometry();
    ASSERT_TRUE(spinUntil([&] { return map->getRobotState().rcv; }));
    cloud("uav7/missing_sensor");
    spinUntil([] { return false; }, 100ms);
    EXPECT_FALSE(map->isOccupied(rog_map::Vec3f(1, 0, 0)));
}

TEST_F(MapFrames, RejectsCloudWithoutOdometry) {
    makeMap();
    cloud("uav7/odometry");
    spinUntil([] { return false; }, 100ms);
    EXPECT_FALSE(map->getRobotState().rcv);
    EXPECT_FALSE(map->isOccupied(rog_map::Vec3f(1, 0, 0)));
}

TEST_F(MapFrames, RejectsEmptyAndChangedOdometryFrames) {
    makeMap();
    odometry("");
    spinUntil([] { return false; }, 100ms);
    EXPECT_FALSE(map->getRobotState().rcv);
    odometry();
    ASSERT_TRUE(spinUntil([&] { return map->getRobotState().rcv; }));
    odometry("uav7/other", 3.0);
    spinUntil([] { return false; }, 100ms);
    EXPECT_EQ(map->getMapConfig().target_frame, "uav7/odometry");
    EXPECT_DOUBLE_EQ(map->getRobotState().p.x(), 0.0);
}

TEST_F(MapFrames, RejectsOdometryOutsideConfiguredTargetFrame) {
    makeMap("uav7/map");
    odometry();
    spinUntil([] { return false; }, 100ms);
    EXPECT_FALSE(map->getRobotState().rcv);
    odometry("uav7/map");
    EXPECT_TRUE(spinUntil([&] { return map->getRobotState().rcv; }));
}

TEST(MapGeometry, IntersectsBoxForSuperAstar) {
    rog_map::Vec3f hit;
    EXPECT_TRUE(rog_map::lineIntersectBox(rog_map::Vec3f(-2, 0, 0),
        rog_map::Vec3f(2, 0, 0), rog_map::Vec3f(-1, -1, -1),
        rog_map::Vec3f(1, 1, 1), hit));
    EXPECT_NEAR(hit.x(), -1.0, 1e-6);
    EXPECT_FALSE(rog_map::lineIntersectBox(rog_map::Vec3f(-2, 2, 0),
        rog_map::Vec3f(2, 2, 0), rog_map::Vec3f(-1, -1, -1),
        rog_map::Vec3f(1, 1, 1), hit));
}
