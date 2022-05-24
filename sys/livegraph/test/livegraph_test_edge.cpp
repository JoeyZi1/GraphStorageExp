//
// Created by zxy on 5/7/22.
//

#include "livegraph_test.h"



void batch_ins_del_read(commandLine& P){
    PRINT("=============== Batch Insert BEGIN ===============");

    auto gname = P.getOptionValue("-gname", "none");
    std::ofstream alg_file("../../../log/livegraph/edge.log",ios::app);
    alg_file << "GRAPH" << "\t"+gname <<"\t["<<getCurrentTime0()<<']'<<std::endl;
    auto thd_num = P.getOptionLongValue("-core", 1);
    alg_file << "Using threads :" << "\t"<<thd_num<<endl;

    std::vector<uint32_t> update_sizes = {10, 100, 1000 ,10000,100000,1000000,10000000};//
    auto r = random_aspen();
    auto update_times = std::vector<double>();
    size_t n_trials = 1;
    for (size_t us=0; us<update_sizes.size(); us++) {
        printf("GN: %lu \n",G->get_max_vertex_id() );
        double avg_insert = 0;
        double avg_delete = 0;
        double avg_read = 0;
        std::cout << "Running batch size: " << update_sizes[us] << std::endl;

        if (update_sizes[us] <= 10000000)
            n_trials = 20;
        else n_trials = 5;
        size_t updates_to_run = update_sizes[us];
        auto perm = get_random_permutation(updates_to_run);
        for (size_t ts=0; ts<n_trials; ts++) {
            uint64_t GN = G->get_max_vertex_id();
            new_srcs.clear();
            new_dests.clear();

            double a = 0.5;
            double b = 0.1;
            double c = 0.1;
            size_t nn = 1 << (log2_up(GN) - 1);
            auto rmat = rMat<uint32_t>(nn, r.ith_rand(100+ts), a, b, c);
            for( uint32_t i = 0; i < updates_to_run; i++) {
                std::pair<uint32_t, uint32_t> edge = rmat(i);
                new_srcs.push_back(edge.first);
                new_dests.push_back(edge.second);
            }

            vertex_dictionary_t::const_accessor accessor1, accessor2;  // shared lock on the dictionary

            // insert edge
            gettimeofday(&t_start, &tzp);
            auto tx1 = G->begin_transaction();

            for (uint32_t i =0 ; i< updates_to_run;i++){
                VertexDictionary->find(accessor1, new_srcs[i]);
                VertexDictionary->find(accessor2, new_dests[i]);
                lg::vertex_t internal_source_id = accessor1->second;
                lg::vertex_t internal_destination_id = accessor2->second;
                int w = 1;string_view weight { (char*) &w, sizeof(w) };
                tx1.put_edge(internal_source_id, /* label */ 0, internal_destination_id, weight);
            }
            tx1.commit();

            gettimeofday(&t_end, &tzp);
            avg_insert += cal_time_elapsed(&t_start, &t_end);

            // read edge
            gettimeofday(&t_start, &tzp);
            auto tx2 = G->begin_read_only_transaction();
            for (uint32_t i =0 ; i< updates_to_run;i++){
                VertexDictionary->find(accessor1, new_srcs[i]);
                VertexDictionary->find(accessor2, new_dests[i]);
                lg::vertex_t internal_source_id = accessor1->second;
                lg::vertex_t internal_destination_id = accessor2->second;

                string_view lg_weight = tx2.get_edge(internal_source_id, /* label */ 0, internal_destination_id);
            }
            tx2.abort();
            gettimeofday(&t_end, &tzp);
            avg_read += cal_time_elapsed(&t_start, &t_end);

            // del edge
            gettimeofday(&t_start, &tzp);

            auto tx3 = G->begin_transaction();
            for (uint32_t i =0 ; i< updates_to_run;i++){
                VertexDictionary->find(accessor1, new_srcs[i]);
                VertexDictionary->find(accessor2, new_dests[i]);
                lg::vertex_t internal_source_id = accessor1->second;
                lg::vertex_t internal_destination_id = accessor2->second;
                tx3.del_edge(internal_source_id, /* label */ 0, internal_destination_id);
            }
            tx3.commit();
            gettimeofday(&t_end, &tzp);
            avg_delete +=  cal_time_elapsed(&t_start, &t_end);
        }
        double time_i = (double) avg_insert / n_trials;
        double insert_throughput = updates_to_run / time_i;
        printf("batch_size = %zu, average insert: %f, throughput %e\n", updates_to_run, time_i, insert_throughput);
        alg_file <<"\t["<<getCurrentTime0()<<']' << "Insert"<< "\tbatch_size=" << updates_to_run<< "\ttime=" << time_i<< "\tthroughput=" << insert_throughput << std::endl;

        double time_r = (double) avg_read / n_trials;
        double read_throughput = updates_to_run / time_r;
        printf("batch_size = %zu, average read: %f, throughput %e\n", updates_to_run, time_r, read_throughput);
        alg_file <<"\t["<<getCurrentTime0()<<']' << "Read"<< "\tbatch_size=" << updates_to_run<< "\ttime=" << time_r<< "\tthroughput=" << read_throughput << std::endl;

        double time_d = (double) avg_delete / n_trials;
        double delete_throughput = updates_to_run / time_d;
        printf("batch_size = %zu, average delete: %f, throughput %e\n", updates_to_run, time_d, delete_throughput);
        alg_file <<"\t["<<getCurrentTime0()<<']' << "Delete"<< "\tbatch_size=" << updates_to_run<< "\ttime=" << time_d<< "\tthroughput=" << delete_throughput << std::endl;
    }
    PRINT("=============== Batch Insert END ===============");
}


// -src 9 -maxiters 5 -f ../../../data/slashdot.adj
// -gname LiveJournal -core 1 -f ../../../data/ADJgraph/LiveJournal.adj
int main(int argc, char** argv) {
    srand(time(NULL));
    commandLine P(argc, argv, "./graph_bm [-t testname -r rounds -f file");
    auto thd_num = P.getOptionLongValue("-core", 1);
    printf("Running LiveGraph using %ld threads.\n", thd_num );

    load_graph(P);

    batch_ins_del_read(P);

    del_G();
    printf("!!!!! TEST OVER !!!!!\n");
    return 0;
}