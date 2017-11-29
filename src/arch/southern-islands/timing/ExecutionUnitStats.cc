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
#include "Uop.h"

namespace SI {

void ExecutionUnitStats::Update(Uop *uop, long long cycle) {
  len_inst_sum_ += uop->cycle_length;

  auto min = std::min(len_inst_min_, uop->cycle_length);
  len_inst_min_ = len_inst_min_ == 0 ? uop->cycle_length : min;
  len_inst_max_ = std::max(len_inst_max_, uop->cycle_length);
}
}
