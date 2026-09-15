.code
	do_initial_syscall_gadget proc
		; [rcx] = real rcx
		; [rcx + 8] = gadget address
		; [rcx + 16] = syscall id

		mov eax, dword ptr [rcx + 16]

		mov r10, qword ptr [rcx + 8]
		push r10

		mov rcx, qword ptr [rcx]
		mov r10, rcx

		ret
	do_initial_syscall_gadget endp
end