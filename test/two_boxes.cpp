#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/node.h>
#include <bvh/v2/default_builder.h>
#include <bvh/v2/thread_pool.h>
#include <bvh/v2/executor.h>
#include <bvh/v2/stack.h>
#include <bvh/v2/tri.h>

using Scalar  = float;
using Vec3    = bvh::v2::Vec<Scalar, 3>;
using BBox    = bvh::v2::BBox<Scalar, 3>;
using Tri     = bvh::v2::Tri<Scalar, 3>;
using Node    = bvh::v2::Node<Scalar, 3>;
using Bvh     = bvh::v2::Bvh<Node>;
using Ray     = bvh::v2::Ray<Scalar, 3>;

int main() {

    std::vector<BBox> bboxes = {
        BBox(Vec3(0,0,0), Vec3(1,1,1)),
        BBox(Vec3(100,100,100), Vec3(101,101,101))
    };

    std::vector<Vec3> centers = {
        Vec3(0.5, 0.5, 0.5),
        Vec3(100.5, 100.5, 100.5)
    };

    bvh::v2::ThreadPool thread_pool;

    typename bvh::v2::DefaultBuilder<Node>::Config config;
    config.quality = bvh::v2::DefaultBuilder<Node>::Quality::High;
    auto bvh = bvh::v2::DefaultBuilder<Node>::build(thread_pool, bboxes, centers, config);

    auto ray = Ray {
        Vec3(100.5, 100.5, 0), // Ray origin
        Vec3(0., 0., 1.),    // Ray direction
        0.,                  // Minimum intersection distance
        1000.                 // Maximum intersection distance
    };

    static constexpr size_t stack_size = 64;
    static constexpr bool use_robust_traversal = false;

    bvh::v2::SmallStack<Bvh::Index, stack_size> stack;
    bvh.intersect<false, use_robust_traversal>(ray, bvh.get_root().index, stack,
        [&] (size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                printf("hit prim %d\n", (int) bvh.prim_ids[i]);
            }
            return false;
        });

    printf("done\n");
    return 0;
}