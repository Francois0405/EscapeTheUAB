#pragma once

#include <cstddef>
#include <vector>

// CPU-only helper. Receives the legacy unindexed, per-material OBJ streams
// and produces a triangle index buffer without changing normals, UVs or colors.
namespace objmeshopt {

struct IndexedGeometry {
    std::vector<double> positions;
    std::vector<double> normals;
    std::vector<double> colors;
    std::vector<double> texcoords;
    std::vector<unsigned int> indices;
    std::size_t inputVertexCount = 0;
    std::size_t outputVertexCount = 0;
};

// Returns false if meshoptimizer is unavailable or the four attribute arrays
// are invalid. Callers must use their existing, unindexed upload as fallback.
bool Build(const std::vector<double>& positions,
           const std::vector<double>& normals,
           const std::vector<double>& colors,
           const std::vector<double>& texcoords,
           IndexedGeometry& output);

} // namespace objmeshopt
