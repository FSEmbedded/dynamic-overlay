#include "dynamic_mounting.h"
#include "preinit.h"
#include "persistent_mem_detector.h"

#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

int main()
{	
	try
	{
		PreInit::MountArgs proc = PreInit::MountArgs();
		proc.source_dir = std::filesystem::path("none");
		proc.dest_dir = std::filesystem::path("/proc");
		proc.filesystem_type = "proc";
		proc.flags = 0;

		PreInit::MountArgs sys = PreInit::MountArgs();
		sys.source_dir = std::filesystem::path("none");
		sys.dest_dir = std::filesystem::path("/sys");
		sys.filesystem_type = "sysfs";
		sys.flags = 0;

		PreInit::MountArgs persistent = PreInit::MountArgs();

		PreInit::PreInit init_stage1 = PreInit::PreInit();
		PreInit::PreInit init_stage2 = PreInit::PreInit();


		init_stage1.add(sys);
		init_stage1.add(proc);

		init_stage1.prepare();
		try 
		{
			PersistentMemDetector::PersistentMemDetector mem_dect;

			if(mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC)
			{
				persistent.source_dir = std::filesystem::path("/dev/mmcblk2p9");
				persistent.dest_dir = std::filesystem::path("/rw_fs/root");
				persistent.filesystem_type = "ext4";
				persistent.flags = 0;
			}
			else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND)
			{
				persistent.source_dir = std::filesystem::path("/dev/ubi0_2");
				persistent.dest_dir = std::filesystem::path("/rw_fs/root");
				persistent.filesystem_type = "ubifs";
				persistent.flags = 0;
			}
			else
			{
				throw(std::logic_error("Could not determine current memory type (NAND|eMMC)"));
			}

			init_stage2.add(persistent);

			init_stage2.prepare();
		}
		catch(...)
		{
			init_stage1.remove(sys.dest_dir);
			init_stage1.remove(proc.dest_dir);
			throw;
		}

		DynamicMounting handler = DynamicMounting();

		handler.application_image();
		
		init_stage1.remove(sys.dest_dir);
		init_stage1.remove(proc.dest_dir);
	}
	catch( const std::exception &err)
	{
		std::cerr << "Error during execution: " << err.what() << std::endl;
		return 1;
	}

	return 0;
}