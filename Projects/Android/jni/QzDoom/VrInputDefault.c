/************************************************************************************

Filename	:	VrInputDefault.c
Content		:	Handles default controller input
Created		:	August 2019
Authors		:	Simon Brown

*************************************************************************************/

#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_SystemUtils.h>
#include <VrApi_Input.h>
#include <VrApi_Types.h>
#include <android/keycodes.h>

#include "VrInput.h"

#include "doomkeys.h"

int getGameState();
int isMenuActive();
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);

void HandleInput_Default( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )

{
    //Menu button - invoke menu
    handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, KEY_ESCAPE);

    if (getGameState() != 0 || isMenuActive()) //gamestate != GS_LEVEL
    {
        Joy_GenerateButtonEvents((pOffTrackedRemoteOld->Joystick.x > 0.7f ? 1 : 0), (pOffTrackedRemoteNew->Joystick.x > 0.7f ? 1 : 0), 1, KEY_PAD_DPAD_RIGHT);
        Joy_GenerateButtonEvents((pDominantTrackedRemoteOld->Joystick.x > 0.7f ? 1 : 0), (pDominantTrackedRemoteNew->Joystick.x > 0.7f ? 1 : 0), 1, KEY_PAD_DPAD_RIGHT);

        Joy_GenerateButtonEvents((pOffTrackedRemoteOld->Joystick.x < -0.7f ? 1 : 0), (pOffTrackedRemoteNew->Joystick.x < -0.7f ? 1 : 0), 1, KEY_PAD_DPAD_LEFT);
        Joy_GenerateButtonEvents((pDominantTrackedRemoteOld->Joystick.x < -0.7f ? 1 : 0), (pDominantTrackedRemoteNew->Joystick.x < -0.7f ? 1 : 0), 1, KEY_PAD_DPAD_LEFT);

        Joy_GenerateButtonEvents((pOffTrackedRemoteOld->Joystick.y < -0.7f ? 1 : 0), (pOffTrackedRemoteNew->Joystick.y < -0.7f ? 1 : 0), 1, KEY_PAD_DPAD_DOWN);
        Joy_GenerateButtonEvents((pDominantTrackedRemoteOld->Joystick.y < -0.7f ? 1 : 0), (pDominantTrackedRemoteNew->Joystick.y < -0.7f ? 1 : 0), 1, KEY_PAD_DPAD_DOWN);

        Joy_GenerateButtonEvents((pOffTrackedRemoteOld->Joystick.y > 0.7f ? 1 : 0), (pOffTrackedRemoteNew->Joystick.y > 0.7f ? 1 : 0), 1, KEY_PAD_DPAD_UP);
        Joy_GenerateButtonEvents((pDominantTrackedRemoteOld->Joystick.y > 0.7f ? 1 : 0), (pDominantTrackedRemoteNew->Joystick.y > 0.7f ? 1 : 0), 1, KEY_PAD_DPAD_UP);

        handleTrackedControllerButton(pDominantTrackedRemoteNew, pDominantTrackedRemoteOld, domButton1, KEY_PAD_A);
        handleTrackedControllerButton(pDominantTrackedRemoteNew, pDominantTrackedRemoteOld, ovrButton_Trigger, KEY_PAD_A);
        handleTrackedControllerButton(pDominantTrackedRemoteNew, pDominantTrackedRemoteOld, domButton2, KEY_PAD_B);

        handleTrackedControllerButton(pOffTrackedRemoteNew, pOffTrackedRemoteOld, domButton1, KEY_PAD_A);
        handleTrackedControllerButton(pOffTrackedRemoteNew, pOffTrackedRemoteOld, ovrButton_Trigger, KEY_PAD_A);
        handleTrackedControllerButton(pOffTrackedRemoteNew, pOffTrackedRemoteOld, domButton2, KEY_PAD_B);

        handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, KEY_PAD_B);
    }
    else
    {
        float distance = sqrtf(powf(pOffTracking->HeadPose.Pose.Position.x - pDominantTracking->HeadPose.Pose.Position.x, 2) +
                               powf(pOffTracking->HeadPose.Pose.Position.y - pDominantTracking->HeadPose.Pose.Position.y, 2) +
                               powf(pOffTracking->HeadPose.Pose.Position.z - pDominantTracking->HeadPose.Pose.Position.z, 2));

        //Turn on weapon stabilisation?
        if ((pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) !=
            (pOffTrackedRemoteOld->Buttons & ovrButton_GripTrigger)) {

            if (pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger)
            {
                if (distance < 0.50f)
                {
                    weaponStabilised = true;
                }
            }
            else
            {
                weaponStabilised = false;
            }
        }

        //dominant hand stuff first
        {
			///Weapon location relative to view
            weaponoffset[0] = pDominantTracking->HeadPose.Pose.Position.x - hmdPosition[0];
            weaponoffset[1] = pDominantTracking->HeadPose.Pose.Position.y - hmdPosition[1];
            weaponoffset[2] = pDominantTracking->HeadPose.Pose.Position.z - hmdPosition[2];

			{
				vec2_t v;
				rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -(doomYaw - hmdorientation[YAW]), v);
				weaponoffset[0] = v[1];
				weaponoffset[2] = v[0];
			}

            //Set gun angles
            const ovrQuatf quatRemote = pDominantTracking->HeadPose.Pose.Orientation;
            vec3_t rotation = {0};
            rotation[PITCH] = vr_weapon_pitchadjust;
            //rotation[YAW] = (doomYaw - hmdorientation[YAW]);
            QuatToYawPitchRoll(quatRemote, rotation, weaponangles);


            if (weaponStabilised)
            {
                float z = pOffTracking->HeadPose.Pose.Position.z - pDominantTracking->HeadPose.Pose.Position.z;
                float x = pOffTracking->HeadPose.Pose.Position.x - pDominantTracking->HeadPose.Pose.Position.x;
                float y = pOffTracking->HeadPose.Pose.Position.y - pDominantTracking->HeadPose.Pose.Position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles, -degrees(atanf(y / zxDist)), -degrees(atan2f(x, -z)), weaponangles[ROLL]);
                }
            }
        }

        float controllerYawHeading = 0.0f;

        //off-hand stuff
        {
            offhandoffset[0] = pOffTracking->HeadPose.Pose.Position.x - hmdPosition[0];
            offhandoffset[1] = pOffTracking->HeadPose.Pose.Position.y - hmdPosition[1];
            offhandoffset[2] = pOffTracking->HeadPose.Pose.Position.z - hmdPosition[2];

			vec2_t v;
			rotateAboutOrigin(offhandoffset[0], offhandoffset[2], -(doomYaw - hmdorientation[YAW]), v);
			offhandoffset[0] = v[1];
			offhandoffset[2] = v[0];

            vec3_t rotation = {0};
            QuatToYawPitchRoll(pOffTracking->HeadPose.Pose.Orientation, rotation, offhandangles);

			if (vr_moveuseoffhand != 0) {
				controllerYawHeading = offhandangles[YAW] - hmdorientation[YAW];
			}
			else
			{
				controllerYawHeading = 0.0f;
			}
        }

        //Positional movement
        {
            ALOGV("        Right-Controller-Position: %f, %f, %f",
                  pDominantTracking->HeadPose.Pose.Position.x,
				  pDominantTracking->HeadPose.Pose.Position.y,
				  pDominantTracking->HeadPose.Pose.Position.z);

            vec2_t v;
            rotateAboutOrigin(positionDeltaThisFrame[0], positionDeltaThisFrame[2], -(doomYaw - hmdorientation[YAW]), v);
            positional_movementSideways = v[1];
            positional_movementForward = v[0];

            ALOGV("        positional_movementSideways: %f, positional_movementForward: %f",
                  positional_movementSideways,
                  positional_movementForward);
        }

        //Off-hand specific stuff
        {
            ALOGV("        Left-Controller-Position: %f, %f, %f",
                  pOffTracking->HeadPose.Pose.Position.x,
				  pOffTracking->HeadPose.Pose.Position.y,
				  pOffTracking->HeadPose.Pose.Position.z);

			//Teleport - only does anything if vr_teleport cvar is true
			if (pOffTrackedRemoteOld->Joystick.y > 0.7f && !ready_teleport)
            {
                ready_teleport = true;
            }
			else if (pOffTrackedRemoteOld->Joystick.y < 0.7f & ready_teleport)
            {
                ready_teleport = false;
                trigger_teleport = true;
                resetDoomYaw = true;
            }

			//Apply a filter and quadratic scaler so small movements are easier to make
			//and we don't get movement jitter when the joystick doesn't quite center properly
			float dist = length(pOffTrackedRemoteNew->Joystick.x, pOffTrackedRemoteNew->Joystick.y);
			float nlf = nonLinearFilter(dist);
            float x = nlf * pOffTrackedRemoteNew->Joystick.x;
            float y = nlf * pOffTrackedRemoteNew->Joystick.y;

            //Apply a simple deadzone
            player_moving = (fabs(x) + fabs(y)) > 0.05f;
            x = player_moving ? x : 0;
            y = player_moving ? y : 0;

			//Adjust to be off-hand controller oriented
            vec2_t v;
            rotateAboutOrigin(x, y, controllerYawHeading, v);

            remote_movementSideways = v[0];
            remote_movementForward = v[1];
            ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
                  remote_movementSideways,
                  remote_movementForward);

            // Turning logic
            static int increaseSnap = true;
			if (pDominantTrackedRemoteNew->Joystick.x > 0.6f)
			{
				if (increaseSnap)
				{
                    resetDoomYaw = true;
                    snapTurn -= vr_snapturn_angle;
                    if (vr_snapturn_angle > 10.0f) {
                        increaseSnap = false;
                    }

                    if (snapTurn < -180.0f)
                    {
                        snapTurn += 360.f;
                    }
                }
			} else if (pDominantTrackedRemoteNew->Joystick.x < 0.4f) {
				increaseSnap = true;
			}

			static int decreaseSnap = true;
			if (pDominantTrackedRemoteNew->Joystick.x < -0.6f)
			{
				if (decreaseSnap)
				{
                    resetDoomYaw = true;
					snapTurn += vr_snapturn_angle;

					//If snap turn configured for less than 10 degrees
					if (vr_snapturn_angle > 10.0f) {
                        decreaseSnap = false;
                    }

                    if (snapTurn > 180.0f)
                    {
                        snapTurn -= 360.f;
                    }
				}
			} else if (pDominantTrackedRemoteNew->Joystick.x > -0.4f)
			{
				decreaseSnap = true;
			}
        }

        //Now handle all the buttons
        {
            //Dominant Grip works like a shift key
            bool dominantGripPushedOld = (pDominantTrackedRemoteOld->Buttons & ovrButton_GripTrigger) != 0;
            bool dominantGripPushedNew = (pDominantTrackedRemoteNew->Buttons & ovrButton_GripTrigger) != 0;

            //Weapon Chooser
            static int itemSwitched = 0;
            if (between(-0.2f, pDominantTrackedRemoteNew->Joystick.x, 0.2f) &&
                (between(0.8f, pDominantTrackedRemoteNew->Joystick.y, 1.0f) ||
                 between(-1.0f, pDominantTrackedRemoteNew->Joystick.y, -0.8f)))
            {
                if (itemSwitched == 0) {
                    if (between(0.8f, pDominantTrackedRemoteNew->Joystick.y, 1.0f))
                    {
                        Joy_GenerateButtonEvents(0, 1, 1, KEY_MWHEELDOWN);
                        itemSwitched = 1;
                    }
                    else
                    {
                        Joy_GenerateButtonEvents(0, 1, 1, KEY_MWHEELUP);
                        itemSwitched = 2;
                    }
                }
            } else {
                if (itemSwitched == 1)
                {
                    Joy_GenerateButtonEvents(1, 0, 1, KEY_MWHEELDOWN);
                }
                else if (itemSwitched == 2)
                {
                    Joy_GenerateButtonEvents(1, 0, 1, KEY_MWHEELUP);
                }
                itemSwitched = 0;
            }

            //Dominant Hand - Primary keys (no grip pushed) - All keys are remappable, default bindngs are shown below

            //Fire
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_RTRIGGER);

            //"Use" (open door, toggle switch etc)
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & domButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & domButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_A);

            //No Binding
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & domButton2) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & domButton2) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_B);

            // Inv Use
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_ENTER);



            //Dominant Hand - Secondary keys (grip pushed)
            //Alt-Fire
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_LTRIGGER);

            //Crouch
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & domButton1) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & domButton1) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_LTHUMB);

            //No Binding
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & domButton2) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & domButton2) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_RSHIFT);

            //Toggle Map
            Joy_GenerateButtonEvents(((pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_TAB);




            //Off Hand - Primary keys (no grip pushed)

            //No Default Binding
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_LSHIFT);

            //No Default Binding
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & offButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & offButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_X);

            //Jump
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & offButton2) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & offButton2) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PAD_Y);

            //"Use" (open door, toggle switch etc) - Can be rebound for other uses
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
                                     1, KEY_SPACE);



            //Off Hand - Secondary keys (grip pushed)

            //Toggle Run
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, AKEYCODE_CAPS_LOCK);

            //Move Down
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & offButton1) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & offButton1) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PGDN);

            //Move Up
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & offButton2) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & offButton2) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_PGUP);

            //Land
            Joy_GenerateButtonEvents(((pOffTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
                                     ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
                                     1, KEY_HOME);
        }
    }

    //Save state
    rightTrackedRemoteState_old = rightTrackedRemoteState_new;
    leftTrackedRemoteState_old = leftTrackedRemoteState_new;
}