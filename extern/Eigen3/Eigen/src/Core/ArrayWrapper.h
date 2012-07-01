// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_ARRAYWRAPPER_H
#define EIGEN_ARRAYWRAPPER_H

namespace Eigen { 

/** \class ArrayWrapper
  * \ingroup Core_Module
  *
  * \brief Expression of a mathematical vector or matrix as an array object
  *
  * This class is the return type of MatrixBase::array(), and most of the time
  * this is the only way it is use.
  *
  * \sa MatrixBase::array(), class MatrixWrapper
  */

namespace internal {
template<typename ExpressionType>
struct traits<ArrayWrapper<ExpressionType> >
  : public traits<typename remove_all<typename ExpressionType::Nested>::type >
{
  typedef ArrayXpr XprKind;
};
}

template<typename ExpressionType>
class ArrayWrapper : public ArrayBase<ArrayWrapper<ExpressionType> >
{
  public:
    typedef ArrayBase<ArrayWrapper> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(ArrayWrapper)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(ArrayWrapper)

    typedef typename internal::conditional<
                       internal::is_lvalue<ExpressionType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;

    typedef typename internal::nested<ExpressionType>::type NestedExpressionType;

    inline ArrayWrapper(ExpressionType& matrix) : m_expression(matrix) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline ScalarWithConstIfNotLvalue* data() { return m_expression.data(); }
    inline const Scalar* data() const { return m_expression.data(); }

    inline CoeffReturnType coeff(Index rowId, Index colId) const
    {
      return m_expression.coeff(rowId, colId);
    }

    inline Scalar& coeffRef(Index rowId, Index colId)
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline const Scalar& coeffRef(Index rowId, Index colId) const
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_expression.coeff(index);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index rowId, Index colId) const
    {
      return m_expression.template packet<LoadMode>(rowId, colId);
    }

    template<int LoadMode>
    inline void writePacket(Index rowId, Index colId, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(rowId, colId, val);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return m_expression.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(index, val);
    }

    template<typename Dest>
    inline void evalTo(Dest& dst) const { dst = m_expression; }

    const typename internal::remove_all<NestedExpressionType>::type& 
    nestedExpression() const 
    {
      return m_expression;
    }

  protected:
    NestedExpressionType m_expression;
};

/** \class MatrixWrapper
  * \ingroup Core_Module
  *
  * \brief Expression of an array as a mathematical vector or matrix
  *
  * This class is the return type of ArrayBase::matrix(), and most of the time
  * this is the only way it is use.
  *
  * \sa MatrixBase::matrix(), class ArrayWrapper
  */

namespace internal {
template<typename ExpressionType>
struct traits<MatrixWrapper<ExpressionType> >
 : public traits<typename remove_all<typename ExpressionType::Nested>::type >
{
  typedef MatrixXpr XprKind;
};
}

template<typename ExpressionType>
class MatrixWrapper : public MatrixBase<MatrixWrapper<ExpressionType> >
{
  public:
    typedef MatrixBase<MatrixWrapper<ExpressionType> > Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(MatrixWrapper)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(MatrixWrapper)

    typedef typename internal::conditional<
                       internal::is_lvalue<ExpressionType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;

    typedef typename internal::nested<ExpressionType>::type NestedExpressionType;

    inline MatrixWrapper(ExpressionType& a_matrix) : m_expression(a_matrix) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline ScalarWithConstIfNotLvalue* data() { return m_expression.data(); }
    inline const Scalar* data() const { return m_expression.data(); }

    inline CoeffReturnType coeff(Index rowId, Index colId) const
    {
      return m_expression.coeff(rowId, colId);
    }

    inline Scalar& coeffRef(Index rowId, Index colId)
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline const Scalar& coeffRef(Index rowId, Index colId) const
    {
      return m_expression.derived().coeffRef(rowId, colId);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_expression.coeff(index);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index rowId, Index colId) const
    {
      return m_expression.template packet<LoadMode>(rowId, colId);
    }

    template<int LoadMode>
    inline void writePacket(Index rowId, Index colId, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(rowId, colId, val);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return m_expression.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(index, val);
    }

    const typename internal::remove_all<NestedExpressionType>::type& 
    nestedExpression() const 
    {
      return m_expression;
    }

  protected:
    NestedExpressionType m_expression;
};

} // end namespace Eigen

#endif // EIGEN_ARRAYWRAPPER_H
