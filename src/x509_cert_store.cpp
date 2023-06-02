#include "x509_cert_store.h"

#include <vector>
#include <memory>
#include <sstream>
//#include <iostream> /* Add for cout... */

/* set max nr. of mtd devices to max number of ubi volumes */
#define MAX_NR_MTD_DEVICES 128

#ifndef PART_NAME_MTD_CERT
#define PART_NAME_MTD_CERT Secure
#endif

x509_store::CertStore::CertStore()
{
    if (!std::filesystem::exists(FUS_AZURE_CONFIGURATION))
    {
        throw std::runtime_error(std::string("File: ") + std::string(FUS_AZURE_CONFIGURATION) + std::string(" does not exists"));
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
        if (mtd_line.find(PART_NAME_MTD_CERT) != std::string::npos)
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
    std::string arch_mtd_file_path = SOURCE_ARCHIVE_MTD_FILE_PATH;
    std::string target_mtd_cert_store = TARGET_ARCHIVE_MTD_CERT_STORE;
    std::unique_ptr<struct fs_header_v1_0> fsheader10 = std::make_unique<struct fs_header_v1_0>();
    /* scan for secure partition if partition is available
     * then use this.
     */
    if (ScanForPartition(PART_NAME_MTD_CERT) == 0) {
        /* use secure partition to read data */
        arch_mtd_file_path = "/dev/mtd" + std::to_string(GetPartitionNumber());
        use_mdt_part_cert = true;
    }

    std::ifstream archive_mdt(arch_mtd_file_path, (std::ifstream::in | std::ifstream::binary));

    // open file
    if (!archive_mdt.good())
    {
        if (archive_mdt.bad() || archive_mdt.fail())
        {
            throw OpenMTDDevFailed(SOURCE_ARCHIVE_MTD_FILE_PATH);
        }
    }
    archive_mdt.read((char *)fsheader10.get(), sizeof(struct fs_header_v1_0));

    uint64_t file_size = 0;
    file_size = fsheader10->info.file_size_high & 0xFFFFFFFF;
    file_size = file_size << 32;
    file_size = file_size | (fsheader10->info.file_size_low & 0xFFFFFFFF);

    if (!std::strcmp("CERT", fsheader10->type) && (file_size > 0))
    {
        std::ofstream archive(target_mtd_cert_store, (std::ofstream::out | std::ofstream::binary));
        if (!archive.good())
        {
            if (archive.bad() || archive.fail())
            {
                throw CreateCertStore(target_mtd_cert_store);
            }
        }
        archive << archive_mdt.rdbuf();
    }
    else
    {
        throw NoCERTTypeFSFile();
    }

    std::string cmd = uncompress_cmd_source_archive;
    cmd += std::string(TARGET_ARCHIVE_MTD_CERT_STORE);
    cmd += uncompress_cmd_dest_folder;
    cmd += std::string(TARGET_ARCHIV_DIR_PATH);

    const int ret = ::system(cmd.c_str());
    if ((ret == -1) || (ret == 127))
    {
        throw CouldNotExtractCertStore(TARGET_ARCHIVE_MTD_CERT_STORE, std::string(TARGET_ARCHIV_DIR_PATH));
    }

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
    }
}

void x509_store::CertMMCstore::ExtractCertStore(const std::filesystem::path &path_to_ramdisk)
{
    const bool update_du_json = this->parseDuJsonConfig();

    if (!std::filesystem::exists(SOURCE_ARCHIVE_MMC_FILE_PATH))
    {
        throw OpenMMCDevFailed(SOURCE_ARCHIVE_MMC_FILE_PATH);
    }

    std::string cmd = uncompress_cmd_source_archive;
    cmd += std::string(SOURCE_ARCHIVE_MMC_FILE_PATH);
    cmd += uncompress_cmd_dest_folder;
    cmd += std::string(TARGET_ARCHIV_DIR_PATH);

    const int ret = ::system(cmd.c_str());
    if ((ret == -1) || (ret == 127))
    {
        throw CouldNotExtractCertStore(SOURCE_ARCHIVE_MMC_FILE_PATH, std::string(TARGET_ARCHIV_DIR_PATH));
    }

    if (update_du_json == true)
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

    mount.wrapper_c_mount(
        "none",
        path_to_ramdisk,
        "",
        "ramfs",
        0);

    OverlayDescription::Persistent ramdisk;
    ramdisk.lower_directory = std::string(TARGET_ADU_DIR_PATH);
    ramdisk.merge_directory = std::string(TARGET_ADU_DIR_PATH);

    ramdisk.upper_directory = path_to_ramdisk;
    ramdisk.upper_directory += std::string("/upper");

    ramdisk.work_directory = path_to_ramdisk;
    ramdisk.work_directory += std::string("/work");

    mount.mount_overlay_persistent(ramdisk);
    return ramdisk;
}

OverlayDescription::ReadOnly x509_store::close_ramdisk(const std::filesystem::path &path_to_ramdisk)
{
    Mount mount;
    mount.wrapper_c_umount(TARGET_ADU_DIR_PATH);
    mount.wrapper_c_mount(
        "none",
        path_to_ramdisk,
        "",
        "ramfs",
        MS_REMOUNT | MS_RDONLY);

    OverlayDescription::ReadOnly ramdisk_ro;
    ramdisk_ro.lower_directory = path_to_ramdisk;
    ramdisk_ro.lower_directory += std::string("/upper");

    ramdisk_ro.merge_directory = TARGET_ADU_DIR_PATH;

    return ramdisk_ro;
}
