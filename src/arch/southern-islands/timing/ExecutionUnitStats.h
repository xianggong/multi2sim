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

#ifndef SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_
#define SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_

#include <arch/southern-islands/emulator/Wavefront.h>
#include <arch/southern-islands/emulator/WorkGroup.h>
#include <lib/cpp/Debug.h>
#include <iostream>
#include <map>
#include <string>

namespace SI {

// Forward declaration
class Uop;
class ComputeUnit;

// Status of pipeline stage
enum StageStatus { Idle = 1, Active, Stall };
extern std::map<StageStatus, std::string> stage_status_map;

/// This class contains statistics
class ExecutionUnitStatistics {
 public:
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
  long long num_inst_iss_ = 0;
  long long num_inst_wip_ = 0;
  long long num_inst_cpl_ = 0;

  long long len_inst_min_ = 0;
  long long len_inst_max_ = 0;
  long long len_inst_sum_ = 0;

  int wf_id_inst_min_ = -1;
  int wf_id_inst_max_ = -1;
  int wg_id_inst_min_ = -1;
  int wg_id_inst_max_ = -1;

 public:
  /// Member functions

  /// Reset all counters
  void Reset();

  /// Record the cycle stats of a finished UOP
  void Complete(Uop* uop, long long cycle);

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Dump utilization
  void DumpUtilization(std::ostream& os = std::cout) const;
  void DumpUtilizationField(std::ostream& os = std::cout) const;

  /// Dump counters
  void DumpCounter(std::ostream& os = std::cout) const;
  void DumpCounterField(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os,
                                  const ExecutionUnitStatistics& stats) {
    stats.Dump(os);
    return os;
  }
};

/// Dummy class used to dump statatistics
class ExecutionUnitStatisticsDump {
 private:
  bool active = false;
  misc::Debug* output_stream_;
  class ExecutionUnitStatistics* data_;

  unsigned cycle_prev_run_ = 0;
  unsigned cycle_interval_ = 0;

 public:
  ~ExecutionUnitStatisticsDump() {
    if (active) {
      auto cycle = cycle_interval_ == 0
                       ? cycle_prev_run_
                       : (cycle_prev_run_ / cycle_interval_) * cycle_interval_ +
                             cycle_interval_;
      *output_stream_ << cycle << ",";
      data_->DumpCounter(*output_stream_);
      data_->DumpUtilization(*output_stream_);
      *output_stream_ << "\n";
    }
  }

  /// Setters
  void setStatistics(class ExecutionUnitStatistics* data) {
    data_ = data;
    if (output_stream_ != nullptr) active = true;
  }
  void setOutputStream(misc::Debug* output_stream) {
    output_stream_ = output_stream;
    if (data_ != nullptr) active = true;
  }
  void setPrevRunCycle(unsigned cycle) { cycle_prev_run_ = cycle; }
  void setInterval(unsigned interval) { cycle_interval_ = interval; }

  /// Getters
  unsigned getPrevRunCycle() const { return cycle_prev_run_; }
};

/// This class contains statistics
class ExecutionUnitStatisticsModule {
 private:
  // Compute unit that it belongs to, assigned in constructor
  ComputeUnit* compute_unit_;

  // Name of execution unit, assigned in constructor
  std::string execution_unit_name_;

  // Interval
  unsigned sampling_interval_ = 1000;
  int last_dumped_interval_ = -1;

 protected:
  // Statistics output stream
  misc::Debug overview_file_;
  // Statistics data up to current cycle
  class ExecutionUnitStatistics overview_stats_;
  // Used to dump the last piece of statistics
  class ExecutionUnitStatisticsDump overview_dump_;

  // Statistics file
  misc::Debug interval_file_;
  // Statistics up to previous interval
  class ExecutionUnitStatistics interval_stats_;
  // Used to dump the last piece of statistics
  class ExecutionUnitStatisticsDump interval_dump_;

  // Status of pipeline stage
  StageStatus IssueStatus = Idle;
  StageStatus DecodeStatus = Idle;
  StageStatus ReadStatus = Idle;
  StageStatus ExecutionStatus = Idle;
  StageStatus WriteStatus = Idle;

 public:
  /// Constructor
  ExecutionUnitStatisticsModule(ComputeUnit* compute_unit,
                                std::string execution_unit_name);

  /// Getters
  unsigned getInterval() const { return sampling_interval_; }

  ExecutionUnitStatistics* getIntervalStats() { return &interval_stats_; }
  ExecutionUnitStatistics* getOverviewStats() { return &overview_stats_; }

  /// Setters
  void setInterval(unsigned interval) { sampling_interval_ = interval; }

  /// Member functions

  // Before execution unit run, reset all status to idle
  void PreRun();

  // Update status of current cycle
  void UpdateStatus();

  // After execution unit run, update counter
  void PostRun();
};
}

#endif  // SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_
