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

#include <arch/southern-islands/disassembler/Instruction.h>
#include <arch/southern-islands/emulator/Emulator.h>
#include <arch/southern-islands/emulator/NDRange.h>
#include <arch/southern-islands/emulator/Wavefront.h>
#include <arch/southern-islands/emulator/WorkGroup.h>
#include <memory/Module.h>

#include "ComputeUnit.h"
#include "ComputeUnitStatistics.h"
#include "Timing.h"
#include "WavefrontPool.h"

namespace SI {

int ComputeUnit::num_wavefront_pools = 4;
int ComputeUnit::max_work_groups_per_wavefront_pool = 10;
int ComputeUnit::max_wavefronts_per_wavefront_pool = 10;
int ComputeUnit::fetch_latency = 1;
int ComputeUnit::fetch_width = 1;
int ComputeUnit::fetch_buffer_size = 10;
int ComputeUnit::issue_latency = 1;
int ComputeUnit::issue_width = 5;
int ComputeUnit::max_instructions_issued_per_type = 1;
int ComputeUnit::lds_size = 65536;
int ComputeUnit::lds_alloc_size = 64;
int ComputeUnit::lds_latency = 2;
int ComputeUnit::lds_block_size = 64;
int ComputeUnit::lds_num_ports = 2;
unsigned ComputeUnit::register_allocation_size = 32;
int ComputeUnit::num_scalar_registers = 2048;
int ComputeUnit::num_vector_registers = 65536;
long long ComputeUnit::cycle_map_first_wg = 0;

ComputeUnit::ComputeUnit(int index, Gpu* gpu)
    : gpu(gpu),
      index(index),
      scalar_unit(this),
      branch_unit(this),
      lds_unit(this),
      vector_memory_unit(this) {
  // Create the Lds module
  lds_module = misc::new_unique<mem::Module>(
      misc::fmt("LDS[%d]", index), mem::Module::TypeLocalMemory, lds_num_ports,
      lds_block_size, lds_latency);

  // Create wavefront pools, and SIMD units
  wavefront_pools.resize(num_wavefront_pools);
  fetch_buffers.resize(num_wavefront_pools);
  simd_units.resize(num_wavefront_pools);
  for (int i = 0; i < num_wavefront_pools; i++) {
    wavefront_pools[i] = misc::new_unique<WavefrontPool>(i, this);
    fetch_buffers[i] = misc::new_unique<FetchBuffer>(i, this);
    simd_units[i] = misc::new_unique<SimdUnit>(this, i);
  }

  // Create statistics file
  if (Timing::statistics_level >= 1) {
    std::string cu_id = std::to_string(getIndex());

    // Workgroup
    std::string workgroup_stats_filename = "cu_" + cu_id + ".workgp";
    workgroup_stats.setPath(workgroup_stats_filename);
    workgroup_stats << "ndrange_id,wg_id,len_map,clk_map,clk_unmap,len_uop,clk_"
                       "uop_begin,clk_uop_end,num_stall_issue,num_stall_decode,"
                       "num_stall_read,num_stall_execution,num_stall_write,"
                       "brch_num_stall_issue,brch_num_stall_decode,brch_num_"
                       "stall_read,brch_num_stall_execution,brch_num_stall_"
                       "write,lds_num_stall_issue,lds_num_stall_decode,lds_num_"
                       "stall_read,lds_num_stall_execution,lds_num_stall_write,"
                       "sclr_num_stall_issue,sclr_num_stall_decode,sclr_num_"
                       "stall_read,sclr_num_stall_execution,sclr_num_stall_"
                       "write,vmem_num_stall_issue,vmem_num_stall_decode,vmem_"
                       "num_stall_read,vmem_num_stall_execution,vmem_num_stall_"
                       "write,simd_num_stall_issue,simd_num_stall_decode,simd_"
                       "num_stall_read,simd_num_stall_execution,simd_num_stall_"
                       "write_\n";

    // Wavefront
    std::string wavefront_stats_filename = "cu_" + cu_id + ".waveft";

    wavefront_stats.setPath(wavefront_stats_filename);
    wavefront_stats << "ndrange_id,wg_id,wf_id,len_map,clk_map,clk_unmap,len_"
                       "uop,clk_uop_begin,clk_uop_end,num_stall_issue,num_"
                       "stall_decode,num_stall_read,num_stall_execution,num_"
                       "stall_write,brch_num_stall_issue,brch_num_stall_decode,"
                       "brch_num_stall_read,brch_num_stall_execution,brch_num_"
                       "stall_write,lds_num_stall_issue,lds_num_stall_decode,"
                       "lds_num_stall_read,lds_num_stall_execution,lds_num_"
                       "stall_write,sclr_num_stall_issue,sclr_num_stall_decode,"
                       "sclr_num_stall_read,sclr_num_stall_execution,sclr_num_"
                       "stall_write,vmem_num_stall_issue,vmem_num_stall_decode,"
                       "vmem_num_stall_read,vmem_num_stall_execution,vmem_num_"
                       "stall_write,simd_num_stall_issue,simd_num_stall_decode,"
                       "simd_num_stall_read,simd_num_stall_execution,simd_num_"
                       "stall_write_\n";
  }
}

void ComputeUnit::IssueToExecutionUnit(FetchBuffer* fetch_buffer,
                                       ExecutionUnit* execution_unit) {
  // Issue at most 'max_instructions_per_type'
  for (int num_issued_instructions = 0;
       num_issued_instructions < max_instructions_issued_per_type;
       num_issued_instructions++) {
    // Nothing if execution unit cannot absorb more instructions
    if (!execution_unit->canIssue()) break;

    // Find oldest uop
    auto oldest_uop_iterator = fetch_buffer->end();
    for (auto it = fetch_buffer->begin(), e = fetch_buffer->end(); it != e;
         ++it) {
      // Discard uop if it is not suitable for this execution
      // unit
      Uop* uop = it->get();
      if (!execution_unit->isValidUop(uop)) continue;

      // Skip uops that have not completed fetch
      if (timing->getCycle() < uop->fetch_ready) continue;

      // Save oldest uop
      if (oldest_uop_iterator == fetch_buffer->end() ||
          uop->getWavefront()->getId() <
              (*oldest_uop_iterator)->getWavefront()->getId())
        oldest_uop_iterator = it;
    }

    // Stop if no instruction found
    if (oldest_uop_iterator == fetch_buffer->end()) break;

    Uop* uop = oldest_uop_iterator->get();
    long long compute_unit_id = uop->getIdInComputeUnit();
    int wavefront_id = uop->getWavefront()->getId();
    long long id_in_wavefront = uop->getIdInWavefront();

    // Erase from fetch buffer, issue to execution unit
    execution_unit->Issue(std::move(*oldest_uop_iterator));
    fetch_buffer->Remove(oldest_uop_iterator);

    // Update uop info
    uop->cycle_issue_begin = uop->fetch_ready;
    uop->cycle_issue_active = timing->getCycle();

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"i\"\n",
        compute_unit_id, index, wavefront_id, id_in_wavefront);
  }
}

void ComputeUnit::Issue(FetchBuffer* fetch_buffer) {
  // Issue instructions to branch unit
  IssueToExecutionUnit(fetch_buffer, &branch_unit);

  // Issue instructions to scalar unit
  IssueToExecutionUnit(fetch_buffer, &scalar_unit);

  // Issue instructions to SIMD units
  for (int i = 0; i < num_wavefront_pools; ++i) {
    // Save timing simulator
    timing = Timing::getInstance();

    // Issue buffer chosen to issue this cycle
    int active_issue_buffer = timing->getCycle() % num_wavefront_pools;

    // Randomize simd unit
    int index = (i + active_issue_buffer) % num_wavefront_pools;
    IssueToExecutionUnit(fetch_buffer, simd_units[index].get());
  }

  // Issue instructions to vector memory unit
  IssueToExecutionUnit(fetch_buffer, &vector_memory_unit);

  // Issue instructions to LDS unit
  IssueToExecutionUnit(fetch_buffer, &lds_unit);

  // Update visualization states for all instructions not issued
  for (auto it = fetch_buffer->begin(), e = fetch_buffer->end(); it != e;
       ++it) {
    // Get Uop
    Uop* uop = it->get();

    // Skip uops that have not completed fetch
    if (timing->getCycle() < uop->fetch_ready) continue;

    // Update UOP info for m2svis
    uop->cycle_issue_stall++;

    if (Timing::statistics_level >= 2) {
      if (branch_unit.isValidUop(uop)) {
        branch_unit.getIntervalStats()->num_stall_issue_++;
        branch_unit.getOverviewStats()->num_stall_issue_++;
      } else if (scalar_unit.isValidUop(uop)) {
        scalar_unit.getIntervalStats()->num_stall_issue_++;
        scalar_unit.getOverviewStats()->num_stall_issue_++;
      } else if (vector_memory_unit.isValidUop(uop)) {
        vector_memory_unit.getIntervalStats()->num_stall_issue_++;
        vector_memory_unit.getOverviewStats()->num_stall_issue_++;
      } else if (lds_unit.isValidUop(uop)) {
        lds_unit.getIntervalStats()->num_stall_issue_++;
        lds_unit.getOverviewStats()->num_stall_issue_++;
      } else if (simd_units[0]->isValidUop(uop)) {
        for (auto& simd_unit : simd_units) {
          simd_unit.get()->getIntervalStats()->num_stall_issue_++;
          simd_unit.get()->getOverviewStats()->num_stall_issue_++;
        }
      }
    }

    // Update per WF/WG stats
    if (Timing::statistics_level >= 1) {
      // Per WF stats
      unsigned wf_id = uop->getWavefront()->getIdInComputeUnit();
      getWavefrontStatsById(wf_id)->num_stall_issue_++;

      // Per WG stats
      unsigned wg_id = uop->getWorkGroup()->getIdInComputeUnit();
      getWorkgroupStatsById(wg_id)->num_stall_issue_++;

      if (branch_unit.isValidUop(uop)) {
        getWavefrontStatsById(wf_id)->brch_num_stall_issue_++;
        getWorkgroupStatsById(wg_id)->brch_num_stall_issue_++;
      } else if (scalar_unit.isValidUop(uop)) {
        getWavefrontStatsById(wf_id)->sclr_num_stall_issue_++;
        getWorkgroupStatsById(wg_id)->sclr_num_stall_issue_++;
      } else if (vector_memory_unit.isValidUop(uop)) {
        getWavefrontStatsById(wf_id)->vmem_num_stall_issue_++;
        getWorkgroupStatsById(wg_id)->vmem_num_stall_issue_++;
      } else if (lds_unit.isValidUop(uop)) {
        getWavefrontStatsById(wf_id)->lds_num_stall_issue_++;
        getWorkgroupStatsById(wg_id)->lds_num_stall_issue_++;
      } else if (simd_units[0]->isValidUop(uop)) {
        getWavefrontStatsById(wf_id)->simd_num_stall_issue_++;
        getWorkgroupStatsById(wg_id)->simd_num_stall_issue_++;
      }
    }

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"s\"\n",
        uop->getIdInComputeUnit(), index, uop->getWavefront()->getId(),
        uop->getIdInWavefront());
  }
}

void ComputeUnit::Fetch(FetchBuffer* fetch_buffer,
                        WavefrontPool* wavefront_pool) {
  // Checks
  assert(fetch_buffer);
  assert(wavefront_pool);
  assert(fetch_buffer->getId() == wavefront_pool->getId());

  // Set up variables
  int instructions_processed = 0;

  // Fetch the instructions
  for (auto it = wavefront_pool->begin(), e = wavefront_pool->end(); it != e;
       ++it) {
    // Get wavefront pool entry
    WavefrontPoolEntry* wavefront_pool_entry = it->get();

    // Get wavefront
    Wavefront* wavefront = wavefront_pool_entry->getWavefront();

    // No waverfront
    if (!wavefront) continue;

    // Check wavefront
    assert(wavefront->getWavefrontPoolEntry());
    assert(wavefront->getWavefrontPoolEntry() == wavefront_pool_entry);

    // This should always be checked, regardless of how many
    // instructions have been fetched
    if (wavefront_pool_entry->ready_next_cycle) {
      wavefront_pool_entry->ready = true;
      wavefront_pool_entry->ready_next_cycle = false;
      continue;
    }

    // Only fetch a fixed number of instructions per cycle
    if (instructions_processed == fetch_width) continue;

    // Wavefront is not ready (previous instructions is still
    // in flight
    if (!wavefront_pool_entry->ready) continue;

    // If the wavefront finishes, there still may be outstanding
    // memory operations, so if the entry is marked finished
    // the wavefront must also be finished, but not vice-versa
    if (wavefront_pool_entry->wavefront_finished) {
      assert(wavefront->getFinished());
      continue;
    }

    // Wavefront is finished but other wavefronts from the
    // workgroup remain.  There may still be outstanding
    // memory operations, but no more instructions should
    // be fetched.
    if (wavefront->getFinished()) {
      continue;
    }

    // Wavefront is ready but waiting on outstanding
    // memory instructions
    if (wavefront_pool_entry->mem_wait) {
      // No outstanding accesses
      if (!wavefront_pool_entry->lgkm_cnt && !wavefront_pool_entry->exp_cnt &&
          !wavefront_pool_entry->vm_cnt) {
        wavefront_pool_entry->mem_wait = false;
        Timing::pipeline_debug << misc::fmt(
            "wg=%d/wf=%d "
            "Mem-wait:Done\n",
            wavefront->getWorkGroup()->getId(), wavefront->getId());
      } else {
        // TODO show a waiting state in Visualization
        // tool for the wait.
        Timing::pipeline_debug << misc::fmt(
            "wg=%d/wf=%d "
            "Waiting-Mem\n",
            wavefront->getWorkGroup()->getId(), wavefront->getId());
        continue;
      }
    }

    // Wavefront is ready but waiting at barrier
    if (wavefront_pool_entry->wait_for_barrier) continue;

    // Stall if fetch buffer is full
    assert(fetch_buffer->getSize() <= fetch_buffer_size);
    if (fetch_buffer->getSize() == fetch_buffer_size) continue;

    // Emulate instructions
    wavefront->Execute();
    wavefront_pool_entry->ready = false;

    // Create uop
    auto uop = misc::new_unique<Uop>(
        wavefront, wavefront_pool_entry, timing->getCycle(),
        wavefront->getWorkGroup(), fetch_buffer->getId(),
        wavefront->getWorkGroup()->getNDRange()->getId());
    uop->vector_memory_read = wavefront->vector_memory_read;
    uop->vector_memory_write = wavefront->vector_memory_write;
    uop->vector_memory_atomic = wavefront->vector_memory_atomic;
    uop->scalar_memory_read = wavefront->scalar_memory_read;
    uop->lds_read = wavefront->lds_read;
    uop->lds_write = wavefront->lds_write;
    uop->wavefront_last_instruction = wavefront->finished;
    uop->memory_wait = wavefront->memory_wait;
    uop->at_barrier = wavefront->isBarrierInstruction();
    uop->setInstruction(wavefront->getInstruction());
    uop->vector_memory_global_coherency =
        wavefront->vector_memory_global_coherency;

    // Checks
    assert(wavefront->getWorkGroup() && uop->getWorkGroup());

    // Convert instruction name to string
    if (Timing::trace || Timing::pipeline_debug) {
      std::string instruction_name = wavefront->getInstruction()->getName();
      misc::StringSingleSpaces(instruction_name);

      // Trace
      Timing::trace << misc::fmt(
          "si.new_inst "
          "id=%lld "
          "cu=%d "
          "ib=%d "
          "wf=%d "
          "uop_id=%lld "
          "stg=\"f\" "
          "asm=\"%s\"\n",
          uop->getIdInComputeUnit(), index, uop->getWavefrontPoolId(),
          uop->getWavefront()->getId(), uop->getIdInWavefront(),
          instruction_name.c_str());

      // Debug
      Timing::pipeline_debug << misc::fmt(
          "wg=%d/wf=%d cu=%d wfPool=%d "
          "inst=%lld asm=%s id_in_wf=%lld\n"
          "\tinst=%lld (Fetch)\n",
          uop->getWavefront()->getWorkGroup()->getId(),
          uop->getWavefront()->getId(), index, uop->getWavefrontPoolId(),
          uop->getId(), instruction_name.c_str(), uop->getIdInWavefront(),
          uop->getId());
    }

    // Update last memory accesses
    for (auto it = wavefront->getWorkItemsBegin(),
              e = wavefront->getWorkItemsEnd();
         it != e; ++it) {
      // Get work item
      WorkItem* work_item = it->get();

      // Get uop work item info
      Uop::WorkItemInfo* work_item_info;
      work_item_info = &uop->work_item_info_list[work_item->getIdInWavefront()];

      // Global memory
      work_item_info->global_memory_access_address =
          work_item->global_memory_access_address;
      work_item_info->global_memory_access_size =
          work_item->global_memory_access_size;

      // LDS
      work_item_info->lds_access_count = work_item->lds_access_count;
      for (int j = 0; j < work_item->lds_access_count; j++) {
        work_item_info->lds_access[j].type = work_item->lds_access[j].type;
        work_item_info->lds_access[j].addr = work_item->lds_access[j].addr;
        work_item_info->lds_access[j].size = work_item->lds_access[j].size;
      }
    }

    // Access instruction cache. Record the time when the
    // instruction will have been fetched, as per the latency
    // of the instruction memory.
    uop->fetch_ready = timing->getCycle() + fetch_latency;

    // Update UOP info for m2svis
    uop->cycle_start = timing->getCycle();
    uop->cycle_fetch_begin = timing->getCycle();
    uop->cycle_fetch_active = timing->getCycle();

    // Insert uop into fetch buffer
    uop->getWorkGroup()->inflight_instructions++;
    fetch_buffer->addUop(std::move(uop));

    instructions_processed++;
    stats.num_total_insts_++;
  }
}

static int uniform_distribution(int rangeLow, int rangeHigh);

static int uniform_distribution(int rangeLow, int rangeHigh) {
  int range =
      rangeHigh - rangeLow + 1;  //+1 makes it [rangeLow, rangeHigh], inclusive.
  int copies =
      RAND_MAX / range;  // we can fit n-copies of [0...range-1] into RAND_MAX
  // Use rejection sampling to avoid distribution errors
  int limit = range * copies;
  int myRand = -1;
  while (myRand < 0 || myRand >= limit) {
    myRand = rand();
  }
  return myRand / copies + rangeLow;  // note that this involves the high-bits
}

void ComputeUnit::SetInitialPC(WorkGroup* work_group) {
  auto ndrange = work_group->getNDRange();

  /* Get mix ratio */
  float ratio_val = 0.5f;
  char* ratio = getenv("M2S_MIX_RATIO");
  if (ratio) ratio_val = atof(ratio);

  /* Get mix pattern */
  int pattern_val = 0;
  char* pattern = getenv("M2S_MIX_PATTERN");
  if (pattern) pattern_val = atoi(pattern);

  /* Calculate number of active wavefront/workgroups per CU */
  int active_wg_per_cu = gpu->getWorkGroupsPerComputeUnit();
  int active_wf_per_cu = gpu->getWavefrontsPerComputeUnit();

  if (getIndex() == 0)
    Emulator::scheduler_debug
        << misc::fmt("Can run %d WF or %d WG at the same time\n",
                     active_wf_per_cu, active_wg_per_cu);

  int low;
  int high;
  int wf_stride;
  int r_val;
  int threshold;

  enum pattern { GT, LT, RD, RR };

  /* Intialize wavefront state */
  Wavefront* wavefront;
  for (auto wf_i = work_group->getWavefrontsBegin(),
            wf_e = work_group->getWavefrontsEnd();
       wf_i != wf_e; ++wf_i) {
    wavefront = (*wf_i).get();

    wavefront->setPC(0);
    int secondPC = ndrange->GetSecondPC();
    int wavefront_id = wavefront->getIdInComputeUnit() % active_wf_per_cu;

    switch (pattern_val) {
      case GT:  // Default pattern: greater than
        if (wavefront_id > static_cast<int>(active_wf_per_cu * ratio_val))
          wavefront->setPC(secondPC);
        break;
      case LT:  // Reverse pattern: less than
        if (wavefront_id < static_cast<int>(active_wf_per_cu * ratio_val))
          wavefront->setPC(secondPC);
        break;
      case RD:  // Random pattern: random
        low = 0;
        high = 100;
        threshold = static_cast<int>((high - low) * ratio_val);
        r_val = uniform_distribution(low, high);
        if (r_val <= threshold) {
          wavefront->setPC(secondPC);
        }
        break;
      case RR:  // Round-Robin
        wf_stride = static_cast<int>(active_wf_per_cu * ratio_val / 2);
        wf_stride = wf_stride < 1 ? 1 : wf_stride;
        Emulator::scheduler_debug << misc::fmt("wf_stride = %d\n", wf_stride);
        if ((wavefront->id_in_compute_unit / wf_stride) % 2)
          wavefront->setPC(secondPC);
        break;
      default:  // Default to greater than
        if (wavefront_id >= static_cast<int>(active_wf_per_cu * ratio_val))
          wavefront->setPC(secondPC);
        break;
    }

    // Only dump information from compute unit 0
    if (getIndex() == 0) {
      Emulator::scheduler_debug << misc::fmt(
          "PC of WF[%d|%d] in CU[%d] = %d, Pattern = %d, Mix ratio = %f, "
          "Threshold = %d\n",
          wavefront->getIdInComputeUnit(), wavefront_id, getIndex(),
          wavefront->getPC(), pattern_val, ratio_val,
          static_cast<int>(active_wf_per_cu * ratio_val));
    }
  }
}

void ComputeUnit::MapWorkGroup(WorkGroup* work_group) {
  // Checks
  assert(work_group);
  assert((int)work_groups.size() <= gpu->getWorkGroupsPerComputeUnit());
  assert(!work_group->id_in_compute_unit);

  // Find an available slot
  while (work_group->id_in_compute_unit < gpu->getWorkGroupsPerComputeUnit() &&
         (work_group->id_in_compute_unit < (int)work_groups.size()) &&
         (work_groups[work_group->id_in_compute_unit] != nullptr))
    work_group->id_in_compute_unit++;

  // Checks
  assert(work_group->id_in_compute_unit < gpu->getWorkGroupsPerComputeUnit());

  // Save timing simulator
  timing = Timing::getInstance();

  // Debug
  Emulator::scheduler_debug << misc::fmt(
      "@%lld available slot %d "
      "found in compute unit %d\n",
      timing->getCycle(), work_group->id_in_compute_unit, index);

  // Insert work group into the list
  AddWorkGroup(work_group);

  // Record the cycle when the first WG is mapped
  if (cycle_map_first_wg == 0) cycle_map_first_wg = timing->getCycle();

  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    auto stats = addWorkgroupStats(work_group->id_in_compute_unit);
    stats->setCycle(timing->getCycle(), EVENT_MAPPED);
  }

  // Checks
  assert((int)work_groups.size() <= gpu->getWorkGroupsPerComputeUnit());

  // If compute unit is not full, add it back to the available list
  if ((int)work_groups.size() < gpu->getWorkGroupsPerComputeUnit()) {
    if (!in_available_compute_units) gpu->InsertInAvailableComputeUnits(this);
  }

  // Assign wavefront identifiers in compute unit
  int wavefront_id = 0;
  for (auto it = work_group->getWavefrontsBegin();
       it != work_group->getWavefrontsEnd(); ++it) {
    // Get wavefront pointer
    Wavefront* wavefront = it->get();

    // Set wavefront Id
    wavefront->id_in_compute_unit = work_group->id_in_compute_unit *
                                        work_group->getWavefrontsInWorkgroup() +
                                    wavefront_id;

    // Update info if statistics enables
    if (Timing::statistics_level >= 1) {
      auto stats = addWavefrontStats(wavefront->id_in_compute_unit);
      stats->setCycle(timing->getCycle(), EVENT_MAPPED);
    }

    // Update internal counter
    wavefront_id++;
  }

  // Set wavefront pool for work group
  int wavefront_pool_id = work_group->id_in_compute_unit % num_wavefront_pools;
  work_group->wavefront_pool = wavefront_pools[wavefront_pool_id].get();

  // Check if the wavefronts in the work group can fit into the wavefront
  // pool
  assert((int)work_group->getWavefrontsInWorkgroup() <=
         max_wavefronts_per_wavefront_pool);

  // Insert wavefronts into an instruction buffer
  work_group->wavefront_pool->MapWavefronts(work_group);

  // Set PC for TwinKernel execution mode
  SetInitialPC(work_group);

  // Increment count of mapped work groups
  stats.num_mapped_work_groups_++;

  // Debug info
  Emulator::scheduler_debug << misc::fmt(
      "\t\tfirst wavefront=%d, "
      "count=%d\n"
      "\t\tfirst work-item=%d, count=%d\n",
      work_group->getWavefront(0)->getId(), work_group->getNumWavefronts(),
      work_group->getWorkItem(0)->getId(), work_group->getNumWorkItems());

  // Trace info
  Timing::trace << misc::fmt(
      "si.map_wg "
      "cu=%d "
      "wg=%d "
      "wi_first=%d "
      "wi_count=%d "
      "wf_first=%d "
      "wf_count=%d\n",
      index, work_group->getId(), work_group->getWorkItem(0)->getId(),
      work_group->getNumWorkItems(), work_group->getWavefront(0)->getId(),
      work_group->getNumWavefronts());
}

void ComputeUnit::AddWorkGroup(WorkGroup* work_group) {
  // Add a work group only if the id in compute unit is the id for a new
  // work group in the compute unit's list
  int index = work_group->id_in_compute_unit;
  if (index == (int)work_groups.size() &&
      (int)work_groups.size() < gpu->getWorkGroupsPerComputeUnit()) {
    work_groups.push_back(work_group);
  } else {
    // Make sure an entry is emptied up
    assert(work_groups[index] == nullptr);
    assert(work_groups.size() <= (unsigned)gpu->getWorkGroupsPerComputeUnit());

    // Set the new work group to the empty entry
    work_groups[index] = work_group;
  }

  // Checks
  assert(work_group->id_in_compute_unit == index);

  // Save iterator
  auto it = work_groups.begin();
  std::advance(it, work_group->getIdInComputeUnit());
  work_group->compute_unit_work_groups_iterator = it;

  // Debug info
  Emulator::scheduler_debug << misc::fmt(
      "\twork group %d "
      "added\n",
      work_group->getId());
}

void ComputeUnit::RemoveWorkGroup(WorkGroup* work_group) {
  // Debug info
  Emulator::scheduler_debug << misc::fmt(
      "@%lld work group %d "
      "removed from compute unit %d slot %d\n",
      timing->getCycle(), work_group->getId(), index,
      work_group->id_in_compute_unit);

  // Unmap work group from the compute unit
  assert(work_group->compute_unit_work_groups_iterator != work_groups.end());
  work_groups[work_group->id_in_compute_unit] = nullptr;
}

void ComputeUnit::Reset() {
  // If there are no work groups ever assigned to this compute unit
  // the size would be zero, so no reset is required.
  if (!work_groups.size()) return;

  // Debug info
  Emulator::scheduler_debug << misc::fmt(
      "@%lld compute unit %d "
      "reset\n",
      timing->getCycle(), index);

  // Reset the workgroups size to 0
  work_groups.resize(0);
}

void ComputeUnit::UnmapWorkGroup(WorkGroup* work_group) {
  // Get Gpu object
  Gpu* gpu = getGpu();

  // Add work group register access statistics to compute unit
  stats.num_sreg_reads_ += work_group->getSregReadCount();
  stats.num_sreg_writes_ += work_group->getSregWriteCount();
  stats.num_vreg_reads_ += work_group->getVregReadCount();
  stats.num_vreg_writes_ += work_group->getVregWriteCount();

  // Remove the work group from the list
  assert(work_groups.size() > 0);
  RemoveWorkGroup(work_group);

  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    auto stats = getWorkgroupStatsById(work_group->id_in_compute_unit);
    if (stats) {
      stats->setCycle(Timing::getInstance()->getCycle(), EVENT_UNMAPPED);
    }
    auto ndrange_id = work_group->getNDRange()->getId();
    auto workgroup_id = work_group->getId();

    workgroup_stats << ndrange_id << "," << workgroup_id << "," << *stats;

    // Clean up
    workgroup_stats_map.erase(work_group->id_in_compute_unit);
  }

  // Unmap wavefronts from instruction buffer
  work_group->wavefront_pool->UnmapWavefronts(work_group);

  // If compute unit is not already in the available list, place
  // it there. The vector list of work groups does not shrink,
  // when we unmap a workgroup.
  assert((int)work_groups.size() <= gpu->getWorkGroupsPerComputeUnit());
  if (!in_available_compute_units) gpu->InsertInAvailableComputeUnits(this);

  // Trace
  Timing::trace << misc::fmt("si.unmap_wg cu=%d wg=%d\n", index,
                             work_group->getId());

  // Remove the work group from the running work groups list
  NDRange* ndrange = work_group->getNDRange();
  ndrange->RemoveWorkGroup(work_group);
}

void ComputeUnit::UpdateFetchVisualization(FetchBuffer* fetch_buffer) {
  for (auto it = fetch_buffer->begin(), e = fetch_buffer->end(); it != e;
       ++it) {
    // Get uop
    Uop* uop = it->get();
    assert(uop);

    // Skip all uops that have not yet completed the fetch
    if (timing->getCycle() < uop->fetch_ready) break;

    uop->cycle_issue_stall++;

    // Trace
    Timing::trace << misc::fmt(
        "si.inst "
        "id=%lld "
        "cu=%d "
        "wf=%d "
        "uop_id=%lld "
        "stg=\"s\"\n",
        uop->getIdInComputeUnit(), index, uop->getWavefront()->getId(),
        uop->getIdInWavefront());
  }
}

void ComputeUnit::Run() {
  // Return if no work groups are mapped to this compute unit
  if (!work_groups.size()) return;

  // Save timing simulator
  timing = Timing::getInstance();

  // Issue buffer chosen to issue this cycle, round-robin
  int active_issue_buffer = timing->getCycle() % num_wavefront_pools;
  assert(active_issue_buffer >= 0 && active_issue_buffer < num_wavefront_pools);

  // Issue from fetch buffer with greatest pressure
  char* isFetchPressureScheduler = getenv("M2S_FP_SCHED");
  if (isFetchPressureScheduler) {
    int pressure = 0;
    for (unsigned i = 0; i < fetch_buffers.size(); ++i) {
      if (fetch_buffers[i]->getSize() > pressure) {
        active_issue_buffer = i;
        pressure = fetch_buffers[i]->getSize();
      }
    }
  }

  // SIMDs
  for (auto& simd_unit : simd_units) simd_unit->Run();

  // Vector memory
  vector_memory_unit.Run();

  // LDS unit
  lds_unit.Run();

  // Scalar unit
  scalar_unit.Run();

  // Branch unit
  branch_unit.Run();

  // Issue from the active issue buffer
  Issue(fetch_buffers[active_issue_buffer].get());

  // Update visualization in non-active issue buffers
  for (int i = 0; i < (int)simd_units.size(); i++) {
    if (i != active_issue_buffer) {
      UpdateFetchVisualization(fetch_buffers[i].get());
    }
  }

  // Fetch, randomized
  char* isRandom = getenv("M2S_RANDOM_FETCH");
  if (isRandom) {
    for (int i = 0; i < num_wavefront_pools; i++) {
      int index = (i + active_issue_buffer) % num_wavefront_pools;
      Fetch(fetch_buffers[index].get(), wavefront_pools[index].get());
    }
  } else {
    for (int i = 0; i < num_wavefront_pools; i++) {
      Fetch(fetch_buffers[i].get(), wavefront_pools[i].get());
    }
  }
}

void ComputeUnit::Dump(std::ostream& os) const {
  // Title
  std::string output_line = misc::fmt("Compute unit %d", index);
  os << output_line << '\n';
  os << std::string(output_line.length(), '=') << "\n\n";

  // Number of workgroups and their ids
  os << "Work group capacity = " << work_groups.size() << '\n';
  for (int i = 0; i < (int)work_groups.size(); i++) {
    // Get the work group
    WorkGroup* work_group = work_groups[i];

    // Dump the information
    output_line = misc::fmt("[%d] work group %d ", i, work_group->getId());
  }
  os << '\n';

  // Is it an available compute unit
  os << "Compute unit is available : "
     << (in_available_compute_units ? "True" : "False");
  os << '\n';
}

void ComputeUnit::FlushWorkgroupStats() {
  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    for (auto& stats : workgroup_stats_map) {
      auto workgroup_id = stats.first;
      auto& workgroup_st = stats.second;
      workgroup_st->setCycle(Timing::getInstance()->getCycle(), EVENT_UNMAPPED);
      workgroup_stats << "-1," << workgroup_id << "," << *workgroup_st;
      // Clean up
      workgroup_stats_map.erase(workgroup_id);
    }
  }
}

void ComputeUnit::FlushWavefrontStats() {
  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    for (auto& stats : wavefront_stats_map) {
      auto id = stats.first;
      auto& st = stats.second;
      st->setCycle(Timing::getInstance()->getCycle(), EVENT_UNMAPPED);
      wavefront_stats << "-1,-1," << id << "," << *st;
      // Clean up
      wavefront_stats_map.erase(id);
    }
  }
}

}  // Namespace SI
