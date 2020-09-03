#pragma once

#include "red.h"

#define PAT_REDSHIFT    (1 << 0)
#define PAT_HOLE        (1 << 1)
#define PAT_RELOC       (1 << 2)
#define PAT_WIDE        (1 << 3)
#define PAT_HID         (1 << 4)
#define PAT_UNSTART     (1 << 5)
#define PAT_RTCOM       (1 << 6)
#define PAT_DEBUG       (1 << 7)
#define PAT_GPUSCALING  (1 << 8)
#define PAT_EHANDLER    (1 << 9)
#define PAT_SQUISH      (1 << 10)
#define PAT_ANTIWEAR    (1 << 11)

#define PAT_K11_KMAP    (1 << 20)
#define PAT_K11_TLS     (1 << 21)
#define PAT_K11_EHAND   (1 << 22)
#define PAT_K11_MMAP    (1 << 23)

void* memesearch(const void* patptr, const void* bitptr, const void* searchptr, size_t searchlen, size_t patsize);
size_t pat_apply_background(uint8_t* codecptr, size_t codecsize, const color_setting_t* sets, size_t mask);
size_t pat_apply_tower(uint8_t* codecptr, size_t codecsize, size_t mask);
size_t pat_apply_canine(uint8_t* codecptr, size_t codecsize, size_t mask);
size_t pat_apply_destroyer(uint8_t* codecptr, size_t codecsize, size_t mask);
