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

/*
TEST(oa_correctness, less_trips) {
  std::vector<std::string_view> stop_list = {
      "0001779",
      "0001045",
      "0001541",
      "0005645",
      "0001149",
      "0002953"
  };

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
      // loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  location_idx_t start =
      tt.locations_.location_id_to_idx_.at({"0001573", src}); // Kirschbäumchen

  auto const start_timer = steady_clock::now();
  auto const results = tripbased_onetoall_query(
      tt, ts, start,
      interval{
          unixtime_t{sys_days{June / 11 / 2023}} + hours{0},
          unixtime_t{sys_days{June / 11 / 2023}} + hours{23} + minutes{59}});
  auto const stop_timer = steady_clock::now();
  TBDL << "AllToAll calculation took: "
       << hh_mm_ss_str(stop_timer - start_timer) <<  "\n";

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

  int n_days = 365;

  std::string outputDir = "/home/jona/uni/thesis/";

  int equal = 0;
  int larger = 0;
  int less = 0;

  int journey_oa_count = 0;
  int journey_tb_count = 0;
  int journey_rp_count = 0;

  for(unsigned long i = 0; i < stop_list.size(); i++) {
    location_idx_t end_location = tt.locations_.location_id_to_idx_.at({stop_list[i], src});

    std::string filename =
        outputDir + std::string(location_name(tt, end_location)) + "_" +
        std::string(tt.locations_.names_[end_location].view()) + ".txt";
    std::ofstream outputFile(filename);

    if (!outputFile.is_open()) {
      return;
    }

    for (auto& journey : results[end_location.v_]) {
      journey.print(outputFile, tt);
      outputFile << journey.bitfield_ << "\n";
    }

    for (int n_day = 0; n_day < n_days; n_day++) {
      outputFile << "Day " << n_day << " (" << 0 << ":" << 24 << ")\n";

      unixtime_t start_time = unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{4};
      unixtime_t end_time = unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{5} + minutes{30};

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location.v_], results_oa,
          interval{start_time, end_time});

      location_idx_t end = location_idx_t{end_location};
      outputFile << "From: " << location_name(tt, start)
                 << " to: " << location_name(tt, end) << "_"
                 << std::string(tt.locations_.names_[end_location].view())
                 << "\n";

      pareto_set<routing::journey> results_tb = tripbased_search_correctness(
          tt, ts, start, end,
          interval{
              start_time,end_time});

      pareto_set<routing::journey> results_rp = raptor_search(
          tt, nullptr, start, end,
          interval{start_time,end_time});

      outputFile << "OA Trips:(" << results_oa.size() << ")\n";
      for (auto& journey : results_oa) {
        journey.print(outputFile, tt);
      }
      journey_oa_count += results_oa.size();

      outputFile << "TB Trips:(" << results_tb.size() << ")\n";
      for (auto& journey : results_tb) {
        journey.print(outputFile, tt);
      }
      journey_tb_count += results_tb.size();


      outputFile << "RP Trips:(" << results_rp.size() << ")\n";
      for (auto& journey : results_rp) {
        journey.print(outputFile, tt);
      }
      journey_rp_count += results_rp.size();


      if (results_oa.size() == results_rp.size() &&
          results_oa.size() == results_tb.size()) {
        equal++;
      } else if (results_oa.size() > results_rp.size() ||
                 results_oa.size() > results_tb.size()) {
        larger++;
      } else {
        less++;
      }

      int equal_length = 0;
      if(results_oa.size() > results_tb.size()) {
        equal_length = 1;
      } else if(results_oa.size() < results_tb.size()) {
        equal_length = -1;
      }
      outputFile << "OA Size=" << results_oa.size()
                 << ", TB Size=" << results_tb.size()
                 << ", RP Size=" << results_rp.size()
                 << " All equal length=" << equal_length << "\n";
      outputFile << "\n";
    }
    outputFile.close();
  }

  TBDL << "equal OA than other results: " << equal
       << ", larger: " << larger
       << ", less: " << less << "\n";
  TBDL << "AMount of journeys: OA=" << journey_oa_count << ", TB=" << journey_tb_count << ", RP=" << journey_rp_count << "\n";
}
*/

/*
TEST(oa_correctness, save_all_results) {
  int begin_h = 0;
  int end_h = 23;
  unixtime_t begin_d = unixtime_t{sys_days{June / 11 / 2023}};
  unixtime_t end_d = unixtime_t{sys_days{June / 11 / 2023}};

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
      // loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  location_idx_t start_loc_idx =
      //tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
      //tt.locations_.location_id_to_idx_.at({"0001029", src}); // Elisenbrunnen
      //tt.locations_.location_id_to_idx_.at({"0001227", src}); // Kirschbäumchen
      tt.locations_.location_id_to_idx_.at({"0001573", src}); // Lemiers
      //tt.locations_.location_id_to_idx_.at({"0006033", src}); // Vettweiß Gladbach Mersheim

  unixtime_t start_time = begin_d + hours{0};
  unixtime_t end_time = end_d + hours{23} + minutes{59};

  auto const start_timer = steady_clock::now();
  auto const results = tripbased_onetoall_query(
      tt, ts, start_loc_idx, interval{start_time, end_time});
  auto const stop_timer = steady_clock::now();
  TBDL << "AllToAll calculation took: " << hh_mm_ss_str(stop_timer - start_timer) << "\n";

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

  int n_days = 365;
  int n_tests = 100;

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = 5438;

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(begin_h, end_h);

  std::string outputDir = "/home/jona/uni/thesis/";

  int equal = 0;
  int larger = 0;
  int less = 0;

  int journey_oa_count = 0;
  int journey_tb_count = 0;
  int journey_rp_count = 0;



  int i = 0;
  for (; i < n_tests; i++) {
    TBDL << "Test " << i << "\n";
    // outputFile << "Test " << i << "\n";

    std::size_t max_diff = 0;

    unsigned long end_location = static_cast<unsigned long>(dis(gen));
    while (results[end_location].size() == 0) {
      end_location = static_cast<unsigned long>(dis(gen));
    }

    if(end_location == start_loc_idx.v_) {
      while (end_location == start_loc_idx.v_) {
        end_location = static_cast<unsigned long>(dis(gen));
      }
    }
    location_idx_t end_loc_idx = location_idx_t{end_location};

    std::string filename =
        outputDir + std::to_string(end_loc_idx.v_) + ".txt";
    std::ofstream outputFile(filename);

    if (!outputFile.is_open()) {
      continue;
    }


    TBDL << "From: " << location_name(tt, start_loc_idx)
         << " to: " << location_name(tt, end_loc_idx) << "_"
         << end_loc_idx.v_ << "\n";
    outputFile << "From: " << location_name(tt, start_loc_idx)
               << " to: " << location_name(tt, end_loc_idx) << "_"
               << end_loc_idx.v_ << "\n";

    for(auto& journey : results[end_location]) {
      journey.print(outputFile, tt);
      outputFile << journey.bitfield_ << "\n";
    }

    bool check = true;
    for (int n_day = 0; n_day < n_days; n_day++) {
      check = true;
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

      h1 = begin_h;
      h2 = end_h;

      outputFile << "Day " << n_day << " (" << h1 << ":" << h2 << ")\n";

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location], results_oa,
          interval{
              begin_d + days{n_day} + hours{h1},
              end_d + days{n_day} + hours{h2}});

      pareto_set<routing::journey> results_tb = tripbased_search_correctness(
          tt, ts, start_loc_idx, end_loc_idx,
          interval{
              begin_d + days{n_day} + hours{h1},
              end_d + days{n_day} + hours{h2}});

      pareto_set<routing::journey> results_rp =
          raptor_search(tt, nullptr, start_loc_idx, end_loc_idx,
                        interval{begin_d + days{n_day} + hours{h1},
                                 end_d + days{n_day} + hours{h2}});

      if(results_tb.size() > 0) {
        for (auto& journey : results_tb) {
          bool temp_check = false;
          for (auto& journey2 : results_oa) {
            //journey.print(std::cout, tt);
            //journey2.print(std::cout, tt);
            if (journey.transfers_ == journey2.transfers_ &&
                tt.day_idx_mam(journey.start_time_).second.count() ==
                tt.day_idx_mam(journey2.start_time_).second.count() &&
                tt.day_idx_mam(journey.dest_time_).second.count() ==
                tt.day_idx_mam(journey2.dest_time_).second.count()) {
              temp_check = true;
              break;
            }
          }
          if (!temp_check) {
            //outputFile << "TB: ";
            //journey.print(outputFile, tt);
            check = false;
          }
        }
      }

      if(results_oa.size() > 0) {
        for (auto& journey : results_oa) {
          bool temp_check = false;
          for (auto& journey2 : results_tb) {
            //journey.print(std::cout, tt);
            //journey2.print(std::cout, tt);
            if (journey.transfers_ == journey2.transfers_ &&
                tt.day_idx_mam(journey.start_time_).second.count() ==
                tt.day_idx_mam(journey2.start_time_).second.count() &&
                tt.day_idx_mam(journey.dest_time_).second.count() ==
                tt.day_idx_mam(journey2.dest_time_).second.count()) {
              temp_check = true;
              break;
            }
          }
          if (!temp_check) {
            //outputFile << "OA: ";
            //journey.print(outputFile, tt);
            check = false;
          }
        }
      }

      if(!check) {
        outputFile << "TB:\n";
        for(auto& journey : results_tb) {
          journey.print(outputFile, tt);
        }
        if(results_tb.size() != results_rp.size()) {
          outputFile << "RP:\n";
          for(auto& journey : results_rp) {
            journey.print(outputFile, tt);
          }
        }
        outputFile << "OA:\n";
        for(auto& journey : results_oa) {
          journey.print(outputFile, tt);
        }
      }

      journey_oa_count += results_oa.size();
      journey_tb_count += results_tb.size();
      journey_rp_count += results_rp.size();

      int equal_length = 0;
      if(results_oa.size() > results_tb.size()) {
        equal_length = 1;
        larger++;
      } else if(results_oa.size() < results_tb.size()) {
        equal_length = -1;
        less++;
      } else {
        equal++;
      }

      if(equal_length == 1) {
        if( (results_oa.size() - results_tb.size()) > max_diff) {
          max_diff = results_oa.size() - results_tb.size();
        }
        //outputFile << ", Diff: " << results_oa.size() - results_tb.size();
      }
      outputFile << "OA Size=" << results_oa.size()
                 << ", TB Size=" << results_tb.size()
                 << ", RP Size=" << results_rp.size()
                 << " All equal length=" << equal_length;

      outputFile << "\n";
      outputFile << "\n";
    }

    //EXPECT_EQ(check, true);

    TBDL << "Check: " << check << "\n";
    //EXPECT_EQ(check, true);
    TBDL << "equal OA than other results: " << equal
         << ", larger: " << larger
         << ", less: " << less
         << ", max diff: " << max_diff << "\n";
    TBDL << "Amount of journeys: OA=" << journey_oa_count << ", TB=" << journey_tb_count << ", RP=" << journey_rp_count << "\n";

    outputFile.close();
  }
  TBDL << "Tests done: " << i << "\n";
}
*/

/*
TEST(oa_correctness, save_all_results2) {
  int begin_h = 6;
  int end_h = 18;

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
      //loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen_reduced"}, tt);
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
  std::cout << "Number of trips in total: " << count
       << " Max Transfers: " << max_transfers << "\n";

  int n_tests = 1;
  int n_days = 30;

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = 5438;

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(begin_h, end_h);

  std::string outputDir = "/home/jona/uni/thesis/results_test/";

  int equal = 0;
  int larger = 0;
  int less = 0;

  int diff_days = 0;
  int not_at_all = 0;
  int journey_oa_count = 0;
  int journey_tb_count = 0;
  int journey_rp_count = 0;
  for (int i = 0; i < n_tests; i++) {
    unsigned long end_location = static_cast<unsigned long>(dis(gen));
    while(results[end_location].size() == 0) {
      end_location = static_cast<unsigned long>(dis(gen));
    }

    end_location = tt.locations_.location_id_to_idx_.at({"0001061", src}).v_;

    std::cout << "Test " << i << " " << std::string(tt.locations_.names_[location_idx_t{end_location}].view()) << "\n";
    // end_location = tt.locations_.location_id_to_idx_.at({"0001573", src}).v_;

    std::string filename = outputDir + std::string(location_name(tt, location_idx_t{end_location})) + "_" + std::string(tt.locations_.names_[location_idx_t{end_location}].view()) + ".txt";
    std::ofstream outputFile(filename);

    if(!outputFile.is_open()) {
      continue;
    }

    for(int n_day = 0; n_day < n_days; n_day++) {
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
      h1 = 8;
      h2 = 11;

      outputFile << "Day " << n_day << " (" << h1 << ":" << h2 << ")\n";

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location], results_oa,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h2}});

      location_idx_t start =
          //tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
          //tt.locations_.location_id_to_idx_.at({"0001029", src}); // Elisenbrunnen
          tt.locations_.location_id_to_idx_.at({"0001227", src}); // Kirschbäumchen

      location_idx_t end = location_idx_t{end_location};

      pareto_set<routing::journey> results_tb = tripbased_search_correctness(
          tt, ts, start, end,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} +hours{h2}});

      pareto_set<routing::journey> results_rp = raptor_search(
          tt, nullptr, start, end,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h2}});

      if(results_oa.size() == results_rp.size() && results_oa.size() == results_tb.size()) {
        continue;
      }

      outputFile << "From: " << location_name(tt, start)
                 << " to: " << location_name(tt, end) << "\n";

      outputFile << "OA Trips:(" << results_oa.size() << ")\n";
      for (auto& journey : results_oa) {
        journey_oa_count++;
        journey.print(outputFile, tt);
      }

      outputFile << "TB Trips:(" << results_tb.size() << ")\n";
      for (auto& journey : results_tb) {
        journey_tb_count++;
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

        // EXPECT_EQ(results_oa.size(), results_tb.size());

        if (!isInResults) {
          bool inResultsWithDiffDays = false;
          for (auto& journey2 : results[end_location]) {
            if (tt.day_idx_mam(journey.start_time_).second.count() ==
                tt.day_idx_mam(journey2.start_time_).second.count() &&
                tt.day_idx_mam(journey.dest_time_).second.count() ==
                tt.day_idx_mam(journey2.dest_time_).second.count() &&
                journey.transfers_ == journey2.transfers_) {
              outputFile << "Journey is in onetoall results with different operating day.\n";
              diff_days++;
              inResultsWithDiffDays = true;
              break;
            }
          }
          if (!inResultsWithDiffDays) {
            not_at_all++;
            outputFile << "Trip not at all in results.\n";
          }
        }
      }

      outputFile << "RP Trips:(" << results_rp.size() << ")\n";
      for (auto& journey : results_rp) {
        journey_rp_count++;
        journey.print(outputFile, tt);
      }

      if(results_oa.size() == results_rp.size() && results_oa.size() == results_tb.size()) {
        equal++;
      } else if(results_oa.size() > results_rp.size() || results_oa.size() > results_tb.size()) {
        larger++;
      } else {
        less++;
      }
      int equal_length = 0;
      if(results_oa.size() > results_tb.size()) {
        equal_length = 1;
      } else if (results_oa.size() < results_tb.size()) {
        equal_length = -1;
      }

      outputFile << "OA Size=" << results_oa.size()
                 << ", TB Size=" << results_tb.size() << ", RP Size=" << results_rp.size() << " All equal length=" << equal_length << "\n";
      outputFile << "\n";
    }
    outputFile.close();

    // EXPECT_EQ(results_tb.size(), results_oa.size());
  }

  std::cout << "equal OA than other results: " << equal << ", larger: " << larger << ", less: " << less << "\n";
  std::cout << "Trips with diff days: " << diff_days << ", not at all: " << not_at_all << "\n";
  std::cout << "AMount of journeys: OA=" << journey_oa_count << ", TB=" << journey_tb_count << ", RP=" << journey_rp_count << "\n";

  close_stream();
  close_oa_stream();
}
 */

/*
TEST(oa_correctness, check_differences) {
  int begin_h = 6;
  int end_h = 12;

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

  int correct_j = 0;
  int only_oa_j = 0;
  int only_tb_j = 0;
  int deviation_j = 0;

  int n_tests = 150;
  int n_days = 150;

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = 5438;

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(begin_h, end_h);

  std::string outputDir = "/home/jona/uni/thesis/results2/";

  std::string log_filename = outputDir + "log.txt";
  std::ofstream logFile(log_filename);

  for (int i = 0; i < n_tests; i++) {
    unsigned long end_location = static_cast<unsigned long>(dis(gen));
    while(results[end_location].size() == 0) {
      end_location = static_cast<unsigned long>(dis(gen));
    }

    TBDL << "Test " << i << " " << std::string(tt.locations_.names_[location_idx_t{end_location}].view()) << "\n";
    // end_location = tt.locations_.location_id_to_idx_.at({"0001573", src}).v_;

    std::string filename = outputDir + std::string(tt.locations_.names_[location_idx_t{end_location}].view()) + ".txt";
    std::ofstream outputFile(filename);

    for(int n_day = 0; n_day < n_days; n_day++) {
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

      outputFile << "Day " << n_day << " (" << h1 << ":" << h2 << ")\n";
      logFile << "Day " << n_day << " (" << h1 << ":" << h2 << ")\n";

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location], results_oa,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h2}});

      location_idx_t start =
          //tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
          //tt.locations_.location_id_to_idx_.at({"0001029", src}); // Elisenbrunnen
          tt.locations_.location_id_to_idx_.at({"0001227", src}); // Kirschbäumchen

      location_idx_t end = location_idx_t{end_location};
      outputFile << "From: " << location_name(tt, start)
                 << " to: " << location_name(tt, end) << "\n";
      logFile << "From: " << location_name(tt, start)
              << " to: " << location_name(tt, end) << "\n";

      pareto_set<journey> results_tb = tripbased_search_correctness(
          tt, ts, start, end,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} +hours{h2}});

      pareto_set<journey> results_rp = raptor_search(
          tt, nullptr, start, end,
          interval{unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h1},
                   unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{h2}});

      outputFile << "OA Size: " << results_oa.size() << " TB Size: " << results_tb.size() << " RP Size: " << results_rp.size() << "\n";
      logFile << "OA Size: " << results_oa.size() << " TB Size: " << results_tb.size() << " RP Size: " << results_rp.size() << "\n";

      if(results_oa.size() == results_tb.size()) {
        for(auto it = results_oa.begin(), it2 = results_tb.begin();
            it != results_oa.end() && it2 != results_tb.end();
            ++it, ++it2) {
          if (tt.day_idx_mam(it->start_time_).second.count() ==
              tt.day_idx_mam(it2->start_time_).second.count() &&
              tt.day_idx_mam(it->dest_time_).second.count() ==
              tt.day_idx_mam(it2->dest_time_).second.count() &&
              it->transfers_ == it2->transfers_) {
            correct_j++;
            journey temp_j = *it;
            temp_j.print(outputFile, tt);
            continue;
          } else {
            deviation_j++;
            outputFile << "Mismatch:\nOA: ";
            logFile << "Mismatch:\nOA: ";
            journey temp_j = *it;
            temp_j.print(outputFile, tt);
            temp_j.print(logFile, tt);
            outputFile << "TB: ";
            journey temp_j2 = *it2;
            temp_j2.print(outputFile, tt);
            temp_j2.print(logFile, tt);
          }
        }
      } else {
        for(auto it = results_oa.begin(); it != results_oa.end(); ++it) {
          bool j_found = false;
          for (auto it2 = results_tb.begin(); it2 != results_tb.end(); ++it2) {
            if (tt.day_idx_mam(it->start_time_).second.count() ==
                tt.day_idx_mam(it2->start_time_).second.count() &&
                tt.day_idx_mam(it->dest_time_).second.count() ==
                tt.day_idx_mam(it2->dest_time_).second.count() &&
                it->transfers_ == it2->transfers_) {
              correct_j++;
              journey temp_j = *it;
              temp_j.print(outputFile, tt);
              j_found = true;
              break;
            }
          }
          if(!j_found) {
            only_oa_j++;
            outputFile << "Journey only in OA: ";
            logFile << "Journey only in OA: ";
            journey temp_j = *it;
            temp_j.print(outputFile, tt);
            temp_j.print(logFile, tt);
          }
        }
        for(auto it = results_tb.begin(); it != results_tb.end(); ++it) {
          bool j_found = false;
          for(auto it2 = results_oa.begin(); it2 != results_oa.end(); ++it2) {
            if (tt.day_idx_mam(it->start_time_).second.count() ==
                tt.day_idx_mam(it2->start_time_).second.count() &&
                tt.day_idx_mam(it->dest_time_).second.count() ==
                tt.day_idx_mam(it2->dest_time_).second.count() &&
                it->transfers_ == it2->transfers_) {
              correct_j++;
              j_found = true;
              break;
            }
          }
          if(!j_found) {
            only_tb_j++;
            outputFile << "Journey only in TB: ";
            logFile << "Journey only in TB: ";
            journey temp_j = *it;
            temp_j.print(outputFile, tt);
            temp_j.print(logFile, tt);
          }
        }
      }
      outputFile << "\n";
    }
    outputFile.close();

    TBDL << "OA Size=" << results_oa.size()
         << ", TB Size=" << results_tb.size() << "\n";

    // EXPECT_EQ(results_tb.size(), results_oa.size());
  }

  logFile.close();
  TBDL << "Correct journeys: " << correct_j << ", Only OA: " << only_oa_j << ", Only TB: " << only_tb_j << ", deviating: " << deviation_j << "\n";
}
*/

/*
TEST(oa_correctness, tb_earliest_arrival) {
  int begin_h = 0;
  int end_h = 23;
  unixtime_t begin_d = unixtime_t{sys_days{June / 11 / 2023}};
  unixtime_t end_d = unixtime_t{sys_days{June / 11 / 2023}};

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
      // loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  location_idx_t start_loc_idx =
      //tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
      //tt.locations_.location_id_to_idx_.at({"0001029", src}); // Elisenbrunnen
      //tt.locations_.location_id_to_idx_.at({"0001227", src}); // Kirschbäumchen
      tt.locations_.location_id_to_idx_.at({"0001573", src}); // Lemiers
  //tt.locations_.location_id_to_idx_.at({"0006033", src}); // Vettweiß Gladbach Mersheim

  unixtime_t start_time = begin_d + hours{0};
  unixtime_t end_time = end_d + hours{23} + minutes{59};

  int n_days = 365;
  int n_tests = 100;

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = 5438;

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(begin_h, end_h);

  std::cout << "Start onetoall\n";
  auto const results = tripbased_onetoall_query(
      tt, ts, start_loc_idx, interval{start_time, end_time});
  std::cout << "End onetoall\n";

  int i = 0;
  for (; i < n_tests; i++) {
    TBDL << "Test " << i << "\n";

    std::size_t max_diff = 0;

    unsigned long end_location = static_cast<unsigned long>(dis(gen));
    location_idx_t end_loc_idx = location_idx_t{end_location};

    TBDL << "From: " << location_name(tt, start_loc_idx)
         << " to: " << location_name(tt, end_loc_idx) << "_"
         << end_loc_idx.v_ << "\n";

    for (int n_day = 0; n_day < n_days; n_day++) {
      int h1 = dis2(gen);

      pareto_set<routing::journey> results_oa;
      tripbased_onetoall_correctness(
          tt, results[end_location], results_oa,
          {begin_d + days{n_day} + hours{h1}});

      pareto_set<routing::journey> results_tb = tripbased_search_correctness(
          tt, ts, start_loc_idx, end_loc_idx,
          {begin_d + days{n_day} + hours{h1}});

      pareto_set<routing::journey> results_rp = raptor_search(
          tt, nullptr, start_loc_idx, end_loc_idx,
          {begin_d + days{n_day} + hours{h1}});

      if(results_oa.size() != results_tb.size() && h1 < 22) {
        std::cout << "Day " << n_day << " (" << h1 << ")\n";
        if (results_oa.size()) {
          std::cout << "OA:\n";
          for (auto& journey : results_oa) {
            journey.print(std::cout, tt);
          }
        }

        if (results_tb.size()) {
          std::cout << "TB:\n";
          for (auto& journey : results_tb) {
            journey.print(std::cout, tt);
          }
        }

        if (results_rp.size()) {
          std::cout << "RP:\n";
          for (auto& journey : results_rp) {
            journey.print(std::cout, tt);
          }
        }
      }
    }
  }
}
*/

TEST(oa_correctness, test_bitfield_idx) {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  location_idx_t start =
      tt.locations_.location_id_to_idx_.at({"0001008", src}); // Aachen Hbf
      //tt.locations_.location_id_to_idx_.at({"0001573", src}); // Kirschbäumchen

  TBDL << "From: " << location_name(tt, start)
       << "\n";

  auto const start_timer = steady_clock::now();
  auto const results = tripbased_onetoall_query(
      tt, ts, start,
      interval{
          unixtime_t{sys_days{June / 11 / 2023}} + hours{0},
          unixtime_t{sys_days{June / 11 / 2023}} + hours{23} + minutes{59}});
  auto const stop_timer = steady_clock::now();
  TBDL << "AllToAll calculation took: "
       << hh_mm_ss_str(stop_timer - start_timer) <<  "\n";

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
}

/*
TEST(oa_correctness, zero_trips) {
  std::vector<std::string_view> stop_list = {
      "0123483",
      "0003222",
      "0004331",
      "0123166",
      "0278269",
      "0123433",
      "2655801",
      "2654931",
      "0009249",
      "0123069",
      "0005436",
      "0272131",
      "0278646",
      "2655815",
      "0003244",
      "0272966",
      "0123214",
      "0123395",
      "0123733",
      "2655811",
      "0005247",
      "0123186",
      "0123357",
      "0123843",
      "0123365",
      "2658593",
      "0123521",
      "0282498",
      "0277131",
      "0275152",
      "0002712",
      "0123453",
      "0009116",
      "0275361",
      "0275137",
      "0275185",
      "0286344",
      "2650775",
      "0123847",
      "0004670",
      "0275253",
      "0278933",
      "0274233",
      "0123287"
  };

  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv,
                 loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  auto const locations = get_locations(("/home/jona/uni/thesis/data/input/aachen/bahnhof"));

  for(unsigned long i = 0; i < stop_list.size(); i++) {
    location_idx_t start =
        tt.locations_.location_id_to_idx_.at({stop_list[i], src}); // Kirschbäumchen

    TBDL << "From: " << location_name(tt, start)
         << "\n";

    auto const start_timer = steady_clock::now();
    auto const results = tripbased_onetoall_query(
        tt, ts, start,
        interval{
            unixtime_t{sys_days{June / 11 / 2023}} + hours{0},
            unixtime_t{sys_days{June / 11 / 2023}} + hours{23} + minutes{59}});
    auto const stop_timer = steady_clock::now();
    TBDL << "AllToAll calculation took: "
         << hh_mm_ss_str(stop_timer - start_timer) <<  "\n";

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

    for(unsigned long test = 0; test < locations.size(); test++) {
      if(test%250 == 0 && test > 0) {
        TBDL << test << "\n";
      }
      location_idx_t end = tt.locations_.location_id_to_idx_.at({locations[test], src});
      for(int day = 0; day < test_full_period().size().count(); day++) {
        pareto_set<routing::journey> results_tb = tripbased_search_correctness(
            tt, ts, start, end,
            interval{
                unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{0},
                unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{23} + minutes{59}});

        if (results_tb.size()) {
          TBDL << "TB\n";
        }

        pareto_set<routing::journey> results_rp = raptor_search(
            tt, nullptr, start, end,
            interval{
                unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{0},
                unixtime_t{sys_days{June / 11 / 2023}} + days{day} + hours{23} + minutes{59}});

        if (results_rp.size()) {
          TBDL << "RP\n";
        }
      }
    }
  }

  int n_tests = 100;
  int n_days = 365;

  std::string outputDir = "/home/jona/uni/thesis/";

  int equal = 0;
  int larger = 0;
  int less = 0;

  int journey_oa_count = 0;
  int journey_tb_count = 0;
  int journey_rp_count = 0;

  location_idx_t end_location = tt.locations_.location_id_to_idx_.at({stop_list[i], src});

  std::string filename =
      outputDir + std::string(location_name(tt, end_location)) + "_" +
      std::string(tt.locations_.names_[end_location].view()) + ".txt";
  std::ofstream outputFile(filename);

  if (!outputFile.is_open()) {
    return;
  }

  for (auto& journey : results[end_location.v_]) {
    journey.print(outputFile, tt);
    outputFile << journey.bitfield_ << "\n";
  }

  for (int n_day = 0; n_day < n_days; n_day++) {
    outputFile << "Day " << n_day << " (" << 0 << ":" << 24 << ")\n";

    unixtime_t start_time = unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{4};
    unixtime_t end_time = unixtime_t{sys_days{June / 11 / 2023}} + days{n_day} + hours{5} + minutes{30};

    pareto_set<routing::journey> results_oa;
    tripbased_onetoall_correctness(
        tt, results[end_location.v_], results_oa,
        interval{start_time, end_time});

    location_idx_t end = location_idx_t{end_location};
    outputFile << "From: " << location_name(tt, start)
               << " to: " << location_name(tt, end) << "_"
               << std::string(tt.locations_.names_[end_location].view())
               << "\n";

    pareto_set<routing::journey> results_tb = tripbased_search_correctness(
        tt, ts, start, end,
        interval{
            start_time,end_time});

    pareto_set<routing::journey> results_rp = raptor_search(
        tt, nullptr, start, end,
        interval{start_time,end_time});

    outputFile << "OA Trips:(" << results_oa.size() << ")\n";
    for (auto& journey : results_oa) {
      journey.print(outputFile, tt);
    }
    journey_oa_count += results_oa.size();

    outputFile << "TB Trips:(" << results_tb.size() << ")\n";
    for (auto& journey : results_tb) {
      journey.print(outputFile, tt);
    }
    journey_tb_count += results_tb.size();


    outputFile << "RP Trips:(" << results_rp.size() << ")\n";
    for (auto& journey : results_rp) {
      journey.print(outputFile, tt);
    }
    journey_rp_count += results_rp.size();


    if (results_oa.size() == results_rp.size() &&
        results_oa.size() == results_tb.size()) {
      equal++;
    } else if (results_oa.size() > results_rp.size() ||
               results_oa.size() > results_tb.size()) {
      larger++;
    } else {
      less++;
    }

    int equal_length = 0;
    if(results_oa.size() > results_tb.size()) {
      equal_length = 1;
    } else if(results_oa.size() < results_tb.size()) {
      equal_length = -1;
    }
    outputFile << "OA Size=" << results_oa.size()
               << ", TB Size=" << results_tb.size()
               << ", RP Size=" << results_rp.size()
               << " All equal length=" << equal_length << "\n";
    outputFile << "\n";
  }
  outputFile.close();

  TBDL << "equal OA than other results: " << equal
       << ", larger: " << larger
       << ", less: " << less << "\n";
  TBDL << "AMount of journeys: OA=" << journey_oa_count << ", TB=" << journey_tb_count << ", RP=" << journey_rp_count << "\n";
}
 */