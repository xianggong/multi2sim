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
#include "Uop.h"
#include "WavefrontPool.h"


namespace SI
{

long long Uop::id_counter = 0;


Uop::Uop(Wavefront *wavefront, WavefrontPoolEntry *wavefront_pool_entry,
		long long cycle_created,
		WorkGroup *work_group,
		int wavefront_pool_id) :
		wavefront(wavefront),
		wavefront_pool_entry(wavefront_pool_entry),
		work_group(work_group),
		wavefront_pool_id(wavefront_pool_id)
{
	// Assign unique identifier
	id = ++id_counter;
	id_in_wavefront = wavefront->getUopId();
	compute_unit = wavefront_pool_entry->getWavefrontPool()->getComputeUnit();
	id_in_compute_unit = compute_unit->getUopId();
	
	// Allocate room for the work-item info structures
	work_item_info_list.resize(WorkGroup::WavefrontSize);
}

std::string Uop::getLifeCycleInCSV(const char *execunit)
{
	std::stringstream ss;
	ss << misc::fmt(
		"%lld,%lld,%lld,"             // Instruction st/fn/len
		"%lld,%lld,%lld,%lld,"        // Fetch begin/stall/active/end
		"%lld,%lld,%lld,%lld,"        // Issue begin/stall/active/end
		"%lld,%lld,%lld,%lld,"        // Decode begin/stall/active/end
		"%lld,%lld,%lld,%lld,"        // Read begin/stall/active/end
		"%lld,%lld,%lld,%lld,"        // Execute begin/stall/active/end
		"%lld,%lld,%lld,%lld,"        // Write begin/stall/active/end

		"%lld,"                       // Complete stall

		"%lld,%lld,%d,%d,"            // GUID/ID/CU/IB/
		"%d,%d,%lld,"                 // WF/WG/UOP
		"%s,%d,",                     // Exec unit/Inst type

		cycle_start,                  // Instruction start
		cycle_finish,                 // Instruction finish
		cycle_length,                 // Instruction length

		cycle_fetch_begin,            // Fetch begin
		cycle_fetch_stall,            // Fetch stall
		cycle_fetch_active,           // Fetch active
		cycle_issue_begin,            // Fetch end

		cycle_issue_begin,            // Issue begin
		cycle_issue_stall,            // Issue stall
		cycle_issue_active,           // Issue active
		cycle_decode_begin,           // Issue end

		cycle_decode_begin,           // Decode begin
		cycle_decode_stall,           // Decode stall
		cycle_decode_active,          // Decode active
		cycle_read_begin,             // Decode end

		cycle_read_begin,             // Read begin
		cycle_read_stall,             // Read stall
		cycle_read_active,            // Read active
		cycle_execute_begin,          // Read end

		cycle_execute_begin,          // execute begin
		cycle_execute_stall,          // execute stall
		cycle_execute_active,         // execute active
		cycle_write_begin,            // execute end

		cycle_write_begin,            // write begin
		cycle_write_stall,            // write stall
		cycle_write_active,           // write active
		cycle_finish,                 // write end

		cycle_complete_stall,         // complete stall

		id,                           // GUID
		getIdInComputeUnit(),         // ID in CU
		compute_unit->getIndex(),     // CU ID
		getWavefrontPoolId(),         // IB ID
		getWavefront()->getId(),      // WF ID
		getWorkGroup()->getId(),      // WG ID
		getIdInWavefront(),           // UOP ID
		execunit,                     // Execution Unit
		getInstruction()->getFormat() // Instruction type
		) << *getInstruction() << "\n";
	
	return ss.str();
}

}

