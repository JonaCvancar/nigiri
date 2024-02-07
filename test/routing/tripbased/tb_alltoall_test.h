#pragma once

#include "nigiri/routing/journey.h"
#include "nigiri/routing/onetoall_search.h"
#include "nigiri/timetable.h"

#include "nigiri/routing/debug.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/transfer_set.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_state.h"


namespace nigiri::test {

    std::vector<pareto_set<routing::journey>> tripbased_onetoall(timetable &tt,
                                                            std::string_view from,
                                                            routing::start_time_t time) {
        using algo_t = routing::tripbased::oneToAll_engine;
        using algo_state_t = routing::tripbased::oneToAll_state;

        static auto search_state = routing::onetoall_search_state{};
        routing::tripbased::transfer_set ts{};
        //build_transfer_set(tt, ts, 10);
        auto algo_state = algo_state_t{tt, ts};

        auto const src = source_idx_t{0};
        routing::onetoall_query q;
        q = routing::onetoall_query{
                .start_time_ = time,
                .start_ = {{tt.locations_.location_id_to_idx_.at({from, src}), 0_minutes,
                            0U}}
        };

        return *(routing::onetoall_search<direction::kForward, algo_t>{
            tt, nullptr, search_state, algo_state, std::move(q)}
            .execute().journeys_);
    }

    timetable tripbased_alltoall(timetable &tt) {
        //using algo_t = routing::tripbased::query_engine;
        using algo_state_t = routing::tripbased::oneToAll_state;

        static auto search_state = routing::onetoall_search_state{};
        routing::tripbased::transfer_set ts;
        build_transfer_set(tt, ts, 10);
        auto algo_state = algo_state_t{tt, ts};

        return timetable{};
        /*
        return *(routing::search<direction::kForward, algo_t>{
                tt, nullptr, search_state, algo_state, std::move(q)}
                .execute()
                .journeys_);
                */
    }
}