#include "../../include/interface/CheatIO.h"

#include <unistd.h>

#include <csignal>
#include <iostream>

#include "../../include/interface/KeyBoard.h"
#include "ros/node_handle.h"
#include "std_msgs/Float32MultiArray.h"

inline void RosShutDown(int sig) {
  ROS_INFO("ROS interface shutting down!");
  ros::shutdown();
}

CheatIO::CheatIO(std::string robot_name,ros::NodeHandle n) : _nm(n), IOInterface() {
  // int argc; char **argv;
  // ros::init(argc, argv, "unitree_gazebo_servo");
  std::cout << "The control interface for ROS Gazebo simulation with cheat states from gazebo" << std::endl;
  _robot_name = robot_name;

  // start subscriber
  initRecv();
  // ros::AsyncSpinner subSpinner(1);  // one threads
  // subSpinner.start();
  usleep(3000);  // wait for subscribers start
  // initialize publisher
  initSend();

  // signal(SIGINT, RosShutDown);

  cmdPanel = new KeyBoard();

  // * Set subscriber and publisher for mujoco communication
  jointsTorquePub_ = _nm.advertise<std_msgs::Float32MultiArray>("/jointsTorque", 1);
  joinsPosVelSub_ = _nm.subscribe("/jointsPosVel", 1, &CheatIO::jointsPosVelCallback, this);
  bodyPoseSub_ = _nm.subscribe("/bodyPose", 1, &CheatIO::bodyPoseCallback, this);
  bodyTwistSub_ = _nm.subscribe("/bodyTwist",1,&CheatIO::bodyTwistCallback,this);
}

CheatIO::~CheatIO() { ros::shutdown(); }

void CheatIO::sendRecv(const std::shared_ptr<LowlevelCmd> cmd, LowlevelState* state) {
  // sendCmd(cmd);
  std::cout<<"send and receive once "<<std::endl;
  sendCmdMj(cmd);
  recvState(state);
  cmdPanel->updateVelCmd(state);

  state->userCmd = cmdPanel->getUserCmd();
  state->userValue = cmdPanel->getUserValue();
}

void CheatIO::sendCmdMj(const std::shared_ptr<LowlevelCmd> cmd) {
  std_msgs::Float32MultiArray torqueMsg;
  torqueMsg.data.resize(10);
  for (int i = 0; i < 10; i++) {
    float q = cmd->motorCmd[i].q;
    float dq = cmd->motorCmd[i].dq;
    float kp = cmd->motorCmd[i].Kp;
    float kd = cmd->motorCmd[i].Kd;
    float tau = cmd->motorCmd[i].tau;
    float q_m = _highState.motorState[i].q;
    float dq_m = _highState.motorState[i].dq; 
    // kp kd 都是0
    // std::cout<<"kp:"<<kp<<"  kd:"<<kd<<std::endl;
    // torqueMsg.data[i] = -kp*(q-q_m)-kd*(dq-dq_m)+tau;
    torqueMsg.data[i] = tau;
  }
  // std::cout<<"----------"<<std::endl;
  jointsTorquePub_.publish(torqueMsg);
  ros::spinOnce();
}

void CheatIO::jointsPosVelCallback(const std_msgs::Float32MultiArray::ConstPtr& msg) {
  std::cout<<"joints position and velocidy callback"<<std::endl;
  for (int i = 0; i < 10; i++) {
    _highState.motorState[i].q = msg->data[i];
    _highState.motorState[i].dq = msg->data[i + 10];
  }
}
void CheatIO::bodyPoseCallback(const geometry_msgs::Pose::ConstPtr& msg) {
  std::cout<<"body position and orientation callback"<<std::endl;
  _highState.position[0] = msg->position.x;
  _highState.position[1] = msg->position.y;
  _highState.position[2] = msg->position.z;
  _highState.imu.quaternion[0] = msg->orientation.w;
  _highState.imu.quaternion[1] = msg->orientation.x;
  _highState.imu.quaternion[2] = msg->orientation.y;
  _highState.imu.quaternion[3] = msg->orientation.z;
}
void CheatIO::bodyTwistCallback(const geometry_msgs::TwistConstPtr& msg) {
  std::cout<<"body twist callback"<<std::endl;
  _highState.velocity[0] = msg->linear.x;
  _highState.velocity[1] = msg->linear.y;
  _highState.velocity[2] = msg->linear.z;
  _highState.imu.gyroscope[0] = msg->angular.x;
  _highState.imu.gyroscope[1] = msg->angular.y;
  _highState.imu.gyroscope[2] = msg->angular.z;
}

void CheatIO::sendCmd(const std::shared_ptr<LowlevelCmd> cmd) {
  for (int i = 0; i < 10; i++) {
    _lowCmd.motorCmd[i].mode = 0X0A;  // alwasy set it to 0X0A
    _lowCmd.motorCmd[i].q = cmd->motorCmd[i].q;
    _lowCmd.motorCmd[i].dq = cmd->motorCmd[i].dq;
    _lowCmd.motorCmd[i].tau = cmd->motorCmd[i].tau;
    _lowCmd.motorCmd[i].Kd = cmd->motorCmd[i].Kd;
    _lowCmd.motorCmd[i].Kp = cmd->motorCmd[i].Kp;
  }
  for (int m = 0; m < 10; m++) {
    _servo_pub[m].publish(_lowCmd.motorCmd[m]);
  }

  ros::spinOnce();
}

void CheatIO::recvState(LowlevelState* state) {
  for (int i = 0; i < 10; i++) {
    state->motorState[i].q = _highState.motorState[i].q;
    state->motorState[i].dq = _highState.motorState[i].dq;
    state->motorState[i].tauEst = _highState.motorState[i].tauEst;
  }
  for (int i = 0; i < 3; i++) {
    state->imu.quaternion[i] = _highState.imu.quaternion[i];
    state->imu.gyroscope[i] = _highState.imu.gyroscope[i];
    state->position[i] = _highState.position[i];
    state->vWorld[i] = _highState.velocity[i];
  }
  state->imu.quaternion[3] = _highState.imu.quaternion[3];
}

void CheatIO::initSend() {
  _servo_pub[0] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/L_hip_controller/command", 1);
  _servo_pub[1] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/L_hip2_controller/command", 1);
  _servo_pub[2] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/L_thigh_controller/command", 1);
  _servo_pub[3] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/L_calf_controller/command", 1);
  _servo_pub[4] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/L_toe_controller/command", 1);
  _servo_pub[5] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/R_hip_controller/command", 1);
  _servo_pub[6] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/R_hip2_controller/command", 1);
  _servo_pub[7] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/R_thigh_controller/command", 1);
  _servo_pub[8] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/R_calf_controller/command", 1);
  _servo_pub[9] =
      _nm.advertise<unitree_legged_msgs::MotorCmd>("/" + _robot_name + "_gazebo/R_toe_controller/command", 1);
}

void CheatIO::initRecv() {
  _state_sub = _nm.subscribe("/gazebo/model_states", 1, &CheatIO::StateCallback, this);
  _servo_sub[0] = _nm.subscribe("/" + _robot_name + "_gazebo/L_hip_controller/state", 1, &CheatIO::LhipCallback, this);
  _servo_sub[1] =
      _nm.subscribe("/" + _robot_name + "_gazebo/L_hip2_controller/state", 1, &CheatIO::Lhip2Callback, this);
  _servo_sub[2] =
      _nm.subscribe("/" + _robot_name + "_gazebo/L_thigh_controller/state", 1, &CheatIO::LthighCallback, this);
  _servo_sub[3] =
      _nm.subscribe("/" + _robot_name + "_gazebo/L_calf_controller/state", 1, &CheatIO::LcalfCallback, this);
  _servo_sub[4] = _nm.subscribe("/" + _robot_name + "_gazebo/L_toe_controller/state", 1, &CheatIO::LtoeCallback, this);
  _servo_sub[5] = _nm.subscribe("/" + _robot_name + "_gazebo/R_hip_controller/state", 1, &CheatIO::RhipCallback, this);
  _servo_sub[6] =
      _nm.subscribe("/" + _robot_name + "_gazebo/R_hip2_controller/state", 1, &CheatIO::Rhip2Callback, this);
  _servo_sub[7] =
      _nm.subscribe("/" + _robot_name + "_gazebo/R_thigh_controller/state", 1, &CheatIO::RthighCallback, this);
  _servo_sub[8] =
      _nm.subscribe("/" + _robot_name + "_gazebo/R_calf_controller/state", 1, &CheatIO::RcalfCallback, this);
  _servo_sub[9] = _nm.subscribe("/" + _robot_name + "_gazebo/R_toe_controller/state", 1, &CheatIO::RtoeCallback, this);
}

void CheatIO::StateCallback(const gazebo_msgs::ModelStates& msg) {
  std::cout<<"gazebo receive state"<<std::endl;
  int robot_index;
  // std::cout << msg.name.size() << std::endl;
  for (int i = 0; i < msg.name.size(); i++) {
    if (msg.name[i] == _robot_name + "_gazebo") {
      robot_index = i;
    }
  }

  _highState.position[0] = msg.pose[robot_index].position.x;
  _highState.position[1] = msg.pose[robot_index].position.y;
  _highState.position[2] = msg.pose[robot_index].position.z;

  _highState.velocity[0] = msg.twist[robot_index].linear.x;
  _highState.velocity[1] = msg.twist[robot_index].linear.y;
  _highState.velocity[2] = msg.twist[robot_index].linear.z;

  _highState.imu.quaternion[0] = msg.pose[robot_index].orientation.w;
  _highState.imu.quaternion[1] = msg.pose[robot_index].orientation.x;
  _highState.imu.quaternion[2] = msg.pose[robot_index].orientation.y;
  _highState.imu.quaternion[3] = msg.pose[robot_index].orientation.z;

  _highState.imu.gyroscope[0] = msg.twist[robot_index].angular.x;
  _highState.imu.gyroscope[1] = msg.twist[robot_index].angular.y;
  _highState.imu.gyroscope[2] = msg.twist[robot_index].angular.z;
}

void CheatIO::LhipCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[0].mode = msg.mode;
  _highState.motorState[0].q = msg.q;
  _highState.motorState[0].dq = msg.dq;
  _highState.motorState[0].tauEst = msg.tauEst;
}

void CheatIO::Lhip2Callback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[1].mode = msg.mode;
  _highState.motorState[1].q = msg.q;
  _highState.motorState[1].dq = msg.dq;
  _highState.motorState[1].tauEst = msg.tauEst;
}

void CheatIO::LthighCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[2].mode = msg.mode;
  _highState.motorState[2].q = msg.q;
  _highState.motorState[2].dq = msg.dq;
  _highState.motorState[2].tauEst = msg.tauEst;
}

void CheatIO::LcalfCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[3].mode = msg.mode;
  _highState.motorState[3].q = msg.q;
  _highState.motorState[3].dq = msg.dq;
  _highState.motorState[3].tauEst = msg.tauEst;
}

void CheatIO::LtoeCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[4].mode = msg.mode;
  _highState.motorState[4].q = msg.q;
  _highState.motorState[4].dq = msg.dq;
  _highState.motorState[4].tauEst = msg.tauEst;
}

void CheatIO::RhipCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[5].mode = msg.mode;
  _highState.motorState[5].q = msg.q;
  _highState.motorState[5].dq = msg.dq;
  _highState.motorState[5].tauEst = msg.tauEst;
}

void CheatIO::Rhip2Callback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[6].mode = msg.mode;
  _highState.motorState[6].q = msg.q;
  _highState.motorState[6].dq = msg.dq;
  _highState.motorState[6].tauEst = msg.tauEst;
}

void CheatIO::RthighCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[7].mode = msg.mode;
  _highState.motorState[7].q = msg.q;
  _highState.motorState[7].dq = msg.dq;
  _highState.motorState[7].tauEst = msg.tauEst;
}

void CheatIO::RcalfCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[8].mode = msg.mode;
  _highState.motorState[8].q = msg.q;
  _highState.motorState[8].dq = msg.dq;
  _highState.motorState[8].tauEst = msg.tauEst;
}

void CheatIO::RtoeCallback(const unitree_legged_msgs::MotorState& msg) {
  _highState.motorState[9].mode = msg.mode;
  _highState.motorState[9].q = msg.q;
  _highState.motorState[9].dq = msg.dq;
  _highState.motorState[9].tauEst = msg.tauEst;
}