// Copyright (c) 2016 The UUV Simulator Authors.
// All rights reserved.
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

/// \file UnderwaterObjectROSPlugin.hh Publishes the underwater object's
/// Gazebo topics and parameters into ROS standards

#ifndef __UNDERWATER_OBJECT_ROS_PLUGIN_HH__
#define __UNDERWATER_OBJECT_ROS_PLUGIN_HH__

#include <string>

#include <uuv_gazebo_plugins/UnderwaterObjectPlugin.hh>

#include <boost/scoped_ptr.hpp>
#include <gazebo/gazebo.hh>
#include <gazebo/common/Plugin.hh>
#include <ros/ros.h>
#include <geometry_msgs/WrenchStamped.h>

#include <map>

namespace uuv_simulator_ros
{
  class UnderwaterObjectROSPlugin : public gazebo::UnderwaterObjectPlugin
  {
    /// \brief Constructor
    public: UnderwaterObjectROSPlugin();

    /// \brief Destructor
    public: virtual ~UnderwaterObjectROSPlugin();

    /// \brief Load module and read parameters from SDF.
    public: void Load(gazebo::physics::ModelPtr _parent, sdf::ElementPtr _sdf);

    /// \brief Initialize Module.
    public: virtual void Init();

    /// \brief Reset Module.
    public: virtual void Reset();

    /// \brief Publish restoring force
    /// \param[in] _link Pointer to the link where the force information will
    /// be extracted from
    protected: virtual void PublishRestoringForce(
      gazebo::physics::LinkPtr _link);

    /// \brief Publish hydrodynamic wrenches
    /// \param[in] _link Pointer to the link where the force information will
    /// be extracted from
    protected: virtual void PublishHydrodynamicWrenches(
      gazebo::physics::LinkPtr _link);

    /// \brief Returns the wrench message for debugging topics
    /// \param[in] _force Force vector
    /// \param[in] _torque Torque vector
    /// \param[in] _output Stamped wrench message to be updated
    protected: virtual void GenWrenchMsg(
      gazebo::math::Vector3 _force, gazebo::math::Vector3 _torque,
      geometry_msgs::WrenchStamped &_output);

    /// \brief Sets the topics used for publishing the intermediate data during
    /// the simulation
    /// \param[in] _link Pointer to the link
    /// \param[in] _hydro Pointer to the hydrodynamic model
    protected: virtual void InitDebug(gazebo::physics::LinkPtr _link,
      gazebo::HydrodynamicModelPtr _hydro);

    /// \brief Pointer to this ROS node's handle.
    private: boost::scoped_ptr<ros::NodeHandle> rosNode;

    /// \brief Subscriber reacting to new reference thrust set points.
    private: ros::Subscriber subThrustReference;

    /// \brief Publisher for current actual thrust.
    private: std::map<std::string, ros::Publisher> rosHydroPub;
  };
}

#endif  // __UNDERWATER_OBJECT_ROS_PLUGIN_HH__
