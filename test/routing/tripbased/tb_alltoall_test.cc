#include <random>
#include <fstream>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/tripbased/query/query_engine.h"

#include "../../loader/hrd/hrd_timetable.h"

#include "./test_data.h"

#include "nigiri/routing/tripbased/AllToAll/onetoall_engine.h"

#include "tb_alltoall_test.h"

#include "utl/progress_tracker.h"

#include "nigiri/routing/tripbased/dbg.h"

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
TEST(all_to_all_queries, one_to_all_fullday) {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("aachen test");

  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  constexpr auto const src = source_idx_t{0U};
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  for(location_idx_t i = location_idx_t{0}; i < tt.n_locations(); i++) {
    // auto const results = tripbased_onetoall(tt, "0001573",

    auto const results = tripbased_onetoall(
        tt, ts, i,
        interval{unixtime_t{sys_days{September / 1 / 2023}} + 0h,
                 unixtime_t{sys_days{September / 1 / 2023}} + 23h});

    int count = 0;
    int max_transfers = 0;
    for (const auto& element : results) {
      count += element.size();
      for(const auto& journey : element) {
        if(journey.transfers_ > max_transfers) {
          max_transfers = journey.transfers_;
        }
      }
    }
    TBDL << "Location(" << i << ":" << tt.n_locations() << "): " << location{tt, i} <<  " Number of trips in total: " << count <<
        " Max Transfers: " << max_transfers << " Results.size: " << sizeof(results) / 1e6 << "MB\n";
  }

  rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  auto const peak_memory_usage = static_cast<double>(r_usage.ru_maxrss) / 1e6;

  TBDL << "Peak memory usage: " << peak_memory_usage << "GB\n";

#ifndef NDEBUG
  int i = 0;
    count = 0;
    for(const auto& element : results) {
      if(count >= 10){
          break;
      }
      if(element.size()) {
        TBDL << location_name(tt, location_idx_t{i}) << " (" << i << ") results: " << element.size() << "\n";
        for (auto& j : element) {
          j.print(std::cout, tt);
        }
        count++;
      }
      i++;
    }
#endif

  EXPECT_EQ(1, 1);
}
*/


TEST(aachen_test, one_to_all_3_hours) {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("aachen test");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  auto const results = tripbased_onetoall(tt, "0001029",
  //auto const results = tripbased_onetoall(tt, "0001573",
  //auto const results = tripbased_onetoall(tt, "0001008",
                                          interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                   unixtime_t{sys_days{July / 30 / 2023}} + 8h});

  EXPECT_EQ(1, 1);

  int count = 0;
  int idx = 0;
  int max_transfers_total = 0;
  std::vector<int> transfer_ns;
  std::string outputDir = "/home/jona/uni/thesis/results/";
  for(const auto& element : results) {
    if(element.size() == 0) {
      idx++;
      continue;
    }
    int max_transfers = 0;

    for(const auto& journey : element) {

      auto it = std::find(transfer_ns.begin(), transfer_ns.end(), journey.transfers_);
      if(it == transfer_ns.end()) {
        transfer_ns.emplace_back(journey.transfers_);
      }
    }

    std::string filename = outputDir + "output_" + std::string(tt.locations_.names_[location_idx_t{idx}].view()) + "(" + std::to_string(idx) + ").txt";
    std::ofstream outputFile(filename);
    if (!outputFile.is_open()) {
      std::cerr << "Error: Unable to open file " << filename << " for writing." << std::endl;
      idx++;
      continue;
    }
    outputFile << tt.locations_.names_[location_idx_t{idx}].view() << " (" << idx << ") with " << element.size() << " results.\n";
    count += element.size();

    std::vector<bitfield> bitfields;

    for(const auto& journey : element) {
      if(std::find(bitfields.begin(), bitfields.end(), journey.bitfield_) == bitfields.end()) {
        bitfields.emplace_back(journey.bitfield_);
        outputFile << "Operating Days: "; // << journey.bitfield_ << "\n";
        for(unsigned long i = 0; i < journey.bitfield_.num_blocks; i++) {
          outputFile << journey.bitfield_.blocks_[i] << " ";
        }
        outputFile << "\n";
        for(const auto& journey_temp : element) {
          if(journey.bitfield_ == journey_temp.bitfield_) {
            journey_temp.print(outputFile, tt);
          }
        }

      } else {
        continue;
      }
      if(journey.transfers_ > max_transfers) {
        max_transfers = journey.transfers_;
      }
      if(journey.transfers_ > max_transfers_total) {
        max_transfers_total = journey.transfers_;
      }
    }

    outputFile << "\n\n";
    for(const auto& bits : bitfields) {
      outputFile << bits << "\n";
    }

    outputFile.close();
    idx++;
  }
  TBDL << "Number of trips in total: " << count << " Max Transfers: " << max_transfers_total << "\n";
}


/*
TEST(aachen_test, one_to_all_6_hours) {
  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  auto const results = tripbased_onetoall(tt, "0001008",
                                          interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                   unixtime_t{sys_days{July / 30 / 2023}} + 13h});

  EXPECT_EQ(1, 1);

  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }
  TBDL << "Number of trips in total: " << count << "\n";
}

TEST(aachen_test, one_to_all_9_hours) {
  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  auto const results = tripbased_onetoall(tt, "0001008",
                                          interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                   unixtime_t{sys_days{July / 30 / 2023}} + 16h});

  EXPECT_EQ(1, 1);

  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }
  TBDL << "Number of trips in total: " << count << "\n";
}

TEST(aachen_test, one_to_all_12_hours) {
  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  auto const results = tripbased_onetoall(tt, "0001008",
                                          interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                   unixtime_t{sys_days{July / 30 / 2023}} + 19h});

  EXPECT_EQ(1, 1);

  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }
  TBDL << "Number of trips in total: " << count << "\n";
}
*/

/*
TEST(profile_query, files_abc) {
  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_5_20_26, files_abc(), tt);
  finalize(tt);

  auto const results =
      tripbased_onetoall(tt, "0000001",
                       interval{unixtime_t{sys_days{March / 30 / 2020}} + 5h,
                                unixtime_t{sys_days{March / 30 / 2020}} + 10h});

  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }
  TBDL << "Number of trips in total: " << count << "\n";

  int i = 0;
  count = 0;
  for(const auto& element : results) {
    if (count >= 10) {
      break;
    }
    if (element.size()) {
      TBDL << location_name(tt, location_idx_t{i}) << " (" << i
           << ") results: " << element.size() << "\n";
      for (auto& j : element) {
        j.print(std::cout, tt);
      }
      count++;
    }
    i++;
  }

  EXPECT_EQ(1,1);
}
 */
