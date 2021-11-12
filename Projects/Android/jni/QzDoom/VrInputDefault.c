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
int getMenuState();
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);
float getViewpointYaw();

void HandleInput_Default( int control_scheme, ovrInputStateGamepad *pFootTrackingNew, ovrInputStateGamepad *pFootTrackingOld,
                        ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )

{
    //Menu button - invoke menu
    handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, KEY_ESCAPE);
    handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, ovrButton_Enter, KEY_ESCAPE); // For users who have switched the buttons

    //Dominant Grip works like a shift key
    bool dominantGripPushedOld = vr_secondarybuttonmappings ?
            (pDominantTrackedRemoteOld->Buttons & ovrButton_GripTrigger) != 0 : false;
    bool dominantGripPushedNew = vr_secondarybuttonmappings ?
            (pDominantTrackedRemoteNew->Buttons & ovrButton_GripTrigger) != 0 : false;

    ovrInputStateTrackedRemote *pPrimaryTrackedRemoteNew, *pPrimaryTrackedRemoteOld,  *pSecondaryTrackedRemoteNew, *pSecondaryTrackedRemoteOld;
    if (vr_switchsticks)
    {
        pPrimaryTrackedRemoteNew = pOffTrackedRemoteNew;
        pPrimaryTrackedRemoteOld = pOffTrackedRemoteOld;
        pSecondaryTrackedRemoteNew = pDominantTrackedRemoteNew;
        pSecondaryTrackedRemoteOld = pDominantTrackedRemoteOld;
    }
    else
    {
        pPrimaryTrackedRemoteNew = pDominantTrackedRemoteNew;
        pPrimaryTrackedRemoteOld = pDominantTrackedRemoteOld;
        pSecondaryTrackedRemoteNew = pOffTrackedRemoteNew;
        pSecondaryTrackedRemoteOld = pOffTrackedRemoteOld;
    }

    //All this to allow stick and button switching!
    uint32_t primaryButtonsNew;
    uint32_t primaryButtonsOld;
    uint32_t secondaryButtonsNew;
    uint32_t secondaryButtonsOld;
    int primaryButton1;
    int primaryButton2;
    int secondaryButton1;
    int secondaryButton2;

    if (control_scheme == 11) // Left handed Alt
    {
        primaryButtonsNew = pOffTrackedRemoteNew->Buttons;
        primaryButtonsOld = pOffTrackedRemoteOld->Buttons;
        secondaryButtonsNew = pDominantTrackedRemoteNew->Buttons;
        secondaryButtonsOld = pDominantTrackedRemoteOld->Buttons;

        primaryButton1 = offButton1;
        primaryButton2 = offButton2;
        secondaryButton1 = domButton1;
        secondaryButton2 = domButton2;
    }
    else // Left and right handed
    {
        primaryButtonsNew = pDominantTrackedRemoteNew->Buttons;
        primaryButtonsOld = pDominantTrackedRemoteOld->Buttons;
        secondaryButtonsNew = pOffTrackedRemoteNew->Buttons;
        secondaryButtonsOld = pOffTrackedRemoteOld->Buttons;

        primaryButton1 = domButton1;
        primaryButton2 = domButton2;
        secondaryButton1 = offButton1;
        secondaryButton2 = offButton2;
    }

    //In cinema mode, right-stick controls mouse
    const float mouseSpeed = 3.0f;
    if (cinemamode)
    {
        if (fabs(pPrimaryTrackedRemoteNew->Joystick.x) > 0.1f) {
            cinemamodeYaw -= mouseSpeed * pPrimaryTrackedRemoteNew->Joystick.x;
        }
        if (fabs(pPrimaryTrackedRemoteNew->Joystick.y) > 0.1f) {
            cinemamodePitch -= mouseSpeed * pPrimaryTrackedRemoteNew->Joystick.y;
        }
    }

    // Only do the following if we are definitely not in the menu
    if (getMenuState() == 0)
    {
        float distance = sqrtf(powf(pOffTracking->HeadPose.Pose.Position.x -
                                    pDominantTracking->HeadPose.Pose.Position.x, 2) +
                               powf(pOffTracking->HeadPose.Pose.Position.y -
                                    pDominantTracking->HeadPose.Pose.Position.y, 2) +
                               powf(pOffTracking->HeadPose.Pose.Position.z -
                                    pDominantTracking->HeadPose.Pose.Position.z, 2));

        //Turn on weapon stabilisation?
        if (vr_twohandedweapons &&
                (pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) !=
            (pOffTrackedRemoteOld->Buttons & ovrButton_GripTrigger)) {

            if (pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) {
                if (distance < 0.50f) {
                    weaponStabilised = true;
                }
            } else {
                weaponStabilised = false;
            }
        }

        //dominant hand stuff first
        {
            ///Weapon location relative to view
            weaponoffset[0] = pDominantTracking->HeadPose.Pose.Position.x - hmdPosition[0];
            weaponoffset[1] = pDominantTracking->HeadPose.Pose.Position.y - hmdPosition[1];
            weaponoffset[2] = pDominantTracking->HeadPose.Pose.Position.z - hmdPosition[2];

            vec2_t v;
            float yawRotation = getViewpointYaw() - hmdorientation[YAW];
            rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -yawRotation, v);
            weaponoffset[0] = v[1];
            weaponoffset[2] = v[0];

            //Set gun angles
            const ovrQuatf quatRemote = pDominantTracking->HeadPose.Pose.Orientation;
            vec3_t rotation = {0};
            rotation[PITCH] = vr_weapon_pitchadjust;
            QuatToYawPitchRoll(quatRemote, rotation, weaponangles);


            if (weaponStabilised) {
                float z = pOffTracking->HeadPose.Pose.Position.z -
                          pDominantTracking->HeadPose.Pose.Position.z;
                float x = pOffTracking->HeadPose.Pose.Position.x -
                          pDominantTracking->HeadPose.Pose.Position.x;
                float y = pOffTracking->HeadPose.Pose.Position.y -
                          pDominantTracking->HeadPose.Pose.Position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles, -degrees(atanf(y / zxDist)), -degrees(atan2f(x, -z)),
                              weaponangles[ROLL]);
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
            float yawRotation = getViewpointYaw() - hmdorientation[YAW];
            rotateAboutOrigin(offhandoffset[0], offhandoffset[2], -yawRotation, v);
            offhandoffset[0] = v[1];
            offhandoffset[2] = v[0];

            vec3_t rotation = {0};
            rotation[PITCH] = vr_weapon_pitchadjust;
            QuatToYawPitchRoll(pOffTracking->HeadPose.Pose.Orientation, rotation, offhandangles);

            if (vr_moveuseoffhand) {
                controllerYawHeading = offhandangles[YAW] - hmdorientation[YAW];
            } else {
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
            rotateAboutOrigin(positionDeltaThisFrame[0], positionDeltaThisFrame[2],
                              -(doomYaw - hmdorientation[YAW]), v);
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
            if (vr_use_teleport) {
                if ((pSecondaryTrackedRemoteOld->Joystick.y > 0.7f) && !ready_teleport) {
                    ready_teleport = true;
                } else if ((pSecondaryTrackedRemoteOld->Joystick.y < 0.7f) && ready_teleport) {
                    ready_teleport = false;
                    trigger_teleport = true;
                }
            }

            //Apply a filter and quadratic scaler so small movements are easier to make
            //and we don't get movement jitter when the joystick doesn't quite center properly
            float dist = length(pSecondaryTrackedRemoteNew->Joystick.x, pSecondaryTrackedRemoteNew->Joystick.y);
            float nlf = nonLinearFilter(dist);
            float x = nlf * pSecondaryTrackedRemoteNew->Joystick.x + pFootTrackingNew->LeftJoystick.x;
            float y = nlf * pSecondaryTrackedRemoteNew->Joystick.y - pFootTrackingNew->LeftJoystick.y;

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


            if (!cinemamode && !dominantGripPushedNew)
            {
                // Turning logic
                static int increaseSnap = true;
                if (pPrimaryTrackedRemoteNew->Joystick.x > 0.6f) {
                    if (increaseSnap) {
                        resetDoomYaw = true;
                        snapTurn -= vr_snapturn_angle;
                        if (vr_snapturn_angle > 10.0f) {
                            increaseSnap = false;
                        }

                        if (snapTurn < -180.0f) {
                            snapTurn += 360.f;
                        }
                    }
                } else if (pPrimaryTrackedRemoteNew->Joystick.x < 0.4f) {
                    increaseSnap = true;
                }

                static int decreaseSnap = true;
                if (pPrimaryTrackedRemoteNew->Joystick.x < -0.6f) {
                    if (decreaseSnap) {
                        resetDoomYaw = true;
                        snapTurn += vr_snapturn_angle;

                        //If snap turn configured for less than 10 degrees
                        if (vr_snapturn_angle > 10.0f) {
                            decreaseSnap = false;
                        }

                        if (snapTurn > 180.0f) {
                            snapTurn -= 360.f;
                        }
                    }
                } else if (pPrimaryTrackedRemoteNew->Joystick.x > -0.4f) {
                    decreaseSnap = true;
                }
            }
        }
    }

    //if in cinema mode, then the dominant joystick is used differently
    if (!cinemamode) 
    {
        //Default this is Weapon Chooser - This _could_ be remapped
        Joy_GenerateButtonEvents(
            (pPrimaryTrackedRemoteOld->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0), 
            (pPrimaryTrackedRemoteNew->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0), 
            1, KEY_MWHEELDOWN);

        //Default this is Weapon Chooser - This _could_ be remapped
        Joy_GenerateButtonEvents(
            (pPrimaryTrackedRemoteOld->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0), 
            (pPrimaryTrackedRemoteNew->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0), 
            1, KEY_MWHEELUP);

        //If snap turn set to 0, then we can use left/right on the stick as mappable functions
        Joy_GenerateButtonEvents(
            (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld && !vr_snapturn_angle ? 1 : 0), 
            (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew && !vr_snapturn_angle ? 1 : 0), 
            1, KEY_MWHEELLEFT);

        Joy_GenerateButtonEvents(
            (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld && !vr_snapturn_angle ? 1 : 0), 
            (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew && !vr_snapturn_angle ? 1 : 0), 
            1, KEY_MWHEELRIGHT);
    }


    //Dominant Hand - Primary keys (no grip pushed) - All keys are re-mappable, default bindngs are shown below
    {
        //Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_RTRIGGER);

        //"Use" (open door, toggle switch etc)
        Joy_GenerateButtonEvents(
            ((primaryButtonsOld & primaryButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((primaryButtonsNew & primaryButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_A);

        //No Binding
        Joy_GenerateButtonEvents(
            ((primaryButtonsOld & primaryButton2) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((primaryButtonsNew & primaryButton2) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_B);

        // Inv Use
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_ENTER);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Touches & ovrTouch_ThumbRest) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Touches & ovrTouch_ThumbRest) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY5);

    }
    
    //Dominant Hand - Secondary keys (grip pushed)
    {
        //Alt-Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_LTRIGGER);

        //Crouch
        Joy_GenerateButtonEvents(
            ((primaryButtonsOld & primaryButton1) != 0) && dominantGripPushedOld ? 1 : 0,
            ((primaryButtonsNew & primaryButton1) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_LTHUMB);

        //No Binding
        Joy_GenerateButtonEvents(
            ((primaryButtonsOld & primaryButton2) != 0) && dominantGripPushedOld ? 1 : 0,
            ((primaryButtonsNew & primaryButton2) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_RSHIFT);

        //No Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_TAB);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Touches & ovrTouch_ThumbRest) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Touches & ovrTouch_ThumbRest) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY6);

        //Use grip as an extra button
        //Alt-Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & ovrButton_GripTrigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & ovrButton_GripTrigger) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_LTRIGGER);
    }


    //Off Hand - Primary keys (no grip pushed)
    {
        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_LSHIFT);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((secondaryButtonsOld & secondaryButton1) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((secondaryButtonsNew & secondaryButton1) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_X);

        //Toggle Map
        Joy_GenerateButtonEvents(
            ((secondaryButtonsOld & secondaryButton2) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((secondaryButtonsNew & secondaryButton2) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_Y);

        //"Use" (open door, toggle switch etc) - Can be rebound for other uses
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_SPACE);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Touches & ovrTouch_ThumbRest) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Touches & ovrTouch_ThumbRest) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY7);

        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_GripTrigger) != 0) && !dominantGripPushedOld && !vr_twohandedweapons ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) != 0) && !dominantGripPushedNew && !vr_twohandedweapons ? 1 : 0,
            1, KEY_PAD_RTHUMB);
    }

    //Off Hand - Secondary keys (grip pushed)
    {
        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_LALT);

        //Move Down
        Joy_GenerateButtonEvents(
            ((secondaryButtonsOld & secondaryButton1) != 0) && dominantGripPushedOld ? 1 : 0,
            ((secondaryButtonsNew & secondaryButton1) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_PGDN);

        //Move Up
        Joy_GenerateButtonEvents(
            ((secondaryButtonsOld & secondaryButton2) != 0) && dominantGripPushedOld ? 1 : 0,
            ((secondaryButtonsNew & secondaryButton2) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_PGUP);

        //Land
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_HOME);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Touches & ovrTouch_ThumbRest) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Touches & ovrTouch_ThumbRest) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY8);

        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & ovrButton_GripTrigger) != 0) && dominantGripPushedOld && !vr_twohandedweapons ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) != 0) && dominantGripPushedNew && !vr_twohandedweapons ? 1 : 0,
            1, KEY_PAD_DPAD_UP);
    }

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS1PLUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS1MINUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS3PLUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS3MINUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS2MINUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS2PLUS);
    
    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.y < -0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.y < -0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS4MINUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.y > 0.7f && !dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.y > 0.7f && !dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS4PLUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.x > 0.7f && dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.x > 0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS5PLUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.x < -0.7f && dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.x < -0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS5MINUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS7PLUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS7MINUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.y < -0.7f && dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.y < -0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS6MINUS);

    Joy_GenerateButtonEvents(
        (pSecondaryTrackedRemoteOld->Joystick.y > 0.7f && dominantGripPushedOld ? 1 : 0), 
        (pSecondaryTrackedRemoteNew->Joystick.y > 0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS6PLUS);
    
    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.y < -0.7f && dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.y < -0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS8MINUS);

    Joy_GenerateButtonEvents(
        (pPrimaryTrackedRemoteOld->Joystick.y > 0.7f && dominantGripPushedOld ? 1 : 0), 
        (pPrimaryTrackedRemoteNew->Joystick.y > 0.7f && dominantGripPushedNew ? 1 : 0), 
        1, KEY_JOYAXIS8PLUS);

    //Save state
    rightTrackedRemoteState_old = rightTrackedRemoteState_new;
    leftTrackedRemoteState_old = leftTrackedRemoteState_new;
}