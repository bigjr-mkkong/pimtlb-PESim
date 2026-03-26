#ifndef __PESIM_CONFIG_H__
#define __PESIM_CONFIG_H__

#define MASA_TLDRAM

#ifdef MASA_TLDRAM
#define MEM_CONFIG_PATH     "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/pimtlb-PESim/cfg/DDR4_8Gb_x4_2400_pim.ini"
#else
#define MEM_CONFIG_PATH     "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/pimtlb-PESim/cfg/DDR4_8Gb_x4_2400.ini"
#endif

#define MEM_OUTPUT_PATH     "/home/michael/Projects/pimtlb/PIM-AutoDSE/output"

#endif

/*
 * No TLDRAM: 1903
 * With TLDRAM: 1936
 */
