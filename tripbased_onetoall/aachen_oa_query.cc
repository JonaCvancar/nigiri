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

  auto const locations = routing::tripbased::onetoall::get_locations((aachen_dir_oa.path().string() + "/bahnhof"));
  std::cout << "location length: " << locations.size() << "\n";

  std::random_device rd;
  std::mt19937 gen(rd());

  unsigned long location_count = locations.size();
  std::uniform_int_distribution<> dis_start(0, static_cast<int>(location_count));

  std::uniform_int_distribution<> dis_end(0, static_cast<int>(location_count));

  int days_count = aachen_period_oa().size().count();
  std::uniform_int_distribution<> dis_days(0, static_cast<int>(days_count));

  bool profile = false;
  if(profile) {
    std::vector<int> spans = {2, 12, 24};

    for (int i = 0; i < 500; i++) {
      location_idx_t start_loc_idx = location_idx_t{dis_start(gen)};
      TBDL << " From: " << location_name(tt, start_loc_idx) << "_"
           << start_loc_idx.v_ << "\n";
      auto result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                             interval{start_time, end_time});

      auto results = *result_stats.journeys_;

      while (result_stats.search_stats_.n_results_in_interval == 0) {
        start_loc_idx = location_idx_t{dis_start(gen)};
        TBDL << " From: " << location_name(tt, start_loc_idx) << "_"
             << start_loc_idx.v_ << "\n";
        result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                          interval{start_time, end_time});

        results = *result_stats.journeys_;
      }

      for (auto& span : spans) {
        std::string filename =
            outputDir + "profile_query_" + std::to_string(span) + ".txt";

        std::ifstream infile_temp(filename);
        infile_temp.seekg(0, std::ios::end);  // Move to the end of the file
        if (!(infile_temp.tellg() > 0)) {
          infile_temp.close();

          std::ofstream outputFile(filename, std::ios::app);

          outputFile << "from to day span from_h OA_TB TB RP OA_count TB_count RP_count\n";

          outputFile.close();
        }

        std::uniform_int_distribution<> dis_time(
            0, static_cast<int>(end_h - span + 1));
        for (int j = 0; j < 100; j++) {
          if (j > 0 & j % 50 == 0) {
            std::cout << "End_stop: " << j << "\n";
          }

          location_idx_t end_loc_idx = location_idx_t{dis_end(gen)};

          for (int k = 0; k < 20; k++) {
            int day = dis_days(gen);
            int time = dis_time(gen);

            auto start_timer = steady_clock::now();
            pareto_set<routing::journey> results_oa;
            tripbased_onetoall_query(
                tt, results[end_loc_idx.v_], results_oa,
                interval{unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time},
                         unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time + span - 1} + minutes{59}});
            auto stop_timer = steady_clock::now();
            auto oa_tb_time =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    stop_timer - start_timer)
                    .count();
            start_timer = steady_clock::now();
            pareto_set<routing::journey> results_tb = tripbased_search(
                tt, ts, start_loc_idx, end_loc_idx,
                interval{unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time},
                         unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time + span - 1} + minutes{59}});
            stop_timer = steady_clock::now();
            auto tb_time =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    stop_timer - start_timer)
                    .count();

            start_timer = steady_clock::now();
            pareto_set<routing::journey> results_rp = raptor_search(
                tt, nullptr, start_loc_idx, end_loc_idx,
                interval{unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time},
                         unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                         hours{time + span - 1} + minutes{59}});
            stop_timer = steady_clock::now();
            auto rp_time =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    stop_timer - start_timer)
                    .count();

            std::ofstream outputFile(filename, std::ios::app);
            outputFile << start_loc_idx << " " << end_loc_idx << " " << day
                       << " " << span << " " << time << " " << oa_tb_time << " "
                       << tb_time << " " << rp_time << " " << results_oa.size()
                       << " " << results_tb.size() << " " << results_rp.size()
                       << "\n";
            outputFile.close();
          }
        }
      }
    }
  } else {
    for (int i = 0; i < 500; i++) {
      location_idx_t start_loc_idx = location_idx_t{dis_start(gen)};
      TBDL << " From: " << location_name(tt, start_loc_idx) << "_"
           << start_loc_idx.v_ << "\n";
      auto result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                             interval{start_time, end_time});

      auto results = *result_stats.journeys_;

      while (result_stats.search_stats_.n_results_in_interval == 0) {
        start_loc_idx = location_idx_t{dis_start(gen)};
        TBDL << " From: " << location_name(tt, start_loc_idx) << "_"
             << start_loc_idx.v_ << "\n";
        result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                          interval{start_time, end_time});

        results = *result_stats.journeys_;
      }

      std::string filename =
          outputDir + "ea_query_.txt";

      std::ifstream infile_temp(filename);
      infile_temp.seekg(0, std::ios::end);  // Move to the end of the file
      if (!(infile_temp.tellg() > 0)) {
        infile_temp.close();

        std::ofstream outputFile(filename, std::ios::app);

        outputFile << "from to day from_mam OA_TB TB RP OA_count TB_count RP_count\n";

        outputFile.close();
      }

      std::uniform_int_distribution<> dis_time(0, 1440);
      for (int j = 0; j < 100; j++) {
        if (j > 0 & j % 50 == 0) {
          std::cout << "End_stop: " << j << "\n";
        }

        location_idx_t end_loc_idx = location_idx_t{dis_end(gen)};

        for (int k = 0; k < 20; k++) {
          int day = dis_days(gen);
          int time = dis_time(gen);

          auto start_timer = steady_clock::now();
          pareto_set<routing::journey> results_oa;
          tripbased_onetoall_query(
              tt, results[end_loc_idx.v_], results_oa,
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                  minutes{time});
          auto stop_timer = steady_clock::now();
          auto oa_tb_time =
              std::chrono::duration_cast<std::chrono::microseconds>(
                  stop_timer - start_timer)
                  .count();
          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_tb = tripbased_search(
              tt, ts, start_loc_idx, end_loc_idx,
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                       minutes{time});
          stop_timer = steady_clock::now();
          auto tb_time =
              std::chrono::duration_cast<std::chrono::microseconds>(
                  stop_timer - start_timer)
                  .count();

          start_timer = steady_clock::now();
          pareto_set<routing::journey> results_rp = raptor_search(
              tt, nullptr, start_loc_idx, end_loc_idx,
              unixtime_t{sys_days{June / 11 / 2023}} + days{day} +
                  minutes{time});
          stop_timer = steady_clock::now();
          auto rp_time =
              std::chrono::duration_cast<std::chrono::microseconds>(
                  stop_timer - start_timer)
                  .count();

          std::ofstream outputFile(filename, std::ios::app);
          outputFile << start_loc_idx << " " << end_loc_idx << " " << day
                      << " " << time << " " << oa_tb_time << " "
                     << tb_time << " " << rp_time << " " << results_oa.size()
                     << " " << results_tb.size() << " " << results_rp.size()
                     << "\n";
          outputFile.close();
        }
      }
    }
  }
}