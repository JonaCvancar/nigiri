#include <random>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/tripbased/query/query_engine.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_engine.h"

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
  auto bars = utl::global_progress_bars{false};
  auto progress_tracker = utl::activate_progress_tracker("correctness");

  constexpr auto const src = source_idx_t{0U};
  timetable tt;
  tt.date_range_ = test_full_period();
  register_special_stations(tt);
  load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
  finalize(tt);

  routing::tripbased::transfer_set ts;
  build_transfer_set(tt, ts, 10);

  TBDL << "Begin test.\n";

  auto const results = tripbased_onetoall_correctness(tt, ts, tt.locations_.location_id_to_idx_.at({"00001008", src}),
                                          interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                   unixtime_t{sys_days{July / 30 / 2023}} + 10h});

  TBDL << "End test.\n";

  int count = 0;
  int max_transfers = 0;
  for(const auto& element : results) {
    count += element.size();
    for(const auto& journey : element) {
      if(journey.transfers_ > max_transfers) {
        max_transfers = journey.transfers_;
      }
    }
  }
  TBDL << "Number of trips in total: " << count << " Max Transfers: " << max_transfers << "\n";

  std::random_device rd;
  std::mt19937 gen(rd());

  int min_value = 0;
  int max_value = static_cast<int>(tt.n_locations());

  std::uniform_int_distribution<> dis(min_value, max_value);
  std::uniform_int_distribution<> dis2(0, 23);

  int h1 = dis2(gen);
  int h2 = dis2(gen);
  if(h1 == h2) {
    while(h1 == h2) {
      h2 = dis2(gen);
    }
  }

  std::vector<pareto_set<journey_bitfield>> results_oa;
  if(h1 > h2) {
    TBDL << "Time Interval " << h2 << ":" << h1 << "\n";
    results_oa = tripbased_onetoall_correctness(
        tt, ts, tt.locations_.location_id_to_idx_.at({"00001008", src}),
        interval{unixtime_t{sys_days{September / 1 / 2023}} + hours{h2},
                 unixtime_t{sys_days{September / 1 / 2023}} + hours{h1}});
  } else {
    TBDL << "Time Interval" << h1 << ":" << h2 << "\n";
    results_oa = tripbased_onetoall_correctness(
        tt, ts, tt.locations_.location_id_to_idx_.at({"00001008", src}),
        interval{unixtime_t{sys_days{September / 1 / 2023}} + hours{h1},
                 unixtime_t{sys_days{September / 1 / 2023}} + hours{h2}});
  }

  TBDL << "finished onetoall\n";
  for(int i = 0; i < 3; i++)
  {
    location_idx_t random_idx = location_idx_t{dis(gen)};
    TBDL << "Location idx=" << random_idx.v_ << ", time " ;
    pareto_set<journey> results_tb;
    if(h1 > h2) {
      results_tb = tripbased_search_correctness(
          tt, ts, tt.locations_.location_id_to_idx_.at({"00001008", src}), random_idx,
          interval{unixtime_t{sys_days{September / 1 / 2023}} + hours{h2},
                   unixtime_t{sys_days{September / 1 / 2023}} + hours{h1}});
    } else {
      results_tb = tripbased_search_correctness(
          tt, ts, tt.locations_.location_id_to_idx_.at({"00001008", src}), random_idx,
          interval{unixtime_t{sys_days{September / 1 / 2023}} + hours{h1},
                   unixtime_t{sys_days{September / 1 / 2023}} + hours{h2}});
    }

    TBDL << "OA Size=" << results_oa[random_idx.v_].size() << ", TB Size=" << results_tb.size() << "\n";
    EXPECT_EQ(results_tb.size(), results_oa[random_idx.v_].size());
  }
}