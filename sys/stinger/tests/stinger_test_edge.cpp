//
// Created by zxy on 5/6/22.
//
#include "stinger_test.h"
#include <thread>


template<typename graph>
void insert_edges(graph *GA, std::vector<uint32_t> &new_srcs, std::vector<uint32_t> &new_dests, int num_threads){
    auto routine_insert_edges = [&](int thread_id, uint64_t start, uint64_t length){
        for(int64_t pos = start, end = start + length; pos < end; pos++){
            stinger_insert_edge(GA, 0,new_srcs[pos] , new_dests[pos], 1, 0);
        }
    };
    int64_t edges_per_thread = new_srcs.size() / num_threads;
    int64_t odd_threads = new_srcs.size() % num_threads;
    vector<thread> threads;
    int64_t start = 0;
    for(int thread_id = 0; thread_id < num_threads; thread_id ++){
        int64_t length = edges_per_thread + (thread_id < odd_threads);
        threads.emplace_back(routine_insert_edges, thread_id, start, length);
        start += length;
    }
    for(auto& t : threads) t.join();
    threads.clear();
}

template<typename graph>
void delete_edges(graph *GA, std::vector<uint32_t> &new_srcs, std::vector<uint32_t> &new_dests, int num_threads){
    auto routine_insert_edges = [&](int thread_id, uint64_t start, uint64_t length){
        for(int64_t pos = start, end = start + length; pos < end; pos++){
            if(new_srcs[pos] != new_dests[pos])
                stinger_remove_edge (GA, 0 ,new_srcs[pos] , new_dests[pos] );
        }
    };
    int64_t edges_per_thread = new_srcs.size() / num_threads;
    int64_t odd_threads = new_srcs.size() % num_threads;
    vector<thread> threads;
    int64_t start = 0;
    for(int thread_id = 0; thread_id < num_threads; thread_id ++){
        int64_t length = edges_per_thread + (thread_id < odd_threads);
        threads.emplace_back(routine_insert_edges, thread_id, start, length);
        start += length;
    }
    for(auto& t : threads) t.join();
    threads.clear();
}



void batch_ins_del_read(commandLine& P){
    PRINT("=============== Batch Insert BEGIN ===============");

    auto gname = P.getOptionValue("-gname", "none");
    auto thd_num = P.getOptionLongValue("-core", 1);
    auto log = P.getOptionValue("-log","none");
    std::ofstream log_file(log, ios::app);

    stinger &Ga = *G;
    std::vector<uint32_t> update_sizes = {10, 100, 1000, 10000, 100000, 1000000, 10000000};
    auto r = random_aspen();
    auto update_times = std::vector<double>();
    size_t n_trials = 1;
    for (size_t us=0; us<update_sizes.size(); us++) {
        //printf("GN: %u %u\n",Ga.max_nv,Ga.edges.N);
        double avg_insert = 0;
        double avg_delete = 0;
        double avg_read = 0;
        std::cout << "Running batch size: " << update_sizes[us] << std::endl;

        if (update_sizes[us] < 10000000)
            n_trials = 20;
        else n_trials = 5;
        size_t updates_to_run = update_sizes[us];
        auto perm = get_random_permutation(updates_to_run);
        for (size_t ts=0; ts<n_trials; ts++) {
            uint32_t num_nodes = Ga.max_nv;
            new_srcs.clear();
            new_dests.clear();

            double a = 0.5;
            double b = 0.1;
            double c = 0.1;
            size_t nn = 1 << (log2_up(num_nodes) - 1);
            auto rmat = rMat<uint32_t>(nn, r.ith_rand(100+ts), a, b, c);
            for( uint32_t i = 0; i < updates_to_run; i++) {
                std::pair<uint32_t, uint32_t> edge = rmat(i);
                new_srcs.push_back(edge.first);
                new_dests.push_back(edge.second);
            }
//            std::vector<update> updates;
//            for (int i = 0; i < updates_to_run; i++){
//                update u = {
//                        0, // type
//                        new_srcs[i], // source
//                        new_dests[i], // destination
//                        1, // weight
//                        int(ts)*100, // time
//                        0 // result
//                };
//                updates.push_back(u);
//            }
            gettimeofday(&t_start, &tzp);
            insert_edges(G, new_srcs, new_dests, thd_num);
//            stinger_batch_insert_edges<update>(G, updates.begin(), updates.end());
//            for (int i = 0; i < updates_to_run; i++) {
//                stinger_insert_edge(G, 0,new_srcs[i] , new_dests[i], 1, 0);//G, type, from, to, weight, timestamp
//            }
            gettimeofday(&t_end, &tzp);
            avg_insert += cal_time_elapsed(&t_start, &t_end);

            gettimeofday(&t_start, &tzp);
            delete_edges(G, new_srcs, new_dests, thd_num);
//            for(uint32_t i = 0; i < updates_to_run; i++) {
//                if(new_srcs[i]!= new_dests[i])
//                    stinger_remove_edge (G, 0 ,new_srcs[i] , new_dests[i] );
//            }
            gettimeofday(&t_end, &tzp);
            avg_delete +=  cal_time_elapsed(&t_start, &t_end);

        }
        double time_i = (double) avg_insert / n_trials;
        double insert_throughput = updates_to_run / time_i;
        printf("batch_size = %zu, average insert: %f, throughput %e\n", updates_to_run, time_i, insert_throughput);
        log_file<< gname<<","<<thd_num<<",e,insert,"<< update_sizes[us] <<","<<insert_throughput << "\n";

        double time_d = (double) avg_delete / n_trials;
        double delete_throughput = updates_to_run / time_d;
        printf("batch_size = %zu, average delete: %f, throughput %e\n", updates_to_run, time_d, delete_throughput);
        log_file<< gname<<"," <<thd_num<<",e,delete,"<< update_sizes[us] <<","<<delete_throughput << "\n";
    }
    PRINT("=============== Batch Insert END ===============");
}


// -gname livejournal -core 8 -f ../../../data/ADJgraph/livejournal.adj -log ../../../log/stinger/edge.log
// -gname uniform-24 -core 16 -f ../../../data/ADJgraph/uniform-24.adj -log ../../../log/stinger/edge.log
int main(int argc, char** argv) {
    srand(time(NULL));
    commandLine P(argc, argv);
    auto thd_num = P.getOptionLongValue("-core", 1);
    omp_set_num_threads(thd_num);

    printf("Running Stinger using %ld threads.\n", thd_num );
    load_graph(P);

    batch_ins_del_read(P);

    del_G();
    printf("!!!!! TEST OVER !!!!!\n");
    return 0;
}