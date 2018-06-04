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

#include "ExecutionUnitStats.h"
#include "Timing.h"
#include "Uop.h"

namespace SI {

std::map<StageStatus, std::string> stage_status_map = {
    std::make_pair(Idle, "i"), std::make_pair(Active, "a"),
    std::make_pair(Stall, "s")};

void ExecutionUnitStatistics::Reset() {
  num_total_cycles_ = 0;

  num_idle_cycles_ = 0;
  num_active_or_stall_cycles_ = 0;
  num_active_only_cycles_ = 0;
  num_active_and_stall_cycles_ = 0;

  num_stall_only_cycles_ = 0;
  num_stall_issue_ = 0;
  num_stall_decode_ = 0;
  num_stall_read_ = 0;
  num_stall_execution_ = 0;
  num_stall_write_ = 0;

  num_vmem_divergence_ = 0;
  num_inst_iss_ = 0;
  num_inst_cpl_ = 0;

  len_inst_min_ = 0;
  len_inst_max_ = 0;
  len_inst_sum_ = 0;
}

void ExecutionUnitStatistics::Complete(Uop* uop, long long cycle) {
  len_inst_sum_ += uop->cycle_length;

  if (uop->cycle_length > len_inst_max_) {
    len_inst_max_ = uop->cycle_length;
    wf_id_inst_max_ = uop->getWavefront()->getId();
    wg_id_inst_max_ = uop->getWorkGroup()->getId();
  } else if (uop->cycle_length < len_inst_min_ || len_inst_min_ == 0) {
    len_inst_min_ = uop->cycle_length;
    wf_id_inst_min_ = uop->getWavefront()->getId();
    wg_id_inst_min_ = uop->getWorkGroup()->getId();
  }

  // auto min = std::min(len_inst_min_, uop->cycle_length);
  // len_inst_min_ = len_inst_min_ == 0 ? uop->cycle_length : min;
  // len_inst_max_ = std::max(len_inst_max_, uop->cycle_length);
  num_inst_cpl_++;
  num_inst_wip_--;
}

void ExecutionUnitStatistics::DumpUtilization(std::ostream& os) const {
  os << misc::fmt(
      "%.2g,%.2g,%.2g,%.2g,%.2g",
      num_total_cycles_ == 0 ? 0 : (double)num_active_or_stall_cycles_ /
                                       (double)num_total_cycles_,
      num_total_cycles_ == 0 ? 0 : (double)num_idle_cycles_ /
                                       (double)num_total_cycles_,
      num_total_cycles_ == 0 ? 0 : (double)num_active_only_cycles_ /
                                       (double)num_total_cycles_,
      num_total_cycles_ == 0 ? 0 : (double)num_active_and_stall_cycles_ /
                                       (double)num_total_cycles_,
      num_total_cycles_ == 0 ? 0 : (double)num_stall_only_cycles_ /
                                       (double)num_total_cycles_);
}

void ExecutionUnitStatistics::DumpUtilizationField(std::ostream& os) const {
  os << "u_actv|stll,";
  os << "u_idle,";
  os << "u_actv,";
  os << "u_actv&stll,";
  os << "u_stll\n";
}

void ExecutionUnitStatistics::DumpCounter(std::ostream& os) const {
  os << misc::fmt(
      "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%"
      "lld,%lld,%d,%d,%lld,%d,%d,%lld,%lld,",
      num_total_cycles_, num_active_or_stall_cycles_, num_idle_cycles_,
      num_active_only_cycles_, num_active_and_stall_cycles_,
      num_stall_only_cycles_, num_stall_issue_, num_stall_decode_,
      num_stall_read_, num_stall_execution_, num_stall_write_,
      num_vmem_divergence_, num_inst_iss_, num_inst_wip_, num_inst_cpl_,
      len_inst_min_, wf_id_inst_min_, wg_id_inst_min_, len_inst_max_,
      wf_id_inst_max_, wg_id_inst_max_,
      num_inst_cpl_ == 0 ? 0 : len_inst_sum_ / num_inst_cpl_, len_inst_sum_);
}

void ExecutionUnitStatistics::DumpCounterField(std::ostream& os) const {
  os << "interval,";
  os << "c_total,";
  os << "c_actv|c_stll,";
  os << "c_idle,";
  os << "c_actv,";
  os << "c_actv&c_stll,";
  os << "c_stll,";
  os << "n_stll_iss,";
  os << "n_stll_dec,";
  os << "n_stll_rea,";
  os << "n_stll_exe,";
  os << "n_stll_wrt,";
  os << "n_vmem_dvg,";
  os << "n_inst_iss,";
  os << "n_inst_wip,";
  os << "n_inst_cpl,";
  os << "l_inst_min,";
  os << "i_inst_min_wf_id,";
  os << "i_inst_min_wg_id,";
  os << "l_inst_max,";
  os << "i_inst_max_wf_id,";
  os << "i_inst_max_wg_id,";
  os << "l_inst_avg,";
  os << "l_inst_sum,";
}

ExecutionUnitStatisticsModule::ExecutionUnitStatisticsModule(
    ComputeUnit* compute_unit, std::string execution_unit_name)
    : compute_unit_(compute_unit), execution_unit_name_(execution_unit_name) {
  // Create output files if statistics enables
  if (Timing::statistics_level >= 1) {
    sampling_interval_ = Timing::statistics_sampling_cycle;

    auto overview_path = "cu_" + std::to_string(compute_unit->getIndex()) +
                         "_" + execution_unit_name_ + ".overvw";
    overview_file_.setPath(overview_path);
    overview_stats_.DumpCounterField(overview_file_);
    overview_stats_.DumpUtilizationField(overview_file_);

    overview_dump_.setStatistics(&overview_stats_);
    overview_dump_.setOutputStream(&overview_file_);

    auto interval_path = "cu_" + std::to_string(compute_unit->getIndex()) +
                         "_" + execution_unit_name_ + ".intrvl";
    interval_file_.setPath(interval_path);
    interval_stats_.DumpCounterField(interval_file_);
    interval_stats_.DumpUtilizationField(interval_file_);

    interval_dump_.setStatistics(&interval_stats_);
    interval_dump_.setOutputStream(&interval_file_);
    interval_dump_.setInterval(sampling_interval_);
  }
}

void ExecutionUnitStatisticsModule::PreRun() {
  // Reset status of all pipeline stage to idle before run
  IssueStatus = Idle;
  DecodeStatus = Idle;
  ReadStatus = Idle;
  ExecutionStatus = Idle;
  WriteStatus = Idle;
}

void ExecutionUnitStatisticsModule::UpdateStatus() {
  // Update statistics
  overview_stats_.num_total_cycles_++;
  interval_stats_.num_total_cycles_++;

  // ExecutionUnit is idle when all stages are idle
  if (IssueStatus == Idle && DecodeStatus == Idle && ReadStatus == Idle &&
      ExecutionStatus == Idle && WriteStatus == Idle) {
    overview_stats_.num_idle_cycles_++;
    interval_stats_.num_idle_cycles_++;
    return;
  } else {
    overview_stats_.num_active_or_stall_cycles_++;
    interval_stats_.num_active_or_stall_cycles_++;
  }

  bool isAnyStageActive = IssueStatus == Active || DecodeStatus == Active ||
                          ReadStatus == Active || ExecutionStatus == Active ||
                          WriteStatus == Active;
  bool isAnyStageStall = IssueStatus == Stall || DecodeStatus == Stall ||
                         ReadStatus == Stall || ExecutionStatus == Stall ||
                         WriteStatus == Stall;
  if (isAnyStageStall) {
    if (isAnyStageActive) {
      overview_stats_.num_active_and_stall_cycles_++;
      interval_stats_.num_active_and_stall_cycles_++;
    } else {
      overview_stats_.num_stall_only_cycles_++;
      interval_stats_.num_stall_only_cycles_++;
    }
  } else {
    overview_stats_.num_active_only_cycles_++;
    interval_stats_.num_active_only_cycles_++;
  }
}

void ExecutionUnitStatisticsModule::PostRun() {
  // No need to proceed if statistics is not enabled
  if (!overview_file_ && !interval_file_) return;

  long long curr_cycle = Timing::getInstance()->getCycle();

  bool is_dump_cycle = curr_cycle % sampling_interval_ == 0;
  bool is_prev_dumpd = curr_cycle / sampling_interval_ == last_dumped_interval_;

  // Dump
  if (is_dump_cycle || !is_prev_dumpd) {
    // Dump interval statistics
    interval_file_ << (curr_cycle / sampling_interval_) * sampling_interval_
                   << ",";
    interval_stats_.DumpCounter(interval_file_);
    interval_stats_.DumpUtilization(interval_file_);
    interval_file_ << "\n";

    // Update status
    interval_stats_.Reset();
    last_dumped_interval_ = curr_cycle / sampling_interval_;
  }

  overview_dump_.setPrevRunCycle(curr_cycle);
  interval_dump_.setPrevRunCycle(curr_cycle);

  // Update status
  UpdateStatus();
}

}  // namespace SI
