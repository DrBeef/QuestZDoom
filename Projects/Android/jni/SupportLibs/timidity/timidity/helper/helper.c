#include <stdio.h>
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include "helper.h"

typedef double FLOAT_T;

#define MAX_CHANNELS 32
// So libtimidityplusplus doesn't have to have any android libs
void andro_timidity_log_print(const char* tag, const char* fmt, ...)
{ 
	va_list listPointer;
	va_start(listPointer, fmt);
	__android_log_vprint(ANDROID_LOG_ERROR, tag, fmt, listPointer);
	va_end (listPointer);
}

extern int sfkl_Decode(const char *InFileName, const char *ReqOutFileName);

//extern void timidity_start_initialize(void);
void (*timidity_start)(void);

//extern int timidity_pre_load_configuration(void);
int (*timidity_preload)(void);

//extern int timidity_post_load_configuration(void);
int (*timidity_postload)(void);

//extern void timidity_init_player(void);
void (*timidity_initplayer)(void);

//extern int timidity_play_main(int nfiles, char **files);
int (*timidity_play)(int, char**);

//extern int play_list(int number_of_files, char *list_of_files[]);
int (*ext_play_list)(int, char*[]);

//extern int set_current_resampler(int type);
int (*set_resamp)(int);

//extern void midi_program_change(int ch, int prog);
void (*change_prog)(int, int);

//extern void midi_volume_change(int ch, int prog);
void (*change_vol)(int, int);

//extern int droid_rc;
int *dr_rc;

//extern int droid_arg;
int * dr_arg;

//extern int got_a_configuration;
int * got_config;

//extern FLOAT_T midi_time_ratio;
FLOAT_T * time_ratio;

//extern int opt_preserve_silence;
int * preserve_silence;

char* configFile;
char* configFile2;

int32_t * amplification;
int sixteen;
int mono;
int outputOpen = 0;
int shouldFreeInsts = 1;
int verbosity = -1;

static jclass pushClazz;
static jmethodID pushBuffit;
static jmethodID flushId;
static jmethodID buffId;
static jmethodID controlId;
static jmethodID rateId;
static jmethodID finishId;
static jmethodID seekInitId;
static jmethodID updateSeekId;
static jmethodID pushLyricId;
static jmethodID updateMaxChanId;
static jmethodID updateProgId;
static jmethodID updateVolId;
static jmethodID updateDrumId;
static jmethodID updateTempoId;
static jmethodID updateMaxVoiceId;
static jmethodID updateKeyId;
static JavaVM* mJavaVM;

static int libsLoaded = 0;
static void* libHandle;
static int badState = 0;

static jmethodID pushCmsgId;

static void Android_JNI_ThreadDestroyed(void* value)
{
	/* The thread is being destroyed, detach it from the Java VM and set the mThreadKey value to NULL as required */
	JNIEnv *env = (JNIEnv*) value;
	if (env != NULL)
	{
		(*mJavaVM)->DetachCurrentThread(mJavaVM);
	}
}


// Note: It appears that TiMidity++ is multithreaded, at least enough that we have to get the env this way
static JNIEnv* Android_JNI_GetEnv(void)
{
	/* From http://developer.android.com/guide/practices/jni.html
	 * All threads are Linux threads, scheduled by the kernel.
	 * They're usually started from managed code (using Thread.start), but they can also be created elsewhere and then
	 * attached to the JavaVM. For example, a thread started with pthread_create can be attached with the
	 * JNI AttachCurrentThread or AttachCurrentThreadAsDaemon functions. Until a thread is attached, it has no JNIEnv,
	 * and cannot make JNI calls.
	 * Attaching a natively-created thread causes a java.lang.Thread object to be constructed and added to the "main"
	 * ThreadGroup, making it visible to the debugger. Calling AttachCurrentThread on an already-attached thread
	 * is a no-op.
	 * Note: You can call this function any number of times for the same thread, there's no harm in it
	 */

	JNIEnv *env;
	int status = (*mJavaVM)->GetEnv(mJavaVM, (void**) &env, JNI_VERSION_1_4);
	if (status == JNI_EDETACHED) {
		if ((*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL) != 0) {
			//LOGE("Failed to attach");
		}
	}
	return env;
}
static int Android_JNI_SetupThread(void)
{
	/* From http://developer.android.com/guide/practices/jni.html
	 * Threads attached through JNI must call DetachCurrentThread before they exit. If coding this directly is awkward,
	 * in Android 2.0 (Eclair) and higher you can use pthread_key_create to define a destructor function that will be
	 * called before the thread exits, and call DetachCurrentThread from there. (Use that key with pthread_setspecific
	 * to store the JNIEnv in thread-local-storage; that way it'll be passed into your destructor as the argument.)
	 * Note: The destructor is not called unless the stored value is != NULL
	 * Note: You can call this function any number of times for the same thread, there's no harm in it
	 *       (except for some lost CPU cycles)
	 */
	JNIEnv *env = Android_JNI_GetEnv();
	return 1;
}
extern jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv *env;
	mJavaVM = vm;
	if ((*mJavaVM)->GetEnv(mJavaVM, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		return -1;
	}
	/*
	 * Create mThreadKey so we can keep track of the JNIEnv assigned to each thread
	 * Refer to http://developer.android.com/guide/practices/design/jni.html for the rationale behind this
	 */
	Android_JNI_SetupThread();

	return JNI_VERSION_1_4;
}
extern void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv* env = Android_JNI_GetEnv();
	(*env)->DeleteGlobalRef(env, pushClazz);
}


int checkLibError()
{
	const char* error = dlerror();
	if(error)
	{
		__android_log_print(ANDROID_LOG_ERROR, "TIMIDITY", "%s", error);
		return 1;
	}
	return 0;
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_loadLib(JNIEnv * env, jobject  obj, jstring path)
{
	if(!libsLoaded)
	{
		jboolean isCopy;
		char* libPath =(char*)(*env)->GetStringUTFChars(env, path, &isCopy); 
		dlerror();
		libHandle = dlopen(libPath, RTLD_NOW );

		if(checkLibError())
		{
			return -1;	
		}
		timidity_start = dlsym(libHandle, "timidity_start_initialize");
		if(checkLibError())
		{
			return -2;
		}
		timidity_preload = dlsym(libHandle, "timidity_pre_load_configuration");
		if(checkLibError())
		{
			return -3;
		}
		timidity_postload = dlsym(libHandle, "timidity_post_load_configuration");
		if(checkLibError())
		{
			return -4;
		}
		timidity_initplayer = dlsym(libHandle, "timidity_init_player");
		if(checkLibError())
		{
			return -5;
		}
		timidity_play = dlsym(libHandle, "timidity_play_main");
		if(checkLibError())
		{
			return -6;
		}
		ext_play_list = dlsym(libHandle, "play_list");
		if(checkLibError())
		{
			return -7;
		}
		set_resamp = dlsym(libHandle, "set_current_resampler");
		if(checkLibError())
		{
			return -8;
		}
		change_prog = dlsym(libHandle, "midi_program_change");
		if(checkLibError())
		{
			return -9;
		}
		change_vol = dlsym(libHandle, "midi_volume_change");
		if(checkLibError())
		{
			return -10;
		}
		dr_rc = dlsym(libHandle, "droid_rc");
		if(checkLibError())
		{
			return -11;
		}
		dr_arg = dlsym(libHandle, "droid_arg");
		if(checkLibError())
		{
			return -12;
		}
		got_config = dlsym(libHandle, "got_a_configuration");
		if(checkLibError())
		{
			return -13;
		}
		time_ratio = dlsym(libHandle, "midi_time_ratio");
		if(checkLibError())
		{
			return -14;
		}
		preserve_silence = dlsym(libHandle, "opt_preserve_silence");
		if(checkLibError())
		{
			return -15;
		}
		amplification = dlsym(libHandle, "amplification");
		if(checkLibError())
		{
			return -16;
		}
		libsLoaded = 1;
		(*env)->ReleaseStringUTFChars(env, path, libPath);
		return 0;
	}
	return 1;
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_unloadLib(JNIEnv * env, jobject  obj)
{
	if(libsLoaded&&!libHandle)
	{
		__android_log_print(ANDROID_LOG_WARN, "TIMIDITY", "Nothing to unload");
		return -1; // nothing to do
	}
	int libclose = dlclose(libHandle);
	if(libclose!=0)
	{
		__android_log_print(ANDROID_LOG_ERROR, "TIMIDITY", "Couldn't unload %d", libclose);
	}else{
		libsLoaded = 0;
	}
	checkLibError();
	return 0;
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_prepareTimidity(JNIEnv * env, jobject  obj, jstring config, jstring config2, jint jmono, jint jcustResamp, jint jPresSil, jint jreloading, jint jfreeInsts, jint jverbosity, jint volume)
{
	outputOpen = 0;
	if(!jreloading)
	{
		Android_JNI_SetupThread();
		pushClazz = (*env)->NewGlobalRef(env,(*env)->FindClass(env, "com/xperia64/timidityae/JNIHandler"));
		pushBuffit=(*env)->GetStaticMethodID(env, pushClazz, "buffit", "([BI)V");
		flushId=(*env)->GetStaticMethodID(env, pushClazz, "flushTrack", "()V");
		buffId=(*env)->GetStaticMethodID(env, pushClazz, "bufferSize", "()I");
		controlId=(*env)->GetStaticMethodID(env, pushClazz, "controlCallback", "(I)V");
		buffId=(*env)->GetStaticMethodID(env, pushClazz, "bufferSize", "()I");
		rateId=(*env)->GetStaticMethodID(env, pushClazz, "getRate", "()I");
		finishId=(*env)->GetStaticMethodID(env, pushClazz, "finishCallback", "()V");
		seekInitId=(*env)->GetStaticMethodID(env, pushClazz, "initSeekBar", "(I)V");
		updateSeekId=(*env)->GetStaticMethodID(env, pushClazz, "updateSeekBar", "(II)V");
		pushLyricId=(*env)->GetStaticMethodID(env, pushClazz, "updateLyrics", "([B)V");
		updateMaxChanId=(*env)->GetStaticMethodID(env, pushClazz, "updateMaxChannels", "(I)V");
		updateProgId=(*env)->GetStaticMethodID(env, pushClazz, "updateProgramInfo", "(II)V");
		updateVolId=(*env)->GetStaticMethodID(env, pushClazz, "updateVolInfo", "(II)V");
		updateDrumId=(*env)->GetStaticMethodID(env, pushClazz, "updateDrumInfo", "(II)V");
		updateTempoId=(*env)->GetStaticMethodID(env, pushClazz, "updateTempo", "(II)V");
		updateMaxVoiceId=(*env)->GetStaticMethodID(env, pushClazz, "updateMaxVoice", "(I)V");
		updateKeyId=(*env)->GetStaticMethodID(env, pushClazz, "updateKey", "(I)V");
		pushCmsgId=(*env)->GetStaticMethodID(env, pushClazz, "updateCmsg", "([B)V");
	}

	mono = (int)jmono;
	sixteen = 1;
	shouldFreeInsts = (int)jfreeInsts;
	jboolean isCopy;
	configFile=(char*)(*env)->GetStringUTFChars(env, config, &isCopy); 
	configFile2=(char*)(*env)->GetStringUTFChars(env, config2, &isCopy); 
	int err=0;
	verbosity = jverbosity;

	(*timidity_start)();

	if ((err = (*timidity_preload)()) != 0)
		return err;
	err += (*timidity_postload)();
	if (err) {
		return -121;
	}

	*preserve_silence = (int)jPresSil;
	*amplification = (int)volume;
	(*timidity_initplayer)();
	(*set_resamp)(jcustResamp);
	(*env)->ReleaseStringUTFChars(env, config, configFile);
	(*env)->ReleaseStringUTFChars(env, config2, configFile2);

	return 0;
}
void setMaxChannels(int ca)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateMaxChanId, ca);
}
void finishAE()
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, finishId);
}
	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_loadSongTimidity(JNIEnv * env, jobject  obj, jstring song)
{
	// Must be called once to open output. Thank you mac_main for the NULL file list thing	
	if(!outputOpen)
	{	
		setMaxChannels((int)MAX_CHANNELS);
		(*timidity_play)(0, NULL);
		outputOpen=1;
	}
	int main_ret;
	char *filez[1];
	jboolean isCopy;
	filez[0]=(char*)(*env)->GetStringUTFChars(env, song, &isCopy);
	(*ext_play_list)(1,filez);
	(*env)->ReleaseStringUTFChars(env, song, filez[0]);
	finishAE();

	return 0;
}
	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_setResampleTimidity(JNIEnv * env, jobject  obj, jint jcustResamp)
{
	return (*set_resamp)(jcustResamp);
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_decompressSFArk(JNIEnv * env, jobject  obj, jstring jfrom, jstring jto)
{
	jboolean isCopy;
	const char* from = (*env)->GetStringUTFChars(env, jfrom, &isCopy); 
	const char* to = (*env)->GetStringUTFChars(env, jto, &isCopy); 
	int x = sfkl_Decode(from, to);
	(*env)->ReleaseStringUTFChars(env, jfrom, from);
	(*env)->ReleaseStringUTFChars(env, jto, to);
	return x;
}

	JNIEXPORT void JNICALL
Java_com_xperia64_timidityae_JNIHandler_setChannelTimidity(JNIEnv * env, jobject  obj, jint jchan, jint jprog)
{
	(*change_prog)((int)jchan, (int)jprog);
}
	JNIEXPORT void JNICALL
Java_com_xperia64_timidityae_JNIHandler_setChannelVolumeTimidity(JNIEnv * env, jobject  obj, jint jchan, jint jvol)
{
	(*change_vol)((int)jchan, (int)jvol);
}
char* getConfig()
{
	return configFile;
}
char* getConfig2()
{
	return configFile2;
}
int getFreeInsts()
{
	return shouldFreeInsts;
}
int nativePush(char* buf, int nframes)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	jbyteArray byteArr = (*theGoodEnv)->NewByteArray(theGoodEnv, nframes);
	(*theGoodEnv)->SetByteArrayRegion(theGoodEnv, byteArr , 0, nframes, (jbyte *)buf);
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, pushBuffit, byteArr, nframes);
	(*theGoodEnv)->DeleteLocalRef(theGoodEnv, byteArr);
	return 0;
}
	JNIEXPORT void JNICALL
Java_com_xperia64_timidityae_JNIHandler_controlTimidity(JNIEnv*env, jobject obj, jint jcmd, jint jcmdArg)
{
	(*dr_rc)=(int)jcmd;
	(*dr_arg)=(int)jcmdArg;
	if((*dr_rc)==6) // When else are samples even used w/JNI?
	{
		(*dr_arg)*=(int)((*time_ratio)*getSampleRate()); // I'm not syncing that nasty float to the java side.
	}
}
	JNIEXPORT jboolean JNICALL
Java_com_xperia64_timidityae_JNIHandler_timidityReady(JNIEnv*env, jobject obj)
{
	return ((*dr_rc)?JNI_FALSE:JNI_TRUE);
}
void flushIt()
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticIntMethod(theGoodEnv, pushClazz, flushId);
}
int getBuffer()
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	int r = (int)(*theGoodEnv)->CallStaticIntMethod(theGoodEnv, pushClazz, buffId);
	return r;
}
int getMono()
{
	return mono;
}
int getSixteen()
{
	return sixteen;
}

void setMaxTime(int time)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, seekInitId, time);
}

void setCurrTime(int time, int v)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateSeekId, time, v);
}
void controller(int aa)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, controlId, aa);
}
int getSampleRate()
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	return (*theGoodEnv)->CallStaticIntMethod(theGoodEnv, pushClazz, rateId);
}

void setCurrLyric(char* lyric)
{
	int len = strlen(lyric);
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	jbyteArray byteArr = (*theGoodEnv)->NewByteArray(theGoodEnv, len);
	(*theGoodEnv)->SetByteArrayRegion(theGoodEnv, byteArr , 0, len, (jbyte *)lyric);
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, pushLyricId, byteArr);
	(*theGoodEnv)->DeleteLocalRef(theGoodEnv, byteArr);
}

void setProgram(int ch, int prog)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateProgId, ch, prog);
}
void setVol(int ch, int vol)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateVolId, ch, vol);
}
void setDrum(int ch, int isDrum)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateDrumId, ch, isDrum);
}
void sendTempo(int t, int tr)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateTempoId, t, tr);
}
void sendKey(int k)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateKeyId, k);
}
void sendMaxVoice(int mv)
{
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, updateMaxVoiceId, mv);
}

int getVerbosity()
{
	return verbosity;
}

void andro_timidity_cmsg_print(const char* msg)
{
	int len = strlen(msg);
	JNIEnv* theGoodEnv = Android_JNI_GetEnv();
	jbyteArray byteArr = (*theGoodEnv)->NewByteArray(theGoodEnv, len);
	(*theGoodEnv)->SetByteArrayRegion(theGoodEnv, byteArr , 0, len, (const jbyte *)msg);
	(*theGoodEnv)->CallStaticVoidMethod(theGoodEnv, pushClazz, pushCmsgId, byteArr);
	(*theGoodEnv)->DeleteLocalRef(theGoodEnv, byteArr);
}
