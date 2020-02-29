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

extern "C" {
#include <VrApi.h>
#include <VrApi_Helpers.h>
}

EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_floor_offset)

//EXTERN_CVAR(Bool, oculusquest_rightHanded)
//EXTERN_CVAR(Bool, oculusquest_moveFollowsOffHand)
//EXTERN_CVAR(Bool, oculusquest_drawControllers)
//EXTERN_CVAR(Float, oculusquest_weaponRotate);
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
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;
    }

/* virtual */
    VSMatrix OculusQuestEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
    {
        // Ignore those arguments and get the projection from the SDK
        // VSMatrix vs1 = ShiftedEyePose::GetProjection(fov, aspectRatio, fovRatio);
        return projectionMatrix;
    }

    void OculusQuestEyePose::initialize()
    {
        float zNear = 5.0;
        float zFar = 65536.0;
/*        HmdMatrix44_t projection = vrsystem->GetProjectionMatrix(
                EVREye(eye), zNear, zFar);
        HmdMatrix44_t proj_transpose;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                proj_transpose.m[i][j] = projection.m[j][i];
            }
        }*/
        projectionMatrix.loadIdentity();
        //projectionMatrix.multMatrix(&proj_transpose.m[0][0]);
    }

    void OculusQuestEyePose::dispose()
    {
    }

    bool OculusQuestEyePose::submitFrame() const
    {
        // Copy HDR game texture to local vr LDR framebuffer, so gamma correction could work
/*        if (eyeTexture->handle == nullptr) {
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

            GLuint texture;
            glGenTextures(1, &texture);
            eyeTexture->handle = (void *)(std::ptrdiff_t)texture;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GLRenderer->mSceneViewport.width,
                         GLRenderer->mSceneViewport.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
            glDrawBuffers(1, drawBuffers);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return false;
        GLRenderer->mBuffers->BindEyeTexture(eye, 0);
        GL_IRECT box = {0, 0, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height};
        GLRenderer->DrawPresentTexture(box, true);

        // Maybe this would help with AMD boards?
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
*/

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
            float openVrRollDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[2]);
            new_projection.rotate(-openVrRollDegrees, 0, 0, 1);

            if (doFixPitch) {
                float openVrPitchDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[1]);
                new_projection.rotate(-openVrPitchDegrees, 1, 0, 0);
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

        VSMatrix proj(activeEye->projectionMatrix);
        proj.multMatrix(new_projection);
        new_projection = proj;

        return new_projection;
    }

    void OculusQuestEyePose::AdjustHud() const
    {
        // Draw crosshair on a separate quad, before updating HUD matrix
        const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
        if (mode3d->IsMono())
            return;
        const OculusQuestMode * openVrMode = static_cast<const OculusQuestMode *>(mode3d);
        if (openVrMode
            && openVrMode->crossHairDrawer
            // Don't draw the crosshair if there is none
            && CrosshairImage != NULL
            && gamestate != GS_TITLELEVEL
            && r_viewpoint.camera->health > 0)
        {
            const float crosshair_distance_meters = 10.0f; // meters
            const float crosshair_width_meters = 0.2f * crosshair_distance_meters;
            gl_RenderState.mProjectionMatrix = getQuadInWorld(
                    crosshair_distance_meters,
                    crosshair_width_meters,
                    false,
                    0.0);
            gl_RenderState.ApplyMatrices();
            openVrMode->crossHairDrawer->Draw();
        }

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

    OculusQuestMode::OculusQuestMode()
            : leftEyeView(0)
            , rightEyeView(1)
            , sceneWidth(0), sceneHeight(0)
            , crossHairDrawer(new F2DDrawer)
            , cached2DDrawer(nullptr)
    {
        eye_ptrs.Push(&leftEyeView); // initially default behavior to Mono non-stereo rendering


        //Get this from my code
        //vrSystem->GetRecommendedRenderTargetSize(&sceneWidth, &sceneHeight);

        leftEyeView.initialize();
        rightEyeView.initialize();

        eye_ptrs.Push(&rightEyeView); // NOW we render to two eyes

        crossHairDrawer->Clear();
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
        long oculusquest_rightHanded = 1;
        if (GetHandTransform(oculusquest_rightHanded ? 1 : 0, out))
        {
            //out->rotate(oculusquest_weaponRotate, 1, 0, 0);
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

    }

    void OculusQuestMode::updateHmdPose(
            double hmdYawRadians,
            double hmdPitchRadians,
            double hmdRollRadians) const
    {
        hmdYaw = hmdYawRadians;
        double hmdpitch = hmdPitchRadians;
        double hmdroll = hmdRollRadians;

        double hmdYawDelta = 0;
        {
            // Set HMD angle game state parameters for NEXT frame
            static double previousHmdYaw = 0;
            static bool havePreviousYaw = false;
            if (!havePreviousYaw) {
                previousHmdYaw = hmdYaw;
                havePreviousYaw = true;
            }
            hmdYawDelta = hmdYaw - previousHmdYaw;
//            G_AddViewAngle(mAngleFromRadians(-hmdYawDelta));
            previousHmdYaw = hmdYaw;
        }

        /* */
        // Pitch
        {
            double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
            double hmdPitchInDoom = -atan(tan(hmdpitch) / pixelstretch);
            double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
            double dPitch =
                    // hmdPitchInDoom
                    -hmdpitch
                    - viewPitchInDoom;
//            G_AddViewPitch(mAngleFromRadians(-dPitch));
        }

        // Roll can be local, because it doesn't affect gameplay.
        GLRenderer->mAngles.Roll = RAD2DEG(-hmdroll);
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

        if (gamestate == GS_LEVEL) {
            cachedScreenBlocks = screenblocks;
            screenblocks = 12; // always be full-screen during 3D scene render
            setUseScreenLayer(false);
        }
        else {
            //Ensure we are drawing on virtual screen
            setUseScreenLayer(true);
        }

        //Get controller state here

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
*/    }

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


