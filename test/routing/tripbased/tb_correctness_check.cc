#include <random>
#include <fstream>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/tripbased/query/query_engine.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_engine.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_queries.h"

#include "../../loader/hrd/hrd_timetable.h"

#include "./test_data.h"

#include "utl/progress_tracker.h"

#include "nigiri/routing/tripbased/dbg.h"

#include "./tb_correctness_check.h"

#include "../../raptor_search.h"

#include <chrono>

using namespace nigiri;
using namespace nigiri::test;
using namespace nigiri::loader;
using namespace nigiri::loader::gtfs;
using namespace nigiri::routing;
using namespace nigiri::routing::tripbased;
using namespace nigiri::routing::tripbased::test;
using namespace nigiri::test_data::hrd_timetable;
using namespace std::chrono;

/*
TEST(oa_correctness, stations) {
  int begin_h = 0;
  int end_h = 24;

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(
      src, loader::hrd::hrd_test_avv,
      // loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
      loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen_reduced"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  std::string outputDir = "/home/jona/uni/thesis/";
  std::string filename = outputDir +  "log.txt";
  std::ofstream outputFile(filename);

  for(unsigned long i = 0; i < 5438; i++) {
    outputFile << std::string(location_name(tt, location_idx_t{i})) << " - " << std::string(tt.locations_.names_[location_idx_t{i}].view()) <<  "\n";
  }
  outputFile.close();
}
*/

std::string hh_mm_ss_str(std::chrono::duration<double, std::ratio<1>> time) {
  std::uint32_t uint_time = std::round(time.count());
  std::uint32_t hours = 0, minutes = 0;
  while (uint_time >= 3600) {
    uint_time -= 3600;
    ++hours;
  }
  while (uint_time >= 60) {
    uint_time -= 60;
    ++minutes;
  }
  std::stringstream ss;
  ss << std::setw(2) << std::setfill('0') << hours << ":" << std::setw(2)
     << std::setfill('0') << minutes << ":" << std::setw(2)
     << std::setfill('0') << uint_time;
  return ss.str();
}

TEST(oa_correctness, test_bitfield_idx) {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);

  auto const start_timer_load = steady_clock::now();
  load_timetable(src, loader::hrd::hrd_test_avv,
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);
  auto const stop_timer_load = steady_clock::now();
  TBDL << "Loading took: "
       << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer_load -
                                                                start_timer_load).count() <<  "\n";
  rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  std::cout << static_cast<double>(r_usage.ru_maxrss) / 1e6 << "\n";

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  std::cout << "Trip names size: " << tt.trip_display_names_.size() << "\n";

  std::cout << "Locations: " << tt.n_locations() << "\n";
  std::cout << "Lines: " << tt.trip_lines_.size() << "\n";

  location_idx_t start =
      tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
  //tt.locations_.location_id_to_idx_.at({"0001573", src}); // Kirschbäumchen

  TBDL << "From: " << location_name(tt, start)
       << "\n";

  auto const start_timer = steady_clock::now();
  auto result_stats =  tripbased_onetoall_query(
      tt, ts, start,
      interval{
          unixtime_t{sys_days{June / 11 / 2023}} + hours{0},
          unixtime_t{sys_days{June / 11 / 2023}} + hours{23} + minutes{59}});
  auto const stop_timer = steady_clock::now();
  TBDL << "AllToAll calculation took: "
       << hh_mm_ss_str(stop_timer - start_timer) <<  "\n";

  auto results = *result_stats.journeys_;

  bool test = true;
  if(test) {
    return;
  }

  std::cout << "Stats:\n";
#ifdef TB_ONETOALL_BITFIELD_IDX
  std::cout << "New bitfields: " << result_stats.search_stats_.n_new_bitfields_ << "\n";
  std::cout << "Bitfield deduplication storage savings: "
            << static_cast<double>((result_stats.search_stats_.n_results_in_interval * sizeof(bitfield)) -
                                                                                 (result_stats.search_stats_.n_new_bitfields_ * sizeof(bitfield))) / 1e9 << "GB\n";
#endif
  std::cout << "Journeys found: " << result_stats.search_stats_.n_results_in_interval << "\n";
#ifdef TB_OA_COLLECT_STATS
  std::cout << "Tmin comparisons: " << result_stats.algo_stats_.n_tmin_comparisons_ << "\n";
  std::cout << "Reached comparison: " << result_stats.algo_stats_.n_reached_comparisons_ << "\n";
  std::cout << "Results comparison: " << result_stats.algo_stats_.n_results_comparisons_ << "\n";
#endif
  std::cout << "Enqueued prevented by reached: " << result_stats.algo_stats_.n_enqueue_prevented_by_reached_ << "\n";
  std::cout << "Segments pruned: " << result_stats.algo_stats_.n_segments_pruned_ << "\n";
  std::cout << "Peak memory usage: " << result_stats.algo_stats_.peak_memory_usage_ << "\n";
  std::cout << "Max queue size: " << result_stats.algo_stats_.n_largest_queue_size << "\n";

  std::ofstream testfile("/home/jona/uni/test_compare.txt");
  int count = 0;
  int max_transfers = 0;
  for (const auto& element : results) {
    testfile << element.size() << "\n";
    count += element.size();
    for (const auto& journey : element) {
      if (journey.transfers_ > max_transfers) {
        max_transfers = journey.transfers_;
      }
    }
  }
  testfile.close();

  TBDL << "Number of trips in total: " << count
       << " Max Transfers: " << max_transfers << "\n";

  std::string outputDir = "/home/jona/uni/thesis/";

  int count_less = 0;
  int count_larger = 0;
  int count_equal = 0;
  int count_equal_diffs = 0;

  std::vector<unsigned long> idx_list{
      51,
  123,
  383,
  385,
  386,
  387,
  388,
  446,
  485};
  for(unsigned long idx = 10; idx < 300; ++idx) {
  //for(auto idx : idx_list) {
    location_idx_t location = location_idx_t{idx};
    std::string filename =
        outputDir + std::to_string(location.v_) + ".txt";
    std::ofstream outputFile(filename);

    outputFile << location.v_ << " To: " << location_name(tt, location) << "\n";
    std::cout << location.v_ << " To: " << location_name(tt, location) << "\n";

    outputFile.close();

    /*
    if(results[location.v_].size()) {
      for(auto& journey : results[location.v_]) {
        journey.print(outputFile, tt);
        outputFile << journey.bitfield_ << "\n";
      }
      outputFile << "\n";
    }
     */

    for (int day = 0; day < tt.date_range_.size().count(); day++) {
      //for (int day = 0; day < 50; day++) {
      //TBDL << "Day " << day << "\n";
      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[location.v_], results_oa,
          interval{
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{0},
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{23} +
              minutes{59}});

      pareto_set<routing::journey> results_tb = tripbased_search_correctness(
          tt, ts, start, location,
          interval{
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{0},
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{23} +
              minutes{59}});

      pareto_set<routing::journey> results_rp = raptor_search(
          tt, nullptr, start, location,
          interval{
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{0},
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{23} +
              minutes{59}});

      if(results_oa.size() == 0 && results_tb.size() == 0) {
        continue;
      }

      std::ofstream outputFile2(filename, std::ios::app);
      if(results_oa.size() != results_tb.size()) {
        if(results_oa.size() > results_tb.size()) {
          count_larger++;
        } else {
          count_less++;
        }
        outputFile2 << "Diff: Day: " << day << ", TB: " << results_tb.size() << ", OA: " << results_oa.size() << ", RP: " << results_rp.size() << "\n";

        std::vector<routing::journey> diff_oa;
        std::vector<routing::journey> diff_tb;
        for (auto& j1 : results_oa) {
          bool exists = false;
          for (auto& j2 : results_tb) {
            if (j1.transfers_ == j2.transfers_ &&
                tt.day_idx_mam(j1.start_time_).second.count() ==
                tt.day_idx_mam(j2.start_time_).second.count() &&
                tt.day_idx_mam(j1.dest_time_).second.count() ==
                tt.day_idx_mam(j2.dest_time_).second.count()) {
                exists = true;
            }
          }
          if (!exists) {
            diff_oa.emplace_back(j1);
          }
        }
        for (auto& j1 : results_tb) {
          bool exists = false;
          for (auto& j2 : results_oa) {
            if (j1.transfers_ == j2.transfers_ &&
                tt.day_idx_mam(j1.start_time_).second.count() ==
                tt.day_idx_mam(j2.start_time_).second.count() &&
                tt.day_idx_mam(j1.dest_time_).second.count() ==
                tt.day_idx_mam(j2.dest_time_).second.count()) {
              exists = true;
            }
          }
          if (!exists) {
            diff_tb.emplace_back(j1);
          }
        }
        if (diff_oa.size()) {
          outputFile2 << "OA:\n";
          for (auto& j : diff_oa) {
            j.print(outputFile2, tt);
          }
        }
        if (diff_tb.size()) {
          outputFile2 << "TB:\n";
          for (auto& j : diff_tb) {
            j.print(outputFile2, tt);
          }
        }
      } else {
        count_equal++;
        bool first = false;
        for(size_t i = 0; i < results_oa.size(); ++i) {
          if (results_oa[i].transfers_ == results_tb[i].transfers_ &&
              tt.day_idx_mam(results_oa[i].start_time_).second.count() ==
              tt.day_idx_mam(results_tb[i].start_time_).second.count() &&
              tt.day_idx_mam(results_oa[i].dest_time_).second.count() ==
              tt.day_idx_mam(results_tb[i].dest_time_).second.count()) {
            continue;
          } else {
            int check = false;
            for(auto j_oa : results_oa) {
              if(j_oa.transfers_ == results_tb[i].transfers_ &&
                 tt.day_idx_mam(j_oa.start_time_).second.count() ==
                 tt.day_idx_mam(results_tb[i].start_time_).second.count() &&
                 tt.day_idx_mam(j_oa.dest_time_).second.count() ==
                 tt.day_idx_mam(results_tb[i].dest_time_).second.count()) {
                check = true;
              }
            }
            if(!check) {
              if(!first) {
                outputFile2 << "Day: " << day << "Equal\n";
                first = true;
              }
              outputFile2 << "TB:";
              results_tb[i].print(outputFile2, tt);
            }
            check = false;
            for(auto j_tb : results_tb) {
              if(j_tb.transfers_ == results_oa[i].transfers_ &&
                 tt.day_idx_mam(j_tb.start_time_).second.count() ==
                 tt.day_idx_mam(results_oa[i].start_time_).second.count() &&
                 tt.day_idx_mam(j_tb.dest_time_).second.count() ==
                 tt.day_idx_mam(results_oa[i].dest_time_).second.count()) {
                check = true;
              }
            }
            if(!check) {
              if(!first) {
                outputFile2 << "Day: " << day << "Equal\n";
                first = true;
              }
              outputFile2 << "OA:";
              results_oa[i].print(outputFile2, tt);
            }
          }
        }
        if(first) {
          count_equal_diffs++;
        } /*else {
          outputFile2 << "Day: " << day << ", OA: " << results_oa.size() << ", TB: " << results_tb.size() << ", RP: " << results_rp.size() << "\n";
        }*/
      }
      outputFile2.close();
    }
    std::cout << "Count less: " << count_less << ", Count larger: " << count_larger << ", Count equal: " << count_equal << ", Count equal diff: " << count_equal_diffs << "\n";
  }
  std::cout << "Count less: " << count_less << ", Count larger: " << count_larger << ", Count equal: " << count_equal << ", Count equal diff: " << count_equal_diffs << "\n";
}