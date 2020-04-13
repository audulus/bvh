#ifndef BVH_LINEAR_BVH_BUILDER_HPP
#define BVH_LINEAR_BVH_BUILDER_HPP

#include <cstdint>
#include <cassert>
#include <numeric>

#include "bvh/morton_code_based_builder.hpp"
#include "bvh/utilities.hpp"

namespace bvh {

template <typename Bvh, typename Morton>
class LinearBvhBuilder : public MortonCodeBasedBuilder<Bvh, Morton> {
    using Scalar = typename Bvh::ScalarType;

    using ParentBuilder = MortonCodeBasedBuilder<Bvh, Morton>;
    using ParentBuilder::sort_primitives_by_morton_code;

    using Level = typename SizedIntegerType<RoundUpLog2<sizeof(Morton) * CHAR_BIT + 1>::value>::Unsigned;
    using Node  = typename Bvh::Node;

    Bvh& bvh;

    std::pair<size_t, size_t> merge(
        const Node* bvh__restrict__ input_nodes,
        Node* bvh__restrict__ output_nodes,
        const Level* bvh__restrict__ input_levels,
        Level* bvh__restrict__ output_levels,
        size_t* bvh__restrict__ merged_index,
        size_t* bvh__restrict__ needs_merge,
        size_t begin, size_t end,
        size_t previous_end)
    {
        size_t children_begin = 0;
        size_t unmerged_begin = 0;

        merged_index[end - 1] = 0;
        needs_merge [end - 1] = 0;

        #pragma omp parallel if (end - begin > ParentBuilder::loop_parallel_threshold)
        {
            // Determine, for each node, if it should be merged with the one on the right.
            #pragma omp for
            for (size_t i = begin; i < end - 1; ++i)
                needs_merge[i] = input_levels[i] >= input_levels[i + 1] && (i == begin || input_levels[i] >= input_levels[i - 1]);

            // Resolve conflicts between nodes that want to be merged with different neighbors.
            #pragma omp for
            for (size_t i = begin; i < end - 1; i += 2) {
                if (needs_merge[i] && needs_merge[i + 1])
                    needs_merge[i] = 0;
            }
            #pragma omp for
            for (size_t i = begin + 1; i < end - 1; i += 2) {
                if (needs_merge[i] && needs_merge[i + 1])
                    needs_merge[i] = 0;
            }

            // Perform a prefix sum to compute the insertion indices
            #pragma omp single
            {
                size_t merged_count = *std::prev(std::partial_sum(needs_merge + begin, needs_merge + end, merged_index + begin));
                size_t unmerged_count = end - begin - merged_count;
                size_t children_count = merged_count * 2;
                children_begin = end - children_count;
                unmerged_begin = end - (children_count + unmerged_count);
                assert(merged_count > 0);
            }

            // Perform one step of node merging
            #pragma omp for nowait
            for (size_t i = begin; i < end; ++i) {
                if (needs_merge[i]) {
                    size_t unmerged_index = unmerged_begin + i + 1 - begin - merged_index[i];
                    auto& unmerged_node = output_nodes[unmerged_index];
                    auto first_child = children_begin + (merged_index[i] - 1) * 2;
                    unmerged_node.bounding_box_proxy() = input_nodes[i]
                        .bounding_box_proxy()
                        .to_bounding_box()
                        .extend(input_nodes[i + 1].bounding_box_proxy());
                    unmerged_node.is_leaf = false;
                    unmerged_node.first_child_or_primitive = first_child;
                    output_nodes[first_child + 0] = input_nodes[i + 0];
                    output_nodes[first_child + 1] = input_nodes[i + 1];
                    output_levels[unmerged_index] = input_levels[i + 1];
                } else if (i == begin || !needs_merge[i - 1]) {
                    size_t unmerged_index = unmerged_begin + i - begin - merged_index[i];
                    output_nodes [unmerged_index] = input_nodes[i];
                    output_levels[unmerged_index] = input_levels[i];
                }
            }

            // Copy the nodes of the previous level into the current array of nodes.
            #pragma omp for nowait
            for (size_t i = end; i < previous_end; ++i)
                output_nodes[i] = input_nodes[i];
        }

        return std::make_pair(unmerged_begin, children_begin);
    }

public:
    LinearBvhBuilder(Bvh& bvh)
        : bvh(bvh)
    {}

    void build(
        const BoundingBox<Scalar>* bboxes,
        const Vector3<Scalar>* centers,
        size_t primitive_count)
    {
        assert(primitive_count > 0);

        auto [primitive_indices, morton_codes] =
            sort_primitives_by_morton_code(bboxes, centers, primitive_count);

        auto node_count = 2 * primitive_count - 1;

        auto nodes          = std::make_unique<Node[]>(node_count);
        auto nodes_copy     = std::make_unique<Node[]>(node_count);
        auto auxiliary_data = std::make_unique<size_t[]>(node_count * 2);
        auto level_data     = std::make_unique<Level[]>(node_count * 2);

        size_t begin        = node_count - primitive_count;
        size_t end          = node_count;
        size_t previous_end = end;

        auto input_levels  = level_data.get();
        auto output_levels = level_data.get() + node_count;

        #pragma omp parallel
        {
            // Create the leaves
            #pragma omp for
            for (size_t i = 0; i < primitive_count; ++i) {
                auto& node = nodes[begin + i];
                node.bounding_box_proxy()     = bboxes[primitive_indices[i]];
                node.is_leaf                  = true;
                node.primitive_count          = 1;
                node.first_child_or_primitive = i;
            }

            // Compute the level of the tree where the current node is joined with the next.
            #pragma omp for
            for (size_t i = 0; i < primitive_count - 1; ++i)
                input_levels[begin + i] = count_leading_zeros(morton_codes[i] ^ morton_codes[i + 1]);
        }

        while (end - begin > 1) {
            input_levels[end - 1] = 0;

            auto [next_begin, next_end] = merge(
                nodes.get(),
                nodes_copy.get(),
                input_levels,
                output_levels,
                auxiliary_data.get(),
                auxiliary_data.get() + node_count,
                begin, end,
                previous_end);

            std::swap(nodes, nodes_copy);
            std::swap(input_levels, output_levels);

            previous_end = end;
            begin        = next_begin;
            end          = next_end;
        }

        nodes[0].first_child_or_primitive = 1;
        nodes[0].is_leaf = false;

        std::swap(bvh.nodes, nodes);
        std::swap(bvh.primitive_indices, primitive_indices);
        bvh.node_count = node_count;
    }
};

} // namespace bvh

#endif