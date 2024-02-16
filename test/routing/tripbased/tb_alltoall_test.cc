#include <random>

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


TEST(all_to_all_queries, one_to_all) {
    auto bars = utl::global_progress_bars{false};
    auto progress_tracker = utl::activate_progress_tracker("aachen test");

    timetable tt;
    tt.date_range_ = test_full_period();
    register_special_stations(tt);
    constexpr auto const src = source_idx_t{0U};
    load_timetable(src, loader::hrd::hrd_test_avv, loader::fs_dir{"/home/jona/uni/thesis/data/input/aachen"}, tt);
    finalize(tt);

    //auto const results = tripbased_onetoall(tt, "0001573",
    auto const results = tripbased_onetoall(tt, "0001008",
                                            interval{unixtime_t{sys_days{June / 12 / 2023}} + 0h,
                                                     unixtime_t{sys_days{June / 12 / 2023}} + 24h});

    int count = 0;
    for(const auto& element : results) {
        count += element.size();
    }
    TBDL << "Number of trips in total: " << count << "\n";

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


/*
TEST(aachen_test, one_to_all) {
    constexpr auto const src = source_idx_t{0U};
    timetable tt;
    tt.date_range_ = full_period();
    register_special_stations(tt);
    load_timetable(src, loader::hrd::hrd_5_20_26, loader::fs_dir{"/home/jc/uni/thesis/data/input/aachen"}, tt);
    finalize(tt);

    auto const results = tripbased_onetoall(tt, "0001008",
                                            interval{unixtime_t{sys_days{July / 30 / 2023}} + 7h,
                                                     unixtime_t{sys_days{July / 30 / 2023}} + 10h});

    EXPECT_EQ(1, 1);
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
                                unixtime_t{sys_days{March / 30 / 2020}} + 7h});

  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }
  TBDL << "Number of trips in total: " << count << "\n";

  EXPECT_EQ(1,1);
}
 */