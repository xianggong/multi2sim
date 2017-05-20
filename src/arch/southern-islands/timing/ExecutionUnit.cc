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

void ExecutionUnit::Issue(std::unique_ptr<Uop> uop) {
  // Spend issue latency
  Timing* timing = Timing::getInstance();
  assert(uop->issue_ready == 0);
  uop->issue_ready = timing->getCycle() + ComputeUnit::issue_latency;

  // Insert into issue buffer
  assert(canIssue());
  issue_buffer.push_back(std::move(uop));
}

void ExecutionUnit::resetStatus() {
  // Reset status of all pipeline stage to idle
  IssueStatus = Idle;
  DecodeStatus = Idle;
  ReadStatus = Idle;
  ExecutionStatus = Idle;
  WriteStatus = Idle;
}

void ExecutionUnit::updateCounter() {
  count_total_cycles++;

  // ExecutionUnit is idle when all stages are idle
  if (IssueStatus == Idle && DecodeStatus == Idle && ReadStatus == Idle &&
      ExecutionStatus == Idle && WriteStatus == Idle) {
    count_idle_cycles++;
    return;
  } else  // ExecutionUnit is active when not idle, including stalls
    count_active_or_stall_cycles++;

  bool isAnyStageActive = IssueStatus == Active || DecodeStatus == Active ||
                          ReadStatus == Active || ExecutionStatus == Active ||
                          WriteStatus == Active;
  bool isAnyStageStall = IssueStatus == Stall || DecodeStatus == Stall ||
                         ReadStatus == Stall || ExecutionStatus == Stall ||
                         WriteStatus == Stall;
  if (isAnyStageStall) {
    if (isAnyStageActive)
      count_active_and_stall_cycles++;
    else
      count_stall_only_cycles++;
  } else
    count_active_only_cycles++;
}

std::string ExecutionUnit::getUtilization(std::string ExecutionUnitName) {
  return "Util." + ExecutionUnitName +
         misc::fmt(":\t %.2g \t %.2g \t %.2g \t %.2g \t %.2g\n",
                   100 * (double)count_active_or_stall_cycles /
                       (double)count_total_cycles,
                   100 * (double)count_idle_cycles / (double)count_total_cycles,
                   100 * (double)count_active_only_cycles /
                       (double)count_total_cycles,
                   100 * (double)count_active_and_stall_cycles /
                       (double)count_total_cycles,
                   100 * (double)count_stall_only_cycles /
                       (double)count_total_cycles);
}

std::string ExecutionUnit::getCounter(std::string ExecutionUnitName) {
  return "Count." + ExecutionUnitName +
         misc::fmt(
             ":\t %lld \t %lld \t %lld \t %lld \t %lld \t %lld[%lld %lld %lld "
             "%lld %lld] \t %lld\n",
             count_total_cycles, count_active_or_stall_cycles,
             count_idle_cycles, count_active_only_cycles,
             count_active_and_stall_cycles, count_stall_only_cycles,
             count_stall_issue, count_stall_decode, count_stall_read,
             count_stall_execution, count_stall_write, count_vmem_divergence);
}

bool ExecutionUnit::isActive() {
  bool isIdle = IssueStatus == Idle && DecodeStatus == Idle &&
                ReadStatus == Idle && ExecutionStatus == Idle &&
                WriteStatus == Idle;
  return !isIdle;
}
}
