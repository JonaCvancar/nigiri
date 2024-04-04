#pragma once

#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"
#include "onetoall_reached.h"

namespace nigiri::routing::tripbased {

    using queue_idx_t = std::uint32_t;

    struct onetoall_q_n {
        onetoall_q_n() = delete;
#ifdef TB_ONETOALL_BITFIELD_IDX
        explicit onetoall_q_n(onetoall_reached& r,
                              timetable& tt
                              ) : r_(r),
                                  tt_(tt) {}
#else
        explicit onetoall_q_n(onetoall_reached& r) : r_(r) {}
#endif

        void reset(day_idx_t new_base);

#ifdef TB_ONETOALL_BITFIELD_IDX
        bitfield_idx_t get_or_create_bfi(bitfield const& bf);

        void set_bitfield_to_bitfield_idx(hash_map<bitfield, bitfield_idx_t>& bitfield_to_bitfield_idx) {
          bitfield_to_bitfield_idx_= &bitfield_to_bitfield_idx;
        }
#endif

        bool enqueue(std::uint16_t const transport_day,
                     transport_idx_t const,
                     std::uint16_t const stop_idx,
                     std::uint16_t const n_transfers,
                     std::uint32_t const transferred_from,
                     bitfield const operating_days,
                     std::vector<std::string_view> trip_names);

        auto& operator[](queue_idx_t pos) { return segments_[pos]; }

        void erase(unsigned int start_idx, unsigned int end_idx) { segments_.erase(segments_.begin() + start_idx, segments_.begin() + end_idx); }

        auto size() const { return segments_.size(); }

        void print(std::ostream&, queue_idx_t const);

        onetoall_reached& r_;

#ifdef TB_ONETOALL_BITFIELD_IDX
        timetable& tt_;
        hash_map<bitfield, bitfield_idx_t>* bitfield_to_bitfield_idx_;
#endif

        std::optional<day_idx_t> base_ = std::nullopt;
        std::vector<queue_idx_t> start_;
        std::vector<queue_idx_t> end_;
        std::vector<onetoall_transport_segment> segments_;
    };

}  // namespace nigiri::routing::tripbased
