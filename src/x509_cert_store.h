#pragma once

#include "error.h"
#include "posix_utils.h"

#include <cstdint>
#include <string_view>

#include <json/json.h>

// Optional x509 certificate store — conditionally compiled with --x509.
// Extracts certs from Secure partition (NAND MTD or eMMC block offset)
// to volatile tmpfs. Failures never block boot (graceful degradation).
namespace x509_store
{
    struct fs_header_v0_0 {
        char magic[4];
        uint32_t file_size_low;
        uint32_t file_size_high;
        uint16_t flags;
        uint8_t padsize;
        uint8_t version;
    };

    struct fs_header_v1_0 {
        struct fs_header_v0_0 info;
        char type[16];
        union {
            char descr[32];
            uint8_t p8[32];
            uint16_t p16[16];
            uint32_t p32[8];
            uint64_t p64[4];
        } param;
    };

    class CertStore
    {
    protected:
        Json::Value root_;

        [[nodiscard]] Error load_json_config() noexcept;
        [[nodiscard]] Error save_json_config() noexcept;
        [[nodiscard]] Error parseDuJsonConfig(bool &config_updated) noexcept;

    public:
        CertStore() noexcept = default;
        ~CertStore() noexcept = default;

        CertStore(const CertStore &) = delete;
        CertStore &operator=(const CertStore &) = delete;
        CertStore(CertStore &&) = delete;
        CertStore &operator=(CertStore &&) = delete;

        [[nodiscard]] Error init() noexcept;
    };

    [[nodiscard]] Error extract_archive(std::string_view archive_path,
                                         std::string_view dest_dir) noexcept;

    class CertMDTstore : public CertStore
    {
        uint32_t uPartNumber_ = 255;

        [[nodiscard]] bool IsPartitionAvailable() const noexcept;
        [[nodiscard]] uint32_t GetPartitionNumber() const noexcept;
        [[nodiscard]] int ScanForPartition(std::string_view part_name) noexcept;

    public:
        CertMDTstore() noexcept = default;
        ~CertMDTstore() noexcept = default;

        [[nodiscard]] Error ExtractCertStore() noexcept;

        CertMDTstore(const CertMDTstore &) = delete;
        CertMDTstore &operator=(const CertMDTstore &) = delete;
        CertMDTstore(CertMDTstore &&) = delete;
        CertMDTstore &operator=(CertMDTstore &&) = delete;
    };

    class CertMMCstore : public CertStore
    {
    public:
        CertMMCstore() noexcept = default;
        ~CertMMCstore() noexcept = default;

        [[nodiscard]] Error ExtractCertStore(std::string_view bootdevice) noexcept;

        CertMMCstore(const CertMMCstore &) = delete;
        CertMMCstore &operator=(const CertMMCstore &) = delete;
        CertMMCstore(CertMMCstore &&) = delete;
        CertMMCstore &operator=(CertMMCstore &&) = delete;
    };
}
