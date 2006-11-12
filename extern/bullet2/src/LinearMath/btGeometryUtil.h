/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#ifndef BT_GEOMETRY_UTIL_H
#define BT_GEOMETRY_UTIL_H
#include <vector>
#include "btVector3.h"

class btGeometryUtil
{
	public:
	
	
		static void	getPlaneEquationsFromVertices(std::vector<btVector3>& vertices, std::vector<btVector3>& planeEquationsOut );

		static void	getVerticesFromPlaneEquations(const std::vector<btVector3>& planeEquations , std::vector<btVector3>& verticesOut );
	
		static bool	isInside(const std::vector<btVector3>& vertices, const btVector3& planeNormal, float	margin);
		
		static bool	isPointInsidePlanes(const std::vector<btVector3>& planeEquations, const btVector3& point, float	margin);

		static bool	areVerticesBehindPlane(const btVector3& planeNormal, const std::vector<btVector3>& vertices, float	margin);

};


#endif //BT_GEOMETRY_UTIL_H