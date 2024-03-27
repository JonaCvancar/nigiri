#include "nigiri/loader/dir.h"

#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/loader/loader_interface.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/routing/tripbased/transfer_set.h"
#include "nigiri/timetable.h"
#include "./paths_oa.h"
#include "./periods_oa.h"
#include "utl/progress_tracker.h"
#include "./test_queries.h"

#include <random>
#include <chrono>

using namespace nigiri;
using namespace nigiri::loader;
using namespace nigiri::routing;
using namespace nigiri::routing::tripbased;
using namespace nigiri::routing::tripbased::onetoall;
using namespace std::chrono;

int main() {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("aachen");

  // init timetable
  timetable tt;
  tt.date_range_ = aachen_period_oa();
  register_special_stations(tt);
  constexpr auto const src = source_idx_t{0U};
  load_timetable(src, loader::hrd::hrd_test_avv, aachen_dir_oa, tt);
  finalize(tt);

  // run preprocessing
  transfer_set ts;
  build_transfer_set(tt, ts);

  unixtime_t begin_d = unixtime_t{sys_days{June / 11 / 2023}};
  unixtime_t end_d = unixtime_t{sys_days{June / 11 / 2023}};

  int begin_h = 0;
  int end_h = 23;
  unixtime_t start_time = begin_d + hours{begin_h};
  unixtime_t end_time = end_d + hours{end_h} + minutes{59};

  std::string outputDir = "/home/jona/uni/thesis/results/aachen/";

  auto const locations = routing::tripbased::onetoall::get_locations((aachen_dir_oa.path().string() + "/" + "bahnhof"));

  std::random_device rd;
  std::mt19937 gen(rd());

  unsigned long location_count = locations.size();
  std::uniform_int_distribution<> dis_start(0, static_cast<int>(location_count));

  std::uniform_int_distribution<> dis(0, static_cast<int>(location_count));

  int days_count = aachen_period_oa().size().count();
  std::uniform_int_distribution<> dis_days(0, static_cast<int>(days_count));

  int n_days = 50;
  int n_tests_oa = 50;
  int n_tests = 25;


  std::vector<int> spans = {2, 12, 24};

  std::string filename = outputDir + "onetoall.txt";

  std::ofstream outputFile(filename, std::ios::app);
  outputFile << "Stop"
             << " Runtime"
             << " journey_count"
             << " stop_count_not_empty"
             << " stop_count_empty"
             << " segments_count"
             << " max_transfers"
             << " segments_pruned"
             << " equal_journey"
             << " queue_handling"
             << " bitfield_idx"
             << " add_ontrip"
             << " peak_memory"
             << " exceeds_preprocessor"
             << "\n";
  outputFile.close();

  for(int oa = 0; oa < n_tests_oa; ++oa) {
    unsigned long start = dis_start(gen);
    location_idx_t start_loc_idx =
        tt.locations_.location_id_to_idx_.at({locations[start], src});
    TBDL << oa << " From: " << location_name(tt, start_loc_idx) << "_"
         << start_loc_idx.v_ << "\n";
    auto start_timer = steady_clock::now();
    auto result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                            interval{start_time, end_time});
    auto stop_timer = steady_clock::now();

    auto results = *result_stats.journeys_;
    int count = 0;
    int stop_count_not_empty = 0;
    int stop_count_empty = 0;
    for (const auto& element : results) {
      if(element.size()) {
        count += element.size();
        stop_count_not_empty++;
      } else {
        stop_count_empty++;
      }
    }
    TBDL << "Number of trips in total: " << count << "\n";

    TBDL << "AllToAll calculation took: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer -
                                                                  start_timer)
                .count()
         << "\n";

    std::ofstream outputFile(filename, std::ios::app);
    outputFile << "" << locations[start]
               << "" << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer - start_timer).count()
               << " " << count
               << " " << stop_count_not_empty
               << " " << stop_count_empty
               << " " << result_stats.algo_stats_.n_segments_enqueued_
               << " " << result_stats.algo_stats_.max_transfers_reached_
               << " " << result_stats.algo_stats_.n_segments_pruned_
               << " " << result_stats.algo_stats_.equal_journey_
               << " " << result_stats.algo_stats_.tb_queue_handling_
               << " " << result_stats.algo_stats_.tb_onetoall_bitfield_idx_
               << " " << result_stats.algo_stats_.tb_oa_add_ontrip_
               << " " << result_stats.algo_stats_.peak_memory_usage_
               << " " << (result_stats.algo_stats_.peak_memory_usage_ > result_stats.algo_stats_.pre_memory_usage_)
               << "\n";
    outputFile.close();

    /*
    if(count == 0) {
      continue;
    }

    for (auto& span : spans) {
      std::uniform_int_distribution<> dis2(begin_h, end_h - (span-1));

      int i = 0;
      for (; i < n_tests; i++) {
        auto end_location = static_cast<unsigned long>(dis(gen));
        location_idx_t end_loc_idx =
            tt.locations_.location_id_to_idx_.at({locations[end_location], src});
        while (results[end_loc_idx.v_].size() == 0) {
          end_location = static_cast<unsigned long>(dis(gen));
          end_loc_idx =
              tt.locations_.location_id_to_idx_.at({locations[end_location], src});
        }

        if (end_loc_idx.v_ == start_loc_idx.v_) {
          while (end_loc_idx.v_ == start_loc_idx.v_) {
            end_location = static_cast<unsigned long>(dis(gen));
            end_loc_idx =
                tt.locations_.location_id_to_idx_.at({locations[end_location], src});
          }
        }
        TBDL << i << " To: " << location_name(tt, end_loc_idx) << "_"
             << end_loc_idx.v_ << ", Results length: " << results.size() <<  "\n";

        std::string filename_oa = outputDir + "oa_" + std::to_string(span) + ".txt";
        std::string filename_tb = outputDir + "tb_" + std::to_string(span) + ".txt";
        std::string filename_rp = outputDir + "rp_" + std::to_string(span) + ".txt";
        std::string filename_oa_ea = outputDir + "oa_ea_" + std::to_string(span) + ".txt";
        std::string filename_tb_ea = outputDir + "tb_ea_" + std::to_string(span) + ".txt";
        std::string filename_rp_ea = outputDir + "rp_ea_" + std::to_string(span) + ".txt";

        for (int n_day = 0; n_day < n_days; n_day++) {
          int day = dis_days(gen);
          int h1 = dis2(gen);

          auto query_interval = interval{begin_d + days{day} + hours{h1},
                end_d + days{day} + hours{h1 + (span-1)} + minutes{59}};

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_oa;
          routing::tripbased::onetoall::tripbased_onetoall_query(
              tt, results[end_loc_idx.v_], results_oa,
              query_interval);
          stop_timer = steady_clock::now();
          std::ofstream outputFile_oa(filename_oa, std::ios::app);
          outputFile_oa << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                     << " " << results_oa.size() << "\n";
          outputFile_oa.close();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_tb = tripbased_search(
              tt, ts, start_loc_idx, end_loc_idx,
              query_interval);
          stop_timer = steady_clock::now();
          std::ofstream outputFile_tb(filename_tb, std::ios::app);
          outputFile_tb << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                        << " " << results_tb.size() << "\n";
          outputFile_tb.close();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_rp = raptor_search(
              tt, nullptr, start_loc_idx, end_loc_idx,
              query_interval);
          stop_timer = steady_clock::now();
          std::ofstream outputFile_rp(filename_rp, std::ios::app);
          outputFile_rp << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                        << " " << results_rp.size() << "\n";
          outputFile_rp.close();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_oa_ea;
          tripbased_onetoall_query(
              tt, results[end_loc_idx.v_], results_oa_ea,
              {begin_d + days{day} + hours{h1}});
          stop_timer = steady_clock::now();
          std::ofstream outputFile_oa_ea(filename_oa_ea, std::ios::app);
          outputFile_oa_ea << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                        << " " << results_oa_ea.size() << "\n";
          outputFile_oa_ea.close();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_tb_ea = tripbased_search(
              tt, ts, start_loc_idx, end_loc_idx,
              {begin_d + days{day} + hours{h1}});
          stop_timer = steady_clock::now();
          std::ofstream outputFile_tb_ea(filename_tb_ea, std::ios::app);
          outputFile_tb_ea << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                           << " " << results_tb_ea.size() << "\n";
          outputFile_tb_ea.close();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_rp_ea = raptor_search(
              tt, nullptr, start_loc_idx, end_loc_idx,
              {begin_d + days{day} + hours{h1}});
          stop_timer = steady_clock::now();
          std::ofstream outputFile_rp_ea(filename_rp_ea, std::ios::app);
          outputFile_rp_ea << std::chrono::duration_cast<std::chrono::microseconds>(stop_timer - start_timer).count()
                           << " " << results_rp_ea.size() << "\n";
          outputFile_rp_ea.close();
        }
      }
    }
    */
  }
}