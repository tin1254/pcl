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
EXPECT_POINTS_EQ(PointCloud<PointT> pc1, PointCloud<PointT> pc2)
{
  EXPECT_EQ(pc1.size(), pc2.size());

  // avoid printing too many error messages
  if (pc1.size() != pc2.size())
    return;

  auto pt_cmp = [](const PointT& p1, const PointT& p2) -> bool {
    return p1.x > p2.x || (p1.x == p2.x && p1.y > p2.y) ||
           (p1.x == p2.x && p1.y == p2.y && p1.z > p2.z);
  };
  std::sort(pc1.begin(), pc1.end(), pt_cmp);
  std::sort(pc2.begin(), pc2.end(), pt_cmp);

  for (size_t i = 0; i < pc1.size(); ++i) {
    const auto& p1 = pc1.at(i).getVector4fMap();
    const auto& p2 = pc2.at(i).getVector4fMap();
    EXPECT_TRUE(p1.isApprox(p2, PRECISION))
        << "Point1: " << p1.transpose() << "\nPoint2: " << p2.transpose()
        << "\nnorm diff: " << (p1 - p2).norm() << std::endl;
  }
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

TEST(ProtectedMethods, ExperimentalVoxelGridEquivalency)
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

    // Test hashing point
    const Eigen::Vector3i old_min_b = old_grid.getMinBoxCoordinates();
    const Eigen::Vector3i old_divb_mul = old_grid.getDivisionMultiplier();
    const Eigen::Vector3f old_inverse_leaf_size = 1 / old_grid.getLeafSize().array();

    // Copied from the old VoxelGrid as there is no dedicated method
    auto old_hash = [&](const PointXYZ& p) {
      int ijk0 = static_cast<int>(std::floor(p.x * old_inverse_leaf_size[0]) -
                                  static_cast<float>(old_min_b[0]));
      int ijk1 = static_cast<int>(std::floor(p.y * old_inverse_leaf_size[1]) -
                                  static_cast<float>(old_min_b[1]));
      int ijk2 = static_cast<int>(std::floor(p.z * old_inverse_leaf_size[2]) -
                                  static_cast<float>(old_min_b[2]));

      // Compute the centroid leaf index
      return ijk0 * old_divb_mul[0] + ijk1 * old_divb_mul[1] + ijk2 * old_divb_mul[2];
    };

    for (size_t i = 0; i < cloud->size(); ++i) {
      if (isXYZFinite(cloud->at(i))) {
        EXPECT_EQ(new_grid.hashPoint(cloud->at(i)), old_hash(cloud->at(i)));
      }
    }
  }

  // PointXYZRGB
  {
    PointCloud<PointXYZRGB> new_out_cloud, old_out_cloud;

    experimental::VoxelGrid<PointXYZRGB> new_grid;
    pcl::VoxelGrid<PointXYZRGB> old_grid;
    new_grid.setLeafSize(0.05f, 0.05f, 0.05f);
    old_grid.setLeafSize(0.05f, 0.05f, 0.05f);
    new_grid.setInputCloud(cloud_organized);
    old_grid.setInputCloud(cloud_organized);
    new_grid.filter(new_out_cloud);
    old_grid.filter(old_out_cloud);

    // Test hashing point
    const Eigen::Vector3i old_min_b = old_grid.getMinBoxCoordinates();
    const Eigen::Vector3i old_divb_mul = old_grid.getDivisionMultiplier();
    const Eigen::Vector3f old_inverse_leaf_size = 1 / old_grid.getLeafSize().array();

    // Copied from the old VoxelGrid as there is no dedicated method
    auto old_hash = [&](const PointXYZRGB& p) {
      int ijk0 = static_cast<int>(std::floor(p.x * old_inverse_leaf_size[0]) -
                                  static_cast<float>(old_min_b[0]));
      int ijk1 = static_cast<int>(std::floor(p.y * old_inverse_leaf_size[1]) -
                                  static_cast<float>(old_min_b[1]));
      int ijk2 = static_cast<int>(std::floor(p.z * old_inverse_leaf_size[2]) -
                                  static_cast<float>(old_min_b[2]));

      // Compute the centroid leaf index
      return ijk0 * old_divb_mul[0] + ijk1 * old_divb_mul[1] + ijk2 * old_divb_mul[2];
    };

    for (size_t i = 0; i < cloud_organized->size(); ++i) {
      if (isXYZFinite(cloud_organized->at(i))) {
        EXPECT_EQ(new_grid.hashPoint(cloud_organized->at(i)),
                  old_hash(cloud_organized->at(i)));
      }
    }
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

  // TODO: leaf_layout

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

  // TODO: leaf_layout

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
