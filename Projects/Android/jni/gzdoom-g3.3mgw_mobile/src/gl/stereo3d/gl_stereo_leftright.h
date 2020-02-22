// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
** gl_stereo_leftright.h
** Offsets for left and right eye views
**
*/

#ifndef GL_STEREO_LEFTRIGHT_H_
#define GL_STEREO_LEFTRIGHT_H_

#include "gl_stereo3d.h"

namespace s3d {


class ShiftedEyePose : public EyePose
{
public:
	ShiftedEyePose(float shift) : shift(shift) {};
	float getShift() const;
	virtual VSMatrix GetProjection(float fov, float aspectRatio, float fovRatio) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;

protected:
	void setShift(float shift) { this->shift = shift; }

private:
	float shift;
};


class LeftEyePose : public ShiftedEyePose
{
public:
	LeftEyePose(float ipd) : ShiftedEyePose( float(-0.5) * ipd) {}
	float getIpd() const { return float(fabs(2.0f*getShift())); }
	void setIpd(float ipd) { setShift(float(-0.5)*ipd); }
};


class RightEyePose : public ShiftedEyePose
{
public:
	RightEyePose(float ipd) : ShiftedEyePose(float(+0.5)*ipd) {}
	float getIpd() const { return float(fabs(2.0f*getShift())); }
	void setIpd(float ipd) { setShift(float(+0.5)*ipd); }
};


/**
 * As if viewed through the left eye only
 */
class LeftEyeView : public Stereo3DMode
{
public:
	static const LeftEyeView& getInstance(float ipd);

	LeftEyeView(float ipd) : eye(ipd) { eye_ptrs.Push(&eye); }
	float getIpd() const { return eye.getIpd(); }
	void setIpd(float ipd) { eye.setIpd(ipd); }
	void Present() const override;
protected:
	LeftEyePose eye;
};


class RightEyeView : public Stereo3DMode
{
public:
	static const RightEyeView& getInstance(float ipd);

	RightEyeView(float ipd) : eye(ipd) { eye_ptrs.Push(&eye); }
	float getIpd() const { return eye.getIpd(); }
	void setIpd(float ipd) { eye.setIpd(ipd); }
	void Present() const override;
protected:
	RightEyePose eye;
};


} /* namespace s3d */

#endif /* GL_STEREO_LEFTRIGHT_H_ */
