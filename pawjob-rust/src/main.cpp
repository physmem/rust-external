#include <cstdio>
#include <iostream>
#include <print>
#include <windows.h>

#include "app/winapp.hpp"

#include "memory/memory.hpp"
#include "utils/vmp.hpp"
#include <globals.hpp>
#include <isyscall/inline_syscall.hpp>

std::int32_t main()
{
	std::cout << "\033[38;2;255;85;200m" << R"(
			   ________  ________  ___       __         ___  ________  ________     
			  |\   __  \|\   __  \|\  \     |\  \      |\  \|\   __  \|\   __  \    
			  \ \  \|\  \ \  \|\  \ \  \    \ \  \     \ \  \ \  \|\  \ \  \|\ /_   
			   \ \   ____\ \   __  \ \  \  __\ \  \  __ \ \  \ \  \\\  \ \   __  \  
			    \ \  \___|\ \  \ \  \ \  \|\__\_\  \|\  \\_\  \ \  \\\  \ \  \|\  \ 
			     \ \__\    \ \__\ \__\ \____________\ \________\ \_______\ \_______\
			      \|__|     \|__|\|__|\|____________|\|________|\|_______|\|_______|
			                                                                        
			                                                                          
			                                                                          
			)" << "\033[0m" << std::endl;

	::timeBeginPeriod(1);
	::SetThreadPriority(::GetCurrentProcess(), THREAD_PRIORITY_HIGHEST);

	memory::create_instance("RustClient.exe");

	if (!winapp::create_window_instance())
	{
		return 0;
	}

	winapp::destroy_window_instance();

	isyscall::unload();

	::timeEndPeriod(1);
	return 0;
}