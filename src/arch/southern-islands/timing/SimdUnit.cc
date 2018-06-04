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

#include "ComputeUnit.h"
#include "Gpu.h"
#include "SimdUnit.h"
#include "Timing.h"

namespace SI {

int SimdUnit::width = 1;
int SimdUnit::num_simd_lanes = 16;
int SimdUnit::issue_buffer_size = 1;
int SimdUnit::decode_latency = 1;
int SimdUnit::decode_buffer_size = 1;
int SimdUnit::read_exec_write_latency = 8;
int SimdUnit::exec_buffer_size = 2;
int SimdUnit::read_exec_write_buffer_size = 2;

void SimdUnit::Run() {
  SimdUnit::PreRun();

  SimdUnit::Complete();
  SimdUnit::Execute();
  SimdUnit::Decode();

  SimdUnit::PostRun();
}

std::string SimdUnit::getStatus() const {
  std::string status = "SIMD  ";

  status += "\t";
  if (issue_buffer.size() != 0) {
    status += stage_status_map[IssueStatus] +
              std::to_string(issue_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\t";
  if (decode_buffer.size() != 0) {
    status += stage_status_map[DecodeStatus] +
              std::to_string(decode_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\t";
  if (exec_buffer.size() != 0) {
    if (exec_buffer.size() != 1)
      status += "+" + std::to_string(exec_buffer.size());
    else
      status += stage_status_map[ExecutionStatus] +
                std::to_string(exec_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\n";

  return status;
}

bool SimdUnit::isValidUop(Uop* uop) const {
  // Get instruction
  Instruction* instruction = uop->getInstruction();

  // Determine if simd instruction
  if (instruction->getFormat() != Instruction::FormatVOP2 &&
      instruction->getFormat() != Instruction::FormatVOP1 &&
      instruction->getFormat() != Instruction::FormatVOPC &&
      instruction->getFormat() != Instruction::FormatVOP3a &&
      instruction->getFormat() != Instruction::FormatVOP3b)
    return false;

  return true;
}

void SimdUnit::Issue(std::unique_ptr<Uop> uop) {
  // One more instruction of this kind
  ComputeUnit* compute_unit = getComputeUnit();
  compute_unit->stats.num_simd_insts_++;

  // Issue it
  ExecutionUnit::Issue(std::move(uop));

  // Update pipeline stage status
  IssueStatus = Active;
}

void SimdUnit::Complete() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();
  Gpu* gpu = compute_unit->getGpu();

  // Sanity check exec buffer
  assert(int(exec_buffer.size()) <= exec_buffer_size);

  // Initialize iterator
  auto it = exec_buffer.begin();

  // Process completed instructions
  while (it != exec_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // Checks
    assert(uop);

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->execute_ready) {
      ReadStatus = Active;
      ExecutionStatus = Active;
      break;
    }

    // Update uop info
    uop->cycle_finish = compute_unit->getTiming()->getCycle();
    uop->cycle_length = uop->cycle_finish - uop->cycle_start;

    // Trace for m2svis
    Timing::m2svis << uop->getLifeCycleInCSV("simd");

    // Update pipeline stage status
    WriteStatus = Active;

    // Update statistics
    if (overview_file_)
      overview_stats_.Complete(uop, compute_unit->getTiming()->getCycle());
    if (interval_file_)
      interval_stats_.Complete(uop, compute_unit->getTiming()->getCycle());

    // Trace
    Timing::trace << misc::fmt(
        "si.end_inst "
        "id=%lld "
        "cu=%d\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex());

    // Statistics
    num_instructions++;
    gpu->last_complete_cycle = compute_unit->getTiming()->getCycle();

    // Remove uop from the exec buffer and get the iterator to the
    // next element
    auto uop_complete = std::move(*it);
    it = exec_buffer.erase(it);
    assert(uop->getWorkGroup()->inflight_instructions > 0);
    uop->getWorkGroup()->inflight_instructions--;

    // Update info if statistics enables
    if (Timing::statistics_level >= 2) {
      auto gpu = compute_unit->getGpu();
      if (gpu) {
        // NDRange
        auto ndrange_stats =
            gpu->getNDRangeStatsById(uop_complete->getNDRangeId());
        if (ndrange_stats) {
          ndrange_stats->setCycle(Timing::getInstance()->getCycle(),
                                  EVENT_FINISH);
        }

        // Workgroup
        auto workgroup_stats = compute_unit->getWorkgroupStatsById(
            uop_complete->getWorkGroup()->id_in_compute_unit);
        if (workgroup_stats) {
          workgroup_stats->setCycle(Timing::getInstance()->getCycle(),
                                    EVENT_FINISH);
        }

        // Wavefront
        auto wavefront_stats = compute_unit->getWavefrontStatsById(
            uop_complete->getWavefront()->id_in_compute_unit);
        if (wavefront_stats) {
          wavefront_stats->setCycle(Timing::getInstance()->getCycle(),
                                    EVENT_FINISH);
        }
      }
    }
  }
}

void SimdUnit::Execute() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Sanity check decode buffer
  assert(int(decode_buffer.size()) <= decode_buffer_size);

  // Internal counter
  int instructions_processed = 0;

  // Initialize iterator
  auto it = decode_buffer.begin();

  // Process completed instructions
  while (it != decode_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->decode_ready) {
      DecodeStatus = Active;
      break;
    }

    // Stall if width has been reached
    if (instructions_processed > width) {
      // Update uop stall
      uop->cycle_execute_stall++;

      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      compute_unit->getWavefrontStatsById(wf_id)->num_stall_execution_++;
      compute_unit->getWavefrontStatsById(wf_id)->simd_num_stall_execution_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      compute_unit->getWorkgroupStatsById(wg_id)->num_stall_execution_++;
      compute_unit->getWorkgroupStatsById(wg_id)->simd_num_stall_execution_++;

      // Update pipeline stage status
      ReadStatus = Stall;
      ExecutionStatus = Stall;
      WriteStatus = Stall;

      if (overview_file_) {
        overview_stats_.num_stall_read_++;
        overview_stats_.num_stall_execution_++;
        overview_stats_.num_stall_write_++;
      }

      if (interval_file_) {
        interval_stats_.num_stall_read_++;
        interval_stats_.num_stall_execution_++;
        interval_stats_.num_stall_write_++;
      }

      // Trace
      Timing::trace << misc::fmt(
          "si.inst "
          "id=%lld "
          "cu=%d "
          "wf=%d "
          "uop_id=%lld "
          "stg=\"s\"\n",
          uop->getIdInComputeUnit(), compute_unit->getIndex(),
          uop->getWavefront()->getId(), uop->getIdInWavefront());
      break;
    }

    // Sanity check exec buffer
    assert(int(exec_buffer.size()) <= exec_buffer_size);

    // Stall if there is not room in the exec buffer
    if (int(exec_buffer.size()) == exec_buffer_size) {
      // Update uop stall
      uop->cycle_execute_stall++;

      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      compute_unit->getWavefrontStatsById(wf_id)->num_stall_execution_++;
      compute_unit->getWavefrontStatsById(wf_id)->simd_num_stall_execution_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      compute_unit->getWorkgroupStatsById(wg_id)->num_stall_execution_++;
      compute_unit->getWorkgroupStatsById(wg_id)->simd_num_stall_execution_++;

      // Update pipeline stage status
      ReadStatus = Stall;
      ExecutionStatus = Stall;
      WriteStatus = Stall;

      if (overview_file_) {
        overview_stats_.num_stall_read_++;
        overview_stats_.num_stall_execution_++;
        overview_stats_.num_stall_write_++;
      }
      if (interval_file_) {
        interval_stats_.num_stall_read_++;
        interval_stats_.num_stall_execution_++;
        interval_stats_.num_stall_write_++;
      }

      // Trace
      Timing::trace << misc::fmt(
          "si.inst "
          "id=%lld "
          "cu=%d "
          "wf=%d "
          "uop_id=%lld "
          "stg=\"s\"\n",
          uop->getIdInComputeUnit(), compute_unit->getIndex(),
          uop->getWavefront()->getId(), uop->getIdInWavefront());
      break;
    }

    // Includes time for pipelined read-exec-write of all
    // subwavefronts
    uop->execute_ready =
        compute_unit->getTiming()->getCycle() + read_exec_write_latency;

    // Update uop cycle
    int read_latency = 1;
    int write_latency = 1;

    uop->cycle_read_begin = uop->decode_ready;
    uop->cycle_read_active = uop->decode_ready;
    uop->read_ready = uop->cycle_read_active + read_latency;

    uop->cycle_execute_begin = uop->read_ready;
    uop->cycle_execute_active = uop->read_ready + uop->cycle_execute_stall;

    uop->cycle_write_begin = uop->execute_ready - write_latency;
    uop->cycle_write_active = uop->execute_ready - write_latency;
    uop->write_ready = uop->execute_ready;

    // Update pipeline stage status
    ReadStatus = Active;
    ExecutionStatus = Active;
    WriteStatus = Active;

    // Update wavefront pool entry
    uop->getWavefrontPoolEntry()->ready_next_cycle = true;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"simd-e\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to exec buffer and get the iterator for
    // the next element
    exec_buffer.push_back(std::move(*it));
    it = decode_buffer.erase(it);
  }
}

void SimdUnit::Decode() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Sanity check issue buffer
  assert(int(issue_buffer.size()) <= issue_buffer_size);

  // Internal counter
  int instructions_processed = 0;

  // Initialize iterator
  auto it = issue_buffer.begin();

  // Process completed instructions
  while (it != issue_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->issue_ready) {
      IssueStatus = Active;
      break;
    }

    // Stall if width has been reached
    if (instructions_processed > width) {
      // Update uop stall
      uop->cycle_decode_stall++;

      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      compute_unit->getWavefrontStatsById(wf_id)->num_stall_decode_++;
      compute_unit->getWavefrontStatsById(wf_id)->simd_num_stall_decode_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      compute_unit->getWorkgroupStatsById(wg_id)->num_stall_decode_++;
      compute_unit->getWorkgroupStatsById(wg_id)->simd_num_stall_decode_++;

      // Update pipeline stage status
      DecodeStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_decode_++;
      if (interval_file_) interval_stats_.num_stall_decode_++;

      // Trace
      Timing::trace << misc::fmt(
          "si.inst "
          "id=%lld "
          "cu=%d "
          "wf=%d "
          "uop_id=%lld "
          "stg=\"s\"\n",
          uop->getIdInComputeUnit(), compute_unit->getIndex(),
          uop->getWavefront()->getId(), uop->getIdInWavefront());
      break;
    }

    // Sanity check decode buffer
    assert(int(decode_buffer.size()) <= decode_buffer_size);

    // Stall if there is not room in the decode buffer
    if (int(decode_buffer.size()) == decode_buffer_size) {
      // Update uop stall
      uop->cycle_decode_stall++;

      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      compute_unit->getWavefrontStatsById(wf_id)->num_stall_decode_++;
      compute_unit->getWavefrontStatsById(wf_id)->simd_num_stall_decode_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      compute_unit->getWorkgroupStatsById(wg_id)->num_stall_decode_++;
      compute_unit->getWorkgroupStatsById(wg_id)->simd_num_stall_decode_++;

      // Update pipeline stage status
      DecodeStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_decode_++;
      if (interval_file_) interval_stats_.num_stall_decode_++;

      // Trace
      Timing::trace << misc::fmt(
          "si.inst "
          "id=%lld "
          "cu=%d "
          "wf=%d "
          "uop_id=%lld "
          "stg=\"s\"\n",
          uop->getIdInComputeUnit(), compute_unit->getIndex(),
          uop->getWavefront()->getId(), uop->getIdInWavefront());
      break;
    }

    // Update uop
    uop->decode_ready = compute_unit->getTiming()->getCycle() + decode_latency;

    // Update uop cycle
    uop->cycle_decode_begin = uop->issue_ready;
    uop->cycle_decode_active = compute_unit->getTiming()->getCycle();

    // Update pipeline stage status
    DecodeStatus = Active;

    // if (si_spatial_report_active)
    //  SIComputeUnitReportNewALUInst(simd->compute_unit);

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"simd-d\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to decode buffer and get the iterator for
    // the next element
    decode_buffer.push_back(std::move(*it));
    it = issue_buffer.erase(it);
  }
}

}  // namespace SI
