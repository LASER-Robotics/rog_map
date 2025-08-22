#include "rog_map_ros/rog_map_ros.hpp"

namespace rog_map
{

/* class RunRogMap //{ */

class RunRogMap : public rclcpp::Node {

public:
  RunRogMap(const rclcpp::NodeOptions & options);

private:
  void initialize();

  rclcpp::TimerBase::SharedPtr timer_preinitialization_;
  void timerPreInitialization();

  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<rog_map::ROGMapROS> rog_map_ros_;
};

//}

/* RunRogMap() //{ */

RunRogMap::RunRogMap(const rclcpp::NodeOptions & options) : rclcpp::Node("run_rog_map", options) {
  timer_preinitialization_ = create_wall_timer(std::chrono::duration<double>(1.0), std::bind(&RunRogMap::timerPreInitialization, this));
}

//}

/* timerPreInitialization() //{ */

void RunRogMap::timerPreInitialization() {
  node_ = this->shared_from_this();
  initialize();
  timer_preinitialization_->cancel();
}

//}

/* initialize() //{ */

void RunRogMap::initialize(){

  RCLCPP_INFO(node_->get_logger(), "Initializing");

  std::string config_path = "unset_path_to_config.yaml";
  node_->declare_parameter("config_path", rclcpp::ParameterType::PARAMETER_STRING);
  node_->get_parameter("config_path", config_path);
  rog_map_ros_ = std::make_shared<rog_map::ROGMapROS>(node_, config_path);

  RCLCPP_INFO_ONCE(node_->get_logger(), "Initialized!");
}

//}

} // namespace rog_map

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(rog_map::RunRogMap)
