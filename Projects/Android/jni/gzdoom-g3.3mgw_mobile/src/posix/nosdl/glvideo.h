#ifndef __SDLGLVIDEO_H__
#define __SDLGLVIDEO_H__

#include "hardware.h"
#include "v_video.h"

#include "gl/system/gl_system.h"

EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

struct FRenderer;
FRenderer *gl_CreateInterface();

class NoSDLGLVideo : public IVideo
{
 public:
	NoSDLGLVideo (int parm);
	~NoSDLGLVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_FullscreenOnly; }
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool SetResolution (int width, int height, int bits);

	void SetupPixelFormat(bool allowsoftware, int multisample, const int *glver);

private:
	int IteratorMode;
	int IteratorBits;
};

class NoSDLBaseFB : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	using DFrameBuffer::DFrameBuffer;

	friend class NoSDLGLVideo;
};

class NoSDLGLFB : public NoSDLBaseFB
{
	typedef NoSDLBaseFB Super;
public:
	// this must have the same parameters as the Windows version, even if they are not used!
	NoSDLGLFB (void *hMonitor, int width, int height, int, int, bool fullscreen, bool bgra); 
	~NoSDLGLFB ();

	void ForceBuffering (bool force);
	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();

	bool IsValid ();
	bool IsFullscreen ();

	virtual void SetVSync( bool vsync );
	void SwapBuffers();
	
	void NewRefreshRate ();

	friend class NoSDLGLVideo;

	int GetClientWidth();
	int GetClientHeight();

	virtual void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	virtual int GetTrueHeight() { return GetClientHeight(); }
protected:
	void SetGammaTable(uint16_t *tbl);
	void ResetGammaTable();
	void InitializeState();

	NoSDLGLFB () {}
	uint8_t GammaTable[3][256];
	bool UpdatePending;

//	SDL_Window *Screen;

//	SDL_GLContext GLContext;

	void UpdateColors ();

	int m_Lock;
	bool m_supportsGamma;
};
#endif
