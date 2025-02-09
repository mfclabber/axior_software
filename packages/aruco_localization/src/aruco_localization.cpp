#include "aruco_localization.hpp"

#include "cv_bridge/cv_bridge.h"
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <chrono>
#include <opencv2/calib3d.hpp>
#include <thread>

#include "visualization_helpers.hpp"
#include "math_helpers.hpp"
#include "msgs_helpers.hpp"

using std::placeholders::_1;

const static std::string kImageRawTopic = "/camera/camera/color/image_raw";
const static std::string kCameraInfoTopic = "/camera/camera/color/camera_info";

const static std::string PiImageRawTopic = "/image_raw";
const static std::string PiCameraInfoTopic = "/camera_info";

const static std::string kArucoLocalizationNodeName = "aruco_localization";
const static std::string kArucoOdometryTopic = "/axior/aruco/odometry";
const static std::string kArucoPoseTopic = "/axior/aruco/pose";
const static std::string kArucoMarkersTopic = "/axior/aruco/markers";
const static std::string kArucoTransformsTopic = "/axior/aruco/transforms";

const static std::string kCameraFrameId = "camera";

const static int kCameraMatrixSize = 3;
const static int kDistCoeffsCount = 5;
const static int kCV_64FSize = 8;
const static int kMarkerCount = 250;

namespace rosaruco {

rclcpp::QoS qos(1);
int id_camera = 1;

ArucoLocalization::ArucoLocalization(): rclcpp::Node(kArucoLocalizationNodeName),
                                        camera_matrix_(kCameraMatrixSize, kCameraMatrixSize, CV_64F),
                                        dist_coeffs_(1, kDistCoeffsCount, CV_64F),
                                        detector_parameters_(cv::makePtr<cv::aruco::DetectorParameters>()),
                                        marker_dictionary_(cv::makePtr<cv::aruco::Dictionary>(
                                             cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_250))),
                                        coordinator_(kMarkerCount) {

    // rclcpp::QoS qos(1);
    qos.reliable();
    qos.durability_volatile();

    subscription_image_raw_ = this->create_subscription<sensor_msgs::msg::Image>(
        kImageRawTopic, qos, std::bind(&ArucoLocalization::HandleImage, this, _1));

    subscription_camera_info_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
        kCameraInfoTopic, qos, std::bind(&ArucoLocalization::UpdateCameraInfo, this, _1));

    // publisher_odometry_ = this->create_publisher<nav_msgs::msg::Odometry>(kArucoOdometryTopic, qos);
    // publisher_pose_stamped_ =
    //     this->create_publisher<geometry_msgs::msg::PoseStamped>(kArucoPoseTopic, qos);
    publisher_marker_array_ =
        this->create_publisher<aruco_msgs::msg::ArucoMarker>(kArucoMarkersTopic, qos);
    // publisher_tf2_transform_ =
    //     this->create_publisher<tf2_msgs::msg::TFMessage>(kArucoTransformsTopic, qos);
}

void ArucoLocalization::HandleImage(sensor_msgs::msg::Image::ConstSharedPtr msg) {
    // RCLCPP_INFO(this->get_logger(), "HandleImage got message");
    auto cv_image = cv_bridge::toCvShare(msg);

    std::vector<int> marker_ids;
    std::vector<std::vector<cv::Point2f>> marker_corners, rejected_candidates;
    
    cv::aruco::detectMarkers(cv_image->image, marker_dictionary_, marker_corners, marker_ids,
                             detector_parameters_, rejected_candidates);
    // RCLCPP_INFO(this->get_logger(), "HandleImage detected %ld markers", detector_parameters_);
    if(marker_ids.size() > 0)
        RCLCPP_INFO(this->get_logger(), "ID %i marker", marker_ids[0]);
    else    
        // this->subscription_image_raw_ = this->create_subscription<sensor_msgs::msg::Image>(
        //     PiImageRawTopic, qos, std::bind(&ArucoLocalization::HandleImage, this, _1));
        // this->subscription_camera_info_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
        //     PiCameraInfoTopic, qos, std::bind(&ArucoLocalization::UpdateCameraInfo, this, _1));

        if(id_camera == 1) {
            id_camera = 2;
            // this->subscription_image_raw_ = this->create_subscription<sensor_msgs::msg::Image>(
            //     PiImageRawTopic, qos, std::bind(&ArucoLocalization::HandleImage, this, _1));
            // this->subscription_camera_info_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
            //     PiCameraInfoTopic, qos, std::bind(&ArucoLocalization::UpdateCameraInfo, this, _1));
        }else {
            id_camera = 1;
            this->subscription_image_raw_ = this->create_subscription<sensor_msgs::msg::Image>(
                kImageRawTopic, qos, std::bind(&ArucoLocalization::HandleImage, this, _1));
            this->subscription_camera_info_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
                kCameraInfoTopic, qos, std::bind(&ArucoLocalization::UpdateCameraInfo, this, _1));
        }
        // RCLCPP_INFO(this->get_logger(), "Change camera %i", id_camera);

    std::vector<cv::Vec3d> rvecs, tvecs;

    cv::aruco::estimatePoseSingleMarkers(marker_corners, 0.05, camera_matrix_,
                                            dist_coeffs_, rvecs, tvecs);

    std::vector<Transform> from_cam_to_marker;
    // from_cam_to_marker.reserve(rvecs.size());

    // for (size_t i = 0; i < rvecs.size(); i++) {
    //     from_cam_to_marker.push_back(GetTransform(rvecs[i], tvecs[i]));
    // }
    
    // coordinator_.Update(marker_ids, from_cam_to_marker);

    // auto pose = coordinator_.GetPose();

    // geometry_msgs::msg::Pose pose_msg;

    // SetPoint(pose.point, pose_msg.position);
    
    // auto orientation_msg = tf2::toMsg(pose.orientation);

    // pose_msg.orientation = orientation_msg;

    // if (publisher_pose_stamped_->get_subscription_count()) {
    //     geometry_msgs::msg::PoseStamped pose_stamped;
    //     pose_stamped.pose = pose_msg;
    //     pose_stamped.header.stamp = msg->header.stamp;
    //     publisher_pose_stamped_->publish(pose_stamped);
    //     RCLCPP_INFO(this->get_logger(), "POSE GETTING");
    // }

    // if (publisher_tf2_transform_->get_subscription_count()) {
    //     geometry_msgs::msg::TransformStamped transform_msg;

    //     transform_msg.header.stamp = msg->header.stamp;
    //     transform_msg.header.frame_id = "";
    //     transform_msg.child_frame_id = kCameraFrameId;

    //     geometry_msgs::msg::Vector3 translation_msg;
    //     SetPoint(pose.point, translation_msg);

    //     transform_msg.transform.rotation = orientation_msg;
    //     transform_msg.transform.translation = translation_msg;

    //     tf2_msgs::msg::TFMessage tf_msg;
    //     tf_msg.transforms.push_back(transform_msg);

    //     publisher_tf2_transform_->publish(tf_msg);
    // }

    // nav_msgs::msg::Odometry odometry_msg;
    // odometry_msg.pose.pose = pose_msg;
    // odometry_msg.header.stamp = msg->header.stamp;
    // publisher_odometry_->publish(odometry_msg);

    if (publisher_marker_array_->get_subscription_count() && marker_ids.size() > 0) {
        // RCLCPP_INFO(this->get_logger(), "TOPIC PUBLISHING");
        aruco_msgs::msg::ArucoMarker marker_array;

        std::unordered_set<int> visible_markers(marker_ids.begin(), marker_ids.end());

        // for (size_t i = 0; i < kMarkerCount; i++) {
        //     auto to_anchor = coordinator_.GetTransformToAnchor(i);
        //     if (to_anchor) {
        //         AddLabeledMarker(marker_array.ids, *to_anchor, i, 1.0, visible_markers.count(i));
        //     }
        // }

        // for (auto &marker : marker_array.ids) {
        //     // marker.header.stamp = msg->header.stamp;
        //     marker.ids[0] = marker_ids[0];
        // }
        marker_array.id = marker_ids[0];

        marker_array.poses.position.x = tvecs[0][0];
        marker_array.poses.position.y = tvecs[0][1];
        marker_array.poses.position.z = tvecs[0][2];

        marker_array.poses.orientation.x = rvecs[0][0];
        marker_array.poses.orientation.y = rvecs[0][1];
        marker_array.poses.orientation.z = rvecs[0][2];
        marker_array.poses.orientation.w = rvecs[0][3];
        
        // RCLCPP_INFO(this->get_logger(), "%f", rvecs[0][0]);

        publisher_marker_array_->publish(marker_array);
    }
}

void ArucoLocalization::UpdateCameraInfo(sensor_msgs::msg::CameraInfo::ConstSharedPtr msg) {
    // RCLCPP_INFO(this->get_logger(), "UpdateCameraInfo got message");

    static_assert(std::tuple_size<decltype(msg->k)>::value ==
                  kCameraMatrixSize * kCameraMatrixSize);
    static_assert(sizeof(decltype(msg->k)::value_type) == kCV_64FSize);
    std::memcpy(camera_matrix_.data, msg->k.data(),
                kCameraMatrixSize * kCameraMatrixSize * kCV_64FSize);

    static_assert(sizeof(decltype(msg->d)::value_type) == kCV_64FSize);
    assert(msg->d.size() == kDistCoeffsCount);
    std::memcpy(dist_coeffs_.data, msg->d.data(), kDistCoeffsCount * kCV_64FSize);
}

}

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<rosaruco::ArucoLocalization>());
    rclcpp::shutdown();
    return 0;
} // namespace rosaruco
