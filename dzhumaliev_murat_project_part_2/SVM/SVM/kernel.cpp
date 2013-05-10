#include "kernel.h"

#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <limits>

namespace vm
{
    Kernel::Kernel(Scheduler scheduler, std::vector<std::string> executables_paths)
        : machine(), processes(), priorities(), scheduler(scheduler),
          _last_issued_process_id(0),
		  _current_process_index(0), 
		  _cycles_passed_after_preemption(0)
    {
        // Memory

		/*
			TODO:
			
			Initialize data structures for methods:
				AllocateMemory, FreeMemory
		*/
		machine.mmu.ram[0] = _free_physical_memory_index = 0;
		machine.mmu.ram[1] = machine.mmu.ram.size() - 2;

        // Process page faults (find an empty frame)
        machine.pic.isr_4 = [&]()
		{
			/*
				TODO:

				Get the faulting page index from register 'a'

				Try to acquire a new frame from MMU (AcquireFrame)
				Check if this frame is valid
					if valid, write the frame to the current faulting page in the MMU page table (at index from register 'a')
					or else, notify the process or stop the machine (out of physical memory)
			*/
            std::cout << "Kernel: page fault." << std::endl;

			MMU::page_table_size_type faulting_page_index = machine.cpu.registers.first_reg;
			MMU::page_entry_type new_frame = machine.mmu.AcquireFrame();
			if(new_frame != MMU::INVALID_PAGE)
			{
				processes[_current_process_index].page_table->at(faulting_page_index) = new_frame;
			} 
			else
			{
				machine.Stop();
			}		
		};

        // Process Management

        std::for_each(executables_paths.begin(), executables_paths.end(), [&](const std::string &path) {
            CreateProcess(path);
        });

        if (!processes.empty()) {
            std::cout << "Kernel: setting the first process: " << processes[_current_process_index].id << " for execution." << std::endl;

            machine.cpu.registers = processes[_current_process_index].registers;
			// TODO: switch the page table in MMU to the table of the current process
			machine.mmu.page_table = processes[_current_process_index].page_table;

            processes[_current_process_index].state = Process::Running;
        }

        if (scheduler == FirstComeFirstServed || scheduler == ShortestJob) {
            machine.pic.isr_0 = [&]() {};
            machine.pic.isr_3 = [&]() {};
        } else if (scheduler == RoundRobin) {
            machine.pic.isr_0 = [&]() {
                std::cout << "Kernel: processing the timer interrupt." << std::endl;

                if (!processes.empty()) {
                    if (_cycles_passed_after_preemption <= Kernel::_MAX_CYCLES_BEFORE_PREEMPTION)
                    {
                        std::cout << "Kernel: allowing the current process " << processes[_current_process_index].id << " to run." << std::endl;

                        ++_cycles_passed_after_preemption;

                        std::cout << "Kernel: the current cycle is " << _cycles_passed_after_preemption << std::endl;
                    } else {
                        if (processes.size() > 1) {
                            std::cout << "Kernel: switching the context from process " << processes[_current_process_index].id;
                
                            processes[_current_process_index].registers = machine.cpu.registers;
                            processes[_current_process_index].state = Process::Ready;

                            _current_process_index = (_current_process_index + 1) % processes.size();

                            std::cout << " to process " << processes[_current_process_index].id << std::endl;

                            machine.cpu.registers = processes[_current_process_index].registers;
							// TODO: switch the page table in MMU to the table of the current process
							machine.mmu.page_table = processes[_current_process_index].page_table;
                            processes[_current_process_index].state = Process::Running;
                        }

                        _cycles_passed_after_preemption = 0;
                    }
                }

                std::cout << std::endl;
            };

            machine.pic.isr_3 = [&]() {
                std::cout << "Kernel: processing the first software interrupt." << std::endl;

                if (!processes.empty()) {
                    std::cout << "Kernel: unloading the process " << processes[_current_process_index].id << std::endl;

					// TODO: free process memory with FreePhysicalMemory
					FreeMemory(processes[_current_process_index].memory_start_position);
                    processes.erase(processes.begin() + _current_process_index);

                    if (processes.empty()) {
                        _current_process_index = 0;

                        std::cout << "Kernel: no more processes. Stopping the machine." << std::endl;

                        machine.Stop();
                    } else {
                        if (_current_process_index >= processes.size()) {
                            _current_process_index %= processes.size();
                        }

                        std::cout << "Kernel: switching the context to process " << processes[_current_process_index].id << std::endl;

                        machine.cpu.registers = processes[_current_process_index].registers;
						// TODO: switch the page table in MMU to the table of the current process
						machine.mmu.page_table = processes[_current_process_index].page_table;
                        processes[_current_process_index].state = Process::Running;

                        _cycles_passed_after_preemption = 0;
                    }
                }

                std::cout << std::endl;
            };
        } else if (scheduler == Priority) {
            machine.pic.isr_0 = [&]() {};
            machine.pic.isr_3 = [&]() {};
        }

        machine.Start();
    }

    Kernel::~Kernel() {}

    void Kernel::CreateProcess(const std::string &name)
    {
        if (_last_issued_process_id == std::numeric_limits<Process::process_id_type>::max()) {
            std::cerr << "Kernel: failed to create a new process. The maximum number of processes has been reached." << std::endl;
        } else {
            std::ifstream input_stream(name, std::ios::in | std::ios::binary);
            if (!input_stream) {
                std::cerr << "Kernel: failed to open the program file." << std::endl;
            } else {
                MMU::ram_type ops;

                input_stream.seekg(0, std::ios::end);
                auto file_size = input_stream.tellg();
                input_stream.seekg(0, std::ios::beg);
                ops.resize(static_cast<MMU::ram_size_type>(file_size) / 4);

                input_stream.read(reinterpret_cast<char *>(&ops[0]), file_size);

                if (input_stream.bad()) {
                    std::cerr << "Kernel: failed to read the program file." << std::endl;
                } else {
                    MMU::ram_size_type new_memory_position = Kernel::AllocateMemory(ops.size()); // TODO: allocate memory for the process (AllocateMemory)
					if (new_memory_position == -1) {
						std::cerr << "Kernel: failed to allocate memory." << std::endl;
                    } else {
                        std::copy(ops.begin(), ops.end(), (machine.mmu.ram.begin() + new_memory_position));

                        Process process(_last_issued_process_id++, new_memory_position,
                                                                   new_memory_position + ops.size());

                        // Old sequential allocation
                        //
                        // std::copy(ops.begin(), ops.end(), (machine.memory.ram.begin() + _last_ram_position));
                        //
                        // Process process(_last_issued_process_id++, _last_ram_position,
                        //                                            _last_ram_position + ops.size());
                        //
                        // _last_ram_position += ops.size();
						   
						  
						
                    }
                }
            }
        }
    }

    MMU::ram_size_type Kernel::AllocateMemory(MMU::ram_size_type nunits)
    {
		MMU::ram_size_type prevp = _free_physical_memory_index;
		
		for(MMU::ram_size_type next_free_ind = machine.mmu.ram[prevp]; ; 
			prevp = next_free_ind, next_free_ind = machine.mmu.ram[next_free_ind])
		{
			MMU::ram_size_type size = machine.mmu.ram[next_free_ind+ 1];
			if( size >= nunits)
			{
				if(size == nunits)
				{
					machine.mmu.ram[prevp] = machine.mmu.ram[next_free_ind];
				}
				else
				{
					machine.mmu.ram[next_free_ind + 1] -= nunits + 2;
					next_free_ind +=  machine.mmu.ram[next_free_ind + 1];	
					machine.mmu.ram[next_free_ind + 1] = nunits;
				}
				_free_physical_memory_index= prevp;
				return next_free_ind + 2;
			}
			if(next_free_ind == _free_physical_memory_index)
			{
				return -1;
			}
		
		}

		return -1;
	}

	

    void Kernel::FreeMemory(MMU::ram_size_type free_ind)
    {
		MMU::ram_size_type start_free_ind;
		MMU::ram_size_type our_free_ind = free_ind;

		for( start_free_ind=_free_physical_memory_index; !(our_free_ind > start_free_ind && our_free_ind < machine.mmu.ram[start_free_ind]);
			start_free_ind =  machine.mmu.ram[start_free_ind])	
		{

			if( start_free_ind >= machine.mmu.ram[start_free_ind] && 
				((our_free_ind > start_free_ind || our_free_ind < machine.mmu.ram[start_free_ind])))
			{
				break;
			}

		}
		if(our_free_ind + machine.mmu.ram[our_free_ind + 1] == machine.mmu.ram[start_free_ind])
		{
			machine.mmu.ram[our_free_ind + 1] += machine.mmu.ram[start_free_ind + 1];
			machine.mmu.ram[our_free_ind] = machine.mmu.ram[machine.mmu.ram[start_free_ind]];
		}
		else
		{
			machine.mmu.ram[our_free_ind] = machine.mmu.ram[start_free_ind];
		}
		if(start_free_ind + machine.mmu.ram[start_free_ind + 1] == our_free_ind)
		{
			machine.mmu.ram[start_free_ind + 1] += machine.mmu.ram[our_free_ind + 1];
			machine.mmu.ram[start_free_ind] = machine.mmu.ram[our_free_ind];
		}
		else
		{
			machine.mmu.ram[start_free_ind] = our_free_ind;
		}

		_free_physical_memory_index = start_free_ind;
    }
}
