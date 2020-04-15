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
extern bool resetDoomYaw;
extern float doomYaw;

extern float vrFOV;

extern vec3_t worldPosition;

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t positionDeltaThisFrame;

extern vec3_t weaponangles;
extern vec3_t weaponoffset;

extern bool weaponStabilised;
extern float vr_weapon_pitchadjust;
extern bool vr_moveuseoffhand;
extern bool vr_switchsticks;
extern float vr_snapturn_angle;
extern float vr_use_teleport;


extern vec3_t offhandangles;
extern vec3_t offhandoffset;

extern bool player_moving;

extern bool ready_teleport;
extern bool trigger_teleport;

extern bool shutdown;
void shutdownVR();

float radians(float deg);
float degrees(float rad);
bool isMultiplayer();
double GetTimeInMilliSeconds();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void QuatToYawPitchRoll(ovrQuatf q, vec3_t rotation, vec3_t out);
void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key);

//Called from engine code
bool QzDoom_useScreenLayer();
void QzDoom_GetScreenRes(uint32_t *width, uint32_t *height);
void QzDoom_Vibrate(float duration, int channel, float intensity );
bool QzDoom_processMessageQueue();
void QzDoom_FrameSetup();
void QzDoom_setUseScreenLayer(bool use);
void QzDoom_processHaptics();
void QzDoom_getHMDOrientation(ovrTracking2 *tracking);
void QzDoom_getTrackedRemotesOrientation(int vr_control_scheme);

void incrementFrameIndex();

void QzDoom_prepareEyeBuffer(int eye );
void QzDoom_finishEyeBuffer(int eye );
void QzDoom_submitFrame(ovrTracking2 *tracking);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //vrcommon_h