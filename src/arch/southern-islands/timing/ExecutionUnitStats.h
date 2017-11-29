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

#ifndef SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_
#define SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_

#include <iostream>
#include <string>

namespace SI {

// Forward declaration
class Uop;

/// This class contains statistics for the timing simulation
class ExecutionUnitStats {
 public:
  double utiliation_ = 0.0f;

  long long num_inst_fetch_ = 0;
  long long num_inst_issue_ = 0;
  long long num_inst_write_ = 0;

  long long len_inst_min_ = 0;
  long long len_inst_max_ = 0;
  long long len_inst_avg_ = 0;
  long long len_inst_sum_ = 0;

 public:
  /// Member functions
  void Update(Uop* uop, long long cycle);

  /// Setters

  /// Dump statistics
  void Dump(std::ostream& os = std::cout) const;

  /// Same as Dump()
  friend std::ostream& operator<<(std::ostream& os,
                                  const ExecutionUnitStats& info) {
    info.Dump(os);
    return os;
  }
};
}

#endif  // SRC_ARCH_SOUTHERN_ISLANDS_TIMING_EXECITIONUNITSTATS_H_
