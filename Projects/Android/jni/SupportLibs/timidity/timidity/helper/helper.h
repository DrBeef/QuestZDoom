#ifndef _TIMIDITY_HELPER_H_
#define _TIMIDITY_HELPER_H_
	extern int getMono();
	extern int getSixteen();
	extern int getSampleRate();
	extern void controller(int aa);
	extern int nativePush(char* buf, int nframes);
	extern int getBuffer();
	extern void finishAE();
	extern void setMaxTime(int time);
	extern void setCurrTime(int time, int v);
	extern void sendMaxVoice(int mv);
	extern void setCurrLyric(char* lyric);
	extern void setProgram(int ch, int prog);
	extern void setDrum(int ch, int isDrum);
	extern void setVol(int ch, int vol);
	extern void sendTempo(int t, int tr);
	extern void sendKey(int k);
	extern int getVerbosity();
	extern void andro_timidity_log_print(const char* tag, const char* fmt, ...);
	extern void andro_timidity_cmsg_print(const char* msg);
#endif
