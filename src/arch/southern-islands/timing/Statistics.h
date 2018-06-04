/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SRC_ARCH_SOUTHERN_ISLANDS_TIMING_STATISTICS_H_
#define SRC_ARCH_SOUTHERN_ISLANDS_TIMING_STATISTICS_H_

#include <iostream>
#include <string>

namespace SI {

enum CycleEvent { EVENT_MAPPED = 0, EVENT_UNMAPPED, EVENT_START, EVENT_FINISH };

/// This class contains cycle statistics for the timing simulation
class CycleStats {
  uint64_t cycle_mapped_ = 0;
  uint64_t cycle_unmapped_ = 0;
  uint64_t cycle_start_ = 0;
  uint64_t cycle_finish_ = 0;

 public:
  long long num_stall_issue_ = 0;
  long long num_stall_decode_ = 0;
  long long num_stall_read_ = 0;
  long long num_stall_execution_ = 0;
  long long num_stall_write_ = 0;

  long long brch_num_stall_issue_ = 0;
  long long brch_num_stall_decode_ = 0;
  long long brch_num_stall_read_ = 0;
  long long brch_num_stall_execution_ = 0;
  long long brch_num_stall_write_ = 0;

  long long lds_num_stall_issue_ = 0;
  long long lds_num_stall_decode_ = 0;
  long long lds_num_stall_read_ = 0;
  long long lds_num_stall_execution_ = 0;
  long long lds_num_stall_write_ = 0;

  long long sclr_num_stall_issue_ = 0;
  long long sclr_num_stall_decode_ = 0;
  long long sclr_num_stall_read_ = 0;
  long long sclr_num_stall_execution_ = 0;
  long long sclr_num_stall_write_ = 0;

  long long vmem_num_stall_issue_ = 0;
  long long vmem_num_stall_decode_ = 0;
  long long vmem_num_stall_read_ = 0;
  long long vmem_num_stall_execution_ = 0;
  long long vmem_num_stall_write_ = 0;

  long long simd_num_stall_issue_ = 0;
  long long simd_num_stall_decode_ = 0;
  long long simd_num_stall_read_ = 0;
  long long simd_num_stall_execution_ = 0;
  long long simd_num_stall_write_ = 0;

 public:
  /// Setters
  void setCycle(uint64_t cycle, enum CycleEvent event);

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os, const CycleStats& info) {
    info.Dump(os);
    return os;
  }
};

}  // namespace SI

#endif  // SRC_ARCH_SOUTHERN_ISLANDS_TIMING_STATISTICS_H_
