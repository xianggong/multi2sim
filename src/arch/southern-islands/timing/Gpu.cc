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

#include <arch/southern-islands/emulator/Emulator.h>
#include <arch/southern-islands/emulator/NDRange.h>

#include "Gpu.h"
#include "Timing.h"

namespace SI {

// Static variables
int Gpu::num_compute_units = 32;
long long Gpu::max_cycles = 0;

// String map of the argument's access type
const misc::StringMap Gpu::register_allocation_granularity_map = {
    {"Wavefront", RegisterAllocationWavefront},
    {"WorkGroup", RegisterAllocationWorkGroup}};

Gpu::Gpu() {
  // Create MMU
  mmu = misc::new_unique<mem::Mmu>("Southern Islands");

  // Create compute units
  compute_units.reserve(num_compute_units);
  for (int i = 0; i < num_compute_units; i++) {
    // Add new compute unit to the overall list of compute units
    compute_units.emplace_back(misc::new_unique<ComputeUnit>(i, this));

    // Add compute units to the list of available compute units
    ComputeUnit* compute_unit = compute_units.back().get();
    InsertInAvailableComputeUnits(compute_unit);
  }

  if (Timing::statistics_level >= 1) {
    ndrange_stats_file.setPath("cu_all.ndrange");
    ndrange_stats_file << "ndrange_id,len_map,clk_map,clk_unmap,len_uop,clk_"
                          "uop_begin,clk_uop_end\n";
  }
}

ComputeUnit* Gpu::getAvailableComputeUnit() {
  if (available_compute_units.empty())
    return nullptr;
  else
    return available_compute_units.front();
}

void Gpu::InsertInAvailableComputeUnits(ComputeUnit* compute_unit) {
  // Sanity
  assert(!compute_unit->in_available_compute_units);

  // Insert compute_unit
  compute_unit->in_available_compute_units = true;
  compute_unit->available_compute_units_iterator =
      available_compute_units.insert(available_compute_units.end(),
                                     compute_unit);
}

void Gpu::RemoveFromAvailableComputeUnits(ComputeUnit* compute_unit) {
  // Sanity
  assert(compute_unit->in_available_compute_units);

  // Remove context
  available_compute_units.erase(compute_unit->available_compute_units_iterator);
  compute_unit->in_available_compute_units = false;
  compute_unit->available_compute_units_iterator =
      available_compute_units.end();
}

void Gpu::MapNDRange(NDRange* ndrange) {
  // Check that at least one work-group can be allocated per
  // wavefront pool
  Gpu::CalcGetWorkGroupsPerWavefrontPool(
      ndrange->getLocalSize1D(), ndrange->getNumVgprUsed(),
      ndrange->getNumSgprUsed(), ndrange->getLocalMemTop());

  // Make sure the number of work groups per wavefront pool is non-zero
  if (!work_groups_per_wavefront_pool) {
    throw Timing::Error(
        misc::fmt("work-group resources cannot be allocated to a compute "
                  "unit.\n\tA compute unit in the GPU has a limit in "
                  "number of wavefronts, number\n\tof registers, and "
                  "amount of local memory. If the work-group size\n"
                  "\texceeds any of these limits, the ND-Range cannot "
                  "be executed.\n"));
  }

  // Calculate limit of work groups per compute unit
  work_groups_per_compute_unit =
      work_groups_per_wavefront_pool * ComputeUnit::num_wavefront_pools;
  Emulator::scheduler_debug << misc::fmt("Hardware limit: %d WG per CU\n",
                                         work_groups_per_compute_unit);

  // Limit work groups per compute unit from environment variable
  char* env = getenv("M2S_WG_LIMIT");
  if (env) {
    int wg_limit = atoi(env);
    if (wg_limit > work_groups_per_compute_unit) {
      Emulator::scheduler_debug
          << misc::fmt("Manual limit > Hardware limit, aborting...\n");
      exit(-1);
    }
    work_groups_per_compute_unit = wg_limit;
    Emulator::scheduler_debug << misc::fmt("Manual limit: %d WG per CU\n",
                                           work_groups_per_compute_unit);
  }

  assert(work_groups_per_wavefront_pool <=
         ComputeUnit::max_work_groups_per_wavefront_pool);
  // Debug info
  Emulator::scheduler_debug << misc::fmt(
      "NDRange %d calculations:\n"
      "\t%d work group per wavefront pool\n"
      "\t%d work group slot per compute unit\n",
      ndrange->getId(), work_groups_per_wavefront_pool,
      work_groups_per_compute_unit);

  // Map ndrange
  mapped_ndrange = ndrange;

  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    auto stats = addNDRangeStats(ndrange->getId());
    if (stats) {
      stats->setCycle(Timing::getInstance()->getCycle(), EVENT_MAPPED);
    }
  }
}

void Gpu::UnmapNDRange(NDRange* ndrange) {
  // Unmap NDRange
  mapped_ndrange = nullptr;

  // Erase every workgroup in each compute unit, setting the
  // work_groups size to 0
  for (auto& compute_unit : compute_units) compute_unit->Reset();

  // Update info if statistics enables
  if (Timing::statistics_level >= 1) {
    auto stats = getNDRangeStatsById(ndrange->getId());
    if (stats) {
      stats->setCycle(Timing::getInstance()->getCycle(), EVENT_UNMAPPED);

      // Dump
      ndrange_stats_file << ndrange->getKernelName() << "_"
                         << std::to_string(ndrange->getId()) << "," << *stats;
    }
  }
}

void Gpu::CalcGetWorkGroupsPerWavefrontPool(int work_items_per_work_group,
                                            int vector_registers_per_work_item,
                                            int scalar_registers_per_wavefront,
                                            int local_memory_per_work_group) {
  // Get maximum number of work-groups per SIMD as limited by the
  // maximum number of wavefronts, given the number of wavefronts per
  // work-group in the NDRange
  assert(WorkGroup::WavefrontSize > 0);
  int wavefronts_per_work_group =
      (work_items_per_work_group + WorkGroup::WavefrontSize - 1) /
      WorkGroup::WavefrontSize;
  int max_work_groups_limited_by_max_wavefronts =
      ComputeUnit::max_wavefronts_per_wavefront_pool /
      wavefronts_per_work_group;

  Emulator::scheduler_debug << misc::fmt("work_items_per_work_group: %d\n",
                                         work_items_per_work_group);
  Emulator::scheduler_debug << misc::fmt("wavefronts_per_work_group: %d\n",
                                         wavefronts_per_work_group);

  // Get maximum number of work-groups per SIMD as limited by the number
  // of available registers, given the number of registers used per
  // work-item.
  int vector_registers_per_work_group;
  int scalar_registers_per_work_group;
  if (register_allocation_granularity == RegisterAllocationWavefront) {
    vector_registers_per_work_group =
        misc::RoundUp(vector_registers_per_work_item * WorkGroup::WavefrontSize,
                      ComputeUnit::register_allocation_size) *
        wavefronts_per_work_group;
    scalar_registers_per_work_group =
        scalar_registers_per_wavefront * wavefronts_per_work_group;
  } else {
    vector_registers_per_work_group = misc::RoundUp(
        vector_registers_per_work_item * work_items_per_work_group,
        ComputeUnit::register_allocation_size);
    scalar_registers_per_work_group =
        scalar_registers_per_wavefront * wavefronts_per_work_group;
  }
  Emulator::scheduler_debug << misc::fmt("vector_registers_per_work_item: %d\n",
                                         vector_registers_per_work_item);
  Emulator::scheduler_debug << misc::fmt("scalar_registers_per_wavefront: %d\n",
                                         scalar_registers_per_wavefront);
  Emulator::scheduler_debug << misc::fmt(
      "vector_registers_per_work_group: %d\n", vector_registers_per_work_group);

  Emulator::scheduler_debug << misc::fmt(
      "scalar_registers_per_work_group: %d\n", scalar_registers_per_work_group);

  int max_work_groups_limited_by_num_vector_registers =
      vector_registers_per_work_group
          ? ComputeUnit::num_vector_registers / vector_registers_per_work_group
          : ComputeUnit::max_work_groups_per_wavefront_pool;

  int max_work_groups_limited_by_num_scalar_registers =
      scalar_registers_per_work_group
          ? ComputeUnit::num_scalar_registers / scalar_registers_per_work_group
          : ComputeUnit::max_work_groups_per_wavefront_pool;

  int max_work_groups_limited_by_num_registers =
      std::min(max_work_groups_limited_by_num_scalar_registers,
               max_work_groups_limited_by_num_vector_registers);

  Emulator::scheduler_debug << misc::fmt("local_memory_per_work_group: %d\n",
                                         local_memory_per_work_group);
  // Get maximum number of work-groups per SIMD as limited by the
  // amount of available local memory, given the local memory used
  // by each work-group in the NDRange
  local_memory_per_work_group =
      misc::RoundUp(local_memory_per_work_group, ComputeUnit::lds_alloc_size);
  int max_work_groups_limited_by_local_memory =
      local_memory_per_work_group
          ? ComputeUnit::lds_size / local_memory_per_work_group
          : ComputeUnit::max_work_groups_per_wavefront_pool;

  Emulator::scheduler_debug << misc::fmt("local_memory_per_work_group: %d\n",
                                         local_memory_per_work_group);

  // Based on the limits above, calculate the actual limit of work-groups
  // per SIMD.
  work_groups_per_wavefront_pool =
      ComputeUnit::max_work_groups_per_wavefront_pool;
  work_groups_per_wavefront_pool =
      std::min(work_groups_per_wavefront_pool,
               max_work_groups_limited_by_max_wavefronts);
  work_groups_per_wavefront_pool = std::min(
      work_groups_per_wavefront_pool, max_work_groups_limited_by_num_registers);
  work_groups_per_wavefront_pool = std::min(
      work_groups_per_wavefront_pool, max_work_groups_limited_by_local_memory);

  Emulator::scheduler_debug
      << misc::fmt("max_work_groups_limited_by_max_wavefronts: %d\n",
                   max_work_groups_limited_by_max_wavefronts);
  Emulator::scheduler_debug
      << misc::fmt("max_work_groups_limited_by_num_scalar_registers: %d\n",
                   max_work_groups_limited_by_num_scalar_registers);
  Emulator::scheduler_debug
      << misc::fmt("max_work_groups_limited_by_num_vector_registers: %d\n",
                   max_work_groups_limited_by_num_vector_registers);
  Emulator::scheduler_debug
      << misc::fmt("max_work_groups_limited_by_num_registers: %d\n",
                   max_work_groups_limited_by_num_registers);
  Emulator::scheduler_debug
      << misc::fmt("max_work_groups_limited_by_local_memory: %d\n",
                   max_work_groups_limited_by_local_memory);

  if (work_groups_per_wavefront_pool ==
      max_work_groups_limited_by_max_wavefronts) {
    Emulator::scheduler_debug << "WG is limited by max wavefronts\n";
  } else if (work_groups_per_wavefront_pool ==
             max_work_groups_limited_by_local_memory) {
    Emulator::scheduler_debug << "WG is limited by local memory\n";
  } else if (work_groups_per_wavefront_pool ==
             max_work_groups_limited_by_num_registers) {
    Emulator::scheduler_debug << "WG is limited by number of registers\n";
  }
}

void Gpu::Run() {
  // Advance one cycle in each compute unit
  if (getenv("M2S_RANDOM_CU")) {
    auto timing = Timing::getInstance();
    int start_cu = timing->getCycle() % num_compute_units;

    for (int i = 0; i < num_compute_units; ++i) {
      int index = (i + start_cu) % num_compute_units;
      compute_units[index]->Run();
    }
  } else {
    for (auto& compute_unit : compute_units) compute_unit->Run();
  }
}
}
