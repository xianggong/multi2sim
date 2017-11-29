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

#include <arch/southern-islands/emulator/Wavefront.h>
#include <arch/southern-islands/emulator/WorkGroup.h>

#include "BranchUnit.h"
#include "Timing.h"

namespace SI {

std::map<StageStatus, std::string> stage_status_map = {
    std::make_pair(Idle, "i"), std::make_pair(Active, "a"),
    std::make_pair(Stall, "s")};

void ExecutionUnit::Issue(std::unique_ptr<Uop> uop) {
  // Spend issue latency
  Timing* timing = Timing::getInstance();
  assert(uop->issue_ready == 0);
  uop->issue_ready = timing->getCycle() + ComputeUnit::issue_latency;

  // Insert into issue buffer
  assert(canIssue());
  issue_buffer.push_back(std::move(uop));
  num_instructions++;
}

void ExecutionUnit::resetStatus() {
  // Reset status of all pipeline stage to idle
  IssueStatus = Idle;
  DecodeStatus = Idle;
  ReadStatus = Idle;
  ExecutionStatus = Idle;
  WriteStatus = Idle;
}

void ExecutionUnit::updateCounter(std::string exec_unit) {
  stats.num_total_cycles_++;

  // ExecutionUnit is idle when all stages are idle
  if (IssueStatus == Idle && DecodeStatus == Idle && ReadStatus == Idle &&
      ExecutionStatus == Idle && WriteStatus == Idle) {
    stats.num_idle_cycles_++;
    return;
  } else {
    stats.num_active_or_stall_cycles_++;
  }

  bool isAnyStageActive = IssueStatus == Active || DecodeStatus == Active ||
                          ReadStatus == Active || ExecutionStatus == Active ||
                          WriteStatus == Active;
  bool isAnyStageStall = IssueStatus == Stall || DecodeStatus == Stall ||
                         ReadStatus == Stall || ExecutionStatus == Stall ||
                         WriteStatus == Stall;
  if (isAnyStageStall) {
    if (isAnyStageActive)
      stats.num_active_and_stall_cycles_++;
    else
      stats.num_stall_only_cycles_++;
  } else {
    stats.num_active_only_cycles_++;
  }

  if (Timing::getInstance()->getCycle() % 1000 == 0) {
    curr_interval_stats.num_total_cycles_ =
        stats.num_total_cycles_ - prev_interval_stats.num_total_cycles_;
    curr_interval_stats.num_idle_cycles_ =
        stats.num_idle_cycles_ - prev_interval_stats.num_idle_cycles_;
    curr_interval_stats.num_active_or_stall_cycles_ =
        stats.num_active_or_stall_cycles_ -
        prev_interval_stats.num_active_or_stall_cycles_;
    curr_interval_stats.num_active_only_cycles_ =
        stats.num_active_only_cycles_ -
        prev_interval_stats.num_active_only_cycles_;
    curr_interval_stats.num_active_and_stall_cycles_ =
        stats.num_active_and_stall_cycles_ -
        prev_interval_stats.num_active_and_stall_cycles_;
    curr_interval_stats.num_stall_only_cycles_ =
        stats.num_stall_only_cycles_ -
        prev_interval_stats.num_stall_only_cycles_;
    curr_interval_stats.num_stall_issue_ =
        stats.num_stall_issue_ - prev_interval_stats.num_stall_issue_;
    curr_interval_stats.num_stall_decode_ =
        stats.num_stall_decode_ - prev_interval_stats.num_stall_decode_;
    curr_interval_stats.num_stall_read_ =
        stats.num_stall_read_ - prev_interval_stats.num_stall_read_;
    curr_interval_stats.num_stall_execution_ =
        stats.num_stall_execution_ - prev_interval_stats.num_stall_execution_;
    curr_interval_stats.num_stall_write_ =
        stats.num_stall_write_ - prev_interval_stats.num_stall_write_;
    curr_interval_stats.num_vmem_divergence_ =
        stats.num_vmem_divergence_ - prev_interval_stats.num_vmem_divergence_;

    prev_interval_stats.num_total_cycles_ = stats.num_total_cycles_;
    prev_interval_stats.num_idle_cycles_ = stats.num_idle_cycles_;
    prev_interval_stats.num_active_or_stall_cycles_ =
        stats.num_active_or_stall_cycles_;
    prev_interval_stats.num_active_only_cycles_ = stats.num_active_only_cycles_;
    prev_interval_stats.num_active_and_stall_cycles_ =
        stats.num_active_and_stall_cycles_;
    prev_interval_stats.num_stall_only_cycles_ = stats.num_stall_only_cycles_;
    prev_interval_stats.num_stall_issue_ = stats.num_stall_issue_;
    prev_interval_stats.num_stall_decode_ = stats.num_stall_decode_;
    prev_interval_stats.num_stall_read_ = stats.num_stall_read_;
    prev_interval_stats.num_stall_execution_ = stats.num_stall_execution_;
    prev_interval_stats.num_stall_write_ = stats.num_stall_write_;
    prev_interval_stats.num_vmem_divergence_ = stats.num_vmem_divergence_;

    // printf("%lld %s ", Timing::getInstance()->getCycle(), exec_unit.c_str());
    // auto stats = *Timing::statistics[compute_unit->getIndex()];
    // stats << exec_unit;
    // Dump(std::cout);
  }
}

void ExecutionUnit::Dump(std::ostream& os) const {
  os << misc::fmt(
      " util %.2g%% %.2g%% %.2g%% %.2g%% %.2g%%\n",
      100 * (double)curr_interval_stats.num_active_or_stall_cycles_ /
          (double)curr_interval_stats.num_total_cycles_,
      100 * (double)curr_interval_stats.num_idle_cycles_ /
          (double)curr_interval_stats.num_total_cycles_,
      100 * (double)curr_interval_stats.num_active_only_cycles_ /
          (double)curr_interval_stats.num_total_cycles_,
      100 * (double)curr_interval_stats.num_active_and_stall_cycles_ /
          (double)curr_interval_stats.num_total_cycles_,
      100 * (double)curr_interval_stats.num_stall_only_cycles_ /
          (double)curr_interval_stats.num_total_cycles_);
}

std::string ExecutionUnit::getUtilization(std::string ExecutionUnitName) {
  return "Util." + ExecutionUnitName +
         misc::fmt(":\t %.2g \t %.2g \t %.2g \t %.2g \t %.2g\n",
                   100 * (double)stats.num_active_or_stall_cycles_ /
                       (double)stats.num_total_cycles_,
                   100 * (double)stats.num_idle_cycles_ /
                       (double)stats.num_total_cycles_,
                   100 * (double)stats.num_active_only_cycles_ /
                       (double)stats.num_total_cycles_,
                   100 * (double)stats.num_active_and_stall_cycles_ /
                       (double)stats.num_total_cycles_,
                   100 * (double)stats.num_stall_only_cycles_ /
                       (double)stats.num_total_cycles_);
}

std::string ExecutionUnit::getCounter(std::string ExecutionUnitName) {
  return "Count." + ExecutionUnitName +
         misc::fmt(
             ":\t %lld \t %lld \t %lld \t %lld \t %lld \t %lld[%lld %lld %lld "
             "%lld %lld] \t %lld\n",
             stats.num_total_cycles_, stats.num_active_or_stall_cycles_,
             stats.num_idle_cycles_, stats.num_active_only_cycles_,
             stats.num_active_and_stall_cycles_, stats.num_stall_only_cycles_,
             stats.num_stall_issue_, stats.num_stall_decode_,
             stats.num_stall_read_, stats.num_stall_execution_,
             stats.num_stall_write_, stats.num_vmem_divergence_);
}

bool ExecutionUnit::isActive() {
  bool isIdle = IssueStatus == Idle && DecodeStatus == Idle &&
                ReadStatus == Idle && ExecutionStatus == Idle &&
                WriteStatus == Idle;
  return !isIdle;
}
}
