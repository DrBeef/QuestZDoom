/************************************************************************************

Filename	:	VrInputRight.c 
Content		:	Handles common controller input functionality
Created		:	September 2019
Authors		:	Simon Brown

*************************************************************************************/

#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_SystemUtils.h>
#include <VrApi_Input.h>
#include <VrApi_Types.h>

#include "VrInput.h"

void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);

void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key)
{
    Joy_GenerateButtonEvents(prevTrackedRemoteState->Buttons & button ? 1 : 0, trackedRemoteState->Buttons & button ? 1 : 0, 1, key);
}

static void Matrix4x4_Transform (const matrix4x4 *in, const float v[3], float out[3])
{
    out[0] = v[0] * (*in)[0][0] + v[1] * (*in)[0][1] + v[2] * (*in)[0][2] + (*in)[0][3];
    out[1] = v[0] * (*in)[1][0] + v[1] * (*in)[1][1] + v[2] * (*in)[1][2] + (*in)[1][3];
    out[2] = v[0] * (*in)[2][0] + v[1] * (*in)[2][1] + v[2] * (*in)[2][2] + (*in)[2][3];
}

void Matrix4x4_CreateFromEntity( matrix4x4 out, const vec3_t angles, const vec3_t origin, float scale );

void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out)
{
    vec3_t temp = {0.0f, 0.0f, 0.0f};
    temp[0] = v1;
    temp[1] = v2;

    vec3_t v = {0.0f, 0.0f, 0.0f};
    matrix4x4 matrix;
    vec3_t angles = {0.0f, rotation, 0.0f};
    vec3_t origin = {0.0f, 0.0f, 0.0f};
    Matrix4x4_CreateFromEntity(matrix, angles, origin, 1.0f);
    Matrix4x4_Transform(&matrix, temp, v);

    out[0] = v[0];
    out[1] = v[1];
}

float length(float x, float y)
{
    return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

#define NLF_DEADZONE 0.1
#define NLF_POWER 2.2

float nonLinearFilter(float in)
{
    float val = 0.0f;
    if (in > NLF_DEADZONE)
    {
        val = in;
        val -= NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = powf(val, NLF_POWER);
    }
    else if (in < -NLF_DEADZONE)
    {
        val = in;
        val += NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = -powf(fabsf(val), NLF_POWER);
    }

    return val;
}

bool between(float min, float val, float max)
{
    return (min < val) && (val < max);
}

void acquireTrackedRemotesData(const ovrMobile *Ovr, double displayTime) {//The amount of yaw changed by controller
    for ( int i = 0; ; i++ ) {
        ovrInputCapabilityHeader capsHeader;
        ovrResult result = vrapi_EnumerateInputDevices(Ovr, i, &capsHeader);
        if (result < 0) {
            break;
        }

        if (capsHeader.Type == ovrControllerType_Gamepad) {

            ovrInputGamepadCapabilities remoteCaps;
            remoteCaps.Header = capsHeader;
            if (vrapi_GetInputDeviceCapabilities(Ovr, &remoteCaps.Header) >= 0) {
                // remote is connected
                ovrInputStateGamepad remoteState;
                remoteState.Header.ControllerType = ovrControllerType_Gamepad;
                if ( vrapi_GetCurrentInputState( Ovr, capsHeader.DeviceID, &remoteState.Header ) >= 0 )
                {
                    // act on device state returned in remoteState
                    footTrackedRemoteState_new = remoteState;
                }
            }
        }
        else if (capsHeader.Type == ovrControllerType_TrackedRemote) {
            ovrTracking remoteTracking;
            ovrInputTrackedRemoteCapabilities remoteCaps;
            remoteCaps.Header = capsHeader;
            if ( vrapi_GetInputDeviceCapabilities( Ovr, &remoteCaps.Header ) >= 0 )
            {
                // remote is connected
                ovrInputStateTrackedRemote remoteState;
                remoteState.Header.ControllerType = ovrControllerType_TrackedRemote;

                if(vrapi_GetCurrentInputState(Ovr, capsHeader.DeviceID, &remoteState.Header) >= 0) {
                    if (vrapi_GetInputTrackingState(Ovr, capsHeader.DeviceID, displayTime,
                                                    &remoteTracking) >= 0) {
                        // act on device state returned in remoteState
                        if (remoteCaps.ControllerCapabilities & ovrControllerCaps_RightHand) {
                            rightTrackedRemoteState_new = remoteState;
                            rightRemoteTracking_new = remoteTracking;
                            controllerIDs[1] = capsHeader.DeviceID;
                        } else {
                            leftTrackedRemoteState_new = remoteState;
                            leftRemoteTracking_new = remoteTracking;
                            controllerIDs[0] = capsHeader.DeviceID;
                        }
                    }
                }
            }
        }
    }
}
