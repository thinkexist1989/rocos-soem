/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : slaveinfo [ifname] [-sdo] [-map]
 * Ifname is NIC interface, f.e. eth0.
 * Optional -sdo to display CoE object dictionary.
 * Optional -map to display slave PDO mapping
 *
 * This shows the configured slave data.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "ethercat.h"

//Add by think 2024.03.02
#include <ecat_config_master.h>
#include <ecat_flags.h>
#include <ver.h>
#include <cstring>
#include <iostream>
#include <termcolor/termcolor.hpp>
#include <boost/format.hpp>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <csignal>

int cycle_us = 0;

EcatConfigMaster *pEcm = nullptr;
volatile bool bRun = true;

char IOmap[4096];
ec_ODlistt ODlist;
ec_OElistt OElist;
boolean printSDO = FALSE;
boolean printMAP = FALSE;
char usdo[128];
char hstr[1024];

OSAL_THREAD_HANDLE thread1;

#define EC_TIMEOUTMON 500

int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
boolean forceByteAlignment = FALSE;


struct PACKED VelocityOut {
    int32 targetVelocity;
    uint16_t controlWord;
};

struct PACKED VelocityIn {
    int32 positionActualValue;
    int32 velocityDemondValue;
    int16 torqueDemond;
    uint16_t statusWord;
};

VelocityOut *target = nullptr;
VelocityIn *val = nullptr;

/********************************************************************************/
/** \brief  signal handler.
*
* \return N/A
*/
static void SignalHandler(int nSignal) {
//    bRun = false;
    ec_close();
    exit(-1);
}

/********************************************************************************/
/** Enable real-time environment
*
* Return: EC_E_NOERROR in case of success, EC_E_ERROR in case of failure.
*/
int EnableRealtimeEnvironment(void) {
    struct utsname SystemName;
    int nMaj, nMin, nSub;
    struct timespec ts;
    int nRetval;
    int dwResult = -1;
    bool bHighResTimerAvail;
    struct sched_param schedParam;

    /* master only tested on >= 2.6 kernel */
    nRetval = uname(&SystemName);
    if (nRetval != 0) {
        std::cerr << "ERROR calling uname(), required Linux kernel >= 2.6" << std::endl;
        dwResult = -1;
        goto Exit;
    }
    sscanf(SystemName.release, "%d.%d.%d", &nMaj, &nMin, &nSub);
    if (!(((nMaj == 2) && (nMin == 6)) || (nMaj >= 3))) {
        std::cerr << boost::format("ERROR - detected kernel = {}.{}.{}, required Linux kernel >= 2.6") % nMaj % nMin %
                     nSub << std::endl;
        dwResult = -1;
        goto Exit;
    }

    /* request realtime scheduling for the current process
    * This value is overwritten for each individual task
    */
    schedParam.sched_priority = 99; /* 1 lowest priority, 99 highest priority */
    nRetval = sched_setscheduler(0, SCHED_FIFO, &schedParam);
    if (nRetval == -1) {
        std::cerr << "ERROR - cannot change scheduling policy!\n"
                  << "root privilege is required or realtime group has to be joined!" << std::endl;
        goto Exit;
    }

    /* disable paging */
    nRetval = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (nRetval == -1) {
        std::cerr << "ERROR - cannot disable paging!" << std::endl;
        dwResult = -1;
        goto Exit;
    }

    /* check if high resolution timers are available */
    if (clock_getres(CLOCK_MONOTONIC, &ts)) {
        bHighResTimerAvail = false;
    } else {
        bHighResTimerAvail = !(ts.tv_sec != 0 || ts.tv_nsec != 1);
    }
    if (!bHighResTimerAvail) {
        std::cerr << "WARNING: High resolution timers not available" << std::endl;
    }

    /* set type of OsSleep implementation  (eSLEEP_USLEEP, eSLEEP_NANOSLEEP or eSLEEP_CLOCK_NANOSLEEP) */
//    OsSleepSetType(eSLEEP_CLOCK_NANOSLEEP);

    dwResult = 0;
    Exit:
    return dwResult;
}


char *dtype2string(uint16 dtype) {
    switch (dtype) {
        case ECT_BOOLEAN:
            sprintf(hstr, "BOOLEAN");
            break;
        case ECT_INTEGER8:
            sprintf(hstr, "INTEGER8");
            break;
        case ECT_INTEGER16:
            sprintf(hstr, "INTEGER16");
            break;
        case ECT_INTEGER32:
            sprintf(hstr, "INTEGER32");
            break;
        case ECT_INTEGER24:
            sprintf(hstr, "INTEGER24");
            break;
        case ECT_INTEGER64:
            sprintf(hstr, "INTEGER64");
            break;
        case ECT_UNSIGNED8:
            sprintf(hstr, "UNSIGNED8");
            break;
        case ECT_UNSIGNED16:
            sprintf(hstr, "UNSIGNED16");
            break;
        case ECT_UNSIGNED32:
            sprintf(hstr, "UNSIGNED32");
            break;
        case ECT_UNSIGNED24:
            sprintf(hstr, "UNSIGNED24");
            break;
        case ECT_UNSIGNED64:
            sprintf(hstr, "UNSIGNED64");
            break;
        case ECT_REAL32:
            sprintf(hstr, "REAL32");
            break;
        case ECT_REAL64:
            sprintf(hstr, "REAL64");
            break;
        case ECT_BIT1:
            sprintf(hstr, "BIT1");
            break;
        case ECT_BIT2:
            sprintf(hstr, "BIT2");
            break;
        case ECT_BIT3:
            sprintf(hstr, "BIT3");
            break;
        case ECT_BIT4:
            sprintf(hstr, "BIT4");
            break;
        case ECT_BIT5:
            sprintf(hstr, "BIT5");
            break;
        case ECT_BIT6:
            sprintf(hstr, "BIT6");
            break;
        case ECT_BIT7:
            sprintf(hstr, "BIT7");
            break;
        case ECT_BIT8:
            sprintf(hstr, "BIT8");
            break;
        case ECT_VISIBLE_STRING:
            sprintf(hstr, "VISIBLE_STRING");
            break;
        case ECT_OCTET_STRING:
            sprintf(hstr, "OCTET_STRING");
            break;
        default:
            sprintf(hstr, "Type 0x%4.4X", dtype);
    }
    return hstr;
}

char *SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype) {
    int l = sizeof(usdo) - 1, i;
    uint8 *u8;
    int8 *i8;
    uint16 *u16;
    int16 *i16;
    uint32 *u32;
    int32 *i32;
    uint64 *u64;
    int64 *i64;
    float *sr;
    double *dr;
    char es[32];

    memset(&usdo, 0, 128);
    ec_SDOread(slave, index, subidx, FALSE, &l, &usdo, EC_TIMEOUTRXM);
    if (EcatError) {
        return ec_elist2string();
    } else {
        switch (dtype) {
            case ECT_BOOLEAN:
                u8 = (uint8 *) &usdo[0];
                if (*u8) sprintf(hstr, "TRUE");
                else sprintf(hstr, "FALSE");
                break;
            case ECT_INTEGER8:
                i8 = (int8 *) &usdo[0];
                sprintf(hstr, "0x%2.2x %d", *i8, *i8);
                break;
            case ECT_INTEGER16:
                i16 = (int16 *) &usdo[0];
                sprintf(hstr, "0x%4.4x %d", *i16, *i16);
                break;
            case ECT_INTEGER32:
            case ECT_INTEGER24:
                i32 = (int32 *) &usdo[0];
                sprintf(hstr, "0x%8.8x %d", *i32, *i32);
                break;
            case ECT_INTEGER64:
                i64 = (int64 *) &usdo[0];
                sprintf(hstr, "0x%16.16" PRIx64" %" PRId64, *i64, *i64);
                break;
            case ECT_UNSIGNED8:
                u8 = (uint8 *) &usdo[0];
                sprintf(hstr, "0x%2.2x %u", *u8, *u8);
                break;
            case ECT_UNSIGNED16:
                u16 = (uint16 *) &usdo[0];
                sprintf(hstr, "0x%4.4x %u", *u16, *u16);
                break;
            case ECT_UNSIGNED32:
            case ECT_UNSIGNED24:
                u32 = (uint32 *) &usdo[0];
                sprintf(hstr, "0x%8.8x %u", *u32, *u32);
                break;
            case ECT_UNSIGNED64:
                u64 = (uint64 *) &usdo[0];
                sprintf(hstr, "0x%16.16" PRIx64" %" PRIu64, *u64, *u64);
                break;
            case ECT_REAL32:
                sr = (float *) &usdo[0];
                sprintf(hstr, "%f", *sr);
                break;
            case ECT_REAL64:
                dr = (double *) &usdo[0];
                sprintf(hstr, "%f", *dr);
                break;
            case ECT_BIT1:
            case ECT_BIT2:
            case ECT_BIT3:
            case ECT_BIT4:
            case ECT_BIT5:
            case ECT_BIT6:
            case ECT_BIT7:
            case ECT_BIT8:
                u8 = (uint8 *) &usdo[0];
                sprintf(hstr, "0x%x", *u8);
                break;
            case ECT_VISIBLE_STRING:
                strcpy(hstr, usdo);
                break;
            case ECT_OCTET_STRING:
                hstr[0] = 0x00;
                for (i = 0; i < l; i++) {
                    sprintf(es, "0x%2.2x ", usdo[i]);
                    strcat(hstr, es);
                }
                break;
            default:
                sprintf(hstr, "Unknown type");
        }
        return hstr;
    }
}

/** Read PDO assign structure */
int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset) {
    auto *pSlave = &pEcm->ecatBus->slaves[slave - 1]; //TODO: slaveId is 1-based
    int *pNum = nullptr;
    rocos::PdVar *pdVar = nullptr;
    std::string str;

    if (PDOassign == 0x1c12) { // Outputs
        pNum = &pSlave->output_var_num;
        pdVar = pSlave->output_vars;
        str = "OUT";
    } else if (PDOassign == 0x1c13) { // Inputs
        pNum = &pSlave->input_var_num;
        pdVar = pSlave->input_vars;
        str = "IN.";
    } else {
        return 0;
    }

    *pNum = 0;

    uint16 idxloop, nidx, subidxloop, rdat, idx, subidx;
    uint8 subcnt;
    int wkc, bsize = 0, rdl;
    int32 rdat2;
    uint8 bitlen, obj_subidx;
    uint16 obj_idx;
    int abs_offset, abs_bit;

    rdl = sizeof(rdat);
    rdat = 0;
    /* read PDO assign subindex 0 ( = number of PDO's) */
    wkc = ec_SDOread(slave, PDOassign, 0x00, FALSE, &rdl, &rdat, EC_TIMEOUTRXM); // rdat -> 1
    rdat = etohs(rdat);

    /* positive result from slave ? */
    if ((wkc > 0) && (rdat > 0)) {
        /* number of available sub indexes */
        nidx = rdat;
        bsize = 0;
        /* read all PDO's */
        for (idxloop = 1; idxloop <= nidx; idxloop++) {
            rdl = sizeof(rdat);
            rdat = 0;
            /* read PDO assign */
            wkc = ec_SDOread(slave, PDOassign, (uint8) idxloop, FALSE, &rdl, &rdat, EC_TIMEOUTRXM); // rdat -> 0x1600
            /* result is index of PDO */
            idx = etohs(rdat);
            if (idx > 0) {
                rdl = sizeof(subcnt);
                subcnt = 0;
                /* read number of subindexes of PDO */
                wkc = ec_SDOread(slave, idx, 0x00, FALSE, &rdl, &subcnt, EC_TIMEOUTRXM); // subcnt -> 3 (entries)
                subidx = subcnt;

                /* for each subindex */
                for (subidxloop = 1; subidxloop <= subidx; subidxloop++) {
                    rdl = sizeof(rdat2);
                    rdat2 = 0;
                    /* read SDO that is mapped in PDO */
                    wkc = ec_SDOread(slave, idx, (uint8) subidxloop, FALSE, &rdl, &rdat2, EC_TIMEOUTRXM);
                    rdat2 = etohl(rdat2);
                    /* extract bitlength of SDO */
                    bitlen = LO_BYTE(rdat2);
                    bsize += bitlen;
                    obj_idx = (uint16) (rdat2 >> 16);
                    obj_subidx = (uint8) ((rdat2 >> 8) & 0x000000ff);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;
                    ODlist.Slave = slave;
                    ODlist.Index[0] = obj_idx;
                    OElist.Entries = 0;
                    wkc = 0;
                    /* read object entry from dictionary if not a filler (0x0000:0x00) */
                    if (obj_idx || obj_subidx)
                        wkc = ec_readOEsingle(0, obj_subidx, &ODlist, &OElist);
//                    printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);

                    pdVar[*pNum].index = obj_idx;
                    pdVar[*pNum].sub_index = obj_subidx;

                    if ((wkc > 0) && OElist.Entries) {
//                        printf(" %-12s %s\n", dtype2string(OElist.DataType[obj_subidx]), OElist.Name[obj_subidx]);

                        memset(pdVar[*pNum].name, '\0', sizeof(pdVar[*pNum].name)); /// Slave Name
                        memcpy(pdVar[*pNum].name, OElist.Name[obj_subidx],
                               strlen(OElist.Name[obj_subidx])); /// Input Var Name

                        pdVar[*pNum].offset = abs_offset;
                        pdVar[*pNum].size = bitlen / 8;


                        *pNum += 1;

                    } else
                        printf("\n");

                    bitoffset += bitlen;
                };
            };
        };
    };

    printf("No. of PD %s.......: %d\n", str.c_str(), *pNum);
    for (int i = 0; i < *pNum; i++) {
        printf("[%02d] 0x%4.4X:0x%2.2X....: %s, %d offs, %d size\n", i + 1, pdVar[i].index, pdVar[i].sub_index,
               pdVar[i].name, pdVar[i].offset, pdVar[i].size);
    }

    /* return total found bitlength (PDO) */
    return bsize;
}

int si_map_sdo(int slave) {
    rocos::Slave *pSlave = &pEcm->ecatBus->slaves[slave - 1]; //TODO: slaveId is 1-based

    printf("=================================\n");

    memcpy(pSlave->name, ec_slave[slave].name, sizeof(ec_slave[slave].name)); /// Slave Name
    printf("Slave Name..........: %s\n", pSlave->name);

    pSlave->id = slave - 1; /// Slave ID
    printf("Slave ID............: %d\n", pSlave->id);

    printf("Configured address..: %4.4x\n", ec_slave[slave].configadr);


    int wkc, rdl;
    int retVal = 0;
    uint8 nSM, iSM, tSM;
    int Tsize, outputs_bo, inputs_bo;
    uint8 SMt_bug_add;

//    printf("PDO mapping according to CoE :\n");
    SMt_bug_add = 0;
    outputs_bo = 0;
    inputs_bo = 0;
    rdl = sizeof(nSM);
    nSM = 0;
    /* read SyncManager Communication Type object count */
    wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, 0x00, FALSE, &rdl, &nSM, EC_TIMEOUTRXM);
    /* positive result from slave ? */
    if ((wkc > 0) && (nSM > 2)) {
        /* make nSM equal to number of defined SM */
        nSM--;
        /* limit to maximum number of SM defined, if true the slave can't be configured */
        if (nSM > EC_MAXSM)
            nSM = EC_MAXSM;
        /* iterate for every SM type defined */
        for (iSM = 2; iSM <= nSM; iSM++) {
            rdl = sizeof(tSM);
            tSM = 0;
            /* read SyncManager Communication Type */
            wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, iSM + 1, FALSE, &rdl, &tSM, EC_TIMEOUTRXM);
            if (wkc > 0) {
                if ((iSM == 2) && (tSM == 2)) // SM2 has type 2 == mailbox out, this is a bug in the slave!
                {
                    SMt_bug_add = 1; // try to correct, this works if the types are 0 1 2 3 and should be 1 2 3 4
                    printf("Activated SM type workaround, possible incorrect mapping.\n");
                }
                if (tSM)
                    tSM += SMt_bug_add; // only add if SMt > 0


                int slaveInOffset = 0;
                int slaveOutOffset = 0;

                for(int i = 1; i < slave; i++) {
                    slaveInOffset += ec_slave[i].Ibytes;
                    slaveOutOffset += ec_slave[i].Obytes;
                }


                if (tSM == 3) // outputs
                {
                    /* read the assign RXPDO */
//                    printf("  SM%1d outputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM,
                                         slaveOutOffset, outputs_bo);
                    outputs_bo += Tsize;
                }
                if (tSM == 4) // inputs
                {
                    /* read the assign TXPDO */
//                    printf("  SM%1d inputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM,
                                         slaveInOffset, inputs_bo);
                    inputs_bo += Tsize;
                }

            }
        }
    }

    /* found some I/O bits ? */
    if ((outputs_bo > 0) || (inputs_bo > 0))
        retVal = 1;
    return retVal;
}

int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset) {
    uint16 a, w, c, e, er, Size;
    uint8 eectl;
    uint16 obj_idx;
    uint8 obj_subidx;
    uint8 obj_name;
    uint8 obj_datatype;
    uint8 bitlen;
    int totalsize;
    ec_eepromPDOt eepPDO;
    ec_eepromPDOt *PDO;
    int abs_offset, abs_bit;
    char str_name[EC_MAXNAME + 1];

    eectl = ec_slave[slave].eep_pdi;
    Size = 0;
    totalsize = 0;
    PDO = &eepPDO;
    PDO->nPDO = 0;
    PDO->Length = 0;
    PDO->Index[1] = 0;
    for (c = 0; c < EC_MAXSM; c++) PDO->SMbitsize[c] = 0;
    if (t > 1)
        t = 1;
    PDO->Startpos = ec_siifind(slave, ECT_SII_PDO + t);
    if (PDO->Startpos > 0) {
        a = PDO->Startpos;
        w = ec_siigetbyte(slave, a++);
        w += (ec_siigetbyte(slave, a++) << 8);
        PDO->Length = w;
        c = 1;
        /* traverse through all PDOs */
        do {
            PDO->nPDO++;
            PDO->Index[PDO->nPDO] = ec_siigetbyte(slave, a++);
            PDO->Index[PDO->nPDO] += (ec_siigetbyte(slave, a++) << 8);
            PDO->BitSize[PDO->nPDO] = 0;
            c++;
            /* number of entries in PDO */
            e = ec_siigetbyte(slave, a++);
            PDO->SyncM[PDO->nPDO] = ec_siigetbyte(slave, a++);
            a++;
            obj_name = ec_siigetbyte(slave, a++);
            a += 2;
            c += 2;
            if (PDO->SyncM[PDO->nPDO] < EC_MAXSM) /* active and in range SM? */
            {
                str_name[0] = 0;
                if (obj_name)
                    ec_siistring(str_name, slave, obj_name);
                if (t)
                    printf("  SM%1d RXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                else
                    printf("  SM%1d TXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                printf("     addr b   index: sub bitl data_type    name\n");
                /* read all entries defined in PDO */
                for (er = 1; er <= e; er++) {
                    c += 4;
                    obj_idx = ec_siigetbyte(slave, a++);
                    obj_idx += (ec_siigetbyte(slave, a++) << 8);
                    obj_subidx = ec_siigetbyte(slave, a++);
                    obj_name = ec_siigetbyte(slave, a++);
                    obj_datatype = ec_siigetbyte(slave, a++);
                    bitlen = ec_siigetbyte(slave, a++);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;

                    PDO->BitSize[PDO->nPDO] += bitlen;
                    a += 2;

                    /* skip entry if filler (0x0000:0x00) */
                    if (obj_idx || obj_subidx) {
                        str_name[0] = 0;
                        if (obj_name)
                            ec_siistring(str_name, slave, obj_name);

                        printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx,
                               bitlen);
                        printf(" %-12s %s\n", dtype2string(obj_datatype), str_name);
                    }
                    bitoffset += bitlen;
                    totalsize += bitlen;
                }
                PDO->SMbitsize[PDO->SyncM[PDO->nPDO]] += PDO->BitSize[PDO->nPDO];
                Size += PDO->BitSize[PDO->nPDO];
                c++;
            } else /* PDO deactivated because SM is 0xff or > EC_MAXSM */
            {
                c += 4 * e;
                a += 8 * e;
                c++;
            }
            if (PDO->nPDO >= (EC_MAXEEPDO - 1)) c = PDO->Length; /* limit number of PDO entries in buffer */
        } while (c < PDO->Length);
    }
    if (eectl) ec_eeprom2pdi(slave); /* if eeprom control was previously pdi then restore */
    return totalsize;
}

void si_sdo(int cnt) {
    int i, j;

    ODlist.Entries = 0;
    memset(&ODlist, 0, sizeof(ODlist));
    if (ec_readODlist(cnt, &ODlist)) {
        printf(" CoE Object Description found, %d entries.\n", ODlist.Entries);
        for (i = 0; i < ODlist.Entries; i++) {
            ec_readODdescription(i, &ODlist);
            while (EcatError) printf("%s", ec_elist2string());
            printf(" Index: %4.4x Datatype: %4.4x Objectcode: %2.2x Name: %s\n",
                   ODlist.Index[i], ODlist.DataType[i], ODlist.ObjectCode[i], ODlist.Name[i]);
            memset(&OElist, 0, sizeof(OElist));
            ec_readOE(i, &ODlist, &OElist);
            while (EcatError) printf("%s", ec_elist2string());
            for (j = 0; j < ODlist.MaxSub[i] + 1; j++) {
                if ((OElist.DataType[j] > 0) && (OElist.BitLength[j] > 0)) {
                    printf("  Sub: %2.2x Datatype: %4.4x Bitlength: %4.4x Obj.access: %4.4x Name: %s\n",
                           j, OElist.DataType[j], OElist.BitLength[j], OElist.ObjAccess[j], OElist.Name[j]);
                    if ((OElist.ObjAccess[j] & 0x0007)) {
                        printf("          Value :%s\n", SDO2string(cnt, ODlist.Index[i], j, OElist.DataType[j]));
                    }
                }
            }
        }
    } else {
        while (EcatError) printf("%s", ec_elist2string());
    }
}

void slaveinfo(const char *ifname) {
    int cnt, i, j, nSM;
    uint16 ssigen;
    int expectedWKC;

    printf("Starting slaveinfo\n");

    /* initialise SOEM, bind socket to ifname */
    if (ec_init(ifname)) {
        printf("ec_init on %s succeeded.\n", ifname);

        /* find and auto-config slaves */
        if (ec_config(FALSE, &IOmap) > 0) {
            ec_configdc();
            while (EcatError) printf("%s", ec_elist2string());
            printf("%d slaves found and configured.\n", ec_slavecount);

            expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
            printf("Calculated workcounter %d\n", expectedWKC);
            /* wait for all slaves to reach SAFE_OP state */
            ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 3);
            if (ec_slave[0].state != EC_STATE_SAFE_OP) {
                printf("Not all slaves reached safe operational state.\n");
                ec_readstate();
                for (i = 1; i <= ec_slavecount; i++) {
                    if (ec_slave[i].state != EC_STATE_SAFE_OP) {
                        printf("Slave %d State=%2x StatusCode=%4x : %s\n",
                               i, ec_slave[i].state, ec_slave[i].ALstatuscode,
                               ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
            }

            uint16 map_1c12[2] = {0x0001, 0x1600};
            uint16 map_1c13[2] = {0x0001, 0x1a00};

            ec_SDOwrite(1, 0x1c12, 0x00, TRUE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);
            ec_SDOwrite(1, 0x1c13, 0x00, TRUE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);

            std::cout << "PD Input Size: byte -> " << ec_slave[0].Ibytes << "; bit -> " << ec_slave[0].Ibits
                      << std::endl;
            std::cout << "PD Output Size: byte -> " << ec_slave[0].Obytes << "; bit -> " << ec_slave[0].Obits
                      << std::endl;

            pEcm = new EcatConfigMaster(FLAGS_id);
            pEcm->createSharedMemory();
            /* 创建PD Memory */
            pEcm->createPdDataMemoryProvider(ec_slave[0].Ibytes, ec_slave[0].Obytes);

            ec_readstate();
            pEcm->ecatBus->slave_num = ec_slavecount;
            for (cnt = 1; cnt <= ec_slavecount; cnt++) {

                ssigen = ec_siifind(cnt, ECT_SII_GENERAL);
                /* SII general section */
                if (ssigen) {
                    ec_slave[cnt].CoEdetails = ec_siigetbyte(cnt, ssigen + 0x07);
                    ec_slave[cnt].FoEdetails = ec_siigetbyte(cnt, ssigen + 0x08);
                    ec_slave[cnt].EoEdetails = ec_siigetbyte(cnt, ssigen + 0x09);
                    ec_slave[cnt].SoEdetails = ec_siigetbyte(cnt, ssigen + 0x0a);
                    if ((ec_siigetbyte(cnt, ssigen + 0x0d) & 0x02) > 0) {
                        ec_slave[cnt].blockLRW = 1;
                        ec_slave[0].blockLRW++;
                    }
                    ec_slave[cnt].Ebuscurrent = ec_siigetbyte(cnt, ssigen + 0x0e);
                    ec_slave[cnt].Ebuscurrent += ec_siigetbyte(cnt, ssigen + 0x0f) << 8;
                    ec_slave[0].Ebuscurrent += ec_slave[cnt].Ebuscurrent;
                }

                if ((ec_slave[cnt].mbx_proto & ECT_MBXPROT_COE) && printSDO)
                    si_sdo(cnt);

                // Print Map SDO
                si_map_sdo(cnt);

            }
        } else {
            printf("No slaves found!\n");
        }
    } else {
        printf("No socket connection on %s\nExcecute as root\n", ifname);
    }
}

OSAL_THREAD_FUNC ecatcheck(void *ptr) {
    int slave;
    (void) ptr;                  /* Not used */

    while (1) {
        if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate)) {
            if (needlf) {
                needlf = FALSE;
                printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++) {
                if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL)) {
                    ec_group[currentgroup].docheckstate = TRUE;
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state == EC_STATE_SAFE_OP) {
                        printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state > EC_STATE_NONE) {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d reconfigured\n", slave);
                        }
                    } else if (!ec_slave[slave].islost) {
                        /* re-check state */
                        ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (ec_slave[slave].state == EC_STATE_NONE) {
                            ec_slave[slave].islost = TRUE;
                            printf("ERROR : slave %d lost\n", slave);
                        }
                    }
                }
                if (ec_slave[slave].islost) {
                    if (ec_slave[slave].state == EC_STATE_NONE) {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d recovered\n", slave);
                        }
                    } else {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d found\n", slave);
                    }
                }
            }
            if (!ec_group[currentgroup].docheckstate)
                printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}

int thread_create(void *thandle, int stacksize, void (*func)(void *), void *param) {
    int ret;
    pthread_attr_t attr;
    pthread_t *threadp;

    threadp = static_cast<pthread_t *>(thandle);
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stacksize);
    ret = pthread_create(threadp, &attr, reinterpret_cast<void *(*)(void *)>(func), param);
    if (ret < 0) {
        return 0;
    }
    return 1;
}


int main(int argc, char *argv[]) {
    //! Linux realtime configuration
    EnableRealtimeEnvironment();
    {
        sigset_t SigSet;
        int nSigNum = SIGALRM;
        sigemptyset(&SigSet);
        sigaddset(&SigSet, nSigNum);
        sigprocmask(SIG_BLOCK, &SigSet, NULL);
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset); // 清空CPU集合

    // 设置进程可以运行在CPU核心0和1上
    CPU_SET(0, &cpuset);

    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
        perror("sched_setaffinity");
        return 1;
    }

    printf("Cpu affinity set success\n");



    //! Set running flag
    bRun = true;

    //! gflags parsing parameters
    gflags::SetVersionString(ROCOS_ECM_VERSION);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

//    ec_adaptert *adapter = NULL;
//    printf("Available adapters\n");
//    adapter = ec_find_adapters();
//    while (adapter != NULL) {
//        printf("Description : %s, Device to use for wpcap: %s\n", adapter->desc, adapter->name);
//        adapter = adapter->next;
//    }

    printf("ROCOS-SOEM (ROCOS - Simple Open EtherCAT Master)\n");


    /* create thread to handle slave error handling in OP */
    thread_create(&thread1, 128000, &ecatcheck, (void *) &ctime);

    slaveinfo(FLAGS_instance.c_str());

    cycle_us = FLAGS_cycle;

    int8_t mode = 8;
    uint8_t period = 2;
    for(int i = 1; i < ec_slavecount; i++) {
        ec_SDOwrite(i, 0x6060, 0, TRUE, sizeof(mode), &mode, EC_TIMEOUTSAFE);
        ec_SDOwrite(i, 0x60c2, 1, TRUE, sizeof(period), &period, EC_TIMEOUTSAFE);
    }



    /** going operational */
    ec_slave[0].state = EC_STATE_OPERATIONAL;
    int wkc = ec_send_processdata();
    std::cout << "ec_send_processdata returned wkc is: " << wkc << std::endl;
    wkc = ec_receive_processdata(EC_TIMEOUTRET100);
    std::cout << "ec_receive_processdata returned wkc is: " << wkc << std::endl; //此处wkc为1，是因为处于Safe-Op，从站只能Tx

    /* request OP state for all slaves */
    ec_writestate(0); //将ec_slave[i].state状态写入各个slave
    int chk = 40;
    /* wait for all slaves to reach OP state */
    do {
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET100);
        ec_statecheck(0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
    } while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));

    if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
        std::cout << "Operational state reached for all slaves." << std::endl;
    } else {
        std::cout << "Not all slaves reached operational state." << std::endl;
        return -1;
    }

    std::cout << "ec_state is: " << ec_statecheck(0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE) << std::endl;

    uint16_t expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
    printf("Calculated workcounter %d\n", expectedWKC);


    while (1) {
        /** PDO I/O refresh */

        auto time_start = std::chrono::high_resolution_clock::now();
        ec_send_processdata();
        wkc = ec_receive_processdata(EC_TIMEOUTRET100);


        if (wkc < expectedWKC) {
            std::cout << "wkc is: " << wkc << std::endl;

        } else {

            memcpy(pEcm->pdInputPtr, ec_slave[0].inputs, ec_slave[0].Ibytes);   // Slave -> Master
            memcpy(ec_slave[0].outputs, pEcm->pdOutputPtr, ec_slave[0].Obytes); // Master -> Slave

            pEcm->updateSempahore();

            auto time_end = std::chrono::high_resolution_clock::now();
            int elasped_time = (time_end - time_start).count() / 1000;



            if (elasped_time < cycle_us) {
                osal_usleep(cycle_us - 10 - elasped_time);
            } else {
                std::cout << "elasped time: " << elasped_time << " us" << std::endl;
            }
        }

    }

}
