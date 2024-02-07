#include "nigiri/loader/dir.h"

#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/loader/loader_interface.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/routing/tripbased/transfer_set.h"
#include "nigiri/timetable.h"
#include "./paths.h"
#include "./periods.h"
#include "utl/progress_tracker.h"

using namespace nigiri;
using namespace nigiri::loader;
using namespace nigiri::routing;
using namespace nigiri::routing::tripbased;
using namespace nigiri::routing::tripbased::performance;
using namespace std::chrono;

int main() {
    auto bars = utl::global_progress_bars{false};
    auto progress_tracker = utl::activate_progress_tracker("aachen");

    // init timetable
    timetable tt;
    tt.date_range_ = aachen_test_period();
    register_special_stations(tt);
    constexpr auto const src = source_idx_t{0U};
    load_timetable(src, loader::hrd::hrd_test_avv, aachen_dir, tt);
    finalize(tt);

    /*
    // run preprocessing
    transfer_set ts;
    build_transfer_set(tt, ts);
     */
}