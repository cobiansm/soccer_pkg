/* Author: Marlene Cobian 
 Maintainer: Pedro Deniz */

#include <std_srvs/Empty.h>
#include <chrono>
#include <thread>

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Bool.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/Imu.h>
#include <geometry_msgs/Point.h>
#include <iostream>
#include <fstream>
#include <eigen3/Eigen/Eigen>

#include "robotis_controller_msgs/SetModule.h"
#include "robotis_controller_msgs/SyncWriteItem.h"
#include "robotis_math/robotis_linear_algebra.h"
#include "op3_action_module_msgs/IsRunning.h"
#include "op3_walking_module_msgs/GetWalkingParam.h"
#include "op3_walking_module_msgs/WalkingParam.h"
//#include "op3_online_walkin_module_msgs/GetJointPose.h"

void readyToDemo();
void setModule(const std::string& module_name);
bool checkManagerRunning(std::string& manager_name);
void torqueOnAll();

void goInitPose();
void goAction(int page);
void turn2search();
void goWalk(std::string& command);

bool isActionRunning();
bool getWalkingParam();
bool getJointPose();
void waitFollowing();

void calcFootstep(double target_distance, double target_angle, double delta_time, double& fb_move, double& rl_angle);
void setWalkingParam(double x_move, double y_move, double rotation_angle, bool balance = true);
bool processFollowing();
void waitFollowing();
void do_walking();

void callbackImu(const sensor_msgs::Imu::ConstPtr& msg);
void callbackPosition(const geometry_msgs::Point& msg);
void callbackJointStates(const sensor_msgs::JointState& msg);
void callbackError(const geometry_msgs::Point& msg);
void callbackTurn(const std_msgs::Bool& msg);
void callbackfindBall(const std_msgs::Bool& msg);


double rest_inc = 0.2181;
//rest_inc =0.2618 15°
double rest_inc_giro = 0.08726;

double alpha = 0.4;
double pitch;

double rpy_orientation;
const double FALL_FORWARD_LIMIT = 55;
const double FALL_BACK_LIMIT = -55;
double present_pitch_;
double head_pan;
double head_tilt;
double positionx;
double positiony;
double errorx;
double errory;
int page;
int state;
bool end_walking = true;

std_msgs::String scan;

const int row = 5700;
const int col = 14;
float posiciones[row][col];

const int row2 = 40;
const int col2 = 6;
float posiciones2[row2][col2];

const double SPOT_FB_OFFSET = 0.0;
const double SPOT_RL_OFFSET = 0.0;
const double SPOT_ANGLE_OFFSET = 0.0;
double current_x_move_ = 0.005;
double current_r_angle_ = 0.0;
const double IN_PLACE_FB_STEP = -0.003;
const int NOT_FOUND_THRESHOLD = 0.5;
int count_not_found_ = 0;
int count_to_kick_ = 0;
const double CAMERA_HEIGHT = 0.46;
const double FOV_WIDTH = 35.2 * M_PI / 180;
const double FOV_HEIGHT = 21.6 * M_PI / 180;
const double hip_pitch_offset_ = 0.12217305;  //7°
const double UNIT_FB_STEP = 0.002;
const double UNIT_RL_TURN = 0.00872665;  //0.5°
const double MAX_FB_STEP = 0.007;
const double MAX_RL_TURN =  0.26179939;  //15°
const double MIN_FB_STEP = 0.003;
const double MIN_RL_TURN = 0.08726646;  //5°
double accum_period_time = 0.0;
double current_period_time = 0.6;

enum ControlModule
{
  None = 0,
  DirectControlModule = 1,
  Framework = 2,
};

const int SPIN_RATE = 30;
const bool DEBUG_PRINT = false;

ros::Publisher init_pose_pub;
ros::Publisher dxl_torque_pub;
ros::Publisher write_head_joint_pub;
ros::Publisher write_joint_pub;
ros::Publisher vision_case_pub;
ros::Publisher action_pose_pub;
ros::Publisher walk_command_pub;
ros::Publisher set_walking_param_pub;
ros::Publisher reset_body_pub;
ros::Publisher scan_command;

ros::Subscriber read_joint_sub;
ros::Subscriber imu_sub;
ros::Subscriber position_sub;
ros::Subscriber error_sub;
ros::Subscriber find_ball_sub;
ros::Subscriber turnNsearch_sub;

ros::ServiceClient set_joint_module_client;
ros::ServiceClient is_running_client;
ros::ServiceClient get_param_client;
//ros::ServiceClient get_joint_client;

op3_walking_module_msgs::WalkingParam current_walking_param;

int control_module = None;
bool demo_ready = false;

bool ball = false;
bool turnNsearch = false;

//node main
int main(int argc, char **argv)
{
  
  //init ros
  ros::init(argc, argv, "read_write");
  ros::NodeHandle nh(ros::this_node::getName());

  //subscribers
  read_joint_sub = nh.subscribe("/robotis/present_joint_states",1, callbackJointStates);
  position_sub = nh.subscribe("/position", 5, callbackPosition);
  error_sub = nh.subscribe("/error", 5, callbackError);
  //ros::Subscriber error_sub = nh.subscribe("/error", 5, callbackError);
  //ros::Subscriber position_sub = nh.subscribe("/position", 5, callbackPosition);
  //ros::Subscriber joint_error_sub = nh.subscribe("/robotis/present_joint_states", 5, CallBack);
  //ros::Subscriber find_ball_sub = nh.subscribe("/find_ball", 5, findballCallBack);
  imu_sub = nh.subscribe("/robotis/open_cr/imu", 1, callbackImu);
  find_ball_sub = nh.subscribe("/find_ball", 5, callbackfindBall);
  turnNsearch_sub = nh.subscribe("/turnNsearch", 5, callbackTurn);
  
  std::string command;
  
  std::ifstream myfile ("/home/robotis/blenders_ws/src/soccer_pkg/data/Pararse.txt");
  if (myfile.is_open())
  {
	std::cout << "El archivo se abrió";
	
		for (int idx2 = 0; idx2 < row2; idx2++){
			for (int idy2 = 0; idy2 < col2; idy2++){
				myfile >> posiciones2[idx2][idy2];
			}
			
		}
		myfile.close();
  } else {
  	std::cout << "El archivo no abrió";
  }
  
  //publishers
  init_pose_pub = nh.advertise<std_msgs::String>("/robotis/base/ini_pose", 0);
  dxl_torque_pub = nh.advertise<std_msgs::String>("/robotis/dxl_torque", 0);
  write_head_joint_pub = nh.advertise<sensor_msgs::JointState>("/robotis/head_control/set_joint_states", 0);
  write_joint_pub = nh.advertise<sensor_msgs::JointState>("/robotis/set_joint_states", 0);
  vision_case_pub = nh.advertise<std_msgs::Bool>("/vision_case", 1000);
  action_pose_pub = nh.advertise<std_msgs::Int32>("/robotis/action/page_num", 0);
  walk_command_pub = nh.advertise<std_msgs::String>("/robotis/walking/command", 0);
  set_walking_param_pub = nh.advertise<op3_walking_module_msgs::WalkingParam>("/robotis/walking/set_params", 0);
  reset_body_pub = nh.advertise<std_msgs::Bool>("/robotis/online_walking/reset_body", 0);
  scan_command = nh.advertise<std_msgs::String>("/robotis/head_control/scan_command", 0);

  //services
  set_joint_module_client = nh.serviceClient<robotis_controller_msgs::SetModule>("/robotis/set_present_ctrl_modules");
  is_running_client = nh.serviceClient<op3_action_module_msgs::IsRunning>("/robotis/action/is_running");
  get_param_client = nh.serviceClient<op3_walking_module_msgs::GetWalkingParam>("/robotis/walking/get_params");
  //get_joint_client = nh.serviceClient<op3_online_walking_module_msgs::GetJointPose>("robotis/online_walking/get_joint_pose");

  ros::start();

  //set node loop rate
  ros::Rate loop_rate(SPIN_RATE);

  //wait for starting of op3_manager
  std::string manager_name = "/op3_manager";
  while (ros::ok())
  {
    ros::Duration(1.0).sleep();

    if (checkManagerRunning(manager_name) == true)
    {
      break;
      ROS_INFO_COND(DEBUG_PRINT, "Succeed to connect");
    }
    ROS_WARN("Waiting for op3 manager");
  }

  readyToDemo();

  //node loop
  sensor_msgs::JointState write_msg;
  write_msg.header.stamp = ros::Time::now();
  
  //pararse en posición para caminar
  ros::Duration(1).sleep();
  ros::Rate loop_rate_pararse(60);
  
  /*for (int fila2=0; fila2<row2; fila2++)
  {
    write_msg.name.push_back("r_ank_pitch");
    write_msg.position.push_back(posiciones2[fila2][0]);
    write_msg.name.push_back("r_knee");
    write_msg.position.push_back(posiciones2[fila2][1]);
    write_msg.name.push_back("r_hip_pitch");
    write_msg.position.push_back(posiciones2[fila2][2] + rest_inc);
    write_msg.name.push_back("l_ank_pitch");
    write_msg.position.push_back(posiciones2[fila2][3]);
    write_msg.name.push_back("l_knee");
    write_msg.position.push_back(posiciones2[fila2][4]);
    write_msg.name.push_back("l_hip_pitch");
    write_msg.position.push_back(posiciones2[fila2][5] - rest_inc);
    write_joint_pub.publish(write_msg);
      
    loop_rate_pararse.sleep();
  }
  
 //////////////////////////////////////////////// acomodo de pies ////////////////////////////////////////////////
  ros::Duration(1).sleep();
  write_msg.name.push_back("l_hip_yaw");
  write_msg.position.push_back(-0.0873);
  write_msg.name.push_back("r_hip_yaw");
  write_msg.position.push_back(0.0873);
  write_joint_pub.publish(write_msg);
    
  ros::Duration(1).sleep();
  write_msg.name.push_back("l_hip_roll");
  write_msg.position.push_back(-0.0873);
  write_msg.name.push_back("r_hip_roll");
  write_msg.position.push_back(0.0873);
  write_msg.name.push_back("l_ank_roll");
  write_msg.position.push_back(-0.0873);
  write_msg.name.push_back("r_ank_roll");
  write_msg.position.push_back(0.0873);
  write_joint_pub.publish(write_msg);

 //////////////////////////////////////////////// loop ////////////////////////////////////////////////

  while (ros::ok())
  {
	ros::spinOnce();
	ros::Rate loop_rate(SPIN_RATE);
	
	setModule("head_control_module");

        write_msg.name.push_back("head_pan");
        write_msg.position.push_back(positionx);
        write_msg.name.push_back("head_tilt");
        write_msg.position.push_back(positiony);
        write_head_joint_pub.publish(write_msg);

	//std::string command = "start";
	//goWalk(command);
	/*getWalkingParam();
	setWalkingParam(0.02, 0, -0.0872664600611, true);
	std_msgs::String command_msg;
	command_msg.data = "start";
	walk_command_pub.publish(command_msg);
	
	ros::Duration(3.0).sleep();
	command = "stop";
	goWalk(command);
	ros::Duration(3.0).sleep();
  }*/
  
  goAction(9);
  ros::Time prev_time_ = ros::Time::now();
  
  while (ros::ok()){
    ros::Rate loop_rate(SPIN_RATE);
    ros::spinOnce();

    /*if (ball){
      turn2search()
    }*/
    
    
    

    /*if (!ball){
      setModule("none");
      write_msg.name.push_back("head_pan");
      write_msg.position.push_back(positionx);
      write_msg.name.push_back("head_tilt");
      write_msg.position.push_back(positiony);
      write_joint_pub.publish(write_msg);
    }else{
      setModule("head_control_module");
      write_msg.name.push_back("head_pan");
      write_msg.position.push_back(positionx);
      write_msg.name.push_back("head_tilt");
      write_msg.position.push_back(positiony);
      write_head_joint_pub.publish(write_msg);
    }*/
      setModule("head_control_module");
      write_msg.name.push_back("head_pan");
      write_msg.position.push_back(positionx);
      write_msg.name.push_back("head_tilt");
      write_msg.position.push_back(positiony);
      write_head_joint_pub.publish(write_msg);

    if (turnNsearch){
      turn2search();
    }else{
      std::cout << "NOOOOOOOOOOOO GIRAAAA" << std::endl;
    }
    //errorx = 0;
    //errory = 0;
        
	  setModule("walking_module");
      std::cout << errorx << std::endl;
        
    if ((errorx > -8 && errorx < 8) && (errory > -8 && errory < 8) && (errorx != 0) && (errory != 0) && end_walking == true){
      end_walking = false;
      do_walking();
	  	
		}/*else{
		  std::string command = "stop";
		  goWalk(command);
	    ros::Duration(1.5).sleep();
		}*/
	}
	return 0;
}

void do_walking(){
  ros::Time curr_time = ros::Time::now();
  ros::Duration dur = curr_time - prev_time_;
  double delta_time = dur.nsec * 0.000000001 + dur.sec;
  prev_time_ = curr_time;

  count_not_found_ = 0;

  double distance_to_ball = CAMERA_HEIGHT * tan(M_PI * 0.5 + head_tilt - hip_pitch_offset_);

  if (distance_to_ball < 0)
    distance_to_ball *= (-1);

  double distance_to_kick = 0.22;
  std::cout << distance_to_ball << std::endl;
  if ((distance_to_ball < distance_to_kick) ){ //&& (fabs(ball_x_angle) < 25.0) to kick
    count_to_kick_ += 1;	
    std::cout << count_to_kick_ << std::endl;
    if (count_to_kick_ > 20){
      std::string command = "stop";
      goWalk(command);
      if (head_pan > 0){ //left
        std::cout << "PATEA DERECHA" << std::endl;
        goAction(84); //left kick
      }
      else{ //right
        std::cout << "PATEA IZQUIERDA" << std::endl;
        goAction(83); //right kick
      }
    }
    else if (count_to_kick_ > 15){
      getWalkingParam();
      setWalkingParam(IN_PLACE_FB_STEP, 0, 0, true);

      std_msgs::String command_msg;
      command_msg.data = "start";
      walk_command_pub.publish(command_msg);
    }
  }else{
    count_to_kick_ = 0;
  }

  double fb_move = 0.0, rl_angle = 0.0;
  double distance_to_walk = distance_to_ball - distance_to_kick;

  calcFootstep(distance_to_walk, head_pan, delta_time, fb_move, rl_angle);

  getWalkingParam();
  setWalkingParam(fb_move, 0, rl_angle, true);
  
  std_msgs::String command_msg;
  command_msg.data = "start";
  walk_command_pub.publish(command_msg);

  end_walking = true;
}

void readyToDemo()
{
  ROS_INFO("Start read-write demo");
  torqueOnAll();
  ROS_INFO("Torque on all joints");

  //send message for going init posture
  goInitPose();
  ROS_INFO("Go init pose");

  //wait while ROBOTIS-OP3 goes to the init posture.
  ros::Duration(4.0).sleep();

  setModule("none");
}

void goInitPose()
{
  std_msgs::String init_msg;
  init_msg.data = "ini_pose";
  init_pose_pub.publish(init_msg);
}

void turn2search(){
  //node loop
  sensor_msgs::JointState write_msg;
  write_msg.header.stamp = ros::Time::now();
  
  for (int i = 1; i <= 6; i++)
  {
    setModule("none");
    std::cout  << "Derechaaaa D:" << std::endl;
    //Levantar pie derecho 
    ros::Duration(0.1).sleep();
    write_msg.name.push_back("r_ank_pitch");
    write_msg.position.push_back(-0.7091);
    write_msg.name.push_back("r_knee");
    write_msg.position.push_back(-1.4131);
    write_msg.name.push_back("r_hip_pitch");
    write_msg.position.push_back(0.7091 + rest_inc_giro);
    write_msg.name.push_back("r_hip_yaw");
    write_msg.position.push_back(0.1746*1.5);
    write_msg.name.push_back("l_hip_yaw");
    write_msg.position.push_back(-0.1746*1.5);

    write_msg.name.push_back("l_hip_roll");
    write_msg.position.push_back(-0.0873);
    write_msg.name.push_back("r_hip_roll");
    write_msg.position.push_back(0.0873);
    write_msg.name.push_back("l_ank_roll");
    write_msg.position.push_back(-0.0873);
    write_msg.name.push_back("r_ank_roll");
    write_msg.position.push_back(0.0873);
    write_joint_pub.publish(write_msg);

    //Bajar pie derecho
    ros::Duration(0.1).sleep();
    write_msg.name.push_back("r_ank_pitch");
    write_msg.position.push_back(posiciones2[row2-1][0]);
    write_msg.name.push_back("r_knee");
    write_msg.position.push_back(posiciones2[row2-1][1]);
    write_msg.name.push_back("r_hip_pitch");
    write_msg.position.push_back(posiciones2[row2-1][2] + rest_inc_giro);
    write_joint_pub.publish(write_msg);
    
    //Levantar pie izquierdo
    ros::Duration(0.1).sleep();
    write_msg.name.push_back("l_ank_pitch");
    write_msg.position.push_back(0.7091);
    write_msg.name.push_back("l_knee");
    write_msg.position.push_back(1.4131);
    write_msg.name.push_back("l_hip_pitch");
    write_msg.position.push_back(-0.7091 - rest_inc_giro);
    write_msg.name.push_back("r_hip_yaw");
    write_msg.position.push_back(0);
    write_msg.name.push_back("l_hip_yaw");
    write_msg.position.push_back(0);
    write_joint_pub.publish(write_msg);
    
    //Bajar pie izquierdo
    ros::Duration(0.1).sleep();
    write_msg.name.push_back("l_ank_pitch");
    write_msg.position.push_back(posiciones2[row2-1][3]);
    write_msg.name.push_back("l_knee");
    write_msg.position.push_back(posiciones2[row2-1][4]);
    write_msg.name.push_back("l_hip_pitch");
    write_msg.position.push_back(posiciones2[row2-1][5] - rest_inc_giro);
    write_joint_pub.publish(write_msg);
  }
}

void goAction(int page) 
{
  setModule("action_module");
  ROS_INFO("Action pose");

  std_msgs::Int32 action_msg;
  action_msg.data = page;
  action_pose_pub.publish(action_msg);
}

void goWalk(std::string& command) 
{
  setModule("walking_module");
  if (command == "start") {
    getWalkingParam();
    setWalkingParam(IN_PLACE_FB_STEP, 0, 0, true);
  }

  std_msgs::String command_msg;
  command_msg.data = command;
  walk_command_pub.publish(command_msg);
}

void waitFollowing() 
{
  count_not_found_++;

  if (count_not_found_ > NOT_FOUND_THRESHOLD * 0.5)
    setWalkingParam(0.0, 0.0, 0.0);
}

void calcFootstep(double target_distance, double target_angle, double delta_time, double& fb_move, double& rl_angle) 
{
  double next_movement = current_x_move_;
  if (target_distance < 0)
    target_distance = 0.0;

  double fb_goal = fmin(target_distance * 0.1, MAX_FB_STEP);
  accum_period_time += delta_time;
  if (accum_period_time > (current_period_time  / 4))
  {
    accum_period_time = 0.0;
    if ((target_distance * 0.1 / 2) < current_x_move_)
      next_movement -= UNIT_FB_STEP;
    else
      next_movement += UNIT_FB_STEP;
  }
  fb_goal = fmin(next_movement, fb_goal);
  fb_move = fmax(fb_goal, MIN_FB_STEP);

  double rl_goal = 0.0;
  if (fabs(target_angle) * 180 / M_PI > 5.0)
  {
    double rl_offset = fabs(target_angle) * 0.2;
    rl_goal = fmin(rl_offset, MAX_RL_TURN);
    rl_goal = fmax(rl_goal, MIN_RL_TURN);
    rl_angle = fmin(fabs(current_r_angle_) + UNIT_RL_TURN, rl_goal);

    if (target_angle < 0)
      rl_angle *= (-1);
  }
}

bool checkManagerRunning(std::string& manager_name) 
{
  std::vector<std::string> node_list;
  ros::master::getNodes(node_list);

  for (unsigned int node_list_idx = 0; node_list_idx < node_list.size(); node_list_idx++)
  {
    if (node_list[node_list_idx] == manager_name)
      return true;
  }
  ROS_ERROR("Can't find op3_manager");
  return false;
}

void setModule(const std::string& module_name) 
{
  robotis_controller_msgs::SetModule set_module_srv;
  set_module_srv.request.module_name = module_name;

  if (set_joint_module_client.call(set_module_srv) == false)
  {
    ROS_ERROR("Failed to set module");
    return;
  }
  return ;
}

void torqueOnAll() 
{
  std_msgs::String check_msg;
  check_msg.data = "check";
  dxl_torque_pub.publish(check_msg);
}

bool isActionRunning() 
{
  op3_action_module_msgs::IsRunning is_running_srv;

  if (is_running_client.call(is_running_srv) == false) {
    ROS_ERROR("Failed to start action module");
    return true;
  } else {
    if (is_running_srv.response.is_running == true) {
      return true;
    }
  }
  return false;
}

void setWalkingParam(double x_move, double y_move, double rotation_angle, bool balance)
{
  current_walking_param.balance_enable = balance;
  current_walking_param.x_move_amplitude = x_move + SPOT_FB_OFFSET;
  current_walking_param.y_move_amplitude = y_move + SPOT_RL_OFFSET;
  current_walking_param.angle_move_amplitude = rotation_angle + SPOT_ANGLE_OFFSET;

  set_walking_param_pub.publish(current_walking_param);

  current_x_move_ = x_move;
  current_r_angle_ = rotation_angle;
}

bool getWalkingParam() 
{
  
  op3_walking_module_msgs::GetWalkingParam walking_param_msg;

  if (get_param_client.call(walking_param_msg))
  {
    current_walking_param = walking_param_msg.response.parameters;

    // update ui
    ROS_INFO_COND(DEBUG_PRINT, "Get walking parameters");

    return true;
  }
  else
  {
    ROS_ERROR("Fail to get walking parameters.");

    return false;
  }
}

void callbackJointStates(const sensor_msgs::JointState& msg)
{ 
  head_pan = msg.position[0];
  head_tilt = msg.position[1];
  return;
}

void callbackPosition(const geometry_msgs::Point& msg)
{
  positionx = msg.x;
  positiony = msg.y;
}

void callbackError(const geometry_msgs::Point& msg)
{
  errorx = msg.x;
  errory = msg.y;
}

void callbackImu(const sensor_msgs::Imu::ConstPtr& msg) 
{
  Eigen::Quaterniond orientation(msg->orientation.w, msg->orientation.x, msg->orientation.y, msg->orientation.z);
  Eigen::MatrixXd rpy_orientation = robotis_framework::convertQuaternionToRPY(orientation);
  rpy_orientation *= (180 / 3.141516);

  double pitch = rpy_orientation.coeff(1, 0);

  double alpha = 0.4;
  if (present_pitch_ == 0) 
    present_pitch_ = pitch;
  else
    present_pitch_ = present_pitch_ * (1 - alpha) + pitch * alpha;

  if (present_pitch_ > FALL_FORWARD_LIMIT) {
    goAction(122);
    setModule("none");
  } else if (present_pitch_ < FALL_BACK_LIMIT) {
    goAction(1);
    setModule("none");
    ros::Duration(1).sleep();
    goAction(82);
    setModule("none");
  } else {
    state = 0;
  }
}

void callbackfindBall(const std_msgs::Bool& msg)
{
  ball = msg.data;
}

void callbackTurn(const std_msgs::Bool& msg)
{
  turnNsearch = msg.data;
}
