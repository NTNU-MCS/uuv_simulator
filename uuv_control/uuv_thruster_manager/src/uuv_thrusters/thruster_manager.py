#!/usr/bin/env python
# Copyright (c) 2016 The UUV Simulator Authors.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import numpy
import rospy
import tf
import tf.transformations as trans
from os.path import isdir, join
import yaml

from models import Thruster
from uuv_gazebo_ros_plugins_msgs.msg import FloatStamped
from geometry_msgs.msg import Wrench


class ThrusterManager:
    """
    The thruster manager generates the thruster allocation matrix using the
    TF information and publishes the thruster forces assuming the the thruster
    topics are named in the following pattern

    <thruster_topic_prefix>/<index>/<thruster_topic_suffix>

    Thruster frames should also be named as follows

    <thruster_frame_base>_<index>
    """

    MAX_THRUSTERS = 16

    def __init__(self):
        """Class constructor."""
        # This flag will be set to true once the thruster allocation matrix is
        # available
        self._ready = False

        # Acquiring the namespace of the vehicle
        self.namespace = rospy.get_namespace()
        if self.namespace[-1] != '/':
            self.namespace += '/'

        if self.namespace[0] != '/':
            self.namespace = '/' + self.namespace

        if not rospy.has_param('thruster_manager'):
            raise rospy.ROSException('Thruster manager parameters not '
                                     'initialized for uuv_name=' +
                                     self.namespace)

        # Load all parameters
        self.config = rospy.get_param('thruster_manager')

        if self.config['update_rate'] < 0:
            self.config['update_rate'] = 50

        self.listener = tf.TransformListener()

        rospy.loginfo(
          'ThrusterManager::update_rate=' + str(self.config['update_rate']))

        # Set the tf_prefix parameter
        rospy.set_param('thruster_manager/tf_prefix', self.namespace)

        # Retrieve the output file path to store the TAM
        # matrix for future use
        self.output_dir = None
        if rospy.has_param('~output_dir'):
            self.output_dir = rospy.get_param('~output_dir')
            if not isdir(self.output_dir):
                raise rospy.ROSException(
                    'Invalid output directory, output_dir=' + self.output_dir)
            print 'output_dir=', self.output_dir

        # Number of thrusters
        self.n_thrusters = 0

        # Thruster objects used to calculate the right angular velocity command
        self.thrusters = list()

        # Thrust forces vector
        self.thrust = None

        # Thruster allocation matrix: transform thruster inputs to force/torque
        self.configuration_matrix = None
        if rospy.has_param('~tam'):
            tam = rospy.get_param('~tam')
            self.configuration_matrix = numpy.array(tam)
            # Set number of thrusters from the number of columns
            self.n_thrusters = self.configuration_matrix.shape[1]
            # Create publishing topics to each thruster
            for i in range(self.n_thrusters):
                topic = self.config['thruster_topic_prefix'] + str(i) + \
                    self.config['thruster_topic_suffix']
                # TODO Read the option of type of thruster conversion function
                # and create thruster model accordingly
                params = self.config['conversion_fcn_params']
                thruster = Thruster.create_thruster(
                    self.config['conversion_fcn'], i, topic, None, None,
                    **params)
                if thruster is None:
                    rospy.ROSException('Invalid thruster conversion '
                                       'function=%s'
                                       % self.config['conversion_fcn'])
                self.thrusters.append(thruster)
            print 'Thruster allocation matrix provided!'
            print 'TAM='
            print self.configuration_matrix
            self.thrust = numpy.zeros(self.n_thrusters)

        if not self.update_tam():
            raise rospy.ROSException('No thrusters found')

        # (pseudo) inverse: force/torque to thruster inputs
        self.inverse_configuration_matrix = None
        if self.configuration_matrix is not None:
            self.inverse_configuration_matrix = numpy.linalg.pinv(
                self.configuration_matrix)

        # If an output directory was provided, store matrix for further use
        if self.output_dir is not None:
            with open(join(self.output_dir, 'TAM.yaml'), 'w') as yaml_file:
                yaml_file.write(
                    yaml.safe_dump(
                        dict(tam=self.configuration_matrix.tolist())))

        self.ready = True
        print ('ThrusterManager: ready')

    def update_tam(self, recalculate=False):
        """Calculate the thruster allocation matrix, if one is not given."""
        if self.configuration_matrix is not None and not recalculate:
            self.ready = True
            print 'TAM provided, skipping...'
            print ('ThrusterManager: ready')
            return True

        self.ready = False
        print('ThrusterManager: updating thruster poses')
        # Small margin to make sure we get thruster frames via tf
        now = rospy.Time.now() + rospy.Duration(1.0)

        base = self.namespace + self.config['base_link']

        self.thrusters = list()

        for i in range(self.MAX_THRUSTERS):
            frame = self.namespace + \
                self.config['thruster_frame_base'] + str(i)
            try:
                # try to get thruster pose with respect to base frame via tf
                print('transform: ' + base + ' -> ' + frame)
                self.listener.waitForTransform(base, frame,
                                               now, rospy.Duration(4.0))
                [pos, quat] = self.listener.lookupTransform(base, frame, now)

                topic = self.config['thruster_topic_prefix'] + str(i) + \
                    self.config['thruster_topic_suffix']
                params = self.config['conversion_fcn_params']
                thruster = Thruster.create_thruster(
                    self.config['conversion_fcn'], i, topic, pos, quat,
                    **params)
                if thruster is None:
                    rospy.ROSException('Invalid thruster conversion '
                                       'function=%s'
                                       % self.config['conversion_fcn'])
                self.thrusters.append(thruster)
            except tf.Exception:
                print('could not get transform from: ' + base)
                print('to: ' + frame)
                break

        print self.thrusters
        if len(self.thrusters) == 0:
            return False

        # Set the number of thrusters found
        self.n_thrusters = len(self.thrusters)

        # Fill the thrust vector
        self.thrust = numpy.zeros(self.n_thrusters)

        # Fill the thruster allocation matrix
        self.configuration_matrix = numpy.zeros((6, self.n_thrusters))

        for i in range(self.n_thrusters):
            self.configuration_matrix[:, i] = self.thrusters[i].tam_column

        # Eliminate small values
        self.configuration_matrix[numpy.abs(
            self.configuration_matrix) < 1e-3] = 0.0

        print 'TAM='
        print self.configuration_matrix

        # Once we know the configuration matrix we can compute its
        # (pseudo-)inverse:
        self.inverse_configuration_matrix = numpy.linalg.pinv(
            self.configuration_matrix)

        # If an output directory was provided, store matrix for further use
        if self.output_dir is not None and not recalculate:
            with open(join(self.output_dir, 'TAM.yaml'), 'w') as yaml_file:
                yaml_file.write(
                    yaml.safe_dump(
                        dict(tam=self.configuration_matrix.tolist())))

        self.ready = True
        print ('ThrusterManager: ready')
        return True

    def command_thrusters(self):
        """Publish the thruster input into their specific topic."""
        if self.thrust is None:
            return
        for i in range(self.n_thrusters):
            self.thrusters[i].publish_command(self.thrust[i])

    def publish_thrust_forces(self, control_forces, control_torques):
        if not self.ready:
            return
        gen_forces = numpy.hstack(
            (control_forces, control_torques)).transpose()
        self.thrust = self.compute_thruster_forces(gen_forces)
        self.command_thrusters()

    def compute_thruster_forces(self, gen_forces):
        """Compute desired thruster forces using the inverse configuration
        matrix.
        """
        # Calculate individual thrust forces
        thrust = self.inverse_configuration_matrix.dot(gen_forces)
        # Obey limit on max thrust by applying a constant scaling factor to all
        # thrust forces
        limitation_factor = 1.0
        for t in thrust:
            if abs(t) > self.config['max_thrust']:
                limitation_factor = min(limitation_factor,
                                        self.config['max_thrust'] / abs(t))

        thrust = thrust * limitation_factor
        return thrust
