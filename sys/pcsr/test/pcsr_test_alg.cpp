
#define OPENMP 1
#include "pcsr_test.h"
#include "BFS.h"
#include "PR.h"
#include "SSSP.h"
#include "CC.h"
#include "k-hop.h"
#include "LP.h"
#include "TC.h"
#include "parallel.h"

double test_bfs(commandLine& P) {
    long bfs_src = P.getOptionLongValue("-src",9);
    std::cout << "Running BFS from source = " << bfs_src << std::endl;

    gettimeofday(&t_start, &tzp);
    BFS(G, bfs_src);
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}


double test_pr(commandLine& P) {
    long maxiters = P.getOptionLongValue("-maxiters",10);
    gettimeofday(&t_start, &tzp);
    PR<double, PCSR>(G, maxiters);
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}

double test_sssp(commandLine& P){
    long sssp_src = P.getOptionLongValue("-src",9);
    gettimeofday(&t_start, &tzp);
    SSSP(G, sssp_src);
    gettimeofday(&t_end, &tzp);

    return cal_time_elapsed(&t_start, &t_end);
}

double test_cc(commandLine& P) {
    gettimeofday(&t_start, &tzp);
    CC(G);
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}

double test_k_hop(commandLine& P, int k) {
    gettimeofday(&t_start, &tzp);
//    K_HOP(G, k);
    uint64_t n = G->get_n();
    uint32_t nsrc = n/20;
    srand(n);
    parallel_for(int i=0;i<nsrc;i++){
        auto rdsrc = rand()%n;
        uint32_t sta = G->nodes[rdsrc].beginning;
        uint32_t end = G->nodes[rdsrc].end;
        for (uint32_t i = sta + 1; i < end; i++) {
            uint32_t v = G->edges.items[i].dest;
            uint32_t w = G->edges.items[i].value;
            if(k==2) {
                uint32_t sta2 = G->nodes[v].beginning;
                uint32_t end2 = G->nodes[v].end;
                for (uint32_t j = sta2 + 1; j < end2; j++) {
                    uint32_t v2 = G->edges.items[j].dest;
                }
            }
        }
    }
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}

double test_lp(commandLine& P) {

    long maxiters = P.getOptionLongValue("-maxiters",10);
    gettimeofday(&t_start, &tzp);
    LP(G, maxiters);
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}

double test_tc(commandLine& P) {

    gettimeofday(&t_start, &tzp);
    auto count = TC(G);
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}


double test_read(commandLine& P) {
    auto r = random_aspen();
    uint64_t n = G->get_n();
    double a = 0.5;
    double b = 0.1;
    double c = 0.1;
    size_t nn = 1 << (log2_up(n) - 1);
    auto rmat = rMat<uint32_t>(nn, r.ith_rand(100), a, b, c);
    new_srcs.clear();new_dests.clear();
    uint32_t updates = num_edges/20;
    for( uint32_t i = 0; i < updates; i++) {
        std::pair<uint32_t, uint32_t> edge = rmat(i);
        new_srcs.push_back(edge.first);
        new_dests.push_back(edge.second);
    }
    gettimeofday(&t_start, &tzp);
    parallel_for(uint32_t i = 0; i < updates; i++) {
        G->find_value(new_srcs[i], new_dests[i]);
    }
    gettimeofday(&t_end, &tzp);
    return cal_time_elapsed(&t_start, &t_end);
}


double execute(commandLine& P, string testname) {
    if (testname == "BFS") {
        return test_bfs(P);
    } else if (testname == "PR") {
        return test_pr(P);
    } else if (testname == "SSSP") {
        return test_sssp(P);
    } else if (testname == "CC") {
        return test_cc(P);
    } else if (testname == "1-HOP") {
        return test_k_hop(P, 1);
    }else if (testname == "2-HOP") {
        return test_k_hop(P, 2);
    } else if (testname == "LP") {
        return test_lp(P);
    } else if (testname == "TC") {
        return test_tc(P);
    } else if (testname == "Read") {
        return test_read(P);
    } else {
        std::cout << "Unknown test: " << testname << ". Quitting." << std::endl;
        exit(0);
    }
}

void run_algorithm(commandLine& P) {
    PRINT("=============== Run Algorithm BEGIN ===============");

    std::vector<std::string> test_ids;
//    test_ids = {"1-HOP","2-HOP","BFS","SSSP","PR","CC","LP","Read","TC"};
    test_ids = {"1-HOP","2-HOP","Read"};

    size_t rounds = P.getOptionLongValue("-rounds", 5);
    auto gname = P.getOptionValue("-gname", "none");
    auto thd_num = P.getOptionLongValue("-core", 1);
    auto log = P.getOptionValue("-log", "none");
    std::ofstream alg_file(log, ios::app);

    for (auto test_id : test_ids) {
        std::vector<double> total_time;
        for (size_t i=0; i<rounds; i++) {
            double tm = execute(P, test_id);

            std::cout << "\ttest=" << test_id << "\ttime=" << tm << "\titeration=" << i << std::endl;
            total_time.emplace_back(tm);
        }
        double avg_time = cal_time(total_time);
        std::cout << "\ttest=" << test_id<< "\ttime=" << avg_time << "\tgraph=" << gname << std::endl;
        alg_file << gname<<","<< thd_num<<","<<test_id<<","<< avg_time << std::endl;
    }
    alg_file.close();
    PRINT("=============== Run Algorithm END ===============");
}


// -gname livejournal -core 16 -f ../../../data/ADJgraph/livejournal.adj -log ../../../log/risgraph/alg.log
int main(int argc, char** argv) {
    commandLine P(argc, argv );
    auto thd_num = P.getOptionLongValue("-core", 1);
    omp_set_num_threads(thd_num);
    printf("Running PCSR using %ld threads.\n", thd_num );

    load_graph(P);
    run_algorithm(P);
    del_G();

    printf("!!!!! TEST OVER !!!!!\n");
    return 0;
}

