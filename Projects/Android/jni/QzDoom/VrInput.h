
#if !defined(vrinput_h)
#define vrinput_h

#include "VrCommon.h"



void acquireTrackedRemotesData(const ovrMobile *Ovr, double displayTime);

void HandleInput_Default( int control_scheme, ovrInputStateGamepad *pFootTrackingNew, ovrInputStateGamepad *pFootTrackingOld, ovrInputStateTrackedRemote *pDominantTrackedRemoteNew,
                         ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 );


#endif //vrinput_h