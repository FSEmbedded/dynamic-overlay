#include "x509_cert_store.h"
#include "config.h"
#include "logging.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>

#include <archive.h>
#include <archive_entry.h>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
}

namespace {
inline constexpr uint32_t MAX_NR_MTD_DEVICES = 128;
inline constexpr std::size_t CERT_BUF_SIZE = 1024;
inline constexpr int DEFAULT_SECTOR_SIZE = 512;
} // anonymous namespace

Error x509_store::CertStore::load_json_config() noexcept
{
    if (!posix_utils::path_exists(config::fus_azure_configuration)) {
        LOG_ERROR("azure config not found: " + std::string(config::fus_azure_configuration));
        return Error::config_not_found;
    }

    std::string content;
    Error err = posix_utils::read_file_to_string(config::fus_azure_configuration, content);
    if (err != Error::none) {
        return err;
    }

    Json::CharReaderBuilder reader;
    std::string errs;
    std::unique_ptr<Json::CharReader> parser(reader.newCharReader());
    if (!parser->parse(content.data(), content.data() + content.size(), &root_, &errs)) {
        LOG_ERROR("JSON parse error: " + errs);
        return Error::json_parse_failed;
    }

    return Error::none;
}

Error x509_store::CertStore::save_json_config() noexcept
{
    Json::StreamWriterBuilder builder;
    const std::string json_str = Json::writeString(builder, root_);
    return posix_utils::write_string_to_file(config::fus_azure_configuration, json_str);
}

Error x509_store::CertStore::init() noexcept
{
    return load_json_config();
}

Error x509_store::CertStore::parseDuJsonConfig(bool &config_updated) noexcept
{
    config_updated = false;

    if (root_["agents"][0]["connectionSource"]["connectionType"].asString() != "x509") {
        LOG_WARNING("no x509 configuration in du-config.json, skipping cert store");
        return Error::config_invalid;
    }

    const std::string x509_cert = root_["agents"][0]["connectionSource"]["x509_cert"].asString();
    const std::string x509_key = root_["agents"][0]["connectionSource"]["x509_key"].asString();
    const std::string x509_container = root_["agents"][0]["connectionSource"]["x509_container"].asString();

    if (x509_cert != std::string(config::fus_azure_cert_certificate_name)) {
        root_["agents"][0]["connectionSource"]["x509_cert"] = std::string(config::fus_azure_cert_certificate_name);
        config_updated = true;
    }
    if (x509_key != std::string(config::fus_azure_cert_key_name)) {
        root_["agents"][0]["connectionSource"]["x509_key"] = std::string(config::fus_azure_cert_key_name);
        config_updated = true;
    }
    if (x509_container != std::string(config::target_archiv_dir_path)) {
        root_["agents"][0]["connectionSource"]["x509_container"] = std::string(config::target_archiv_dir_path);
        config_updated = true;
    }

    return Error::none;
}

namespace {

// RAII guard for libarchive read handle
struct ArchiveReadDeleter {
    void operator()(struct archive *a) const noexcept {
        if (a != nullptr) { archive_read_free(a); }
    }
};

// RAII guard for libarchive write-to-disk handle
struct ArchiveWriteDeleter {
    void operator()(struct archive *a) const noexcept {
        if (a != nullptr) { archive_write_free(a); }
    }
};

using ArchiveReadPtr = std::unique_ptr<struct archive, ArchiveReadDeleter>;
using ArchiveWritePtr = std::unique_ptr<struct archive, ArchiveWriteDeleter>;

Error copy_archive_data(struct archive *reader, struct archive *writer) noexcept
{
    const void *buf = nullptr;
    std::size_t size = 0;
    la_int64_t offset = 0;

    for (;;) {
        const int r = archive_read_data_block(reader, &buf, &size, &offset);
        if (r == ARCHIVE_EOF) { return Error::none; }
        if (r != ARCHIVE_OK) {
            LOG_ERROR("archive read error: " + std::string(archive_error_string(reader)));
            return Error::read_failed;
        }
        if (archive_write_data_block(writer, buf, size, offset) != ARCHIVE_OK) {
            LOG_ERROR("archive write error: " + std::string(archive_error_string(writer)));
            return Error::write_failed;
        }
    }
}

} // anonymous namespace

Error x509_store::extract_archive(std::string_view archive_path,
                                   std::string_view dest_dir) noexcept
{
    ArchiveReadPtr reader(archive_read_new());
    if (!reader) { return Error::cert_store_failed; }

    archive_read_support_filter_bzip2(reader.get());
    archive_read_support_format_tar(reader.get());

    const std::string archive(archive_path);
    if (archive_read_open_filename(reader.get(), archive.c_str(), CERT_BUF_SIZE) != ARCHIVE_OK) {
        LOG_ERROR("cannot open archive: " + std::string(archive_error_string(reader.get())));
        return Error::open_failed;
    }

    ArchiveWritePtr writer(archive_write_disk_new());
    if (!writer) { return Error::cert_store_failed; }

    archive_write_disk_set_options(writer.get(),
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);

    const std::string dest(dest_dir);
    struct archive_entry *entry = nullptr;

    for (;;) {
        const int r = archive_read_next_header(reader.get(), &entry);
        if (r == ARCHIVE_EOF) { break; }
        if (r != ARCHIVE_OK) {
            LOG_ERROR("archive header error: " + std::string(archive_error_string(reader.get())));
            return Error::cert_store_failed;
        }

        // Validate archive entry path before extraction
        const char *entry_path = archive_entry_pathname(entry);
        if (entry_path == nullptr || entry_path[0] == '/' ||
            std::strstr(entry_path, "..") != nullptr) {
            LOG_WARNING("skipping unsafe archive entry: " +
                        std::string(entry_path ? entry_path : "(null)"));
            continue;
        }

        const std::string full_path = dest + "/" + entry_path;
        archive_entry_set_pathname(entry, full_path.c_str());

        if (archive_write_header(writer.get(), entry) != ARCHIVE_OK) {
            LOG_ERROR("extract header failed: " + std::string(archive_error_string(writer.get())));
            return Error::write_failed;
        }

        if (archive_entry_size(entry) > 0) {
            const Error err = copy_archive_data(reader.get(), writer.get());
            if (err != Error::none) { return err; }
        }

        if (archive_write_finish_entry(writer.get()) != ARCHIVE_OK) {
            LOG_ERROR("extract finish failed: " + std::string(archive_error_string(writer.get())));
            return Error::write_failed;
        }
    }

    return Error::none;
}

bool x509_store::CertMDTstore::IsPartitionAvailable() const noexcept
{
    return uPartNumber_ <= MAX_NR_MTD_DEVICES;
}

uint32_t x509_store::CertMDTstore::GetPartitionNumber() const noexcept
{
    return uPartNumber_;
}

int x509_store::CertMDTstore::ScanForPartition(std::string_view part_name) noexcept
{
    if (IsPartitionAvailable()) {
        return 0; // Already resolved — skip rescan
    }

    std::string mtd_content;
    if (posix_utils::read_file_to_string("/proc/mtd", mtd_content) != Error::none) {
        LOG_ERROR("/proc/mtd not available");
        return -1;
    }

    // Parse line by line, skip header
    std::string_view remaining(mtd_content);
    int i = -1; // Start at -1 to skip header line
    while (!remaining.empty()) {
        const auto nl = remaining.find('\n');
        std::string_view line;
        if (nl != std::string_view::npos) {
            line = remaining.substr(0, nl);
            remaining.remove_prefix(nl + 1);
        } else {
            line = remaining;
            remaining = {};
        }

        if (i < 0) {
            ++i;
            continue; // skip header
        }

        // Match against quoted name field: mtdN: size erasesize "name"
        const auto quote_end = line.rfind('"');
        if (quote_end != std::string_view::npos && quote_end > 0) {
            const auto quote_start = line.rfind('"', quote_end - 1);
            if (quote_start != std::string_view::npos) {
                const auto name = line.substr(quote_start + 1, quote_end - quote_start - 1);
                if (name == part_name) {
                    uPartNumber_ = static_cast<uint32_t>(i);
                    return 0;
                }
            }
        }
        ++i;
    }

    return -1;
}

Error x509_store::CertMDTstore::ExtractCertStore() noexcept
{
    Error err = init();
    if (err != Error::none) return err;

    bool config_updated = false;
    err = parseDuJsonConfig(config_updated);
    if (err != Error::none) return err;

    if (ScanForPartition(config::part_name_mtd_cert) != 0) {
        LOG_ERROR("MTD partition not found: " + std::string(config::part_name_mtd_cert));
        return Error::cert_store_failed;
    }

    const std::string cert_device = "/dev/mtd" + std::to_string(GetPartitionNumber());
    const std::string temp_archive = std::string(config::target_archiv_dir_path) + "/tmp.tar.bz2";
    LOG_INFO("MTD cert source: " + cert_device);

    auto fsheader10 = std::make_unique<fs_header_v1_0>();

    FdGuard fd(::open(cert_device.c_str(), O_RDONLY));
    if (!fd.valid()) {
        LOG_ERRNO("open MTD cert source", errno);
        return Error::open_failed;
    }

    ssize_t bytes_read = ::read(fd.get(), fsheader10.get(), sizeof(fs_header_v1_0));
    if (bytes_read < static_cast<ssize_t>(sizeof(fs_header_v1_0))) {
        LOG_ERROR("failed to read FS header from MTD");
        return Error::read_failed;
    }

    uint64_t file_size = (static_cast<uint64_t>(fsheader10->info.file_size_high) << 32) |
                          static_cast<uint64_t>(fsheader10->info.file_size_low);

    if (std::strncmp("CERT", fsheader10->type, 4) != 0 || file_size == 0) {
        LOG_ERROR("not a CERT type FS file");
        return Error::cert_store_failed;
    }

    // Temp file cleanup on all exit paths (success or failure)
    ScopeGuard cleanup_temp([&temp_archive]() {
        static_cast<void>(posix_utils::remove_file(temp_archive));
    });

    {
        FdGuard fd_wr(::open(temp_archive.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
        if (!fd_wr.valid()) {
            LOG_ERRNO("create cert store temp file", errno);
            return Error::open_failed;
        }

        char buffer[CERT_BUF_SIZE];
        while (file_size > 0) {
            const std::size_t chunk = (file_size < sizeof(buffer))
                ? static_cast<std::size_t>(file_size) : sizeof(buffer);
            bytes_read = ::read(fd.get(), buffer, chunk);
            if (bytes_read <= 0) break;

            const ssize_t written = ::write(fd_wr.get(), buffer, static_cast<std::size_t>(bytes_read));
            if (written != bytes_read) {
                LOG_ERROR("incomplete write to cert store");
                return Error::write_failed;
            }
            file_size -= static_cast<uint64_t>(bytes_read);
        }
    }

    if (file_size > 0) {
        LOG_ERROR("incomplete read from MTD cert store");
        return Error::read_failed;
    }

    err = extract_archive(temp_archive, config::target_archiv_dir_path);

    if (err != Error::none) {
        return err;
    }

    if (config_updated) {
        const Error save_err = save_json_config();
        if (save_err != Error::none) {
            LOG_WARNING("failed to save du-config.json");
        }
    }

    return Error::none;
}

Error x509_store::CertMMCstore::ExtractCertStore(std::string_view bootdevice) noexcept
{
    Error err = init();
    if (err != Error::none) return err;

    bool config_updated = false;
    err = parseDuJsonConfig(config_updated);
    if (err != Error::none) return err;

    const std::string path_to_update_image = "/dev/" + std::string(bootdevice);
    const std::string temp_archive = std::string(config::target_archiv_dir_path) + "/tmp.tar.bz2";
    LOG_INFO("eMMC cert source: " + path_to_update_image
             + " block " + std::to_string(config::emmc_secure_part_blk_nr));

    FdGuard fd_in(::open(path_to_update_image.c_str(), O_RDONLY));
    if (!fd_in.valid()) {
        LOG_ERRNO("open MMC device", errno);
        return Error::open_failed;
    }

    const off_t target_offset = static_cast<off_t>(config::emmc_secure_part_blk_nr)
                                * static_cast<off_t>(DEFAULT_SECTOR_SIZE);
    if (::lseek(fd_in.get(), target_offset, SEEK_SET) == -1) {
        LOG_ERRNO("lseek to secure partition", errno);
        return Error::read_failed;
    }

    auto fsheader10 = std::make_unique<fs_header_v1_0>();
    ssize_t bytes_read = ::read(fd_in.get(), fsheader10.get(), sizeof(fs_header_v1_0));
    if (bytes_read < static_cast<ssize_t>(sizeof(fs_header_v1_0))) {
        LOG_ERROR("failed to read FS header from MMC");
        return Error::read_failed;
    }

    uint64_t file_size = (static_cast<uint64_t>(fsheader10->info.file_size_high) << 32) |
                          static_cast<uint64_t>(fsheader10->info.file_size_low);

    if (std::strncmp("CERT", fsheader10->type, 4) != 0 || file_size == 0) {
        LOG_ERROR("not a CERT type FS file on MMC");
        return Error::cert_store_failed;
    }

    // Temp file cleanup on all exit paths (success or failure)
    ScopeGuard cleanup_temp([&temp_archive]() {
        static_cast<void>(posix_utils::remove_file(temp_archive));
    });

    {
        FdGuard fd_out(::open(temp_archive.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
        if (!fd_out.valid()) {
            LOG_ERRNO("create cert store temp file", errno);
            return Error::open_failed;
        }

        char buffer[CERT_BUF_SIZE];
        uint64_t remaining = file_size;
        while (remaining > 0) {
            const std::size_t chunk = (remaining < sizeof(buffer))
                ? static_cast<std::size_t>(remaining) : sizeof(buffer);
            bytes_read = ::read(fd_in.get(), buffer, chunk);
            if (bytes_read <= 0) break;

            const ssize_t written = ::write(fd_out.get(), buffer, static_cast<std::size_t>(bytes_read));
            if (written != bytes_read) {
                LOG_ERROR("incomplete write to cert store");
                return Error::write_failed;
            }
            remaining -= static_cast<uint64_t>(bytes_read);
        }

        if (remaining > 0) {
            LOG_ERROR("incomplete read from MMC cert store");
            return Error::read_failed;
        }
    }

    LOG_DEBUG("cert store file written: " + temp_archive);

    err = extract_archive(temp_archive, config::target_archiv_dir_path);

    if (err != Error::none) {
        return err;
    }

    if (config_updated) {
        const Error save_err = save_json_config();
        if (save_err != Error::none) {
            LOG_WARNING("failed to save du-config.json");
        }
    }

    return Error::none;
}

