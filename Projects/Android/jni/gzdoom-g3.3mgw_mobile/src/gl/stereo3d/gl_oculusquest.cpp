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
//EXTERN_CVAR(Bool, oculusquest_drawControllers)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_snapTurn);
//EXTERN_CVAR(Float, oculusquest_weaponScale);

CVAR(Float, oculusquest_kill_momentum, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
const float DEAD_ZONE = 0.25f;


static int axisTrackpad = -1;
static int axisJoystick = -1;
static int axisTrigger = -1;
static bool identifiedAxes = false;

DVector3 oculusquest_dpos(0,0,0);
DAngle oculusquest_to_doom_angle;

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
        doomYawDegrees = yaw;
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

        // Pitch and Roll are identical between vr and Doom worlds.
        // But yaw can differ, depending on starting state, and controller movement.
        float doomYawDegrees = yaw;
        deltaYawDegrees = doomYawDegrees - hmdorientation[YAW];
        while (deltaYawDegrees > 180)
            deltaYawDegrees -= 360;
        while (deltaYawDegrees < -180)
            deltaYawDegrees += 360;

        DAngle vr_to_doom_angle = DAngle(-deltaYawDegrees);

        const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
        if (mode3d->IsMono())
            return;
        const OculusQuestMode  * vrMode = static_cast<const OculusQuestMode *>(mode3d);

        ovrTracking2 tracking;
        vrMode->getTracking(&tracking);

        /*
        // extract rotation component from hmd transform
        LSMatrix44 vr_X_hmd(hmdPose);
        LSMatrix44 hmdRot = vr_X_hmd.getWithoutTranslation(); // .transpose();

        /// In these eye methods, just get local inter-eye stereoscopic shift, not full position shift ///

        // compute local eye shift
        LSMatrix44 eyeShift2;
        eyeShift2.loadIdentity();
        eyeShift2 = eyeShift2 * eyeToHeadTransform; // eye to head
        eyeShift2 = eyeShift2 * hmdRot; // head to vr

        LSVec3 eye_EyePos = LSVec3(0, 0, 0); // eye position in eye frame
        LSVec3 hmd_EyePos = LSMatrix44(eyeToHeadTransform) * eye_EyePos;
        LSVec3 hmd_HmdPos = LSVec3(0, 0, 0); // hmd position in hmd frame
        LSVec3 vr_EyePos = vr_X_hmd * hmd_EyePos;
        LSVec3 vr_HmdPos = vr_X_hmd * hmd_HmdPos;
        LSVec3 hmd_OtherEyePos = LSMatrix44(otherEyeToHeadTransform) * eye_EyePos;
        LSVec3 vr_OtherEyePos = vr_X_hmd * hmd_OtherEyePos;
        LSVec3 vr_EyeOffset = vr_EyePos - vr_HmdPos;

        VSMatrix doomInvr = VSMatrix();
        doomInvr.loadIdentity();
        // permute axes
        float permute[] = { // Convert from vr to Doom axis convention, including mirror inversion
                -1,  0,  0,  0, // X-right in vr -> X-left in Doom
                0,  0,  1,  0, // Z-backward in vr -> Y-backward in Doom
                0,  1,  0,  0, // Y-up in vr -> Z-up in Doom
                0,  0,  0,  1};
        doomInvr.multMatrix(permute);
        doomInvr.scale(vr_vunits_per_meter, vr_vunits_per_meter, vr_vunits_per_meter); // Doom units are not meters
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        doomInvr.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio
        doomInvr.rotate(deltaYawDegrees, 0, 0, 1);

        LSVec3 doom_EyeOffset = LSMatrix44(doomInvr) * vr_EyeOffset;

        if (doTrackHmdVerticalPosition) {
            // In vr, the real world floor level is at y==0
            // In Doom, the virtual player foot level is viewheight below the current viewpoint (on the Z axis)
            // We want to align those two heights here
            const player_t & player = players[consoleplayer];
            double vh = player.viewheight; // Doom thinks this is where you are
            double hh = (vr_X_hmd[1][3] - vr_floor_offset) * vr_vunits_per_meter; // HMD is actually here
            doom_EyeOffset[2] += hh - vh;
            // TODO: optionally allow player to jump and crouch by actually jumping and crouching
        }

        if (doTrackHmdHorizontalPosition) {
            // shift viewpoint when hmd position shifts
            static bool is_initial_origin_set = false;
            if (! is_initial_origin_set) {
                // initialize origin to first noted HMD location
                // TODO: implement recentering based on a CCMD
                vr_origin = vr_HmdPos;
                is_initial_origin_set = true;
            }
            vr_dpos = vr_HmdPos - vr_origin;

            LSVec3 doom_dpos = LSMatrix44(doomInvr) * vr_dpos;
            doom_EyeOffset[0] += doom_dpos[0];
            doom_EyeOffset[1] += doom_dpos[1];
        }

        outViewShift[0] = doom_EyeOffset[0];
        outViewShift[1] = doom_EyeOffset[1];
        outViewShift[2] = doom_EyeOffset[2];
         */
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

        float scale = 0.00125f * 1.0;//oculusquest_weaponScale;
        gl_RenderState.mModelMatrix.scale(scale, -scale, scale);
        gl_RenderState.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f);

        gl_RenderState.EnableModelMatrix(true);
    }

    void OculusQuestMode::UnAdjustPlayerSprites() const {

        gl_RenderState.EnableModelMatrix(false);
    }

    void OculusQuestMode::AdjustCrossHair() const
    {
        cached2DDrawer = GLRenderer->m2DDrawer;
        // Remove effect of screenblocks setting on crosshair position
        cachedViewheight = viewheight;
        cachedViewwindowy = viewwindowy;
        viewheight = SCREENHEIGHT;
        viewwindowy = 0;

        if (crossHairDrawer != nullptr) {
            // Hijack 2D drawing to our local crosshair drawer
            crossHairDrawer->Clear();
            GLRenderer->m2DDrawer = crossHairDrawer;
        }
    }

    void OculusQuestMode::UnAdjustCrossHair() const
    {
        viewheight = cachedViewheight;
        viewwindowy = cachedViewwindowy;
        if (cached2DDrawer)
            GLRenderer->m2DDrawer = cached2DDrawer;
        cached2DDrawer = nullptr;
    }

    bool OculusQuestMode::GetHandTransform(int hand, VSMatrix* mat) const
    {
        /*
        if (controllers[hand].active)
        {
            mat->loadIdentity();

            APlayerPawn* playermo = r_viewpoint.camera->player->mo;
            DVector3 pos = playermo->InterpolatedPosition(r_viewpoint.TicFrac);

            mat->translate(pos.X, pos.Z, pos.Y);

            mat->scale(vr_vunits_per_meter, vr_vunits_per_meter, -vr_vunits_per_meter);

            mat->rotate(-deltaYawDegrees - 180, 0, 1, 0);

            mat->translate(-oculusquest_origin.x, -vr_floor_offset, -oculusquest_origin.z);

            LSMatrix44 handToAbs;
            vSMatrixFromHmdMatrix34(handToAbs, controllers[hand].pose.mDeviceToAbsoluteTracking);

            mat->multMatrix(handToAbs.transpose());

            return true;
        }
         */
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

/*    static DVector3 MapAttackDir(AActor* actor, DAngle yaw, DAngle pitch)
    {
        LSMatrix44 mat;
        if (!s3d::Stereo3DMode::getCurrentMode().GetWeaponTransform(&mat))
        {
            double pc = pitch.Cos();

            DVector3 direction = { pc * yaw.Cos(), pc * yaw.Sin(), -pitch.Sin() };
            return direction;
        }
        double pc = pitch.Cos();

        DVector3 refdirection = { pc * yaw.Cos(), pc * yaw.Sin(), -pitch.Sin() };

        yaw -= actor->Angles.Yaw;

        //ignore specified pitch (would need to compensate for auto aim and no (vanilla) Doom weapon varies this)
        //pitch -= actor->Angles.Pitch;
        pitch.Degrees = 0;

        pc = pitch.Cos();

        DVector3 local = { (float)(pc * yaw.Cos()), (float)(pc * yaw.Sin()), (float)(-pitch.Sin()), 0.0f };

        DVector3 dir;
        dir.X = local.x * -mat[2][0] + local.y * -mat[0][0] + local.z * -mat[1][0];
        dir.Y = local.x * -mat[2][2] + local.y * -mat[0][2] + local.z * -mat[1][2];
        dir.Z = local.x * -mat[2][1] + local.y * -mat[0][1] + local.z * -mat[1][1];
        dir.MakeUnit();

        return dir;
    }
*/


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


/*    void Joy_GenerateUIButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys)
    {
        int changed = oldbuttons ^ newbuttons;
        if (changed != 0)
        {
            event_t ev = { 0, 0, 0, 0, 0, 0, 0 };
            int mask = 1;
            for (int j = 0; j < numbuttons; mask <<= 1, ++j)
            {
                if (changed & mask)
                {
                    ev.data1 = keys[j];
                    ev.type = EV_GUI_Event;
                    ev.subtype = (newbuttons & mask) ? EV_GUI_KeyDown : EV_GUI_KeyUp;
                    D_PostEvent(&ev);
                }
            }
        }
    }

    static void HandleVRAxis(VRControllerState_t& lastState, VRControllerState_t& newState, int vrAxis, int axis, int negativedoomkey, int positivedoomkey, int base)
    {
        int keys[] = { negativedoomkey + base, positivedoomkey + base };
        Joy_GenerateButtonEvents(GetVRAxisState(lastState, vrAxis, axis), GetVRAxisState(newState, vrAxis, axis), 2, keys);
    }

    static void HandleUIVRAxis(VRControllerState_t& lastState, VRControllerState_t& newState, int vrAxis, int axis, ESpecialGUIKeys negativedoomkey, ESpecialGUIKeys positivedoomkey)
    {
        int keys[] = { (int)negativedoomkey, (int)positivedoomkey };
        Joy_GenerateUIButtonEvents(GetVRAxisState(lastState, vrAxis, axis), GetVRAxisState(newState, vrAxis, axis), 2, keys);
    }

    static void HandleUIVRAxes(VRControllerState_t& lastState, VRControllerState_t& newState, int vrAxis,
                               ESpecialGUIKeys xnegativedoomkey, ESpecialGUIKeys xpositivedoomkey, ESpecialGUIKeys ynegativedoomkey, ESpecialGUIKeys ypositivedoomkey)
    {
        int oldButtons = abs(lastState.rAxis[vrAxis].x) > abs(lastState.rAxis[vrAxis].y)
                         ? GetVRAxisState(lastState, vrAxis, 0)
                         : GetVRAxisState(lastState, vrAxis, 1) << 2;
        int newButtons = abs(newState.rAxis[vrAxis].x) > abs(newState.rAxis[vrAxis].y)
                         ? GetVRAxisState(newState, vrAxis, 0)
                         : GetVRAxisState(newState, vrAxis, 1) << 2;

        int keys[] = { xnegativedoomkey, xpositivedoomkey, ynegativedoomkey, ypositivedoomkey };

        Joy_GenerateUIButtonEvents(oldButtons, newButtons, 4, keys);
    }

    static void HandleVRButton(VRControllerState_t& lastState, VRControllerState_t& newState, long long vrindex, int doomkey, int base)
    {
        Joy_GenerateButtonEvents((lastState.ulButtonPressed & (1LL << vrindex)) ? 1 : 0, (newState.ulButtonPressed & (1LL << vrindex)) ? 1 : 0, 1, doomkey + base);
    }

    static void HandleUIVRButton(VRControllerState_t& lastState, VRControllerState_t& newState, long long vrindex, int doomkey)
    {
        Joy_GenerateUIButtonEvents((lastState.ulButtonPressed & (1LL << vrindex)) ? 1 : 0, (newState.ulButtonPressed & (1LL << vrindex)) ? 1 : 0, 1, &doomkey);
    }

    static void HandleControllerState(int device, int role, VRControllerState_t& newState)
    {
        VRControllerState_t& lastState = controllers[role].lastState;

        //trigger (swaps with handedness)
        int controller = oculusquest_rightHanded ? role : 1 - role;

        if (CurrentMenu == nullptr) //the quit menu is cancelled by any normal keypress, so don't generate the fire while in menus
        {
            HandleVRAxis(lastState, newState, 1, 0, KEY_JOY4, KEY_JOY4, controller * (KEY_PAD_RTRIGGER - KEY_JOY4));
        }
        HandleUIVRAxis(lastState, newState, 1, 0, GK_RETURN, GK_RETURN);

        //touchpad
        if (axisTrackpad != -1)
        {
            HandleVRAxis(lastState, newState, axisTrackpad, 0, KEY_PAD_LTHUMB_LEFT, KEY_PAD_LTHUMB_RIGHT, role * (KEY_PAD_RTHUMB_LEFT - KEY_PAD_LTHUMB_LEFT));
            HandleVRAxis(lastState, newState, axisTrackpad, 1, KEY_PAD_LTHUMB_DOWN, KEY_PAD_LTHUMB_UP, role * (KEY_PAD_RTHUMB_DOWN - KEY_PAD_LTHUMB_UP));
            HandleUIVRAxes(lastState, newState, axisTrackpad, GK_LEFT, GK_RIGHT, GK_DOWN, GK_UP);
        }

        //WMR joysticks
        if (axisJoystick != -1)
        {
            HandleVRAxis(lastState, newState, axisJoystick, 0, KEY_JOYAXIS1MINUS, KEY_JOYAXIS1PLUS, role * (KEY_JOYAXIS3PLUS - KEY_JOYAXIS1PLUS));
            HandleVRAxis(lastState, newState, axisJoystick, 1, KEY_JOYAXIS2MINUS, KEY_JOYAXIS2PLUS, role * (KEY_JOYAXIS3PLUS - KEY_JOYAXIS1PLUS));
            HandleUIVRAxes(lastState, newState, axisJoystick, GK_LEFT, GK_RIGHT, GK_DOWN, GK_UP);
        }

        HandleVRButton(lastState, newState, vr::k_EButton_Grip, KEY_PAD_LSHOULDER, role * (KEY_PAD_RSHOULDER - KEY_PAD_LSHOULDER));
        HandleUIVRButton(lastState, newState, vr::k_EButton_Grip, GK_BACK);
        HandleVRButton(lastState, newState, vr::k_EButton_ApplicationMenu, KEY_PAD_START, role * (KEY_PAD_BACK - KEY_PAD_START));

        //Extra controls for rift
        HandleVRButton(lastState, newState, vr::k_EButton_A, KEY_PAD_A, role * (KEY_PAD_B - KEY_PAD_A));
        HandleVRButton(lastState, newState, vr::k_EButton_SteamVR_Touchpad, KEY_PAD_X, role * (KEY_PAD_Y - KEY_PAD_X));

        lastState = newState;
    }


    VRControllerState_t& OculusQuest_GetState(int hand)
    {
        int controller = oculusquest_rightHanded ? hand : 1 - hand;
        return controllers[controller].lastState;
    }


    int OculusQuest_GetTouchPadAxis()
    {
        return axisTrackpad;
    }

    int OculusQuest_GetJoystickAxis()
    {
        return axisJoystick;
    }

    bool OculusQuest_OnHandIsRight()
    {
        return oculusquest_rightHanded;
    }


    static inline int joyint(double val)
    {
        if (val >= 0)
        {
            return int(ceil(val));
        }
        else
        {
            return int(floor(val));
        }
    }

    bool JustStoppedMoving(VRControllerState_t& lastState, VRControllerState_t& newState, int axis)
    {
        if (axis != -1)
        {
            bool wasMoving = (abs(lastState.rAxis[axis].x) > DEAD_ZONE || abs(lastState.rAxis[axis].y) > DEAD_ZONE);
            bool isMoving = (abs(newState.rAxis[axis].x) > DEAD_ZONE || abs(newState.rAxis[axis].y) > DEAD_ZONE);
            return !isMoving && wasMoving;
        }
        return false;
    }
*/
/* virtual */
    void OculusQuestMode::SetUp() const
    {
        super::SetUp();

        // Set VR-appropriate settings
        const bool doAdjustVrSettings = true;
        if (doAdjustVrSettings) {
            movebob = 0;
            gl_billboard_faces_camera = true;
            if (gl_multisample < 2)
                gl_multisample = 4;
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
        vr_walkdirection = !vr_moveFollowsOffHand; //FIX THIS!
        doomYawDegrees = GLRenderer->mAngles.Yaw.Degrees;
        getTrackedRemotesOrientation(vr_control_scheme);

/*        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        {
            LSMatrix44 mat;
            if (player)
            {
                if (GetWeaponTransform(&mat))
                {
                    player->mo->OverrideAttackPosDir = true;

                    player->mo->AttackPos.X = mat[3][0];
                    player->mo->AttackPos.Y = mat[3][2];
                    player->mo->AttackPos.Z = mat[3][1];

                    player->mo->AttackDir = MapAttackDir;
                }
                if (GetHandTransform(oculusquest_rightHanded ? 0 : 1, &mat) && oculusquest_moveFollowsOffHand)
                {
                    player->mo->ThrustAngleOffset = DAngle(RAD2DEG(atan2f(-mat[2][2], -mat[2][0]))) - player->mo->Angles.Yaw;
                }
                else
                {
                    player->mo->ThrustAngleOffset = 0.0f;
                }
                auto vel = player->mo->Vel;
                player->mo->Vel = DVector3((DVector2(-oculusquest_dpos.x, oculusquest_dpos.z) * vr_vunits_per_meter).Rotated(oculusquest_to_doom_angle), 0);
                bool wasOnGround = player->mo->Z() <= player->mo->floorz;
                float oldZ = player->mo->Z();
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
                oculusquest_origin += oculusquest_dpos;
            }
        }
*/
  /*      //To feel smooth, yaw changes need to accumulate over the (sub) tic (i.e. render frame, not per tic)
        unsigned int time = I_FPSTime();
        static unsigned int lastTime = time;

        unsigned int delta = time - lastTime;
        lastTime = time;

        //G_AddViewAngle(joyint(-1280 * I_OculusQuestGetYaw() * delta * 30 / 1000));
*/

        //Always update roll (as the game tic cmd doesn't support roll
        GLRenderer->mAngles.Roll = hmdorientation[ROLL];

        //updateHmdPose();
    }


    void OculusQuestMode::updateHmdPose() const
    {
        double hmdYawDeltaDegrees = 0;
        {
            static double previousHmdYaw = 0;
            static bool havePreviousYaw = false;
            if (!havePreviousYaw) {
                previousHmdYaw = hmdorientation[YAW];
                havePreviousYaw = true;
            }
            hmdYawDeltaDegrees = hmdorientation[YAW] - previousHmdYaw;
            G_AddViewAngle(mAngleFromRadians(DEG2RAD(-hmdYawDeltaDegrees)));
            previousHmdYaw = hmdorientation[YAW];
        }

        /* */
        // Pitch
        {
            double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
            double dPitch =
                    - DEG2RAD(hmdorientation[PITCH])
                    - viewPitchInDoom;
            G_AddViewPitch(mAngleFromRadians(dPitch));
        }

        {
            GLRenderer->mAngles.Pitch = hmdorientation[PITCH];

            double viewYaw = r_viewpoint.Angles.Yaw.Degrees - hmdYawDeltaDegrees;
            while (viewYaw <= -180.0)
                viewYaw += 360.0;
            while (viewYaw > 180.0)
                viewYaw -= 360.0;
            r_viewpoint.Angles.Yaw = viewYaw;
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


