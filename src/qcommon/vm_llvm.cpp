
#ifdef __cplusplus
extern          "C"
{
#endif
#include "vm_local.h"
#ifdef __cplusplus
}
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <llvm/Module.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/PassManager.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/ModuleProvider.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

using namespace llvm;

#ifdef __cplusplus
extern          "C"
{
#endif

static ExecutionEngine *engine = NULL;

void           *VM_LoadLLVM(vm_t * vm, intptr_t(*systemcalls) (intptr_t, ...))
{
	char            name[MAX_QPATH];
	char            filename[MAX_QPATH];
	int             len;
	char           *bytes;
					std::string error;

					Com_StripExtension(vm->name, name, sizeof(name));
					Com_sprintf(filename, sizeof(filename), "%sllvm.bc", name);
					len = FS_ReadFile(filename, (void **)&bytes);

	if              (!bytes)
	{
		Com_Printf("Couldn't load llvm file: %s\n", filename);
		return NULL;
	}

	MemoryBuffer   *buffer = MemoryBuffer::getMemBuffer(bytes, bytes + len);
	Module         *module = ParseBitcodeFile(buffer, &error);
	delete          buffer;

	FS_FreeFile(bytes);

	if(!module)
	{
		Com_Printf("Couldn't parse llvm file: %s: %s\n", filename, error.c_str());
		return NULL;
	}

	PassManager     p;

	p.add(new TargetData(module));
	p.add(createFunctionInliningPass());
	p.run(*module);

	ExistingModuleProvider *mp = new ExistingModuleProvider(module);

	if(!engine)
	{
		engine = ExecutionEngine::create(mp);
	}
	else
	{
		engine->addModuleProvider(mp);
	}

	Function       *func = module->getFunction(std::string("dllEntry"));
	void            (*dllEntry) (intptr_t(*syscallptr) (intptr_t, ...)) =
		(void (*)(intptr_t(*syscallptr) (intptr_t,...)))engine->getPointerToFunction(func);
	dllEntry(systemcalls);

	func = module->getFunction(std::string("vmMain"));
	intptr_t(*fp) (int,...) = (intptr_t(*)(int,...))engine->getPointerToFunction(func);

	vm->entryPoint = fp;

	if(com_developer->integer)
	{
		Com_Printf("Loaded LLVM %s with mp==%p\n", name, mp);
	}

	return mp;
}

void            VM_UnloadLLVM(void *llvmModuleProvider)
{
	if(!llvmModuleProvider)
	{
		Com_Printf("VM_UnloadLLVM called with NULL pointer\n");
		return;
	}

	if(com_developer->integer)
	{
		Com_Printf("Unloading LLVM with mp==%p\n", llvmModuleProvider);
	}

	Module         *module = engine->removeModuleProvider((ExistingModuleProvider *) llvmModuleProvider);

	if(!module)
	{
		Com_Printf("Couldn't remove llvm\n");
		return;
	}
	delete          module;
}

#ifdef __cplusplus
}
#endif
