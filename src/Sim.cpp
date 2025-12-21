#include "Sim.h"
#include "HMT.h"
#include "cpu.h"
#include "libpimeval.h"
#include <cstdlib>
#include <iostream>

/*
 * NOTE:
 * mb_fill contain all hard-coded information about memory layout and memory content,
 * which will be provided by OS during trace-enabled simulation
 *
 * For now the actual format of how data will being transfered haven't become clear yet,
 * so we keep all information hard-coded
 */
void Sim::mb_fill(){
#ifdef SIM_NOTRACE
    mem_bond bond_tmpl0;
    bond_tmpl0.varidx = 0xcca;
    bond_tmpl0.typ = mb_type::INPUT;
    bond_tmpl0.size = 32;

    mem_bond bond_tmpl1;
    bond_tmpl1.varidx = 0xbbc;
    bond_tmpl1.typ = mb_type::INPUT;
    bond_tmpl1.size = 32;

    mem_bond bond_tmpl2;
    bond_tmpl2.varidx = 0x24;
    bond_tmpl2.typ = mb_type::OUTPUT;
    bond_tmpl2.size = 32;

    mem_list.push_back(bond_tmpl0);
    mem_list.push_back(bond_tmpl1);
    mem_list.push_back(bond_tmpl2);

    for(auto it = mem_list.begin(); it != mem_list.end(); ++it) {
        it->base = calloc(32, sizeof(int));
        int *ptr = (int*)it->base;
        for(int i=0; i<32; i++) {
            if(it->typ == mb_type::INPUT)
                ptr[i] = 1;
            else
                ptr[i] = 0;
        }
    }
#else
    std::cerr<<"Trace based simulation haven't implement yet"<<std::endl;
    exit(1);
#endif
    return;
}

/*
 * Allocate PIM device memory and fill up HMT table
 */
void Sim::init_mem(){
    sim_hmt = new HMT_table_t();

    bool is_first = true;
    for(auto it = mem_list.begin(); it != mem_list.end(); ++it) {
        if(is_first) {
            it->pim_id = pimAlloc(PIM_ALLOC_AUTO, it->size, PIM_INT32);
            is_first = false;
        } else {
            it->pim_id = pimAllocAssociated(std::prev(it)->pim_id, PIM_INT32);
        }

        sim_hmt->add_new_ent(
            (size_t)it->base,
            it->varidx,
            it->size,
            it->pim_id,
            HMT_flag_t::RW
        );
        printf("Added mem bound 0x%x\n", it->varidx);
    }
}


/*
 * Note
 * SCALED_ADD(And some other instructions) does not yield READ_SRCx event
 * in Auto-DSE, but HMT only needs to appear inside READ_SRCx.
 *
 * When evaluate the latency for this kinds of instruction, remember to
 * turn off FUSE instructions and use old timing model
 *
 * The best way to solve this problem is to implement a HMT event inside
 * AutoDSE and generate an HMT event in correct place.
 */
void Sim::init_imem(){
    sim_imem = new IMEM_t();
    sim_imem->insts = {
        (instruction_t){
            Opcode::ADD,
            0x100,
            0xdead,
            {
                (fatptr){0x24, 0}, //dst
                (fatptr){0xcca, 0},//src0
                (fatptr){0xbbc, 0},//src1
            }
        },
        // (instruction_t){
        //     Opcode::ADD,
        //     0xdead,
        //     0xdead,
        //     {
        //         (fatptr){0x24, 0}, //dst
        //         (fatptr){0xcca, 0},//src0
        //         (fatptr){0xbbc, 0},//src1
        //     }
        // },
        (instruction_t){
            Opcode::EXIT,
            {
            }
        },
    };

    return;
}


void Sim::init_cpu(){
    sim_cpu = new cpu_t(sim_imem, sim_hmt, fused_prog);
}

void Sim::sim_begin(size_t max_cycle) {
    for(int i=0; i<max_cycle; i++){
        if(sim_cpu->cpu_stop == true && sim_cpu->stopable()){
            std::cout<<"Simulation finished"<<std::endl;
            break;
        }
        sim_cpu->tick();
    }

}

void Sim::pre_sim(){
    for(auto it = mem_list.begin(); it != mem_list.end(); ++it) {
        assert(it->base != nullptr);
        if(it->typ == mb_type::INPUT) {
#ifdef PIM_FUSE
            fused_prog->add(pimCopyHostToDevice, it->base, it->pim_id, 0UL, 0UL);
#else
            pimCopyHostToDevice(it->base, it->pim_id, 0UL, 0UL);
#endif
        }
    }
}

void Sim::post_sim(){
    for(auto it = mem_list.begin(); it != mem_list.end(); ++it) {
        assert(it->base != nullptr);
        if(it->typ == mb_type::OUTPUT) {
#ifdef PIM_FUSE
            fused_prog->add(pimCopyDeviceToHost, it->pim_id, it->base, 0UL, 0UL);
#else
            pimCopyDeviceToHost(it->pim_id, it->base, 0UL, 0UL);
#endif
        }
    }

#ifdef PIM_FUSE
    sim_cpu->exec_fuse();
#endif


    pimShowStats();
}
