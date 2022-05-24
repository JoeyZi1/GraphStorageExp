//#define OPENMP 1
//#define CILK 1

#include "graph/api.h"
#include "algorithms/BFS.h"
#include "algorithms/BC.h"
#include "algorithms/LDD.h"
//#include "algorithms/k-Hop.h"
#include "algorithms/k_hop.h"
#include "algorithms/mutual_friends.h"
#include "algorithms/MIS.h"
#include "algorithms/Nibble.h"
#include "algorithms/PR.h"
#include "algorithms/CC.h"
#include "algorithms/LP.h"
#include "trees/utils.h"

#include "omp.h"

#include <cstring>
#include <chrono>
#include <thread>
#include <cmath>
#include <random>

static std::string getCurrentTime0() {
    std::time_t result = std::time(nullptr);
    std::string ret;
    ret.resize(64);
    int wsize = sprintf((char *)&ret[0], "%s", std::ctime(&result));
    ret.resize(wsize-1);
    return ret;
}

double cal_time(std::vector<double> timelist){
    if(timelist.size() == 1) return timelist[0];
    sort(timelist.begin(),timelist.end());
    double st = 0.0;
    for(uint32_t i = 1 ;i < timelist.size()-1;i++)
        st += timelist[i];
    return st/double(timelist.size()-2);
}

template <class G>
double test_bfs(G& GA, commandLine& P) {
    bool no_snapshot = P.getOptionValue("-noflatsnap");
    bool print_stats = P.getOptionValue("-stats");
    long src = P.getOptionLongValue("-src",-1);
    if (src == -1) {
        std::cout << "Please specify a source vertex to run the BFS from using -src" << std::endl;
        exit(0);
    }
    std::cout << "Running BFS from source = " << src << std::endl;
    timer bfst; bfst.start();
    if (no_snapshot) {
        BFS(GA, src, print_stats);
    } else {
        BFS_Fetch(GA, src, print_stats);
    }
    bfst.stop();
    return bfst.get_total();
}


template <class G>
double test_k_hop(G& GA, commandLine& P, int k) {
    timer tmr; tmr.start();
    K_HOP(GA, k);
    tmr.stop();
    return (tmr.get_total());
}



template <class G>
double test_pr(G& GA, commandLine& P) {

    long maxiters = P.getOptionLongValue("-maxiters",10);

    timer tmr; tmr.start();
    PR<double,G>(GA,maxiters);
    tmr.stop();
    return (tmr.get_total());
}

template <class G>
double test_cc(G& GA, commandLine& P) {

    timer tmr; tmr.start();
    CC<G>(GA);
    tmr.stop();
    return (tmr.get_total());
}

template <class G>
double test_lp(G& GA, commandLine& P) {

    long maxiters = P.getOptionLongValue("-maxiters",10);

    timer tmr; tmr.start();
    LP(GA,maxiters);
    tmr.stop();
    return (tmr.get_total());
}



template <class G>
double test_tc(G& GA, commandLine& P) {
    size_t num_sources = static_cast<size_t>(P.getOptionLongValue("-nsrc",1024));
    double epsilon = P.getOptionDoubleValue("-e",0.000001);
    size_t T = static_cast<size_t>(P.getOptionLongValue("-T",10));
    std::cout << std::scientific << setprecision(3) << "Running Nibble,  epsilon = " << epsilon << " T = " << T << std::endl;

    size_t n = GA.num_vertices();
    auto r = pbbs::random();
    timer t; t.start();
    size_t i = 0;
    size_t b_size = P.getOptionLongValue("-BS", num_sources);
    while (i < num_sources) {
        size_t end = std::min(i + b_size, num_sources);
        parallel_for(i, end, [&] (size_t j) {
            uintV src = r.ith_rand(j) % n;
            NibbleSerial(GA, src, epsilon, T);
        }, 1);
        i += b_size;
    }
    t.stop();
    return (t.get_total() / num_sources);
}




template <class Graph>
double execute(Graph& G, commandLine& P, string testname) {
    if (testname == "BFS") {
        return test_bfs(G, P);
    } else if (testname == "PR") {
        return test_pr(G, P);
    } else if (testname == "CC") {
        return test_cc(G, P);
    } else if (testname == "1-HOP") {
        return test_k_hop(G, P, 1);
    }else if (testname == "2-HOP") {
        return test_k_hop(G, P, 2);
    } else if (testname == "TC") {
        return test_tc(G, P);
    } else if (testname == "LP") {
        return test_lp(G, P);
    } else {
        std::cout << "Unknown test: " << testname << ". Quitting." << std::endl;
        exit(0);
    }
}

void run_algorithm(commandLine& P) {
    size_t threads = num_workers();

    auto VG = initialize_treeplus_graph(P);
    cout<<"init over"<<endl;
    // Run the algorithm on it
    size_t rounds = P.getOptionLongValue("-rounds", 4);

    auto gname = P.getOptionValue("-gname", "none");
    std::ofstream alg_file("../../../log/aspen/alg.log",ios::app);
    alg_file << "GRAPH" << "\t"+gname <<"\t["<<getCurrentTime0()<<']'<<std::endl;
    auto thd_num = P.getOptionLongValue("-core", 1);
    alg_file << "Using threads :" << "\t"<<thd_num<<endl;

    std::vector<std::string> test_ids;
    test_ids = {"BFS","PR","LP","CC","1-HOP","2-HOP"};//

    for (auto test_id : test_ids) {
        std::vector<double> total_time;
        for (size_t i = 0; i < rounds; i++) {
            auto S = VG.acquire_version();
            double tm = execute(S.graph, P, test_id);
            std::cout << "RESULT" << fixed << setprecision(6)<< "\ttest=" << test_id<< "\ttime=" << tm<< "\titeration=" << i<< std::endl;
            total_time.emplace_back(tm);
            VG.release_version(std::move(S));
        }
        double avg_time = cal_time(total_time);
        std::cout << "["<<getCurrentTime0()<<']' << fixed << setprecision(6)<< "\ttest=" << test_id<< "\ttime=" << avg_time<< std::endl;
        alg_file <<"\t["<<getCurrentTime0()<<']' << "AVG"<< "\ttest=" << test_id<< "\ttime=" << avg_time << std::endl;
    }
}


// -src 9 -s -gname LiveJournal -core 1 -f ../../../data/ADJgraph/LiveJournal.adj
// -t BFS -src 1 -r 4 -s -f ../../../data/slashdot.adj
int main(int argc, char** argv) {

    commandLine P(argc, argv );
    auto thd_num = P.getOptionLongValue("-core", 2);
//    omp_set_num_threads(thd_num);
    printf("Running Aspen using %ld threads.\n", thd_num );

//    parallel_for(0,100,[&](int i){
//        printf("%d ", i);
//    },1);
//    _Pragma("omp parallel for") for
//#pragma omp parallel for
//    for(int i = 0;i<100;i++){
//        printf("%d ", i);
//    }
//    cout<<endl;
    run_algorithm(P);
}
