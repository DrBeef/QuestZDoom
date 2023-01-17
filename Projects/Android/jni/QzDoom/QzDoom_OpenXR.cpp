#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include "argtable3.h"
#include "VrInput.h"


//#define ENABLE_GL_DEBUG
#define ENABLE_GL_DEBUG_VERBOSE 1

//Let's go to the maximum!
extern int NUM_MULTI_SAMPLES;
extern int REFRESH	         ;
extern float SS_MULTIPLIER    ;


/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *msaa;
struct arg_int *refresh;
struct arg_end *end;

char **argv;
int argc=0;


//Define all variables here that were externs in the VrCommon.h
bool qzdoom_initialised;
long long global_time;
float playerYaw;
bool resetDoomYaw;
bool resetPreviousPitch;
float doomYaw;
float previousPitch;
float vrFOV;
vec3_t worldPosition;
vec3_t hmdPosition;
vec3_t hmdorientation;
vec3_t positionDeltaThisFrame;
vec3_t weaponangles;
vec3_t weaponoffset;
bool weaponStabilised;

vec3_t offhandangles;
vec3_t offhandoffset;
bool player_moving;
bool shutdown;
bool ready_teleport;
bool trigger_teleport;
bool cinemamode;

//This is now controlled by the engine
static bool useVirtualScreen = true;

static bool hasIWADs = false;
static bool hasLauncher = false;

/*
================================================================================

QuestZDoom Stuff

================================================================================
*/

void QzDoom_setUseScreenLayer(bool use)
{
	useVirtualScreen = use;
}

int QzDoom_SetRefreshRate(int refreshRate)
{
#ifdef META_QUEST
	OXR(gAppState.pfnRequestDisplayRefreshRate(gAppState.Session, (float)refreshRate));
	return refreshRate;
#endif

	return 0;
}

void QzDoom_GetScreenRes(uint32_t *width, uint32_t *height)
{
	int iWidth, iHeight;
	TBXR_GetScreenRes(&iWidth, &iHeight);
	*width = iWidth;
	*height = iHeight;
}

bool VR_UseScreenLayer()
{
	return useVirtualScreen || cinemamode;
}

float VR_GetScreenLayerDistance()
{
	return 4.0f;
}

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}


void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer())
    {
    	playerYaw = yaw;
	}
}

void VR_SetHMDPosition(float x, float y, float z )
{
 	VectorSet(hmdPosition, x, y, z);

    positionDeltaThisFrame[0] = (worldPosition[0] - x);
    positionDeltaThisFrame[1] = (worldPosition[1] - y);
    positionDeltaThisFrame[2] = (worldPosition[2] - z);

    worldPosition[0] = x;
    worldPosition[1] = y;
    worldPosition[2] = z;
}

void VR_GetMove(float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up,
				float *yaw, float *pitch, float *roll)
{
    *joy_forward = remote_movementForward;
    *hmd_forward = positional_movementForward;
    *up = remote_movementUp;
    *joy_side = remote_movementSideways;
    *hmd_side = positional_movementSideways;
	*yaw = cinemamode ? cinemamodeYaw : hmdorientation[YAW] + snapTurn;
	*pitch = cinemamode ? cinemamodePitch : hmdorientation[PITCH];
	*roll = cinemamode ? 0.0f : hmdorientation[ROLL];
}

void VR_DoomMain(int argc, char** argv);

void VR_Init()
{
	//Initialise all our variables
	playerYaw = 0.0f;
    resetDoomYaw = true;
	resetPreviousPitch = true;
	remote_movementSideways = 0.0f;
	remote_movementForward = 0.0f;
	remote_movementUp = 0.0f;
	positional_movementSideways = 0.0f;
	positional_movementForward = 0.0f;
	snapTurn = 0.0f;
	cinemamodeYaw = 0.0f;
	cinemamodePitch = 0.0f;

	//init randomiser
	srand(time(NULL));

	shutdown = false;
    ready_teleport = false;
    trigger_teleport = false;

	cinemamode = false;

	chdir("/sdcard/QuestZDoom");
}

void * AppThreadFunction(void * parm ) {
	gAppThread = (ovrAppThread *) parm;

	java.Vm = gAppThread->JavaVm;
	java.Vm->AttachCurrentThread(&java.Env, NULL);
	java.ActivityObject = gAppThread->ActivityObject;

	jclass cls = java.Env->GetObjectClass(java.ActivityObject);

	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long) "AppThreadFunction", 0, 0, 0);

	//Set device defaults
	if (SS_MULTIPLIER == 0.0f)
	{
		//GB Override as refresh is now 72 by default as we decided a higher res is better as 90hz has stutters
		SS_MULTIPLIER = 1.25f;
	}
	else if (SS_MULTIPLIER > 1.5f)
	{
		SS_MULTIPLIER = 1.5f;
	}

	gAppState.MainThreadTid = gettid();

	VR_Init();

	TBXR_InitialiseOpenXR();

	TBXR_EnterVR();
	TBXR_InitRenderer();
	TBXR_InitActions();

	TBXR_WaitForSessionActive();

	if (REFRESH != 0)
	{
		QzDoom_SetRefreshRate(REFRESH);
	}

	if (hasIWADs)// && hasLauncher)
	{
		//Should now be all set up and ready - start the Doom main loop
		VR_DoomMain(argc, argv);
	}

	TBXR_LeaveVR();

	//Ask Java to shut down
    jni_shutdown();

	return NULL;
}

//All the stuff we want to do each frame specifically for this game
void VR_FrameSetup()
{

}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection)
{
#ifdef PICO_XR
		XrMatrix4x4f_CreateProjectionFov(
				&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
				gAppState.Projections[eye].fov, zNear, zFar);
#endif

#ifdef META_QUEST
		XrFovf fov = {};
		for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
			fov.angleLeft += gAppState.Projections[eye].fov.angleLeft / 2.0f;
			fov.angleRight += gAppState.Projections[eye].fov.angleRight / 2.0f;
			fov.angleUp += gAppState.Projections[eye].fov.angleUp / 2.0f;
			fov.angleDown += gAppState.Projections[eye].fov.angleDown / 2.0f;
		}
		XrMatrix4x4f_CreateProjectionFov(
				&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
				fov, zNear, zFar);
#endif

	memcpy(projection, gAppState.ProjectionMatrices[eye].m, 16 * sizeof(float));
	return true;
}


extern "C" {
void jni_haptic_event(const char *event, int position, int intensity, float angle, float yHeight);
void jni_haptic_updateevent(const char *event, int intensity, float angle);
void jni_haptic_stopevent(const char *event);
void jni_haptic_endframe();
void jni_haptic_enable();
void jni_haptic_disable();
};

void VR_ExternalHapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	jni_haptic_event(event, position, intensity, angle, yHeight);
}

void VR_HapticStopEvent(const char* event)
{
	jni_haptic_stopevent(event);
}

void VR_HapticEnable()
{
	static bool firstTime = true;
	if (firstTime) {
		jni_haptic_enable();
		firstTime = false;
		jni_haptic_event("fire_pistol", 0, 100, 0, 0);
	}
}

void VR_HapticDisable()
{
	jni_haptic_disable();
}

void VR_HapticEvent(const char* event, int position, int intensity, float angle, float yHeight )
{
    static char buffer[256];

    memset(buffer, 0, 256);
    for(int i = 0; event[i]; i++)
    {
        buffer[i] = tolower(event[i]);
    }

    jni_haptic_event(buffer, position, intensity, angle, yHeight);
}

void QzDoom_Vibrate(float duration, int channel, float intensity )
{
	TBXR_Vibrate(duration, channel+1, intensity);
}

void VR_HandleControllerInput() {
	TBXR_UpdateControllers();

    //Call additional control schemes here
    switch (vr_control_scheme)
    {
            case RIGHT_HANDED_DEFAULT:
    	        HandleInput_Default(vr_control_scheme,
    	                &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
                                &leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
                                xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
                    break;
            case LEFT_HANDED_DEFAULT:
			case LEFT_HANDED_ALT:
	            HandleInput_Default(vr_control_scheme,
	                                &leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
                                        &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
                                        xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
                    break;
    }
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

jmethodID android_shutdown;
static JavaVM *jVM;
static jobject jniCallbackObj=0;

void jni_shutdown()
{
	ALOGV("Calling: jni_shutdown");
	JNIEnv *env;
	jobject tmp;
	if ((jVM->GetEnv((void**) &env, JNI_VERSION_1_4))<0)
	{
		jVM->AttachCurrentThread(&env, NULL);
	}
	return env->CallVoidMethod(jniCallbackObj, android_shutdown);
}

void VR_Shutdown()
{
	jni_shutdown();
}

jmethodID android_haptic_event;
jmethodID android_haptic_stopevent;
jmethodID android_haptic_enable;
jmethodID android_haptic_disable;

void jni_haptic_event(const char* event, int position, int intensity, float angle, float yHeight)
{
	JNIEnv *env;
	jobject tmp;
	if ((jVM->GetEnv((void**) &env, JNI_VERSION_1_4))<0)
	{
		jVM->AttachCurrentThread(&env, NULL);
	}

	jstring StringArg1 = env->NewStringUTF(event);

	return env->CallVoidMethod(jniCallbackObj, android_haptic_event, StringArg1, position, intensity, angle, yHeight);
}

void jni_haptic_stopevent(const char* event)
{
	ALOGV("Calling: jni_haptic_stopevent");
	JNIEnv *env;
	jobject tmp;
	if ((jVM->GetEnv((void**) &env, JNI_VERSION_1_4))<0)
	{
		jVM->AttachCurrentThread(&env, NULL);
	}

	jstring StringArg1 = env->NewStringUTF(event);

	return env->CallVoidMethod(jniCallbackObj, android_haptic_stopevent, StringArg1);
}


void jni_haptic_enable()
{
	ALOGV("Calling: jni_haptic_enable");
	JNIEnv *env;
	jobject tmp;
	if ((jVM->GetEnv((void**) &env, JNI_VERSION_1_4))<0)
	{
		jVM->AttachCurrentThread(&env, NULL);
	}

	return env->CallVoidMethod(jniCallbackObj, android_haptic_enable);
}

void jni_haptic_disable()
{
	ALOGV("Calling: jni_haptic_disable");
	JNIEnv *env;
	jobject tmp;
	if ((jVM->GetEnv((void**) &env, JNI_VERSION_1_4))<0)
	{
		jVM->AttachCurrentThread(&env, NULL);
	}

	return env->CallVoidMethod(jniCallbackObj, android_haptic_disable);
}

extern "C" {

int JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv *env;
    jVM = vm;
	if(vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		ALOGE("Failed JNI_OnLoad");
		return -1;
	}

	return JNI_VERSION_1_4;
}

JNIEXPORT jlong JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onCreate( JNIEnv * env, jclass activityClass, jobject activity,
																		 jstring commandLineParams, jboolean jHasIWADs, jboolean jHasLauncher)
{
	ALOGV( "    GLES3JNILib::onCreate()" );

	/* the global arg_xxx structs are initialised within the argtable */
	void *argtable[] = {
			ss    = arg_dbl0("s", "supersampling", "<double>", "super sampling value (default: Q1: 1.2, Q2: 1.35)"),
            cpu   = arg_int0("c", "cpu", "<int>", "CPU perf index 1-4 (default: 2)"),
            gpu   = arg_int0("g", "gpu", "<int>", "GPU perf index 1-4 (default: 3)"),
            msaa  = arg_int0("m", "msaa", "<int>", "MSAA (default: 1)"),
            refresh  = arg_int0("r", "refresh", "<int>", "Refresh Rate (default: Q1: 72, Q2: 72)"),
            end   = arg_end(20)
	};

	hasIWADs = jHasIWADs != 0;
	hasLauncher = jHasLauncher != 0;

	jboolean iscopy;
	const char *arg = env->GetStringUTFChars(commandLineParams, &iscopy);

	char *cmdLine = NULL;
	if (arg && strlen(arg))
	{
		cmdLine = strdup(arg);
	}

	env->ReleaseStringUTFChars(commandLineParams, arg);

	ALOGV("Command line %s", cmdLine);
	argv = (char**)malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);

	/* verify the argtable[] entries were allocated sucessfully */
	if (arg_nullcheck(argtable) == 0) {
		/* Parse the command line as defined by argtable[] */
		arg_parse(argc, argv, argtable);

        if (ss->count > 0 && ss->dval[0] > 0.0)
        {
            SS_MULTIPLIER = ss->dval[0];
        }

        if (msaa->count > 0 && msaa->ival[0] > 0 && msaa->ival[0] < 10)
        {
            NUM_MULTI_SAMPLES = msaa->ival[0];
        }

        if (refresh->count > 0 && refresh->ival[0] > 0 && refresh->ival[0] <= 120)
        {
            REFRESH = refresh->ival[0];
        }
	}

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity, activityClass );

	surfaceMessageQueue_Enable(&appThread->MessageQueue, true);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);

	return (jlong)((size_t)appThread);
}


JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jlong handle, jobject obj1)
{
	ALOGV( "    GLES3JNILib::onStart()" );


    jniCallbackObj = (jobject)env->NewGlobalRef( obj1);
	jclass callbackClass = env->GetObjectClass( jniCallbackObj);

	android_shutdown = env->GetMethodID(callbackClass,"shutdown","()V");

	android_haptic_event = env->GetMethodID(callbackClass, "haptic_event", "(Ljava/lang/String;IIFF)V");
	android_haptic_stopevent = env->GetMethodID(callbackClass, "haptic_stopevent", "(Ljava/lang/String;)V");
	android_haptic_enable = env->GetMethodID(callbackClass, "haptic_enable", "()V");
	android_haptic_disable = env->GetMethodID(callbackClass, "haptic_disable", "()V");

	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_START, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	surfaceMessageQueue_Enable(&appThread->MessageQueue, false);

	ovrAppThread_Destroy( appThread, env );
	free( appThread );
}

/*
================================================================================

Surface lifecycle

================================================================================
*/

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onSurfaceCreated( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceCreated()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
	appThread->NativeWindow = newNativeWindow;
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
	surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onSurfaceChanged( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceChanged()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	if ( newNativeWindow != appThread->NativeWindow )
	{
		if ( appThread->NativeWindow != NULL )
		{
			srufaceMessage message;
			surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
			surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			srufaceMessage message;
			surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
			surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
			surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
		}
	}
	else if ( newNativeWindow != NULL )
	{
		ANativeWindow_release( newNativeWindow );
	}
}

JNIEXPORT void JNICALL Java_com_drbeef_questzdoom_GLES3JNILib_onSurfaceDestroyed( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onSurfaceDestroyed()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}

}