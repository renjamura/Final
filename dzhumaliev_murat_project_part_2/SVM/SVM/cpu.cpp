#include "cpu.h"

#include <iostream>

namespace vm
{
    Registers::Registers()
        : first_reg(0), second_reg(0), third_reg(0), flags(0), ip(0), sp(0) {}

    CPU::CPU(MMU &mmu, PIC &pic): registers(), _mmu(mmu), _pic(pic) {}

    CPU::~CPU() {}

    void CPU::Step()
    {
        int ip = registers.ip;

        int instruction = _mmu.ram[ip];
        int data = _mmu.ram[ip + 1];

        switch (instruction) {
        case CPU::MOVA_BASE_OPCODE:
            registers.first_reg = data;
            registers.ip += 2;

            break;
        case CPU::MOVB_BASE_OPCODE:
            registers.second_reg = data;
            registers.ip += 2;

            break;
        case CPU::MOVC_BASE_OPCODE:
            registers.third_reg = data;
            registers.ip += 2;

            break;


		case CPU::LDA_BASE_OPCODE:
		{	
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);
			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a = registers.first_reg;
				registers.first_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.first_reg = a;
			} 
			else 
			{
				_mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second] = registers.first_reg;
				registers.ip += 2;
			}
			
		} break;

		case CPU::LDB_BASE_OPCODE:
		{
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);
			
			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a= registers.second_reg;
				registers.second_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.second_reg = a;
			}
			else 
			{
				_mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second] = registers.second_reg;
				registers.ip += 2;
			}

		} break;


		case CPU::LDC_BASE_OPCODE:
		{
			
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);

			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a = registers.third_reg;
				registers.third_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.third_reg = a;
			} 
			else 
			{
				_mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second] = registers.third_reg;
				registers.ip += 2;
			}
		}break;

		case CPU::STA_BASE_OPCODE :
		{
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);
			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a = registers.first_reg;
				registers.first_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.first_reg = a;
			} 
			else
			{
				registers.first_reg = _mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second];
				registers.ip += 2;
			}

		} break;
			
		case CPU::STB_BASE_OPCODE :
		{
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);
			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a = registers.second_reg;
				registers.second_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.second_reg = a;
			}
			else
			{
				registers.second_reg = _mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second];
				registers.ip += 2;
			}

		} break;

		case CPU::STC_BASE_OPCODE :
		{
			MMU::page_index_offset_pair_type page_index_phys_offset  = _mmu.GetPageIndexAndOffsetForVirtualAddress(data);
			MMU::page_entry_type page = _mmu.page_table ->at(page_index_phys_offset.first);
			if( page == MMU::INVALID_PAGE)
			{
				MMU::ram_size_type a = registers.third_reg;
				registers.third_reg = page_index_phys_offset.first;
				_pic.isr_4();
				registers.third_reg = a;
			} 
			else 
			{
				registers.third_reg = _mmu.ram[page_index_phys_offset.first + page_index_phys_offset.second];
				registers.ip += 2;
			}

		} break;
		
		case CPU::JMP_BASE_OPCODE:
		{
			registers.ip += data;
		} break;

        case CPU::INT_BASE_OPCODE:
		{
			_pic.isr_3();
		} break;
        default:
		{
            std::cerr << "CPU: invalid opcode data (" << instruction << "). Skipping..." << std::endl;
            registers.ip += 2;

		} break;

        }
    }
}
