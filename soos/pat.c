#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pat.h"

typedef uint8_t u8;
typedef uint32_t u32;

#include "redshift_bin.h"
#include "hole_bin.h"
#include "reloc_bin.h"
#include "agb_wide_bin.h"
#include "twl_wide_bin.h"
#include "trainer_bin.h"
#include "ehandler_bin.h"
#include "ehook_bin.h"
#include "squish_bin.h"
#include "antiwear_bin.h"

//#define MEME_DEBUG

// only used for percentage display
const size_t PAT_HOLE_SIZE = 0x1E0; // run-once
const size_t HOK_HOLE_SIZE = 0x06C; // frame trainers
const size_t RTC_HOLE_SIZE = 0x130; // rtcom

const size_t PAT_HOLE_ADDR = 0x130000 - 0x100; // not used for actual payload positioning

extern int agbg;

__attribute__((optimize("Ofast"))) void* memesearch(const void* patptr, const void* bitptr, const void* searchptr, size_t searchlen, size_t patsize)
{
    const uint8_t* pat = patptr;
    const uint8_t* bit = bitptr;
    const uint8_t* src = searchptr;
    
    size_t i = 0;
    size_t j = 0;
    
    searchlen -= patsize;
    
    if(bit)
    {
        do
        {
            if((src[i + j] & ~bit[j]) == (pat[j] & ~bit[j]))
            {
                if(++j != patsize)
                    continue;
                
                #ifdef MEME_DEBUG
                printf("memesearch bit %X %X\n", i, j);
                for(j = 0; j != patsize; j++) printf("%02X", src[i + j]);
                puts("");
                for(j = 0; j != patsize; j++) printf("%02X", pat[j]);
                puts("");
                for(j = 0; j != patsize; j++) printf("%02X", bit[j]);
                puts("");
                #endif
                return src + i;
            }
            
            ++i;
            j = 0;
            
            continue;
        }
        while(i != searchlen);
    }
    else
    {
        do
        {
            if(src[i + j] == pat[j])
            {
                if(++j != patsize)
                    continue;
                
                #ifdef MEME_DEBUG
                printf("memsearch nrm %X %X\n", i, j);
                for(j = 0; j != patsize; j++) printf("%02X", src[i + j]);
                puts("");
                for(j = 0; j != patsize; j++) printf("%02X", pat[j]);
                puts("");
                #endif
                return src + i;
            }
            
            ++i;
            j = 0;
            
            continue;
        }
        while(i != searchlen);
    }
    
    return 0;
}

size_t pat_copyholerunonce(uint8_t* patchbuf, const color_setting_t* sets, size_t mask, size_t* outsize)
{
    if(!patchbuf)
        return mask & (PAT_REDSHIFT | PAT_WIDE | PAT_RELOC | PAT_EHANDLER | PAT_SQUISH | PAT_ANTIWEAR);
    
    uint16_t* tptr = (uint16_t*)patchbuf;
    
    uint32_t* reloclen = 0;
    
    uint32_t compress_o = 0;
    
    if((mask & PAT_REDSHIFT) && sets)
    {
        // copy Redshift decompressor code
        memcpy(tptr, redshift_bin, redshift_bin_size);
        tptr += (redshift_bin_size + 1) >> 1;
        
        // there is an ADR here in the decompressor
        uint32_t* compress_t = (uint32_t*)tptr;
        
        // the below code is taken from CTR_Redshift
        
        uint16_t c[0x300];
        uint8_t i = 0;
        uint8_t j = 0;
        
        do
        {
            *(c + i + 0x000) = i | (i << 8);
            *(c + i + 0x100) = i | (i << 8);
            *(c + i + 0x200) = i | (i << 8);
            //*(c + i + 0x300) = 0;
        }
        while(++i);
        
        colorramp_fill(c + 0x000, c + 0x100, c + 0x200, 0x100, sets);
        
        // compress the colorramp
        //  this compression algo abuses the fact
        //  that the colorramp is a ramp, and thus
        //  never goes down, allowing for
        //  extremely low compression ratio
        
        uint8_t prev[4];
        prev[0] = 0;
        prev[1] = 0;
        prev[2] = 0;
        prev[3] = 0;
        
        do
        {
            // it only goes to 3 instead of 4 to save bits
            // because the LCD is not transparent, save 256 bits
            // by not including the Alpha component
            for(j = 0; j != 3; j++)
            {
                uint8_t cur = c[i + (j << 8)] >> 8;
                if(cur < prev[j]) //never supposed to happen
                    puts("error wat");
                
                while(cur > prev[j]) // write increment bit while bigger
                {
                    prev[j]++;
                    compress_t[compress_o >> 5] |= 1 << (compress_o & 31);
                    compress_o++;
                }
                
                //write increment stop bit
                
                compress_t[compress_o >> 5] &= ~(1 << (compress_o & 31));
                compress_o++;
            }
        }
        while(++i);
        
        printf("compress_o bits=%i bytes=%X\n", compress_o, (compress_o + 7) >> 3);
        
        tptr += ((compress_o + 31) >> 5) << 1;
        
        // insert a security NOP just in case someting happens
        
        if(((uint8_t*)tptr - patchbuf) & 2) // how do you even
            *(tptr++) = 0x46C0; //NOP
        
        mask &= ~PAT_REDSHIFT;
    }
    
    if(mask & PAT_WIDE)
    {
        // these patches below modify the MTX hardware registers
        // DO NOT MODIFY THESE PATCHES
        
        if(!agbg)
        {
            memcpy(tptr, twl_wide_bin, twl_wide_bin_size);
            tptr += (twl_wide_bin_size + 1) >> 1;
        }
        else
        {
            memcpy(tptr, agb_wide_bin, agb_wide_bin_size);
            tptr += (agb_wide_bin_size + 1) >> 1;
        }
        
        if(((uint8_t*)tptr - patchbuf) & 2) // how do you even
            *(tptr++) = 0x46C0; //NOP
        
        mask &= ~PAT_WIDE;
    }
    
    // ===[Relocation]===
    
    if(mask & PAT_RELOC)
    {
        puts("Warning: Relocation is no longer supported. NO-OP");
        
        /*if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP
        
        memcpy(tptr, reloc_bin, reloc_bin_size);
        tptr += (reloc_bin_size + 1) >> 1;
        reloclen = ((uint32_t*)tptr) - 1;
        */
        
        if(outsize)
            outsize[1] = (size_t)((((uint8_t*)tptr - patchbuf) + 3) & ~3);
    }
    
    if(mask & PAT_EHANDLER)
    {
        puts("Reinstalling error handler");
        
        memcpy(tptr, ehandler_bin, ehandler_bin_size);
        tptr += (ehandler_bin_size + 1) >> 1;
        
        if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP
        
        mask &= ~PAT_EHANDLER;
    }
    
    if(mask & PAT_ANTIWEAR)
    {
        /*
        puts("Installing antiread antiwear");
        
        if(outsize)
            outsize[2] = (size_t)((((uint8_t*)tptr - patchbuf) + 3) & ~3);
        
        memcpy(tptr, antiwear_bin, antiwear_bin_size);
        tptr += (antiwear_bin_size + 1) >> 1;
        
        if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP
        */
        mask &= ~PAT_ANTIWEAR;
    }
    
    *(tptr++) = 0x4770; // BX LR
    *(tptr++) = 0x4770; // BX LR
    
    do
    {
        size_t currbyte = (((uint8_t*)tptr - patchbuf) + 3) & ~3;
        const size_t maxbyte = PAT_HOLE_SIZE;
        
        if(outsize)
            *outsize = currbyte;
        
        if(reloclen)
        {
            *reloclen = (((uint8_t*)tptr - (uint8_t*)reloclen) + 3) & ~3;
            printf("Relocation %X\n", *reloclen);
        }
        
        printf("Runonce bytes used: %X/%X, %u%%\n", currbyte, maxbyte, (currbyte * (100 * 0x10000) / maxbyte) >> 16);
    }
    while(0);
    
    return mask;
}

size_t pat_copyholehook(uint8_t* patchbuf, size_t mask, size_t* outsize)
{
    if(!patchbuf)
        return mask & (PAT_HOLE | PAT_RTCOM | PAT_EHANDLER);
    
    uint16_t* tptr = (uint16_t*)patchbuf;
    
    if(mask & PAT_EHANDLER)
    {
        puts("Installing dummy error handler");
        
        memcpy(tptr, ehook_bin, ehook_bin_size);
        tptr += (ehook_bin_size + 1) >> 1;
        
        if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP
        
        
        mask &= ~PAT_EHANDLER;
    }
    
    if(mask & PAT_HOLE)
    {
        memcpy(tptr, hole_bin, hole_bin_size);
        tptr += (hole_bin_size + 1) >> 1;
        
        if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP
        
        mask &= ~PAT_HOLE;
    }
    
    if(mask & PAT_RTCOM)
    {
        puts("Warning: rtcom is not supported in the hook hole, NO-OP");
        
        // Only fix glitchy branch if HOLE or RELOC mess up the chain flow
        //if(!(mask & PAT_HOLE) != !(mask & PAT_RELOC))
        /*if(mask & PAT_HOLE)
        {
            puts("Glitchy branch workaround");
            *(tptr++) = 0x46C0; //NOP
            *(tptr++) = 0x46C0; //NOP
            *(tptr++) = 0x46C0; //NOP
            *(tptr++) = 0x46C0; //NOP
            
            if(((uint8_t*)tptr - patchbuf) & 2)
                *(tptr++) = 0x46C0; //NOP
        }
        
        memcpy(tptr, trainer_bin, trainer_bin_size);
        tptr += (trainer_bin_size + 1) >> 1;
        
        if(((uint8_t*)tptr - patchbuf) & 2)
            *(tptr++) = 0x46C0; //NOP*/
    }
    
    *(tptr++) = 0x4770; // BX LR
    *(tptr++) = 0x4770; // BX LR
    
    do
    {
        size_t currbyte = (((uint8_t*)tptr - patchbuf) + 3) & ~3;
        const size_t maxbyte = HOK_HOLE_SIZE;
        
        if(outsize)
            *outsize = currbyte;
        
        printf("Trainer bytes used: %X/%X, %u%%\n", currbyte, maxbyte, (currbyte * (100 * 0x10000) / maxbyte) >> 16);
    }
    while(0);
    
    return mask;
}

static void makeblT(uint8_t** buf, size_t to, size_t from)
{
    uint32_t toffs = (to - (from + 4)) & ~1;
    
    printf("from: %X to: %X diff: %X\n", from, to, toffs);
    
    uint8_t* res = *buf;
    
    *(res++) = (uint8_t)(toffs >> 12);
    *(res++) = (uint8_t)(((toffs >> 20) & 7) | 0xF0);
    *(res++) = (uint8_t)(toffs >> 1);
    *(res++) = (uint8_t)(((toffs >> 9) & 7) | 0xF8);
    
    
    printf("%02X %02X %02X %02X\n", res[-4 + 0], res[-4 + 1], res[-4 + 2], res[-4 + 3]);
    
    *buf = res;
}

size_t pat_apply_background(uint8_t* codecptr, size_t codecsize, const color_setting_t* sets, size_t mask)
{
    uint8_t* resptr = 0;
    size_t patmask = mask;
    
    // ===[Always patch]===
    
    /*resptr = memesearch(
        // unique instructions to send parameters to svcKernelSetState
        (const uint8_t[]){0x00, 0x22, 0x04, 0x20, 0x13, 0x46, 0x11, 0x46},
        0, codecptr, codecsize, 8
    );
    
    if(resptr)
    {
        puts("Patching out memory unmap");
        
        resptr[0x00] = 0xC0;
        resptr[0x01] = 0x46;
        resptr[0x02] = 0xC0;
        resptr[0x03] = 0x46;
        resptr[0x04] = 0xC0;
        resptr[0x05] = 0x46;
        resptr[0x06] = 0xC0;
        resptr[0x07] = 0x46;
        resptr[0x08] = 0xC0;
        resptr[0x09] = 0x46;
        resptr[0x0A] = 0xC0;
        resptr[0x0B] = 0x46;
        resptr[0x0C] = 0xC0;
        resptr[0x0D] = 0x46;
        resptr[0x0E] = 0xC0;
        resptr[0x0F] = 0x46;
        resptr[0x10] = 0xC0;
        resptr[0x11] = 0x46;
        resptr[0x12] = 0xC0;
        resptr[0x13] = 0x46;
    }*/
    
    // === remove opposing DPAD check
    
    if(patmask & PAT_HID)
    {
        resptr = memesearch(
            // bit stuff for removing opposing DPAD bits
            (const uint8_t[]){0x40, 0x20, 0x20, 0x40, 0x20, 0x21, 0x21, 0x40, 0x40, 0x00, 0x49, 0x08, 0x08, 0x43, 0x84, 0x43},
            0, codecptr, codecsize, 16
        );
        
        if(resptr)
        {
            puts("Patching DPAD check");
            
            resptr[0x0] = 0xC0;
            resptr[0x1] = 0x46;
            resptr[0x2] = 0xC0;
            resptr[0x3] = 0x46;
            resptr[0x4] = 0xC0;
            resptr[0x5] = 0x46;
            resptr[0x6] = 0xC0;
            resptr[0x7] = 0x46;
            resptr[0x8] = 0xC0;
            resptr[0x9] = 0x46;
            resptr[0xA] = 0xC0;
            resptr[0xB] = 0x46;
            resptr[0xC] = 0xC0;
            resptr[0xD] = 0x46;
            resptr[0xE] = 0xC0;
            resptr[0xF] = 0x46;
            
            mask &= ~PAT_HID;
        }
    }
    
    // === Flip "START or SELECT to disable upscaling" to "START and SELECT to enable upscaling"
    
    //Personal note: I was wrong about the 4bit patch on two points:
    // 1) there are no conditional bits in Thumb
    // 2) only a single bit flip is required :)
    
    if(patmask & (PAT_UNSTART | PAT_ANTIWEAR))
    {
        resptr = memesearch(
            (const uint8_t[]){0x00, 0x88, 0xC0, 0x43, 0x00, 0x07, 0x80, 0x0F},
            0, codecptr, codecsize, 8);
        
        if(resptr)
        {
            if(patmask & PAT_ANTIWEAR)
            {
                puts("Flipping un-START regs");
                
                resptr[0] |= 7;
                resptr[2] |= 7 << 3;
            }
            
            if(patmask & PAT_UNSTART)
            {
                puts("Flipping un-START bit :)");
                
                // Turn MVN(S) into CMN (basically no-op in the given context)
                resptr[3] ^= 0b1; // :)
                
                mask &= ~PAT_UNSTART;
            }
        }
        else
        {
            puts("Failed to apply un-START patch");
        }
    }
    
    if((patmask & PAT_ANTIWEAR) && !agbg) // TwlBg-only, AgbBg doesn't touch NVFLASH
    {
        resptr = memesearch(
            (const uint8_t[]){0x04, 0x46, 0x01, 0x20},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            puts("Relocating NVFLASH touch scaling test");
            
            resptr[0] = 0x24;
            resptr[1] = 0x78;
        }
        
        resptr = memesearch(
            (const uint8_t[]){0x28, 0x46, 0x01, 0xF0, 0x18, 0xFB, 0x00, 0x20, 0x20, 0x56},
            (const uint8_t[]){0x00, 0x00, 0xFF, 0x07, 0xFF, 0x07, 0x00, 0x00, 0x00, 0x00},
            codecptr, codecsize, 10);
        
        if(resptr)
        {
            puts("Conditional NVFLASH write test");
            
            const int btn = 31 - 10;
            
            resptr[6] = (u8)(0x3F | (btn << 6));
            resptr[7] = 0x05 | (btn >> 2);
            
            resptr[8] = 0x01;
            resptr[9] = 0xD4;
        }
    }
    
    // === DMPGL patch for resolution changing
    
    if(patmask & (PAT_WIDE | PAT_SQUISH))
    {
        // (const uint8_t[]){0xD2, 0x4C, 0x00, 0x26, 0x00, 0x28}, DMA PATCH
        resptr = memesearch
        (
            // == DMPGL width patch
            !agbg ?
                (const uint8_t[]){0xFF, 0x24, 0x41, 0x34, 0xF0, 0x26}
            :
                (const uint8_t[]){0xFF, 0x25, 0x69, 0x35, 0xF0, 0x24}
                ,
            0, codecptr, codecsize, 6);
        if(resptr)
        {
            //TODO: experiment with the below two things
            // these values below change the letterbox size
            
            if(!(~patmask & (PAT_SQUISH | PAT_WIDE)))
            {
                puts("Applying wide-squish patch");
                
                if(!agbg)
                {
                    resptr[0] = (384 * 2) >> 8;
                    
                    resptr[2] = 0x24;
                    resptr[3] = 0x02;
                }
                else
                {
                    resptr[0] = (400 * 2) >> 4;
                    
                    resptr[2] = 0x2D;
                    resptr[3] = 0x01;
                }
            }
            else if(patmask & PAT_WIDE)
            {
                puts("Applying wide patch");
                
                if(!agbg)
                    resptr[2] += (384 - 320); //lol
                else
                    resptr[2] += (400 - 360);
            }
            else // PAT_SQUISH
            {
                puts("Applying squish patch");
                
                if(!agbg)
                {
                    resptr[0] = (320 * 2) >> 8;
                    
                    resptr[2] = 0x24;
                    resptr[3] = 0x02;
                }
                else
                {
                    resptr[0] = (360 * 2) >> 2;
                    
                    resptr[2] = 0xAD;
                    resptr[3] = 0x00;
                }
            }
            
            if(!agbg)
            {
                if(patmask & PAT_GPUSCALING)
                {
                    resptr = memesearch(
                        // TWL non-MTXscale GPUscale path
                        (const uint8_t[]){0xC0, 0x23, 0x00, 0x2F},
                        0, codecptr, codecsize, 4);
                    if(resptr)
                    {
                        puts("Applying DMPGL-GPU patch");
                        
                        if(patmask & PAT_WIDE)
                        {
                            // these two instructions set the texture resolution
                            
                            resptr[0] = 0xC0;
                            resptr[1] = 0x23;
                            
                            resptr[2] = 0xFF;
                            resptr[3] = 0x22;
                            resptr[4] = 0x41;
                            resptr[5] = 0x32;
                            
                            resptr[6] = 0x5F;
                            resptr[7] = 0x00;
                            
                            if(patmask & PAT_SQUISH)
                            {
                                resptr[6] = 0x9F;
                                resptr[7] = 0x00;
                            }
                            else
                            {
                                resptr[8] = 0xC0;
                                resptr[9] = 0x23;
                            }
                            
                            resptr[12] = 0xA7;
                            resptr[13] = 0x60;
                        }
                        else
                        {
                            resptr[0] = 0xC0;
                            resptr[1] = 0x23;
                            
                            resptr[2] = 0xFF;
                            resptr[3] = 0x22;
                            resptr[4] = 0x41;
                            resptr[5] = 0x32;
                            
                            resptr[6] = 0x5F;
                            resptr[7] = 0x00;
                            
                            if(patmask & PAT_SQUISH)
                            {
                                resptr[6] = 0x9F;
                                resptr[7] = 0x00;
                            }
                            else
                            {
                                resptr[8] = 0xC0;
                                resptr[9] = 0x23;
                            }
                            
                            resptr[12] = 0xA7;
                            resptr[13] = 0x60;
                        }
                    }
                }
                else
                {
                    // == ??? TODO document
                    resptr = memesearch(
                        // TWL MTX-scale non-GPUscale path
                        (const uint8_t[]){0x00, 0x2A, 0x04, 0xD0, 0x23, 0x61, 0xA1, 0x80, 0xA1, 0x60},
                        0, codecptr, codecsize, 10);
                    if(resptr)
                    {
                        puts("Applying DMPGL-MTX patch");
                        
                        // these two instructions set the texture resolution
                        
                        resptr[0] = 0xFF;
                        resptr[1] = 0x22;
                        resptr[2] = 0x81;
                        resptr[3] = 0x32;
                        resptr[4] = 0x03;
                        resptr[5] = 0xE0;
                        resptr[6] = 0xC0;
                        resptr[7] = 0x46;
                        
                        resptr[8] = 0xA2; // skip one instruction
                        
                        // Do not clear the mask here, as extra patches are needed below
                        //mask &= ~PAT_WIDE;
                    }
                }
            }
            else
            {
                // == ??? TODO DOCUMENT
                resptr = memesearch(
                    //(const uint8_t[]){0xFF, 0x21, 0x41, 0x31, 0xA3, 0x60, 0xE1, 0x60},
                    (const uint8_t[]){0x00, 0x01, 0x00, 0x00, 0x68, 0x01, 0x00, 0x00},
                    0, codecptr, codecsize, 8);
                if(resptr)
                {
                    puts("Applying DMPGL patch");
                    //resptr[2] += (400 - 360);
                    resptr[4] += (400 - 360);
                    
                    // Do not clear wide patch here
                    //mask &= ~PAT_WIDE;
                }
            }
        }
    }
    
    if(patmask & PAT_SQUISH)
    {
        resptr = memesearch(
            (const uint8_t[]){0x1B, 0x02, 0xD2, 0x1A},
            0, codecptr, codecsize, 4);
        if(resptr)
        {
            puts("Thin pixels force mode");
            
            resptr[4] = 0xC0;
            resptr[5] = 0x46;
            
            resptr[6] = 0xC0;
            resptr[7] = 0x46;
            
            mask &= ~PAT_SQUISH;
        }
        
        /*resptr = memesearch(
            (const uint8_t[]){0x80, 0x39, 0xFF, 0x23},
            0, codecptr, codecsize, 4);
        if(resptr)
        {
            puts("Thin pixels DMPGL");
            
            resptr[2] = 0xCB;
            resptr[3] = 0x00;
            
            resptr[10] = 0xE0;
            resptr[11] = 0x3B;
            
            mask &= ~PAT_SQUISH;
        }*/
    }
    
    if(patmask & PAT_GPUSCALING)
    {
        // This is a big fucking middle finger from the cmdlists
        #if 1
        resptr = memesearch(
            (const uint8_t[]){0x05, 0x1B, 0x41, 0xE2, 0xFF, 0x1F, 0x51, 0xE2, 0x00, 0x10, 0x92, 0x05, 0x00, 0x10, 0x80, 0x05},
            0, codecptr, codecsize, 16);
        
        if(resptr)
        {
            puts("Applying sharp GPU scaling patch");
            
            // LDR r5, [r2]
            resptr[0x08] = 0x00;
            resptr[0x09] = 0x50;
            resptr[0x0A] = 0x92;
            resptr[0x0B] = 0xE5;
            
            // CMP r1, #2
            resptr[0x0C] = 0x02;
            resptr[0x0D] = 0x00;
            resptr[0x0E] = 0x51;
            resptr[0x0F] = 0xE3;
            
            // MOVLT r5, #0x2600
            resptr[0x10] = 0x26;
            resptr[0x11] = 0x5C;
            resptr[0x12] = 0xA0;
            resptr[0x13] = 0xB3;
            
            // ORRLT r5, r5, #0x01
            resptr[0x14] = 0x01;
            resptr[0x15] = 0x50;
            resptr[0x16] = 0x85;
            resptr[0x17] = 0xB3;
            
            // STRLT r5, [r0, #0]
            resptr[0x18] = 0x00;
            resptr[0x19] = 0x50;
            resptr[0x1A] = 0x80;
            resptr[0x1B] = 0xB5;
            
            // STRLT r5, [r0, #4]
            resptr[0x1C] = 0x04;
            resptr[0x1D] = 0x50;
            resptr[0x1E] = 0x80;
            resptr[0x1F] = 0xB5;
            
            // BEQ --> BLT
            resptr[0x23] |= 0xB0;
            
            resptr[0x28] = 0x00;
            resptr[0x29] = 0xF0;
            resptr[0x2A] = 0x20;
            resptr[0x2B] = 0x03;
            
            resptr[0x2D] |= 0x40;
        }
        #endif
        
        resptr = memesearch(
            (const uint8_t[]){0x00, 0x01, 0x00, 0x02, 0x00, 0x22, 0x00, 0x00},
            0, codecptr, codecsize, 8);
        if(resptr)
        {
            puts("Patching scaling to linear #1");
            
            resptr[4] |= 3;
            
            resptr = memesearch(
                (const uint8_t[]){0x00, 0x01, 0x00, 0x02, 0x00, 0x22, 0x00, 0x00},
                0, resptr + 8, codecsize - (resptr - codecptr) - 8, 8);
            if(resptr)
            {
                puts("Patching scaling to linear #2");
                
                resptr[4] |= 3;
            }
        }
        
        // upscale enable  - r3 = 0, r2 = 1 - GPU stretch, stride fucked
        // upscale disable - r3 = 0, r2 = 0 - no stretch, stride fucked
        
        resptr = memesearch(
            // Patch params to twlgraphicsInit(zero, randomptr, bool DisableScaling, bool EnableGPU)
            !agbg ?
            //(const uint8_t[]){0x00, 0x23, 0x01, 0x22, 0x10, 0x31, 0x18, 0x46}, // upscaling disabled
            (const uint8_t[]){0x00, 0x23, 0x1A, 0x46, 0x10, 0x31, 0x18, 0x46}  // upscaling not disabled
            :
            (const uint8_t[]){0x00, 0x23, 0x01, 0x20, 0x18, 0x31, 0x1A, 0x46}, // upscaling not disabled
            0, codecptr, codecsize, 8);
        
        if(resptr)
        {
            puts("Applying GPU scaling patch #1");
            
            resptr[0] = 1;
            resptr[1] = 0x23;
            
            resptr[2] = 0;
            resptr[3] = 0x22;
            
            resptr[6] = !agbg ? 0 : 1;
            resptr[7] = 0x20;
        }
        
        resptr = memesearch(
            // Always keep MTX unscaled
            !agbg ?
            (const uint8_t[]){0x20, 0x78, 0x00, 0x28, 0x03, 0xD0, 0x00, 0x21}
            :
            (const uint8_t[]){0x00, 0x28, 0x01, 0xD0, 0x00, 0x21, 0x00, 0xE0, 0x02, 0x21},
            0, codecptr, codecsize, !agbg ? 8 : 10);
        
        if(resptr)
        {
            puts("Applying GPU scaling patch #2");
            
            if(!agbg)
            {
                resptr[5] &= ~0xF;
                resptr[5] |= 4;
            }
            else
            {
                resptr[8] = 0;
            }
        }
        
        /*if(agbg)
        {
            resptr = memesearch(
                (const uint8_t[]){0xFF, 0x22, 0xA1, 0x80, 0x41, 0x32, 0xA3, 0x60, 0xE2, 0x60, 0x20, 0x61, 0xA0, 0x21, 0x70, 0x82},
                0, codecptr, codecsize, 16);
            
            if(resptr)
            {
                puts("Applying AGBG DMPGL texture stride patch");
                
                resptr[-2] = 0x49;
                resptr[-1] = 0x00;
            }
        }*/
        
        uint8_t widepat[4];
        
        if(!agbg)
        {
            // MOV r1, #256
            widepat[0] = 0x01;
            widepat[1] = 0x1C;
            widepat[2] = 0xA0;
            widepat[3] = 0xE3;
        }
        else
        {
            // MOV r1, #240
            widepat[0] = 0xF0;
            widepat[1] = 0x10;
            widepat[2] = 0xA0;
            widepat[3] = 0xE3;
        }
        
        
        resptr = memesearch(
            (const uint8_t[]){0x03, 0x2A, 0x80, 0xED, 0x88, 0x10, 0x94, 0xE5, 0xB4, 0xE8, 0xD4, 0xE1},
            0, codecptr, codecsize, 12);
        
        if(resptr)
        {
            puts("Applying GPU scaling hotfix #1");
            
            resptr[4] = widepat[0];
            resptr[5] = widepat[1];
            resptr[6] = widepat[2];
            resptr[7] = widepat[3];
        }
        
        resptr = memesearch(
            (const uint8_t[]){0x05, 0x2A, 0x80, 0xED, 0x88, 0x10, 0x94, 0xE5, 0xB4, 0xE8, 0xD4, 0xE1},
            0, codecptr, codecsize, 12);
        
        if(resptr)
        {
            puts("Applying GPU scaling hotfix #2");
            
            resptr[4] = widepat[0];
            resptr[5] = widepat[1];
            resptr[6] = widepat[2];
            resptr[7] = widepat[3];
        }
        
        resptr = memesearch(
            (const uint8_t[]){0x03, 0x0A, 0xC0, 0xED, 0x8C, 0x10, 0x94, 0xE5, 0xB4, 0xE8, 0xD4, 0xE1},
            0, codecptr, codecsize, 12);
        
        if(resptr)
        {
            puts("Applying GPU scaling hotfix #3");
            
            resptr[4] = widepat[0];
            resptr[5] = widepat[1];
            resptr[6] = widepat[2];
            resptr[7] = widepat[3];
        }
        
        resptr = memesearch(
            (const uint8_t[]){0x05, 0x1A, 0x80, 0xED, 0x8C, 0x10, 0x94, 0xE5, 0xB4, 0xE8, 0xD4, 0xE1},
            0, codecptr, codecsize, 12);
        
        if(resptr)
        {
            puts("Applying GPU scaling hotfix #4");
            
            resptr[4] = widepat[0];
            resptr[5] = widepat[1];
            resptr[6] = widepat[2];
            resptr[7] = widepat[3];
        }
        
        resptr = memesearch(
            (const uint8_t[]){0x00, 0x00, 0x80, 0x3B},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            puts("Applying GPU scaling hotfix #-1");
            
            if(!agbg)
            {
                resptr[0] = 0xCC;
                resptr[1] = 0xCC;
                resptr[2] = 0x4C;
                resptr[3] = 0x3B;
            }
            else
            {
                resptr[0] = 0xAA;
                resptr[1] = 0xAA;
                resptr[2] = 0x2A;
                resptr[3] = 0x3B;
            }
        }
        
        mask &= ~PAT_GPUSCALING;
        
        mask &= ~PAT_WIDE;
        patmask &= ~PAT_WIDE;
    }
    
    // ALL of these require hooking
    if(patmask & (PAT_HOLE | PAT_REDSHIFT | PAT_RTCOM | PAT_WIDE | PAT_RELOC | PAT_DEBUG | PAT_GPUSCALING | PAT_EHANDLER | PAT_ANTIWEAR | PAT_SQUISH))
    {
        // ==[ Alternative code spaces, unused for now ]==
        
        // Unknown
        //resptr = memesearch((const uint8_t[]){0x00, 0x29, 0x03, 0xD0, 0x40, 0x69, 0x08, 0x60, 0x01, 0x20, 0x70, 0x47, 0x00, 0x20, 0x70, 0x47},
        
        size_t addroffs = 0;
            
        // === find relocation offset offset
        uint8_t* res2ptr = memesearch(
            //this works on both AGBG and TwlBg
            (const uint8_t[]){0x7F, 0x00, 0x01, 0x00, 0x00, 0x00, 0xF0, 0x1E},
            0, codecptr, codecsize, 8
        );
        
        if(res2ptr)
        {
            // fileoffs + addroffs == vaddr
            //   01FF00h+ addroffs == 100000h
            //   400000h-   31FF00h== 0E0100h
            addroffs = 0x400000 - *(uint32_t*)(res2ptr + 8);
        }
        else
        {
            puts("Warning: failed to find relocation offset, some patches won't apply");
        }
        
        // Copy runonce blob first, if needed
        if(addroffs && pat_copyholerunonce(0, 0, patmask, 0))
        {
            resptr = memesearch(
                // Start of the unused(?) MTX driver blob
                (const uint8_t[]){0x00, 0x23, 0x49, 0x01, 0xF0, 0xB5, 0x16, 0x01},
                0, codecptr, codecsize, 8
            );
            
            if(resptr)
            {
                // == post-init pre-unmap debug print hook (white screen before svcKernelSetState(4))
                // used for runonce purposes
                res2ptr = !agbg ?
                    memesearch
                    (
                        (const uint8_t[]){0x00, 0x28, 0x01, 0xDB, 0x11, 0x98, 0x40, 0x68, 0x58, 0xA1, 0x13, 0xF0, 0x8B, 0xF9},
                        (const uint8_t[]){0x00, 0x00, 0x07, 0x00, 0xFF, 0x00, 0xC0, 0x07, 0xFF, 0x00, 0xFF, 0x07, 0xFF, 0x07},
                        codecptr, codecsize,
                        14
                    )
                    :
                    memesearch
                    (
                        (const uint8_t[]){0x30, 0x46, 0x10, 0x38, 0xC0, 0x46, 0xC0, 0x46},
                        0, codecptr, codecsize, 8
                    );
                
                
                if(res2ptr && (!agbg || !((res2ptr - codecptr) & 3)))
                {
                    printf("runonce hook %X\n", (res2ptr - codecptr) + addroffs);
                    
                    res2ptr[0] = 0xC0;
                    res2ptr[1] = 0x46;
                    res2ptr[2] = 0xC0;
                    res2ptr[3] = 0x46;
                    res2ptr[4] = 0xC0;
                    res2ptr[5] = 0x46;
                    res2ptr[6] = 0xC0;
                    res2ptr[7] = 0x46;
                    
                    if(!agbg)
                    {
                        //there is more space in TwlBg to clear
                        res2ptr[ 8] = 0xC0;
                        res2ptr[ 9] = 0x46;
                        res2ptr[10] = 0xC0;
                        res2ptr[11] = 0x46;
                        res2ptr[12] = 0xC0;
                        res2ptr[13] = 0x46;
                    }
                    
                    if((res2ptr - codecptr) & 2)
                    {
                        *(res2ptr++) = 0xC0;
                        *(res2ptr++) = 0x46;
                    }
                    
                    if((resptr - codecptr) & 2)
                    {
                        *(resptr++) = 0xC0;
                        *(resptr++) = 0x46;
                    }
                    
                    // [0] - total hole usage
                    // [1] - offset from hole start to trainer code (per-frame)
                    // [2] - offset from hole start to trainer code (per-frame)
                    size_t copyret[3];
                    copyret[1] = ~0; // has to be initialized!
                    copyret[2] = ~0; // has to be initialized!
                    
                    // do not clear PAT_RELOC because it's cleared below
                    mask &= pat_copyholerunonce(resptr, sets, (patmask & ~PAT_RELOC), copyret) & ~PAT_RELOC;
                    
                    uint32_t sraddr = (res2ptr - codecptr) + addroffs;
                    uint32_t toaddr = (resptr - codecptr) + 0x300000;
                    
                    // BL debughook --> runonce
                    makeblT(&res2ptr, toaddr, sraddr);
                    /*
                    if((patmask & PAT_ANTIWEAR) && ~copyret[2])
                    {
                        res2ptr = memesearch(
                            (const uint8_t[]){0x60, 0x39, 0x40, 0x1C, 0x40, 0x06},
                            0, codecptr, codecsize, 6);
                        
                        if(res2ptr)
                        {
                            puts("Patching screenfill");
                            
                            res2ptr -= 0x10;
                            
                            sraddr = (res2ptr - codecptr) + 0x300000;
                            toaddr = (resptr - codecptr) + 0x300000 + copyret[2] + 16;
                            makeblT(&res2ptr, toaddr, sraddr);
                        }
                    }
                    */
                }
                else
                {
                    if(!res2ptr)
                        puts("Can't hook init function");
                    else if(agbg)
                        puts("Unlucky misalignment (AGBG)");
                    else
                        puts("sumimasen nan fakku how the hell did you even get here");
                }
            }
            else
            {
                puts("Can't apply runonce patches due to missing hole");
            }
        }
        else if(!addroffs)
        {
            puts("Can't apply runonce patches due to missing relocation offset");
        }
        
        if(addroffs && pat_copyholehook(0, patmask, 0))
        {
            // Frame hook ALWAYS has to be hooked, no matter if rtcom is on,
            //  or if there are any patches present. Either of them always does.
            do
            {
                // This is a really hacky way to find an unused initializer table
                //  and safely overwrite them without overwriting an used function in the middle
                
                const uint8_t spat[] = {0x04, 0x48, 0x00, 0x93, 0x13, 0x46, 0x0A, 0x46, 0x00, 0x68, 0x21, 0x46};
                const uint8_t smat[] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                
                resptr = memesearch(spat, smat,
                    codecptr, codecsize, sizeof(spat));
                
                if(resptr)
                {
                    // This is a two-pass pattern
                    resptr = memesearch(spat, smat,
                        resptr + 1, codecsize - (codecptr - resptr), sizeof(spat));
                }
                
                if(resptr)
                {
                    printf("pat2 at %X\n", (resptr - codecptr) + addroffs);
                    
                    resptr += 0x38; // Skip used function blob
                    
                    // Align search ptr (skip const data and find real func start)
                    if(resptr[1] != 0x48 || !resptr[3])
                    {
                        resptr += 4;
                        if(resptr[1] != 0x48 || !resptr[3])
                        {
                            printf("Invalid pattern match at %X\n", (resptr - codecptr) + addroffs);
                            resptr = 0;
                        }
                    }
                }
            }
            while(0);
            
            // Make sure we found the right blob at the very start
            if(resptr && (resptr - codecptr) < 0x20000)
            {
                printf("Frame hook target %X\n", (resptr - codecptr) + addroffs);
            
                if((resptr - codecptr) & 2) //must be 4-aligned for const pools
                {
                    *(resptr++) = 0xC0;
                    *(resptr++) = 0x46;
                }
                
                // Find frame hook
                res2ptr = !agbg ?
                    memesearch
                    (
                        // ???
                        (const uint8_t[]){0x80, 0x71, 0xDF, 0x0F, 0x18, 0x09, 0x12, 0x00},
                        (const uint8_t[]){0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00},
                        codecptr, codecsize, 8
                    )
                :
                    memesearch
                    (
                        //TODO: relies on magic DCW 0xFFFF
                        (const uint8_t[]){0xFF, 0xFF, 0x0B, 0x48, 0x10, 0xB5, 0x11, 0xF0},
                        (const uint8_t[]){0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00},
                        codecptr, codecsize, 8
                    )
                ;
                
                if(res2ptr)
                {
                    // the function is below the consts the memesearch pattern searches for
                    res2ptr += 0x10;
                    if(!agbg)
                        res2ptr += 6; // 0x16
                    
                    printf("Frame hook patch  %X\n", res2ptr-codecptr+addroffs);
                    
                    // MOV(S) r0, #0 around the BL
                    res2ptr[0] = 0x00;
                    res2ptr[1] = 0x20;
                    
                    res2ptr[2] = 0x00;
                    res2ptr[3] = 0x20;
                    res2ptr[4] = 0x00;
                    res2ptr[5] = 0x20;
                    
                    res2ptr[6] = 0x00;
                    res2ptr[7] = 0x20;
                    res2ptr[8] = 0x00;
                    res2ptr[9] = 0x20;
                    
                    if((res2ptr - codecptr) & 3)
                    {
                        *(res2ptr++) = 0xC0;
                        *(res2ptr++) = 0x46;
                        printf("Aligned code to   %X\n", res2ptr-codecptr+addroffs);
                    }
                    
                    printf("Unused init blob vaddr: %X\n", (resptr - codecptr) + addroffs);
                    
                    // destination to baked trainer init code (in 3xxxxx region, runonce)
                    uint32_t sraddr = (res2ptr - codecptr) + addroffs;
                    uint32_t toaddr = (resptr - codecptr) + addroffs;
                    
                    // BL framehook --> rtcom
                    makeblT(&res2ptr, toaddr, sraddr);
                    
                    // rtcom has its own dedicated hole
                    if(patmask & PAT_RTCOM)
                    {
                        memcpy(resptr, trainer_bin, trainer_bin_size);
                        
                        printf("rt bytes used: %X/%X, %u%%\n", trainer_bin_size + 4, RTC_HOLE_SIZE,
                            ((trainer_bin_size + 8) * (100 * 0x10000) / RTC_HOLE_SIZE) >> 16);
                        
                        resptr += (trainer_bin_size + 3) & ~3;
                        
                        mask &= ~PAT_RTCOM;
                    }
                    
                    uint8_t* mtxblob = resptr;
                    
                    if(patmask & PAT_RTCOM)
                        mtxblob = 0;
                    
                    if(pat_copyholehook(0, patmask & ~PAT_RTCOM, 0))
                    {
                        if(!mtxblob)
                        do
                        {
                            //10189C - 1018C4 - 319C0C
                            // == frame hook hole
                            const uint8_t* patn = (const uint8_t[])
                            { 0x02, 0x48, 0x10, 0xB5, 0x00, 0x68, 0x18, 0xF2, 0x75, 0xF9, 0x10, 0xBD, 0x18, 0x6F, 0x12, 0x00, 0x01, 0x46 };
                            const uint8_t* patb = (const uint8_t[])
                            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x07, 0xFF, 0x07, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00 };
                            
                            const size_t patl = 18;
                            
                            mtxblob = memesearch(patn, patb, codecptr, codecsize, patl);
                            if(mtxblob)
                                mtxblob = memesearch(patn, patb, mtxblob + 1, codecsize - (mtxblob - codecptr), patl);
                        }
                        while(0);
                        
                        if(mtxblob)
                        {
                            if((mtxblob - codecptr) & 2)
                            {
                                *(mtxblob++) = 0xC0;
                                *(mtxblob++) = 0x46;
                            }
                            
                            puts("Hooking rtcom --> extrapatches");
                            printf("%X --> %X\n", resptr - codecptr + addroffs, mtxblob - codecptr + addroffs);
                            
                            mask &= pat_copyholehook(mtxblob, patmask & ~PAT_RTCOM, 0);
                            
                            if(patmask & PAT_RTCOM)
                            {
                                // PUSH {r0, LR}
                                *(resptr++) = 0x01;
                                *(resptr++) = 0xB5;
                                // NOP
                                *(resptr++) = 0xC0;
                                *(resptr++) = 0x46;
                                
                                sraddr = (resptr - codecptr) + addroffs;
                                toaddr = (mtxblob - codecptr) + addroffs;
                                
                                // BL rtcom_end --> extrablob
                                makeblT(&resptr, toaddr, sraddr);
                                
                                // POP {r0, r1}
                                *(resptr++) = 0x03;
                                *(resptr++) = 0xBC;
                                // BX r1
                                *(resptr++) = 0x08;
                                *(resptr++) = 0x47;
                            }
                            
                            // ==[ DO NOT USE ]==
                            /*resptr = memesearch(
                                (const uint8_t[]){0x03, 0x21, 0x1F, 0x20, 0x49, 0x05, 0x00, 0x06, 0x10, 0xB5},
                                0, codecptr, codecsize, 10
                            );
                            if(resptr)
                            {
                                puts("Patching anti-trainer");
                                
                                resptr[0] = 0x70;
                                resptr[1] = 0x47;
                                resptr[2] = 0x70;
                                resptr[3] = 0x47;
                                resptr[4] = 0x70;
                                resptr[5] = 0x47;
                                resptr[6] = 0x70;
                                resptr[7] = 0x47;
                            }*/
                        }
                        else
                        {
                            puts("Failed to find Hole2");
                        }
                    }
                    
                    if(patmask & PAT_RTCOM)
                    {
                        // BX LR; BX LR;
                        *(resptr++) = 0x70;
                        *(resptr++) = 0x47;
                        *(resptr++) = 0x70;
                        *(resptr++) = 0x47;
                    }
                }
                else
                {
                    puts("Can't find frame hook");
                }
            }
            else
            {
                puts("Can't find unused init blob");
            }
        }
        else if(!addroffs)
        {
            puts("Can't apply frame hook due to missing relocation offset");
        }
    }
    
    return mask & patmask;
}

size_t pat_apply_tower(uint8_t* codecptr, size_t codecsize, size_t mask)
{
    uint8_t* resptr = 0;
    size_t patmask = mask;
    
    if(patmask & PAT_K11_KMAP)
    {
        resptr = memesearch(
            (const uint8_t[]){0x00, 0x20, 0x00, 0x90, 0x2A, 0x4B},
            0, codecptr, codecsize, 6);
        
        if(resptr)
        {
            uint8_t* bl = resptr - 6;
            
            uint32_t* chkptr = (uint32_t*)(((size_t)resptr & ~3) + 4);
            uint32_t ofs = 0;
            
            while((*chkptr & ~(0x2000)) != 0x14C06)
            {
                if(++ofs == 0x100)
                    break;
                chkptr++;
            }
            
            if(ofs != 0x100)
            {
                puts("AXI perm fixed");
                printf("%02X\n", ofs);
                
                resptr[0] = (uint8_t)ofs;
                resptr[1] = 0x48;
            }
            
            resptr += 4;
            chkptr = (uint32_t*)(((size_t)resptr & ~3) + 4);
            ofs = 0;
            
            while(*chkptr != 0x1FF80000)
            {
                if(++ofs == 0x100)
                    break;
                chkptr++;
            }
            
            if(ofs != 0x100)
            {
                puts("AXI address fixed");
                printf("%02X\n", ofs);
                
                resptr[0] = (uint8_t)ofs;
            }
            
            resptr += 8;
            
            ofs = bl[0] << 11;
            ofs |= (bl[1] & 7) << 19;
            ofs |= bl[2];
            ofs |= (bl[3] & 7) << 8;
            
            ofs -= (resptr - bl) >> 1;
            
            resptr[0] = ofs >> 11;
            resptr[1] &= ~7;
            resptr[1] |= (ofs >> 19) & 7;
            resptr[2] = ofs;
            resptr[3] &= ~7;
            resptr[3] |= (ofs >> 8) & 7;
            
            puts("AXI mapper fixed");
            printf("%08X\n", *(uint32_t*)bl);
            printf("%08X\n", *(uint32_t*)resptr);
            
            patmask &= ~PAT_K11_KMAP;
        }
    }
    
    if(patmask & PAT_K11_TLS)
    {
        
    }
    
    if(patmask & PAT_K11_EHAND)
    {
        resptr = memesearch(
            (const uint8_t[]){0x1D, 0x48, 0x70, 0xB5},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            uint32_t* chkptr = (uint32_t*)(((size_t)resptr & ~3) + 4);
            uint32_t kaddr = chkptr[*resptr];
            
            uint8_t* varptr = codecptr + (kaddr & 0x1FFFF) - 2;
            varptr[0] = 0xFF;
            varptr[1] = 0;
            varptr[2] = 0xFF;
            
            puts("Patched kernel variables");
            printf("%08X\n", kaddr);
            
            patmask &= ~PAT_K11_EHAND;
        }
    }
    
    if(patmask & PAT_K11_MMAP)
    {
        resptr = memesearch(
            (const uint8_t[]){0x01, 0x25, 0x43, 0x22},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            uint8_t* bl = resptr + 0x14;
            uint32_t offs = bl[0] << 11;
            offs |= (bl[1] & 7) << 19;
            offs |= bl[2];
            offs |= (bl[3] & 7) << 8;
            
            //patmask &= ~PAT_K11_MMAP;
        }
    }
    
    if(patmask & PAT_K11_UNPANIC)
    {
        const uint8_t* param1 = (const uint8_t[]){0xFE, 0xB5, 0x05, 0x46, 0x0F, 0x46};
        const uint8_t* param2 = (const uint8_t[]){0x83, 0x00, 0x00, 0x00, 0x01, 0x00};
        const size_t paramsize = 6;
        
        resptr = memesearch(param1, param2, codecptr, codecsize, paramsize);
        if(resptr)
        {
            printf("panic1: %X\n", resptr - codecptr);
            resptr = memesearch(param1, param2, resptr + 1, codecsize - (resptr - codecptr) - 1, paramsize);
        }
        
        if(resptr)
        {
            printf("panic2: %X\n", resptr - codecptr);
            
            printf("panicdata: %02X%02X %02X%02X %02X%02X\n",
                resptr[0], resptr[1],
                resptr[2], resptr[3],
                resptr[4], resptr[5]
                );
            
            //HACK: this is no good if the compiler decides to muck up the reg allocation again
            if(resptr[0] == 0xFE && resptr[4] == 0x0F)
            {
                resptr[4] = 2;
                resptr[5] = 0x27;
                
                puts("Unpanic for factory kernel");
            }
            else if(resptr[0] == 0x7C && resptr[4] == 0x0E)
            {
                resptr[4] = 2;
                resptr[5] = 0x26;
                
                puts("Unpanic applied");
            }
            else
            {
                puts("[!] Incompatible kernel for unpanic!");
            }
        }
        
        resptr = memesearch(
            (const uint8_t[]){0xF3, 0x0C, 0x00, 0x00},
            0, codecptr, codecsize, 4);
        if(resptr)
        {
            resptr[0] = 0xFF;
            
            puts("Patched panic key to L+R");
        }
        
        patmask &= ~PAT_K11_UNPANIC;
    }
    
    if(patmask & PAT_K11_EHPANIC)
    {
        resptr = memesearch(
            (const uint8_t[]){0x1D, 0x48, 0x70, 0xB5},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            /*
                PUSH {R4-R6,LR}
                LDR r2, =0xFFFF9004
                LDR r2, [r2]
                ADDS r2, #0x80
                LDR r2, [r2]
                ADDS r2, #0x94
                LDR r2, [r2]
                LDR r2, [r2, #0x40]
                MOV r0, SP
                MOVS r1, #0
                CMP r2, #0
                BEQ . + 4
                BLX r2
                POP {R4-R6,PC}
                .word 0xFFFF9004
            */
            
            // <autogen>
            
            *(resptr++) = 0x70;
            *(resptr++) = 0xb5;
            *(resptr++) = 0x06;
            *(resptr++) = 0x4a;
            *(resptr++) = 0x12;
            *(resptr++) = 0x68;
            *(resptr++) = 0x80;
            *(resptr++) = 0x32;
            *(resptr++) = 0x12;
            *(resptr++) = 0x68;
            *(resptr++) = 0x94;
            *(resptr++) = 0x32;
            *(resptr++) = 0x12;
            *(resptr++) = 0x68;
            *(resptr++) = 0x12;
            *(resptr++) = 0x6c;
            *(resptr++) = 0x68;
            *(resptr++) = 0x46;
            *(resptr++) = 0x00;
            *(resptr++) = 0x21;
            *(resptr++) = 0x00;
            *(resptr++) = 0x2a;
            *(resptr++) = 0x00;
            *(resptr++) = 0xd0;
            *(resptr++) = 0x90;
            *(resptr++) = 0x47;
            *(resptr++) = 0x70;
            *(resptr++) = 0xbd;
            *(resptr++) = 0x04;
            *(resptr++) = 0x90;
            *(resptr++) = 0xff;
            *(resptr++) = 0xff;
            
            // </autogen>
            
            puts("Kernel panic exception handler");
        }
        
        patmask &= ~PAT_K11_EHPANIC;
    }
    
    if(patmask & PAT_K11_SVCPERM)
    {
        resptr = memesearch(
            (const uint8_t[]){0x1B, 0x0E, 0x1A, 0xE1},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            resptr[0x8] = 0xFF;
            resptr[0x9] = 0xFF;
            resptr[0xA] = 0xFF;
            resptr[0xB] = 0xEA;
            
            puts("Patched SVC permission checks");
        }
        
        patmask &= ~PAT_K11_SVCPERM;
    }
    
    return mask & patmask;
}

size_t pat_apply_canine(uint8_t* codecptr, size_t codecsize, size_t mask)
{
    //uint8_t* resptr = 0;
    size_t patmask = mask;
    
    return mask & patmask;
}

size_t pat_apply_destroyer(uint8_t* codecptr, size_t codecsize, size_t mask)
{
    uint8_t* resptr = 0;
    size_t patmask = mask;
    
    if(patmask & PAT_K11_EHAND)
    {
        resptr = memesearch(
            (const uint8_t[]){0x1C, 0x30, 0x33, 0x46},
            0, codecptr, codecsize, 4);
        
        if(resptr)
        {
            uint8_t* res2ptr = memesearch(
                (const uint8_t[]){0x3E, 0x60, 0x7D, 0x60},
                0, codecptr, codecsize, 4);
            
            if(res2ptr && res2ptr > resptr)
            {
                uint32_t dist = ((res2ptr - resptr) >> 1) - 2;
                
                resptr[0] = dist;
                resptr[1] = 0xE0;
                
                puts("Patched broken MMU function");
                
                patmask &= ~PAT_K11_EHAND;
            }
        }
    }
    
    return mask & patmask;
}
