#include "u-boot.h"
#include "dynamic_mounting.h"
#include "preinit.h"
#include "persistent_mem_detector.h"
#include "create_link.h"

#include <iostream>
#include <string>
#include <memory>

int main()
{
	try
	{
		PreInit::MountArgs proc = PreInit::MountArgs();
		proc.source_dir = std::string("none");
		proc.dest_dir = std::string("/proc");
		proc.filesystem_type = "proc";
		proc.flags = 0;

		PreInit::MountArgs sys = PreInit::MountArgs();
		sys.source_dir = std::string("none");
		sys.dest_dir = std::string("/sys");
		sys.filesystem_type = "sysfs";
		sys.flags = 0;

		PreInit::MountArgs persistent = PreInit::MountArgs();
		
		OverlayDescription::ReadOnly ramdisk;

		PreInit::PreInit init_stage1 = PreInit::PreInit();

		init_stage1.add(sys);
		init_stage1.add(proc);

		init_stage1.prepare();

		PersistentMemDetector::PersistentMemDetector mem_dect;
		std::string uboot_env_path = create_link::get_fw_env_config(mem_dect.getMemType());

		std::shared_ptr<UBoot> uboot = std::make_shared<UBoot>(uboot_env_path);
		ramdisk = create_link::prepare_ramdisk(RAMFS_MOUNTPOINT, mem_dect.getMemType());

		std::exception_ptr error_during_mount_persistent;
		try
		{
			PreInit::PreInit init_stage2 = PreInit::PreInit();
			if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC)
			{
				persistent.source_dir = mem_dect.getPathToPersistentMemoryDevice(uboot);
				persistent.dest_dir = std::string("/rw_fs/root");
				persistent.filesystem_type = "ext4";
				persistent.flags = 0;
			}
			else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND)
			{
				persistent.source_dir = mem_dect.getPathToPersistentMemoryDevice(uboot);
				persistent.dest_dir = std::string("/rw_fs/root");
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
		catch (const std::exception &err)
		{
			std::cerr << "Error during mount persistent memory: " << err.what() << std::endl;
		}

		try
		{
			// The overlay link is not updated in this scope. It mus use "the old" path.
			DynamicMounting handler = DynamicMounting(uboot);

			handler.add_lower_dir_readonly_memory(ramdisk);
			handler.application_image();
		}
		catch(...)
		{
			error_during_mount_persistent = std::current_exception();
		}
		
		init_stage1.remove(sys);
		init_stage1.remove(proc);

		if(error_during_mount_persistent)
		{
			std::rethrow_exception(error_during_mount_persistent);
		}
	}
	catch (const std::exception &err)
	{
		std::cerr << "Error during execution: " << err.what() << std::endl;
	}

	return 0;
}
