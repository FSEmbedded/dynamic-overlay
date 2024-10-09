#include "u-boot.h"
#include "dynamic_mounting.h"
#include "preinit.h"
#include "persistent_mem_detector.h"
#include "create_link.h"

#ifdef BUILD_X509_CERIFICATE_STORE_MOUNT
	#include "x509_cert_store.h"
#endif

#include <iostream>
#include <string>
#include <memory>

int main()
{
	try
	{
		PreInit::MountArgs proc = PreInit::MountArgs();
		proc.source_dir = std::string("proc");
		proc.dest_dir = std::string("/proc");
		proc.filesystem_type = "proc";
		proc.flags = 0;

		PreInit::MountArgs sys = PreInit::MountArgs();
		sys.source_dir = std::string("sysfs");
		sys.dest_dir = std::string("/sys");
		sys.filesystem_type = "sysfs";
		sys.flags = 0;

		PreInit::MountArgs persistent = PreInit::MountArgs();

		OverlayDescription::ReadOnly ramdisk;

		PreInit::PreInit init_stage1 = PreInit::PreInit();

		/* First mount proc and then sysfs. This ensures
		 * necessary system info is available before starting
		 * other filesystems and services.
		*/
		init_stage1.add(proc);
		init_stage1.add(sys);
		init_stage1.prepare();

		PersistentMemDetector::PersistentMemDetector mem_dect;
		std::string uboot_env_path = create_link::get_fw_env_config(mem_dect.getMemType());
		std::shared_ptr<UBoot> uboot = std::make_shared<UBoot>(uboot_env_path);
		ramdisk = create_link::prepare_ramdisk(RAMFS_HW_CONFIG_MOUNTPOINT, mem_dect.getMemType());

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
			std::cerr << "dynamicoverlay: Error during mount persistent memory: " << err.what() << std::endl;
		}

		try
		{
			// The overlay link is not updated in this scope. It must use "the old" path.
			DynamicMounting handler = DynamicMounting(uboot);
			handler.add_lower_dir_readonly_memory(ramdisk);
#ifdef BUILD_X509_CERIFICATE_STORE_MOUNT
			OverlayDescription::ReadOnly ramdisk_x509_unpacked_store;
			x509_store::prepare_ramdisk_readable(RAMFS_CERT_STORE_MOUNTPOINT);
			bool handle_secure_store_fails = false;
			/* Handling to extract secure store is not critical.
			*  Handle exception from store as warnings.
			*/
			try {

				if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC)
				{
					x509_store::CertMMCstore cert_store;
					cert_store.ExtractCertStore(RAMFS_CERT_STORE_MOUNTPOINT, mem_dect.getBootDevice());
				}
				else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND)
				{
					x509_store::CertMDTstore cert_store;
					cert_store.ExtractCertStore(RAMFS_CERT_STORE_MOUNTPOINT);
				}

				/* after installation prepare for permissions */
				int ret = ::system("chown -R adu:adu /adu");
				if ((ret == -1) || (ret == 127))
				{
					throw ret;
				}

				ramdisk_x509_unpacked_store = x509_store::close_ramdisk(RAMFS_CERT_STORE_MOUNTPOINT);

				handler.add_lower_dir_readonly_memory(ramdisk_x509_unpacked_store);
			} catch(std::exception const& ex) {
				std::cerr << "dynamicoverlay: Warning, " << ex.what() << std::endl;
			}  catch(int e) {
				std::cerr << "dynamicoverlay: Warning, directory adu can't change own %d" << e << std::endl;
			}

			if(handle_secure_store_fails == true)
				ramdisk_x509_unpacked_store = x509_store::close_ramdisk(RAMFS_CERT_STORE_MOUNTPOINT);
#endif
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
		std::cerr << "dynamicoverlay: Error during execution: " << err.what() << std::endl;
	}

	return 0;
}
