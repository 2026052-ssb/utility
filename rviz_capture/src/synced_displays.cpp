#include "rviz_capture/synced_displays.hpp"
#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz_capture::SyncedImage, rviz_common::Display)
PLUGINLIB_EXPORT_CLASS(rviz_capture::SyncedPointCloud2, rviz_common::Display)
PLUGINLIB_EXPORT_CLASS(rviz_capture::SyncedMarkerArray, rviz_common::Display)
