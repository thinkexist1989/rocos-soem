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
Shenyang Institute of Automation, Chinese Academy of Sciences.
 email: luoyang@sia.cn

@Created on: 2021.11.29
*/

//#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <test/doctest.h>

#define private public
#define protected public

#include <ecat_config_master.h>
#include <ecat_config.h>
#include <iostream>

TEST_CASE("csv_slave_info") {
    auto ecatConfig = rocos::EcatConfig::getInstance(0);

    std::cout << "Slave count: " << ecatConfig->ecatBus->slave_num << std::endl;

    std::cout << "Authorized: " << ecatConfig->isAuthorized() << std::endl;

    std::cout << "Min cycle time: " << ecatConfig->getBusMinCycleTime() << std::endl;
    std::cout << "Max cycle time: " << ecatConfig->getBusMaxCycleTime() << std::endl;
    std::cout << "Avg cycle time: " << ecatConfig->getBusAvgCycleTime() << std::endl;
    std::cout << "Curr cycle time: " << ecatConfig->getBusCurrentCycleTime() << std::endl;

    std::cout << ecatConfig->ecatBus->slaves[0].input_vars[0].name << std::endl;

    for(int i = 0; i < ecatConfig->ecatBus->slave_num; i++) {
        std::cout << "---------------------------------------------------------------" << std::endl;
        std::cout << "Slave name: " << ecatConfig->ecatBus->slaves[i].name << std::endl;
        std::cout << "  -input_count  : " << ecatConfig->ecatBus->slaves[i].input_var_num << std::endl;
        std::cout << "    -status_word: " << ecatConfig->getSlaveInputVarValueByName<uint16_t>(i, "Statusword") << std::endl;
        std::cout << "    -pos_act_val: " << ecatConfig->getSlaveInputVarValueByName<int32_t>(i, "Position actual value") << std::endl;
        std::cout << "    -vel_dem_val: " << ecatConfig->getSlaveInputVarValueByName<int32_t>(i, "Velocity demand value") << std::endl;
        std::cout << "    -tor_dem_val: " << ecatConfig->getSlaveInputVarValueByName<int16_t>(i, "Torque demand") << std::endl;
        std::cout << "  -output_count : " << ecatConfig->ecatBus->slaves[i].output_var_num << std::endl;
        std::cout << "    -ctrl_word  : " << ecatConfig->getSlaveOutputVarValueByName<uint16_t>(i, "Controlword") << std::endl;
        std::cout << "    -tar_vel    : " << ecatConfig->getSlaveOutputVarValueByName<int32_t>(i, "Target velocity") << std::endl;
    }

}

TEST_CASE("csv_elmox1") {
    auto ecatConfig = rocos::EcatConfig::getInstance(0);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 128);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 0);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 6);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 7);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 15);

    usleep(200000);

    int targetVel = 100000;

    for(int i = 0; i < 5; i++) {

        targetVel = -targetVel;
        ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target velocity", targetVel);


        usleep(2000000);

    }

    ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target velocity", 0);
}


TEST_CASE("csp_slave_info") {
    auto ecatConfig = rocos::EcatConfig::getInstance(0);

    std::cout << "Slave count: " << ecatConfig->ecatBus->slave_num << std::endl;

    std::cout << "Authorized: " << ecatConfig->isAuthorized() << std::endl;

    std::cout << "Min cycle time: " << ecatConfig->getBusMinCycleTime() << std::endl;
    std::cout << "Max cycle time: " << ecatConfig->getBusMaxCycleTime() << std::endl;
    std::cout << "Avg cycle time: " << ecatConfig->getBusAvgCycleTime() << std::endl;
    std::cout << "Curr cycle time: " << ecatConfig->getBusCurrentCycleTime() << std::endl;

    std::cout << ecatConfig->ecatBus->slaves[0].input_vars[0].name << std::endl;

    for(int i = 0; i < ecatConfig->ecatBus->slave_num; i++) {
        std::cout << "---------------------------------------------------------------" << std::endl;
        std::cout << "Slave name: " << ecatConfig->ecatBus->slaves[i].name << std::endl;
        std::cout << "  -input_count  : " << ecatConfig->ecatBus->slaves[i].input_var_num << std::endl;
        std::cout << "    -status_word: " << ecatConfig->getSlaveInputVarValueByName<uint16_t>(i, "Statusword") << std::endl;
        std::cout << "    -pos_act_val: " << ecatConfig->getSlaveInputVarValueByName<int32_t>(i, "Position actual value") << std::endl;
        std::cout << "    -dig_input  : " << std::hex << ecatConfig->getSlaveInputVarValueByName<uint32_t>(i, "Digital inputs") << std::dec << std::endl;
        std::cout << "  -output_count : " << ecatConfig->ecatBus->slaves[i].output_var_num << std::endl;
        std::cout << "    -ctrl_word  : " << ecatConfig->getSlaveOutputVarValueByName<uint16_t>(i, "Controlword") << std::endl;
        std::cout << "    -tar_pos    : " << ecatConfig->getSlaveOutputVarValueByName<int32_t>(i, "Target position") << std::endl;
        std::cout << "    -phy_output : " << std::hex << ecatConfig->getSlaveOutputVarValueByName<uint32_t>(i, "Physical outputs") << std::dec << std::endl;
    }

}

TEST_CASE("csp_elmox1") {
    auto ecatConfig = rocos::EcatConfig::getInstance(0);

    auto current_pos = ecatConfig->getSlaveInputVarValueByName<int32_t>(0, "Position actual value");
    ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos);


    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 128);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 0);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 6);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 7);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 15);

    usleep(1000000);

    for(int i = 0; i < 5000; i++) {
        current_pos -= 20;
        ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos);

        ecatConfig->waitForSignal();

    }

    for(int i = 0; i < 5000; i++) {
        current_pos += 20;
        ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos);

        ecatConfig->waitForSignal();

    }

}

TEST_CASE("502-csp") {
    auto ecatConfig = rocos::EcatConfig::getInstance(0);

    std::cout << "Slave count: " << ecatConfig->ecatBus->slave_num << std::endl;

    auto current_pos = ecatConfig->getSlaveInputVarValueByName<int32_t>(0, "Position actual value");
    ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 1);

    usleep(1000000);

    ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos - 50000);

    usleep(2000000);

    ecatConfig->setSlaveOutputVarValueByName<int32_t>(0, "Target position", current_pos);

    usleep(200000);
    ecatConfig->setSlaveOutputVarValueByName<uint16_t>(0, "Controlword", 0);

}


TEST_CASE("reset cycle time") {
    auto ecatConfig = rocos::EcatConfig::getInstance();

    std::cout << "Min cycle time: " << ecatConfig->getBusMinCycleTime() << std::endl;
    std::cout << "Max cycle time: " << ecatConfig->getBusMaxCycleTime() << std::endl;
    std::cout << "Avg cycle time: " << ecatConfig->getBusAvgCycleTime() << std::endl;
    std::cout << "Curr cycle time: " << ecatConfig->getBusCurrentCycleTime() << std::endl;


    ecatConfig->resetCycleTime();
    sleep(1);

    std::cout << "Min cycle time: " << ecatConfig->getBusMinCycleTime() << std::endl;
    std::cout << "Max cycle time: " << ecatConfig->getBusMaxCycleTime() << std::endl;
    std::cout << "Avg cycle time: " << ecatConfig->getBusAvgCycleTime() << std::endl;
    std::cout << "Curr cycle time: " << ecatConfig->getBusCurrentCycleTime() << std::endl;

}


#undef private 
#undef protected

