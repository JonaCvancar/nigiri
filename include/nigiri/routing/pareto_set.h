#pragma once

#include <cinttypes>
#include <tuple>
#include <vector>
#include <iostream>
#include "nigiri/types.h"

namespace nigiri {

template <typename T>
struct pareto_set {
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  std::size_t size() const { return els_.size(); }

  std::tuple<bool, iterator, iterator> add(T&& el) {
    auto n_removed = std::size_t{0};
    for (auto i = 0U; i < els_.size(); ++i) {
      if (els_[i].dominates(el)) {
        return {false, end(), std::next(begin(), i)};
      }
      if (el.dominates(els_[i])) {
        n_removed++;
        continue;
      }
      els_[i - n_removed] = els_[i];
    }
    els_.resize(els_.size() - n_removed + 1);
    els_.back() = std::move(el);
    return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
            end()};
  }

  std::tuple<bool, iterator, iterator> add_bitfield(T&& el) {
    auto n_removed = std::size_t{0};
#if defined(EQUAL_JOURNEY)
    bool equal = false;
    unsigned long equal_idx = 0U;
#endif
    for (auto i = 0U; i < els_.size(); ++i) {
#if defined(EQUAL_JOURNEY)
      if(els_[i].equal(el)) {
        els_[i - n_removed] = els_[i];
        equal = true;
        equal_idx = i - n_removed;
        continue;
        // return {true, std::next(begin(), i), end()};
      }
#endif

      if( !(els_[i].bitfield_ & el.bitfield_).any() ) {
        els_[i - n_removed] = els_[i];
        continue;
      }

      if (els_[i].dominates(el)) {
        if ((el.bitfield_ & ~els_[i].bitfield_).any()) {
          el.set_bitfield(el.bitfield_ & ~els_[i].bitfield_);
        } else {
          return {false, end(), std::next(begin(), i)};
        }
      }

      if (el.dominates(els_[i])) {
        if( (els_[i].bitfield_ & ~el.bitfield_).any()) {
          els_[i].set_bitfield(els_[i].bitfield_ & ~el.bitfield_);
        } else {
          n_removed++;
          continue;
        }
      }
      els_[i - n_removed] = els_[i];
    }
#if defined(EQUAL_JOURNEY)
    if(equal) {
      if(els_[equal_idx].equal(el)) {
        els_[equal_idx].set_bitfield(els_[equal_idx].bitfield_ | el.bitfield_);
      } else {
        std::cout << "Equal index wrong.\n";
      }
      els_.resize(els_.size() - n_removed);
      return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
              end()};
    }
#endif

    els_.resize(els_.size() - n_removed + 1);
    if(el.bitfield_.any()) {
      els_.back() = std::move(el);
      return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
              end()};
    } else {
      return {false, end(), std::next(begin())};
    }
  }

  bool check(T&& el) {
    for (auto i = 0U; i < els_.size(); ++i) {
#if defined(EQUAL_JOURNEY)
      if(els_[i].equal(el)) {
        continue;
      }
#endif

      if( !(els_[i].bitfield_ & el.bitfield_).any() ) {
        continue;
      }

      if (els_[i].dominates(el)) {
        if ((el.bitfield_ & ~els_[i].bitfield_).any()) {
          el.set_bitfield(el.bitfield_ & ~els_[i].bitfield_);
        } else {
          return false;
        }
      }
    }

    if(el.bitfield_.any()) {
      return true;
    } else {
      return false;
    }
  }

  const T& operator[](size_t index) const {
    return els_[index];
  }

  friend const_iterator begin(pareto_set const& s) { return s.begin(); }
  friend const_iterator end(pareto_set const& s) { return s.end(); }
  friend iterator begin(pareto_set& s) { return s.begin(); }
  friend iterator end(pareto_set& s) { return s.end(); }
  iterator begin() { return els_.begin(); }
  iterator end() { return els_.end(); }
  const_iterator begin() const { return els_.begin(); }
  const_iterator end() const { return els_.end(); }
  iterator erase(iterator const& it) { return els_.erase(it); }
  iterator erase(iterator const& from, iterator const& to) {
    return els_.erase(from, to);
  }
  void clear() { els_.clear(); }

private:
  std::vector<T> els_;
};

}  // namespace nigiri
