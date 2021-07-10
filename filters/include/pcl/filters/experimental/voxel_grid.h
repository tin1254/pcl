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

#include <unordered_map>

namespace pcl {
namespace experimental {

template <typename PointT>
class VoxelStructT {
public:
  // read by GridFilterBase to deduce point type
  using PointCloud = pcl::PointCloud<PointT>;
  using PointCloudPtr = typename PointCloud::Ptr;
  using PointCloudConstPtr = typename PointCloud::ConstPtr;
  using Grid = typename std::unordered_map<std::size_t, CentroidPoint<PointT>>;
  using GridIterator = typename Grid::iterator;

  /** \brief Empty constructor. */
  VoxelStructT() { filter_name_ = "VoxelGrid"; }

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
  getCentroidIndex(const PointT& pt) const
  {
    return leaf_layout_.at(
        grid_filter_->hashPoint(pt, inverse_leaf_size_, min_b_, divb_mul_));
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
  getNeighborCentroidIndices(const float x,
                             const float y,
                             const float z,
                             const Eigen::MatrixXi& relative_coordinates) const
  {
    const Eigen::Vector4i ijk(
        (Eigen::Vector4i() << getGridCoordinates(x, y, z), 0).finished());
    const Eigen::Array4i diff2min = min_b_ - ijk;
    const Eigen::Array4i diff2max = max_b_ - ijk;

    std::vector<int> neighbors(relative_coordinates.cols());
    for (Eigen::Index ni = 0; ni < relative_coordinates.cols(); ni++) {
      const Eigen::Vector4i displacement =
          (Eigen::Vector4i() << relative_coordinates.col(ni), 0).finished();

      // checking if the specified cell is in the grid
      if ((diff2min <= displacement.array()).all() &&
          (diff2max >= displacement.array()).all())
        // .at() can be omitted
        neighbors[ni] = leaf_layout_[((ijk + displacement - min_b_).dot(divb_mul_))];
      else
        // cell is out of bounds, consider it empty
        neighbors[ni] = -1;
    }
    return neighbors;
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
      const float x,
      const float y,
      const float z,
      const std::vector<Eigen::Vector3i, Eigen::aligned_allocator<Eigen::Vector3i>>&
          relative_coordinates) const
  {
    const Eigen::Vector4i ijk(
        (Eigen::Vector4i() << getGridCoordinates(x, y, z), 0).finished());
    std::vector<int> neighbors;
    neighbors.reserve(relative_coordinates.size());
    for (const auto& relative_coordinate : relative_coordinates)
      neighbors.push_back(
          leaf_layout_[(ijk + (Eigen::Vector4i() << relative_coordinate, 0).finished() -
                        min_b_)
                           .dot(divb_mul_)]);
    return neighbors;
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
    return getNeighborCentroidIndices(
        reference_point.x, reference_point.y, reference_point.z, relative_coordinates);
  }

  /** \brief Set to true if leaf layout information needs to be saved for later access.
   * \param[in] save_leaf_layout the new value (true/false)
   */
  inline void
  setSaveLeafLayout(const bool save_leaf_layout)
  {
    save_leaf_layout_ = save_leaf_layout;
  }

  /** \brief Returns true if leaf layout information will to be saved for later access.
   */
  inline bool
  getSaveLeafLayout() const
  {
    return save_leaf_layout_;
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
  getGridCoordinates(const float x, const float y, const float z) const
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
    const int idx = ((Eigen::Vector4i() << ijk, 0).finished() - min_b_).dot(divb_mul_);
    // this checks also if leaf_layout_.size () == 0 i.e. everything was computed as
    // needed
    if (idx < 0 || idx >= static_cast<int>(leaf_layout_.size()))
      return -1;

    return leaf_layout_[idx];
  }

protected:
  bool
  setUp(TransformFilter<VoxelStructT>* transform_filter)
  {
    grid_filter_ = static_cast<GridFilterBase<VoxelStructT>*>(transform_filter);

    const PointCloudConstPtr input = grid_filter_->getInputCloud();
    const IndicesConstPtr indices = grid_filter_->getIndices();
    downsample_all_data_ = grid_filter_->getDownsampleAllData();
    min_points_per_voxel_ = grid_filter_->getMinimumPointsNumberPerVoxel();

    // Get the minimum and maximum dimensions
    Eigen::Vector4f min_p, max_p;
    getMinMax3D<PointT>(*input, *indices, min_p, max_p);

    min_b_ = (min_p.array() * inverse_leaf_size_).floor().template cast<int>();
    max_b_ = (max_p.array() * inverse_leaf_size_).floor().template cast<int>();

    div_b_ = (max_b_ - min_b_).array() + 1;
    divb_mul_ = Eigen::Vector4i(1, div_b_[0], div_b_[0] * div_b_[1], 0);

    num_voxels_ = 0;
    grid_.clear();

    const std::size_t hash_range =
        grid_filter_->checkHashRange(min_p, max_p, inverse_leaf_size_);
    if (hash_range != 0) {
      grid_.reserve(std::min(hash_range, input->size()));
    }
    else {
      PCL_WARN("[pcl::%s::applyFilter] Leaf size is too small for the input dataset. "
               "Integer indices would overflow.\n",
               filter_name_.c_str());
      return false;
    }

    if (save_leaf_layout_) {
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

  inline void
  addPointToGrid(const PointT& pt)
  {
    const std::size_t h =
        grid_filter_->hashPoint(pt, inverse_leaf_size_, min_b_, divb_mul_);
    grid_[h].add(pt);
  }

  inline experimental::optional<PointT>
  filterGrid(const GridIterator grid_it)
  {
    const auto& voxel = grid_it->second;
    if (voxel.getSize() >= min_points_per_voxel_) {

      PointT centroid;
      if (!downsample_all_data_) {
        PointT pt;
        voxel.get(pt);
        centroid.getVector4fMap() = pt.getVector4fMap();
      }
      else {
        voxel.get(centroid);
      }

      if (save_leaf_layout_)
        leaf_layout_[grid_it->first] = num_voxels_++;

      return centroid;
    }

    return boost::none;
  }

protected:
  /** \brief The filter name. */
  std::string filter_name_;

  /** \brief The size of a leaf. */
  Eigen::Vector4f leaf_size_ = Eigen::Vector4f::Zero();

  /** \brief Internal leaf sizes stored as 1/leaf_size_ for efficiency reasons. */
  Eigen::Array4f inverse_leaf_size_ = Eigen::Array4f::Zero();

  /** \brief Set to true if leaf layout information needs to be saved in \a
   * leaf_layout_. */
  bool save_leaf_layout_ = false;

  /** \brief The leaf layout information for fast access to cells relative to current
   * position **/
  std::vector<int> leaf_layout_;

  /** \brief The minimum and maximum bin coordinates, the number of divisions, and the
   * division multiplier. */
  Eigen::Vector4i min_b_ = Eigen::Vector4i::Zero(), max_b_ = Eigen::Vector4i::Zero(),
                  div_b_ = Eigen::Vector4i::Zero(), divb_mul_ = Eigen::Vector4i::Zero();

  /** \brief The iterable grid object for storing information of each fraction of space
   * in the filtering space defined by the grid */
  Grid grid_;

  GridFilterBase<VoxelStructT>* grid_filter_;

  std::size_t num_voxels_;

  bool downsample_all_data_;

  std::size_t min_points_per_voxel_;
};

template <typename PointT>
using VoxelGrid = GridFilterBase<VoxelStructT<PointT>>;

} // namespace experimental
} // namespace pcl