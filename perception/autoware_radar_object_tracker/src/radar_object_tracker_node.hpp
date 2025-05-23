// Copyright 2023 TIER IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RADAR_OBJECT_TRACKER_NODE_HPP_
#define RADAR_OBJECT_TRACKER_NODE_HPP_

#include "autoware_radar_object_tracker/association/data_association.hpp"

#include <autoware_lanelet2_extension/utility/message_conversion.hpp>
#include <autoware_lanelet2_extension/utility/query.hpp>
#include <autoware_lanelet2_extension/utility/utilities.hpp>
#include <diagnostic_updater/diagnostic_updater.hpp>
#include <rclcpp/rclcpp.hpp>

#include "autoware_perception_msgs/msg/detected_objects.hpp"
#include "autoware_perception_msgs/msg/tracked_objects.hpp"
#include <geometry_msgs/msg/pose_stamped.hpp>

#include <lanelet2_core/LaneletMap.h>
#include <lanelet2_core/geometry/BoundingBox.h>
#include <lanelet2_core/geometry/Lanelet.h>
#include <lanelet2_core/geometry/Point.h>
#include <lanelet2_routing/RoutingGraph.h>
#include <lanelet2_traffic_rules/TrafficRulesFactory.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/convert.h>
#include <tf2/transform_datatypes.h>

#ifdef ROS_DISTRO_GALACTIC
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#else
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#endif
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <list>
#include <map>
#include <memory>
#include <string>

namespace autoware::radar_object_tracker
{
using autoware_map_msgs::msg::LaneletMapBin;
using autoware_perception_msgs::msg::DetectedObject;
using autoware_perception_msgs::msg::DetectedObjects;
using autoware_perception_msgs::msg::TrackedObject;
using autoware_perception_msgs::msg::TrackedObjects;

class RadarObjectTrackerNode : public rclcpp::Node
{
public:
  explicit RadarObjectTrackerNode(const rclcpp::NodeOptions & node_options);

private:
  // pub-sub
  rclcpp::Publisher<autoware_perception_msgs::msg::TrackedObjects>::SharedPtr tracked_objects_pub_;
  rclcpp::Subscription<autoware_perception_msgs::msg::DetectedObjects>::SharedPtr
    detected_object_sub_;
  rclcpp::TimerBase::SharedPtr publish_timer_;              // publish timer
  rclcpp::Subscription<LaneletMapBin>::SharedPtr sub_map_;  // map subscriber

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;
  float tracker_lifetime_;
  std::map<std::uint8_t, std::string> tracker_map_;

  int measurement_count_threshold_;

  void onMeasurement(
    const autoware_perception_msgs::msg::DetectedObjects::ConstSharedPtr input_objects_msg);
  void onTimer();
  void onMap(const LaneletMapBin::ConstSharedPtr msg);

  std::string world_frame_id_;  // tracking frame
  std::string tracker_config_directory_;
  std::list<std::shared_ptr<Tracker>> list_tracker_;
  std::unique_ptr<DataAssociation> data_association_;

  // debug parameters
  struct logging
  {
    bool enable = false;
    std::string path;
  } logging_;

  // noise reduction
  bool use_distance_based_noise_filtering_;
  bool use_map_based_noise_filtering_;

  // distance based noise reduction
  double minimum_range_threshold_;
  std::string sensor_frame_ = "base_link";

  // map based noise reduction
  bool map_is_loaded_ = false;
  double max_distance_from_lane_;
  double max_lateral_velocity_;
  double max_angle_diff_from_lane_;
  // Lanelet Map Pointers
  std::shared_ptr<lanelet::LaneletMap> lanelet_map_ptr_;
  std::shared_ptr<lanelet::routing::RoutingGraph> routing_graph_ptr_;
  std::shared_ptr<lanelet::traffic_rules::TrafficRules> traffic_rules_ptr_;
  // Crosswalk Entry Points
  // lanelet::ConstLanelets crosswalks_;

  // diagnostics
  double radar_input_stale_threshold_ms_;
  std::optional<rclcpp::Time> last_processing_timestamp_;
  diagnostic_updater::Updater diagnostic_updater_{this};

  void checkTrackerLifeCycle(
    std::list<std::shared_ptr<Tracker>> & list_tracker, const rclcpp::Time & time,
    const geometry_msgs::msg::Transform & self_transform);
  void sanitizeTracker(
    std::list<std::shared_ptr<Tracker>> & list_tracker, const rclcpp::Time & time);
  void mapBasedNoiseFilter(
    std::list<std::shared_ptr<Tracker>> & list_tracker, const rclcpp::Time & time);
  void distanceBasedNoiseFilter(
    std::list<std::shared_ptr<Tracker>> & list_tracker, const rclcpp::Time & time,
    const geometry_msgs::msg::Transform & self_transform);
  std::shared_ptr<Tracker> createNewTracker(
    const autoware_perception_msgs::msg::DetectedObject & object, const rclcpp::Time & time,
    const geometry_msgs::msg::Transform & self_transform) const;

  void publish(const rclcpp::Time & time) const;
  inline bool shouldTrackerPublish(const std::shared_ptr<const Tracker> tracker) const;

  void diagnoseRadarInputInterval(diagnostic_updater::DiagnosticStatusWrapper & stat);
};

}  // namespace autoware::radar_object_tracker

#endif  // RADAR_OBJECT_TRACKER_NODE_HPP_
