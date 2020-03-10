//
//---------------------------------------------------------------------------
// Copyright(C) 2016-2017 Christopher Bruns
// Oculus Quest changes Copyright(C) 2020 Simon Brown
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_oculusquest.cpp
** Stereoscopic virtual reality mode for the Oculus Quest HMD
**
*/

#include "gl_oculusquest.h"

#include <string>
#include <map>
#include <cmath>
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/renderer/gl_2ddrawer.h" // crosshair
#include "gl/models/gl_models.h"
#include "g_levellocals.h" // pixelstretch
#include "g_statusbar/sbar.h"
#include "math/cmath.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "m_joy.h"
#include "d_gui.h"
#include "d_event.h"

#include "QzDoom/VrCommon.h"
#include "LSMatrix.h"


EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_floor_offset)

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_moveFollowsOffHand)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponScale);

double P_XYMovement(AActor *mo, DVector2 scroll);
extern "C" void VR_GetMove( float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up, float *yaw, float *pitch, float *roll );


namespace s3d
{
    static DVector3 oculusquest_origin(0, 0, 0);
    static float deltaYawDegrees;

    OculusQuestEyePose::OculusQuestEyePose(int eye)
            : ShiftedEyePose( 0.0f )
            , eye(eye)
    {
    }


/* virtual */
    OculusQuestEyePose::~OculusQuestEyePose()
    {
        dispose();
    }

    /*
    static void vSMatrixFromHmdMatrix34(VSMatrix& m1, const HmdMatrix34_t& m2)
    {
        float tmp[16];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                tmp[4 * i + j] = m2.m[i][j];
            }
        }
        int i = 3;
        for (int j = 0; j < 4; ++j) {
            tmp[4 * i + j] = 0;
        }
        tmp[15] = 1;
        m1.loadMatrix(&tmp[0]);
    }*/


/* virtual */
    void OculusQuestEyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
    {
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

        // Pitch and Roll are identical between OpenVR and Doom worlds.
        // But yaw can differ, depending on starting state, and controller movement.
        float doomYawDegrees = VR_GetRawYaw() + hmdorientation[YAW];
        while (doomYawDegrees > 180)
            doomYawDegrees -= 360;
        while (doomYawDegrees < -180)
            doomYawDegrees += 360;

        VSMatrix shiftMat;
        shiftMat.loadIdentity();

        shiftMat.rotate(GLRenderer->mAngles.Roll.Degrees, 0, 1, 0);
        shiftMat.rotate(GLRenderer->mAngles.Pitch.Degrees, 1, 0, 0);
        shiftMat.rotate(-doomYawDegrees, 0, 0, 1);
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        shiftMat.scale(1.0, pixelstretch, 1.0);

        double mult = eye == 0 ? 1.0 : -1.0;

        LSVec3 vec(0, (vr_ipd * 0.5) * vr_vunits_per_meter * mult, 0);
        LSMatrix44 mat(shiftMat);

        LSVec3 eyeOffset = mat * vec;

        // In vr, the real world floor level is at y==0
        // In Doom, the virtual player foot level is viewheight below the current viewpoint (on the Z axis)
        // We want to align those two heights here
        const player_t & player = players[consoleplayer];
        double vh = player.viewheight; // Doom thinks this is where you are
        double hh = (hmdPosition[1] - vr_floor_offset) * vr_vunits_per_meter; // HMD is actually here
        eyeOffset[2] += hh - vh;

        outViewShift[0] = eyeOffset[0];
        outViewShift[1] = eyeOffset[1];
        outViewShift[2] = eyeOffset[2];
    }

/* virtual */
    VSMatrix OculusQuestEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
    {
        return EyePose::GetProjection(fov, aspectRatio, fovRatio);
    }

    void OculusQuestEyePose::initialize()
    {
    }

    void OculusQuestEyePose::dispose()
    {
    }

    bool OculusQuestEyePose::submitFrame() const
    {
        prepareEyeBuffer( eye );

        GLRenderer->mBuffers->BindEyeTexture(eye, 0);
        GL_IRECT box = {0, 0, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height};
        GLRenderer->DrawPresentTexture(box, true);

        finishEyeBuffer( eye );

        return true;
    }

    VSMatrix OculusQuestEyePose::getQuadInWorld(
            float distance, // meters
            float width, // meters
            bool doFixPitch,
            float pitchOffset) const
    {
        VSMatrix new_projection;
        new_projection.loadIdentity();

        // doom_units from meters
        new_projection.scale(
                -vr_vunits_per_meter,
                vr_vunits_per_meter,
                -vr_vunits_per_meter);
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        new_projection.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

        const OculusQuestEyePose * activeEye = this;

        // eye coordinates from hmd coordinates
        //VSMatrix e2h(activeEye->eyeToHeadTransform);
        //e2h.transpose();
        //new_projection.multMatrix(e2h);

        // Follow HMD orientation, EXCEPT for roll angle (keep weapon upright)
        /*if (activeEye->currentPose) {
            float vrRollDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[2]);
            new_projection.rotate(-vrRollDegrees, 0, 0, 1);

            if (doFixPitch) {
                float vrPitchDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[1]);
                new_projection.rotate(-vrPitchDegrees, 1, 0, 0);
            }
            if (pitchOffset != 0)
                new_projection.rotate(-pitchOffset, 1, 0, 0);
        }*/

        // hmd coordinates (meters) from ndc coordinates
        // const float weapon_distance_meters = 0.55f;
        // const float weapon_width_meters = 0.3f;
        const float aspect = SCREENWIDTH / float(SCREENHEIGHT);
        new_projection.translate(0.0, 0.0, distance);
        new_projection.scale(
                -width,
                width / aspect,
                -width);

        // ndc coordinates from pixel coordinates
        new_projection.translate(-1.0, 1.0, 0);
        new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

//        VSMatrix proj(activeEye->projectionMatrix);
  //      proj.multMatrix(new_projection);
    //    new_projection = proj;

        return new_projection;
    }

    void OculusQuestEyePose::AdjustHud() const
    {
        // Draw crosshair on a separate quad, before updating HUD matrix
        const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
        if (mode3d->IsMono())
            return;

        // Update HUD matrix to render on a separate quad
        const float menu_distance_meters = 1.0f;
        const float menu_width_meters = 0.4f * menu_distance_meters;
        const float pitch_offset = -8.0;
        gl_RenderState.mProjectionMatrix = getQuadInWorld(
                menu_distance_meters,
                menu_width_meters,
                true,
                pitch_offset);
        gl_RenderState.ApplyMatrices();
    }

    void OculusQuestEyePose::AdjustBlend() const
    {
        VSMatrix& proj = gl_RenderState.mProjectionMatrix;
        proj.loadIdentity();
        proj.translate(-1, 1, 0);
        proj.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
        gl_RenderState.ApplyMatrices();
    }


/* static */
    const Stereo3DMode& OculusQuestMode::getInstance()
    {
        static OculusQuestMode instance;
        return instance;
    }

    OculusQuestMode::OculusQuestMode()
            : leftEyeView(0)
            , rightEyeView(1)
            , sceneWidth(0), sceneHeight(0)
            , crossHairDrawer(new F2DDrawer)
            , cached2DDrawer(nullptr)
    {
        eye_ptrs.Push(&leftEyeView);
        eye_ptrs.Push(&rightEyeView);

        //Get this from my code
        Android_GetScreenRes(&sceneWidth, &sceneHeight);

        leftEyeView.initialize();
        rightEyeView.initialize();

        crossHairDrawer->Clear();
    }

    void OculusQuestMode::getTracking(ovrTracking2 *_tracking) const
    {
        *_tracking = tracking;
    }

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
    void OculusQuestMode::AdjustViewports() const
    {
        // Draw the 3D scene into the entire framebuffer
        GLRenderer->mSceneViewport.width = sceneWidth;
        GLRenderer->mSceneViewport.height = sceneHeight;
        GLRenderer->mSceneViewport.left = 0;
        GLRenderer->mSceneViewport.top = 0;

        GLRenderer->mScreenViewport.width = sceneWidth;
        GLRenderer->mScreenViewport.height = sceneHeight;
    }

    void OculusQuestMode::AdjustPlayerSprites() const
    {
        GetWeaponTransform(&gl_RenderState.mModelMatrix);

        float scale = 0.00125f * vr_weaponScale;
        gl_RenderState.mModelMatrix.scale(scale, -scale, scale);
        gl_RenderState.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f);

        gl_RenderState.EnableModelMatrix(true);
    }

    void OculusQuestMode::UnAdjustPlayerSprites() const {

        gl_RenderState.EnableModelMatrix(false);
    }

    bool OculusQuestMode::GetHandTransform(int hand, VSMatrix* mat) const
    {
        {
            mat->loadIdentity();

            AActor* playermo = r_viewpoint.camera->player->mo;
            DVector3 pos = playermo->InterpolatedPosition(r_viewpoint.TicFrac);

            mat->translate(pos.X, pos.Z, pos.Y);

            mat->scale(vr_vunits_per_meter, vr_vunits_per_meter, -vr_vunits_per_meter);

            mat->rotate(-deltaYawDegrees - 180, 0, 1, 0);

            if ((vr_control_scheme < 10 && hand == 1)
                || (vr_control_scheme > 10 && hand == 0)) {
                DVector3 weap(weaponoffset[0], weaponoffset[1], weaponoffset[2]);
                weap *= vr_vunits_per_meter;
                mat->translate(weap.X - hmdPosition[0], weap.Y - vr_floor_offset,
                               weap.Z - hmdPosition[2]);
            }
            else
            {
                DVector3 weap(offhandoffset[0], offhandoffset[1], offhandoffset[2]);
                weap *= vr_vunits_per_meter;
                mat->translate(weap.X - hmdPosition[0], weap.Y - vr_floor_offset,
                               weap.Z - hmdPosition[2]);
            }

            //Perform roration here?


            return true;
        }

        return false;
    }

    bool OculusQuestMode::GetWeaponTransform(VSMatrix* out) const
    {
        long oculusquest_rightHanded = vr_control_scheme < 10;
        if (GetHandTransform(oculusquest_rightHanded ? 1 : 0, out))
        {
            //out->rotate(vr_weaponRotate, 1, 0, 0); - not needed, done in the C
            if (!oculusquest_rightHanded)
                out->scale(-1.0f, 1.0f, 1.0f);
            return true;
        }
        return false;
    }


/* virtual */
    void OculusQuestMode::Present() const {

        leftEyeView.submitFrame();
        rightEyeView.submitFrame();

        submitFrame(&tracking);
    }

    static int mAngleFromRadians(double radians)
    {
        double m = std::round(65535.0 * radians / (2.0 * M_PI));
        return int(m);
    }

/* virtual */
    void OculusQuestMode::SetUp() const
    {
        super::SetUp();

        // Set VR-appropriate settings
        {
            movebob = 0;
            gl_billboard_faces_camera = true;
        }

        if (gamestate == GS_LEVEL && !isMenuActive()) {
            cachedScreenBlocks = screenblocks;
            screenblocks = 12; // always be full-screen during 3D scene render
            setUseScreenLayer(false);
        }
        else {
            //Ensure we are drawing on virtual screen
            setUseScreenLayer(true);
        }

        processHaptics();

        //Get controller state here
        getHMDOrientation(&tracking);

        //Set up stuff used in the tracking code
        vr_weapon_pitchadjust = vr_weaponRotate;
        vr_snapturn_angle = vr_snapTurn;
        vr_walkdirection = !vr_moveFollowsOffHand;
        getTrackedRemotesOrientation(vr_control_scheme);

        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        {
            if (player)
            {
                //Weapon firing tracking - Thanks Fishbiter!
                {
                    player->mo->OverrideAttackPosDir = true;

                    player->mo->AttackPos.X = player->mo->X() + weaponoffset[0];
                    player->mo->AttackPos.Y = player->mo->Y() + weaponoffset[1];
                    player->mo->AttackPos.Z = player->mo->Z() + weaponoffset[2];

                    player->mo->AttackDir = DVector3(weaponangles[0], weaponangles[1], weaponangles[2]);
                }

                //Positional Movement
                float hmd_forward=0;
                float hmd_side=0;
                float dummy=0;
                VR_GetMove(&dummy, &dummy, &hmd_forward, &hmd_side, &dummy, &dummy, &dummy, &dummy);

                //Positional movement - Thanks fishbiter!!
                auto vel = player->mo->Vel;
                player->mo->Vel = DVector3((DVector2(hmd_side, hmd_forward) * vr_vunits_per_meter), 0);
                bool wasOnGround = player->mo->Z() <= player->mo->floorz;
                double oldZ = player->mo->Z();
                P_XYMovement(player->mo, DVector2(0, 0));

                //if we were on the ground before offsetting, make sure we still are (this fixes not being able to move on lifts)
                if (player->mo->Z() >= oldZ && wasOnGround)
                {
                    player->mo->SetZ(player->mo->floorz);
                }
                else
                {
                    player->mo->SetZ(oldZ);
                }
                player->mo->Vel = vel;
            }
        }

        updateHmdPose();
    }


    void OculusQuestMode::updateHmdPose() const
    {
        float dummy=0;
        float yaw=0;
        float pitch=0;
        float roll=0;
        VR_GetMove(&dummy, &dummy, &dummy, &dummy, &dummy, &yaw, &pitch, &roll);

        //Yaw
        {
            static double previousHmdYaw = 0;
            static bool havePreviousYaw = false;
            if (!havePreviousYaw) {
                previousHmdYaw = yaw;
                havePreviousYaw = true;
            }
            double hmdYawDeltaDegrees = yaw - previousHmdYaw;
            G_AddViewAngle(mAngleFromRadians(DEG2RAD(-hmdYawDeltaDegrees)));
            previousHmdYaw = yaw;
        }

        // Pitch
        {
            double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
            double dPitch =
                    - DEG2RAD(pitch)
                    - viewPitchInDoom;
            G_AddViewPitch(mAngleFromRadians(dPitch));
        }

        //Set all the render angles - including roll
        {
            //Always update roll (as the game tic cmd doesn't support roll
            GLRenderer->mAngles.Roll = roll;
            GLRenderer->mAngles.Pitch = pitch;

            double viewYaw = yaw;
            while (viewYaw <= -180.0)
                viewYaw += 360.0;
            while (viewYaw > 180.0)
                viewYaw -= 360.0;
            r_viewpoint.Angles.Yaw.Degrees = viewYaw;
        }
    }

/* virtual */
    void OculusQuestMode::TearDown() const
    {
        if (gamestate == GS_LEVEL) {
            screenblocks = cachedScreenBlocks;
        }
        super::TearDown();
    }

/* virtual */
    OculusQuestMode::~OculusQuestMode()
    {
        {
            leftEyeView.dispose();
            rightEyeView.dispose();
        }
        if (crossHairDrawer != nullptr) {
            delete crossHairDrawer;
            crossHairDrawer = nullptr;
        }
    }

} /* namespace s3d */


