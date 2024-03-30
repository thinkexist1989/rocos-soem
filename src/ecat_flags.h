/*
Copyright 2021, Yang Luo"
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@Author
Yang Luo, PHD
@email: yluo@hit.edu.cn

@Created on: 2021.11.29
@Last Modified: 2024.03.30
*/
#ifndef EC_FLAGS_H
#define EC_FLAGS_H

#include "gflags/gflags.h"

//! @brief Ec-Master ID (Multi-Master can be used)
DECLARE_int32(id);
//! @brief Bus cycle time in usec. Default is 1000
DECLARE_int32(cycle);
//! @brief Verbosity level
DECLARE_int32(verbose);
//! @brief Which CPU the Ec-Master use
DECLARE_int32(cpuidx);
//! @brief Measurement in us for all EtherCAT jobs
DECLARE_bool(perf);
//! @brief Intel network card instances and mode
DECLARE_string(instance);
//! @brief DC mode
DECLARE_int32(dcmmode);
//! @brief The request state of EtherCAT slaves
DECLARE_string(state);
//! @brief license
DECLARE_string(license);

#endif // EC_FLAGS_H