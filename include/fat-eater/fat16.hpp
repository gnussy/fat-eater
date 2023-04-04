#include <fmt/core.h>

#include <cstdint>

namespace fat_eater {
#pragma pack(push, 1)  // ensure that the structure is packed
  struct FAT16_BootSector {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    void print() const {
      fmt::print("oem: {}\n", oem);
      fmt::print("bytesPerSector: {}\n", bytes_per_sector);
      fmt::print("sectorsPerCluster: {}\n", sectors_per_cluster);
      fmt::print("reservedSectorCount: {}\n", reserved_sector_count);
      fmt::print("fatCount: {}\n", fat_count);
      fmt::print("rootEntryCount: {}\n", root_entry_count);
    }
  };
#pragma pack(pop)
}  // namespace fat_eater
