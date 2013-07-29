/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_state_latch.c
 *  \ingroup gpu
 */

#include "gpu_state_latch.h"

#include "gpu_glew.h"
#include "gpu_profile.h"
#include "gpu_safety.h"

#include <string.h>



// XXX jwilkins: this may need to be extended to save these values in different contexts later,
// but Blender doesn't use multiple contexts, so it isn't a problem right now.



static GLdouble depth_range[2] = { 0, 1 };

void gpuDepthRange(GLdouble near, GLdouble far)
{
	GPU_ASSERT(near != far);
	GPU_CHECK_NO_ERROR();

	depth_range[0] = near;
	depth_range[1] = far;

#if !defined(GLEW_ES_ONLY)
	if (!GPU_PROFILE_ES20) {
		glDepthRange(near, far);

		GPU_CHECK_NO_ERROR();
		return;
	}
#endif

#if !defined(GLEW_NO_ES)
	if (GPU_PROFILE_ES20) {
		glDepthRangef((GLfloat)near, (GLfloat)far);

		GPU_CHECK_NO_ERROR();
		return;
	}
#endif
}



void gpuGetDepthRange(GLdouble out[2])
{
	GPU_CHECK_NO_ERROR();

	memcpy(out, depth_range, sizeof(depth_range));
}



GLfloat gpuFeedbackDepthRange(GLfloat z)
{
	GLfloat depth;

	GPU_CHECK_NO_ERROR();

	depth = depth_range[1] - depth_range[0];

	if (depth != 0) {
		return z / depth;
	}
	else {
		GPU_ABORT();
		return z;
	}
}



static GLuint texture_binding_2D = 0;

void gpuBindTexture(GLenum target, GLuint name)
{
	GPU_CHECK_NO_ERROR();

	switch(target)
	{
		case GL_TEXTURE_2D:
			texture_binding_2D = name;
			break;

		default:
			/* a target we don't care to latch */
			break;
	}

	glBindTexture(target, name);

	GPU_CHECK_NO_ERROR();
}



GLuint gpuGetTextureBinding2D(void)
{
	GPU_CHECK_NO_ERROR();

	return texture_binding_2D;
}



static GLboolean depth_writemask = GL_TRUE;

void gpuDepthMask(GLboolean flag)
{
	GPU_CHECK_NO_ERROR();

	depth_writemask = flag;
	glDepthMask(flag);

	GPU_CHECK_NO_ERROR();
}



GLboolean gpuGetDepthWritemask(void)
{
	GPU_CHECK_NO_ERROR();

	return depth_writemask;
}

