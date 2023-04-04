/*
 * @author Curran D. Muhlberger
 * @date 2016-02-28
 */

#include <vtkCellArray.h>
#include <vtkCompositeDataSet.h>
#include <vtkCompositeDataIterator.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>
#include <vtkXMLDataElement.h>
#include <vtkXMLDataParser.h>
#include <vtkXMLMultiBlockDataReader.h>
#include <vtkXMLPolyDataReader.h>

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdlib>

using namespace Alembic::AbcGeom;

class VtpAbc;
void process_pvd(std::string const & filename, OArchive & archive);
void process_vtm(std::string const & filename, bool visible, VtpAbc & vtpabc);
void process_polydata(vtkPolyData * polydata, bool visible, VtpAbc & vtpabc);

class VtpAbc {
 public:
  VtpAbc(std::string const & name, OArchive & archive, TimeSamplingPtr const & ts) :
    _xform(archive.getTop(), name, ts),
    _polymesh(_xform, name + "Shape", ts),
    _visible(_xform.getProperties(), "visible", ts) {
  }

  VtpAbc(VtpAbc const &) = delete;
  VtpAbc & operator=(VtpAbc const &) = delete;

  OPolyMeshSchema & mesh() { return _polymesh.getSchema(); }

  /** Add mesh geometry sample (with normals) to Alembic object. */
  void addMeshSample(std::vector<float> const & points,
                     std::vector<std::int32_t> const & indices,
                     std::vector<std::int32_t> const & counts,
                     std::vector<float> const & normals,
                     bool const visible) {
    OPolyMeshSchema::Sample mesh_samp(
        V3fArraySample(reinterpret_cast<V3f const *>(points.data()), points.size()/3),
        Int32ArraySample(indices),
        Int32ArraySample(counts),
        OV2fGeomParam::Sample(),
        ON3fGeomParam::Sample(N3fArraySample(reinterpret_cast<N3f const *>(normals.data()), normals.size()/3),
                              kFacevaryingScope));
    mesh().set(mesh_samp);
    _visible.set(visible ? -1 : 0);
  }

  /** Add vertex color sample to Alembic object. */
  void addColorSample(std::vector<std::int32_t> const & indices,
                      std::vector<float> const & colors) {
    if (!_colorset) _init_colorset();

    // Make an unsigned copy of indices because the Alembic API requires signed or
    // unsigned arrays in different functions...
    auto uindices = std::vector<std::uint32_t>{};
    uindices.reserve(indices.size());
    std::copy(indices.begin(), indices.end(), std::back_inserter(uindices));
    
    OC3fGeomParam::Sample csamp(C3fArraySample(reinterpret_cast<C3f const *>(colors.data()), colors.size()/3),
                                UInt32ArraySample(uindices),
                                kFacevaryingScope);
    _colorset->set(csamp);
  }

 private:
  OXform _xform;
  OPolyMesh _polymesh;
  OCharProperty _visible;
  std::unique_ptr<OC3fGeomParam> _colorset;

  void _init_colorset() {
    auto arbParams = mesh().getArbGeomParams();
    Alembic::AbcCoreAbstract::MetaData md;
    md.set("mayaColorSet", "1");
    _colorset.reset(new OC3fGeomParam(arbParams, "colorSet1", true, kFacevaryingScope, 1, mesh().getTimeSampling(), md));
  }
};

void process_pvd(std::string const & filename, OArchive & archive) {
  TimeSamplingPtr ts(new TimeSampling(1.0/24.0, 0.0));
  auto const tsIndex = archive.addTimeSampling(*ts);

  vtkSmartPointer<vtkXMLDataParser> parser =
      vtkSmartPointer<vtkXMLDataParser>::New();
  parser->SetFileName(filename.c_str());
  parser->Parse();

  vtkXMLDataElement * root = parser->GetRootElement();
  assert(root);
  vtkXMLDataElement * collection = root->FindNestedElementWithName("Collection");
  assert(collection);

  std::map<std::string, std::unique_ptr<VtpAbc>> vtpabcs;
  std::map<std::string, std::string> last_file;
  // Hack to ignore EH and AHC on first frame
  last_file["source9218"] = "Kip3B/Kip3B_source9218T0000.vtm";
  last_file["source10267"] = "Kip3B/Kip3B_source10267T0000.vtm";
  std::set<std::string> timestamps;  // TODO: double instead of string

  for (int i=0; i<collection->GetNumberOfNestedElements(); ++i) {
    vtkXMLDataElement * dataset = collection->GetNestedElement(i);
    std::string const timestep = dataset->GetAttribute("timestep");
    std::string const group = dataset->GetAttribute("group");
    std::string const file = dataset->GetAttribute("file");
    std::cout << timestep << ": " << group << std::endl;
    timestamps.insert(timestep);
    auto & vtpabc = vtpabcs[group];
    if (!vtpabc) vtpabc.reset(new VtpAbc(group, archive, ts));
    auto const visible = file != last_file[group];
    process_vtm(file, visible, *vtpabc);
    last_file[group] = file;
  }

  OUInt32Property sampsamp(archive.getTop().getProperties(), std::to_string(tsIndex) + ".samples");
  sampsamp.set(timestamps.size());
}

void process_vtm(std::string const & filename, bool const visible, VtpAbc & vtpabc) {
  vtkSmartPointer<vtkXMLMultiBlockDataReader> reader =
      vtkSmartPointer<vtkXMLMultiBlockDataReader>::New();
  reader->SetFileName(filename.c_str());
  reader->Update();

  vtkCompositeDataSet * vtmout = reader->GetOutput();
  vtkCompositeDataIterator * iter = vtmout->NewIterator();
  // We assume each VTM file points to a single PolyData dataset
  vtkPolyData * polydata = vtkPolyData::SafeDownCast(vtmout->GetDataSet(iter));
  assert(polydata);
  iter->Delete();

  process_polydata(polydata, visible, vtpabc);
}

void process_polydata(vtkPolyData * polydata, bool const visible, VtpAbc & vtpabc) {
  /* Extract mesh geometry from VTK data */
  // Extract points
  auto points = std::vector<float>{};
  points.reserve(3*polydata->GetNumberOfPoints());
  double p[3];
  for (vtkIdType i=0; i<polydata->GetNumberOfPoints(); ++i) {
    polydata->GetPoint(i, p);
    points.push_back(p[0]);
    points.push_back(p[1]);
    points.push_back(p[2]);
  }

  // Write face counts and indices
  vtkCellArray * cells = polydata->GetPolys();
  std::cout << cells->GetNumberOfCells() << std::endl;
  auto indices = std::vector<std::int32_t>{};
  indices.reserve(3*cells->GetNumberOfCells());
  auto counts = std::vector<std::int32_t>{};
  counts.reserve(cells->GetNumberOfCells());
  vtkIdType npts;
  vtkIdType * pts;
  cells->InitTraversal();
  while (cells->GetNextCell(npts, pts)) {
    assert(npts == 3);  // We only expect triangles atm
    counts.push_back(npts);
    for (vtkIdType i=0; i<npts; ++i) {
      indices.push_back(pts[i]);
    }
  }

  vtkPointData * pointData = polydata->GetPointData();
  vtkFloatArray * normals = vtkFloatArray::SafeDownCast(pointData->GetNormals());
  assert(normals);
  assert(normals->GetNumberOfComponents() == 3);
  auto norms = std::vector<float>{};
  norms.reserve(indices.size());
  float v[3];
  for (auto i : indices) {
    normals->GetTupleValue(i, v);
    norms.push_back(v[0]);
    norms.push_back(v[1]);
    norms.push_back(v[2]);
  }

  vtpabc.addMeshSample(points, indices, counts, norms, visible);

  /* Extract scalar data from VTK data to use for coloring */
  //vtkDoubleArray * colors = vtkDoubleArray::SafeDownCast(pointData->GetScalars());
  vtkDoubleArray * colors = vtkDoubleArray::SafeDownCast(pointData->GetScalars("Bnn"));
  if (!colors) {
    colors = vtkDoubleArray::SafeDownCast(pointData->GetScalars("WeylB_NN_AhA.dump"));
  }
  if (!colors) {
    colors = vtkDoubleArray::SafeDownCast(pointData->GetScalars("WeylB_NN_AhB.dump"));
  }
  if (!colors) {
    colors = vtkDoubleArray::SafeDownCast(pointData->GetScalars("WeylB_NN_AhC.dump"));
  }
  if (colors) {
    assert(colors->GetNumberOfComponents() == 1);
    auto const colorRangeLo = -1.3;
    auto const colorRangeHi = 1.3;
    auto cols = std::vector<float>();
    cols.reserve(3*colors->GetDataSize());
    for (vtkIdType i=0; i<colors->GetDataSize(); ++i) {
      auto const c = std::min(std::max((colors->GetValue(i) - colorRangeLo)/(colorRangeHi - colorRangeLo), 0.0), 1.0);
      cols.push_back(c);
      cols.push_back(c);
      cols.push_back(c);
    }

    vtpabc.addColorSample(indices, cols);
  }
}
 
int main(int const argc, char const * argv[]) {
  assert(argc == 3);
  auto const pvd_filename = std::string(argv[1]);
  auto const abc_filename = std::string(argv[2]);

  { // Scope for `archive` to control when it flushes
    OArchive archive = CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive(),
        abc_filename, "cdmuhlb.VtkToAbc", "Exported from: " + pvd_filename);
    process_pvd(pvd_filename, archive);
  }

  return EXIT_SUCCESS;
}
