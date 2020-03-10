#if !defined(vrcommon_h)
#define vrcommon_h

#ifdef __cplusplus
extern "C"
{
#endif

//#include <VrApi_Ext.h>
#include <VrApi_Input.h>

#include <android/log.h>

#include "mathlib.h"

#define LOG_TAG "QzDoom"

#ifndef NDEBUG
#define DEBUG 1
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

extern bool qzdoom_initialised;

extern long long global_time;

extern float playerHeight;
extern float playerYaw;

extern float vrFOV;

extern vec3_t worldPosition;

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t positionDeltaThisFrame;

extern vec3_t weaponangles;
extern vec3_t weaponoffset;

extern bool weaponStabilised;
extern float vr_weapon_pitchadjust;
extern bool vr_walkdirection;
extern float vr_snapturn_angle;


extern vec3_t offhandangles;
extern vec3_t offhandoffset;

extern bool player_moving;

void shutdownVR();

float radians(float deg);
float degrees(float rad);
bool isMultiplayer();
double GetTimeInMilliSeconds();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void QuatToYawPitchRoll(ovrQuatf q, float pitchAdjust, vec3_t out);
bool useScreenLayer();
void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key);
void Android_GetScreenRes(uint32_t *width, uint32_t *height);

void C_DoCommandC (const char *cmd);

float VR_GetRawYaw();

void setUseScreenLayer(bool use);

void processHaptics();
void getHMDOrientation(ovrTracking2 *tracking);
void getTrackedRemotesOrientation(int vr_control_scheme);

void incrementFrameIndex();

void prepareEyeBuffer(int eye );
void finishEyeBuffer(int eye );
void submitFrame(ovrTracking2 *tracking);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //vrcommon_h