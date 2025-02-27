#include <Logger/Logger.h>
#include <PEMapper/pefile.h>

#include <SymParser/symparser.hpp>

#include "provider.h"
#include "driver_buddy.h"

namespace Provider
{
	std::unordered_map<std::string, PVOID>	  function_providers;
	std::unordered_map<std::string, PVOID>	  passthrough_provider_cache;
	std::unordered_map<std::string, PVOID>	  data_providers;
	std::vector<std::pair<uintptr_t, size_t>> export_data_range;
}  // namespace Provider

static auto ntdll = LoadLibraryA("ntdll.dll");

uintptr_t Provider::FindFuncImpl(uintptr_t ptr, PCONTEXT ctx)
{
	uintptr_t implPtr = 0;

	auto pe_file = PEFile::FindModule(ptr);		// if it's been PEFile::Open'd, it's in the loaded_module_array
	if (!pe_file)
		DebugBreak();

	std::string reserved_str = "";
	auto		rva = ptr - pe_file->GetMappedImageBase();	// the non-shadow base (aka the base that is known to emulated driver)
	auto		exported_func = pe_file->GetExport(rva);

	auto sym = symparser::find_symbol(pe_file->filename, rva);

	if (!exported_func)
	{
		sym = symparser::find_symbol(pe_file->filename, rva);
		if (!sym || !sym->rva)
		{
			DebugBreak();
			return 0;
		}
		reserved_str = sym->name;
		exported_func = reserved_str.c_str();
	}

	Logger::Log("Executing %s!%s\n", pe_file->name.c_str(), exported_func);
	if (!strcmp(exported_func, "ZwCreateSection"))
		DebugBreak();
	// e.g. a function that is implemented in ntoskrnl_provider.cpp and registered
	if (function_providers.contains(exported_func))
		return (uintptr_t)function_providers[exported_func];

	if (passthrough_provider_cache.contains(exported_func))
		return (uintptr_t)passthrough_provider_cache[exported_func];

	implPtr = (uintptr_t)GetProcAddress(ntdll, exported_func);

	if (!implPtr)
	{
		// PoC based on MmGetSystemRoutineAddress
		auto result = DriverBuddy::Execute(ctx);
		//if (result)
		//	implPtr = (uintptr_t)unimplemented_stub;
	}
		

	passthrough_provider_cache.insert(std::pair(exported_func, (PVOID)implPtr));

	return implPtr;
}

uintptr_t Provider::FindDataImpl(uintptr_t ptr)
{
	auto pe_file = PEFile::FindModule(ptr);
	if (!pe_file)
		return 0;

	auto rva = ptr - pe_file->GetMappedImageBase();
	auto exported_func = pe_file->GetExport(rva);
	std::string reserved_str = "";

	if (!exported_func)
	{
		auto sym = symparser::find_symbol(pe_file->filename, rva);
		if (!sym || !sym->rva)
		{
			return 0;
		}
		reserved_str = sym->name;
		exported_func = reserved_str.c_str();
	}

	Logger::Log("Getting data @ %s!%s\n", pe_file->name.c_str(), exported_func);

	if (data_providers.contains(exported_func))
		return (uintptr_t)data_providers[exported_func];

	Logger::Log("Exported Data %s!%s is not implemented\n", pe_file->name.c_str(), exported_func);
	return 0;
}

uintptr_t Provider::AddFuncImpl(const char* nameFunc, PVOID hookFunc)
{
	function_providers.insert(std::pair(nameFunc, hookFunc));
	return 1;
}

uintptr_t Provider::AddDataImpl(const char* nameExport, PVOID hookExport, size_t exportSize)
{
	data_providers.insert(std::pair(nameExport, hookExport));
	export_data_range.push_back(std::pair((uintptr_t)hookExport, exportSize));
	return 1;
}

uint64_t Provider::unimplemented_stub()
{
	Logger::Log("\t\t\033[38;5;9mINSIDE STUB, RETURNING 0\033[0m\n");
	return 0;
}
