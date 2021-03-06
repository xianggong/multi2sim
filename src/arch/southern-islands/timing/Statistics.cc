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

#include "Statistics.h"
#include <lib/cpp/String.h>

namespace SI {

void CycleStats::setCycle(uint64_t cycle, enum CycleEvent event) {
  switch (event) {
    case EVENT_MAPPED:
      if (cycle_mapped_ == 0) cycle_mapped_ = cycle;
      break;
    case EVENT_UNMAPPED:
      if (cycle_unmapped_ == 0) cycle_unmapped_ = cycle;
      break;
    case EVENT_START:
      if (cycle_start_ == 0)
        cycle_start_ = cycle;
      else
        cycle_start_ = std::min(cycle_start_, cycle);
      break;
    case EVENT_FINISH:
      if (cycle_finish_ == 0)
        cycle_finish_ = cycle;
      else
        cycle_finish_ = std::max(cycle_finish_, cycle);
      break;
    default:
      break;
  }
}

void CycleStats::Dump(std::ostream& os) const {
  os << misc::fmt(
      "%ld,%ld,%ld,%ld,%ld,%ld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
      "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
      "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
      cycle_unmapped_ - cycle_mapped_, cycle_mapped_, cycle_unmapped_,
      cycle_finish_ - cycle_start_, cycle_start_, cycle_finish_,
      num_stall_issue_, num_stall_decode_, num_stall_read_,
      num_stall_execution_, num_stall_write_, brch_num_stall_issue_,
      brch_num_stall_decode_, brch_num_stall_read_, brch_num_stall_execution_,
      brch_num_stall_write_, lds_num_stall_issue_, lds_num_stall_decode_,
      lds_num_stall_read_, lds_num_stall_execution_, lds_num_stall_write_,
      sclr_num_stall_issue_, sclr_num_stall_decode_, sclr_num_stall_read_,
      sclr_num_stall_execution_, sclr_num_stall_write_, vmem_num_stall_issue_,
      vmem_num_stall_decode_, vmem_num_stall_read_, vmem_num_stall_execution_,
      vmem_num_stall_write_, simd_num_stall_issue_, simd_num_stall_decode_,
      simd_num_stall_read_, simd_num_stall_execution_, simd_num_stall_write_);
}
}  // namespace SI
