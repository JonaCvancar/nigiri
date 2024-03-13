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

TEST(oa_correctness, files_abc) {
  int begin_h = 0;
  int end_h = 24;
  int n_tests = 10;
  int n_days = 100;


  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
                 //loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen_reduced"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  auto const results = tripbased_onetoall_query(
      tt, ts,
      // tt.locations_.location_id_to_idx_.at({"0001029", src}), // Aachen Hbf
      //tt.locations_.location_id_to_idx_.at({"0001029", src}), // Elisenbrunnen
      tt.locations_.location_id_to_idx_.at({"0001227", src}), // Kirschbäumchen
      interval{unixtime_t{sys_days{June / 11 / 2023}} + hours{begin_h},
               unixtime_t{sys_days{June / 11 / 2023}} + hours{end_h}});

  int count = 0;
  int max_transfers = 0;
  for (const auto& element : results) {
    count += element.size();
    for (const auto& journey : element) {
      if (journey.transfers_ > max_transfers) {
        max_transfers = journey.transfers_;
      }
    }
  }
  TBDL << "Number of trips in total: " << count
       << " Max Transfers: " << max_transfers << "\n";

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = static_cast<int>(tt.n_locations());

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(begin_h, end_h);

  std::string outputDir = "/home/jona/uni/thesis/results_test/";

  for (int i = 0; i < n_tests; i++) {
    TBDL << "Test " << i << "\n";
    unsigned long end_location = static_cast<unsigned long>(dis(gen));
    while(results[end_location].size() == 0) {
      end_location = static_cast<unsigned long>(dis(gen));
    }

    // end_location = tt.locations_.location_id_to_idx_.at({"0001573", src}).v_;

    int h1 = dis2(gen);
    int h2 = dis2(gen);
    if (h1 == h2) {
      while (h1 == h2) {
        h2 = dis2(gen);
      }
    }

    if (h1 > h2) {
      int temp = h1;
      h1 = h2;
      h2 = temp;
    }

    std::string filename = outputDir + std::string(tt.locations_.names_[location_idx_t{end_location}].view()) + ".txt";
    std::ofstream outputFile(filename);

    for(int n_day = 0; n_day < n_days; n_day++) {
      outputFile << "Day " << n_day << "\n";

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location], results_oa,
          interval{unixtime_t{sys_days{September / 1 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{September / 1 / 2023}} + days{n_day} + hours{h2}});

      location_idx_t start =
          //tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
          //tt.locations_.location_id_to_idx_.at({"0001029", src}); // Elisenbrunnen
          tt.locations_.location_id_to_idx_.at({"0001227", src}); // Kirschbäumchen

      location_idx_t end = location_idx_t{end_location};
      outputFile << "From: " << location_name(tt, start)
           << " to: " << location_name(tt, end) << "\n";

      pareto_set<journey> results_tb = tripbased_search_correctness(
          tt, ts, start, end,
          interval{unixtime_t{sys_days{September / 1 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{September / 1 / 2023}} + days{n_day} +hours{h2}});

      outputFile << "OA Trips:(" << results_oa.size() << ")\n";
      for (auto& journey : results_oa) {
        journey.print(outputFile, tt);
      }

      outputFile << "TB Trips:(" << results_tb.size() << ")\n";
      for (auto& journey : results_tb) {
        journey.print(outputFile, tt);
        bool isInResults = false;
        for (auto& journey2 : results_oa) {
          if (tt.day_idx_mam(journey.start_time_).second.count() ==
              tt.day_idx_mam(journey2.start_time_).second.count() &&
              tt.day_idx_mam(journey.dest_time_).second.count() ==
              tt.day_idx_mam(journey2.dest_time_).second.count() &&
              journey.transfers_ == journey2.transfers_) {
            isInResults = true;
          }
        }

        EXPECT_EQ(results_oa.size(), results_tb.size());

        if (!isInResults) {
          bool inResultsWithDiffDays = false;
          for (auto& journey2 : results[end_location]) {
            if (tt.day_idx_mam(journey.start_time_).second.count() ==
                tt.day_idx_mam(journey2.start_time_).second.count() &&
                tt.day_idx_mam(journey.dest_time_).second.count() ==
                tt.day_idx_mam(journey2.dest_time_).second.count() &&
                journey.transfers_ == journey2.transfers_) {
              outputFile << "Journey is in onetoall results with different operating day.\n";
              inResultsWithDiffDays = true;
              break;
            }
          }
          if (!inResultsWithDiffDays) {
            outputFile << "Trip not at all in results.\n";
          }
        }
      }
    }
    outputFile.close();

    /*TBDL << "OA Size=" << results_oa.size()
         << ", TB Size=" << results_tb.size() << "\n";
         */
    // EXPECT_EQ(results_tb.size(), results_oa.size());
  }
}