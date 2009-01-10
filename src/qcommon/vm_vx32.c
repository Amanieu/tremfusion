/*
===========================================================================
Copyright (C) 2009 Amanieu d'Antras

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// vm_vx32.c -- vx32 binary loader

#include "vm_local.h"
#include "../libvx32/vx32.h"
#include <errno.h>

/*
=================
VM_VX32Syscall

VM has issued a system call, %eax points to arg array
=================
*/
static inline void VM_VX32Syscall(vm_t *vm)
{
	vxproc *p = vm->vx32Handle;
	intptr_t *argptr;

	// Check that the pointer is valid
	if (!vxmem_checkperm(p->mem, p->cpu->reg[EAX], sizeof(int) * 16, VXPERM_READ, NULL))
		Com_Error(ERR_DROP, "Invalid syscall arguments");

	// Get a pointer to the args
#if __WORDSIZE == 64
	// The VM has ints on the stack, but we expect
	// longs so we have to convert it
	intptr_t argarr[16];
	int i;
	for (i = 0; i < 16; ++i) {
		argarr[i] = *((int*)(vm->vx32mmap->base + p->cpu->reg[EAX]) + i);
	}
	argptr = argarr;
#else
	argptr = (int*)(vm->vx32mmap->base + p->cpu->reg[EAX]);
#endif

	// Run the syscall and store the retval in %eax
	p->cpu->reg[EAX] = vm->systemCall(argptr);
}

/*
=================
VM_LoadVX32
=================
*/
void VM_LoadVX32(vm_t *vm)
{
	char filename[MAX_QPATH];
	int length;
	void *buffer;
	vxproc *p;
	
	Com_sprintf(filename, sizeof(filename), "vm/%s.vx32", vm->name);
	Com_Printf( "Loading vx32 file %s...\n", filename );

	// Create the vx32 structure
	p = vxproc_alloc();
	if (!p) {
		Com_Printf("Couldn't allocate vxproc structure\n");
		return;
	}
	p->allowfp = 1;

	// Load the ELF file
	length = FS_ReadFile(filename, &buffer);
	if (!buffer) {
		Com_Printf("Couldn't read %s\n", filename);
		vxproc_free(p);
		return;
	}
	if (vxproc_loadelfmem(p, buffer, length) < 0) {
		Com_Printf("%s is not an ELF binary\n", filename);
		vxproc_free(p);
		return;
	}
	FS_FreeFile(buffer);
	vm->entryEIP = p->cpu->eip;
	vm->vx32Handle = p;

	// Create a memmap so that we can access VM memory
	vm->vx32mmap = vxmem_map(p->mem, 0);
}

/*
=================
VM_CallVX32
=================
*/
int VM_CallVX32(vm_t *vm, int *args)
{
	vxproc *p;
	vxcpu saved_regs;

	// Save the registers to allow recursive VM entry
	p = vm->vx32Handle;
	saved_regs = *p->cpu;

	// Since we are using regparm=3, put the first 3 args in registers and put
	// the rest on the stack
	p->cpu->reg[EAX] = args[0];
	p->cpu->reg[EDX] = args[1];
	p->cpu->reg[ECX] = args[2];
	p->cpu->reg[ESP] -= sizeof(int) * 7;
	if (vxmem_write(p->mem, args + 3, p->cpu->reg[ESP], sizeof(int) * 7) < 0)
		Com_Error(ERR_FATAL, "vx32 stack overflow");

	// Set the entry point EIP
	p->cpu->eip = vm->entryEIP;

	// Now run the vm
	while (1) {
		int rc = vxproc_run(p);
		if (rc < 0)
			Com_Error(ERR_FATAL, "Fatal vx32 error: %s", strerror(errno));
		else if (rc == VXTRAP_SYSCALL) {
			// VM has issued a system call, %eax points to arg array
			VM_VX32Syscall(vm);
		} else if (rc == VXTRAP_BREAKPOINT) {
			// VM has returned from vmMain, retval in %eax
			int retval = p->cpu->reg[EAX];
			// Restore the registers (also cleans up the stack)
			*p->cpu = saved_regs;
			return retval;
		} else
			Com_Error(ERR_DROP, "VX32 VM error %x", rc);
	}
}
