#include "src/rendering/ObjMeshOptimization.h"

#ifdef ENTORNVGI_HAS_MESHOPTIMIZER
#include <meshoptimizer.h>

#include <limits>
#include <utility>
#include <type_traits>
#endif

namespace objmeshopt {

bool Build(const std::vector<double>& positions,
           const std::vector<double>& normals,
           const std::vector<double>& colors,
           const std::vector<double>& texcoords,
           IndexedGeometry& output)
{
    output = {};
#ifdef ENTORNVGI_HAS_MESHOPTIMIZER
    const std::size_t count = positions.size() / 3;
    if (count == 0 || count % 3 != 0 ||
        positions.size() != count * 3 || normals.size() != count * 3 ||
        colors.size() != count * 4 || texcoords.size() != count * 2 ||
        count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;

    // All attributes participate in welding: coincident positions with
    // distinct UVs or normals must remain separate (UV seams/hard edges).
    struct Vertex {
        double position[3];
        double normal[3];
        double color[4];
        double texcoord[2];
    };
    static_assert(std::is_trivially_copyable_v<Vertex>);
    static_assert(sizeof(Vertex) == 12 * sizeof(double));

    std::vector<Vertex> source(count);
    for (std::size_t i = 0; i < count; ++i) {
        Vertex& v = source[i];
        for (int a = 0; a < 3; ++a) {
            v.position[a] = positions[3 * i + a];
            v.normal[a] = normals[3 * i + a];
        }
        for (int a = 0; a < 4; ++a)
            v.color[a] = colors[4 * i + a];
        for (int a = 0; a < 2; ++a)
            v.texcoord[a] = texcoords[2 * i + a];
    }

    std::vector<unsigned int> remap(count);
    const std::size_t uniqueCount = meshopt_generateVertexRemap(
        remap.data(), nullptr, count, source.data(), count, sizeof(Vertex));
    if (uniqueCount == 0) return false;

    std::vector<unsigned int> indices(count);
    meshopt_remapIndexBuffer(indices.data(), nullptr, count, remap.data());
    std::vector<Vertex> unique(uniqueCount);
    meshopt_remapVertexBuffer(unique.data(), source.data(), count, sizeof(Vertex), remap.data());

    meshopt_optimizeVertexCache(indices.data(), indices.data(), count, uniqueCount);

    std::vector<Vertex> fetched(uniqueCount);
    const std::size_t fetchedCount = meshopt_optimizeVertexFetch(
        fetched.data(), indices.data(), count, unique.data(), uniqueCount, sizeof(Vertex));
    if (fetchedCount == 0 || fetchedCount > uniqueCount) return false;

    IndexedGeometry result;
    result.inputVertexCount = count;
    result.outputVertexCount = fetchedCount;
    result.indices = std::move(indices);
    result.positions.reserve(fetchedCount * 3);
    result.normals.reserve(fetchedCount * 3);
    result.colors.reserve(fetchedCount * 4);
    result.texcoords.reserve(fetchedCount * 2);
    for (std::size_t i = 0; i < fetchedCount; ++i) {
        const Vertex& v = fetched[i];
        result.positions.insert(result.positions.end(), v.position, v.position + 3);
        result.normals.insert(result.normals.end(), v.normal, v.normal + 3);
        result.colors.insert(result.colors.end(), v.color, v.color + 4);
        result.texcoords.insert(result.texcoords.end(), v.texcoord, v.texcoord + 2);
    }
    output = std::move(result);
    return true;
#else
    (void)positions;
    (void)normals;
    (void)colors;
    (void)texcoords;
    return false;
#endif
}

} // namespace objmeshopt
