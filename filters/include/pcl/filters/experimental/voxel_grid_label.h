/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2020-, Open Perception
 *
 *  All rights reserved
 */

#pragma once

#include <pcl/filters/experimental/voxel_grid.h>
#include <pcl/filters/filter.h>
#include <pcl/point_types.h>

#include <map>

namespace pcl {
namespace experimental {

struct LabeledVoxel : public Voxel<PointXYZRGBL> {
  using Voxel<PointXYZRGBL>::centroid_;
  using Voxel<PointXYZRGBL>::downsample_all_data_;
  using Voxel<PointXYZRGBL>::num_pt_;

  LabeledVoxel(const bool downsample_all_data)
  : Voxel<PointXYZRGBL>(downsample_all_data)
  {}

  inline void
  add(const PointXYZRGBL& pt)
  {
    num_pt_++;
    if (downsample_all_data_) {
      centroid_.all_fields.add(pt);
    }
    else {
      centroid_.xyz += pt.getArray4fMap();
      labels_[pt.label]++;
    }
  }

  inline PointXYZRGBL
  get() const
  {
    PointXYZRGBL pt;
    if (downsample_all_data_)
      centroid_.all_fields.get(pt);
    else {
      pt.getArray4fMap() = centroid_.xyz / num_pt_;
      std::size_t max = 0;
      for (const auto& label : labels_) {
        if (label.second > max) {
          max = label.second;
          pt.label = label.first;
        }
      }
    }

    return pt;
  }

  inline void
  clear()
  {
    Voxel<PointXYZRGBL>::clear();
    if (!downsample_all_data_)
      labels_.clear();
  }

protected:
  std::map<std::uint32_t, std::size_t> labels_;
};

/**
 * \brief LabeledVoxelStruct defines the transformation operations and the voxel grid of
 * VoxelGridLabel filter
 * \ingroup filters
 */
class LabeledVoxelStruct : public VoxelStruct<Voxel<PointXYZRGBL>, PointXYZRGBL> {
public:
  using VoxelStruct<Voxel<PointXYZRGBL>, PointXYZRGBL>::filter_name_;

  /** \brief Constructor. */
  LabeledVoxelStruct() { filter_name_ = "VoxelGridLabel"; }

  /** \brief Empty destructor. */
  ~LabeledVoxelStruct() {}

  /** \brief Set up the voxel grid variables needed for filtering.
   *  \param[in] transform_filter pointer to the TransformFilter object
   */
  bool
  setUp(TransformFilter<Filter, LabeledVoxelStruct> const* transform_filter)
  {
    auto cartesian_filter =
        static_cast<CartesianFilter<Filter, LabeledVoxelStruct> const*>(
            transform_filter);
    return VoxelStruct<Voxel<PointXYZRGBL>, PointXYZRGBL>::setUp(
        cartesian_filter->getInputCloud(),
        cartesian_filter->getDownsampleAllData(),
        cartesian_filter->getMinimumPointsNumberPerVoxel());
  }
};

/** \brief VoxelGridLabel assembles a local 3D grid over a PointXYZRGBL PointCloud, and
 * downsamples + filters the data. Each cell is labeled by majority vote according
 * to the label of the points.
 * \ingroup filters
 */
using VoxelGridLabel = VoxelFilter<LabeledVoxelStruct>;

} // namespace experimental
} // namespace pcl