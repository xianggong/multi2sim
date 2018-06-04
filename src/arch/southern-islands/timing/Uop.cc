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

#include <arch/southern-islands/emulator/NDRange.h>
#include <arch/southern-islands/emulator/Wavefront.h>
#include <arch/southern-islands/emulator/WorkGroup.h>

#include "ComputeUnit.h"
#include "Timing.h"
#include "Uop.h"
#include "WavefrontPool.h"

namespace SI {

long long Uop::id_counter = 0;

Uop::Uop(Wavefront* wavefront, WavefrontPoolEntry* wavefront_pool_entry,
         long long cycle_created, WorkGroup* work_group, int wavefront_pool_id,
         unsigned ndrange_id)
    : wavefront(wavefront),
      wavefront_pool_entry(wavefront_pool_entry),
      work_group(work_group),
      wavefront_pool_id(wavefront_pool_id),
      ndrange_id((ndrange_id)) {
  // Assign unique identifier
  id = ++id_counter;
  id_in_wavefront = wavefront->getUopId();
  compute_unit = wavefront_pool_entry->getWavefrontPool()->getComputeUnit();
  id_in_compute_unit = compute_unit->getUopId();

  // Allocate room for the work-item info structures
  work_item_info_list.resize(WorkGroup::WavefrontSize);

  // Update info if statistics enables
  if (Timing::statistics_level >= 2) {
    auto gpu = compute_unit->getGpu();

    // NDRange
    auto ndrange_stats = gpu->getNDRangeStatsById(ndrange_id);
    if (ndrange_stats) {
      ndrange_stats->setCycle(Timing::getInstance()->getCycle(), EVENT_START);
    }

    // Workgroup
    auto workgroup_stats =
        compute_unit->getWorkgroupStatsById(work_group->id_in_compute_unit);
    if (workgroup_stats) {
      workgroup_stats->setCycle(Timing::getInstance()->getCycle(), EVENT_START);
    }

    // Wavefront
    auto wavefront_stats =
        compute_unit->getWavefrontStatsById(wavefront->id_in_compute_unit);
    if (wavefront_stats) {
      wavefront_stats->setCycle(Timing::getInstance()->getCycle(), EVENT_START);
    }
  }
}

Uop::~Uop() {}

std::string Uop::getLifeCycleInCSV(const char* execunit) {
  // Sanity check
  bool isValid = true;
  isValid |= cycle_start <= cycle_finish;
  isValid |= cycle_length == cycle_finish - cycle_start;
  isValid |= cycle_fetch_begin + cycle_fetch_stall == cycle_fetch_active;
  isValid |= cycle_fetch_active < cycle_issue_begin;
  isValid |= cycle_issue_begin + cycle_issue_stall == cycle_issue_active;
  isValid |= cycle_issue_active < cycle_decode_begin;
  isValid |= cycle_decode_begin + cycle_decode_stall == cycle_decode_active;
  isValid |= cycle_decode_active < cycle_read_begin;
  isValid |= cycle_read_begin + cycle_read_stall == cycle_read_active;
  isValid |= cycle_read_active < cycle_execute_begin;
  isValid |= cycle_execute_begin + cycle_execute_stall == cycle_execute_active;
  isValid |= cycle_execute_active < cycle_write_begin;
  isValid |= cycle_write_begin + cycle_write_stall == cycle_write_active;
  isValid |= cycle_write_active < cycle_finish;

  std::stringstream ss;
  if (isValid) {
    ss << misc::fmt(
              "%lld|%lld|%lld|"          // Instruction st/fn/len
              "%lld|%lld|%lld|%lld|"     // Fetch begin/stall/active/end
              "%lld|%lld|%lld|%lld|"     // Issue begin/stall/active/end
              "%lld|%lld|%lld|%lld|"     // Decode begin/stall/active/end
              "%lld|%lld|%lld|%lld|"     // Read begin/stall/active/end
              "%lld|%lld|%lld|%lld|"     // Execute begin/stall/active/end
              "%lld|%lld|%lld|%lld|"     // Write begin/stall/active/end
              "%lld|%lld|%d|%d|"         // GUID/ID/CU/IB/
              "%d|%d|%lld|"              // WF/WG/UOP
              "\"%s\"|\"%s\"|",          // Exec unit/Inst type
              cycle_start,               // Instruction start
              cycle_finish,              // Instruction finish
              cycle_length,              // Instruction length
              cycle_fetch_begin,         // Fetch begin
              cycle_fetch_stall,         // Fetch stall
              cycle_fetch_active,        // Fetch active
              cycle_issue_begin,         // Fetch end
              cycle_issue_begin,         // Issue begin
              cycle_issue_stall,         // Issue stall
              cycle_issue_active,        // Issue active
              cycle_decode_begin,        // Issue end
              cycle_decode_begin,        // Decode begin
              cycle_decode_stall,        // Decode stall
              cycle_decode_active,       // Decode active
              cycle_read_begin,          // Decode end
              cycle_read_begin,          // Read begin
              cycle_read_stall,          // Read stall
              cycle_read_active,         // Read active
              cycle_execute_begin,       // Read end
              cycle_execute_begin,       // execute begin
              cycle_execute_stall,       // execute stall
              cycle_execute_active,      // execute active
              cycle_write_begin,         // execute end
              cycle_write_begin,         // write begin
              cycle_write_stall,         // write stall
              cycle_write_active,        // write active
              cycle_finish,              // write end
              id,                        // GUID
              getIdInComputeUnit(),      // ID in CU
              compute_unit->getIndex(),  // CU ID
              getWavefrontPoolId(),      // IB ID
              getWavefront()->getId(),   // WF ID
              getWorkGroup()->getId(),   // WG ID
              getIdInWavefront(),        // UOP ID
              execunit,                  // Execution Unit
              getInstruction()->getFormatString().c_str()  // Instruction type
              )
       << "\"" << *getInstruction() << "\"\n";
  } else {
    return std::string("invalid\n");
  }

  return ss.str();
}
}
