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

  std::random_device rd;
  std::mt19937 gen(rd());

  unsigned long location_count = locations.size();

  std::cout << "location length: " << locations.size() << "\n";
  std::uniform_int_distribution<> dis_start(0, static_cast<int>(location_count));

  std::uniform_int_distribution<> dis(0, static_cast<int>(location_count));

  int days_count = aachen_period_oa().size().count();
  std::uniform_int_distribution<> dis_days(0, static_cast<int>(days_count));

  while(true) {
    std::ifstream infile("/home/jona/uni/thesis/results/aachen/output_1.txt");
    std::vector<std::string> file_content;

    // Check if the file can be opened
    if (!infile.is_open()) {
      std::cerr << "Failed to open file: " << "/home/jona/uni/thesis/results/aachen/output_1.txt" << std::endl;
      return 1;
    }

    // Read each line of the file and store it in the vector
    std::string line;
    while (std::getline(infile, line)) {
      file_content.push_back(line);
    }

    // Close the file
    infile.close();

    std::string filename = outputDir + "onetoall.txt";

    std::ifstream infile_temp(filename);
    infile_temp.seekg(0, std::ios::end); // Move to the end of the file
    if ( !(infile_temp.tellg() > 0)) {
      infile_temp.close();

      std::ofstream outputFile(filename, std::ios::app);
      outputFile << "Stop"
                 << " Runtime"
                 << " journey_count"
                 << " new_bitfields"
                 << " largest_queue"
                 << " segments_enqueued"
                 << " segments_pruned"
                 << " prevent_reached"
                 << " reached_comp"
                 << " tmin_comp"
                 << " res_comp"
                 << " max_transfer"
                 << " peak_memory"
                 << " exceeds_preprocessor"
                 << " equal_journey"
                 << " new_tmin"
                 << " bitfield_idx"
                 << " collect_stats"
                 << "\n";
      outputFile.close();
    }

    if(!file_content.empty()) {
      // Remove the first element from the vector
      std::string first_element = file_content.front();
      file_content.erase(file_content.begin());

      location_idx_t start_loc_idx =
          tt.locations_.location_id_to_idx_.at({first_element, src});
      TBDL << " From: " << location_name(tt, start_loc_idx) << "_"
           << start_loc_idx.v_ << "\n";
      auto start_timer = steady_clock::now();
      auto result_stats = tripbased_onetoall(tt, ts, start_loc_idx,
                                             interval{start_time, end_time});
      auto stop_timer = steady_clock::now();

      auto results = *result_stats.journeys_;
      TBDL << "AllToAll calculation took: "
           << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer -
                                                                    start_timer)
                  .count()
           << "\n";

      std::ofstream outputFile(filename, std::ios::app);
      outputFile << first_element
                 << " " << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer - start_timer).count()
                 << " " << result_stats.search_stats_.n_results_in_interval
                 << " " <<
#ifdef TB_ONETOALL_BITFIELD_IDX
          result_stats.search_stats_.n_new_bitfields_
#else
          0
#endif
                 << " " << result_stats.algo_stats_.n_largest_queue_size
                 << " " << result_stats.algo_stats_.n_segments_enqueued_
                 << " " << result_stats.algo_stats_.n_segments_pruned_
                 << " " << result_stats.algo_stats_.n_enqueue_prevented_by_reached_
#ifdef TB_OA_COLLECT_STATS
                 << " " << result_stats.algo_stats_.n_reached_comparisons_
                 << " " << result_stats.algo_stats_.n_tmin_comparisons_
                 << " " << result_stats.algo_stats_.n_results_comparisons_
#else
                 << " " << 0
                 << " " << 0
                 << " " << 0
#endif
                 << " " << result_stats.algo_stats_.max_transfers_reached_
                 << " " << result_stats.algo_stats_.peak_memory_usage_
                 << " " << (result_stats.algo_stats_.peak_memory_usage_ > result_stats.algo_stats_.pre_memory_usage_)
                 << " " << result_stats.algo_stats_.equal_journey_
                 << " " << result_stats.algo_stats_.tb_new_tmin_
                 << " " << result_stats.algo_stats_.tb_onetoall_bitfield_idx_
                 << " " << result_stats.algo_stats_.tb_oa_collect_stats_
                 << "\n";

      outputFile.close();

      std::ofstream outfile_stops("/home/jona/uni/thesis/results/aachen/output_1.txt");

      // Check if the file can be opened
      if (!outfile_stops.is_open()) {
        std::cerr << "Failed to open file: " << "/home/jona/uni/thesis/results/aachen/output_1.txt" << std::endl;
        return 1;
      }

      // Write each element of the vector to the file
      for (const auto& content : file_content) {
        outfile_stops << content << std::endl;
      }

      // Close the file
      outfile_stops.close();
    } else {
      break;
    }
  }
}