// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_DETERMINANT_H
#define EIGEN_DETERMINANT_H

namespace internal {

template<typename Derived>
inline const typename Derived::Scalar bruteforce_det3_helper
(const MatrixBase<Derived>& matrix, int a, int b, int c)
{
  return matrix.coeff(0,a)
         * (matrix.coeff(1,b) * matrix.coeff(2,c) - matrix.coeff(1,c) * matrix.coeff(2,b));
}

template<typename Derived>
const typename Derived::Scalar bruteforce_det4_helper
(const MatrixBase<Derived>& matrix, int j, int k, int m, int n)
{
  return (matrix.coeff(j,0) * matrix.coeff(k,1) - matrix.coeff(k,0) * matrix.coeff(j,1))
       * (matrix.coeff(m,2) * matrix.coeff(n,3) - matrix.coeff(n,2) * matrix.coeff(m,3));
}

template<typename Derived,
         int DeterminantType = Derived::RowsAtCompileTime
> struct determinant_impl
{
  static inline typename traits<Derived>::Scalar run(const Derived& m)
  {
    if(Derived::ColsAtCompileTime==Dynamic && m.rows()==0)
      return typename traits<Derived>::Scalar(1);
    return m.partialPivLu().determinant();
  }
};

template<typename Derived> struct determinant_impl<Derived, 1>
{
  static inline typename traits<Derived>::Scalar run(const Derived& m)
  {
    return m.coeff(0,0);
  }
};

template<typename Derived> struct determinant_impl<Derived, 2>
{
  static inline typename traits<Derived>::Scalar run(const Derived& m)
  {
    return m.coeff(0,0) * m.coeff(1,1) - m.coeff(1,0) * m.coeff(0,1);
  }
};

template<typename Derived> struct determinant_impl<Derived, 3>
{
  static inline typename traits<Derived>::Scalar run(const Derived& m)
  {
    return bruteforce_det3_helper(m,0,1,2)
          - bruteforce_det3_helper(m,1,0,2)
          + bruteforce_det3_helper(m,2,0,1);
  }
};

template<typename Derived> struct determinant_impl<Derived, 4>
{
  static typename traits<Derived>::Scalar run(const Derived& m)
  {
    // trick by Martin Costabel to compute 4x4 det with only 30 muls
    return bruteforce_det4_helper(m,0,1,2,3)
          - bruteforce_det4_helper(m,0,2,1,3)
          + bruteforce_det4_helper(m,0,3,1,2)
          + bruteforce_det4_helper(m,1,2,0,3)
          - bruteforce_det4_helper(m,1,3,0,2)
          + bruteforce_det4_helper(m,2,3,0,1);
  }
};

} // end namespace internal

/** \lu_module
  *
  * \returns the determinant of this matrix
  */
template<typename Derived>
inline typename internal::traits<Derived>::Scalar MatrixBase<Derived>::determinant() const
{
  assert(rows() == cols());
  typedef typename internal::nested<Derived,Base::RowsAtCompileTime>::type Nested;
  return internal::determinant_impl<typename internal::remove_all<Nested>::type>::run(derived());
}

#endif // EIGEN_DETERMINANT_H
