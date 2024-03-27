#include "nigiri/loader/dir.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/routing/tripbased/transfer_set.h"
#include "nigiri/timetable.h"
#include "./paths_oa.h"
#include "./periods_oa.h"
#include "utl/progress_tracker.h"

#include "./test_queries.h"
#include <chrono>

using namespace nigiri;
using namespace nigiri::loader;
using namespace nigiri::loader::gtfs;
using namespace nigiri::routing;
using namespace nigiri::routing::tripbased;
using namespace nigiri::routing::tripbased::onetoall;
using namespace std::chrono;

int main() {
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("vbb");

  // init timetable
  timetable tt;
  tt.date_range_ = vbb_period_oa();
  register_special_stations(tt);
  constexpr auto const src = source_idx_t{0U};
  load_timetable(loader_config{0, "Europe/Berlin"}, src, vbb_dir_oa, tt);
  finalize(tt);

  //auto const locations = routing::tripbased::onetoall::get_locations((vbb_dir_oa.path().string() + "/" + "stops.csv"), ',');

  // run preprocessing
  transfer_set ts;
  build_transfer_set(tt, ts);

  /*
  for (const auto& pair : tt.locations_.location_id_to_idx_) {
    std::cout << pair.first << std::endl;
  }

  unixtime_t begin_d = unixtime_t{sys_days{June / 11 / 2023}};
  unixtime_t end_d = unixtime_t{sys_days{June / 11 / 2023}};

  unixtime_t start_time = begin_d + hours{0};
  unixtime_t end_time = end_d + hours{23} + minutes{59};

  std::string outputDir = "/home/jona/uni/thesis/results/berlin/";

  std::string filename =
      outputDir + "running_time" + ".txt";
  std::ofstream outputFile(filename, std::ios::app);

  if (!outputFile.is_open()) {
    return 0;
  }

  for(auto const& location : locations) {
    std::ofstream outputFile(filename, std::ios::app);

    if (!outputFile.is_open()) {
      return 0;
    }

    location_idx_t start_loc_idx =
        tt.locations_.location_id_to_idx_.at({location, src});
    TBDL << "From: " << location_name(tt, start_loc_idx) << "_" << start_loc_idx.v_ << "\n";
    auto const start_timer = steady_clock::now();
    auto const results = tripbased_onetoall_query(
        tt, ts, start_loc_idx, interval{start_time, end_time});
    auto const stop_timer = steady_clock::now();

    int count = 0;
    for (const auto& element : results) {
      count += element.size();
    }
    TBDL << "Number of trips in total: " << count << "\n";

    TBDL << "AllToAll calculation took: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer - start_timer).count() << "\n";
    outputFile << std::chrono::duration_cast<std::chrono::milliseconds>(stop_timer - start_timer).count() << " " << count << "\n";

    outputFile.close();
  }
   */
}