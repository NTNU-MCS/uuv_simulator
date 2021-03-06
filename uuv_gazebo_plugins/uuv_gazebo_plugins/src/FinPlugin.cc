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

#include <uuv_gazebo_plugins/FinPlugin.hh>
#include <uuv_gazebo_plugins/Def.hh>

#include <gazebo/gazebo.hh>
#include <gazebo/physics/Link.hh>
#include <gazebo/physics/Model.hh>
#include <gazebo/physics/World.hh>


GZ_REGISTER_MODEL_PLUGIN(gazebo::FinPlugin)

namespace gazebo {

/////////////////////////////////////////////////
FinPlugin::FinPlugin() : inputCommand(0), angle(0)
{
}

/////////////////////////////////////////////////
FinPlugin::~FinPlugin()
{
  if (this->updateConnection)
  {
    event::Events::DisconnectWorldUpdateBegin(this->updateConnection);
  }
}

/////////////////////////////////////////////////
void FinPlugin::Load(physics::ModelPtr _model,
                          sdf::ElementPtr _sdf)
{
  // Initializing the transport node
  this->node = transport::NodePtr(new transport::Node());
  this->node->Init(_model->GetWorld()->GetName());

  // Input/output topics
  GZ_ASSERT(_sdf->HasElement("input_topic"), "Could not find input_topic.");
  std::string input_topic = _sdf->Get<std::string>("input_topic");
  GZ_ASSERT(_sdf->HasElement("output_topic"), "Could not find output_topic.");
  std::string output_topic = _sdf->Get<std::string>("output_topic");

  GZ_ASSERT(_sdf->HasElement("link_name"), "Could not find link_name.");
  std::string link_name = _sdf->Get<std::string>("link_name");
  this->link = _model->GetLink(link_name);
  GZ_ASSERT(this->link, "link is invalid");

  GZ_ASSERT(_sdf->HasElement("joint_name"), "Could not find joint_name.");
  std::string joint_name = _sdf->Get<std::string>("joint_name");
  this->joint = _model->GetJoint(joint_name);
  GZ_ASSERT(this->joint, "joint is invalid");

  // Dynamic model
  GZ_ASSERT(_sdf->HasElement("dynamics"), "Could not find dynamics.");
  this->dynamics.reset(DynamicsFactory::GetInstance().CreateDynamics(
                         _sdf->GetElement("dynamics")));

  // Lift&drag model
  GZ_ASSERT(_sdf->HasElement("liftdrag"), "Could not find liftdrag");
  this->liftdrag.reset(
        LiftDragFactory::GetInstance().CreateLiftDrag(
          _sdf->GetElement("liftdrag")));

  // Advertise the output topic
  this->anglePublisher = this->node->Advertise<
      uuv_gazebo_plugins_msgs::msgs::Double>(output_topic);


  // Subscribe to the input signal topic
  this->commandSubscriber = this->node->Subscribe(input_topic,
                                                &FinPlugin::UpdateInput,
                                                this);

  // Connect the update event
  this->updateConnection = event::Events::ConnectWorldUpdateBegin(
        boost::bind(&FinPlugin::OnUpdate,
                    this, _1));
}

/////////////////////////////////////////////////
void FinPlugin::Init()
{
}

/////////////////////////////////////////////////
void FinPlugin::OnUpdate(const common::UpdateInfo &_info)
{
  GZ_ASSERT(!std::isnan(this->inputCommand),
            "nan in this->inputCommand");

  // Update dynamics model:
  double angle = this->dynamics->update(this->inputCommand,
                                        _info.simTime.Double());

  // Determine velocity in lift/drag plane:
  math::Pose finPose = this->link->GetWorldPose();
  math::Vector3 ldNormalI = finPose.rot.RotateVector(math::Vector3::UnitZ);

  math::Vector3 velI = this->link->GetWorldLinearVel();
  math::Vector3 velInLDPlaneI = ldNormalI.Cross(velI.Cross(ldNormalI));
  math::Vector3 velInLDPlaneL = finPose.rot.RotateVectorReverse(velInLDPlaneI);

  // Compute lift and drag forces:
  math::Vector3 FFin = this->liftdrag->compute(velInLDPlaneL);

  // Apply forces at cg (with torques for position shift).
  this->link->AddRelativeForce(FFin);

  // Apply new fin angle. Do this last since this sets link's velocity to zero.
  this->joint->SetPosition(0, angle);
}

/////////////////////////////////////////////////
void FinPlugin::UpdateInput(ConstDoublePtr &_msg)
{
  this->inputCommand = _msg->value();
}
}
