//
// Copyright 2017 Animal Logic
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.//
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "test_usdmaya.h"

#include "maya/MFileIO.h"

#include "AL/maya/utils/NodeHelper.h"
#include "AL/usdmaya/fileio/ImportParams.h"
#include "AL/usdmaya/fileio/translators/MeshTranslator.h"
#include "AL/usdmaya/fileio/translators/DagNodeTranslator.h"
#include "AL/usdmaya/utils/MeshUtils.h"

using namespace AL::usdmaya::fileio::translators;
//----------------------------------------------------------------------------------------------------------------------
/// \brief  Test some of the functionality of the mesh translator
//----------------------------------------------------------------------------------------------------------------------
TEST(translators_MeshTranslator, convert3DArrayTo4DArray)
{
  std::vector<float> input;
  std::vector<float> output;
  input.resize(39 * 3);
  output.resize(39 * 4);
  for(uint32_t i = 0; i < 39 * 3; ++i)
  {
    input[i] = float(i);
  }

  AL::usdmaya::utils::convert3DArrayTo4DArray(input.data(), output.data(), 39);
  for(uint32_t i = 0, j = 0; i < 39 * 3; i += 3, j += 4)
  {
    EXPECT_NEAR(input[i], output[j], 1e-5f);
    EXPECT_NEAR(input[i + 1], output[j + 1], 1e-5f);
    EXPECT_NEAR(input[i + 2], output[j + 2], 1e-5f);
    EXPECT_NEAR(1.0f, output[j + 3], 1e-5f);
  }
}

TEST(translators_MeshTranslator, convertFloatVec3ArrayToDoubleVec3Array)
{
  std::vector<float> input;
  std::vector<double> output;
  input.resize(39 * 3);
  output.resize(39 * 3);
  for(uint32_t i = 0; i < 39 * 3; ++i)
  {
    input[i] = float(i);
  }

  AL::usdmaya::utils::convertFloatVec3ArrayToDoubleVec3Array(input.data(), output.data(), 39);
  for(uint32_t i = 0; i < 39 * 3; i += 3)
  {
    EXPECT_NEAR(input[i], output[i], 1e-5f);
    EXPECT_NEAR(input[i + 1], output[i + 1], 1e-5f);
    EXPECT_NEAR(input[i + 2], output[i + 2], 1e-5f);
  }

}

TEST(translators_MeshTranslator, zipunzipUVs)
{
  std::vector<float> u, v, uv(78);
  float f = 0;
  for(uint32_t i = 0; i < 39; ++i, f += 2.0f)
  {
    u.push_back(f);
    v.push_back(f + 1.0f);
  }

  AL::usdmaya::utils::zipUVs(u.data(), v.data(), uv.data(), u.size());

  f = 0;
  for(uint32_t i = 0; i < 78; i += 2, f += 2.0f)
  {
    EXPECT_NEAR(f, uv[i], 1e-5f);
    EXPECT_NEAR(f + 1.0f, uv[i + 1], 1e-5f);
  }

  std::vector<float> u2(39), v2(39);
  AL::usdmaya::utils::unzipUVs(uv.data(), u2.data(), v2.data(), u.size());

  for(uint32_t i = 0; i < 39; i++)
  {
    EXPECT_NEAR(u2[i], u[i], 1e-5f);
    EXPECT_NEAR(v2[i], v[i], 1e-5f);
  }
}

TEST(translators_MeshTranslator, interleaveIndexedUvData)
{
  std::vector<float> output(78), u(39), v(39);
  std::vector<int32_t> indices(39);
  const uint32_t numIndices = 39;

  for(uint32_t i = 0; i < 39; ++i)
  {
    u[i] = i * 2.0f + 1.0f;
    v[i] = i * 2.0f;
    indices[i] = 38 - i;
  }

  AL::usdmaya::utils::interleaveIndexedUvData(output.data(), u.data(), v.data(), indices.data(), numIndices);

  for(uint32_t i = 0; i < 78; ++i)
  {
    float fi = (77 - i);
    EXPECT_NEAR(fi, output[i], 1e-4f);
  }
}

TEST(translators_MeshTranslator, isUvSetDataSparse)
{
  std::vector<int32_t> uvCounts;
  uvCounts.resize(35, 1);


  EXPECT_TRUE(!AL::usdmaya::utils::isUvSetDataSparse(uvCounts.data(), uvCounts.size()));

  uvCounts[4] = 0;
  EXPECT_TRUE(AL::usdmaya::utils::isUvSetDataSparse(uvCounts.data(), uvCounts.size()));
  uvCounts[4] = 1;
  uvCounts[33] = 0;

  EXPECT_TRUE(AL::usdmaya::utils::isUvSetDataSparse(uvCounts.data(), uvCounts.size()));
}

TEST(translators_MeshTranslator, generateIncrementingIndices)
{
  MIntArray indices;
  AL::usdmaya::utils::generateIncrementingIndices(indices, 39);

  for(int32_t i = 0; i < 39; ++i)
  {
    EXPECT_EQ(indices[i], i);
  }
}

UsdGeomPrimvar getDefaultUvSet(UsdGeomMesh mesh)
{
  const std::vector<UsdGeomPrimvar> primvars = mesh.GetPrimvars();
  for(auto pvar : primvars)
  {
    if(pvar.GetPrimvarName() == TfToken("st"))
    {
      return pvar;
    }
  }
  return UsdGeomPrimvar();
}


TEST(translators_MeshTranslator, constantUvExport)
{
  // Ignore this test on Maya 2016. On other versions of maya, this test scales the 
  // UV's to a single point via the polyEditUV command. In the UV texture editor in 2016, 
  // echo all commands lists this exact command, however in 2016 this command
  // does nothing when executed in MEL ???? (unlike 2017/2018 ???)
  // I've disabled this test on 2016 as a result (if it passes on 2017/2018, the code itself 
  // works)
  #if MAYA_API_VERSION >= 201700
  MFileIO::newFile(true);

  // create a cube, and shrink all of its UV's to a single point  
  MString command =
  "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;"
  "select -r pCube1.map[0:23];"
  "polyEditUV -pu 0.5 -pv 0.5 -su 0 -sv 0;";
  MGlobal::executeCommand(command);

  // select the cube, and export (this should compact the UV coordinates)
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_constantUV.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_constantUV.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->constant, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);
    ASSERT_EQ(1, received.size());
    EXPECT_NEAR(0.5f, received[0][0], 1e-5f);
    EXPECT_NEAR(0.5f, received[0][1], 1e-5f);
  }

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_constantUV.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_constantUV2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_constantUV2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->constant, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);
    ASSERT_EQ(1, received.size());
    EXPECT_NEAR(0.5f, received[0][0], 1e-5f);
    EXPECT_NEAR(0.5f, received[0][1], 1e-5f);
  }
  #endif
}


TEST(translators_MeshTranslator, vertexUvExport)
{
  MFileIO::newFile(true);

  // create a cube, and apply a planar projection to it (UV's should now be per vertex)
  MString command =
  "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;"
  "select -r pCube1.f[0:5];"
  "polyProjection -ch 1 -type Planar -ibd on -md x  pCube1.f[0:5];"
  "select -r pCube1";
  MGlobal::executeCommand(command);

  // select the cube, and export (this should compact the UV coordinates)
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_vertexUV.usda\";");

  const GfVec2f expected[] = {{0.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, {0.f, 1.f}, {1.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {1.f, 0.f}};
  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_vertexUV.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->vertex, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_vertexUV.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_vertexUV2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_vertexUV2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->vertex, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }
  }

}


TEST(translators_MeshTranslator, faceVaryingUvExport)
{
  MFileIO::newFile(true);

  // create a cube, and apply a planar projection to it (UV's should now be per vertex)
  MString command =
  "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;"
  "select -r pCube1;";
  MGlobal::executeCommand(command);

  // select the cube, and export (this should compact the UV coordinates)
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_faceVaryingUV.usda\";");

  const GfVec2f expected[] = {
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1},
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1},
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1},
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1},
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1},
     {0, 0},
     {1, 0},
     {0, 1},
     {1, 1}};
  const int expectedIndices[] = {0, 1, 3, 2, 4, 5, 7, 6, 8, 9, 11, 10, 12, 13, 15, 14, 16, 17, 19, 18, 20, 21, 23, 22};

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_faceVaryingUV.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->faceVarying, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }

    VtIntArray receivedIndices;
    pvar.GetIndices(&receivedIndices);
    ASSERT_EQ(sizeof(expectedIndices) / sizeof(int), receivedIndices.size());
    for(uint32_t i = 0; i < receivedIndices.size(); ++i)
    {
      EXPECT_EQ(receivedIndices[i], expectedIndices[i]);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_faceVaryingUV.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_faceVaryingUV2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_faceVaryingUV.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->faceVarying, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }

    VtIntArray receivedIndices;
    pvar.GetIndices(&receivedIndices);
    ASSERT_EQ(sizeof(expectedIndices) / sizeof(int), receivedIndices.size());
    for(uint32_t i = 0; i < receivedIndices.size(); ++i)
    {
      EXPECT_EQ(receivedIndices[i], expectedIndices[i]);
    }
  }
}


TEST(translators_MeshTranslator, uniformUvExport)
{
  MFileIO::newFile(true);

  // create a cube, then for each face: apply a planar projection, and squish to the origin.
  // This should result in a single UV assignment to each face
  MString command =
  "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;";
  MGlobal::executeCommand(command);

  MSelectionList sl;
  sl.add("pCubeShape1");
  MObject obj;
  sl.getDependNode(0, obj);
  MFnMesh fn(obj);
  MFloatArray u, v;
  u.setLength(24);
  v.setLength(24);
  for(int i = 0, k = 0; i < 6; ++i)
  {
    for(int j = 0; j < 4; ++j, ++k)
    {
      u[k] = 0.1f * i + 0.1f;
      v[k] = 0.1f * i + 0.1f;
    }
  }
  fn.setUVs(u, v);

  // select the cube, and export (this should compact the UV coordinates)
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_uniformUV.usda\";");

  const GfVec2f expected[] = {{0.1f, 0.1f}, {0.2f, 0.2f}, {0.3f, 0.3f}, {0.4f, 0.4f}, {0.5f, 0.5f}, {0.6f, 0.6f}};

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_uniformUV.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->uniform, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_uniformUV.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_uniformUV2.usda\";");

  // and check that everything still matches
  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_uniformUV2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultUvSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->uniform, pvar.GetInterpolation());

    VtArray<GfVec2f> received;
    pvar.Get(&received);

    ASSERT_EQ(sizeof(expected) / sizeof(GfVec2f), received.size());
    for(size_t i = 0; i < received.size(); ++i)
    {
      EXPECT_NEAR(expected[i][0], received[i][0], 1e-5f);
      EXPECT_NEAR(expected[i][1], received[i][1], 1e-5f);
    }
  }
}


UsdGeomPrimvar getDefaultColourSet(UsdGeomMesh mesh)
{
  const std::vector<UsdGeomPrimvar> primvars = mesh.GetPrimvars();
  for(auto pvar : primvars)
  {
    if(pvar.GetPrimvarName() == TfToken("test"))
    {
      return pvar;
    }
  }
  return UsdGeomPrimvar();
}

TEST(translators_MeshTranslator, constantColourExport)
{
  MFileIO::newFile(true);

  // create a cube, and assign a colour set of the same value
  MString command =
      "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;";
  MGlobal::executeCommand(command);

  MSelectionList sl;
  sl.add("pCubeShape1");
  MObject obj;
  sl.getDependNode(0, obj);

  MFnMesh fn(obj);
  MString name = fn.createColorSetWithName("test");

  MColorArray colours;
  colours.setLength(24);
  for(int i = 0; i < 24; ++i)
  {
    colours[i] = MColor(0.3f, 0.4f, 0.5f, 1.0f);
  }
  fn.setColors(colours, &name);



  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportConstColour.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportConstColour.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->constant, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(1, received.size());
    EXPECT_NEAR(0.3f, received[0][0], 1e-5f);
    EXPECT_NEAR(0.4f, received[0][1], 1e-5f);
    EXPECT_NEAR(0.5f, received[0][2], 1e-5f);
    EXPECT_NEAR(1.0f, received[0][3], 1e-5f);
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_exportConstColour.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportConstColour2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportConstColour2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->constant, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(1, received.size());
    EXPECT_NEAR(0.3f, received[0][0], 1e-5f);
    EXPECT_NEAR(0.4f, received[0][1], 1e-5f);
    EXPECT_NEAR(0.5f, received[0][2], 1e-5f);
    EXPECT_NEAR(1.0f, received[0][3], 1e-5f);
  }
};


TEST(translators_MeshTranslator, vertexColourExport)
{
  MFileIO::newFile(true);

  // create a cube, and assign a colour set of the same value
  MString command =
      "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;";
  MGlobal::executeCommand(command);

  MSelectionList sl;
  sl.add("pCubeShape1");
  MObject obj;
  sl.getDependNode(0, obj);

  MFnMesh fn(obj);
  MString name = fn.createColorSetWithName("test");

  {
    MIntArray counts, indices;
    fn.getVertices(counts, indices);
    MColorArray colours;
    colours.setLength(24);
    for(int i = 0; i < 24; ++i)
    {
      colours[i] = MColor(0.3f * float(indices[i]), 0.4f, 0.5f, 1.0f);
    }
    fn.setColors(colours, &name);
  }

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportVertexColour.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportVertexColour.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->vertex, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(8, received.size());

    for(int i = 0; i < 8; ++i)
    {
      EXPECT_NEAR(0.3f * float(i), received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_exportVertexColour.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportVertexColour2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportVertexColour2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->vertex, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(8, received.size());
    for(int i = 0; i < 8; ++i)
    {
      EXPECT_NEAR(0.3f * i, received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }
};


TEST(translators_MeshTranslator, uniformColourExport)
{
  MFileIO::newFile(true);

  // create a cube, and assign a colour set of the same value
  MString command =
      "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;";
  MGlobal::executeCommand(command);

  MSelectionList sl;
  sl.add("pCubeShape1");
  MObject obj;
  sl.getDependNode(0, obj);

  MFnMesh fn(obj);
  MString name = fn.createColorSetWithName("test");

  {
    MIntArray counts, indices;
    fn.getVertices(counts, indices);

    MColorArray colours;
    colours.setLength(24);
    for(int i = 0; i < 6; i++)
    {
      for(int j = 0; j < 4; ++j)
      {
        colours[4 * i + j] = MColor(0.3f * float(i), 0.4f, 0.5f, 1.0f);
      }
    }
    fn.setColors(colours, &name);
  }

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportUniformColour.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportUniformColour.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->uniform, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(6, received.size());

    for(int i = 0; i < 6; ++i)
    {
      EXPECT_NEAR(0.3f * float(i), received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_exportUniformColour.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportUniformColour2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportUniformColour2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->uniform, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(6, received.size());
    for(int i = 0; i < 6; ++i)
    {
      EXPECT_NEAR(0.3f * float(i), received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }
};


TEST(translators_MeshTranslator, faceVaryingColourExport)
{
  MFileIO::newFile(true);

  // create a cube, and assign a colour set of the same value
  MString command =
      "polyCube -w 1 -h 1 -d 1 -sx 1 -sy 1 -sz 1 -ax 0 1 0 -cuv 2 -ch 1;";
  MGlobal::executeCommand(command);

  MSelectionList sl;
  sl.add("pCubeShape1");
  MObject obj;
  sl.getDependNode(0, obj);

  MFnMesh fn(obj);
  MString name = fn.createColorSetWithName("test");

  {
    MIntArray counts, indices;
    fn.getVertices(counts, indices);

    MColorArray colours;
    colours.setLength(24);
    for(int i = 0; i < 24; i++)
    {
      colours[i] = MColor(0.01f * i, 0.4f, 0.5f, 1.0f);
    }
    fn.setColors(colours, &name);
  }

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportFaceVaryingColour.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportFaceVaryingColour.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->faceVarying, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(24, received.size());

    for(int i = 0; i < 24; ++i)
    {
      EXPECT_NEAR(0.01f * i, received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }

  MFileIO::newFile(true);

  // re-import the file
  command =
  "file -import -type \"AL usdmaya import\" -ignoreVersion -ra true -mergeNamespacesOnClash false -namespace \"cube1\" -options \"Parent_Path=;Import_Meshes=1;Import_Curves=1;"
  "Import_Animations=1;Import_Dynamic_Attributes=1;\" -pr \"/tmp/AL_USDMayaTests_exportFaceVaryingColour.usda\"";
  MGlobal::executeCommand(command);

  // export once more
  MGlobal::executeCommand(
    "select -r pCube1;"
    "file -force -options \"Dynamic_Attributes=0;Meshes=1;Mesh_Normals=1;Nurbs_Curves=1;Duplicate_Instances=1;Compaction_Level=3;"
    "Merge_Transforms=1;Animation=0;Use_Timeline_Range=0;Frame_Min=1;Frame_Max=50;Filter_Sample=0;\" -typ \"AL usdmaya export\" -pr -es \"/tmp/AL_USDMayaTests_exportFaceVaryingColour2.usda\";");

  {
    UsdStageRefPtr stage = UsdStage::Open("/tmp/AL_USDMayaTests_exportFaceVaryingColour2.usda");
    ASSERT_TRUE(stage);

    UsdPrim prim = stage->GetPrimAtPath(SdfPath("/pCube1"));
    ASSERT_TRUE(prim);

    UsdGeomMesh mesh(prim);
    auto pvar = getDefaultColourSet(mesh);
    ASSERT_TRUE(pvar);
    EXPECT_EQ(UsdGeomTokens->faceVarying, pvar.GetInterpolation());

    VtArray<GfVec4f> received;
    pvar.Get(&received);
    ASSERT_EQ(24, received.size());
    for(int i = 0; i < 24; ++i)
    {
      EXPECT_NEAR(0.01f * i, received[i][0], 1e-5f);
      EXPECT_NEAR(0.4f, received[i][1], 1e-5f);
      EXPECT_NEAR(0.5f, received[i][2], 1e-5f);
      EXPECT_NEAR(1.0f, received[i][3], 1e-5f);
    }
  }
};














