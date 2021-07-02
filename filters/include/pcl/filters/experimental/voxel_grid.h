/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2020-, Open Perception
 *
 *  All rights reserved
 */

#pragma once

#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/filters/experimental/grid_filter_base.h>

#include <boost/optional.hpp> // std::optional for C++17

#include <unordered_map>

// For testing protected method
class VoxelGridProtectedMethods_GridFilters_Test;

namespace pcl {
namespace experimental {

template <typename PointT>
struct Voxel {
  // TODO: find if there is better way than storing two centroid types
  CentroidPoint<PointT> centroid;
  Eigen::Vector4f coord_centroid = Eigen::Vector4f::Zero();
  std::size_t num_pt = 0;
};

template <typename PointT>
class VoxelStructT {
public:
  // read by GridFilterBase to deduce point type
  using PointCloud = pcl::PointCloud<PointT>;
  using PointCloudPtr = typename PointCloud::Ptr;
  using PointCloudConstPtr = typename PointCloud::ConstPtr;
  using Grid = typename std::unordered_map<std::size_t, Voxel<PointT>>;
  using GridIterator =
      typename std::unordered_map<std::size_t, CentroidPoint<PointT>>::iterator;

  /** \brief Empty constructor. */
  VoxelStructT()
  : filter_name_("VoxelGrid")
  , leaf_size_(Eigen::Vector4f::Zero())
  , inverse_leaf_size_(Eigen::Array4f::Zero())
  , min_b_(Eigen::Vector4i::Zero())
  , max_b_(Eigen::Vector4i::Zero())
  , div_b_(Eigen::Vector4i::Zero())
  , divb_mul_(Eigen::Vector4i::Zero())
  , grid_()
  {}

  /** \brief Set the voxel grid leaf size.
   * \param[in] leaf_size the voxel grid leaf size
   */
  inline void
  setLeafSize(const Eigen::Vector4f& leaf_size)
  {
    leaf_size_ = leaf_size;
    // Avoid division errors
    if (leaf_size_[3] == 0)
      leaf_size_[3] = 1;
    // Use multiplications instead of divisions
    inverse_leaf_size_ = 1 / leaf_size_.array();
  }

  /** \brief Set the voxel grid leaf size.
   * \param[in] lx the leaf size for X
   * \param[in] ly the leaf size for Y
   * \param[in] lz the leaf size for Z
   */
  inline void
  setLeafSize(const float lx, const float ly, const float lz)
  {
    leaf_size_[0] = lx;
    leaf_size_[1] = ly;
    leaf_size_[2] = lz;
    // Avoid division errors
    if (leaf_size_[3] == 0)
      leaf_size_[3] = 1;
    // Use multiplications instead of divisions
    inverse_leaf_size_ = 1 / leaf_size_.array();
  }

  /** \brief Get the voxel grid leaf size. */
  inline Eigen::Vector3f
  getLeafSize() const
  {
    return leaf_size_.head<3>();
  }

  /** \brief Get the minimum coordinates of the bounding box (after filtering is
   * performed).
   */
  inline Eigen::Vector3i
  getMinBoxCoordinates() const
  {
    return min_b_.head<3>();
  }

  /** \brief Get the minimum coordinates of the bounding box (after filtering is
   * performed).
   */
  inline Eigen::Vector3i
  getMaxBoxCoordinates() const
  {
    return max_b_.head<3>();
  }

  /** \brief Get the number of divisions along all 3 axes (after filtering is
   * performed).
   */
  inline Eigen::Vector3i
  getNrDivisions() const
  {
    return div_b_.head<3>();
  }

  /** \brief Get the multipliers to be applied to the grid coordinates in order to find
   * the centroid index (after filtering is performed).
   */
  inline Eigen::Vector3i
  getDivisionMultiplier() const
  {
    return divb_mul_.head<3>();
  }

  /** \brief Returns the index in the resulting downsampled cloud of the specified
   * point.
   *
   * \note for efficiency, user must make sure that the saving of the leaf layout is
   * enabled and filtering performed, and that the point is inside the grid, to avoid
   * invalid access (or use getGridCoordinates+getCentroidIndexAt)
   *
   * \param[in] p the point to get the index at
   */
  inline int
  getCentroidIndex(const PointT& p) const
  {
    return leaf_layout_.at(hashPoint(p));
  }

  /** \brief Returns the indices in the resulting downsampled cloud of the points at the
   * specified grid coordinates, relative to the grid coordinates of the specified point
   * (or -1 if the cell was empty/out of bounds).
   * \param[in] x the X coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[in] y the Y coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[in] z the Z coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[out] relative_coordinates matrix with the columns being the coordinates of
   * the requested cells, relative to the reference point's cell
   * \note for efficiency,user must make sure that the saving of the leaf layout is
   * enabled and filtering performed
   */
  inline std::vector<int>
  getNeighborCentroidIndices(float x,
                             float y,
                             float z,
                             const Eigen::MatrixXi& relative_coordinates) const
  {
    Eigen::Vector4i ijk(static_cast<int>(std::floor(x * inverse_leaf_size_[0])),
                        static_cast<int>(std::floor(y * inverse_leaf_size_[1])),
                        static_cast<int>(std::floor(z * inverse_leaf_size_[2])),
                        0);
    Eigen::Array4i diff2min = min_b_ - ijk;
    Eigen::Array4i diff2max = max_b_ - ijk;
    std::vector<int> neighbors(relative_coordinates.cols());
    for (Eigen::Index ni = 0; ni < relative_coordinates.cols(); ni++) {
      Eigen::Vector4i displacement =
          (Eigen::Vector4i() << relative_coordinates.col(ni), 0).finished();
      // checking if the specified cell is in the grid
      if ((diff2min <= displacement.array()).all() &&
          (diff2max >= displacement.array()).all())
        neighbors[ni] = leaf_layout_[(
            (ijk + displacement - min_b_).dot(divb_mul_))]; // .at() can be omitted
      else
        neighbors[ni] = -1; // cell is out of bounds, consider it empty
    }
    return (neighbors);
  }

  /** \brief Returns the indices in the resulting downsampled cloud of the points at the
   * specified grid coordinates, relative to the grid coordinates of the specified point
   * (or -1 if the cell was empty/out of bounds).
   * \param[in] x the X coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[in] y the Y coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[in] z the Z coordinate of the reference point (corresponding cell is allowed
   * to be empty/out of bounds)
   * \param[out] relative_coordinates vector with the elements being the coordinates of
   * the requested cells, relative to the reference point's cell
   * \note for efficiency, user must make sure that the saving of the leaf layout is
   * enabled and filtering performed
   */
  inline std::vector<int>
  getNeighborCentroidIndices(
      float x,
      float y,
      float z,
      const std::vector<Eigen::Vector3i, Eigen::aligned_allocator<Eigen::Vector3i>>&
          relative_coordinates) const
  {
    Eigen::Vector4i ijk(static_cast<int>(std::floor(x * inverse_leaf_size_[0])),
                        static_cast<int>(std::floor(y * inverse_leaf_size_[1])),
                        static_cast<int>(std::floor(z * inverse_leaf_size_[2])),
                        0);
    std::vector<int> neighbors;
    neighbors.reserve(relative_coordinates.size());
    for (const auto& relative_coordinate : relative_coordinates)
      neighbors.push_back(
          leaf_layout_[(ijk + (Eigen::Vector4i() << relative_coordinate, 0).finished() -
                        min_b_)
                           .dot(divb_mul_)]);
    return (neighbors);
  }

  /** \brief Returns the indices in the resulting downsampled cloud of the points at the
   * specified grid coordinates, relative to the grid coordinates of the specified point
   * (or -1 if the cell was empty/out of bounds).
   * \param[in] reference_point the coordinates of the reference point (corresponding
   * cell is allowed to be empty/out of bounds)
   * \param[in] relative_coordinates matrix with the columns being the coordinates of
   * the requested cells, relative to the reference point's cell
   * \note for efficiency, user must make sure that the saving of the leaf layout is
   * enabled and filtering performed
   */
  inline std::vector<int>
  getNeighborCentroidIndices(const PointT& reference_point,
                             const Eigen::MatrixXi& relative_coordinates) const
  {
    Eigen::Vector4i ijk(
        static_cast<int>(std::floor(reference_point.x * inverse_leaf_size_[0])),
        static_cast<int>(std::floor(reference_point.y * inverse_leaf_size_[1])),
        static_cast<int>(std::floor(reference_point.z * inverse_leaf_size_[2])),
        0);
    Eigen::Array4i diff2min = min_b_ - ijk;
    Eigen::Array4i diff2max = max_b_ - ijk;
    std::vector<int> neighbors(relative_coordinates.cols());
    for (Eigen::Index ni = 0; ni < relative_coordinates.cols(); ni++) {
      Eigen::Vector4i displacement =
          (Eigen::Vector4i() << relative_coordinates.col(ni), 0).finished();
      // checking if the specified cell is in the grid
      if ((diff2min <= displacement.array()).all() &&
          (diff2max >= displacement.array()).all())
        neighbors[ni] = leaf_layout_[(
            (ijk + displacement - min_b_).dot(divb_mul_))]; // .at() can be omitted
      else
        neighbors[ni] = -1; // cell is out of bounds, consider it empty
    }
    return neighbors;
  }

  /** \brief Returns the layout of the leafs for fast access to cells relative to
   * current position.
   * \note position at (i-min_x) + (j-min_y)*div_x + (k-min_z)*div_x*div_y holds the
   * index of the element at coordinates (i,j,k) in the grid (-1 if empty)
   */
  inline std::vector<int>
  getLeafLayout() const
  {
    return leaf_layout_;
  }

  /** \brief Returns the corresponding (i,j,k) coordinates in the grid of point (x,y,z).
   * \param[in] x the X point coordinate to get the (i, j, k) index at
   * \param[in] y the Y point coordinate to get the (i, j, k) index at
   * \param[in] z the Z point coordinate to get the (i, j, k) index at
   */
  inline Eigen::Vector3i
  getGridCoordinates(float x, float y, float z) const
  {
    return Eigen::Vector3i(static_cast<int>(std::floor(x * inverse_leaf_size_[0])),
                           static_cast<int>(std::floor(y * inverse_leaf_size_[1])),
                           static_cast<int>(std::floor(z * inverse_leaf_size_[2])));
  }

  /** \brief Returns the index in the downsampled cloud corresponding to a given set of
   * coordinates.
   * \param[in] ijk the coordinates (i,j,k) in the grid (-1 if empty)
   */
  inline int
  getCentroidIndexAt(const Eigen::Vector3i& ijk) const
  {
    int idx = ((Eigen::Vector4i() << ijk, 0).finished() - min_b_).dot(divb_mul_);
    if (idx < 0 ||
        idx >= static_cast<int>(
                   leaf_layout_.size())) // this checks also if leaf_layout_.size () ==
                                         // 0 i.e. everything was computed as needed
    {
      // if (verbose)
      //  PCL_ERROR ("[pcl::%s::getCentroidIndexAt] Specified coordinate is outside grid
      //  bounds, or leaf layout is not saved, make sure to call setSaveLeafLayout(true)
      //  and filter(output) first!\n", getClassName ().c_str ());
      return (-1);
    }
    return leaf_layout_[idx];
  }

protected:
  // accessing GridFilterBase
  inline const GridFilterBase<VoxelStructT>*
  getDerived()
  {
    return static_cast<const GridFilterBase<VoxelStructT>*>(this);
  }

  bool
  setUp()
  {
    const auto grid_filter = getDerived();
    const PointCloudConstPtr input = grid_filter->getInputCloud();
    const IndicesConstPtr indices = grid_filter->getIndices();
    filter_field_name_ = grid_filter->getFilterFieldName();
    grid_filter->getFilterLimits(filter_limit_min_, filter_limit_max_);

    // Get the minimum and maximum dimensions
    Eigen::Vector4f min_p, max_p;
    if (!filter_field_name_.empty()) {
      // If we don't want to process the entire cloud...
      getMinMax3D<PointT>(input,
                          *indices,
                          filter_field_name_,
                          static_cast<float>(filter_limit_min_),
                          static_cast<float>(filter_limit_max_),
                          min_p,
                          max_p,
                          grid_filter->getFilterLimitsNegative());

      filter_field_idx_ = getFieldIndex<PointT>(filter_field_name_, filter_fields_);
      if (filter_field_idx_ == -1) {
        PCL_WARN("[pcl::%s::applyFilter] Invalid filter field name. Index is %d.\n",
                 filter_name_.c_str(),
                 filter_field_idx_);
      }
    }
    else {
      getMinMax3D<PointT>(*input, *indices, min_p, max_p);
    }

    min_b_ = (min_p.array() * inverse_leaf_size_).floor().template cast<int>();
    max_b_ = (max_p.array() * inverse_leaf_size_).floor().template cast<int>();

    div_b_ = (max_b_ - min_b_).array() + 1;
    divb_mul_ = Eigen::Vector4i(1, div_b_[0], div_b_[0] * div_b_[1], 0);

    num_voxels_ = 0;
    const boost::optional<std::size_t> max_num_voxels = checkIfOverflow(min_p, max_p);
    if (max_num_voxels) {
      grid_.reserve(std::min(max_num_voxels.value(), input->size()));
    }
    else {
      PCL_WARN("[pcl::%s::applyFilter] Leaf size is too small for the input dataset. "
               "Integer indices would overflow.\n",
               filter_name_.c_str());
      return false;
    }

    if (grid_filter->getSaveLeafLayout()) {
      const std::size_t new_layout_size = div_b_[0] * div_b_[1] * div_b_[2];
      const std::size_t reset_size = std::min(new_layout_size, leaf_layout_.size());

      // TODO: stop using std::vector<int> for leaf_layout_ because the range of int is
      // smaller than size_t
      std::fill(leaf_layout_.begin(), leaf_layout_.begin() + reset_size, -1);

      try {
        leaf_layout_.resize(new_layout_size, -1);
      } catch (std::bad_alloc&) {
        throw PCLException(
            "VoxelGrid bin size is too low; impossible to allocate memory for layout",
            "voxel_grid.hpp",
            "applyFilter");
      } catch (std::length_error&) {
        throw PCLException(
            "VoxelGrid bin size is too low; impossible to allocate memory for layout",
            "voxel_grid.hpp",
            "applyFilter");
      }
    }

    return true;
  }

  boost::optional<std::size_t>
  checkIfOverflow(const Eigen::Vector4f& min_p, const Eigen::Vector4f& max_p)
  {
    // Check that the leaf size is not too small, given the size of the data
    // Otherwise "wrap around" of unsigned int will happen during hashing a point
    const std::size_t dx =
        std::floor((max_p[0] - min_p[0]) * inverse_leaf_size_[0]) + 1;
    const std::size_t dy =
        std::floor((max_p[1] - min_p[1]) * inverse_leaf_size_[1]) + 1;
    const std::size_t dz =
        std::floor((max_p[2] - min_p[2]) * inverse_leaf_size_[2]) + 1;
    const size_t max_size = std::numeric_limits<std::size_t>::max();

    // check unsigned int overflow/wrap-around
    if (dy > max_size / dx || dx * dy > max_size / dz)
      return boost::none;
    else
      return dx * dy * dz;
  }

  inline std::size_t
  hashPoint(const PointT& pt)
  {
    const std::size_t ijk0 = std::floor(pt.x * inverse_leaf_size_[0]) - min_b_[0];
    const std::size_t ijk1 = std::floor(pt.y * inverse_leaf_size_[1]) - min_b_[1];
    const std::size_t ijk2 = std::floor(pt.z * inverse_leaf_size_[2]) - min_b_[2];
    return ijk0 * divb_mul_[0] + ijk1 * divb_mul_[1] + ijk2 * divb_mul_[2];
  }

  inline void
  addPointToGrid(const PointT& pt)
  {
    if (!filter_field_name_.empty()) {
      // Get the distance value
      const std::uint8_t* pt_data = reinterpret_cast<const std::uint8_t*>(&pt);
      float distance_value = 0;
      memcpy(&distance_value,
             pt_data + filter_fields_[filter_field_idx_].offset,
             sizeof(float));

      if (getDerived()->getFilterLimitsNegative()) {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < filter_limit_max_) &&
            (distance_value > filter_limit_min_))
          return;
      }
      else {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > filter_limit_max_) ||
            (distance_value < filter_limit_min_))
          return;
      }
    }

    const std::size_t h = hashPoint(pt);

    const bool downsample_all_data = getDerived()->getDownsampleAllData();
    if (!downsample_all_data)
      grid_[h].coord_centroid += pt.getVector4fMap();
    else
      grid_[h].centroid.add(pt);

    grid_[h].num_pt++;
  }

  inline boost::optional<PointT>
  filterGrid(GridIterator grid_it)
  {
    auto& voxel = grid_it->second;
    const std::size_t min_points_per_voxel =
        getDerived()->getMinimumPointsNumberPerVoxel();
    if (voxel.num_pt >= min_points_per_voxel) {

      const bool save_leaf_layout = getDerived()->getSaveLeafLayout();
      if (save_leaf_layout)
        leaf_layout_[grid_it->first] = num_voxels_++;

      PointT centroid;
      const bool downsample_all_data = getDerived()->getDownsampleAllData();
      if (!downsample_all_data) {
        voxel.coord_centroid.array() /= voxel.num_pt;
        centroid.getVector4fMap() = voxel.coord_centroid;
      }
      else {
        voxel.centroid.get(centroid);
      }
      return centroid;
    }

    return boost::none;
  }

protected:
  /** \brief The filter name. */
  std::string filter_name_;

  /** \brief The size of a leaf. */
  Eigen::Vector4f leaf_size_;

  /** \brief Internal leaf sizes stored as 1/leaf_size_ for efficiency reasons. */
  Eigen::Array4f inverse_leaf_size_;

  /** \brief The leaf layout information for fast access to cells relative to current
   * position **/
  std::vector<int> leaf_layout_;

  /** \brief The minimum and maximum bin coordinates, the number of divisions, and the
   * division multiplier. */
  Eigen::Vector4i min_b_, max_b_, div_b_, divb_mul_;

  /** \brief The iterable grid object for storing information of each fraction of space
   * in the filtering space defined by the grid */
  Grid grid_;

  std::size_t num_voxels_;

  std::vector<pcl::PCLPointField> filter_fields_;

  int filter_field_idx_;

  // ============== Below variables are duplicates from GridFilterBase ==============
  // TODO: maybe declaring friend in GridFilterBase is better?
  double filter_limit_min_, filter_limit_max_;

  std::string filter_field_name_;

private:
  friend class ::VoxelGridProtectedMethods_GridFilters_Test;
};

template <typename PointT>
using VoxelGrid = GridFilterBase<VoxelStructT<PointT>>;

} // namespace experimental
} // namespace pcl