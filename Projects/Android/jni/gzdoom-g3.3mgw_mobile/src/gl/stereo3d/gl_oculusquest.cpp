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
#include "p_trace.h"
#include "p_linetracedata.h"
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "g_levellocals.h" // pixelstretch
#include "math/cmath.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "d_gui.h"
#include "d_event.h"

#include "QzDoom/VrCommon.h"
#include "LSMatrix.h"


EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_height_adjust)

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_move_use_offhand)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponScale);
EXTERN_CVAR(Bool, vr_teleport);

//HUD control
EXTERN_CVAR(Float, vr_hud_scale);
EXTERN_CVAR(Float, vr_hud_stereo);
EXTERN_CVAR(Float, vr_hud_rotate);
EXTERN_CVAR(Bool, vr_hud_fixed_pitch);
EXTERN_CVAR(Bool, vr_hud_fixed_roll);

double P_XYMovement(AActor *mo, DVector2 scroll);
extern "C" void VR_GetMove( float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up, float *yaw, float *pitch, float *roll );
void I_Quit();

extern int game_running;

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
    }

/* virtual */
    void OculusQuestEyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
    {
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

        vec3_t angles;
        VectorSet(angles, -GLRenderer->mAngles.Pitch.Degrees,  doomYaw, GLRenderer->mAngles.Roll.Degrees);

        vec3_t v_forward, v_right, v_up;
        AngleVectors(angles, v_forward, v_right, v_up);

        float stereo_separation = (vr_ipd * 0.5) * vr_vunits_per_meter * (eye == 0 ? -1.0 : 1.0);
        vec3_t tmp;
        VectorScale(v_right, stereo_separation, tmp);

        LSVec3 eyeOffset(tmp[0], tmp[1], tmp[2]);

        // In vr, the real world floor level is at y==0
        // In Doom, the virtual player foot level is viewheight below the current viewpoint (on the Z axis)
        // We want to align those two heights here
        const player_t & player = players[consoleplayer];
        double vh = player.viewheight; // Doom thinks this is where you are
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        double hh = ((hmdPosition[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch; // HMD is actually here
        eyeOffset[2] += hh - vh;

        outViewShift[0] = eyeOffset[0];
        outViewShift[1] = eyeOffset[1];
        outViewShift[2] = eyeOffset[2];
    }

/* virtual */
    VSMatrix OculusQuestEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
    {
        projection = EyePose::GetProjection(fov, aspectRatio, fovRatio);
        return projection;
    }

    bool OculusQuestEyePose::submitFrame() const
    {
        QzDoom_prepareEyeBuffer(eye);

        GLRenderer->mBuffers->BindEyeTexture(eye, 0);
        GL_IRECT box = {0, 0, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height};
        GLRenderer->DrawPresentTexture(box, true);

        QzDoom_finishEyeBuffer(eye);

        return true;
    }

    VSMatrix OculusQuestEyePose::getHUDProjection() const
    {
        VSMatrix new_projection;
        new_projection.loadIdentity();

        float stereo_separation = (vr_ipd * 0.5) * vr_vunits_per_meter * vr_hud_stereo * (eye == 1 ? -1.0 : 1.0);
        new_projection.translate(stereo_separation, 0, 0);

        // doom_units from meters
        new_projection.scale(
                -vr_vunits_per_meter,
                vr_vunits_per_meter,
                -vr_vunits_per_meter);
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        new_projection.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

        if (vr_hud_fixed_roll)
        {
            new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);
        }

        new_projection.rotate(vr_hud_rotate, 1, 0, 0);

        if (vr_hud_fixed_pitch)
        {
            new_projection.rotate(-hmdorientation[PITCH], 1, 0, 0);
        }

        // hmd coordinates (meters) from ndc coordinates
        // const float weapon_distance_meters = 0.55f;
        // const float weapon_width_meters = 0.3f;
        new_projection.translate(0.0, 0.0, 1.0);
        new_projection.scale(
                -vr_hud_scale,
                vr_hud_scale,
                -vr_hud_scale);

        // ndc coordinates from pixel coordinates
        new_projection.translate(-1.0, 1.0, 0);
        new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

        VSMatrix proj = projection;
        proj.multMatrix(new_projection);
        new_projection = proj;

        return new_projection;
    }

    void OculusQuestEyePose::AdjustHud() const
    {
        const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
        if (mode3d->IsMono())
            return;

        // Update HUD matrix to render on a separate quad
        gl_RenderState.mProjectionMatrix = getHUDProjection();
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
    {
        eye_ptrs.Push(&leftEyeView);
        eye_ptrs.Push(&rightEyeView);

        //Get this from my code
        QzDoom_GetScreenRes(&sceneWidth, &sceneHeight);
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

        float scale = 0.000625f * vr_weaponScale;
        gl_RenderState.mModelMatrix.scale(scale, -scale, scale);
        gl_RenderState.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f); // What dis?!

        gl_RenderState.EnableModelMatrix(true);
    }

    void OculusQuestMode::UnAdjustPlayerSprites() const {

        gl_RenderState.EnableModelMatrix(false);
    }

    bool OculusQuestMode::GetHandTransform(int hand, VSMatrix* mat) const
    {
        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        if (player)
        {
            AActor* playermo = player->mo;
            DVector3 pos = playermo->InterpolatedPosition(r_viewpoint.TicFrac);

            mat->loadIdentity();

            mat->translate(pos.X, pos.Z, pos.Y);

            mat->scale(vr_vunits_per_meter, vr_vunits_per_meter, -vr_vunits_per_meter);

            double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
            if ((vr_control_scheme < 10 && hand == 1)
                || (vr_control_scheme >= 10 && hand == 0)) {
                mat->translate(-weaponoffset[0], (hmdPosition[1] + weaponoffset[1] + vr_height_adjust) / pixelstretch, weaponoffset[2]);

                mat->rotate(-90 + (doomYaw - hmdorientation[YAW]) + weaponangles[YAW], 0, 1, 0);
                mat->rotate(-weaponangles[PITCH], 1, 0, 0);
                mat->rotate(-weaponangles[ROLL], 0, 0, 1);
            }
            else
            {
                mat->translate(-offhandoffset[0], (hmdPosition[1] + offhandoffset[1] + vr_height_adjust) / pixelstretch, offhandoffset[2]);

                mat->rotate(-90 + (doomYaw - hmdorientation[YAW]) + offhandangles[YAW], 0, 1, 0);
                mat->rotate(-offhandangles[PITCH], 1, 0, 0);
                mat->rotate(-offhandangles[ROLL], 0, 0, 1);
            }

            return true;

        }

        return false;
    }

    bool OculusQuestMode::GetWeaponTransform(VSMatrix* out) const
    {
        long oculusquest_rightHanded = vr_control_scheme < 10;
        if (GetHandTransform(oculusquest_rightHanded ? 1 : 0, out))
        {
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

        QzDoom_submitFrame(&tracking);
    }

    static int mAngleFromRadians(double radians)
    {
        double m = std::round(65535.0 * radians / (2.0 * M_PI));
        return int(m);
    }


    //Fishbiter's Function.. Thank-you!!
    static DVector3 MapAttackDir(AActor* actor, DAngle yaw, DAngle pitch)
    {
        LSMatrix44 mat;
        if (!s3d::Stereo3DMode::getCurrentMode().GetWeaponTransform(&mat))
        {
            double pc = pitch.Cos();

            DVector3 direction = { pc * yaw.Cos(), pc * yaw.Sin(), -pitch.Sin() };
            return direction;
        }
        double pc = pitch.Cos();

        yaw -= actor->Angles.Yaw;

        //ignore specified pitch (would need to compensate for auto aim and no (vanilla) Doom weapon varies this)
        //pitch -= actor->Angles.Pitch;
        pitch.Degrees = 0;

        pc = pitch.Cos();

        LSVec3 local = { (float)(pc * yaw.Cos()), (float)(pc * yaw.Sin()), (float)(-pitch.Sin()), 0.0f };

        DVector3 dir;
        dir.X = local.x * -mat[2][0] + local.y * -mat[0][0] + local.z * -mat[1][0];
        dir.Y = local.x * -mat[2][2] + local.y * -mat[0][2] + local.z * -mat[1][2];
        dir.Z = local.x * -mat[2][1] + local.y * -mat[0][1] + local.z * -mat[1][1];
        dir.MakeUnit();

        return dir;
    }

    bool OculusQuestMode::GetTeleportLocation(DVector3 &out) const
    {
        if (vr_teleport &&
            ready_teleport &&
            m_TeleportTarget == TRACE_HitFloor) {
            out = m_TeleportLocation;
            return true;
        }

        return false;
    }

    /* virtual */
    void OculusQuestMode::SetUp() const
    {
        super::SetUp();

        QzDoom_FrameSetup();

        if (shutdown)
        {
            game_running = false;
            I_Quit();

            return;
        }

        QzDoom_processMessageQueue();

        // Set VR-appropriate settings
        {
            movebob = 0;
        }

        if (gamestate == GS_LEVEL && !isMenuActive()) {
            cachedScreenBlocks = screenblocks;
            screenblocks = 12;
            QzDoom_setUseScreenLayer(false);
        }
        else {
            //Ensure we are drawing on virtual screen
            QzDoom_setUseScreenLayer(true);
        }

        QzDoom_processHaptics();

        //Get controller state here
        QzDoom_getHMDOrientation(&tracking);

        //Set up stuff used in the tracking code - getting the CVARS in the C code would be a faff, so just
        //set some variables - lazy, should do it properly..
        vr_weapon_pitchadjust = vr_weaponRotate;
        vr_snapturn_angle = vr_snapTurn;
        vr_moveuseoffhand = !vr_move_use_offhand;
        vr_use_teleport = vr_teleport;
        QzDoom_getTrackedRemotesOrientation(vr_control_scheme);

        //Some crazy stuff to ascertain the actual yaw that doom is using at the right times!
        if (gamestate != GS_LEVEL || isMenuActive() || (gamestate == GS_LEVEL && resetDoomYaw))
        {
            doomYaw = (float)r_viewpoint.Angles.Yaw.Degrees;
            if (gamestate == GS_LEVEL && resetDoomYaw) {
                resetDoomYaw = false;
            }
            if (gamestate != GS_LEVEL || isMenuActive())
            {
                resetDoomYaw = true;
            }
        }

        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        {
            if (player)
            {
                double pixelstretch = level.info ? level.info->pixelstretch : 1.2;

                //Weapon firing tracking - Thanks Fishbiter for the inspiration of how/where to use this!
                {
                    player->mo->OverrideAttackPosDir = true;

                    player->mo->AttackPos.X = player->mo->X() - (weaponoffset[0] * vr_vunits_per_meter);
                    player->mo->AttackPos.Y = player->mo->Y() - (weaponoffset[2] * vr_vunits_per_meter);
                    player->mo->AttackPos.Z = player->mo->Z() + (((hmdPosition[1] + weaponoffset[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch);

                    player->mo->AttackDir = MapAttackDir;
                }

                if (vr_teleport) {

                    DAngle yaw((doomYaw - hmdorientation[YAW]) + offhandangles[YAW]);
                    DAngle pitch(offhandangles[PITCH]);
                    double pixelstretch = level.info ? level.info->pixelstretch : 1.2;

                    // Teleport Logic
                    if (ready_teleport) {
                        FLineTraceData trace;
                        if (P_LineTrace(player->mo, yaw, 8192, pitch, TRF_ABSOFFSET|TRF_BLOCKUSE|TRF_BLOCKSELF|TRF_SOLIDACTORS,
                                        ((hmdPosition[1] + offhandoffset[1] + vr_height_adjust) *
                                         vr_vunits_per_meter) / pixelstretch,
                                        -(offhandoffset[2] * vr_vunits_per_meter),
                                        -(offhandoffset[0] * vr_vunits_per_meter), &trace))
                        {
                            m_TeleportTarget = trace.HitType;
                            m_TeleportLocation = trace.HitLocation;
                        } else {
                            m_TeleportTarget = TRACE_HitNone;
                            m_TeleportLocation = DVector3(0, 0, 0);
                        }
                    }
                    else if (trigger_teleport && m_TeleportTarget == TRACE_HitFloor) {
                        auto vel = player->mo->Vel;
                        player->mo->Vel = DVector3(m_TeleportLocation.X - player->mo->X(),
                                                   m_TeleportLocation.Y - player->mo->Y(), 0);
                        bool wasOnGround = player->mo->Z() <= player->mo->floorz;
                        double oldZ = player->mo->Z();
                        P_XYMovement(player->mo, DVector2(0, 0));

                        //if we were on the ground before offsetting, make sure we still are (this fixes not being able to move on lifts)
                        if (player->mo->Z() >= oldZ && wasOnGround) {
                            player->mo->SetZ(player->mo->floorz);
                        } else {
                            player->mo->SetZ(oldZ);
                        }
                        player->mo->Vel = vel;
                    }

                    trigger_teleport = false;
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
        double hmdYawDeltaDegrees;
        {
            static double previousHmdYaw = 0;
            static bool havePreviousYaw = false;
            if (!havePreviousYaw) {
                previousHmdYaw = yaw;
                havePreviousYaw = true;
            }
            hmdYawDeltaDegrees = yaw - previousHmdYaw;
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

        if (gamestate == GS_LEVEL && !isMenuActive())
        {
            doomYaw += hmdYawDeltaDegrees;
        }

        //Set all the render angles - including roll
        {
            //Always update roll (as the game tic cmd doesn't support roll
            GLRenderer->mAngles.Roll = roll;
            GLRenderer->mAngles.Pitch = pitch;

            double viewYaw = doomYaw;
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
    }

} /* namespace s3d */


