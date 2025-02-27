#pragma once

#include <windows.h>

#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>

#include "environment.h"

struct ImportData
{
	std::string library;
	std::string name;
	uint64_t	rva = 0;
};

struct SectionData
{
	uint64_t virtual_size;
	uint64_t virtual_address;
	uint64_t raw_size;
	uint64_t raw_address;
	uint64_t characteristics;
};

class PEFile
{
   private:
	static inline bool ptedit_initialized;
	static std::unordered_map<std::string, PEFile*> moduleList_namekey;

	uintmax_t	  size = 0;
	std::ifstream File;

	template <typename T>
	T makepointer(uint64_t buffer, uint64_t offset)
	{
		return (T)(buffer + offset);
	}

	template <typename T>
	T makepointer(unsigned char* buffer, uint64_t offset)
	{
		return (T)(reinterpret_cast<uint64_t>(buffer) + offset);
	}

	std::unordered_map<uint64_t, ImportData>  imports_rvakey;
	std::unordered_map<uint64_t, std::string> exports_rvakey;

	std::unordered_map<std::string, ImportData> imports_namekey;
	std::unordered_map<std::string, uint64_t>	exports_namekey;

	PIMAGE_DOS_HEADER pDosHeader = 0;
	PIMAGE_NT_HEADERS pNtHeaders = 0;

	PIMAGE_OPTIONAL_HEADER64 pOptionalHeader = 0;
	PIMAGE_FILE_HEADER		 pImageFileHeader = 0;
	PIMAGE_SECTION_HEADER	 pImageSectionHeader = 0;

	bool is_kernel = false;
	bool mirrored = false;
	bool make_um = false;

	void* imagebase_va = 0;
	unsigned char* source_mapped_buffer = 0;	// IF Usermode:  Treat mapped_buffer as source_mapped_buffer						IF Kernel:  the real kernel base of the module.
	
	unsigned char* mapped_buffer = 0;			// <-  What the instrumented driver will see!    
												// IF Usermode:  Will be set as NO_ACCESS once mapping is done.						IF Kernel:  Buffer that represents the real kernel memory
	
	unsigned char* shadow_buffer = 0;			// IF Usermode:  A 1:1 copy of the mapped buffer that will be used for read/write.  IF Kernel:  Buffer that has PFN's from kernel in usermode

	uintmax_t virtual_size = 0;
	uintmax_t imagebase = 0;
	uintmax_t entrypoint = 0;

	bool isExecutable = false;

	void CheckLoadPTEdit();
	void MapKernelToUserMode();
	void MirrorKernelToUserMode();
	unsigned char* GetProtectedBuffer();

	void ParseHeader();
	void ParseSection();
	void ParseImport();
	void ParseExport();


	PEFile(std::string filename, std::string name, uintmax_t size);
	PEFile(PRTL_PROCESS_MODULE_INFORMATION_EX mod,
		   std::string						  name,
		   bool								  is_kernel,
		   bool								  make_user_mode,
		   bool								  mirror);

   public:
	bool isKernelLoaded;
	PRTL_PROCESS_MODULE_INFORMATION_EX proc_mod = 0;

	std::string filename;
	std::string name;

	static std::vector<PEFile*> LoadedModuleArray;

	static PEFile* MirrorMemoryToUM(std::string name);
	static PEFile* ChangeEntriesToUM(std::string name);
	static PEFile* Open(std::string path, std::string name);
	static PEFile* Open(PRTL_PROCESS_MODULE_INFORMATION_EX mod,
						std::string name,
						bool		is_kernel,
						bool		make_user_mode,
						bool		mirror);
	static PEFile* FindModule(uintptr_t ptr);
	static PEFile* FindModule(std::string name);  // find to which module a ptr belongs to.
	static void	   SetPermission();				  // This will prepare the page access for every loaded executable
	static bool	   SetRead(std::string& mod_name, bool enable, uintptr_t addr);

	std::unordered_map<std::string, SectionData> sections;

	void ResolveImport();

	ImportData*								  GetImport(std::string name);
	ImportData*								  GetImport(uint64_t rva);
	uint64_t								  GetExport(std::string name);
	const char*								  GetExport(uint64_t rva);
	uint64_t								  GetVirtualSize();
	uint64_t								  GetImageBase();
	void									  CreateShadowBuffer();
	uint64_t								  GetMappedImageBase();
	uintptr_t								  GetShadowBuffer();
	uintmax_t								  GetEP();
	void									  SetExecutable(bool isExecutable);
	std::unordered_map<uint64_t, std::string> GetAllExports();
};
