/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "btConeShape.h"
#include "LinearMath/btPoint3.h"



btConeShape::btConeShape (btScalar radius,btScalar height):
m_radius (radius),
m_height(height)
{
	setConeUpIndex(1);
	btVector3 halfExtents;
	m_sinAngle = (m_radius / sqrt(m_radius * m_radius + m_height * m_height));
}

///choose upAxis index
void	btConeShape::setConeUpIndex(int upIndex)
{
	switch (upIndex)
	{
	case 0:
			m_coneIndices[0] = 1;
			m_coneIndices[1] = 0;
			m_coneIndices[2] = 2;
		break;
	case 1:
			m_coneIndices[0] = 0;
			m_coneIndices[1] = 1;
			m_coneIndices[2] = 2;
		break;
	case 2:
			m_coneIndices[0] = 0;
			m_coneIndices[1] = 2;
			m_coneIndices[2] = 1;
		break;
	default:
		assert(0);
	};
}

btVector3 btConeShape::coneLocalSupport(const btVector3& v) const
{
	
	float halfHeight = m_height * 0.5f;

 if (v[m_coneIndices[1]] > v.length() * m_sinAngle)
 {
	btVector3 tmp;

	tmp[m_coneIndices[0]] = 0.f;
	tmp[m_coneIndices[1]] = halfHeight;
	tmp[m_coneIndices[2]] = 0.f;
	return tmp;
 }
  else {
    btScalar s = btSqrt(v[m_coneIndices[0]] * v[m_coneIndices[0]] + v[m_coneIndices[2]] * v[m_coneIndices[2]]);
    if (s > SIMD_EPSILON) {
      btScalar d = m_radius / s;
	  btVector3 tmp;
	  tmp[m_coneIndices[0]] = v[m_coneIndices[0]] * d;
	  tmp[m_coneIndices[1]] = -halfHeight;
	  tmp[m_coneIndices[2]] = v[m_coneIndices[2]] * d;
	  return tmp;
    }
    else  {
		btVector3 tmp;
		tmp[m_coneIndices[0]] = 0.f;
		tmp[m_coneIndices[1]] = -halfHeight;
		tmp[m_coneIndices[2]] = 0.f;
		return tmp;
	}
  }

}

btVector3	btConeShape::localGetSupportingVertexWithoutMargin(const btVector3& vec) const
{
		return coneLocalSupport(vec);
}

void	btConeShape::batchedUnitVectorGetSupportingVertexWithoutMargin(const btVector3* vectors,btVector3* supportVerticesOut,int numVectors) const
{
	for (int i=0;i<numVectors;i++)
	{
		const btVector3& vec = vectors[i];
		supportVerticesOut[i] = coneLocalSupport(vec);
	}
}


btVector3	btConeShape::localGetSupportingVertex(const btVector3& vec)  const
{
	btVector3 supVertex = coneLocalSupport(vec);
	if ( getMargin()!=0.f )
	{
		btVector3 vecnorm = vec;
		if (vecnorm .length2() < (SIMD_EPSILON*SIMD_EPSILON))
		{
			vecnorm.setValue(-1.f,-1.f,-1.f);
		} 
		vecnorm.normalize();
		supVertex+= getMargin() * vecnorm;
	}
	return supVertex;
}


