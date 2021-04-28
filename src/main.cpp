#include "dynamic_mounting.h"
#include "preinit.h"

int main()
{	
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
		persistent.source_dir = std::filesystem::path("/dev/mmcblk2p9");
		persistent.dest_dir = std::filesystem::path("/rw_fs/root");
		persistent.filesystem_type = "ext4";
		persistent.flags = 0;

		PreInit::PreInit init = PreInit::PreInit();

		init.add(sys);
		init.add(proc);
		init.add(persistent);

		init.prepare();
		
		DynamicMounting handler = DynamicMounting();

		handler.application_image();
		
		init.remove(sys.dest_dir);
		init.remove(proc.dest_dir);
	}
	return 0;
}
