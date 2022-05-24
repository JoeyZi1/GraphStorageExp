/* Copyright 2020 Guanyu Feng, Tsinghua University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <utility>
#include <chrono>
#include <thread>
#include <immintrin.h>
#include <omp.h>
#include "type.hpp"
#include "graph.hpp"
#include "io.hpp"

int main(int argc, char** argv)
{
    assert(argc > 2);
    std::pair<uint64_t, uint64_t> *raw_edges;
    uint64_t root = std::stoull(argv[2]);
    uint64_t raw_edges_len;
    std::tie(raw_edges, raw_edges_len) = mmap_binary(argv[1]);
    uint64_t num_vertices = 0;
    {
        auto start = std::chrono::system_clock::now();
        #pragma omp parallel for
        for(uint64_t i=0;i<raw_edges_len;i++)
        {
            const auto &e = raw_edges[i];
            write_max(&num_vertices, e.first+1);
            write_max(&num_vertices, e.second+1);
        }
        auto end = std::chrono::system_clock::now();
        fprintf(stderr, "read: %.6lfs\n", 1e-6*(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
        fprintf(stderr, "|E|=%lu\n", raw_edges_len);
    }
    Graph<void> graph(num_vertices, raw_edges_len, false, true);
    //std::random_shuffle(raw_edges.begin(), raw_edges.end());
    {
        auto start = std::chrono::system_clock::now();
        #pragma omp parallel for
        for(uint64_t i=0;i<raw_edges_len;i++)
        {
            const auto &e = raw_edges[i];
            graph.add_edge({e.first, e.second}, true);
        }
        auto end = std::chrono::system_clock::now();
        fprintf(stderr, "add: %.6lfs\n", 1e-6*(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());
    }

    auto labels = graph.alloc_vertex_tree_array<uint64_t>();
    const uint64_t MAXL = 65536;
    auto continue_reduce_func = [](uint64_t depth, uint64_t total_result, uint64_t local_result) -> std::pair<bool, uint64_t>
    {
        return std::make_pair(local_result>0, total_result+local_result);
    };
    auto continue_reduce_print_func = [](uint64_t depth, uint64_t total_result, uint64_t local_result) -> std::pair<bool, uint64_t>
    {
        fprintf(stderr, "active(%lu) >= %lu\n", depth, local_result);
        return std::make_pair(local_result>0, total_result+local_result);
    };
    auto update_func = [](uint64_t src, uint64_t dst, uint64_t src_data, uint64_t dst_data, decltype(graph)::adjedge_type adjedge) -> std::pair<bool, uint64_t>
    {
        return std::make_pair(src_data+1 < dst_data, src_data + 1);
    };
    auto active_result_func = [](uint64_t old_result, uint64_t src, uint64_t dst, uint64_t src_data, uint64_t old_dst_data, uint64_t new_dst_data) -> uint64_t
    {
        return old_result+1;
    };
    auto equal_func = [](uint64_t src, uint64_t dst, uint64_t src_data, uint64_t dst_data, decltype(graph)::adjedge_type adjedge) -> bool
    {
        return src_data + 1 == dst_data;
    };
    auto init_label_func = [=](uint64_t vid) -> std::pair<uint64_t, bool>
    {
        return {vid==root?0:MAXL, vid==root};
    };
    {
        graph.build_tree<uint64_t, uint64_t>(
            init_label_func,
            continue_reduce_print_func,
            update_func,
            active_result_func,
            labels
        );
    }

    //for(uint64_t i=imported_edges;i<raw_edges_len;i++)
    //{
    //    if((i-imported_edges)%10000 == 0)
    //    {
    //        graph.get_dense_active_in().clear();
    //        graph.get_dense_active_in().set_bit(root);

    //        graph.stream_vertices<uint64_t>(
    //            [&](uint64_t vid)
    //            {
    //                labels[vid].data = init_label_func(vid);
    //                return 1;
    //            },
    //            graph.get_dense_active_all()
    //        );
    //        labels[root].data = 0;

    //        graph.build_tree<uint64_t, uint64_t>(
    //            continue_reduce_func,
    //            update_func,
    //            active_result_func,
    //            labels
    //        );

    //        std::vector<std::atomic_uint64_t> layer_counts(MAXL);
    //        for(auto &a : layer_counts) a = 0;
    //        graph.stream_vertices<uint64_t>(
    //            [&](uint64_t vid)
    //            {
    //                if(labels[vid].data != MAXL)
    //                {
    //                    layer_counts[labels[vid].data]++;
    //                    return 1;
    //                }
    //                return 0;
    //            },
    //            graph.get_dense_active_all()
    //        );
    //        for(uint64_t i=0;i<layer_counts.size();i++)
    //        {
    //            if(layer_counts[i] > 0)
    //            {
    //                printf("%lu ", layer_counts[i].load());
    //            }
    //            else
    //            {
    //                printf("\n");
    //                break;
    //            }
    //        }
    //    }
    //    {
    //        const auto &e = raw_edges[i];
    //        graph.add_edge({e.first, e.second}, true);
    //    }
    //    {
    //        const auto &e = raw_edges[i-imported_edges];
    //        graph.del_edge({e.first, e.second}, true);
    //    }
    //}
    {
        std::vector<std::atomic_uint64_t> layer_counts(MAXL);
        for(auto &a : layer_counts) a = 0;
        graph.stream_vertices<uint64_t>(
            [&](uint64_t vid)
            {
                if(labels[vid].data != MAXL)
                {
                    layer_counts[labels[vid].data]++;
                    return 1;
                }
                return 0;
            },
            graph.get_dense_active_all()
        );
        for(uint64_t i=0;i<layer_counts.size();i++)
        {
            if(layer_counts[i] > 0)
            {
                printf("%lu ", layer_counts[i].load());
            }
            else
            {
                printf("\n");
                break;
            }
        }
    }

    return 0;
}
