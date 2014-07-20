#pragma once

/**\file localization.hpp
 * \brief Description...
 *
 * @version 1.0
 * @author Carlos Miguel Correia da Costa
 */

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <includes>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#include <dynamic_robot_localization/localization/localization.h>
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </includes>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



namespace dynamic_robot_localization {
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <imports>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </imports>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// =============================================================================  <public-section>  ============================================================================
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <constructors-destructor>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
template<typename PointT>
Localization<PointT>::Localization() :
	save_clouds_in_binary_format_(true),
	filter_reference_cloud_(true),
	compute_normals_reference_cloud_(true),
	compute_normals_ambient_cloud_(true),
	detect_keypoints_reference_cloud_(true),
	detect_keypoints_ambient_cloud_(true),
	max_outliers_percentage_(0.6),
	publish_tf_map_odom_(false),
	add_odometry_displacement_(false),
	last_scan_time_(0),
	last_map_received_time_(0),
	reference_pointcloud_received_(false),
	reference_pointcloud_2d_(false),
	ignore_height_corrections_(false),
	reference_pointcloud_(new pcl::PointCloud<PointT>()),
	reference_pointcloud_search_method_(new pcl::search::KdTree<PointT>()) {}

template<typename PointT>
Localization<PointT>::~Localization() {}
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </constructors-destructor>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <Localization-functions>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
template<typename PointT>
void Localization<PointT>::setupConfigurationFromParameterServer(ros::NodeHandlePtr& node_handle, ros::NodeHandlePtr& private_node_handle) {
	node_handle_ = node_handle;
	private_node_handle_ = private_node_handle;

	// general configurations
	setupSubcriptionTopicNamesFromParameterServer();
	setupPublishTopicNamesFromParameterServer();
	setupGeneralConfigurations();

	// localization pipeline configurations
	setupFiltersConfigurations();
	setupNormalEstimatorConfigurations();
	setupKeypointDetectors();
	setupCloudMatchersConfigurations();
	setupTransformationValidatorsConfigurations();
	setupOutlierDetectorsConfigurations();

	pose_to_tf_publisher_.setupConfigurationFromParameterServer(node_handle, private_node_handle);
	if (publish_tf_map_odom_) {
		pose_to_tf_publisher_.publishInitialPoseFromParameterServer();
	}
}


template<typename PointT>
void Localization<PointT>::setupSubcriptionTopicNamesFromParameterServer() {
	private_node_handle_->param("pointcloud_topic", ambient_pointcloud_topic_, std::string("assembled_pointcloud"));
	private_node_handle_->param("costmap_topic", costmap_topic_, std::string("map"));
	private_node_handle_->param("reference_cloud_topic", reference_pointcloud_topic_, std::string(""));
}


template<typename PointT>
void Localization<PointT>::setupPublishTopicNamesFromParameterServer() {
	private_node_handle_->param("reference_pointcloud_publish_topic", reference_pointcloud_publish_topic_, std::string("reference_pointcloud"));
	private_node_handle_->param("aligned_pointcloud_publish_topic", aligned_pointcloud_publish_topic_, std::string("aligned_pointcloud"));
	private_node_handle_->param("pose_publish_topic", pose_publish_topic_, std::string("initialpose"));
}


template<typename PointT>
void Localization<PointT>::setupGeneralConfigurations() {
	private_node_handle_->param("save_clouds_in_binary_format", save_clouds_in_binary_format_, true);
	private_node_handle_->param("filter_reference_cloud", filter_reference_cloud_, true);
	private_node_handle_->param("compute_normals_reference_cloud", compute_normals_reference_cloud_, true);
	private_node_handle_->param("compute_normals_ambient_cloud", compute_normals_ambient_cloud_, true);
	private_node_handle_->param("detect_keypoints_reference_cloud", detect_keypoints_reference_cloud_, true);
	private_node_handle_->param("detect_keypoints_ambient_cloud", detect_keypoints_ambient_cloud_, true);
	private_node_handle_->param("ignore_height_corrections", ignore_height_corrections_, false);
	private_node_handle_->param("max_outliers_percentage", max_outliers_percentage_, 0.6);
	private_node_handle_->param("publish_tf_map_odom", publish_tf_map_odom_, false);
	private_node_handle_->param("add_odometry_displacement", add_odometry_displacement_, false);
	private_node_handle_->param("reference_pointcloud_filename", reference_pointcloud_filename_, std::string(""));
	private_node_handle_->param("reference_pointcloud_preprocessed_save_filename", reference_pointcloud_preprocessed_save_filename_, std::string(""));
	private_node_handle_->param("reference_pointcloud_keypoints_filename", reference_pointcloud_keypoints_filename_, std::string(""));
	private_node_handle_->param("reference_pointcloud_keypoints_save_filename", reference_pointcloud_keypoints_save_filename_, std::string(""));
	private_node_handle_->param("map_frame_id", map_frame_id_, std::string("map"));
	private_node_handle_->param("base_link_frame_id", base_link_frame_id_, std::string("base_footprint"));
	private_node_handle_->param("sensor_frame_id_", sensor_frame_id_, std::string("hokuyo_tilt_laser_link"));

	double max_seconds_scan_age;
	private_node_handle_->param("max_seconds_scan_age", max_seconds_scan_age, 0.5);
	max_seconds_scan_age_.fromSec(max_seconds_scan_age);

	double min_seconds_between_scan_registration;
	private_node_handle_->param("min_seconds_between_scan_registration", min_seconds_between_scan_registration, 0.05);
	min_seconds_between_scan_registration_.fromSec(min_seconds_between_scan_registration);

	double min_seconds_between_reference_pointcloud_update;
	private_node_handle_->param("min_seconds_between_reference_pointcloud_update", min_seconds_between_reference_pointcloud_update, 5.0);
	min_seconds_between_reference_pointcloud_update_.fromSec(min_seconds_between_reference_pointcloud_update);
}


template<typename PointT>
void Localization<PointT>::setupFiltersConfigurations() {
	cloud_filters_.clear();
	typename CloudFilter<PointT>::Ptr default_filter(new VoxelGrid<PointT>());
	default_filter->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
	cloud_filters_.push_back(default_filter);
}


template<typename PointT>
void Localization<PointT>::setupNormalEstimatorConfigurations() {
	normal_estimator_ = typename NormalEstimator<PointT>::Ptr(new NormalEstimationOMP<PointT>());
//	normal_estimator_ = typename NormalEstimator<PointT>::Ptr(new MovingLeastSquares<PointT>());
	normal_estimator_->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
}


template<typename PointT>
void Localization<PointT>::setupKeypointDetectors() {
	keypoint_detectors_.clear();
	typename KeypointDetector<PointT>::Ptr default_keypoint_detector(new IntrinsicShapeSignature3D<PointT>());
	default_keypoint_detector->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
	keypoint_detectors_.push_back(default_keypoint_detector);
}


template<typename PointT>
void Localization<PointT>::setupCloudMatchersConfigurations() {
	cloud_matchers_.clear();

	std::string keypoint_descriptor;
	private_node_handle_->param("keypoint_descriptor", keypoint_descriptor, std::string(""));

	if (keypoint_descriptor == "PFH") {
		typename KeypointDescriptor<PointT, pcl::PFHSignature125>::Ptr keypoint_descriptor(new PFH<PointT, pcl::PFHSignature125>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::PFHSignature125>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::PFHSignature125>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	} else if (keypoint_descriptor == "FPFH") {
		typename KeypointDescriptor<PointT, pcl::FPFHSignature33>::Ptr keypoint_descriptor(new FPFH<PointT, pcl::FPFHSignature33>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::FPFHSignature33>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::FPFHSignature33>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	} else if (keypoint_descriptor == "SHOT") {
		typename KeypointDescriptor<PointT, pcl::SHOT352>::Ptr keypoint_descriptor(new SHOT<PointT, pcl::SHOT352>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::SHOT352>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::SHOT352>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	} else if (keypoint_descriptor == "3DSC") {
		typename KeypointDescriptor<PointT, pcl::ShapeContext1980>::Ptr keypoint_descriptor(new ShapeContext3D<PointT, pcl::ShapeContext1980>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::ShapeContext1980>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::ShapeContext1980>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	} else if (keypoint_descriptor == "USC") {
		typename KeypointDescriptor<PointT, pcl::ShapeContext1980>::Ptr keypoint_descriptor(new UniqueShapeContext<PointT, pcl::ShapeContext1980>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::ShapeContext1980>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::ShapeContext1980>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	} else if (keypoint_descriptor == "ESF") {
		typename KeypointDescriptor<PointT, pcl::ESFSignature640>::Ptr keypoint_descriptor(new ESF<PointT, pcl::ESFSignature640>());
		keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		typename FeatureMatcher<PointT, pcl::ESFSignature640>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, pcl::ESFSignature640>());
		initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
		initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(initial_aligment_matcher);
	}


	std::string final_registration;
	private_node_handle_->param("final_registration", final_registration, std::string("IterativeClosestPoint"));
	if (final_registration == "IterativeClosestPoint") {
		typename CloudMatcher<PointT>::Ptr final_aligment_matcher(new IterativeClosestPoint<PointT>());
		final_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(final_aligment_matcher);
	} else if (final_registration == "IterativeClosestPointWithNormals") {
		typename CloudMatcher<PointT>::Ptr final_aligment_matcher(new IterativeClosestPointWithNormals<PointT>());
		final_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		cloud_matchers_.push_back(final_aligment_matcher);
	}
}


template<typename PointT>
void Localization<PointT>::setupTransformationValidatorsConfigurations() {
	transformation_validators_.clear();
	TransformationValidator::Ptr default_validator(new EuclideanTransformationValidator());
	default_validator->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
	transformation_validators_.push_back(default_validator);
}


template<typename PointT>
void Localization<PointT>::setupOutlierDetectorsConfigurations() {
	std::string outlier_detector;
	private_node_handle_->param("outlier_detector", outlier_detector, std::string(""));
	if (outlier_detector == "EuclideanOutlierDetector") {
		outlier_detectors_.clear();
		typename OutlierDetector<PointT>::Ptr default_outlier_detector(new EuclideanOutlierDetector<PointT>());
		default_outlier_detector->setupConfigurationFromParameterServer(node_handle_, private_node_handle_);
		outlier_detectors_.push_back(default_outlier_detector);
	}
}


template<typename PointT>
bool Localization<PointT>::loadReferencePointCloudFromFile(const std::string& reference_pointcloud_filename) {
	if (pcl::io::loadPCDFile<PointT>(reference_pointcloud_filename, *reference_pointcloud_) == 0) {
		if (!reference_pointcloud_->points.empty()) {
			ROS_DEBUG_STREAM("Loaded reference point cloud from file " << reference_pointcloud_filename << " with " << reference_pointcloud_->points.size() << " points");
			reference_pointcloud_->header.frame_id = map_frame_id_;

			last_map_received_time_ = ros::Time::now();
			if (updateLocalizationPipelineWithNewReferenceCloud()) {
				reference_pointcloud_2d_ = false;
				return true;
			}
		}
	}

	return false;
}


template<typename PointT>
void Localization<PointT>::loadReferencePointCloudFromROSPointCloud(const sensor_msgs::PointCloud2ConstPtr& reference_pointcloud_msg) {
	if (!reference_pointcloud_received_ || (ros::Time::now() - last_map_received_time_) > min_seconds_between_reference_pointcloud_update_) {
		last_map_received_time_ = ros::Time::now();

		if (reference_pointcloud_msg->width > 0 && reference_pointcloud_msg->data.size() > 0 && reference_pointcloud_msg->fields.size() >= 3) {
			pcl::fromROSMsg(*reference_pointcloud_msg, *reference_pointcloud_);
			if (!reference_pointcloud_->points.empty()) {
				if (updateLocalizationPipelineWithNewReferenceCloud()) {
					reference_pointcloud_2d_ = false;
					ROS_DEBUG_STREAM("Loaded reference point cloud from cloud topic " << reference_pointcloud_topic_ << " with " << reference_pointcloud_->points.size() << " points");
				}
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::loadReferencePointCloudFromROSOccupancyGrid(const nav_msgs::OccupancyGridConstPtr& occupancy_grid_msg) {
	if (!reference_pointcloud_received_ || (ros::Time::now() - last_map_received_time_) > min_seconds_between_reference_pointcloud_update_) {
		last_map_received_time_ = ros::Time::now();

		if (pointcloud_conversions::fromROSMsg(*occupancy_grid_msg, *reference_pointcloud_)) {
			if (!reference_pointcloud_->points.empty()) {
				if (updateLocalizationPipelineWithNewReferenceCloud()) {
					reference_pointcloud_2d_ = true;
					ROS_DEBUG_STREAM("Loaded reference point cloud from costmap topic " << costmap_topic_ << " with " << reference_pointcloud_->points.size() << " points");
				}
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::publishReferencePointCloud() {
	if (!reference_pointcloud_publish_topic_.empty() && reference_pointcloud_publisher_.getNumSubscribers() > 0) {
		sensor_msgs::PointCloud2Ptr reference_pointcloud(new sensor_msgs::PointCloud2());
		pcl::toROSMsg(*reference_pointcloud_, *reference_pointcloud);
		reference_pointcloud->header.frame_id = map_frame_id_;
		reference_pointcloud->header.stamp = ros::Time::now();
		reference_pointcloud_publisher_.publish(reference_pointcloud);
	}
}


template<typename PointT>
bool Localization<PointT>::updateLocalizationPipelineWithNewReferenceCloud() {
	if (filter_reference_cloud_) {
		if (!applyFilters(reference_pointcloud_)) { return false; }
	}

	reference_pointcloud_search_method_->setInputCloud(reference_pointcloud_);
	if (compute_normals_reference_cloud_) {
		if (!applyNormalEstimation(reference_pointcloud_, reference_pointcloud_search_method_)) { return false; }
		reference_pointcloud_search_method_->setInputCloud(reference_pointcloud_); // update kdtree
	}

	if (!reference_pointcloud_->points.empty()) {
		reference_pointcloud_received_ = true;

		if (!reference_pointcloud_preprocessed_save_filename_.empty()) {
			ROS_DEBUG_STREAM("Saving reference pointcloud preprocessed with " << reference_pointcloud_->points.size() << " points to file " << reference_pointcloud_preprocessed_save_filename_);
			pcl::io::savePCDFile<PointT>(reference_pointcloud_preprocessed_save_filename_, *reference_pointcloud_, save_clouds_in_binary_format_);
		}

		typename pcl::PointCloud<PointT>::Ptr reference_pointcloud_keypoints(new pcl::PointCloud<PointT>());

		if (detect_keypoints_reference_cloud_) {
			if (reference_pointcloud_keypoints_filename_.empty() || pcl::io::loadPCDFile<PointT>(reference_pointcloud_keypoints_filename_, *reference_pointcloud_keypoints) != 0) {
				if (!applyKeypointDetection(reference_pointcloud_, reference_pointcloud_search_method_, reference_pointcloud_keypoints)) { return false; }

				if (!reference_pointcloud_keypoints_save_filename_.empty()) {
					ROS_DEBUG_STREAM("Saving reference pointcloud keypoints with " << reference_pointcloud_keypoints->points.size() << " points to file " << reference_pointcloud_keypoints_save_filename_);
					pcl::io::savePCDFile<PointT>(reference_pointcloud_keypoints_save_filename_, *reference_pointcloud_keypoints, save_clouds_in_binary_format_);
				}
			} else {
				ROS_DEBUG_STREAM("Loaded " << reference_pointcloud_keypoints->points.size() << " keypoints from file " << reference_pointcloud_keypoints_filename_);
			}
		}


		for (size_t i = 0; i < cloud_matchers_.size(); ++i) {
			cloud_matchers_[i]->setupReferenceCloud(reference_pointcloud_, reference_pointcloud_keypoints, reference_pointcloud_search_method_);
		}

		publishReferencePointCloud();
		return true;
	}

	return false;
}


template<typename PointT>
void Localization<PointT>::startLocalization() {
	// publishers
	reference_pointcloud_publisher_ = node_handle_->advertise<sensor_msgs::PointCloud2>(reference_pointcloud_publish_topic_, 2);
	aligned_pointcloud_publisher_ = node_handle_->advertise<sensor_msgs::PointCloud2>(aligned_pointcloud_publish_topic_, 5);
	pose_publisher_ = node_handle_->advertise<geometry_msgs::PoseWithCovarianceStamped>(pose_publish_topic_, 10);


	// subscribers
	ambient_pointcloud_subscriber_ = node_handle_->subscribe(ambient_pointcloud_topic_, 2, &dynamic_robot_localization::Localization<PointT>::processAmbientPointCloud, this);

	if (reference_pointcloud_filename_.empty()) {
		if (!reference_pointcloud_topic_.empty()) {
			reference_pointcloud_subscriber_ = node_handle_->subscribe(reference_pointcloud_topic_, 1, &dynamic_robot_localization::Localization<PointT>::loadReferencePointCloudFromROSPointCloud, this);
		} else {
			if (!costmap_topic_.empty())
				costmap_subscriber_ = node_handle_->subscribe(costmap_topic_, 1, &dynamic_robot_localization::Localization<PointT>::loadReferencePointCloudFromROSOccupancyGrid, this);
		}
	} else {
		loadReferencePointCloudFromFile(reference_pointcloud_filename_);
	}


	if (publish_tf_map_odom_) {
		ros::Rate publish_rate(pose_to_tf_publisher_.getPublishRate());
		while (ros::ok()) {
			pose_to_tf_publisher_.publishTFMapToOdom();
			publish_rate.sleep();
			ros::spinOnce();
		}
	} else {
		ros::spin();
	}
}


template<typename PointT>
void Localization<PointT>::processAmbientPointCloud(const sensor_msgs::PointCloud2ConstPtr& ambient_cloud_msg) {
	ros::Duration scan_age = ros::Time::now() - ambient_cloud_msg->header.stamp;
	ros::Duration elapsed_time_since_last_scan = ros::Time::now() - last_scan_time_;

	ROS_DEBUG_STREAM("Received pointcloud with " << (ambient_cloud_msg->width * ambient_cloud_msg->height) << " points and with time stamp " << ambient_cloud_msg->header.stamp);

	if (reference_pointcloud_received_
			&& ambient_cloud_msg->data.size() > 0
			&& elapsed_time_since_last_scan > min_seconds_between_scan_registration_
			&& scan_age < max_seconds_scan_age_) {

		last_scan_time_ = ros::Time::now();

		tf2::Transform pose_tf_initial_guess;
		if (!pose_to_tf_publisher_.getTfCollector().lookForTransform(pose_tf_initial_guess, ambient_cloud_msg->header.frame_id, base_link_frame_id_, ambient_cloud_msg->header.stamp)) {
			ROS_DEBUG_STREAM("Dropping pointcloud because tf between " << ambient_cloud_msg->header.frame_id << " and " << base_link_frame_id_ << " isn't available");
			return;
		}

		typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud(new pcl::PointCloud<PointT>());
		pcl::fromROSMsg(*ambient_cloud_msg, *ambient_pointcloud);
		if (reference_pointcloud_2d_) {
			resetPointCloudHeight(*ambient_pointcloud);
		}

		// >>>>> localization pipeline <<<<<
		tf2::Transform pose_tf_corrected;
		if (updateLocalizationWithAmbientPointCloud(ambient_pointcloud, pose_tf_initial_guess, pose_tf_corrected)) {
			geometry_msgs::PoseWithCovarianceStampedPtr pose_corrected_msg(new geometry_msgs::PoseWithCovarianceStamped());
			pose_corrected_msg->header.frame_id = ambient_cloud_msg->header.frame_id;

			if (ignore_height_corrections_) {
				pose_tf_corrected.getOrigin().setZ(pose_tf_initial_guess.getOrigin().getZ());
			}

			if (add_odometry_displacement_) {
				pose_corrected_msg->header.stamp = ros::Time::now();
				pose_to_tf_publisher_.addOdometryDisplacementToTransform(pose_tf_corrected, ambient_cloud_msg->header.stamp, pose_corrected_msg->header.stamp);
			} else {
				pose_corrected_msg->header.stamp = ambient_cloud_msg->header.stamp;
			}

			if (publish_tf_map_odom_)
				pose_to_tf_publisher_.publishTFMapToOdom(pose_tf_corrected, ambient_cloud_msg->header.stamp);

			laserscan_to_pointcloud::tf_rosmsg_eigen_conversions::transformTF2ToMsg(pose_tf_corrected, pose_corrected_msg->pose.pose);
			// todo: fill covariance
			// pose->pose.covariance

			if (pose_publisher_.getNumSubscribers() > 0) {
				pose_publisher_.publish(pose_corrected_msg);
			}

			if (!aligned_pointcloud_publish_topic_.empty() && aligned_pointcloud_publisher_.getNumSubscribers() > 0) {
				sensor_msgs::PointCloud2Ptr aligned_pointcloud_msg(new sensor_msgs::PointCloud2());
				pcl::toROSMsg(*ambient_pointcloud, *aligned_pointcloud_msg);
				aligned_pointcloud_publisher_.publish(aligned_pointcloud_msg);
			}
		} else {
			ROS_DEBUG_STREAM("Discarded cloud because localization couldn't be calculated");
		}
	} else {
		if (!reference_pointcloud_received_) {
			ROS_DEBUG_STREAM("Discarded cloud because there is no reference cloud to compare to");
		} else {
			ROS_DEBUG_STREAM("Discarded cloud with age " << scan_age.toSec());
		}
	}
}


template<typename PointT>
void Localization<PointT>::resetPointCloudHeight(pcl::PointCloud<PointT>& pointcloud, float height) {
	for (size_t i = 0; i < pointcloud.points.size(); ++i) {
		pointcloud.points[i].z = height;
	}
}


template<typename PointT>
bool Localization<PointT>::applyFilters(typename pcl::PointCloud<PointT>::Ptr& pointcloud) {
	for (size_t i = 0; i < cloud_filters_.size(); ++i) {
		typename pcl::PointCloud<PointT>::Ptr filtered_ambient_pointcloud(new pcl::PointCloud<PointT>());
		cloud_filters_[i]->filter(pointcloud, filtered_ambient_pointcloud);
		pointcloud = filtered_ambient_pointcloud; // switch pointers
	}

	return !pointcloud->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyNormalEstimation(typename pcl::PointCloud<PointT>::Ptr& pointcloud, typename pcl::search::KdTree<PointT>::Ptr& surface_search_method) {
	tf2::Transform sensor_pose_tf_guess;
	if (!pose_to_tf_publisher_.getTfCollector().lookForTransform(sensor_pose_tf_guess, pointcloud->header.frame_id, sensor_frame_id_, pcl_conversions::fromPCL(pointcloud->header).stamp)) {
		sensor_pose_tf_guess.setIdentity();
	}/* else {
		pointcloud->sensor_origin_(0) = sensor_pose_tf_guess.getOrigin().getX();
		pointcloud->sensor_origin_(1) = sensor_pose_tf_guess.getOrigin().getY();
		pointcloud->sensor_origin_(2) = sensor_pose_tf_guess.getOrigin().getZ();
	}*/
	typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_with_normals(new pcl::PointCloud<PointT>());
	normal_estimator_->estimateNormals(pointcloud, pointcloud, surface_search_method, sensor_pose_tf_guess, ambient_pointcloud_with_normals);
	pointcloud = ambient_pointcloud_with_normals; // switch pointers

	return !pointcloud->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyKeypointDetection(typename pcl::PointCloud<PointT>::Ptr& pointcloud, typename pcl::search::KdTree<PointT>::Ptr& surface_search_method, typename pcl::PointCloud<PointT>::Ptr& keypoints) {
	for (size_t i = 0; i < keypoint_detectors_.size(); ++i) {
		if (i == 0) {
			keypoint_detectors_[i]->findKeypoints(pointcloud, keypoints, pointcloud, surface_search_method);
		} else {
			typename pcl::PointCloud<PointT>::Ptr keypoints_temp(new pcl::PointCloud<PointT>());
			keypoint_detectors_[i]->findKeypoints(pointcloud, keypoints_temp, pointcloud, surface_search_method);
			*keypoints += *keypoints_temp;
		}
	}

	return !keypoints->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyCloudRegistration(typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud,
		typename pcl::search::KdTree<PointT>::Ptr& surface_search_method,
		typename pcl::PointCloud<PointT>::Ptr& pointcloud_keypoints,
		tf2::Transform& pointcloud_pose_in_out) {

	if (ambient_pointcloud->points.empty()) { return false; }

	bool registration_successful = false;
	for (size_t i = 0; i < cloud_matchers_.size(); ++i) {
		typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_aligned(new pcl::PointCloud<PointT>());
		if (cloud_matchers_[i]->registerCloud(ambient_pointcloud, surface_search_method, pointcloud_keypoints, pointcloud_pose_in_out, ambient_pointcloud_aligned, false)) {
			registration_successful = true;
			ambient_pointcloud = ambient_pointcloud_aligned; // switch pointers
		}
	}

	return registration_successful;
}


template<typename PointT>
double Localization<PointT>::applyOutlierDetection(typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud) {
	double max_outlier_percentage = 0.0;
	for (size_t i = 0; i < outlier_detectors_.size(); ++i) {
		sensor_msgs::PointCloud2Ptr outliers = outlier_detectors_[i]->processOutliers(reference_pointcloud_search_method_, *ambient_pointcloud);

		double number_ambient_pointcloud_points = (double) (ambient_pointcloud->points.size());
		double outlier_percentage = 1.0;
		if (number_ambient_pointcloud_points > 0) {
			outlier_percentage = ((double) ((outliers->width * outliers->height))) / number_ambient_pointcloud_points;
			max_outlier_percentage = std::max(max_outlier_percentage, outlier_percentage);
		}
		outliers_detected_.push_back(std::pair<sensor_msgs::PointCloud2Ptr, double>(outliers, outlier_percentage));
	}

	return max_outlier_percentage;
}


template<typename PointT>
void Localization<PointT>::publishDetectedOutliers() {
	for (size_t i = 0; i < outliers_detected_.size(); ++i) {
		if (outliers_detected_[i].second < max_outliers_percentage_) {
			outlier_detectors_[i]->publishOutliers(outliers_detected_[i].first);
		}
	}

	outliers_detected_.clear();
}


template<typename PointT>
bool Localization<PointT>::applyTransformationValidators(const tf2::Transform& pointcloud_pose_initial_guess, tf2::Transform& pointcloud_pose_corrected_in_out, double max_outlier_percentage) {
	for (size_t i = 0; i < transformation_validators_.size(); ++i) {
		if (!transformation_validators_[i]->validateNewLocalizationPose(pointcloud_pose_initial_guess, pointcloud_pose_corrected_in_out,
				cloud_matchers_.back()->getCloudMatcher()->getFitnessScore(), max_outlier_percentage)) {
			return false;
		}
	}

	return true;
}


template<typename PointT>
bool Localization<PointT>::updateLocalizationWithAmbientPointCloud(typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud, const tf2::Transform& pointcloud_pose_initial_guess, tf2::Transform& pointcloud_pose_corrected_out) {
	pointcloud_pose_corrected_out = pointcloud_pose_initial_guess;
	if (ambient_pointcloud->points.empty()) {
		return false;
	}

	// ==============================================================  filters
	if (!applyFilters(ambient_pointcloud)) { return false; }

	// ==============================================================  normal estimation
	typename pcl::search::KdTree<PointT>::Ptr ambient_search_method(new pcl::search::KdTree<PointT>());
	ambient_search_method->setInputCloud(ambient_pointcloud);
	if (compute_normals_ambient_cloud_) {
		if (!applyNormalEstimation(ambient_pointcloud, ambient_search_method)) { return false; }
		ambient_search_method->setInputCloud(ambient_pointcloud);
	}

	// ==============================================================  keypoint selection
	typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_keypoints(new pcl::PointCloud<PointT>());
	if (detect_keypoints_ambient_cloud_) {
		if (!applyKeypointDetection(ambient_pointcloud, ambient_search_method, ambient_pointcloud_keypoints)) { return false; }
	}

	// ==============================================================  cloud registration
	if (!applyCloudRegistration(ambient_pointcloud, ambient_search_method, detect_keypoints_ambient_cloud_ ? ambient_pointcloud_keypoints : ambient_pointcloud, pointcloud_pose_corrected_out)) { return false; }

	// ==============================================================  outlier detection
	double max_outlier_percentage = applyOutlierDetection(ambient_pointcloud);

	// ==============================================================  localization post processors
	if (!applyTransformationValidators(pointcloud_pose_initial_guess, pointcloud_pose_corrected_out, max_outlier_percentage)) { return false; }
	publishDetectedOutliers();

	// ==============================================================  publish localization statistics

	return true;
}
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </Localization-functions>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// =============================================================================  </public-section>  ===========================================================================



// =============================================================================   <protected-section>   =======================================================================
// =============================================================================   </protected-section>  =======================================================================



// =============================================================================   <private-section>   =========================================================================
// =============================================================================   </private-section>  =========================================================================

} /* namespace dynamic_robot_localization */
