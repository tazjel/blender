/*

Copyright (c) 2005 Gino van den Bergen / Erwin Coumans <www.erwincoumans.com>

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/


#ifndef SimdTransform_H
#define SimdTransform_H

#include "SimdVector3.h"
#include "SimdMatrix3x3.h"



class SimdTransform {
	enum { 
		TRANSLATION = 0x01,
		ROTATION    = 0x02,
		RIGID       = TRANSLATION | ROTATION,  
		SCALING     = 0x04,
		LINEAR      = ROTATION | SCALING,
		AFFINE      = TRANSLATION | LINEAR
	};

public:
	SimdTransform() {}

	//	template <typename Scalar2>
	//	explicit Transform(const Scalar2 *m) { setValue(m); }

	explicit SIMD_FORCE_INLINE SimdTransform(const SimdQuaternion& q, 
		const SimdVector3& c = SimdVector3(SimdScalar(0), SimdScalar(0), SimdScalar(0))) 
		: m_basis(q),
		m_origin(c),
		m_type(RIGID)
	{}

	explicit SIMD_FORCE_INLINE SimdTransform(const SimdMatrix3x3& b, 
		const SimdVector3& c = SimdVector3(SimdScalar(0), SimdScalar(0), SimdScalar(0)), 
		unsigned int type = AFFINE)
		: m_basis(b),
		m_origin(c),
		m_type(type)
	{}


		SIMD_FORCE_INLINE void mult(const SimdTransform& t1, const SimdTransform& t2) {
			m_basis = t1.m_basis * t2.m_basis;
			m_origin = t1(t2.m_origin);
			m_type = t1.m_type | t2.m_type;
		}

		void multInverseLeft(const SimdTransform& t1, const SimdTransform& t2) {
			SimdVector3 v = t2.m_origin - t1.m_origin;
			if (t1.m_type & SCALING) {
				SimdMatrix3x3 inv = t1.m_basis.inverse();
				m_basis = inv * t2.m_basis;
				m_origin = inv * v;
			}
			else {
				m_basis = SimdMultTransposeLeft(t1.m_basis, t2.m_basis);
				m_origin = v * t1.m_basis;
			}
			m_type = t1.m_type | t2.m_type;
		}

	SIMD_FORCE_INLINE SimdVector3 operator()(const SimdVector3& x) const
	{
		return SimdVector3(m_basis[0].dot(x) + m_origin[0], 
			m_basis[1].dot(x) + m_origin[1], 
			m_basis[2].dot(x) + m_origin[2]);
	}

	SIMD_FORCE_INLINE SimdVector3 operator*(const SimdVector3& x) const
	{
		return (*this)(x);
	}

	SIMD_FORCE_INLINE SimdMatrix3x3&       getBasis()          { return m_basis; }
	SIMD_FORCE_INLINE const SimdMatrix3x3& getBasis()    const { return m_basis; }

	SIMD_FORCE_INLINE SimdVector3&         getOrigin()         { return m_origin; }
	SIMD_FORCE_INLINE const SimdVector3&   getOrigin()   const { return m_origin; }

	SimdQuaternion getRotation() const { 
		SimdQuaternion q;
		m_basis.getRotation(q);
		return q;
	}
	template <typename Scalar2>
		void setValue(const Scalar2 *m) 
	{
		m_basis.setValue(m);
		m_origin.setValue(&m[12]);
		m_type = AFFINE;
	}

	
	void setFromOpenGLMatrix(const SimdScalar *m)
	{
		m_basis.setFromOpenGLSubMatrix(m);
		m_origin[0] = m[12];
		m_origin[1] = m[13];
		m_origin[2] = m[14];
	}

	void getOpenGLMatrix(SimdScalar *m) const 
	{
		m_basis.getOpenGLSubMatrix(m);
		m[12] = m_origin[0];
		m[13] = m_origin[1];
		m[14] = m_origin[2];
		m[15] = SimdScalar(1.0f);
	}

	SIMD_FORCE_INLINE void setOrigin(const SimdVector3& origin) 
	{ 
		m_origin = origin;
		m_type |= TRANSLATION;
	}

	SIMD_FORCE_INLINE SimdVector3 invXform(const SimdVector3& inVec) const;



	SIMD_FORCE_INLINE void setBasis(const SimdMatrix3x3& basis)
	{ 
		m_basis = basis;
		m_type |= LINEAR;
	}

	SIMD_FORCE_INLINE void setRotation(const SimdQuaternion& q)
	{
		m_basis.setRotation(q);
		m_type = (m_type & ~LINEAR) | ROTATION;
	}

	SIMD_FORCE_INLINE void scale(const SimdVector3& scaling)
	{
		m_basis = m_basis.scaled(scaling);
		m_type |= SCALING;
	}

	void setIdentity()
	{
		m_basis.setIdentity();
		m_origin.setValue(SimdScalar(0.0), SimdScalar(0.0), SimdScalar(0.0));
		m_type = 0x0;
	}

	SIMD_FORCE_INLINE bool isIdentity() const { return m_type == 0x0; }

	SimdTransform& operator*=(const SimdTransform& t) 
	{
		m_origin += m_basis * t.m_origin;
		m_basis *= t.m_basis;
		m_type |= t.m_type; 
		return *this;
	}

	SimdTransform inverse() const
	{ 
		if (m_type)
		{
			SimdMatrix3x3 inv = (m_type & SCALING) ? 
				m_basis.inverse() : 
			m_basis.transpose();

			return SimdTransform(inv, inv * -m_origin, m_type);
		}

		return *this;
	}

	SimdTransform inverseTimes(const SimdTransform& t) const;  

	SimdTransform operator*(const SimdTransform& t) const;

private:

	SimdMatrix3x3 m_basis;
	SimdVector3   m_origin;
	unsigned int      m_type;
};


SIMD_FORCE_INLINE SimdVector3
SimdTransform::invXform(const SimdVector3& inVec) const
{
	SimdVector3 v = inVec - m_origin;
	return (m_basis.transpose() * v);
}

SIMD_FORCE_INLINE SimdTransform 
SimdTransform::inverseTimes(const SimdTransform& t) const  
{
	SimdVector3 v = t.getOrigin() - m_origin;
	if (m_type & SCALING) 
	{
		SimdMatrix3x3 inv = m_basis.inverse();
		return SimdTransform(inv * t.getBasis(), inv * v, 
			m_type | t.m_type);
	}
	else 
	{
		return SimdTransform(m_basis.transposeTimes(t.m_basis),
			v * m_basis, m_type | t.m_type);
	}
}

SIMD_FORCE_INLINE SimdTransform 
SimdTransform::operator*(const SimdTransform& t) const
{
	return SimdTransform(m_basis * t.m_basis, 
		(*this)(t.m_origin), 
		m_type | t.m_type);
}	



#endif





