// Copyright (c) 2011 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "libmv/simple_pipeline/intersect.h"

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/triangulation.h"
#include "libmv/multiview/nviewtriangulation.h"
#include "libmv/numeric/numeric.h"
#include "libmv/numeric/levenberg_marquardt.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"

#include "ceres/ceres.h"

namespace libmv {

namespace {

class EuclideanIntersectCostFunctor {
 public:
  EuclideanIntersectCostFunctor(const Marker &marker,
                                const EuclideanView &view)
      : marker_(marker), view_(view) {}

  template<typename T>
  bool operator()(const T *X, T *residuals) const {
    typedef Eigen::Matrix<T, 3, 3> Mat3;
    typedef Eigen::Matrix<T, 3, 1> Vec3;

    Vec3 x(X);
    Mat3 R(view_.R.cast<T>());
    Vec3 t(view_.t.cast<T>());

    Vec3 projected = R * x + t;
    projected /= projected(2);

    residuals[0] = projected(0) - T(marker_.x);
    residuals[1] = projected(1) - T(marker_.y);

    return true;
  }

  const Marker &marker_;
  const EuclideanView &view_;
};

}  // namespace

bool EuclideanIntersect(const vector<Marker> &markers,
                        EuclideanReconstruction *reconstruction) {
  if (markers.size() < 2) {
    return false;
  }

  // Compute projective camera matrices for the cameras the intersection is
  // going to use.
  Mat3 K = Mat3::Identity();
  vector<Mat34> cameras;
  Mat34 P;
  for (int i = 0; i < markers.size(); ++i) {
    EuclideanView *view = reconstruction->ViewForImage(0, markers[i].image);
    P_From_KRt(K, view->R, view->t, &P);
    cameras.push_back(P);
  }

  // Stack the 2D coordinates together as required by NViewTriangulate.
  Mat2X points(2, markers.size());
  for (int i = 0; i < markers.size(); ++i) {
    points(0, i) = markers[i].x;
    points(1, i) = markers[i].y;
  }

  Vec4 Xp;
  LG << "Intersecting with " << markers.size() << " markers.";
  NViewTriangulateAlgebraic(points, cameras, &Xp);

  // Get euclidean version of the homogeneous point.
  Xp /= Xp(3);
  Vec3 X = Xp.head<3>();

  ceres::Problem problem;

  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    const EuclideanView &view =
        *reconstruction->ViewForImage(0, marker.image);

    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<
            EuclideanIntersectCostFunctor,
            2, /* num_residuals */
            3>(new EuclideanIntersectCostFunctor(marker, view)),
        NULL,
        &X(0));
  }

  // Configure the solve.
  ceres::Solver::Options solver_options;
  solver_options.linear_solver_type = ceres::DENSE_QR;
  solver_options.max_num_iterations = 50;
  solver_options.update_state_every_iteration = true;
  solver_options.parameter_tolerance = 1e-16;
  solver_options.function_tolerance = 1e-16;

  // Run the solve.
  ceres::Solver::Summary summary;
  ceres::Solve(solver_options, &problem, &summary);

  VLOG(1) << "Summary:\n" << summary.FullReport();

  // Try projecting the point; make sure it's in front of everyone.
  for (int i = 0; i < cameras.size(); ++i) {
    const EuclideanView &view =
        *reconstruction->ViewForImage(0, markers[i].image);
    Vec3 x = view.R * X + view.t;
    if (x(2) < 0) {
      LOG(ERROR) << "POINT BEHIND CAMERA " << markers[i].image
                 << ": " << x.transpose();
      return false;
    }
  }

  Vec3 point = X.head<3>();
  reconstruction->InsertPoint(markers[0].track, point);

  // TODO(keir): Add proper error checking.
  return true;
}

namespace {

struct ProjectiveIntersectCostFunction {
 public:
  typedef Vec  FMatrixType;
  typedef Vec4 XMatrixType;

  ProjectiveIntersectCostFunction(
      const vector<Marker> &markers,
      const ProjectiveReconstruction &reconstruction)
    : markers(markers), reconstruction(reconstruction) {}

  Vec operator()(const Vec4 &X) const {
    Vec residuals(2 * markers.size());
    residuals.setZero();
    for (int i = 0; i < markers.size(); ++i) {
      const ProjectiveView &view =
          *reconstruction.ViewForImage(0, markers[i].image);
      Vec3 projected = view.P * X;
      projected /= projected(2);
      residuals[2*i + 0] = projected(0) - markers[i].x;
      residuals[2*i + 1] = projected(1) - markers[i].y;
    }
    return residuals;
  }
  const vector<Marker> &markers;
  const ProjectiveReconstruction &reconstruction;
};

}  // namespace

bool ProjectiveIntersect(const vector<Marker> &markers,
                         ProjectiveReconstruction *reconstruction) {
  if (markers.size() < 2) {
    return false;
  }

  // Get the cameras to use for the intersection.
  vector<Mat34> cameras;
  for (int i = 0; i < markers.size(); ++i) {
    ProjectiveView *view = reconstruction->ViewForImage(0, markers[i].image);
    cameras.push_back(view->P);
  }

  // Stack the 2D coordinates together as required by NViewTriangulate.
  Mat2X points(2, markers.size());
  for (int i = 0; i < markers.size(); ++i) {
    points(0, i) = markers[i].x;
    points(1, i) = markers[i].y;
  }

  Vec4 X;
  LG << "Intersecting with " << markers.size() << " markers.";
  NViewTriangulateAlgebraic(points, cameras, &X);
  X /= X(3);

  typedef LevenbergMarquardt<ProjectiveIntersectCostFunction> Solver;

  ProjectiveIntersectCostFunction triangulate_cost(markers, *reconstruction);
  Solver::SolverParameters params;
  Solver solver(triangulate_cost);

  Solver::Results results = solver.minimize(params, &X);
  (void) results;  // TODO(keir): Ensure results are good.

  // Try projecting the point; make sure it's in front of everyone.
  for (int i = 0; i < cameras.size(); ++i) {
    const ProjectiveView &view =
        *reconstruction->ViewForImage(0, markers[i].image);
    Vec3 x = view.P * X;
    if (x(2) < 0) {
      LOG(ERROR) << "POINT BEHIND CAMERA " << markers[i].image
                 << ": " << x.transpose();
    }
  }

  reconstruction->InsertPoint(markers[0].track, X);

  // TODO(keir): Add proper error checking.
  return true;
}

}  // namespace libmv
