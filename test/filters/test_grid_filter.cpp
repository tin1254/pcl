/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Point CLoud Library (PCL) - www.pointclouds.org
 * Copyright (c) 2020-, Open Perception
 *
 * All rights reserved
 */

#include <pcl/filters/experimental/grid_filter_base.h>
#include <pcl/io/pcd_io.h>
#include <pcl/test/gtest.h>
#include <pcl/point_types.h>

#include <boost/optional.hpp> // std::optional for C++17

#include <unordered_map>

using namespace pcl;

// Grid structure with necessary declarations for input to GridFilterBase

struct EmptyVoxel {
  bool voxel_info = false;
};

// hash map grid
template <typename PointT>
struct EmptyMapStruct {
  using PointCloud = pcl::PointCloud<PointT>;
  using Grid = std::unordered_map<std::size_t, EmptyVoxel>;

  EmptyMapStruct() : filter_name_("empty_map") {}

protected:
  // accessing GridFilterBase
  const experimental::GridFilterBase<EmptyMapStruct>*
  getDerived()
  {
    return static_cast<const experimental::GridFilterBase<EmptyMapStruct>*>(this);
  }

  bool
  setUp()
  {
    // test passed or failed setUp
    return getDerived()->getDownsampleAllData();
  }

  void
  addPointToGrid(const PointT& pt)
  {
    grid_[pt.x].voxel_info = true;
  }

  boost::optional<PointT>
  filterGrid(Grid::iterator grid_it)
  {
    // test optional
    if (getDerived()->getFilterLimitsNegative()) {
      // hashing index of the iterating grid
      return PointT(grid_it->first, 0, 0);
    }
    return boost::none;
  }

protected:
  std::string filter_name_;
  Grid grid_;
};

// vector grid
template <typename PointT>
struct EmptyVecStruct {
  using PointCloud = pcl::PointCloud<PointT>;
  using Grid = std::vector<std::size_t>;

  EmptyVecStruct() : filter_name_("empty_vec") {}

protected:
  bool
  setUp()
  {
    return true;
  }

  void
  addPointToGrid(const PointT& pt)
  {
    grid_.push_back(pt.x);
  }

  boost::optional<PointT>
  filterGrid(Grid::iterator grid_it)
  {
    // element, index of the iterating element
    return PointT(*grid_it, grid_it - grid_.begin(), 0);
  }

protected:
  std::string filter_name_;
  Grid grid_;
};

TEST(ApplyFilter, GridFilter)
{
  PointCloud<PointXYZ>::Ptr input = std::make_shared<PointCloud<PointXYZ>>();
  input->push_back(PointXYZ(10, 10, 10));
  input->push_back(PointXYZ(20, 20, 20));

  PointCloud<PointXYZ> output;

  // hash map grid
  experimental::GridFilterBase<EmptyMapStruct<PointXYZ>> mf;
  mf.setDownsampleAllData(true);    // setUp
  mf.setFilterLimitsNegative(true); // filterGrid
  mf.setInputCloud(input);
  mf.filter(output);

  std::unordered_map<size_t, bool> coordinates{{10, false}, {20, false}};

  for (std::size_t i = 0; i < input->size(); ++i) {
    EXPECT_FALSE(coordinates.at(static_cast<size_t>(output.at(i).x)));
    coordinates.at(static_cast<size_t>(output.at(i).x)) = true;
  }
  output.clear();

  // vector grid
  experimental::GridFilterBase<EmptyVecStruct<PointXYZ>> vf;
  vf.setInputCloud(input);
  vf.filter(output);

  for (std::size_t i = 0; i < input->size(); ++i) {
    EXPECT_EQ(output.at(i).x, input->at(i).x);
    EXPECT_EQ(output.at(i).y, i);
  }
}

TEST(StructMethods, GridFilter)
{
  // disable warning
  console::setVerbosityLevel(console::L_ALWAYS);

  PointCloud<PointXYZ>::Ptr input = std::make_shared<PointCloud<PointXYZ>>();
  input->push_back(PointXYZ(10, 10, 10));
  input->push_back(PointXYZ(20, 20, 20));

  PointCloud<PointXYZ> output;

  experimental::GridFilterBase<EmptyMapStruct<PointXYZ>> f;
  output.is_dense = false;
  f.filter(output);

  // return if input is empty
  EXPECT_FALSE(output.is_dense);
  output.clear();

  f.setInputCloud(input);
  f.setDownsampleAllData(false);    // setUp
  f.setFilterLimitsNegative(false); // filterGrid
  f.filter(output);

  // failed setUp
  EXPECT_TRUE(output.is_dense);
  EXPECT_EQ(output.height, 1);
  EXPECT_EQ(output.size(), input->size());
  output.clear();

  // filterGrid return optional
  f.setDownsampleAllData(true);     // setUp
  f.setFilterLimitsNegative(false); // filterGrid
  f.filter(output);
  EXPECT_EQ(output.size(), 0);
  output.clear();

  // filterGrid return points
  f.setDownsampleAllData(true);    // setUp
  f.setFilterLimitsNegative(true); // filterGrid
  f.filter(output);
  EXPECT_EQ(output.size(), input->size());
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return (RUN_ALL_TESTS());
}