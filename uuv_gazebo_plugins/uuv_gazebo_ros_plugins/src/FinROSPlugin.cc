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

#include <uuv_gazebo_ros_plugins/FinROSPlugin.hh>

#include <string>

#include <gazebo/physics/Base.hh>
#include <gazebo/physics/Link.hh>
#include <gazebo/physics/Model.hh>
#include <gazebo/physics/World.hh>

namespace uuv_simulator_ros
{
FinROSPlugin::FinROSPlugin()
{
  this->rosPublishPeriod = gazebo::common::Time(0.05);
  this->lastRosPublishTime = gazebo::common::Time(0.0);
}

FinROSPlugin::~FinROSPlugin()
{
  gazebo::event::Events::DisconnectWorldUpdateBegin(
        this->rosPublishConnection);
  this->rosNode->shutdown();
}

void FinROSPlugin::SetReference(
    const uuv_gazebo_ros_plugins_msgs::FloatStamped::ConstPtr &_msg)
{
  if (std::isnan(_msg->data))
  {
    ROS_WARN("FinROSPlugin: Ignoring nan command");
    return;
  }

  this->inputCommand = _msg->data;
}

gazebo::common::Time FinROSPlugin::GetRosPublishPeriod()
{
  return this->rosPublishPeriod;
}

void FinROSPlugin::SetRosPublishRate(double _hz)
{
  if (_hz > 0.0)
    this->rosPublishPeriod = 1.0 / _hz;
  else
    this->rosPublishPeriod = 0.;
}

void FinROSPlugin::Init()
{
  FinPlugin::Init();
}

void FinROSPlugin::Reset()
{
  this->lastRosPublishTime.Set(0, 0);
}

void FinROSPlugin::Load(gazebo::physics::ModelPtr _parent, sdf::ElementPtr _sdf)
{
  try {
    FinPlugin::Load(_parent, _sdf);
  } catch(gazebo::common::Exception &_e)
  {
    gzerr << "Error loading plugin."
          << "Please ensure that your model is correct."
          << '\n';
    return;
  }

  if (!ros::isInitialized())
  {
    gzerr << "Not loading plugin since ROS has not been "
          << "properly initialized.  Try starting gazebo with ros plugin:\n"
          << "  gazebo -s libgazebo_ros_api_plugin.so\n";
    return;
  }

  this->rosNode.reset(new ros::NodeHandle(""));

  this->subReference = this->rosNode->subscribe<
    uuv_gazebo_ros_plugins_msgs::FloatStamped
    >(this->commandSubscriber->GetTopic(), 10,
      boost::bind(&FinROSPlugin::SetReference, this, _1));

  this->pubState = this->rosNode->advertise<
    uuv_gazebo_ros_plugins_msgs::FloatStamped
    >(this->anglePublisher->GetTopic(), 10);

  this->rosPublishConnection = gazebo::event::Events::ConnectWorldUpdateBegin(
        boost::bind(&FinROSPlugin::RosPublishStates, this));
}

void FinROSPlugin::RosPublishStates()
{
  // Limit publish rate according to publish period
  if (this->angleStamp - this->lastRosPublishTime >=
      this->rosPublishPeriod)
  {
    this->lastRosPublishTime = this->angleStamp;

    uuv_gazebo_ros_plugins_msgs::FloatStamped state_msg;
    state_msg.header.stamp = ros::Time().now();
    state_msg.header.frame_id = this->link->GetName();
    state_msg.data = this->angle;
    this->pubState.publish(state_msg);
  }
}

GZ_REGISTER_MODEL_PLUGIN(FinROSPlugin)
}
