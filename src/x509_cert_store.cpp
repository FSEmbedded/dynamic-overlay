#include "x509_cert_store.h"

#include <memory>
#include <sstream>
#include <vector>
#include <iostream> /* Add for cout... */
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>  // For stat(), chmod()
#include <unistd.h>

/* set max nr. of mtd devices to max number of ubi volumes */
#define MAX_NR_MTD_DEVICES 128
#define BUF_SIZE 1024

#ifndef PART_NAME_MTD_CERT
#define PART_NAME_MTD_CERT "Secure"
#endif

#ifndef FUS_AZURE_CONFIGURATION
#define FUS_AZURE_CONFIGURATION "/adu/du-config.json"
#endif

#define DEFAULT_SECTOR_SIZE 512

x509_store::CertStore::CertStore()
{
    if (!std::filesystem::exists(FUS_AZURE_CONFIGURATION))
    {
        throw std::runtime_error(std::string("File: ") + std::string(FUS_AZURE_CONFIGURATION) +
                                 std::string(" does not exists"));
    }

    this->iot_hub_conf = std::ifstream(FUS_AZURE_CONFIGURATION, std::ifstream::in);
    if (!this->iot_hub_conf.good())
    {
        if (this->iot_hub_conf.bad() || this->iot_hub_conf.fail())
        {
            throw OpenAzureConfigDocument(FUS_AZURE_CONFIGURATION, FileOpenMode::READ);
        }
    }

    std::string strline;
    std::stringstream strjson;
    while (std::getline(this->iot_hub_conf, strline))
    {
        strjson << strline;
    }

    Json::CharReaderBuilder reader;
    std::string errs;
    if (!Json::parseFromStream(reader, strjson, &root, &errs))
    {
        throw ParseJsonDocumentError(errs);
    }
}

bool x509_store::CertStore::parseDuJsonConfig()
{
    if (this->root["agents"][0]["connectionSource"]["connectionType"].asString() != "x509")
    {
        throw NotAx509ConfigurationInDuJson();
    }

    const std::string x509_cert = this->root["agents"][0]["connectionSource"]["x509_cert"].asString();
    const std::string x509_key = this->root["agents"][0]["connectionSource"]["x509_key"].asString();
    const std::string x509_container = this->root["agents"][0]["connectionSource"]["x509_container"].asString();

    bool update_du_json = false;
    if (x509_cert != std::string(FUS_AZURE_CERT_CERTIFICATE_NAME))
    {
        this->root["agents"][0]["connectionSource"]["x509_cert"] = std::string(FUS_AZURE_CERT_CERTIFICATE_NAME);
        update_du_json = true;
    }

    if (x509_key != std::string(FUS_AZURE_CERT_KEY_NAME))
    {
        this->root["agents"][0]["connectionSource"]["x509_key"] = std::string(FUS_AZURE_CERT_KEY_NAME);
        update_du_json = true;
    }

    if (x509_container != std::string(TARGET_ARCHIV_DIR_PATH))
    {
        this->root["agents"][0]["connectionSource"]["x509_container"] = std::string(TARGET_ARCHIV_DIR_PATH);
        update_du_json = true;
    }

    return update_du_json;
}

bool x509_store::CertMDTstore::IsPartitionAvailable()
{
    if (uPartNumber > MAX_NR_MTD_DEVICES)
        return false;
    return true;
}

uint32_t x509_store::CertMDTstore::GetPartitionNumber()
{
    return uPartNumber;
}

int x509_store::CertMDTstore::ScanForPartition(const std::string part_name)
{
    int i;
    std::string mtd_line;

    if (IsPartitionAvailable())
        return 0;

    /* open mtd table to scan for given partition name */
    std::ifstream mtd_table("/proc/mtd", (std::ifstream::in));
    if (!mtd_table.good())
    {
        if (mtd_table.bad() || mtd_table.fail())
        {
            throw OpenMTDDevFailed("/pro/mtd not available.");
        }
    }
    /* read every line and check for the data partition */
    /* first line is row description */
    getline(mtd_table, mtd_line);
    for (i = 0; !mtd_table.eof(); i++)
    {
        getline(mtd_table, mtd_line);
        if (mtd_line.find(part_name) != std::string::npos)
        {
            this->uPartNumber = i;
            return 0;
        }
    }

    return -ENODEV;
}

void x509_store::CertMDTstore::ExtractCertStore(const std::filesystem::path &path_to_ramdisk)
{
    bool use_mdt_part_cert = false;
    /* parse default du configuration */
    bool update_du_json = this->parseDuJsonConfig();
    /* use default file path for secure data */
    std::string arch_mtd_file_path = path_to_ramdisk;
    std::string target_mtd_cert_store = TARGET_ARCHIVE_MTD_CERT_STORE;
    int fd, fd_wr;
    struct fs_header_v1_0 *fsheader10;
    struct fs_header_v0_0 *fsheader00;
    int bytes_read = 0;
    uint64_t file_size = 0;
    char buffer[BUF_SIZE];

    /* scan for secure partition if partition is available
     * then use this.
     */
    if (ScanForPartition(PART_NAME_MTD_CERT) == 0)
    {
        /* use secure partition to read data */
        arch_mtd_file_path = "/dev/mtd" + std::to_string(GetPartitionNumber());
        use_mdt_part_cert = true;
    }

    fsheader10 = (struct fs_header_v1_0 *)calloc(1, sizeof(struct fs_header_v1_0));
    fsheader00 = (struct fs_header_v0_0 *)fsheader10;

    fd = open(arch_mtd_file_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        throw OpenMTDDevFailed(SOURCE_ARCHIVE_MTD_FILE_PATH);
    }
    bytes_read = read(fd, fsheader10, sizeof(struct fs_header_v1_0));
    fsheader00 = (struct fs_header_v0_0 *)fsheader10;
    file_size = fsheader00->file_size_high & 0xFFFFFFFF;
    file_size = file_size << 32;
    file_size = file_size | (fsheader00->file_size_low & 0xFFFFFFFF);

    /*
    std::cout << "arch_mtd_file_path: " << arch_mtd_file_path
              << " filesize: " << file_size << std::endl;
    */

    if (!strcmp("CERT", fsheader10->type) && (file_size > 0))
    {
        int chunk_size = sizeof(buffer);
        fd_wr = open(target_mtd_cert_store.c_str(), (O_WRONLY | O_CREAT), 0600);

        if (fd_wr < 0)
        {
            close(fd);
            free(fsheader10);
            throw CreateCertStore(target_mtd_cert_store);
        }

        while (file_size > 0)
        {
            if (file_size < chunk_size)
                chunk_size = file_size;
            bytes_read = read(fd, buffer, chunk_size);
            ssize_t written = write(fd_wr, buffer, bytes_read);
            if (written == -1) {
                throw std::runtime_error("Failed to write to certificate store: " +
                                       std::string(strerror(errno)));
            }
            if (written != bytes_read) {
                throw std::runtime_error("Incomplete write to certificate store");
            }
            file_size -= bytes_read;
            if (file_size < 0)
            {
                // std::cout << "Read overflow : " << target_mtd_cert_store
                //           << ", filesize: " << file_size << std::endl;
                break;
            }
        }
        close(fd_wr);
    }
    else
    {
        close(fd);
        free(fsheader10);
        throw NoCERTTypeFSFile();
    }

    close(fd);
    free(fsheader10);

    std::string cmd = uncompress_cmd_source_archive;
    cmd += std::string(TARGET_ARCHIVE_MTD_CERT_STORE);
    cmd += uncompress_cmd_dest_folder;
    cmd += std::string(TARGET_ARCHIV_DIR_PATH);

    const int ret = ::system(cmd.c_str());
    if ((ret == -1) || (ret == 127))
    {
        throw CouldNotExtractCertStore(TARGET_ARCHIVE_MTD_CERT_STORE, std::string(TARGET_ARCHIV_DIR_PATH));
    }

    std::remove(target_mtd_cert_store.c_str());

    if (update_du_json == true && use_mdt_part_cert == false)
    {
        /* lets write default values */
        Json::StreamWriterBuilder builder_writer;
        std::unique_ptr<Json::StreamWriter> writer(builder_writer.newStreamWriter());
        std::ofstream fus_json_du(FUS_AZURE_CONFIGURATION, std::ofstream::out);
        if (!fus_json_du.good())
        {
            if (fus_json_du.bad() || fus_json_du.fail())
            {
                throw OpenAzureConfigDocument(FUS_AZURE_CONFIGURATION, FileOpenMode::WRITE);
            }
        }
        writer->write(this->root, &fus_json_du);
        fus_json_du.close();
    }
}

void x509_store::CertMMCstore::ExtractCertStore(const std::filesystem::path &path_to_ramdisk, const std::string bootdevice)
{
    const bool update_du_json = this->parseDuJsonConfig();
    /* use default file path for secure data */
    const std::filesystem::path dev {R"(/dev)"};
    const std::filesystem::path path_to_update_image(dev / bootdevice);
    bool use_part_cert = true;
    std::unique_ptr<struct fs_header_v1_0> fsheader10 = std::make_unique<struct fs_header_v1_0>();
    std::ifstream update_img(path_to_update_image, (std::ifstream::in | std::ifstream::binary));
    std::string target_update_store = (path_to_ramdisk / std::string("tmp.tar.bz2"));
    std::string update_image_file = path_to_update_image;
    int target_sector = EMMC_SECURE_PART_BLK_NR;

    // open file
    if (!update_img.good())
    {
        if (update_img.bad() || update_img.fail())
        {
            std::string error_str = std::string("Open file ") + update_image_file + std::string("fails");
            throw OpenMMCDevFailed(SOURCE_ARCHIVE_MMC_FILE_PATH);
        }
    }

    update_img.seekg(target_sector * DEFAULT_SECTOR_SIZE);

    update_img.read((char *)fsheader10.get(), sizeof(struct fs_header_v1_0));

    uint64_t file_size = 0;
    file_size = fsheader10->info.file_size_high & 0xFFFFFFFF;
    file_size = file_size << 32;
    file_size = file_size | (fsheader10->info.file_size_low & 0xFFFFFFFF);

    if (!std::strcmp("CERT", fsheader10->type) && (file_size > 0))
    {
        uint64_t cursor;
        char BUFFER[BUFSIZ] = {0};
#ifdef DEBUG
        std::cout << "FS-Header available " << std::endl;
#endif
        std::ofstream archive_store(target_update_store, (std::ofstream::out | std::ofstream::binary));
        if (!archive_store.good())
        {
            if (archive_store.bad() || archive_store.fail())
            {
                std::string error_str = std::string("Open file ") + target_update_store + std::string("fails");
                throw CreateCertStore(target_update_store);
            }
        }
        for(cursor = 0; (cursor + sizeof(BUFFER)) <= file_size; cursor += sizeof(BUFFER))
        {
            update_img.read((char *) BUFFER, sizeof(BUFFER));
            archive_store.write(BUFFER, sizeof(BUFFER));
        }
        if(cursor < file_size)
        {
            uint64_t rest_size = (file_size - cursor);
            update_img.read((char *) BUFFER, rest_size);
            archive_store.write(BUFFER, rest_size);
        }
        archive_store.flush();
        archive_store.close();
        update_img.close();
    }
    else
    {
        throw CreateCertStore(std::string("Update has wrong format"));
    }
#ifdef DEBUG
    std::cout << "File " << target_update_store << " written." << std::endl;
#endif

    if (!std::filesystem::exists(target_update_store))
    {
        throw OpenMMCDevFailed(target_update_store);
    }

    std::string cmd = uncompress_cmd_source_archive;
    cmd += target_update_store;
    cmd += uncompress_cmd_dest_folder;
    cmd += std::string(TARGET_ARCHIV_DIR_PATH);

    const int ret = ::system(cmd.c_str());
    if ((ret == -1) || (ret == 127))
    {
        throw CouldNotExtractCertStore(SOURCE_ARCHIVE_MMC_FILE_PATH, std::string(TARGET_ARCHIV_DIR_PATH));
    }

    remove(target_update_store.c_str());

    if (update_du_json == true && use_part_cert == false)
    {
        Json::StreamWriterBuilder builder_writer;
        std::unique_ptr<Json::StreamWriter> writer(builder_writer.newStreamWriter());
        std::ofstream fus_json_du(FUS_AZURE_CONFIGURATION, std::ofstream::out);
        if (!fus_json_du.good())
        {
            if (fus_json_du.bad() || fus_json_du.fail())
            {
                throw OpenAzureConfigDocument(FUS_AZURE_CONFIGURATION, FileOpenMode::WRITE);
            }
        }
        writer->write(this->root, &fus_json_du);
        fus_json_du.close();
    }
}

OverlayDescription::Persistent x509_store::prepare_ramdisk_readable(const std::filesystem::path &path_to_ramdisk)
{
    if (!std::filesystem::exists(path_to_ramdisk))
    {
        if (!std::filesystem::create_directories(path_to_ramdisk))
        {
            throw(CreateRAMfsMountpoint(path_to_ramdisk));
        }
    }

    Mount mount;

    mount.wrapper_c_mount("none", path_to_ramdisk, "size=8M", "tmpfs", 0);

    OverlayDescription::Persistent ramdisk;
    ramdisk.lower_directory = std::string(TARGET_ADU_DIR_PATH);
    ramdisk.merge_directory = std::string(TARGET_ADU_DIR_PATH);

    ramdisk.upper_directory = path_to_ramdisk;
    ramdisk.upper_directory += std::string("/upper");

    ramdisk.work_directory = path_to_ramdisk;
    ramdisk.work_directory += std::string("/work");

    // Get permissions from lower directory
    struct stat lower_stat;
    if (stat(ramdisk.lower_directory.c_str(), &lower_stat) != 0) {
        throw std::runtime_error("Failed to get lower directory permissions");
    }

    // Create directories
    if (!std::filesystem::exists(ramdisk.upper_directory))
    {
        std::filesystem::create_directories(ramdisk.upper_directory);

        // Apply permissions and ownership from lower to upper directory
        chmod(ramdisk.upper_directory.c_str(), lower_stat.st_mode & 07777);
        chown(ramdisk.upper_directory.c_str(), lower_stat.st_uid, lower_stat.st_gid);
        if (chown(ramdisk.upper_directory.c_str(),
                  lower_stat.st_uid,
                  lower_stat.st_gid) != 0) {
            std::cerr << "Warning: Failed to set ownership of upper directory: "
                      << strerror(errno) << std::endl;
        }
    }

    if (!std::filesystem::exists(ramdisk.work_directory))
    {
        std::filesystem::create_directories(ramdisk.work_directory);
        // Optional: Also set the same permissions for work directory
        chmod(ramdisk.work_directory.c_str(), lower_stat.st_mode & 07777);
        chown(ramdisk.work_directory.c_str(), lower_stat.st_uid, lower_stat.st_gid);
        if (chown(ramdisk.work_directory.c_str(),
                  lower_stat.st_uid,
                  lower_stat.st_gid) != 0) {
            std::cerr << "Warning: Failed to set ownership of work directory: "
                      << strerror(errno) << std::endl;
        }
    }

    mount.mount_overlay_persistent(ramdisk);
    return ramdisk;
}

OverlayDescription::ReadOnly x509_store::prepare_readonly_overlay_from_ramdisk (const std::filesystem::path &path_to_ramdisk)
{
    Mount mount;
    mount.wrapper_c_mount("none", path_to_ramdisk, "", "tmpfs", MS_REMOUNT | MS_RDONLY);

    OverlayDescription::ReadOnly ramdisk_ro;
    ramdisk_ro.lower_directory = path_to_ramdisk;
    ramdisk_ro.lower_directory += std::string("/upper");

    ramdisk_ro.merge_directory = TARGET_ADU_DIR_PATH;

    return ramdisk_ro;
}

bool x509_store::close_ramdisk(const std::filesystem::path &path_to_ramdisk)
{
    try {
        // Check if the ramdisk directory exists
        if (!std::filesystem::exists(path_to_ramdisk)) {
            return false;
        }

        // First attempt to unmount
        Mount mount;
        mount.wrapper_c_umount(path_to_ramdisk);
    } catch (const std::exception& e) {
        // Error handling
        return false;
    }
    return true;
}