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

#include "Statistics.h"
#include <arch/southern-islands/emulator/Emulator.h>
#include <lib/cpp/String.h>

namespace SI {

// Singleton instance
std::unique_ptr<Statistics> Statistics::instance_;

Statistics::Statistics() {}

Statistics* Statistics::getInstance() {
  // Instance already exists
  if (instance_.get()) return instance_.get();

  // Create instance
  instance_ = misc::new_unique<Statistics>();
  return instance_.get();
}

NDRangeInfo::NDRangeInfo(std::string kernel_name) : kernel_name_(kernel_name) {}

void NDRangeInfo::setCycle(long long cycle, enum NDRangeEventType event_type) {
  switch (event_type) {
    case NDRANGE_MAPPED:
      cycle_mapped_ = cycle;
      break;
    case NDRANGE_UNMAPPED:
      cycle_unmapped_ = cycle;
      break;
    case UOP_START:
      if (cycle < cycle_uop_created_ || cycle_uop_created_ == 0) {
        cycle_uop_created_ = cycle;
      }
      break;
    case UOP_FINISH:
      if (cycle > cycle_uop_destroy_) {
        cycle_uop_destroy_ = cycle;
      }
      break;
    default:
      break;
  }
}

void NDRangeInfo::Dump(std::ostream& os) const {
  os << misc::fmt("%8lld[%8lld %8lld] %8lld[%8lld %8lld]\n",
                  cycle_unmapped_ - cycle_mapped_, cycle_mapped_,
                  cycle_unmapped_, cycle_uop_destroy_ - cycle_uop_created_,
                  cycle_uop_created_, cycle_uop_destroy_);
}

void ComputeUnitInfo::Sampling(ComputeUnit* cu, long long cycle) {
  if (cu_id_ == cu->getIndex()) {
    // Record sampling
    struct ComputeUnitStats stats;
    stats.end_cycle_ = cycle;

    // Instruction counters
    stats.num_total_instructions_ =
        cu->num_total_instructions - previous_stats.num_total_instructions_;
    stats.num_branch_instructions_ =
        cu->num_branch_instructions - previous_stats.num_branch_instructions_;
    stats.num_scalar_memory_instructions_ =
        cu->num_scalar_memory_instructions -
        previous_stats.num_scalar_memory_instructions_;
    stats.num_scalar_alu_instructions_ =
        cu->num_scalar_alu_instructions -
        previous_stats.num_scalar_alu_instructions_;
    stats.num_simd_instructions_ =
        cu->num_simd_instructions - previous_stats.num_simd_instructions_;
    stats.num_vector_memory_instructions_ =
        cu->num_vector_memory_instructions -
        previous_stats.num_vector_memory_instructions_;
    stats.num_lds_instructions_ =
        cu->num_lds_instructions - previous_stats.num_lds_instructions_;

    // Register counters
    stats.num_sreg_reads_ = cu->num_sreg_reads - previous_stats.num_sreg_reads_;
    stats.num_sreg_writes_ =
        cu->num_sreg_writes - previous_stats.num_sreg_writes_;
    stats.num_vreg_reads_ = cu->num_vreg_reads - previous_stats.num_vreg_reads_;
    stats.num_vreg_writes_ =
        cu->num_vreg_writes - previous_stats.num_vreg_writes_;
    stats.num_mapped_work_groups_ =
        cu->num_mapped_work_groups - previous_stats.num_mapped_work_groups_;

    // Cycle counters
    stats.sum_cycle_branch_instructions_ =
        cu->sum_cycle_branch_instructions -
        previous_stats.sum_cycle_branch_instructions_;
    stats.sum_cycle_scalar_memory_instructions_ =
        cu->sum_cycle_scalar_memory_instructions -
        previous_stats.sum_cycle_scalar_memory_instructions_;
    stats.sum_cycle_scalar_alu_instructions_ =
        cu->sum_cycle_scalar_alu_instructions -
        previous_stats.sum_cycle_scalar_alu_instructions_;
    stats.sum_cycle_simd_instructions_ =
        cu->sum_cycle_simd_instructions -
        previous_stats.sum_cycle_simd_instructions_;
    stats.sum_cycle_vector_memory_instructions_ =
        cu->sum_cycle_vector_memory_instructions -
        previous_stats.sum_cycle_vector_memory_instructions_;
    stats.sum_cycle_lds_instructions_ =
        cu->sum_cycle_lds_instructions -
        previous_stats.sum_cycle_lds_instructions_;

    // stats.min_cycle_branch_instructions_ =
    //     cu->min_cycle_branch_instructions -
    //     previous_stats.min_cycle_branch_instructions_;
    // stats.min_cycle_scalar_memory_instructions_ =
    //     cu->min_cycle_scalar_memory_instructions -
    //     previous_stats.min_cycle_scalar_memory_instructions_;
    // stats.min_cycle_scalar_alu_instructions_ =
    //     cu->min_cycle_scalar_alu_instructions -
    //     previous_stats.min_cycle_scalar_alu_instructions_;
    // stats.min_cycle_simd_instructions_ =
    //     cu->min_cycle_simd_instructions -
    //     previous_stats.min_cycle_simd_instructions_;
    // stats.min_cycle_vector_memory_instructions_ =
    //     cu->min_cycle_vector_memory_instructions -
    //     previous_stats.min_cycle_vector_memory_instructions_;
    // stats.min_cycle_lds_instructions_ =
    //     cu->min_cycle_lds_instructions -
    //     previous_stats.min_cycle_lds_instructions_;
    // stats.max_cycle_branch_instructions_ =
    //     cu->max_cycle_branch_instructions -
    //     previous_stats.max_cycle_branch_instructions_;
    // stats.max_cycle_scalar_memory_instructions_ =
    //     cu->max_cycle_scalar_memory_instructions -
    //     previous_stats.max_cycle_scalar_memory_instructions_;
    // stats.max_cycle_scalar_alu_instructions_ =
    //     cu->max_cycle_scalar_alu_instructions -
    //     previous_stats.max_cycle_scalar_alu_instructions_;
    // stats.max_cycle_simd_instructions_ =
    //     cu->max_cycle_simd_instructions -
    //     previous_stats.max_cycle_simd_instructions_;
    // stats.max_cycle_vector_memory_instructions_ =
    //     cu->max_cycle_vector_memory_instructions -
    //     previous_stats.max_cycle_vector_memory_instructions_;
    // stats.max_cycle_lds_instructions_ =
    //     cu->max_cycle_lds_instructions -
    //     previous_stats.max_cycle_lds_instructions_;

    stats_table.push_back(stats);

    // Save current stats as previous stats
    previous_stats.num_total_instructions_ = cu->num_total_instructions;
    previous_stats.num_branch_instructions_ = cu->num_branch_instructions;
    previous_stats.num_scalar_memory_instructions_ =
        cu->num_scalar_memory_instructions;
    previous_stats.num_scalar_alu_instructions_ =
        cu->num_scalar_alu_instructions;
    previous_stats.num_simd_instructions_ = cu->num_simd_instructions;
    previous_stats.num_vector_memory_instructions_ =
        cu->num_vector_memory_instructions;
    previous_stats.num_lds_instructions_ = cu->num_lds_instructions;
    previous_stats.num_sreg_reads_ = cu->num_sreg_reads;
    previous_stats.num_sreg_writes_ = cu->num_sreg_writes;
    previous_stats.num_vreg_reads_ = cu->num_vreg_reads;
    previous_stats.num_vreg_writes_ = cu->num_vreg_writes;
    previous_stats.num_mapped_work_groups_ = cu->num_mapped_work_groups;
    previous_stats.sum_cycle_branch_instructions_ =
        cu->sum_cycle_branch_instructions;
    previous_stats.sum_cycle_scalar_memory_instructions_ =
        cu->sum_cycle_scalar_memory_instructions;
    previous_stats.sum_cycle_scalar_alu_instructions_ =
        cu->sum_cycle_scalar_alu_instructions;
    previous_stats.sum_cycle_simd_instructions_ =
        cu->sum_cycle_simd_instructions;
    previous_stats.sum_cycle_vector_memory_instructions_ =
        cu->sum_cycle_vector_memory_instructions;
    previous_stats.sum_cycle_lds_instructions_ = cu->sum_cycle_lds_instructions;
    // previous_stats.min_cycle_branch_instructions_ =
    //     cu->min_cycle_branch_instructions;
    // previous_stats.min_cycle_scalar_memory_instructions_ =
    //     cu->min_cycle_scalar_memory_instructions;
    // previous_stats.min_cycle_scalar_alu_instructions_ =
    //     cu->min_cycle_scalar_alu_instructions;
    // previous_stats.min_cycle_simd_instructions_ =
    //     cu->min_cycle_simd_instructions;
    // previous_stats.min_cycle_vector_memory_instructions_ =
    //     cu->min_cycle_vector_memory_instructions;
    // previous_stats.min_cycle_lds_instructions_ =
    // cu->min_cycle_lds_instructions;
    // previous_stats.max_cycle_branch_instructions_ =
    //     cu->max_cycle_branch_instructions;
    // previous_stats.max_cycle_scalar_memory_instructions_ =
    //     cu->max_cycle_scalar_memory_instructions;
    // previous_stats.max_cycle_scalar_alu_instructions_ =
    //     cu->max_cycle_scalar_alu_instructions;
    // previous_stats.max_cycle_simd_instructions_ =
    //     cu->max_cycle_simd_instructions;
    // previous_stats.max_cycle_vector_memory_instructions_ =
    //     cu->max_cycle_vector_memory_instructions;
    // previous_stats.max_cycle_lds_instructions_ =
    // cu->max_cycle_lds_instructions;
  }
}

void ComputeUnitInfo::Dump(std::ostream& os) const {
  for (unsigned i = 0; i < stats_table.size(); ++i) {
    long long st_cycle = 0;
    if (i != 0) {
      st_cycle = stats_table[i - 1].end_cycle_;
    }
    long long fn_cycle = stats_table[i].end_cycle_;
    os << misc::fmt(
        "%8lld Insts in |%16lld %16lld|:\t%8lld BRCH[%5lld] | %8lld "
        "LDS [%5lld] | %8lld SALU[%5lld] | %8lld "
        "SMEM[%5lld] | %8lld VALU[%5lld] | %8lld "
        "VMEM[%5lld]\n",
        stats_table[i].num_total_instructions_, st_cycle, fn_cycle,
        stats_table[i].num_branch_instructions_,
        stats_table[i].num_branch_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_branch_instructions_ /
                  stats_table[i].num_branch_instructions_,
        stats_table[i].num_lds_instructions_,
        stats_table[i].num_lds_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_lds_instructions_ /
                  stats_table[i].num_lds_instructions_,
        stats_table[i].num_scalar_alu_instructions_,
        stats_table[i].num_scalar_alu_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_scalar_alu_instructions_ /
                  stats_table[i].num_scalar_alu_instructions_,
        stats_table[i].num_scalar_memory_instructions_,
        stats_table[i].num_scalar_memory_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_scalar_memory_instructions_ /
                  stats_table[i].num_scalar_memory_instructions_,
        stats_table[i].num_simd_instructions_,
        stats_table[i].num_simd_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_simd_instructions_ /
                  stats_table[i].num_simd_instructions_,
        stats_table[i].num_vector_memory_instructions_,
        stats_table[i].num_vector_memory_instructions_ == 0
            ? 0
            : stats_table[i].sum_cycle_vector_memory_instructions_ /
                  stats_table[i].num_vector_memory_instructions_);
  }
}

void Statistics::setNDRangeCycle(unsigned int ndrange_id, long long cycle,
                                 enum NDRangeEventType event_type) {
  if (ndrange_info_.count(ndrange_id) != 1) {
    auto emulator = Emulator::getInstance();
    auto ndrange = emulator->getNDRangeById(ndrange_id);
    auto kernel_name = ndrange->getKernelName();
    ndrange_info_[ndrange_id] = NDRangeInfo(kernel_name);
  } else {
    auto& info = ndrange_info_.at(ndrange_id);
    info.setCycle(cycle, event_type);
    // std::cout << ndrange_id << " " << event_type << " : " << cycle << "\n";
  }
}

void Statistics::Dump(std::ostream& os) const {
  for (auto& item : ndrange_info_) {
    os << item.second.getKernelName() << " - NDRange[" << item.first << "] : ";
    item.second.Dump(os);
  }

  for (auto& item : cu_info_) {
    os << "CU[" << item.second.getComputeUnitId() << "]:\n";
    item.second.Dump(os);
  }
}

void Statistics::Sampling(SI::ComputeUnit* cu, long long cycle) {
  auto cu_id = cu->getIndex();
  if (cu_info_.count(cu_id) != 1) {
    cu_info_[cu_id] = ComputeUnitInfo(cu_id);
  }
  auto& info = cu_info_.at(cu_id);
  info.Sampling(cu, cycle);
}
}  // namespace SI
