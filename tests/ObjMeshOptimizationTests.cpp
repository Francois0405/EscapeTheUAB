#include "src/rendering/ObjMeshOptimization.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    // A two-triangle quad: 6 input corners, only 4 distinct vertices.
    const std::vector<double> positions = {
        0,0,0, 1,0,0, 1,1,0,
        0,0,0, 1,1,0, 0,1,0
    };
    const std::vector<double> normals(18, 0.0);
    const std::vector<double> colors(24, 1.0);
    const std::vector<double> uv = {
        0,0, 1,0, 1,1,
        0,0, 1,1, 0,1
    };

    objmeshopt::IndexedGeometry output;
    const bool enabled = objmeshopt::Build(positions, normals, colors, uv, output);
#ifdef ENTORNVGI_HAS_MESHOPTIMIZER
    assert(enabled);
    assert(output.inputVertexCount == 6);
    assert(output.outputVertexCount == 4);
    assert(output.indices.size() == 6);
    for (unsigned int index : output.indices)
        assert(index < output.outputVertexCount);

    // Check original triangle coverage (triangle order may be optimized).
    double area = 0;
    for (std::size_t t = 0; t < output.indices.size(); t += 3) {
        unsigned int a = output.indices[t], b = output.indices[t + 1], c = output.indices[t + 2];
        const double ax = output.positions[3*a], ay = output.positions[3*a+1];
        const double bx = output.positions[3*b], by = output.positions[3*b+1];
        const double cx = output.positions[3*c], cy = output.positions[3*c+1];
        area += ((bx-ax)*(cy-ay) - (by-ay)*(cx-ax)) * 0.5;
    }
    assert(std::abs(area - 1.0) < 1e-12);

    auto seam = uv;
    seam[6] = 0.5; // Same position on another triangle, different UV.
    objmeshopt::IndexedGeometry withSeam;
    assert(objmeshopt::Build(positions, normals, colors, seam, withSeam));
    assert(withSeam.outputVertexCount == 5);

    auto badColors = colors;
    badColors.pop_back();
    assert(!objmeshopt::Build(positions, normals, badColors, uv, output));
    std::cout << "ObjMeshOptimizationTests PASS\n";
#else
    assert(!enabled);
    std::cout << "ObjMeshOptimizationTests SKIP (meshoptimizer unavailable)\n";
#endif
}
