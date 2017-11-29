/*
 *  Multi2Sim
 *  Copyright (C) 2015  Rafael Ubal (ubal@ece.neu.edu)
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

#ifndef ARCH_SOUTHERN_ISLANDS_TIMING_COMPUTEUNITSTATISTICS_H_
#define ARCH_SOUTHERN_ISLANDS_TIMING_COMPUTEUNITSTATISTICS_H_

#include <iostream>

namespace SI {
class ComputeUnitStats {
 public:
  long long end_cycle_ = 0;

  // Instruction counters
  long long num_total_insts_ = 0;
  long long num_branch_insts_ = 0;
  long long num_scalar_memory_insts_ = 0;
  long long num_scalar_alu_insts_ = 0;
  long long num_simd_insts_ = 0;
  long long num_vector_memory_insts_ = 0;
  long long num_lds_insts_ = 0;

  // Register counters
  long long num_sreg_reads_ = 0;
  long long num_sreg_writes_ = 0;
  long long num_vreg_reads_ = 0;
  long long num_vreg_writes_ = 0;
  long long num_mapped_work_groups_ = 0;

 public:
  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os,
                                  const ComputeUnitStats& info) {
    info.Dump(os);
    return os;
  }
};

}  // namespace SI

#endif
