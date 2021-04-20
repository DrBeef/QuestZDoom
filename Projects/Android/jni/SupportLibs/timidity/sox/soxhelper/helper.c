#include <jni.h>
#include <stdlib.h>

#include "sox.h"

int outputOpen = 0;
int rate = 44100;
static jclass pushClazz;
static jmethodID pushBuffit;
static jmethodID seekInitId;
static jmethodID updateSeekId;
static jmethodID soxOverId;
static JavaVM* mJavaVM;

size_t buffer_size;
char* buffer;

int jumps = -1;
float timescale = 1;
int irate;
int ichan;
int stopSox = 0; 

sox_format_t *in;
sox_format_t *out;

JNIEnv *playEnv; // The (*env) from the soxPlay method should be safe to use in all other callbacks
				// as the callbacks can only occur while playing

int fail_flag = 0;

uint64_t currPos = 0;

static int update_status(sox_bool all_done, void * client_data)
{
	if(jumps>=0)
	{
		uint64_t jump = irate*jumps*2; // Input sample rate / where to jump to
		currPos = rate*jumps*timescale*2;
		sox_seek(in, jump, SOX_SEEK_SET);
		jumps = -1;
	}
	int read_time = currPos/(rate*timescale*2);
	(*playEnv)->CallStaticVoidMethod(playEnv, pushClazz, updateSeekId, read_time, -1);
	return (stopSox ? SOX_EOF : SOX_SUCCESS);
}

void droid_fail()
{
	fail_flag = 1;
}

int nativePush(const int16_t *buf, int nframes)
{
	currPos+=nframes;
	jshortArray sArr = (*playEnv)->NewShortArray(playEnv, nframes);
	(*playEnv)->SetShortArrayRegion(playEnv, sArr, 0, nframes, (jshort *)buf);
	(*playEnv)->CallStaticVoidMethod(playEnv, pushClazz, pushBuffit, sArr, nframes);
	(*playEnv)->DeleteLocalRef(playEnv, sArr);	
	return 0;
}

void soxOverDone()
{
	(*playEnv)->CallStaticVoidMethod(playEnv, pushClazz, soxOverId);
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_soxInit(JNIEnv * env, jobject this, jint jreloading, jint jrate)
{
	int retcode = 0;
	if(!jreloading)
	{
		retcode = sox_init();
		pushClazz = (*env)->NewGlobalRef(env,(*env)->FindClass(env, "com/xperia64/timidityae/JNIHandler"));
		pushBuffit=(*env)->GetStaticMethodID(env, pushClazz, "buffsox", "([SI)V");
		seekInitId=(*env)->GetStaticMethodID(env, pushClazz, "initSeekBar", "(I)V");
		updateSeekId=(*env)->GetStaticMethodID(env, pushClazz, "updateSeekBar", "(II)V");
		soxOverId = (*env)->GetStaticMethodID(env, pushClazz, "soxOverDone", "()V");
	}
	rate = jrate;
	return retcode;
}

	JNIEXPORT int JNICALL
Java_com_xperia64_timidityae_JNIHandler_soxPlay(JNIEnv * env, jobject this, jstring jfilename, jobjectArray jeffects, jint jignoreSafety)
{
	playEnv = env;	
	stopSox = 0;
	currPos = 0;
	fail_flag = 0;
	char * sargs[2]; // Static arguments for internal use
	jboolean isCopy;

	char *  filename = (char*)(*env)->GetStringUTFChars(env, jfilename, &isCopy);
	sox_effect_t * e;	
	sox_effects_chain_t * chain;

	in = sox_open_read(filename, NULL, NULL, NULL);
	irate = in->signal.rate;
	ichan = in->signal.channels;
	int ilength = in->signal.length;

	(*env)->CallStaticVoidMethod(env, pushClazz, seekInitId, ilength/(irate*ichan));
	
	out = sox_open_write("default", &in->signal, NULL, "droid", NULL, NULL);
	out->signal.rate = rate;
	out->signal.length = SOX_UNSPEC;
	chain = sox_create_effects_chain(&in->encoding, &out->encoding);

	e = sox_create_effect(sox_find_effect("input"));
	sargs[0] = (char *) in;
	sox_effect_options(e, 1, sargs);
	sox_add_effect(chain, e, &in->signal, &in->signal);
	free(e);
	
	if (in->signal.channels == 1) {
		e = sox_create_effect(sox_find_effect("remix"));
		sargs[0] = "1";
		sargs[1] = "1";
		sox_effect_options(e, 2, sargs);
		sox_add_effect(chain, e, &in->signal, &out->signal);
		free(e);

		// TODO: Figure out why single channel MP3s play at 2x speed with 1 channel and 0.5x speed when remixed to 2
		if(in->encoding.encoding == SOX_ENCODING_MP3)
		{
			in->signal.rate *= 2;
		}
	}

	int numEffects = (*env)->GetArrayLength(env, jeffects);
	if(numEffects>0)
	{
		int o;
		for(o = 0; o<numEffects; o++)
		{
			jobjectArray effectArray = (jobjectArray) ((*env)->GetObjectArrayElement(env, jeffects, o));
			int strcnt = (*env)->GetArrayLength(env, effectArray);
			if(strcnt>0)
			{
				int i;
				int argcnt = 0;
				const char* effName;
				char ** args;
				if(strcnt>1)
				{
					argcnt = strcnt-1;
					args = malloc(argcnt*sizeof(char*));
				}
				for (i=0; i<strcnt; i++) {
					jstring stringy = (jstring) ((*env)->GetObjectArrayElement(env, effectArray, i));
					char *rawString = (char*)(*env)->GetStringUTFChars(env, stringy, 0);
					if(i>0)
					{	
						args[i-1] = rawString;
					}else{
						if(!sox_find_effect(rawString))
						{
							(*env)->ReleaseStringUTFChars(env, stringy, rawString);
							if(argcnt>0)
								free(args);
							(*env)->ReleaseStringUTFChars(env, jfilename, filename);
							return -o;
						}
						e = sox_create_effect(sox_find_effect(effName = rawString));
					}
				}
				sox_effect_options(e, argcnt, args);
				if(fail_flag && !jignoreSafety)
				{
					goto cleanup;
				}
				sox_add_effect(chain, e, &in->signal, &in->signal);
cleanup:
				free(e);
				jstring stringy2 = (jstring) ((*env)->GetObjectArrayElement(env, effectArray, 0));
				(*env)->ReleaseStringUTFChars(env, stringy2, effName);
				for (i=0; i<argcnt; i++) {
					jstring stringy = (jstring) ((*env)->GetObjectArrayElement(env, effectArray, i+1));
					(*env)->ReleaseStringUTFChars(env, stringy, args[i]);
				}
				if(argcnt>0)
					free(args);
				if(fail_flag && !jignoreSafety)
				{
					return -o;
				}
			}

		}
	}

	if (in->signal.rate != out->signal.rate) {
		e = sox_create_effect(sox_find_effect("rate"));
		sox_effect_options(e, 0, NULL);
		sox_add_effect(chain, e, &in->signal, &out->signal);
		free(e);
	}

	timescale = ((float)in->signal.length/in->signal.rate)/((float)ilength/irate);

	e = sox_create_effect(sox_find_effect("output"));
	sargs[0] = (char *) out;
	sox_effect_options(e, 1, sargs);
	sox_add_effect(chain, e, &out->signal, &out->signal);
	free(e);

	sox_flow_effects(chain, update_status, NULL);
	sox_delete_effects_chain(chain);
	sox_close(out);
	sox_close(in);

	(*env)->ReleaseStringUTFChars(env, jfilename, filename);

	return 1;
}

	JNIEXPORT void JNICALL
Java_com_xperia64_timidityae_JNIHandler_soxSeek(JNIEnv * env, jobject this, jint jtime)
{
	jumps = jtime;
}

	JNIEXPORT void JNICALL
Java_com_xperia64_timidityae_JNIHandler_soxStop(JNIEnv * env, jobject this)
{
	stopSox = 1;
}

