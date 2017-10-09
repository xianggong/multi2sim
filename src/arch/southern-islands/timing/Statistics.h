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

#ifndef ARCH_SOUTHERN_ISLANDS_TIMING_STATISTICS_H
#define ARCH_SOUTHERN_ISLANDS_TIMING_STATISTICS_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ComputeUnit.h"

namespace SI {

enum NDRangeEventType {
  NDRANGE_MAPPED = 0,
  NDRANGE_UNMAPPED,
  UOP_START,
  UOP_FINISH
};

class NDRangeInfo {
  std::string kernel_name_;

  long long cycle_mapped_ = 0;
  long long cycle_unmapped_ = 0;
  long long cycle_uop_created_ = 0;
  long long cycle_uop_destroy_ = 0;

 public:
  /// Constructor
  NDRangeInfo() {}
  explicit NDRangeInfo(std::string kernel_name);

  /// Getters
  std::string getKernelName() const { return kernel_name_; }

  /// Setters
  void setCycle(long long cycle, enum NDRangeEventType event_type);

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os, const NDRangeInfo& info) {
    info.Dump(os);
    return os;
  }
};

struct ExecutionUnitStats {
  long long num_total_cycles_ = 0;
  long long num_idle_cycles_ = 0;
  long long num_active_or_stall_cycles_ = 0;
  long long num_active_only_cycles_ = 0;
  long long num_active_and_stall_cycles_ = 0;
  long long num_stall_only_cycles_ = 0;
  long long num_stall_issue_ = 0;
  long long num_stall_decode_ = 0;
  long long num_stall_read_ = 0;
  long long num_stall_execution_ = 0;
  long long num_stall_write_ = 0;
  long long num_vmem_divergence_ = 0;
  long long num_alu_mem_overlap_cycles_ = 0;
};

struct ComputeUnitStats {
  long long end_cycle_ = 0;
  // Instruction counters
  long long num_total_instructions_ = 0;
  long long num_branch_instructions_ = 0;
  long long num_scalar_memory_instructions_ = 0;
  long long num_scalar_alu_instructions_ = 0;
  long long num_simd_instructions_ = 0;
  long long num_vector_memory_instructions_ = 0;
  long long num_lds_instructions_ = 0;

  // Register counters
  long long num_sreg_reads_ = 0;
  long long num_sreg_writes_ = 0;
  long long num_vreg_reads_ = 0;
  long long num_vreg_writes_ = 0;
  long long num_mapped_work_groups_ = 0;

  // Cycle counters
  long long sum_cycle_branch_instructions_ = 0;
  long long sum_cycle_scalar_memory_instructions_ = 0;
  long long sum_cycle_scalar_alu_instructions_ = 0;
  long long sum_cycle_simd_instructions_ = 0;
  long long sum_cycle_vector_memory_instructions_ = 0;
  long long sum_cycle_lds_instructions_ = 0;

  // Utilization counters
  struct ExecutionUnitStats branch;
  struct ExecutionUnitStats scalar_memory;
  struct ExecutionUnitStats scalar_alu;
  struct ExecutionUnitStats vector_memory;
  struct ExecutionUnitStats vecotr_alu;
  struct ExecutionUnitStats lds;
};

class ComputeUnitInfo {
  int cu_id_;

  std::vector<struct ComputeUnitStats> stats_table;
  struct ComputeUnitStats previous_stats;

 public:
  /// Constructor
  ComputeUnitInfo() {}
  explicit ComputeUnitInfo(int cu_id) : cu_id_(cu_id) {}

  /// Getters
  int getComputeUnitId() const { return cu_id_; }

  /// Member function
  /// Sampling compute unit
  void Sampling(ComputeUnit* cu, long long cycle);

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os,
                                  const ComputeUnitInfo& cu_info) {
    cu_info.Dump(os);
    return os;
  }
};

class Statistics {
 private:
  // Unique instance of the singleton
  static std::unique_ptr<Statistics> instance_;

  std::map<unsigned, NDRangeInfo> ndrange_info_;

  unsigned cu_sampling_interval_ = 1000;
  std::map<unsigned, ComputeUnitInfo> cu_info_;

 public:
  Statistics();

  static Statistics* getInstance();

  /// Setters
  void setNDRangeCycle(unsigned ndrange_id, long long cycle,
                       enum NDRangeEventType event_type);

  /// Getters
  unsigned getSamplingInterval() const { return cu_sampling_interval_; }

  /// Member functions
  void Sampling(ComputeUnit* cu, long long cycle);

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os,
                                  const Statistics& statistics) {
    statistics.Dump(os);
    return os;
  }
};
}  // namespace SI
#endif
