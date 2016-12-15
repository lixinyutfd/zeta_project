// straddle_block_client: 
// wsn, December, 2016
//test client to position gripper at perceived grasp pose--then hover there

#include<ros/ros.h>
#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <coordinator/ManipTaskAction.h>
#include <object_manipulation_properties/object_ID_codes.h>
//#include <object_manipulation_properties/object_manipulation_properties.h>
#include <object_finder/objectFinderAction.h>
#include <object_grabber/object_grabberAction.h>
bool g_goal_done = true;
int g_callback_status = coordinator::ManipTaskResult::PENDING;
int g_object_grabber_return_code = 0;
int g_object_finder_return_code = 0;
int g_fdbk_count = 0;

geometry_msgs::PoseStamped g_des_flange_pose_stamped_wrt_torso;
geometry_msgs::PoseStamped g_object_pose;
coordinator::ManipTaskResult g_result;

using namespace std;

void doneCb(const actionlib::SimpleClientGoalState& state,
        const coordinator::ManipTaskResultConstPtr& result) {
    ROS_INFO(" doneCb: server responded with state [%s]", state.toString().c_str());
    g_goal_done = true;
    g_result = *result;
    g_callback_status = result->manip_return_code;

    switch (g_callback_status) {
        case coordinator::ManipTaskResult::MANIP_SUCCESS:
            ROS_INFO("returned MANIP_SUCCESS");
            break;
        case coordinator::ManipTaskResult::FAILED_PERCEPTION:
            ROS_WARN("returned FAILED_PERCEPTION");
            g_object_finder_return_code = result->object_finder_return_code;
            break;
        case coordinator::ManipTaskResult::FAILED_PICKUP:
            ROS_WARN("returned FAILED_PICKUP");
            g_object_grabber_return_code = result->object_grabber_return_code;
            g_object_pose = result->object_pose;
            //g_des_flange_pose_stamped_wrt_torso = result->des_flange_pose_stamped_wrt_torso;
            break;
        case coordinator::ManipTaskResult::FAILED_DROPOFF:
            ROS_WARN("returned FAILED_DROPOFF");
            //g_des_flange_pose_stamped_wrt_torso = result->des_flange_pose_stamped_wrt_torso;          
            break;
    }
}

//optional feedback; output has been suppressed (commented out) below

void feedbackCb(const coordinator::ManipTaskFeedbackConstPtr& fdbk_msg) {
    g_fdbk_count++;
    if (g_fdbk_count > 1000) { //slow down the feedback publications
        g_fdbk_count = 0;
        //suppress this feedback output
        //ROS_INFO("feedback status = %d", fdbk_msg->feedback_status);
    }
    //g_fdbk = fdbk_msg->feedback_status; //make status available to "main()"
}

// Called once when the goal becomes active; not necessary, but possibly useful for diagnostics

void activeCb() {
    ROS_INFO("Goal just went active");
}

int main(int argc, char** argv) {
    ros::init(argc, argv, "task_client_node"); // name this node 
    ros::NodeHandle nh; //standard ros node handle  

    coordinator::ManipTaskGoal goal;

    actionlib::SimpleActionClient<coordinator::ManipTaskAction> action_client("manip_task_action_service", true);

    // attempt to connect to the server:
    ROS_INFO("waiting for server: ");
    bool server_exists = false;
    while ((!server_exists)&&(ros::ok())) {
        server_exists = action_client.waitForServer(ros::Duration(0.5)); // 
        ros::spinOnce();
        ros::Duration(0.5).sleep();
        ROS_INFO("retrying...");
    }
    ROS_INFO("connected to action server"); // if here, then we connected to the server;

    ROS_INFO("sending a goal: move to pre-pose");
    g_goal_done = false;
    goal.action_code = coordinator::ManipTaskGoal::MOVE_TO_PRE_POSE;
    action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
    while (!g_goal_done) {
        ros::Duration(0.1).sleep();
    }
    if (g_callback_status != coordinator::ManipTaskResult::MANIP_SUCCESS) {
        ROS_ERROR("failed to move quitting");
        return 0;
    }
    //send vision request to find table top:
    ROS_INFO("sending a goal: seeking table top");
    g_goal_done = false;
    goal.action_code = coordinator::ManipTaskGoal::FIND_TABLE_SURFACE;

    action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
    while (!g_goal_done) {
        ros::Duration(0.1).sleep();
    }

    int ans = 1;
    //define a dropoff pose; 
    geometry_msgs::PoseStamped desired_pose;

    while (true) {
        ROS_INFO("ready to look for block; enter 1 when you are ready, or 0 to stop: ");

        std::cin>>ans;
        if (ans == 0) return 0;

        //send vision goal to find block:
	// look for the toy block first i.e. the red rectangular block
        ROS_INFO("sending a goal: find block");
        g_goal_done = false;
        goal.action_code = coordinator::ManipTaskGoal::GET_PICKUP_POSE;
        goal.object_code = ObjectIdCodes::TOY_BLOCK_ID;
        goal.perception_source = coordinator::ManipTaskGoal::PCL_VISION;
        action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
        while (!g_goal_done) {
            ros::Duration(0.1).sleep();
        }
        if (g_callback_status != coordinator::ManipTaskResult::MANIP_SUCCESS) {
            ROS_ERROR("failed to find block");
	// if the toy block is not found now look for the other block
	// Should be if statement that looks for the other block and then if that isn't found repeat the process until told to stop looking for 
	
        } else { //here if found block



            g_object_pose = g_result.object_pose;
            ROS_INFO_STREAM("object pose w/rt frame-id " << g_object_pose.header.frame_id << endl);
            ROS_INFO_STREAM("object origin: (x,y,z) = (" << g_object_pose.pose.position.x << ", " << g_object_pose.pose.position.y << ", "
                    << g_object_pose.pose.position.z << ")" << endl);
            ROS_INFO_STREAM("orientation: (qx,qy,qz,qw) = (" << g_object_pose.pose.orientation.x << ","
                    << g_object_pose.pose.orientation.y << ","
                    << g_object_pose.pose.orientation.z << ","
                    << g_object_pose.pose.orientation.w << ")" << endl);


            //send command to acquire block:
            ROS_INFO("sending a goal: straddle block");
            g_goal_done = false;
            goal.action_code = coordinator::ManipTaskGoal::STRADDLE_OBJECT;
            goal.pickup_frame = g_result.object_pose;
            goal.object_code = ObjectIdCodes::TOY_BLOCK_ID;
            action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
            while (!g_goal_done) {
                ros::Duration(0.1).sleep();
            }
            if (g_callback_status != coordinator::ManipTaskResult::MANIP_SUCCESS) {
                ROS_ERROR("failed to straddle block; ");
            }
	    
	    // The block is curently being straddled and now we need to try to sweep it off
	    else {

		// create pose for the arm to move to, to try to push the block off
		//define a dropoff pose; We want to move the block to the left as far as we can go and we want to move in the direction of most contact so we will want to define an eliptical trajectory if possible for first test just move left in the direction of surface, will need to make a new solution for when the block is directly horizontal

		
    		desired_pose.header.frame_id = g_object_pose.header.frame_id;
    		desired_pose.pose.position.x = g_object_pose.pose.position.x;
    		desired_pose.pose.position.y = .1; //All the way off the table
    		desired_pose.pose.position.z = g_object_pose.pose.position.z + 0.03;
    		desired_pose.pose.orientation.x = 0.707;
    		desired_pose.pose.orientation.y = 0.707;
    		desired_pose.pose.orientation.z = 0;
    		desired_pose.pose.orientation.w = 0;
    		desired_pose.header.stamp = ros::Time::now(); 


		// adding if statement to move the hand to the left or to the right

		//if (g_result.object_id == 0){
		//	desired_pose.pose.position.y = -.1; //All the way off the table
		//}
	
		//send command to move gripper to destination pose: 
            	ROS_INFO("sending a goal: move gripper to hard-coded destination");
            	g_goal_done = false;
            	goal.action_code = coordinator::ManipTaskGoal::CART_MOVE_TO_GRIPPER_POSE;
            	goal.gripper_goal_frame = desired_pose;
            	action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
            	while (!g_goal_done) {
                	ros::Duration(0.1).sleep();
            	}
            	if (g_callback_status != coordinator::ManipTaskResult::MANIP_SUCCESS) {
                	ROS_ERROR("failed to perform Cartesian move; ");
            	}


	    }

	    // Now that the arm is over the block we want to move the robot arm to try to sweep the block off of the desk
	    // Do this by calling the cart move to gripper pose
        }

        //ROS_INFO("enter 1 to return to pre-pose: ");
        //std::cin>>ans;

        ROS_INFO("sending a goal: move to pre-pose");
        g_goal_done = false;
        goal.action_code = coordinator::ManipTaskGoal::MOVE_TO_PRE_POSE;
        action_client.sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
        while (!g_goal_done) {
            ros::Duration(0.1).sleep();
        }
        if (g_callback_status != coordinator::ManipTaskResult::MANIP_SUCCESS) {
            ROS_ERROR("failed to move to pre-pose; quitting");
            return 0;
        }
    }
}

