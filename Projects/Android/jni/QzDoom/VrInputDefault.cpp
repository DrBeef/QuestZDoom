/************************************************************************************

Filename	:	VrInputDefault.c
Content		:	Handles default controller input
Created		:	August 2019
Authors		:	Simon Brown

*************************************************************************************/


#include <android/keycodes.h>

#include "VrInput.h"

#include "doomkeys.h"

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;

extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

extern float remote_movementSideways;
extern float remote_movementForward;
extern float remote_movementUp;
extern float positional_movementSideways;
extern float positional_movementForward;
extern float snapTurn;

extern float cinemamodeYaw;
extern float cinemamodePitch;

extern bool weaponStabilised;

int getGameState();
int getMenuState();
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);
float getViewpointYaw();

void HandleInput_Default( int control_scheme, ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTrackedController* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTrackedController* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )

{
    //Menu button - invoke menu
    handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, xrButton_Enter, KEY_ESCAPE);
    handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, xrButton_Enter, KEY_ESCAPE); // For users who have switched the buttons

    //Dominant Grip works like a shift key
    bool dominantGripPushedOld = vr_secondary_button_mappings ?
            (pDominantTrackedRemoteOld->Buttons & xrButton_GripTrigger) : false;
    bool dominantGripPushedNew = vr_secondary_button_mappings ?
            (pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) : false;

    ovrInputStateTrackedRemote *pPrimaryTrackedRemoteNew, *pPrimaryTrackedRemoteOld,  *pSecondaryTrackedRemoteNew, *pSecondaryTrackedRemoteOld;
    if (vr_switch_sticks)
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
        float distance = sqrtf(powf(pOffTracking->Pose.position.x -
                                    pDominantTracking->Pose.position.x, 2) +
                               powf(pOffTracking->Pose.position.y -
                                    pDominantTracking->Pose.position.y, 2) +
                               powf(pOffTracking->Pose.position.z -
                                    pDominantTracking->Pose.position.z, 2));

        //Turn on weapon stabilisation?
        if (vr_two_handed_weapons &&
                (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) !=
            (pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger)) {

            if (pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) {
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
            weaponoffset[0] = pDominantTracking->Pose.position.x - hmdPosition[0] - vr_weaponOffsetX;
            weaponoffset[1] = pDominantTracking->Pose.position.y - hmdPosition[1] + vr_weaponOffsetY;
            weaponoffset[2] = pDominantTracking->Pose.position.z - hmdPosition[2] + vr_weaponOffsetZ;

            vec2_t v;
            float yawRotation = getViewpointYaw() - hmdorientation[YAW];
            rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -yawRotation, v);
            weaponoffset[0] = v[1];
            weaponoffset[2] = v[0];

            //Set gun angles
            vec3_t rotation = {0};
            rotation[PITCH] = vr_weaponRotate;
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, weaponangles);


            if (weaponStabilised) {
                float z = pOffTracking->Pose.position.z -
                          pDominantTracking->Pose.position.z;
                float x = pOffTracking->Pose.position.x -
                          pDominantTracking->Pose.position.x;
                float y = pOffTracking->Pose.position.y -
                          pDominantTracking->Pose.position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles, -RAD2DEG(atanf(y / zxDist)), -RAD2DEG(atan2f(x, -z)),
                              weaponangles[ROLL]);
                }
            }
        }

        float controllerYawHeading = 0.0f;

        //off-hand stuff
        {
            offhandoffset[0] = pOffTracking->Pose.position.x - hmdPosition[0] + vr_weaponOffsetX;
            offhandoffset[1] = pOffTracking->Pose.position.y - hmdPosition[1] + vr_weaponOffsetY;
            offhandoffset[2] = pOffTracking->Pose.position.z - hmdPosition[2] + vr_weaponOffsetZ;

            vec2_t v;
            float yawRotation = getViewpointYaw() - hmdorientation[YAW];
            rotateAboutOrigin(offhandoffset[0], offhandoffset[2], -yawRotation, v);
            offhandoffset[0] = v[1];
            offhandoffset[2] = v[0];

            vec3_t rotation = {0};
            rotation[PITCH] = vr_weaponRotate;
            QuatToYawPitchRoll(pOffTracking->Pose.orientation, rotation, offhandangles);

            if (vr_move_use_offhand) {
                controllerYawHeading = offhandangles[YAW] - hmdorientation[YAW];
            } else {
                controllerYawHeading = 0.0f;
            }
        }

        //Positional movement
        {
            ALOGV("        Right-Controller-Position: %f, %f, %f",
                  pDominantTracking->Pose.position.x,
                  pDominantTracking->Pose.position.y,
                  pDominantTracking->Pose.position.z);

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
                  pOffTracking->Pose.position.x,
                  pOffTracking->Pose.position.y,
                  pOffTracking->Pose.position.z);

            //Teleport - only does anything if vr_teleport cvar is true
            if (vr_teleport) {
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
            dist = (dist > 1.0f) ? dist : 1.0f;
            float x = nlf * (pSecondaryTrackedRemoteNew->Joystick.x / dist);
            float y = nlf * (pSecondaryTrackedRemoteNew->Joystick.y / dist);

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
                static int increaseSnap = true;
                static int decreaseSnap = true;

                float joy = pPrimaryTrackedRemoteNew->Joystick.x;
                if (vr_snapTurn <= 10.0f && abs(joy) > 0.05f)
                {
                    increaseSnap = false;
                    decreaseSnap = false;
                    snapTurn -= vr_snapTurn * nonLinearFilter(joy);
                }

                // Turning logic
                if (joy > 0.6f && increaseSnap) {
                    resetDoomYaw = true;
                    snapTurn -= vr_snapTurn;
                    if (vr_snapTurn > 10.0f) {
                        increaseSnap = false;
                    }
                } else if (joy < 0.4f) {
                    increaseSnap = true;
                }

                if (joy < -0.6f && decreaseSnap) {
                    resetDoomYaw = true;
                    snapTurn += vr_snapTurn;
                    if (vr_snapTurn > 10.0f) {
                        decreaseSnap = false;
                    }
                } else if (joy > -0.4f) {
                    decreaseSnap = true;
                }

                if (snapTurn < -180.0f) {
                    snapTurn += 360.f;
                }
                else if (snapTurn > 180.0f) {
                    snapTurn -= 360.f;
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
            (pPrimaryTrackedRemoteOld->Joystick.x > 0.7f && !dominantGripPushedOld && !vr_snapTurn ? 1 : 0),
            (pPrimaryTrackedRemoteNew->Joystick.x > 0.7f && !dominantGripPushedNew && !vr_snapTurn ? 1 : 0),
            1, KEY_MWHEELLEFT);

        Joy_GenerateButtonEvents(
            (pPrimaryTrackedRemoteOld->Joystick.x < -0.7f && !dominantGripPushedOld && !vr_snapTurn ? 1 : 0),
            (pPrimaryTrackedRemoteNew->Joystick.x < -0.7f && !dominantGripPushedNew && !vr_snapTurn ? 1 : 0),
            1, KEY_MWHEELRIGHT);
    }


    //Dominant Hand - Primary keys (no grip pushed) - All keys are re-mappable, default bindngs are shown below
    {
        //Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
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
            ((pDominantTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_ENTER);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY5);

        //Use grip as an extra button
        //Alt-Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & xrButton_GripTrigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_LTRIGGER);
    }
    
    //Dominant Hand - Secondary keys (grip pushed)
    {
        //Alt-Fire
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
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
            1, KEY_BACKSPACE);

        //No Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_TAB);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pDominantTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pDominantTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY6);

    }


    //Off Hand - Primary keys (no grip pushed)
    {
        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) && !dominantGripPushedNew ? 1 : 0,
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
            ((pOffTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_SPACE);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY7);

        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger) != 0) && !dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0) && !dominantGripPushedNew ? 1 : 0,
            1, KEY_PAD_RTHUMB);
    }

    //Off Hand - Secondary keys (grip pushed)
    {
        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & xrButton_Trigger) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_Trigger) != 0) && dominantGripPushedNew ? 1 : 0,
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
            ((pOffTrackedRemoteOld->Buttons & xrButton_Joystick) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_Joystick) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_HOME);

        //No Default Binding
        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Touches & xrButton_ThumbRest) != 0) && dominantGripPushedOld ? 1 : 0,
            ((pOffTrackedRemoteNew->Touches & xrButton_ThumbRest) != 0) && dominantGripPushedNew ? 1 : 0,
            1, KEY_JOY8);

        Joy_GenerateButtonEvents(
            ((pOffTrackedRemoteOld->Buttons & xrButton_GripTrigger) != 0) && dominantGripPushedOld && !vr_two_handed_weapons ? 1 : 0,
            ((pOffTrackedRemoteNew->Buttons & xrButton_GripTrigger) != 0) && dominantGripPushedNew && !vr_two_handed_weapons ? 1 : 0,
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