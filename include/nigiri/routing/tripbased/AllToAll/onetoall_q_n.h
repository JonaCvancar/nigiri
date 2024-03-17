#pragma once

#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"
#include "onetoall_reached.h"

namespace nigiri::routing::tripbased {

    using queue_idx_t = std::uint32_t;

    struct onetoall_q_n {
        onetoall_q_n() = delete;
        explicit onetoall_q_n(onetoall_reached& r) : r_(r) {}

        void reset(day_idx_t new_base);

        bool enqueue(std::uint16_t const transport_day,
                     transport_idx_t const,
                     std::uint16_t const stop_idx,
                     std::uint16_t const n_transfers,
                     std::uint32_t const transferred_from,
                     bitfield const operating_days);

        auto& operator[](queue_idx_t pos) { return segments_[pos]; }

        void erase(unsigned int start_idx, unsigned int end_idx) { segments_.erase(segments_.begin() + start_idx, segments_.begin() + end_idx); }

        auto size() const { return segments_.size(); }

        void print(std::ostream&, queue_idx_t const);

        onetoall_reached& r_;
        std::optional<day_idx_t> base_ = std::nullopt;
        std::vector<queue_idx_t> start_;
        std::vector<queue_idx_t> end_;
        std::vector<onetoall_transport_segment> segments_;
    };

}  // namespace nigiri::routing::tripbased
