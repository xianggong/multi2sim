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

#include "ComputeUnitStatistics.h"
#include <lib/cpp/String.h>

namespace SI {

void ComputeUnitStats::Dump(std::ostream& os) const {
  os << misc::fmt("WorkGroupCount = %lld\n", num_mapped_work_groups_);
  os << misc::fmt("Instructions = %lld\n", num_total_insts_);
  os << misc::fmt("ScalarALUInstructions = %lld\n", num_scalar_alu_insts_);
  os << misc::fmt("ScalarMemInstructions = %lld\n", num_scalar_memory_insts_);
  os << misc::fmt("BranchInstructions = %lld\n", num_branch_insts_);
  os << misc::fmt("SIMDInstructions = %lld\n", num_simd_insts_);
  os << misc::fmt("VectorMemInstructions = %lld\n", num_vector_memory_insts_);
  os << misc::fmt("LDSInstructions = %lld\n", num_lds_insts_);
  os << misc::fmt("ScalarRegReads= %lld\n", num_sreg_reads_);
  os << misc::fmt("ScalarRegWrites= %lld\n", num_sreg_writes_);
  os << misc::fmt("VectorRegReads= %lld\n", num_vreg_reads_);
  os << misc::fmt("VectorRegWrites= %lld\n", num_vreg_writes_);
}

}  // namespace SI