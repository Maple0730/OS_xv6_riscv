#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

#define FDT_MAGIC         0xd00dfeedU
#define FDT_BEGIN_NODE    0x1
#define FDT_END_NODE      0x2
#define FDT_PROP          0x3
#define FDT_NOP           0x4
#define FDT_END           0x9

struct fdt_header {
  uint32 magic;
  uint32 totalsize;
  uint32 off_dt_struct;
  uint32 off_dt_strings;
  uint32 off_mem_rsvmap;
  uint32 version;
  uint32 last_comp_version;
  uint32 boot_cpuid_phys;
  uint32 size_dt_strings;
  uint32 size_dt_struct;
};

uint64 boot_dtb = 0;
uint64 phys_ram_start = KERNBASE;
uint64 phys_ram_end = DEFAULT_PHYSTOP;
int phys_ram_detected = 0;

static uint32
fdt32(const uint32 *p)
{
  uint32 x = *p;
  return ((x & 0x000000ffU) << 24) | ((x & 0x0000ff00U) << 8) |
         ((x & 0x00ff0000U) >> 8) | ((x & 0xff000000U) >> 24);
}

static uint64
read_cells(const uint32 *cells, int n)
{
  uint64 v = 0;
  for (int i = 0; i < n; i++)
    v = (v << 32) | fdt32(&cells[i]);
  return v;
}

static uint64
align4(uint64 x)
{
  return (x + 3) & ~3ULL;
}

static int
name_is_memory(const char *name)
{
  return strncmp(name, "memory", 6) == 0 && (name[6] == 0 || name[6] == '@');
}

static int
name_eq(const char *a, const char *b)
{
  return strncmp(a, b, strlen(b) + 1) == 0;
}

static void
sanitize_phys_range(void)
{
  if (phys_ram_start < KERNBASE)
    phys_ram_start = KERNBASE;
  phys_ram_end = PGROUNDDOWN(phys_ram_end);
  if (phys_ram_end <= phys_ram_start) {
    phys_ram_start = KERNBASE;
    phys_ram_end = DEFAULT_PHYSTOP;
    phys_ram_detected = 0;
  }
}

void
memdetect(void)
{
  phys_ram_start = KERNBASE;
  phys_ram_end = DEFAULT_PHYSTOP;
  phys_ram_detected = 0;

  if (boot_dtb == 0) {
    sanitize_phys_range();
    return;
  }

  struct fdt_header *hdr = (struct fdt_header *)boot_dtb;
  if (fdt32(&hdr->magic) != FDT_MAGIC) {
    sanitize_phys_range();
    return;
  }

  uint32 totalsize = fdt32(&hdr->totalsize);
  uint32 off_struct = fdt32(&hdr->off_dt_struct);
  uint32 off_strings = fdt32(&hdr->off_dt_strings);
  uint32 size_struct = fdt32(&hdr->size_dt_struct);
  uint32 size_strings = fdt32(&hdr->size_dt_strings);
  if (totalsize < sizeof(*hdr) || off_struct >= totalsize ||
      off_strings >= totalsize || size_struct == 0 || size_strings == 0 ||
      off_struct + size_struct > totalsize ||
      off_strings + size_strings > totalsize) {
    sanitize_phys_range();
    return;
  }

  uchar *base = (uchar *)boot_dtb;
  uint32 *p = (uint32 *)(base + off_struct);
  uint32 *end = (uint32 *)(base + off_struct + size_struct);
  char *strings = (char *)(base + off_strings);

  int depth = -1;
  int memory_depth = -1;
  int root_addr_cells = 2;
  int root_size_cells = 2;

  while (p < end) {
    uint32 token = fdt32(p++);
    if (token == FDT_BEGIN_NODE) {
      char *name = (char *)p;
      depth++;
      if (depth == 1 && name_is_memory(name))
        memory_depth = depth;
      while ((uchar *)p < (uchar *)end && *(char *)p != 0)
        p = (uint32 *)((char *)p + 1);
      p = (uint32 *)align4((uint64)((char *)p + 1));
    } else if (token == FDT_END_NODE) {
      if (depth == memory_depth)
        memory_depth = -1;
      depth--;
    } else if (token == FDT_PROP) {
      if (p + 1 >= end)
        break;
      uint32 len = fdt32(p++);
      uint32 nameoff = fdt32(p++);
      if (nameoff >= size_strings)
        break;
      char *pname = strings + nameoff;
      uint32 *data = p;

      if (depth == 0 && len >= 4) {
        if (name_eq(pname, "#address-cells"))
          root_addr_cells = fdt32(data);
        else if (name_eq(pname, "#size-cells"))
          root_size_cells = fdt32(data);
      } else if (depth == memory_depth && name_eq(pname, "reg")) {
        int entry_cells = root_addr_cells + root_size_cells;
        int entry_bytes = entry_cells * (int)sizeof(uint32);
        if (root_addr_cells > 0 && root_addr_cells <= 2 &&
            root_size_cells > 0 && root_size_cells <= 2 && entry_cells > 0 &&
            entry_bytes > 0) {
          int count = len / entry_bytes;
          for (int i = 0; i < count; i++) {
            uint32 *entry = data + i * entry_cells;
            uint64 addr = read_cells(entry, root_addr_cells);
            uint64 size = read_cells(entry + root_addr_cells, root_size_cells);
            if (size == 0 || addr + size < addr)
              continue;
            if (!(addr <= KERNBASE && KERNBASE < addr + size))
              continue;
            phys_ram_start = addr;
            phys_ram_end = addr + size;
            phys_ram_detected = 1;
            sanitize_phys_range();
            return;
          }
        }
      }

      p = (uint32 *)align4((uint64)((uchar *)p + len));
    } else if (token == FDT_NOP) {
      continue;
    } else if (token == FDT_END) {
      break;
    } else {
      break;
    }
  }

  sanitize_phys_range();
}
