#include <random>

#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/tripbased/query/query_engine.h"

#include "../../loader/hrd/hrd_timetable.h"

#include "./test_data.h"

#include "nigiri/routing/tripbased/AllToAll/alltoall.h"

#include "tb_alltoall_test.h"

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
    constexpr auto const src = source_idx_t{0U};
    timetable tt;
    tt.date_range_ = full_period();
    register_special_stations(tt);
    load_timetable(src, loader::hrd::hrd_5_20_26, files_abc(), tt);
    finalize(tt);

    auto const results = tripbased_onetoall(tt, "0000001",
                                          interval{unixtime_t{sys_days{March / 30 / 2020}} + 5h,
                                                   unixtime_t{sys_days{March / 30 / 2020}} + 9h});

/*#ifndef NDEBUG
    for(int i = 0; i < static_cast<int>(tt.locations_.names_.size()); i++) {
        TBDL << location_name(tt, location_idx_t{i}) << "\n";
    }
#endif*/


    //const auto results = tripbased_onetoall(tt);

    EXPECT_EQ(1, 1);
}