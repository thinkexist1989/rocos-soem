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

@Created on: 2024.03.31
@Last Modified: 2024.03.31
*/

#ifndef ROCOS_SOEM_ECAT_PROCESS_H
#define ROCOS_SOEM_ECAT_PROCESS_H

#include "ethercat.h"
#include <ecat_config_master.h>

#include <string>

class EcatProcess {
    using string = std::string;
public:
    EcatProcess();
    ~EcatProcess();

    void getSlaveInfo(string ifname, bool printSDO = false, bool printMAP = false);
    string dtype2string(uint16 dtype);
    string SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype);
    /** Read PDO assign structure */
    int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset);
    int si_map_sdo(int slave);
    int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset);
    int si_map_sii(int slave);
    void si_sdo(int cnt);

private:
    char IOmap[4096];
    ec_ODlistt ODlist;
    ec_OElistt OElist;
    boolean printSDO  {FALSE};
    boolean printMAP  {FALSE};
    char usdo[128];
    char hstr[1024];

    int cycle_us {0}; // cycle time in us

    EcatConfigMaster *pEcm  {nullptr};

    volatile bool bRun {true};

    OSAL_THREAD_HANDLE thread;

    int expectedWKC;
    boolean needlf;
    volatile int wkc;
    boolean inOP;
    uint8 currentgroup = 0;
    boolean forceByteAlignment = FALSE;


};


#endif //ROCOS_SOEM_ECAT_PROCESS_H
