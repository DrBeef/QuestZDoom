#ifndef __QDRAWER_H
#define __QDRAWER_H

#include "gl/data/gl_vertexbuffer.h"

class FQuadDrawer
{
	FFlatVertex *p;
	int ndx;
	static FFlatVertex buffer[4];
	
	void DoRender(int type);
public:

	FQuadDrawer()
	{
		if (gl.buffermethod == BM_DEFERRED)
		{
			p = buffer;
		}
		else
		{
			p = GLRenderer->mVBO->Alloc(4, &ndx);
		}
	}
	void Set(int ndx, float x, float y, float z, float s, float t)
	{
#ifdef NO_VBO
        if(gl.novbo)
        {
           buffer[ndx].Set(x, y, z, s, t);
        }
        else
#endif
		p[ndx].Set(x, y, z, s, t);
	}
	void Render(int type)
	{
		if (gl.buffermethod == BM_DEFERRED)
		{
			DoRender(type);
		}
		else
		{
#ifdef NO_VBO
            if(gl.novbo)
            {
                glTexCoordPointer(2,GL_FLOAT, sizeof(FFlatVertex),&buffer[0].u);
                glVertexPointer  (3,GL_FLOAT, sizeof(FFlatVertex),&buffer[0].x);

                glEnableClientState (GL_VERTEX_ARRAY);
                glEnableClientState (GL_TEXTURE_COORD_ARRAY);
                glDisableClientState (GL_COLOR_ARRAY);

                glBindBuffer (GL_ARRAY_BUFFER, 0); // NO VBO
                glDrawArrays (type, 0, 4);
            }
            else
#endif
			GLRenderer->mVBO->RenderArray(type, ndx, 4);
		}
	}
};


#endif
