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
#include <arch/southern-islands/emulator/WorkItem.h>

#include "ComputeUnit.h"
#include "LdsUnit.h"
#include "Timing.h"

namespace SI {
int LdsUnit::width = 1;
int LdsUnit::issue_buffer_size = 4;
int LdsUnit::decode_latency = 1;
int LdsUnit::decode_buffer_size = 1;
int LdsUnit::read_latency = 1;
int LdsUnit::read_buffer_size = 1;
int LdsUnit::write_latency = 1;
int LdsUnit::write_buffer_size = 1;
int LdsUnit::max_in_flight_mem_accesses = 32;

void LdsUnit::Run() {
  LdsUnit::PreRun();

  // Run pipeline stages in reverse order
  LdsUnit::Complete();
  LdsUnit::Write();
  LdsUnit::Mem();
  LdsUnit::Read();
  LdsUnit::Decode();

  LdsUnit::PostRun();
}

std::string LdsUnit::getStatus() const {
  std::string status = "LDS   ";

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
  if (read_buffer.size() != 0) {
    status += stage_status_map[ReadStatus] +
              std::to_string(read_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\t";
  if (mem_buffer.size() != 0) {
    if (mem_buffer.size() != 1)
      status += "+" + std::to_string(mem_buffer.size());
    else
      status += stage_status_map[ExecutionStatus] +
                std::to_string(mem_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\t";
  if (write_buffer.size() != 0) {
    status += stage_status_map[WriteStatus] +
              std::to_string(write_buffer[0]->getIdInComputeUnit());
  } else
    status += "__";

  status += "\n";

  return status;
}

bool LdsUnit::isValidUop(Uop* uop) const {
  // Get instruction
  Instruction* instruction = uop->getInstruction();

  // Determine if lds instruction
  if (instruction->getFormat() != Instruction::FormatDS) return false;

  return true;
}

void LdsUnit::Issue(std::unique_ptr<Uop> uop) {
  // Get compute unit
  ComputeUnit* compute_unit = getComputeUnit();

  // One more instruction of this kind
  compute_unit->stats.num_lds_insts_++;
  uop->getWavefrontPoolEntry()->lgkm_cnt++;

  // Issue it
  ExecutionUnit::Issue(std::move(uop));

  // Update pipeline stage status
  IssueStatus = Active;
}

void LdsUnit::Complete() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Sanity check write buffer
  assert(int(write_buffer.size()) <= write_buffer_size);

  // Initialize iterator
  auto it = write_buffer.begin();

  // Iterate through uops in the write buffer
  while (it != write_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // Uop is not ready yet
    if (compute_unit->getTiming()->getCycle() < uop->write_ready) {
      WriteStatus = Active;
      break;
    }

    // Statistics
    assert(uop->getWavefrontPoolEntry()->lgkm_cnt > 0);
    uop->getWavefrontPoolEntry()->lgkm_cnt--;

    // Update uop info
    uop->cycle_finish = compute_unit->getTiming()->getCycle();
    uop->cycle_length = uop->cycle_finish - uop->cycle_start;

    // Trace for m2svis
    Timing::m2svis << uop->getLifeCycleInCSV("lds");

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

    // Access complete, remove the uop from the queue
    auto uop_complete = std::move(*it);
    it = write_buffer.erase(it);
    assert(uop->getWorkGroup()->inflight_instructions > 0);
    uop->getWorkGroup()->inflight_instructions--;

    // Statistics
    num_instructions++;
    compute_unit->getGpu()->last_complete_cycle =
        compute_unit->getTiming()->getCycle();

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

void LdsUnit::Write() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check write buffer
  assert(int(mem_buffer.size()) <= max_in_flight_mem_accesses);

  // Initialize iterator
  auto it = mem_buffer.begin();

  // Process completed instructions
  while (it != mem_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if Uop is not ready yet
    if (uop->lds_witness) {
      ExecutionStatus = Active;
      break;
    }

    // Stall if the width has been reached
    if (instructions_processed > width) {
      // Update uop stall write
      uop->cycle_write_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_write_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_write_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_write_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_write_++;
      }

      // Update pipeline stage status
      WriteStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_write_++;
      if (interval_file_) interval_stats_.num_stall_write_++;

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

    // Sanity check the write buffer
    assert(int(write_buffer.size()) <= write_buffer_size);

    // Stop if the write buffer is full
    if (int(write_buffer.size()) == write_buffer_size) {
      // Update uop stall write
      uop->cycle_write_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_write_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_write_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_write_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_write_++;
      }

      // Update pipeline stage status
      WriteStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_write_++;
      if (interval_file_) interval_stats_.num_stall_write_++;

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

    // Access complete, update write ready
    uop->write_ready = compute_unit->getTiming()->getCycle() + write_latency;

    // Update wavefront pool entry
    uop->getWavefrontPoolEntry()->ready_next_cycle = true;

    // One more instruction processed
    instructions_processed++;

    // Update uop cycle
    uop->cycle_write_begin = uop->execute_ready;
    uop->cycle_write_active = compute_unit->getTiming()->getCycle();

    // Update pipeline stage status
    WriteStatus = Active;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"lds-w\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to write buffer and get the iterator for the next
    // element
    write_buffer.push_back(std::move(*it));
    it = mem_buffer.erase(it);
  }
}

void LdsUnit::Mem() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check write buffer
  assert(int(read_buffer.size()) <= read_buffer_size);

  // Initialize iterator
  auto it = read_buffer.begin();

  // Process completed instructions
  while (it != read_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if Uop is not ready yet
    if (compute_unit->getTiming()->getCycle() < uop->read_ready) {
      ReadStatus = Active;
      break;
    }

    // Stall if the width has been reached
    if (instructions_processed > width) {
      // Update stall execution
      uop->cycle_execute_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_execution_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_execution_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_execution_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_execution_++;
      }

      // Update pipeline stage status
      ExecutionStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_execution_++;
      if (interval_file_) interval_stats_.num_stall_execution_++;

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

    // Sanity check uop
    assert(uop->lds_read || uop->lds_write);

    // Sanity check the mem buffer
    assert(int(mem_buffer.size()) <= max_in_flight_mem_accesses);

    // Stop if the memory buffer is full
    if (int(mem_buffer.size()) == max_in_flight_mem_accesses) {
      // Update stall execution
      uop->cycle_execute_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_execution_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_execution_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_execution_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_execution_++;
      }

      // Update pipeline stage status
      ExecutionStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_execution_++;
      if (interval_file_) interval_stats_.num_stall_execution_++;

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

    // Access local memory
    for (auto it = uop->getWavefront()->getWorkItemsBegin(),
              e = uop->getWavefront()->getWorkItemsEnd();
         it != e; ++it) {
      // Get work item
      WorkItem* work_item = it->get();

      // Get uop work item info
      Uop::WorkItemInfo* work_item_info;
      work_item_info = &uop->work_item_info_list[work_item->getIdInWavefront()];

      // Access type
      mem::Module::AccessType access_type;

      for (int i = 0; i < work_item_info->lds_access_count; i++) {
        switch (work_item->lds_access[i].type) {
          case WorkItem::MemoryAccessType::MemoryAccessRead: {
            access_type = mem::Module::AccessType::AccessLoad;
            break;
          }

          case WorkItem::MemoryAccessType::MemoryAccessWrite: {
            access_type = mem::Module::AccessType::AccessStore;
            break;
          }

          default:

            throw misc::Panic("Invalid lds access");
        }

        // Start access
        compute_unit->getLdsModule()->Access(
            access_type, work_item_info->lds_access[i].addr, &uop->lds_witness);
        uop->lds_witness--;
      }
    }

    // Update uop execute ready cycle for m2svis tool
    uop->execute_ready = compute_unit->getTiming()->getCycle();

    // Update uop cycle
    uop->cycle_execute_begin = uop->read_ready;
    uop->cycle_execute_active = compute_unit->getTiming()->getCycle();

    // Update pipeline stage status
    ExecutionStatus = Active;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"lds-m\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to the mem buffer and get the iterator for the
    // next element
    mem_buffer.push_back(std::move(*it));
    it = read_buffer.erase(it);
  }
}

void LdsUnit::Read() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check write buffer
  assert(int(decode_buffer.size()) <= decode_buffer_size);

  // Initialize iterator
  auto it = decode_buffer.begin();

  // Process completed instructions
  while (it != decode_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Stall if the width has been reached
    if (instructions_processed > width) {
      // Update uop stall read
      uop->cycle_read_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_read_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_read_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_read_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_read_++;
      }

      // Update pipeline stage status
      ReadStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_read_++;
      if (interval_file_) interval_stats_.num_stall_read_++;

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

    // Stop if the read buffer is full
    if ((int)read_buffer.size() == read_buffer_size) {
      // Update uop stall read
      uop->cycle_read_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_read_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_read_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_read_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_read_++;
      }

      // Update pipeline stage status
      ReadStatus = Stall;

      if (overview_file_) overview_stats_.num_stall_read_++;
      if (interval_file_) interval_stats_.num_stall_read_++;

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

    // Uop is not ready yet
    if (compute_unit->getTiming()->getCycle() < uop->decode_ready) {
      DecodeStatus = Active;
      break;
    }

    // Update uop
    uop->read_ready = compute_unit->getTiming()->getCycle() + read_latency;

    // Update uop cycle
    uop->cycle_read_begin = uop->decode_ready;
    uop->cycle_read_active = compute_unit->getTiming()->getCycle();

    // Update pipeline stage status
    ReadStatus = Active;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"lds-r\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to read buffer and get the iterator for the
    // next element
    read_buffer.push_back(std::move(*it));
    it = decode_buffer.erase(it);
  }
}

void LdsUnit::Decode() {
  // Get useful objects
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check write buffer
  assert(int(issue_buffer.size()) <= issue_buffer_size);

  // Initialize iterator
  auto it = issue_buffer.begin();

  // Process completed instructions
  while (it != issue_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Uop is not ready yet
    if (compute_unit->getTiming()->getCycle() < uop->issue_ready) {
      IssueStatus = Active;
      break;
    }

    // Stall if the width has been reached
    if (instructions_processed > width) {
      // Update uop stall decode
      uop->cycle_decode_stall++;

      if (Timing::statistics_level >= 1) {
        // Per WF stats
        unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
        compute_unit->getWavefrontStatsById(wf_id)->num_stall_decode_++;
        compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_decode_++;

        // Per WG stats
        unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
        compute_unit->getWorkgroupStatsById(wg_id)->num_stall_decode_++;
        compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_decode_++;
      }

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

    // Sanity check the decode buffer
    assert(int(decode_buffer.size()) <= decode_buffer_size);

    // Stop if the decode buffer is full
    if (int(decode_buffer.size()) == decode_buffer_size) {
      // Update uop stall decode
      uop->cycle_decode_stall++;

      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      compute_unit->getWavefrontStatsById(wf_id)->num_stall_decode_++;
      compute_unit->getWavefrontStatsById(wf_id)->lds_num_stall_decode_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      compute_unit->getWorkgroupStatsById(wg_id)->num_stall_decode_++;
      compute_unit->getWorkgroupStatsById(wg_id)->lds_num_stall_decode_++;

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
    //  SIComputeUnitReportNewLDSInst(lds->compute_unit);

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"lds-d\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Mode uop to decode buffer and get the iterator for the
    // next element
    decode_buffer.push_back(std::move(*it));
    it = issue_buffer.erase(it);
  }
}

}  // namespace SI
