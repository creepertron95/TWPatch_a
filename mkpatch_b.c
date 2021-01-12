#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "soos/lz.h"
#include "soos/red.h"
#include "soos/pat.h"

#include "mkpatch_a.h"

#include "soos/common_main.h"

size_t lzss_enc2(const void* __restrict in, void* __restrict out, size_t insize, size_t outsize);

extern void sha256(const void* in, size_t inlen, uint8_t out[32]);

static void print_hash(uint8_t* hash)
{
    for(int i = 0; i != 32; i++)
        printf("%02X", hash[i]);
    
    puts("");
}

int main(int argc, char** argv)
{
    //size_t pat = PAT_HID | PAT_GPUSCALING | PAT_SQUISH;
    size_t pat = PAT_EHANDLER | PAT_K11_KMAP | PAT_K11_TLS | PAT_K11_EHAND | PAT_K11_MMAP | PAT_K11_UNPANIC | PAT_K11_EHPANIC | PAT_K11_SVCPERM;
    
    int iskernel = 0;
    
    const char* firstparam = argv[1];
    
    if(firstparam)
    {
        if(firstparam[0] == '.')
        {
            iskernel = 1;
            
            firstparam += 1;
        }
        
        
        if(!strcmp("agb", firstparam))
        {
            puts("AGBG Debug");
            agbg = 1;
        }
        
        if(argv[2])
        {
            const char* aaa = argv[2];
            
            pat = 0;
            
            while(*aaa)
            {
                pat = (pat << 1) | (*aaa & 1);
                ++aaa;
            }
        }
    }
    
    if(pat & PAT_RELOC)
    {
        puts("Relocation patch is obsolete! Please unmask the bit!");
        return 1;
    }
    
    uint8_t* codeptr = 0;
    size_t codesize = 0;
    uint32_t* codesizeptr = 0;
    
    uint8_t* codecptr = 0;
    size_t codecsize = 0;
    
    size_t firmsize = 0;
    uint8_t* firm = LoadFIRM(&firmsize);
    if(!firm)
    {
        puts("Failed to load FIRM");
        return 1;
    }
    
    size_t mirfsize = 0;
    uint8_t* mirf = FindSection0(firm, &mirfsize);
    if(mirf)
    {
        puts("Processing, please wait...");
        codecptr = ParseMIRF(mirf, mirfsize, &codecsize, &codesizeptr, &codeptr);
        if(!codecptr)
        {
            puts("Corrupted codebin");
            return 1;
        }
    }
    else
    {
        puts("\n\nFailed to find TwlBg, please read above");
        puts("Push SELECT to exit");
        
        return 1;
    }
    
    puts("Doing patches");
    
    if(iskernel)
    {
        uint8_t hash[32];
        sha256(mirf, (mirfsize + 0x1FF) & ~0x1FF, hash);
        
        print_hash(hash);
        print_hash(firm + 0x50);
        
        if(!memcmp(hash, firm + 0x50, 32))
            puts("Checksum success!");
        
        uint8_t* firmcopy = firm;
        
        puts("pat_apply_tower");
        pat_apply_tower(firmcopy + *(uint32_t*)(firmcopy + 0x70), *(uint32_t*)(firmcopy + 0x78), pat);
        puts("pat_apply_destroyer");
        pat_apply_destroyer(firmcopy + *(uint32_t*)(firmcopy + 0xA0), *(uint32_t*)(firmcopy + 0xA8), pat);
        puts("pat_apply_canine");
        pat_apply_canine(firmcopy + *(uint32_t*)(firmcopy + 0xD0), *(uint32_t*)(firmcopy + 0xD8), pat);
        
        puts("sha256");
        sha256(firmcopy + *(uint32_t*)(firmcopy + 0x70), *(uint32_t*)(firmcopy + 0x78), hash);
        memcpy(firmcopy + 0x80, hash, 32);
        sha256(firmcopy + *(uint32_t*)(firmcopy + 0xA0), *(uint32_t*)(firmcopy + 0xA8), hash);
        memcpy(firmcopy + 0xB0, hash, 32);
        sha256(firmcopy + *(uint32_t*)(firmcopy + 0xD0), *(uint32_t*)(firmcopy + 0xD8), hash);
        memcpy(firmcopy + 0xE0, hash, 32);
        
        FILE* fo = fopen("twl.firm", "wb");
        if(fo)
        {
            if(fwrite(firmcopy, firmsize, 1, fo) == 1)
            {
                puts("Write success");
                fflush(fo);
            }
            else
            {
                puts("Write fail");
            }
            
            fclose(fo);
        }
        
        return 0;
    }
    
    uint8_t* resptr = 0;
    
    color_setting_t settest;
    settest.temperature = 3200;
    settest.gamma[0] = 1.0F;
    settest.gamma[1] = 1.0F;
    settest.gamma[2] = 1.0F;
    settest.brightness = 1.0F;
    pat_apply_background(codecptr, codecsize, &settest, pat);
    
    puts("Compressing... this will take a year or two");
    
    size_t outsize = ((*codesizeptr) + 0x1FF) & ~0x1FF;
    *codesizeptr = outsize;
    
    size_t lzres = lzss_enc2(codecptr, codeptr, codecsize, outsize);
    if(~lzres)
    {
        puts("Writing file to disk");
        char fnbuf[] = "/TwlBg.cxi\0";
        if(agbg)
            *(uint32_t*)(fnbuf) ^= 0xE101500;
        printf("fnbuf: %s\n", fnbuf);
        FILE* f = fopen(fnbuf + 1, "wb");
        if(f)
        {
            if(fwrite(mirf, mirfsize, 1, f) == 1)
            {
                fflush(f);
                fclose(f);
                puts("Patched TwlBg, ready to use!");
            }
            else
            {
                fclose(f);
                puts("Failed to write TwlBg.cxi to disk :/");
            }
        }
        else
        {
            puts("Can't write TwlBg.cxi to disk...");
            puts(" FAIL ");
        }
    }
    else
    {
        puts("Failed to compress...");
        puts("  Something went terribly wrong :/");
    }
    
    return 0;
}
