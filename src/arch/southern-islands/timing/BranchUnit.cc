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

int BranchUnit::width = 1;
int BranchUnit::issue_buffer_size = 1;
int BranchUnit::decode_latency = 1;
int BranchUnit::decode_buffer_size = 1;
int BranchUnit::read_latency = 1;
int BranchUnit::read_buffer_size = 1;
int BranchUnit::exec_latency = 16;
int BranchUnit::exec_buffer_size = 16;
int BranchUnit::write_latency = 1;
int BranchUnit::write_buffer_size = 1;

void BranchUnit::Run() {
  resetStatus();

  Complete();
  Write();
  Execute();
  Read();
  Decode();

  updateCounter();
}

std::string BranchUnit::getStatus() const {
  std::string status = "Branch ";

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
  if (exec_buffer.size() != 0) {
    if (exec_buffer.size() != 1)
      status += "+" + std::to_string(exec_buffer.size());
    else
      status += stage_status_map[ExecutionStatus] +
                std::to_string(exec_buffer[0]->getIdInComputeUnit());
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

bool BranchUnit::isValidUop(Uop* uop) const {
  // Get instruction
  Instruction* instruction = uop->getInstruction();

  // Check if the instruction is a branch instruction
  return instruction->getFormat() == Instruction::FormatSOPP &&
         instruction->getBytes()->sopp.op > 1 &&
         instruction->getBytes()->sopp.op < 10;
}

void BranchUnit::Issue(std::unique_ptr<Uop> uop) {
  // One more instruction of this kind
  ComputeUnit* compute_unit = getComputeUnit();
  compute_unit->num_branch_instructions++;

  // Issue it
  ExecutionUnit::Issue(std::move(uop));

  // Update pipeline stage status
  IssueStatus = Active;
}

void BranchUnit::Complete() {
  // Get compute unit and GPU objects
  ComputeUnit* compute_unit = getComputeUnit();
  Gpu* gpu = compute_unit->getGpu();

  // Sanity check the write buffer
  assert((int)write_buffer.size() <= write_latency * width);

  // Process completed instructions
  auto it = write_buffer.begin();
  while (it != write_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->write_ready) {
      WriteStatus = Active;
      break;
    }

    // Update uop info
    uop->cycle_finish = compute_unit->getTiming()->getCycle();
    uop->cycle_length = uop->cycle_finish - uop->cycle_start;

    // Trace for m2svis
    Timing::m2svis << uop->getLifeCycleInCSV("branch");

    // Update compute unit statistics
    compute_unit->sum_cycle_branch_instructions += uop->cycle_length;

    compute_unit->min_cycle_branch_instructions =
        compute_unit->min_cycle_branch_instructions < uop->cycle_length
            ? compute_unit->min_cycle_branch_instructions
            : uop->cycle_length;

    compute_unit->max_cycle_branch_instructions =
        compute_unit->max_cycle_branch_instructions > uop->cycle_length
            ? compute_unit->max_cycle_branch_instructions
            : uop->cycle_length;

    // Update pipeline stage status
    WriteStatus = Active;

    // Record trace
    Timing::trace << misc::fmt(
        "si.end_inst "
        "id=%lld "
        "cu=%d\n ",
        uop->getIdInComputeUnit(), compute_unit->getIndex());

    // Allow next instruction to be fetched
    uop->getWavefrontPoolEntry()->ready = true;

    // Access complete, remove the uop from the queue, and get the
    // iterator for the next element
    it = write_buffer.erase(it);
    assert(uop->getWorkGroup()->inflight_instructions > 0);
    uop->getWorkGroup()->inflight_instructions--;

    // Statistics
    num_instructions++;
    gpu->last_complete_cycle = compute_unit->getTiming()->getCycle();
  }
}

void BranchUnit::Write() {
  // Get compute unit object
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check exec buffer
  assert((int)exec_buffer.size() <= exec_buffer_size);

  // Process completed instructions
  auto it = exec_buffer.begin();
  while (it != exec_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->execute_ready) {
      ExecutionStatus = Active;
      break;
    }

    // Stall if width has been reached
    if (instructions_processed > width) {
      // Update stall write
      uop->cycle_write_stall++;

      // Update pipeline stage status
      WriteStatus = Stall;

      count_stall_write++;

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

    // Sanity check write buffer
    assert((int)write_buffer.size() <= write_buffer_size);

    // Stall if the write buffer is full.
    if ((int)write_buffer.size() == write_buffer_size) {
      // Update stall write
      uop->cycle_write_stall++;

      // Update pipeline stage status
      WriteStatus = Stall;

      count_stall_write++;

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

    // Update Uop write ready cycle
    uop->write_ready = compute_unit->getTiming()->getCycle() + write_latency;

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
        "stg=\"bu-w\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to write buffer and get the iterator for the
    // next element
    write_buffer.push_back(std::move(*it));
    it = exec_buffer.erase(it);
  }
}

void BranchUnit::Execute() {
  // Get compute unit object
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check read buffer
  assert((int)read_buffer.size() <= read_buffer_size);

  // Process completed instructions
  auto it = read_buffer.begin();
  while (it != read_buffer.end()) {
    // Get Uop
    Uop* uop = it->get();

    // One more instruction processed
    instructions_processed++;

    // Break if uop is not ready
    if (compute_unit->getTiming()->getCycle() < uop->read_ready) {
      ReadStatus = Active;
      break;
    }

    // Stall if width has been reached
    if (instructions_processed > width) {
      // Update stall execution
      uop->cycle_execute_stall++;

      // Update pipeline stage status
      ExecutionStatus = Stall;

      count_stall_execution++;

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
    assert((int)exec_buffer.size() <= exec_buffer_size);

    // Stall if the exec buffer is full.
    if ((int)exec_buffer.size() == exec_buffer_size) {
      // Update stall execution
      uop->cycle_execute_stall++;

      // Update pipeline stage status
      ExecutionStatus = Stall;

      count_stall_execution++;

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

    // Update Uop exec ready cycle
    uop->execute_ready = compute_unit->getTiming()->getCycle() + exec_latency;

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
        "stg=\"bu-e\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to exec buffer and get the iterator for the
    // next element
    exec_buffer.push_back(std::move(*it));
    it = read_buffer.erase(it);
  }
}

void BranchUnit::Read() {
  // Get compute unit object
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check decode buffer
  assert((int)decode_buffer.size() <= decode_buffer_size);

  // Process completed instructions
  auto it = decode_buffer.begin();
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
      // Update uop stall read
      uop->cycle_read_stall++;

      // Update pipeline stage status
      ReadStatus = Stall;

      count_stall_read++;

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

    // Sanity check the read buffer
    assert((int)read_buffer.size() <= read_buffer_size);

    // Stall if the read buffer is full.
    if ((int)read_buffer.size() == read_buffer_size) {
      // Update uop stall
      uop->cycle_read_stall++;

      // Update pipeline stage status
      ReadStatus = Stall;

      count_stall_read++;

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

    // Update Uop read ready cycle
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
        "stg=\"bu-r\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to read buffer and get the iterator for the next
    // element
    read_buffer.push_back(std::move(*it));
    it = decode_buffer.erase(it);
  }
}

void BranchUnit::Decode() {
  // Get compute unit object
  ComputeUnit* compute_unit = getComputeUnit();

  // Internal counter
  int instructions_processed = 0;

  // Sanity check exec buffer
  assert((int)issue_buffer.size() <= issue_buffer_size);

  // Process completed instructions
  auto it = issue_buffer.begin();
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
      // Update uop stall decode
      uop->cycle_decode_stall++;

      // Update pipeline stage status
      DecodeStatus = Stall;

      count_stall_decode++;

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
    assert((int)decode_buffer.size() <= decode_buffer_size);

    // Stall if the decode buffer is full.
    if ((int)decode_buffer.size() == decode_buffer_size) {
      // Update uop stall decode
      uop->cycle_decode_stall++;

      // Update pipeline stage status
      DecodeStatus = Stall;

      count_stall_decode++;

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

    // Update Uop write ready cycle
    uop->decode_ready = compute_unit->getTiming()->getCycle() + decode_latency;

    // Update uop cycle
    uop->cycle_decode_begin = uop->issue_ready;
    uop->cycle_decode_active = compute_unit->getTiming()->getCycle();

    // Update pipeline stage status
    DecodeStatus = Active;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"bu-d\"\n",
        uop->getIdInComputeUnit(), compute_unit->getIndex(),
        uop->getWavefront()->getId(), uop->getIdInWavefront());

    // Move uop to write buffer
    decode_buffer.push_back(std::move(*it));
    it = issue_buffer.erase(it);
  }
}

}  // SI Namespace
