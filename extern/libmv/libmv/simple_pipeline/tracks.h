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

#ifndef LIBMV_SIMPLE_PIPELINE_TRACKS_H_
#define LIBMV_SIMPLE_PIPELINE_TRACKS_H_

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

/*!
    A Marker is the 2D location of a tracked point in an image.

    \a x, \a y is the position of the marker in pixels from the top left corner
    of the image identified by \a image for the camera identified by \a camera.
    All markers for the same target form a track identified by a common \a track
    number. Markers for a particular track but with different \a camera numbers
    correspond to the same target from an alternate view.

    \note Markers are typically aggregated with the help of the \l Tracks class.

    \sa Tracks
*/
struct Marker {
  int camera;
  int image;
  int track;
  double x, y;
};

/*!
    The Tracks class stores \link Marker reconstruction markers \endlink.

    The Tracks container is intended as the store of correspondences between
    images, which must get created before any 3D reconstruction can take place.

    The container has several fast lookups for queries typically needed for
    structure from motion algorithms, such as \l MarkersForTracksInBothImages().

    \sa Marker
*/
class Tracks {
 public:
  Tracks() { }

  // Copy constructor for a tracks object.
  Tracks(const Tracks &other);

  /// Construct a new tracks object using the given markers to start.
  Tracks(const vector<Marker> &markers);

  /*!
      Inserts a marker into the set. If there is already a marker for the given
      \a camera, \a image and \a track, the existing marker is replaced. If
      there is no marker for the given \a camera, \a image and \a track, a new
      one is added.

      \a camera, \a image and \a track are the keys used to retrieve the markers
      with the other methods in this class.

      \note To get an identifier for a new track, use \l MaxTrack() + 1.
  */
  void Insert(int camera, int image, int track, double x, double y);

  /// Returns all the markers.
  vector<Marker> AllMarkers() const;

  /// Returns all the markers visible from \a camera.
  vector<Marker> MarkersInCamera(int camera) const;

  /// Returns all the markers visible in an \a image from \a camera.
  vector<Marker> MarkersInImage(int camera, int image) const;

  /// Returns all the markers belonging to a track.
  vector<Marker> MarkersForTrack(int track) const;

  /// Returns all the markers visible in \a image1 and \a image2 from \a camera.
  vector<Marker> MarkersInBothImages(int camera, int image1, int image2) const;

  /*!
      Returns the markers in \a image1 and \a image2 (both from \a camera) which
      have a common track.

      This is not the same as the union of the markers in \a image1 and \a
      image2; each marker is for a track that appears in both images.
  */
  vector<Marker> MarkersForTracksInBothImages(int camera, int image1,
                                              int image2) const;

  /// Returns the marker in \a image from \a camera belonging to \a track.
  Marker MarkerInImageForTrack(int camera, int image, int track) const;

  /// Removes all the markers belonging to \a camera.
  void RemoveMarkersForCamera(int camera);

  /// Removes all the markers belonging to \a track.
  void RemoveMarkersForTrack(int track);

  /// Removes the marker in \a image of \a camera belonging to \a track.
  void RemoveMarker(int camera, int image, int track);

  /// Returns the maximum camera identifier used.
  int MaxCamera() const;

  /// Returns the maximum image identifier used.
  int MaxImage() const;

  /// Returns the maximum track identifier used.
  int MaxTrack() const;

  /// Returns the number of markers.
  int NumMarkers() const;

 private:
  vector<Marker> markers_;
};

void CoordinatesForMarkersInImage(const vector<Marker> &markers,
                                  int camera, int image,
                                  Mat *coordinates);

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_MARKERS_H_
