#include <fstream>
#include <filesystem>
#include <string>
#include <exception>
#include <stdexcept>

#include <cstdlib>
#include <cstring>

#include <json/json.h>

#include "mount.h"
#include <mtd/mtd-user.h>
#include <mtd/libmtd.h>

namespace x509_store
{
    class x509ExtractStore: public std::exception
    {
        protected:
            std::string error_string;
        
        public:
            const char * what() const throw () 
            {
                return this->error_string.c_str();
            }
    };

    class OpenMTDDevFailed: public x509ExtractStore
    {
        public:
            OpenMTDDevFailed(const std::string &f)
            {
                error_string = "Could not open MTD device: ";
                error_string += f;
            }
    };

    class OpenMMCDevFailed: public x509ExtractStore
    {
        public:
            OpenMMCDevFailed(const std::string &f)
            {
                error_string = "Could not open read store on MMC device: ";
                error_string += f;
            }
    };

    class CreateCertStore: public x509ExtractStore
    {
        public:
            CreateCertStore(const std::string &f)
            {
                error_string = "Could not create cert store: ";
                error_string += f;
            }
    };

    class NoCERTTypeFSFile: public x509ExtractStore
    {
        public:
            NoCERTTypeFSFile()
            {
                error_string = "Could not create cert store: ";
            }
    };

    enum class FileOpenMode : uint8_t {READ, WRITE};
    class OpenAzureConfigDocument: public x509ExtractStore
    {
        public:
            OpenAzureConfigDocument(const std::string &f, FileOpenMode f_mode)
            {
                error_string = "Could not open azure configuration: ";
                error_string += f;
                error_string += " in mode: ";
                if (f_mode == FileOpenMode::READ)
                {
                    error_string += "r";
                }
                else if (f_mode == FileOpenMode::WRITE)
                {
                    error_string += "w";
                }
            }
    };

    class ParseJsonDocumentError: public x509ExtractStore
    {
        public:
            ParseJsonDocumentError(const std::string &val)
            {
                error_string = "Error during parse json document: ";
                error_string += val;
            }
    };

    class NotAx509ConfigurationInDuJson: public x509ExtractStore
    {
        public:
            NotAx509ConfigurationInDuJson()
            {
                error_string = "No x509 configuration defined iniot hub configuration";
            }
    };

    class CouldNotExtractCertStore: public x509ExtractStore
    {
        public:
            CouldNotExtractCertStore(const std::string &source, const std::string &dest)
            {
                error_string =  "Could not extract: \"";
                error_string +=  source + std::string("\" to \"");
                error_string += dest + std::string("\"");
            }
    };

    class CreateRAMfsMountpoint : public x509ExtractStore
    {
        private:
            std::string error_msg;

        public:
            CreateRAMfsMountpoint(const std::filesystem::path &path)
            {
                this->error_msg = std::string("Error during creating mountpoint for RAMfs: ") + path.string();
            }
    };

    struct fs_header_v0_0 {		    /* Size: 16 Bytes */
        char magic[4];			    /* "FS" + two bytes operating system */
                                    /* (e.g. "LX" for Linux) */
        uint32_t file_size_low;		/* Image size [31:0] */
        uint32_t file_size_high;	/* Image size [63:32] */
        uint16_t flags;			    /* See flags below */
        uint8_t padsize;			/* Number of padded bytes at end */
        uint8_t version;			/* Header version x.y:
                                        [7:4] major x, [3:0] minor y */
    };

    struct fs_header_v1_0 {		    /* Size: 64 bytes */
        struct fs_header_v0_0 info;	/* Image info, see above */
        char type[16];			    /* Image type, e.g. "U-BOOT" */
        union {
            char descr[32];		    /* Description, null-terminated */
            uint8_t p8[32];		    /* 8-bit parameters */
            uint16_t p16[16];		/* 16-bit parameters */
            uint32_t p32[8];		/* 32-bit parameters */
            uint64_t p64[4];		/* 64-bit parameters */
        } param;
    };

    class CertStore
    {
        protected:
            std::ifstream iot_hub_conf;
            Json::Value root;
            const std::string uncompress_cmd_source_archive = "bunzip2 -c ";
            const std::string uncompress_cmd_dest_folder = " | tar x -C ";
    
            bool parseDuJsonConfig(); 

        public:
            CertStore();
            ~CertStore() = default;
    
            CertStore(const CertStore &) = delete;
            CertStore &operator=(const CertStore &) = delete;
            CertStore(CertStore &&) = delete;
            CertStore &operator=(CertStore &&) = delete;
    };

    OverlayDescription::Persistent prepare_ramdisk_readable(const std::filesystem::path &);

    OverlayDescription::ReadOnly close_ramdisk(const std::filesystem::path &);

    class CertMDTstore: public CertStore
    {
        private:
            uint32_t uPartNumber;
        private:
            bool IsPartitionAvailable();
            uint32_t GetPartitionNumber();
            int ScanForPartition(const std::string part_name);
        public:
            CertMDTstore():uPartNumber(255) {};
            ~CertMDTstore() = default;
            
            void ExtractCertStore(const std::filesystem::path & path_to_ramdisk);

            CertMDTstore(const CertMDTstore &) = delete;
            CertMDTstore &operator=(const CertMDTstore &) = delete;
            CertMDTstore(CertMDTstore &&) = delete;
            CertMDTstore &operator=(CertMDTstore &&) = delete;
    };

    class CertMMCstore: public CertStore
    {
        public:
            CertMMCstore() = default;
            ~CertMMCstore() = default;

            void ExtractCertStore(const std::filesystem::path & path_to_ramdisk);

            CertMMCstore(const CertMMCstore &) = delete;
            CertMMCstore &operator=(const CertMMCstore &) = delete;
            CertMMCstore(CertMMCstore &&) = delete;
            CertMMCstore &operator=(CertMMCstore &&) = delete;
    };
};
