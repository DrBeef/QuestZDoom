//
//---------------------------------------------------------------------------
//
// Copyright(C) 2016-2017 Christopher Bruns
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
** gl_openvr.cpp
** Stereoscopic virtual reality mode for the HTC Vive headset
**
*/

#ifdef USE_OPENVR

#include "gl_openvr.h"
#include "openvr_capi.h"
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
#include "LSMatrix.h"
#include "w_wad.h"
#include "m_joy.h"
#include "d_gui.h"
#include "d_event.h"

void I_StartupOpenVR();
double P_XYMovement(AActor *mo, DVector2 scroll);
float I_OpenVRGetYaw();

extern class DMenu *CurrentMenu;

#ifdef DYN_OPENVR
// Dynamically load OpenVR

#include "i_module.h"
FModule OpenVRModule{ "OpenVR" };

/** Pointer-to-function type, useful for dynamically getting OpenVR entry points. */
// Derived from global entry at the bottom of openvr_capi.h, plus a few other functions
typedef intptr_t (*LVR_InitInternal)(EVRInitError *peError, EVRApplicationType eType);
typedef void (*LVR_ShutdownInternal)();
typedef bool (*LVR_IsHmdPresent)();
typedef intptr_t (*LVR_GetGenericInterface)(const char *pchInterfaceVersion, EVRInitError *peError);
typedef bool (*LVR_IsRuntimeInstalled)();
typedef const char * (*LVR_GetVRInitErrorAsSymbol)(EVRInitError error);
typedef const char * (*LVR_GetVRInitErrorAsEnglishDescription)(EVRInitError error);
typedef bool (*LVR_IsInterfaceVersionValid)(const char * version);
typedef uint32_t (*LVR_GetInitToken)();

#define DEFINE_ENTRY(name) static TReqProc<OpenVRModule, L##name> name{#name};
DEFINE_ENTRY(VR_InitInternal)
DEFINE_ENTRY(VR_ShutdownInternal)
DEFINE_ENTRY(VR_IsHmdPresent)
DEFINE_ENTRY(VR_GetGenericInterface)
DEFINE_ENTRY(VR_IsRuntimeInstalled)
DEFINE_ENTRY(VR_GetVRInitErrorAsSymbol)
DEFINE_ENTRY(VR_GetVRInitErrorAsEnglishDescription)
DEFINE_ENTRY(VR_IsInterfaceVersionValid)
DEFINE_ENTRY(VR_GetInitToken)

#ifdef _WIN32
#define OPENVRLIB "openvr_api.dll"
#elif defined(__APPLE__)
#define OPENVRLIB "libopenvr_api.dylib"
#else
#define OPENVRLIB "libopenvr_api.so"
#endif

#else
// Non-dynamic loading of OpenVR

// OpenVR Global entry points
S_API intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsRuntimeInstalled();
S_API const char * VR_GetVRInitErrorAsSymbol(EVRInitError error);
S_API const char * VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);
S_API bool VR_IsInterfaceVersionValid(const char * version);
S_API uint32_t VR_GetInitToken();

#endif

EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_floor_offset)

EXTERN_CVAR(Bool, openvr_rightHanded)
EXTERN_CVAR(Bool, openvr_moveFollowsOffHand)
EXTERN_CVAR(Bool, openvr_drawControllers)
EXTERN_CVAR(Float, openvr_weaponRotate);
EXTERN_CVAR(Float, openvr_weaponScale);

CVAR(Float, openvr_kill_momentum, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
const float DEAD_ZONE = 0.25f;


bool IsOpenVRPresent()
{
#ifndef USE_OPENVR
	return false;
#elif !defined DYN_OPENVR
	return true;
#else
	static bool cached_result = false;
	static bool done = false;

	if (!done)
	{
		done = true;
		cached_result = OpenVRModule.Load({ NicePath("$PROGDIR/" OPENVRLIB), OPENVRLIB });
	}
	return cached_result;
#endif
}

// feature toggles, for testing and debugging
static const bool doTrackHmdYaw = true;
static const bool doTrackHmdPitch = true;
static const bool doTrackHmdRoll = true;
static const bool doLateScheduledRotationTracking = true;
static const bool doStereoscopicViewpointOffset = true;
static const bool doRenderToDesktop = true; // mirroring to the desktop is very helpful for debugging
static const bool doRenderToHmd = true;
static const bool doTrackHmdVerticalPosition = true;
static const bool doTrackHmdHorizontalPosition = true;
static const bool doTrackVrControllerPosition = false; // todo:

static int axisTrackpad = -1;
static int axisJoystick = -1;
static int axisTrigger = -1;
static bool identifiedAxes = false;

LSVec3 openvr_dpos(0,0,0);
DAngle openvr_to_doom_angle;

namespace s3d 
{
	static LSVec3 openvr_origin(0, 0, 0);
	static float deltaYawDegrees;
	
	class FControllerTexture : public FTexture
	{
	public:
		FControllerTexture(RenderModel_TextureMap_t* tex) : FTexture()
		{
			m_pTex = tex;
			Width = m_pTex->unWidth;
			Height = m_pTex->unHeight;
		}

		const uint8_t *GetColumn(unsigned int column, const Span **spans_out)
		{
			return nullptr;
		}
		const uint8_t *GetPixels()
		{
			return m_pTex->rubTextureMapData;
		}

		RenderModel_TextureMap_t* m_pTex;
	};

class VRControllerModel : public FModel
{
public:
	enum LoadState {
		LOADSTATE_INITIAL,
		LOADSTATE_LOADING_VERTICES,
		LOADSTATE_LOADING_TEXTURE,
		LOADSTATE_LOADED,
		LOADSTATE_ERROR
	};

	VRControllerModel(const std::string& model_name, VR_IVRRenderModels_FnTable * vrRenderModels)
		: loadState(LOADSTATE_INITIAL)
		, modelName(model_name)
		, vrRenderModels(vrRenderModels)
	{
		if (!vrRenderModels) {
			loadState = LOADSTATE_ERROR;
			return;
		}
		isLoaded();
	}
	VRControllerModel() {}

	// FModel methods

	virtual bool Load(const char * fn, int lumpnum, const char * buffer, int length) override {
		return false;
	}

	// Controller models don't have frames so always return 0
	virtual int FindFrame(const char * name) override {
		return 0;
	}

	virtual void RenderFrame(FTexture * skin, int frame, int frame2, double inter, int translation = 0)  override
	{
		if (!isLoaded())
			return;
		FMaterial * tex = FMaterial::ValidateTexture(pFTex, false);
		mVBuf->SetupFrame(0, 0, 0);
		gl_RenderState.SetVertexBuffer(mVBuf);
		gl_RenderState.SetMaterial(tex, CLAMP_NONE, translation, -1, false);
		gl_RenderState.Apply();
		glDrawElements(GL_TRIANGLES, pModel->unTriangleCount * 3, GL_UNSIGNED_INT, (void*)(intptr_t)0);
		gl_RenderState.SetVertexBuffer(GLRenderer->mVBO); //this needs to be set back to avoid the level rendering black even though the next draw will be the UI for this eye(???)
	}

	virtual void BuildVertexBuffer() override
	{
		if (loadState != LOADSTATE_LOADED)
			return;
		if (mVBuf != NULL)
			return;

		mVBuf = new FModelVertexBuffer(true, true);
		FModelVertex *vertptr = mVBuf->LockVertexBuffer(pModel->unVertexCount);
		unsigned int *indxptr = mVBuf->LockIndexBuffer(pModel->unTriangleCount * 3);

		for (int v = 0; v < pModel->unVertexCount; ++v)
		{
			const RenderModel_Vertex_t & vd = pModel->rVertexData[v];
			vertptr[v].x = vd.vPosition.v[0];
			vertptr[v].y = vd.vPosition.v[1];
			vertptr[v].z = vd.vPosition.v[2];
			vertptr[v].u = vd.rfTextureCoord[0];
			vertptr[v].v = vd.rfTextureCoord[1];
			vertptr[v].SetNormal(
				vd.vNormal.v[0],
				vd.vNormal.v[1],
				vd.vNormal.v[2]);
		}
		for (int i = 0; i < pModel->unTriangleCount * 3; ++i)
		{
			indxptr[i] = pModel->rIndexData[i];
		}

		mVBuf->UnlockVertexBuffer();
		mVBuf->UnlockIndexBuffer();
	}

	virtual void AddSkins(uint8_t *hitlist) override 
	{
	
	}

	bool isLoaded()
	{
		if (loadState == LOADSTATE_ERROR)
			return false;
		if (loadState == LOADSTATE_LOADED)
			return true;
		if ((loadState == LOADSTATE_INITIAL) || (loadState == LOADSTATE_LOADING_VERTICES))
		{
			// Load vertex data first
			EVRRenderModelError eError = vrRenderModels->LoadRenderModel_Async(const_cast<char *>(modelName.c_str()), &pModel);
			if (eError == EVRRenderModelError_VRRenderModelError_Loading) {
				loadState = LOADSTATE_LOADING_VERTICES;
				return false;
			}
			else if (eError == EVRRenderModelError_VRRenderModelError_None) {
				loadState = LOADSTATE_LOADING_TEXTURE;
				vrRenderModels->LoadTexture_Async(pModel->diffuseTextureId, &pTexture);
			}
			else {
				loadState = LOADSTATE_ERROR;
				return false;
			}
		}
		// Load texture data second
		EVRRenderModelError eError = vrRenderModels->LoadTexture_Async(pModel->diffuseTextureId, &pTexture);
		if (eError == EVRRenderModelError_VRRenderModelError_Loading) {
			return false; // No change, and not done, still loading texture
		}
		if (eError == EVRRenderModelError_VRRenderModelError_None) {
			loadState = LOADSTATE_LOADED;

			pFTex = new FControllerTexture(pTexture);
			BuildVertexBuffer();

			return true;
		}
		loadState = LOADSTATE_ERROR;
		return false;
	}

private:
	RenderModel_t * pModel;
	RenderModel_TextureMap_t * pTexture;
	FTexture* pFTex;
	LoadState loadState;
	std::string modelName;
	VR_IVRRenderModels_FnTable * vrRenderModels;

};

static std::map<std::string, VRControllerModel> controllerMeshes;
struct Controller
{
	bool active = false;
	TrackedDevicePose_t pose;
	VRControllerState_t lastState;
	VRControllerModel* model = nullptr;
};

enum { MAX_ROLES = 2 };
Controller controllers[MAX_ROLES];


/* static */
const Stereo3DMode& OpenVRMode::getInstance()
{
		static OpenVRMode instance;
		if (! instance.hmdWasFound)
			return  MonoView::getInstance();
		return instance;
}

static HmdVector3d_t eulerAnglesFromQuat(HmdQuaternion_t quat) {
	double q0 = quat.w;
	// permute axes to make "Y" up/yaw
	double q2 = quat.x;
	double q3 = quat.y;
	double q1 = quat.z;

	// http://stackoverflow.com/questions/18433801/converting-a-3x3-matrix-to-euler-tait-bryan-angles-pitch-yaw-roll
	double roll = atan2(2 * (q0*q1 + q2*q3), 1 - 2 * (q1*q1 + q2*q2));
	double pitch = asin(2 * (q0*q2 - q3*q1));
	double yaw = atan2(2 * (q0*q3 + q1*q2), 1 - 2 * (q2*q2 + q3*q3));

	return HmdVector3d_t{ yaw, pitch, roll };
}

static HmdQuaternion_t quatFromMatrix(HmdMatrix34_t matrix) {
	HmdQuaternion_t q;
	typedef float f34[3][4];
	f34& a = matrix.m;
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	float trace = a[0][0] + a[1][1] + a[2][2]; // I removed + 1.0f; see discussion with Ethan
	if (trace > 0) {// I changed M_EPSILON to 0
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (a[2][1] - a[1][2]) * s;
		q.y = (a[0][2] - a[2][0]) * s;
		q.z = (a[1][0] - a[0][1]) * s;
	}
	else {
		if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);
			q.w = (a[2][1] - a[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (a[0][1] + a[1][0]) / s;
			q.z = (a[0][2] + a[2][0]) / s;
		}
		else if (a[1][1] > a[2][2]) {
			float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);
			q.w = (a[0][2] - a[2][0]) / s;
			q.x = (a[0][1] + a[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (a[1][2] + a[2][1]) / s;
		}
		else {
			float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);
			q.w = (a[1][0] - a[0][1]) / s;
			q.x = (a[0][2] + a[2][0]) / s;
			q.y = (a[1][2] + a[2][1]) / s;
			q.z = 0.25f * s;
		}
	}

	return q;
}

static HmdVector3d_t eulerAnglesFromMatrix(HmdMatrix34_t mat) {
	return eulerAnglesFromQuat(quatFromMatrix(mat));
}

OpenVREyePose::OpenVREyePose(int eye)
	: ShiftedEyePose( 0.0f )
	, eye(eye)
	, eyeTexture(nullptr)
	, currentPose(nullptr)
{
}


/* virtual */
OpenVREyePose::~OpenVREyePose() 
{
	dispose();
}

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
}


/* virtual */
void OpenVREyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
{
	outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

	if (currentPose == nullptr)
		return;
	const TrackedDevicePose_t& hmd = *currentPose;
	if (! hmd.bDeviceIsConnected)
		return;
	if (! hmd.bPoseIsValid)
		return;

	if (! doStereoscopicViewpointOffset)
		return;

	const HmdMatrix34_t& hmdPose = hmd.mDeviceToAbsoluteTracking;

	// Pitch and Roll are identical between OpenVR and Doom worlds.
	// But yaw can differ, depending on starting state, and controller movement.
	float doomYawDegrees = yaw;
	float openVrYawDegrees = RAD2DEG(-eulerAnglesFromMatrix(hmdPose).v[0]);
	deltaYawDegrees = doomYawDegrees - openVrYawDegrees;
	while (deltaYawDegrees > 180)
		deltaYawDegrees -= 360;
	while (deltaYawDegrees < -180)
		deltaYawDegrees += 360;

	openvr_to_doom_angle = DAngle(-deltaYawDegrees);

	// extract rotation component from hmd transform
	LSMatrix44 openvr_X_hmd(hmdPose);
	LSMatrix44 hmdRot = openvr_X_hmd.getWithoutTranslation(); // .transpose();

	/// In these eye methods, just get local inter-eye stereoscopic shift, not full position shift ///

	// compute local eye shift
	LSMatrix44 eyeShift2;
	eyeShift2.loadIdentity();
	eyeShift2 = eyeShift2 * eyeToHeadTransform; // eye to head
	eyeShift2 = eyeShift2 * hmdRot; // head to openvr

	LSVec3 eye_EyePos = LSVec3(0, 0, 0); // eye position in eye frame
	LSVec3 hmd_EyePos = LSMatrix44(eyeToHeadTransform) * eye_EyePos;
	LSVec3 hmd_HmdPos = LSVec3(0, 0, 0); // hmd position in hmd frame
	LSVec3 openvr_EyePos = openvr_X_hmd * hmd_EyePos;
	LSVec3 openvr_HmdPos = openvr_X_hmd * hmd_HmdPos;
	LSVec3 hmd_OtherEyePos = LSMatrix44(otherEyeToHeadTransform) * eye_EyePos;
	LSVec3 openvr_OtherEyePos = openvr_X_hmd * hmd_OtherEyePos;
	LSVec3 openvr_EyeOffset = openvr_EyePos - openvr_HmdPos;

	VSMatrix doomInOpenVR = VSMatrix();
	doomInOpenVR.loadIdentity();
	// permute axes
	float permute[] = { // Convert from OpenVR to Doom axis convention, including mirror inversion
		-1,  0,  0,  0, // X-right in OpenVR -> X-left in Doom
			0,  0,  1,  0, // Z-backward in OpenVR -> Y-backward in Doom
			0,  1,  0,  0, // Y-up in OpenVR -> Z-up in Doom
			0,  0,  0,  1};
	doomInOpenVR.multMatrix(permute);
	doomInOpenVR.scale(vr_vunits_per_meter, vr_vunits_per_meter, vr_vunits_per_meter); // Doom units are not meters
	double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
	doomInOpenVR.scale(pixelstretch, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio
	doomInOpenVR.rotate(deltaYawDegrees, 0, 0, 1);

	LSVec3 doom_EyeOffset = LSMatrix44(doomInOpenVR) * openvr_EyeOffset;

	if (doTrackHmdVerticalPosition) {
		// In OpenVR, the real world floor level is at y==0
		// In Doom, the virtual player foot level is viewheight below the current viewpoint (on the Z axis)
		// We want to align those two heights here
		const player_t & player = players[consoleplayer];
		double vh = player.viewheight; // Doom thinks this is where you are
		double hh = (openvr_X_hmd[1][3] - vr_floor_offset) * vr_vunits_per_meter; // HMD is actually here
		doom_EyeOffset[2] += hh - vh;
		// TODO: optionally allow player to jump and crouch by actually jumping and crouching
	}

	if (doTrackHmdHorizontalPosition) {
		// shift viewpoint when hmd position shifts
		static bool is_initial_origin_set = false;
		if (! is_initial_origin_set) {
			// initialize origin to first noted HMD location
			// TODO: implement recentering based on a CCMD
			openvr_origin = openvr_HmdPos;
			is_initial_origin_set = true;
		}
		openvr_dpos = openvr_HmdPos - openvr_origin;

		LSVec3 doom_dpos = LSMatrix44(doomInOpenVR) * openvr_dpos;
		doom_EyeOffset[0] += doom_dpos[0];
		doom_EyeOffset[1] += doom_dpos[1];
	}

	outViewShift[0] = doom_EyeOffset[0];
	outViewShift[1] = doom_EyeOffset[1];
	outViewShift[2] = doom_EyeOffset[2];
}

/* virtual */
VSMatrix OpenVREyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
{
	// Ignore those arguments and get the projection from the SDK
	// VSMatrix vs1 = ShiftedEyePose::GetProjection(fov, aspectRatio, fovRatio);
	return projectionMatrix;
}

void OpenVREyePose::initialize(VR_IVRSystem_FnTable * vrsystem)
{
	float zNear = 5.0;
	float zFar = 65536.0;
	HmdMatrix44_t projection = vrsystem->GetProjectionMatrix(
			EVREye(eye), zNear, zFar);
	HmdMatrix44_t proj_transpose;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			proj_transpose.m[i][j] = projection.m[j][i];
		}
	}
	projectionMatrix.loadIdentity();
	projectionMatrix.multMatrix(&proj_transpose.m[0][0]);

	HmdMatrix34_t eyeToHead = vrsystem->GetEyeToHeadTransform(EVREye(eye));
	vSMatrixFromHmdMatrix34(eyeToHeadTransform, eyeToHead);
	HmdMatrix34_t otherEyeToHead = vrsystem->GetEyeToHeadTransform(eye == EVREye_Eye_Left ? EVREye_Eye_Right : EVREye_Eye_Left);
	vSMatrixFromHmdMatrix34(otherEyeToHeadTransform, otherEyeToHead);

	if (eyeTexture == nullptr)
		eyeTexture = new Texture_t();
	eyeTexture->handle = nullptr; // TODO: populate this at resolve time
	eyeTexture->eType = ETextureType_TextureType_OpenGL;
	eyeTexture->eColorSpace = EColorSpace_ColorSpace_Linear;
}

void OpenVREyePose::dispose()
{
	if (eyeTexture) {
		delete eyeTexture;
		eyeTexture = nullptr;
	}
}

bool OpenVREyePose::submitFrame(VR_IVRCompositor_FnTable * vrCompositor) const
{
	if (eyeTexture == nullptr)
		return false;
	if (vrCompositor == nullptr)
		return false;
 
	// Copy HDR game texture to local vr LDR framebuffer, so gamma correction could work
	if (eyeTexture->handle == nullptr) {
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

	static VRTextureBounds_t tBounds = {0, 0, 1, 1};
	vrCompositor->Submit(EVREye(eye), eyeTexture, &tBounds, EVRSubmitFlags_Submit_Default);
	return true;
}

VSMatrix OpenVREyePose::getQuadInWorld(
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

	const OpenVREyePose * activeEye = this;

	// eye coordinates from hmd coordinates
	LSMatrix44 e2h(activeEye->eyeToHeadTransform);
	new_projection.multMatrix(e2h.transpose());

	// Follow HMD orientation, EXCEPT for roll angle (keep weapon upright)
	if (activeEye->currentPose) {
		float openVrRollDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[2]);
		new_projection.rotate(-openVrRollDegrees, 0, 0, 1);

		if (doFixPitch) {
			float openVrPitchDegrees = RAD2DEG(-eulerAnglesFromMatrix(activeEye->currentPose->mDeviceToAbsoluteTracking).v[1]);
			new_projection.rotate(-openVrPitchDegrees, 1, 0, 0);
		}
		if (pitchOffset != 0)
			new_projection.rotate(-pitchOffset, 1, 0, 0);
	}

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

void OpenVREyePose::AdjustHud() const
{
	// Draw crosshair on a separate quad, before updating HUD matrix
	const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
	if (mode3d->IsMono())
		return;
	const OpenVRMode * openVrMode = static_cast<const OpenVRMode *>(mode3d);
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

void OpenVREyePose::AdjustBlend() const
{
	VSMatrix& proj = gl_RenderState.mProjectionMatrix;
	proj.loadIdentity();
	proj.translate(-1, 1, 0);
	proj.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
	gl_RenderState.ApplyMatrices();
}

OpenVRMode::OpenVRMode() 
	: vrSystem(nullptr)
	, leftEyeView(EVREye_Eye_Left)
	, rightEyeView(EVREye_Eye_Right)
	, hmdWasFound(false)
	, sceneWidth(0), sceneHeight(0)
	, vrCompositor(nullptr)
	, vrRenderModels(nullptr)
	, vrToken(0)
	, crossHairDrawer(new F2DDrawer)
	, cached2DDrawer(nullptr)
{
	eye_ptrs.Push(&leftEyeView); // initially default behavior to Mono non-stereo rendering

	if ( ! IsOpenVRPresent() ) return; // failed to load openvr API dynamically

	if ( ! VR_IsRuntimeInstalled() ) return; // failed to find OpenVR implementation

	if ( ! VR_IsHmdPresent() ) return; // no VR headset is attached

	EVRInitError eError;
	// Code below recapitulates the effects of C++ call vr::VR_Init()
	VR_InitInternal(&eError, EVRApplicationType_VRApplication_Scene);
	if (eError != EVRInitError_VRInitError_None) {
		std::string errMsg = VR_GetVRInitErrorAsEnglishDescription(eError);
		return;
	}
	if (! VR_IsInterfaceVersionValid(IVRSystem_Version))
	{
		VR_ShutdownInternal();
		return;
	}
	vrToken = VR_GetInitToken();
	const std::string sys_key = std::string("FnTable:") + std::string(IVRSystem_Version);
	vrSystem = (VR_IVRSystem_FnTable*) VR_GetGenericInterface(sys_key.c_str() , &eError);
	if (vrSystem == nullptr)
		return;

	vrSystem->GetRecommendedRenderTargetSize(&sceneWidth, &sceneHeight);

	leftEyeView.initialize(vrSystem);
	rightEyeView.initialize(vrSystem);

	const std::string comp_key = std::string("FnTable:") + std::string(IVRCompositor_Version);
	vrCompositor = (VR_IVRCompositor_FnTable*)VR_GetGenericInterface(comp_key.c_str(), &eError);
	if (vrCompositor == nullptr)
		return;

	const std::string model_key = std::string("FnTable:") + std::string(IVRRenderModels_Version);
	vrRenderModels = (VR_IVRRenderModels_FnTable*)VR_GetGenericInterface(model_key.c_str(), &eError);

	eye_ptrs.Push(&rightEyeView); // NOW we render to two eyes
	hmdWasFound = true;

	crossHairDrawer->Clear();
}

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
void OpenVRMode::AdjustViewports() const
{
	// Draw the 3D scene into the entire framebuffer
	GLRenderer->mSceneViewport.width = sceneWidth;
	GLRenderer->mSceneViewport.height = sceneHeight;
	GLRenderer->mSceneViewport.left = 0;
	GLRenderer->mSceneViewport.top = 0;

	GLRenderer->mScreenViewport.width = sceneWidth;
	GLRenderer->mScreenViewport.height = sceneHeight;
}

void OpenVRMode::AdjustPlayerSprites() const
{
	GetWeaponTransform(&gl_RenderState.mModelMatrix);

	float scale = 0.00125f * openvr_weaponScale;
	gl_RenderState.mModelMatrix.scale(scale, -scale, scale);
	gl_RenderState.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f);

	gl_RenderState.EnableModelMatrix(true);
}

void OpenVRMode::UnAdjustPlayerSprites() const {

	gl_RenderState.EnableModelMatrix(false);
}

void OpenVRMode::AdjustCrossHair() const
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

void OpenVRMode::UnAdjustCrossHair() const
{
	viewheight = cachedViewheight;
	viewwindowy = cachedViewwindowy;
	if (cached2DDrawer)
		GLRenderer->m2DDrawer = cached2DDrawer;
	cached2DDrawer = nullptr;
}

void OpenVRMode::DrawControllerModels() const
{
	if(!openvr_drawControllers)
		return; 
	
	for (int i = 0; i < MAX_ROLES; ++i) 
	{
		if (GetHandTransform(i, &gl_RenderState.mModelMatrix) && controllers[i].model)
		{
			gl_RenderState.EnableModelMatrix(true);

			controllers[i].model->RenderFrame(0, 0, 0, 0);

			gl_RenderState.EnableModelMatrix(false);
		}
	}
}


bool OpenVRMode::GetHandTransform(int hand, VSMatrix* mat) const
{
	if (controllers[hand].active)
	{
		mat->loadIdentity();

		APlayerPawn* playermo = r_viewpoint.camera->player->mo;
		DVector3 pos = playermo->InterpolatedPosition(r_viewpoint.TicFrac);

		mat->translate(pos.X, pos.Z, pos.Y);

		mat->scale(vr_vunits_per_meter, vr_vunits_per_meter, -vr_vunits_per_meter);

		mat->rotate(-deltaYawDegrees - 180, 0, 1, 0);

		mat->translate(-openvr_origin.x, -vr_floor_offset, -openvr_origin.z);

		LSMatrix44 handToAbs;
		vSMatrixFromHmdMatrix34(handToAbs, controllers[hand].pose.mDeviceToAbsoluteTracking);

		mat->multMatrix(handToAbs.transpose());

		return true;
	}
	return false;
}

bool OpenVRMode::GetWeaponTransform(VSMatrix* out) const
{
	if (GetHandTransform(openvr_rightHanded ? 1 : 0, out))
	{
		out->rotate(openvr_weaponRotate, 1, 0, 0);
		if (!openvr_rightHanded)
			out->scale(-1.0f, 1.0f, 1.0f);
		return true;
	}
	return false;
}

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

	DVector3 refdirection = { pc * yaw.Cos(), pc * yaw.Sin(), -pitch.Sin() };

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



/* virtual */
void OpenVRMode::Present() const {
	// TODO: For performance, don't render to the desktop screen here
	if (doRenderToDesktop) {
		GLRenderer->mBuffers->BindOutputFB();
		GLRenderer->ClearBorders();

		// Compute screen regions to use for left and right eye views
		int leftWidth = GLRenderer->mOutputLetterbox.width / 2;
		int rightWidth = GLRenderer->mOutputLetterbox.width - leftWidth;
		GL_IRECT leftHalfScreen = GLRenderer->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		GL_IRECT rightHalfScreen = GLRenderer->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;

		GLRenderer->mBuffers->BindEyeTexture(0, 0);
		GLRenderer->DrawPresentTexture(leftHalfScreen, true);
		GLRenderer->mBuffers->BindEyeTexture(1, 0);
		GLRenderer->DrawPresentTexture(rightHalfScreen, true);
	}

	if (doRenderToHmd) 
	{
		leftEyeView.submitFrame(vrCompositor);
		rightEyeView.submitFrame(vrCompositor);
	}
}

static int mAngleFromRadians(double radians) 
{
	double m = std::round(65535.0 * radians / (2.0 * M_PI));
	return int(m);
}

void OpenVRMode::updateHmdPose(
	double hmdYawRadians, 
	double hmdPitchRadians, 
	double hmdRollRadians) const 
{
	hmdYaw = hmdYawRadians;
	double hmdpitch = hmdPitchRadians;
	double hmdroll = hmdRollRadians;

	double hmdYawDelta = 0;
	if (doTrackHmdYaw) {
		// Set HMD angle game state parameters for NEXT frame
		static double previousHmdYaw = 0;
		static bool havePreviousYaw = false;
		if (!havePreviousYaw) {
			previousHmdYaw = hmdYaw;
			havePreviousYaw = true;
		}
		hmdYawDelta = hmdYaw - previousHmdYaw;
		G_AddViewAngle(mAngleFromRadians(-hmdYawDelta));
		previousHmdYaw = hmdYaw;
	}

	/* */
	// Pitch
	if (doTrackHmdPitch) {
		double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
		double hmdPitchInDoom = -atan(tan(hmdpitch) / pixelstretch);
		double viewPitchInDoom = GLRenderer->mAngles.Pitch.Radians();
		double dPitch = 
			// hmdPitchInDoom
			-hmdpitch
			- viewPitchInDoom;
		G_AddViewPitch(mAngleFromRadians(-dPitch));
	}

	// Roll can be local, because it doesn't affect gameplay.
	if (doTrackHmdRoll)
		GLRenderer->mAngles.Roll = RAD2DEG(-hmdroll);

	// Late-schedule update to renderer angles directly, too
	if (doLateScheduledRotationTracking) {
		if (doTrackHmdPitch)
			GLRenderer->mAngles.Pitch = RAD2DEG(-hmdpitch);
		if (doTrackHmdYaw) {
			double viewYaw = r_viewpoint.Angles.Yaw.Degrees + RAD2DEG(hmdYawDelta);
			while (viewYaw <= -180.0) 
				viewYaw += 360.0;
			while (viewYaw > 180.0) 
				viewYaw -= 360.0;
			r_viewpoint.Angles.Yaw = viewYaw;
		}
	}
}

static int GetVRAxisState(VRControllerState_t& state, int vrAxis, int axis)
{
	float pos = axis == 0 ? state.rAxis[vrAxis].x : state.rAxis[vrAxis].y;
	return pos < -DEAD_ZONE ? 1 : pos > DEAD_ZONE ? 2 : 0;
}

void Joy_GenerateUIButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys)
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
	int controller = openvr_rightHanded ? role : 1 - role;
	
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

VRControllerState_t& OpenVR_GetState(int hand)
{
	int controller = openvr_rightHanded ? hand : 1 - hand;
	return controllers[controller].lastState;
}


int OpenVR_GetTouchPadAxis()
{
	return axisTrackpad;
}

int OpenVR_GetJoystickAxis()
{
	return axisJoystick;
}

bool OpenVR_OnHandIsRight()
{
	return openvr_rightHanded;
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

/* virtual */
void OpenVRMode::SetUp() const
{
	super::SetUp();

	if (vrCompositor == nullptr)
		return;

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
	}
	else {
		// TODO: Draw a more interesting background behind the 2D screen
		for (int i = 0; i < 2; ++i) {
			GLRenderer->mBuffers->BindEyeFB(i);
			glClearColor(0.3f, 0.1f, 0.1f, 1.0f); // draw a dark red universe
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	static TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
	vrCompositor->WaitGetPoses(
		poses, k_unMaxTrackedDeviceCount, // current pose
		nullptr, 0 // future pose?
	);

	TrackedDevicePose_t& hmdPose0 = poses[k_unTrackedDeviceIndex_Hmd];

	if (hmdPose0.bPoseIsValid) {
		const HmdMatrix34_t& hmdPose = hmdPose0.mDeviceToAbsoluteTracking;
		HmdVector3d_t eulerAngles = eulerAnglesFromMatrix(hmdPose);
		updateHmdPose(eulerAngles.v[0], eulerAngles.v[1], eulerAngles.v[2]);
		leftEyeView.setCurrentHmdPose(&hmdPose0);
		rightEyeView.setCurrentHmdPose(&hmdPose0);

		player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;

		// Check for existence of VR motion controllers...
		for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
			if (i == k_unTrackedDeviceIndex_Hmd)
				continue; // skip the headset position
			TrackedDevicePose_t & pose = poses[i];
			if (! pose.bDeviceIsConnected)
				continue;
			if (! pose.bPoseIsValid)
				continue;
			ETrackedDeviceClass device_class = vrSystem->GetTrackedDeviceClass(i);
			if (device_class != ETrackedDeviceClass_TrackedDeviceClass_Controller)
				continue; // controllers only, please
			
			int role = vrSystem->GetControllerRoleForTrackedDeviceIndex(i) - ETrackedControllerRole_TrackedControllerRole_LeftHand;
			if (role >= 0 && role < MAX_ROLES)
			{
				char model_chars[101];
				ETrackedPropertyError propertyError;
				vrSystem->GetStringTrackedDeviceProperty(i, ETrackedDeviceProperty_Prop_RenderModelName_String, model_chars, 100, &propertyError);
				if (propertyError != ETrackedPropertyError_TrackedProp_Success)
					continue; // something went wrong...
				std::string model_name(model_chars);
				if (controllerMeshes.count(model_name) == 0) {
					controllerMeshes[model_name] = VRControllerModel(model_name, vrRenderModels);
					assert(controllerMeshes.count(model_name) == 1);
				}
				controllers[role].active = true;
				controllers[role].pose = pose;
				if (controllerMeshes[model_name].isLoaded())
				{
					controllers[role].model = &controllerMeshes[model_name];
				}
				VRControllerState_t newState;
				vrSystem->GetControllerState(i, &newState, sizeof(newState));

				if (!identifiedAxes)
				{
					identifiedAxes = true;
					for (int a = 0; a < k_unControllerStateAxisCount; a++)
					{
						switch (vrSystem->GetInt32TrackedDeviceProperty(i, (ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + a), 0))
						{
						case vr::k_eControllerAxis_TrackPad:
							if (axisTrackpad == -1) axisTrackpad = a;
							break;
						case vr::k_eControllerAxis_Joystick:
							if (axisJoystick == -1) axisJoystick = a;
							break;
						case vr::k_eControllerAxis_Trigger:
							if (axisTrigger == -1) axisTrigger = a;
							break;
						}
					}
				}

				if (player && openvr_kill_momentum)
				{
					if (role == (openvr_rightHanded ? 0 : 1))
					{
						if (JustStoppedMoving(controllers[role].lastState, newState, axisTrackpad) 
							|| JustStoppedMoving(controllers[role].lastState, newState, axisJoystick))
						{
							player->mo->Vel[0] = 0;
							player->mo->Vel[1] = 0;
						}
					}
				}

				HandleControllerState(i, role, newState);

				
			}
		}

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
			if (GetHandTransform(openvr_rightHanded ? 0 : 1, &mat) && openvr_moveFollowsOffHand)
			{
				player->mo->ThrustAngleOffset = DAngle(RAD2DEG(atan2f(-mat[2][2], -mat[2][0]))) - player->mo->Angles.Yaw;
			}
			else
			{
				player->mo->ThrustAngleOffset = 0.0f;
			}
			auto vel = player->mo->Vel;
			player->mo->Vel = DVector3((DVector2(-openvr_dpos.x, openvr_dpos.z) * vr_vunits_per_meter).Rotated(openvr_to_doom_angle), 0);
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
			openvr_origin += openvr_dpos;
		}
	}

	I_StartupOpenVR();

	//To feel smooth, yaw changes need to accumulate over the (sub) tic (i.e. render frame, not per tic)
	unsigned int time = I_FPSTime();
	static unsigned int lastTime = time;

	unsigned int delta = time - lastTime;
	lastTime = time;

	G_AddViewAngle(joyint(-1280 * I_OpenVRGetYaw() * delta * 30 / 1000));
}

/* virtual */
void OpenVRMode::TearDown() const
{
	if (gamestate == GS_LEVEL) {
		screenblocks = cachedScreenBlocks;
	}
	super::TearDown();
}

/* virtual */
OpenVRMode::~OpenVRMode() 
{
	if (vrSystem != nullptr) {
		VR_ShutdownInternal();
		vrSystem = nullptr;
		vrCompositor = nullptr;
		vrRenderModels = nullptr;
		leftEyeView.dispose();
		rightEyeView.dispose();
	}
	if (crossHairDrawer != nullptr) {
		delete crossHairDrawer;
		crossHairDrawer = nullptr;
	}
}

} /* namespace s3d */

#endif

