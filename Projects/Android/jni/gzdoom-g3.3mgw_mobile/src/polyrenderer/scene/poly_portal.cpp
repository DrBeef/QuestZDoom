/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "p_maputl.h"
#include "sbar.h"
#include "g_levellocals.h"
#include "r_data/r_translate.h"
#include "poly_portal.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/scene/poly_scene.h"

/////////////////////////////////////////////////////////////////////////////

PolyDrawSectorPortal::PolyDrawSectorPortal(FSectorPortal *portal, bool ceiling) : Portal(portal), Ceiling(ceiling)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

void PolyDrawSectorPortal::Render(int portalDepth)
{
	if (Portal->mType == PORTS_HORIZON || Portal->mType == PORTS_PLANE)
		return;

	/*angle_t angle1 = PolyCull::PointToPseudoAngle(v1->fX(), v1->fY());
	angle_t angle2 = PolyCull::PointToPseudoAngle(v2->fX(), v2->fY());
	Segments.clear();
	Segments.push_back({ angle1, angle2 });*/

	SaveGlobals();

	PortalViewpoint = PolyRenderer::Instance()->SetupPerspectiveMatrix();
	PortalViewpoint.StencilValue = StencilValue;
	PortalViewpoint.PortalDepth = portalDepth;
	PortalViewpoint.PortalEnterSector = Portal->mDestination;

	PolyRenderer::Instance()->Scene.Render(&PortalViewpoint);
	
	RestoreGlobals();
}

void PolyDrawSectorPortal::SaveGlobals()
{
	auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Viewwindow;

	SavedViewpoint = viewpoint;
	SavedInvisibility = viewpoint.camera ? (viewpoint.camera->renderflags & RF_INVISIBLE) == RF_INVISIBLE : false;

	if (Portal->mType == PORTS_SKYVIEWPOINT)
	{
		// Don't let gun flashes brighten the sky box
		AActor *sky = Portal->mSkybox;
		viewpoint.extralight = 0;
		//PolyRenderer::Instance()->Light.SetVisibility(sky->args[0] * 0.25f);
		viewpoint.Pos = sky->InterpolatedPosition(viewpoint.TicFrac);
		viewpoint.Angles.Yaw = SavedViewpoint.Angles.Yaw + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * viewpoint.TicFrac);
	}
	else //if (Portal->mType == PORTS_STACKEDSECTORTHING || Portal->mType == PORTS_PORTAL || Portal->mType == PORTS_LINKEDPORTAL)
	{
		//extralight = pl->extralight;
		//SetVisibility(pl->visibility);
		viewpoint.Pos.X += Portal->mDisplacement.X;
		viewpoint.Pos.Y += Portal->mDisplacement.Y;
	}

	viewpoint.camera = nullptr;
	viewpoint.sector = Portal->mDestination;
	R_SetViewAngle(viewpoint, viewwindow);

	Portal->mFlags |= PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags |= PORTSF_INSKYBOX;
}

void PolyDrawSectorPortal::RestoreGlobals()
{
	Portal->mFlags &= ~PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags &= ~PORTSF_INSKYBOX;

	auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Viewwindow;

	viewpoint = SavedViewpoint;

	if (viewpoint.camera)
	{
		if (SavedInvisibility)
			viewpoint.camera->renderflags |= RF_INVISIBLE;
		else
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;
	}

	//PolyRenderer::Instance()->Light.SetVisibility(savedvisibility);

	R_SetViewAngle(viewpoint, viewwindow);
}

/////////////////////////////////////////////////////////////////////////////

PolyDrawLinePortal::PolyDrawLinePortal(FLinePortal *portal) : Portal(portal)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

PolyDrawLinePortal::PolyDrawLinePortal(line_t *mirror) : Mirror(mirror)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

void PolyDrawLinePortal::Render(int portalDepth)
{
	SaveGlobals();

	// Find portal destination line and make sure it faces the right way
	line_t *clipLine = Portal ? Portal->mDestination : Mirror;
	DVector2 pt1 = clipLine->v1->fPos() - PolyRenderer::Instance()->Viewpoint.Pos;
	DVector2 pt2 = clipLine->v2->fPos() - PolyRenderer::Instance()->Viewpoint.Pos;
	bool backfacing = (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0);

	PortalViewpoint = PolyRenderer::Instance()->SetupPerspectiveMatrix(Mirror != nullptr);
	PortalViewpoint.StencilValue = StencilValue;
	PortalViewpoint.PortalDepth = portalDepth;
	PortalViewpoint.PortalEnterLine = clipLine;
	PortalViewpoint.PortalEnterSector = backfacing ? clipLine->frontsector : clipLine->backsector;

	PolyRenderer::Instance()->Scene.Render(&PortalViewpoint);

	RestoreGlobals();
}

void PolyDrawLinePortal::SaveGlobals()
{
	auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Viewwindow;

	SavedViewpoint = viewpoint;
	SavedInvisibility = viewpoint.camera ? (viewpoint.camera->renderflags & RF_INVISIBLE) == RF_INVISIBLE : false;

	if (Mirror)
	{
		DAngle startang = viewpoint.Angles.Yaw;
		DVector3 startpos = viewpoint.Pos;

		vertex_t *v1 = Mirror->v1;

		// Reflect the current view behind the mirror.
		if (Mirror->Delta().X == 0)
		{ // vertical mirror
			viewpoint.Pos.X = v1->fX() - startpos.X + v1->fX();
		}
		else if (Mirror->Delta().Y == 0)
		{ // horizontal mirror
			viewpoint.Pos.Y = v1->fY() - startpos.Y + v1->fY();
		}
		else
		{ // any mirror
			vertex_t *v2 = Mirror->v2;

			double dx = v2->fX() - v1->fX();
			double dy = v2->fY() - v1->fY();
			double x1 = v1->fX();
			double y1 = v1->fY();
			double x = startpos.X;
			double y = startpos.Y;

			// the above two cases catch len == 0
			double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

			viewpoint.Pos.X = (x1 + r * dx) * 2 - x;
			viewpoint.Pos.Y = (y1 + r * dy) * 2 - y;
		}
		viewpoint.Angles.Yaw = Mirror->Delta().Angle() * 2 - startang;

		if (viewpoint.camera)
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;
	}
	else
	{
		auto src = Portal->mOrigin;
		auto dst = Portal->mDestination;

		P_TranslatePortalXY(src, viewpoint.Pos.X, viewpoint.Pos.Y);
		P_TranslatePortalZ(src, viewpoint.Pos.Z);
		P_TranslatePortalAngle(src, viewpoint.Angles.Yaw);
		P_TranslatePortalXY(src, viewpoint.Path[0].X, viewpoint.Path[0].Y);
		P_TranslatePortalXY(src, viewpoint.Path[1].X, viewpoint.Path[1].Y);

		if (viewpoint.camera && !viewpoint.showviewer)
			viewpoint.camera->renderflags |= RF_INVISIBLE;

		/* What is this code trying to do?
		if (viewpoint.camera)
		{
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;

			if (!viewpoint.showviewer && P_PointOnLineSidePrecise(viewpoint.Path[0], dst) != P_PointOnLineSidePrecise(viewpoint.Path[1], dst))
			{
				double distp = (viewpoint.Path[0] - viewpoint.Path[1]).Length();
				if (distp > EQUAL_EPSILON)
				{
					double dist1 = (viewpoint.Pos - viewpoint.Path[0]).Length();
					double dist2 = (viewpoint.Pos - viewpoint.Path[1]).Length();

					if (dist1 + dist2 < distp + 1)
					{
						viewpoint.camera->renderflags |= RF_INVISIBLE;
					}
				}
			}
		}
		*/
	}

	viewpoint.camera = nullptr;
	viewpoint.sector = R_PointInSubsector(viewpoint.Pos)->sector;

	R_SetViewAngle(viewpoint, viewwindow);
}

void PolyDrawLinePortal::RestoreGlobals()
{
	auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Viewwindow;

	viewpoint = SavedViewpoint;

	if (viewpoint.camera)
	{
		if (SavedInvisibility)
			viewpoint.camera->renderflags |= RF_INVISIBLE;
		else
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;
	}

	R_SetViewAngle(viewpoint, viewwindow);
}
