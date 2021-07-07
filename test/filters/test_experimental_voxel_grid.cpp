/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Point CLoud Library (PCL) - www.pointclouds.org
 * Copyright (c) 2020-, Open Perception
 *
 * All rights reserved
 */

#include <pcl/filters/experimental/voxel_grid.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/test/gtest.h>
#include <pcl/point_types.h>

#include <cmath>
#include <limits>

using namespace pcl;
using namespace pcl::io;

PointCloud<PointXYZ>::Ptr cloud(new PointCloud<PointXYZ>);
PointCloud<PointXYZRGB>::Ptr cloud_organized(new PointCloud<PointXYZRGB>);

const float PRECISION = Eigen::NumTraits<float>::dummy_precision() * 2;

template <typename PointT>
void
EXPECT_POINT_EQ(const PointT& pt1, const PointT& pt2)
{
  const auto& pt1_vec = pt1.getVector4fMap();
  const auto& pt_vec = pt2.getVector4fMap();
  EXPECT_TRUE(pt1_vec.isApprox(pt_vec, PRECISION))
      << "Point1: " << pt1_vec.transpose() << "\nPoint2: " << pt_vec.transpose()
      << "\nnorm diff: " << (pt1_vec - pt_vec).norm() << std::endl;
}

template <typename PointT>
void
EXPECT_POINTS_EQ(PointCloud<PointT> pc1, PointCloud<PointT> pc2)
{
  ASSERT_EQ(pc1.size(), pc2.size());

  auto pt_cmp = [](const PointT& p1, const PointT& p2) -> bool {
    return p1.x > p2.x || (p1.x == p2.x && p1.y > p2.y) ||
           (p1.x == p2.x && p1.y == p2.y && p1.z > p2.z);
  };
  std::sort(pc1.begin(), pc1.end(), pt_cmp);
  std::sort(pc2.begin(), pc2.end(), pt_cmp);

  for (size_t i = 0; i < pc1.size(); ++i)
    EXPECT_POINT_EQ<PointT>(pc1.at(i), pc2.at(i));
}

TEST(SetUp, ExperimentalVoxelGridEquivalency)
{
  // PointXYZ
  {
    PointCloud<PointXYZ> new_out_cloud, old_out_cloud;

    experimental::VoxelGrid<PointXYZ> new_grid;
    pcl::VoxelGrid<PointXYZ> old_grid;
    new_grid.setLeafSize(0.02f, 0.02f, 0.02f);
    old_grid.setLeafSize(0.02f, 0.02f, 0.02f);
    new_grid.setInputCloud(cloud);
    old_grid.setInputCloud(cloud);
    new_grid.filter(new_out_cloud);
    old_grid.filter(old_out_cloud);

    const Eigen::Vector3i new_min_b = new_grid.getMinBoxCoordinates();
    const Eigen::Vector3i old_min_b = old_grid.getMinBoxCoordinates();
    EXPECT_TRUE(new_min_b.isApprox(old_min_b, PRECISION));

    const Eigen::Vector3i new_max_b = new_grid.getMaxBoxCoordinates();
    const Eigen::Vector3i old_max_b = old_grid.getMaxBoxCoordinates();
    EXPECT_TRUE(new_max_b.isApprox(old_max_b, PRECISION));

    const Eigen::Vector3i new_div_b = new_grid.getNrDivisions();
    const Eigen::Vector3i old_div_b = old_grid.getNrDivisions();
    EXPECT_TRUE(new_div_b.isApprox(old_div_b, PRECISION));

    const Eigen::Vector3i new_divb_mul = new_grid.getDivisionMultiplier();
    const Eigen::Vector3i old_divb_mul = old_grid.getDivisionMultiplier();
    EXPECT_TRUE(new_divb_mul.isApprox(old_divb_mul, PRECISION));
  }

  // PointXYZRGB
  {
    PointCloud<PointXYZRGB> new_out_cloud, old_out_cloud;

    // the original hashing range will overflow with leaf size of 0.02
    experimental::VoxelGrid<PointXYZRGB> new_grid;
    pcl::VoxelGrid<PointXYZRGB> old_grid;
    new_grid.setLeafSize(0.05f, 0.05f, 0.05f);
    old_grid.setLeafSize(0.05f, 0.05f, 0.05f);
    new_grid.setInputCloud(cloud_organized);
    old_grid.setInputCloud(cloud_organized);
    new_grid.filter(new_out_cloud);
    old_grid.filter(old_out_cloud);

    const Eigen::Vector3i new_min_b = new_grid.getMinBoxCoordinates();
    const Eigen::Vector3i old_min_b = old_grid.getMinBoxCoordinates();
    EXPECT_TRUE(new_min_b.isApprox(old_min_b, PRECISION));

    const Eigen::Vector3i new_max_b = new_grid.getMaxBoxCoordinates();
    const Eigen::Vector3i old_max_b = old_grid.getMaxBoxCoordinates();
    EXPECT_TRUE(new_max_b.isApprox(old_max_b, PRECISION));

    const Eigen::Vector3i new_div_b = new_grid.getNrDivisions();
    const Eigen::Vector3i old_div_b = old_grid.getNrDivisions();
    EXPECT_TRUE(new_div_b.isApprox(old_div_b, PRECISION));

    const Eigen::Vector3i new_divb_mul = new_grid.getDivisionMultiplier();
    const Eigen::Vector3i old_divb_mul = old_grid.getDivisionMultiplier();
    EXPECT_TRUE(new_divb_mul.isApprox(old_divb_mul, PRECISION));
  }
}

TEST(HashingPoint, ExperimentalVoxelGridEquivalency)
{
  // For extracting indices
  PointCloud<PointXYZ> new_out_cloud;
  experimental::VoxelGrid<PointXYZ> new_grid;
  new_grid.setLeafSize(0.02f, 0.02f, 0.02f);
  new_grid.setInputCloud(cloud);
  new_grid.filter(new_out_cloud);

  Eigen::Vector4f min_p, max_p;
  getMinMax3D<PointXYZ>(*cloud, *(new_grid.getIndices()), min_p, max_p);

  Eigen::Vector4i min_b, max_b, div_b, divb_mul;
  Eigen::Array4f inverse_leaf_size = 1 / Eigen::Array4f::Constant(0.02);
  min_b = (min_p.array() * inverse_leaf_size).floor().cast<int>();
  max_b = (max_p.array() * inverse_leaf_size).floor().cast<int>();
  div_b = (max_b - min_b).array() + 1;
  divb_mul = Eigen::Vector4i(1, div_b[0], div_b[0] * div_b[1], 0);

  // Copied from the old VoxelGrid as there is no dedicated method auto old_hash =
  auto old_hash = [&](const PointXYZ& p) {
    int ijk0 = static_cast<int>(std::floor(p.x * inverse_leaf_size[0]) -
                                static_cast<float>(min_b[0]));
    int ijk1 = static_cast<int>(std::floor(p.y * inverse_leaf_size[1]) -
                                static_cast<float>(min_b[1]));
    int ijk2 = static_cast<int>(std::floor(p.z * inverse_leaf_size[2]) -
                                static_cast<float>(min_b[2]));

    // Compute the centroid leaf index
    return ijk0 * divb_mul[0] + ijk1 * divb_mul[1] + ijk2 * divb_mul[2];
  };

  for (size_t i = 0; i < cloud->size(); ++i) {
    if (isXYZFinite(cloud->at(i))) {
      EXPECT_EQ(new_grid.hashPoint(cloud->at(i), inverse_leaf_size, min_b, divb_mul),
                old_hash(cloud->at(i)));
    }
  }
}

TEST(LeafLayout, ExperimentalVoxelGridEquivalency)
{
  PointCloud<PointXYZ> new_out_cloud, old_out_cloud;

  experimental::VoxelGrid<PointXYZ> new_grid;
  pcl::VoxelGrid<PointXYZ> old_grid;
  new_grid.setLeafSize(0.02f, 0.02f, 0.02f);
  old_grid.setLeafSize(0.02f, 0.02f, 0.02f);
  new_grid.setInputCloud(cloud);
  old_grid.setInputCloud(cloud);
  new_grid.setSaveLeafLayout(true);
  old_grid.setSaveLeafLayout(true);
  new_grid.filter(new_out_cloud);
  old_grid.filter(old_out_cloud);

  const auto new_leaf = new_grid.getLeafLayout();
  const auto old_leaf = old_grid.getLeafLayout();
  ASSERT_EQ(new_leaf.size(), old_leaf.size());

  // Centroid indices are different from the old implememnt because of the order of
  // iterating grids is different.
  // But the index should still point to the same point

  // leaf layout content
  for (size_t i = 0; i < new_leaf.size(); ++i) {
    if (old_leaf.at(i) == -1) {
      EXPECT_EQ(new_leaf.at(i), -1);
    }
    else {
      ASSERT_TRUE(new_leaf.at(i) != -1);
      const auto& new_pt = new_out_cloud.at(new_leaf.at(i));
      const auto& old_pt = old_out_cloud.at(old_leaf.at(i));
      EXPECT_POINT_EQ(new_pt, old_pt);
    }
  }

  // getCentroidIndex
  for (const auto& pt : *cloud) {
    const auto& new_pt = new_out_cloud.at(new_grid.getCentroidIndex(pt));
    const auto& old_pt = old_out_cloud.at(old_grid.getCentroidIndex(pt));
    EXPECT_POINT_EQ(new_pt, old_pt);
  }

  for (const auto& pt : new_out_cloud) {
    const Eigen::MatrixXi random_pt = Eigen::MatrixXi::Random(3, 1);

    const auto new_idx1 = new_grid.getNeighborCentroidIndices(pt, random_pt)[0];
    const auto new_idx2 =
        new_grid.getNeighborCentroidIndices(pt.x, pt.y, pt.z, random_pt)[0];

    const auto old_idx = old_grid.getNeighborCentroidIndices(pt, random_pt)[0];

    EXPECT_EQ(new_idx1, old_idx);
    EXPECT_EQ(new_idx2, old_idx);
  }
}

TEST(PointXYZ, ExperimentalVoxelGridEquivalency)
{
  PointCloud<PointXYZ> new_out, old_out;

  pcl::experimental::VoxelGrid<PointXYZ> new_grid;
  pcl::VoxelGrid<PointXYZ> old_grid;
  new_grid.setLeafSize(0.02f, 0.02f, 0.02f);
  old_grid.setLeafSize(0.02f, 0.02f, 0.02f);
  new_grid.setInputCloud(cloud);
  old_grid.setInputCloud(cloud);

  new_grid.setDownsampleAllData(false);
  old_grid.setDownsampleAllData(false);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setDownsampleAllData(true);
  old_grid.setDownsampleAllData(true);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setMinimumPointsNumberPerVoxel(5);
  old_grid.setMinimumPointsNumberPerVoxel(5);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_grid.setMinimumPointsNumberPerVoxel(0);
  old_grid.setMinimumPointsNumberPerVoxel(0);
  new_out.clear();
  old_out.clear();

  new_grid.setFilterFieldName("z");
  old_grid.setFilterFieldName("z");
  new_grid.setFilterLimitsNegative(true);
  old_grid.setFilterLimitsNegative(true);
  new_grid.setFilterLimits(std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::min());
  old_grid.setFilterLimits(std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::min());
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setFilterLimitsNegative(false);
  old_grid.setFilterLimitsNegative(false);
  new_grid.setFilterLimits(std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::max());
  old_grid.setFilterLimits(std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::max());
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
}

TEST(PointXYZRGB, ExperimentalVoxelGridEquivalency)
{
  PointCloud<PointXYZRGB> new_out, old_out;

  pcl::experimental::VoxelGrid<PointXYZRGB> new_grid;
  pcl::VoxelGrid<PointXYZRGB> old_grid;
  new_grid.setLeafSize(0.05f, 0.05f, 0.05f);
  old_grid.setLeafSize(0.05f, 0.05f, 0.05f);
  new_grid.setInputCloud(cloud_organized);
  old_grid.setInputCloud(cloud_organized);

  new_grid.setDownsampleAllData(false);
  old_grid.setDownsampleAllData(false);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setDownsampleAllData(true);
  old_grid.setDownsampleAllData(true);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setMinimumPointsNumberPerVoxel(5);
  old_grid.setMinimumPointsNumberPerVoxel(5);
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_grid.setMinimumPointsNumberPerVoxel(0);
  old_grid.setMinimumPointsNumberPerVoxel(0);
  new_out.clear();
  old_out.clear();

  new_grid.setFilterFieldName("z");
  old_grid.setFilterFieldName("z");
  new_grid.setFilterLimitsNegative(true);
  old_grid.setFilterLimitsNegative(true);
  new_grid.setFilterLimits(std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::min());
  old_grid.setFilterLimits(std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::min());
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
  new_out.clear();
  old_out.clear();

  new_grid.setFilterLimitsNegative(false);
  old_grid.setFilterLimitsNegative(false);
  new_grid.setFilterLimits(std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::max());
  old_grid.setFilterLimits(std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::max());
  new_grid.filter(new_out);
  old_grid.filter(old_out);
  EXPECT_POINTS_EQ(new_out, old_out);
}

int
main(int argc, char** argv)
{
  // Load a standard PCD file from disk
  if (argc < 3) {
    std::cerr << "No test file given. Please download `bun0.pcd` and "
                 "`milk_cartoon_all_small_clorox.pcd` and pass their paths to the test."
              << std::endl;
    return (-1);
  }

  // Load a standard PCD file from disk
  pcl::io::loadPCDFile(argv[1], *cloud);
  loadPCDFile(argv[2], *cloud_organized);

  testing::InitGoogleTest(&argc, argv);
  return (RUN_ALL_TESTS());
}
