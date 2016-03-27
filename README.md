# pvd2abc
Convert ParaView animations to Alembic

In order to polish a scientific visualization created in
[ParaView](http://www.paraview.org/) or incorporate the data into a work
of art, one needs to export the data in a format that can be read by 3D
graphics software like
[Maya](http://www.autodesk.com/products/maya/overview).  For polygonal
data, the format should capture their geometry, normals, face & vertex
attributes, and time dependence.  ParaView's "Save Geometry" feature
captures this information, but the resulting ".pvd", ".vtm", and ".vtp"
are not understood by most other applications.  The
[Alembic](http://www.alembic.io/) format is, however, and can represent
this same information, along with things like object visibility and
camera trajectories.  The goal of this project is to provide a means of
converting ParaView animation geometry to the Alembic format.

This project also serves as an example for using the VTK and Alembic
libraries, whose API documentation is often lacking.

## Requirements
* `cmake` (https://cmake.org/)
* Alembic and dependencies (https://github.com/alembic/alembic)
* VTK (http://www.vtk.org/)

## Caveats
* ParaView's "Save Geometry" feature handles object visibility in a
strange way.  Any object not visible at the start of the animation will
be omitted from the export.  And objects that lose visibility will
continue to reference the data from their last visible frame.  `pvd2abc`
therefore assumes that any object referencing the same data as in the
previous frame should be marked as invisible.  The geometry for that
frame is still saved, but will be constant throughout the period of
invisibility (since ParaView doesn't export the actual data for those
times).  `pvd2abc` also provides a mechanism to mark objects that should
be invisible on the first frame (but it relies on the ParaView source
ID, which changes with each export).
* "vtkMultiBlockDataSet" (".vtm") files are assumed to contain a single
DataSet pointing to a "PolyData" (".vtp") file.  All of the geometry in
the PolyData must be in the form of "Polys" (not "Strips"), and normals
and scalar attributes must currently be "PointData", not "CellData".
* A user-specified scalar dataset may be used for grayscale coloring.
It will be marked as "mayaColorSet".
* This code was written under a deadline, and the proper calls on the
Alembic side were deduced from lots of trial and error.  As a result,
the code quality isn't as good as I'd like it to be.  These origins are
also why the output is focused on Maya instead of Blender.
