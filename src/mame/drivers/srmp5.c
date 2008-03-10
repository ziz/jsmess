/*

Super Real Mahjong P5
(c)1994 SETA

preliminary driver by Tomasz Slanina


--

CPU   : R3560 (IDT MIPS R3000 derivative)
SOUND : TC6210AF (ST-0016)
OSC.  : 50.0000MHz 42.9545MHz

SX008-01.BIN ; CHR ROM
SX008-02.BIN ;  |
SX008-03.BIN ;  |
SX008-04.BIN ;  |
SX008-05.BIN ;  |
SX008-06.BIN ;  |
SX008-07.BIN ; /
SX008-08.BIN ; SOUND DATA
SX008-09.BIN ; /
SX008-11.BIN ; MAIN PRG
SX008-12.BIN ;  |
SX008-13.BIN ;  |
SX008-14.BIN ; /

*/

#include "driver.h"
#include "deprecat.h"
#include "cpu/mips/r3000.h"
#include "sound/st0016.h"
#include "st0016.h"

extern UINT8 *st0016_charram;
static UINT32 databank;

static UINT16 *tileram;
static UINT16 *sprram;

#define SPRITE_GLOBAL_X 0
#define SPRITE_GLOBAL_Y 1
#define SUBLIST_OFFSET	2
#define SUBLIST_LENGTH	3

#define SUBLIST_OFFSET_SHIFT 3
#define SPRITE_LIST_END_MARKER 0x8000

#define SPRITE_TILE    0
#define SPRITE_PALETTE 1
#define SPRITE_LOCAL_X 2
#define SPRITE_LOCAL_Y 3
#define SPRITE_SIZE    4

#define SPRITE_SUBLIST_ENTRY_LENGTH 8
#define SPRITE_LIST_ENTRY_LENGTH    4

#define SPRITE_DATA_GRANULARITY 0x80


static VIDEO_UPDATE( srmp5 )
{
	// sprite/tile flipping is not implemented
	int x,y,address,xs,ys,height,width,xw,yw,sprite,xb,yb,sizex,sizey;
	UINT16 *sprite_list=sprram;
	UINT16 *sprite_list_end=&sprram[0x1000]; //guess
	UINT8 *pixels=(UINT8 *)tileram;

	fillbitmap(bitmap,0,cliprect);

	while((sprite_list[SUBLIST_OFFSET]&SPRITE_LIST_END_MARKER)==0 && sprite_list<sprite_list_end)
	{
		UINT16 *sprite_sublist=&sprram[sprite_list[SUBLIST_OFFSET]<<SUBLIST_OFFSET_SHIFT];
		UINT16 sublist_length=sprite_list[SUBLIST_LENGTH];
		INT16 global_x,global_y;

		if(0!=sprite_list[SUBLIST_OFFSET])
		{
			global_x=(INT16)sprite_list[SPRITE_GLOBAL_X];
			global_y=(INT16)sprite_list[SPRITE_GLOBAL_Y];
			while(sublist_length)
			{
				sprite=sprite_sublist[SPRITE_TILE];
				x=(INT16)sprite_sublist[SPRITE_LOCAL_X]+global_x;
				y=(INT16)sprite_sublist[SPRITE_LOCAL_Y]+global_y;
				width=(sprite_sublist[SPRITE_SIZE]>>4)&0xf;
				height=(sprite_sublist[SPRITE_SIZE]>>12)&0xf;

				sizex=(sprite_sublist[SPRITE_SIZE]>>0)&0xf;
				sizey=(sprite_sublist[SPRITE_SIZE]>>8)&0xf;

				address=sprite*SPRITE_DATA_GRANULARITY;
				for(xw=0;xw<=width;xw++)
					for(yw=0;yw<=height;yw++)
					{
						xb=x+xw*(sizex+1);
						yb=y+yw*(sizey+1);
						for(ys=0;ys<=sizey;ys++)
						{
		 					for(xs=0;xs<=sizex;xs++)
		 					{
		 						UINT32 pen=pixels[address&(0x100000-1)];

		 						if(pen)
		 						{
		 							if(xb+xs<=screen->machine->screen[0].visarea.max_x && xb+xs>=screen->machine->screen[0].visarea.min_x && yb+ys<=screen->machine->screen[0].visarea.max_y && yb+ys>=screen->machine->screen[0].visarea.min_y )
		 							{
		 								UINT32 pixdata=paletteram32[pen+((sprite_sublist[SPRITE_PALETTE]&0xff)<<8)];
		 								*BITMAP_ADDR32(bitmap, yb+ys, xb+xs) = ((pixdata&0x7c00)>>7) | ((pixdata&0x3e0)<<6) | ((pixdata&0x1f)<<19);
		 							}
		 						}
		 						++address;
		 					}
		 				}

		 			}
				sprite_sublist+=SPRITE_SUBLIST_ENTRY_LENGTH;
				--sublist_length;
			}
		}
		sprite_list+=SPRITE_LIST_ENTRY_LENGTH;
	}

	return 0;
}

static WRITE32_HANDLER(bank_w)
{
	COMBINE_DATA(&databank);
}

static READ32_HANDLER(tileram_r)
{
	return videoram32[offset];
}

static WRITE32_HANDLER(tileram_w)
{
	COMBINE_DATA(&videoram32[offset]);
	tileram[offset]=videoram32[offset]&0xffff;
}

static READ32_HANDLER(spr_r)
{
	return spriteram32[offset];
}

static WRITE32_HANDLER(spr_w)
{
	COMBINE_DATA(&spriteram32[offset]);
	sprram[offset]=spriteram32[offset];
}

static READ32_HANDLER(data_r)
{
	UINT32 data;
	data=((databank>>4)&0xf)*0x100000; //guess
	data=memory_region(REGION_USER2)[data+offset*2]+memory_region(REGION_USER2)[data+offset*2+1]*256;
	return data|(data<<16);
}

static ADDRESS_MAP_START( srmp5_mem, ADDRESS_SPACE_PROGRAM, 32 )
	AM_RANGE(0x00000000, 0x000fffff) AM_RAM //maybe 0 - 2fffff ?
	AM_RANGE(0x002f0000, 0x002f7fff) AM_RAM
	AM_RANGE(0x01000000, 0x01000003) AM_WRITE(SMH_RAM)  // 0xaa .. watchdog ?
	AM_RANGE(0x01800004, 0x01800007) AM_READ(input_port_0_dword_r)
	AM_RANGE(0x01800008, 0x0180000b) AM_READ(input_port_1_dword_r)
	AM_RANGE(0x0180000c, 0x0180000f) AM_WRITE(bank_w)
	AM_RANGE(0x01800010, 0x01800013) AM_READ(input_port_2_dword_r) //multiplexed controls (selected by writes to 1c)
	AM_RANGE(0x01800014, 0x01800017) AM_READ(input_port_3_dword_r)
	AM_RANGE(0x0180001c, 0x0180001f) AM_WRITE(SMH_RAM)//c1 c2 c4 c8 => mahjong inputs (at $10) - bits 0-3
	AM_RANGE(0x01800200, 0x0180020f) AM_RAM  //sound related ? only few writes after boot
	AM_RANGE(0x01a00000, 0x01bfffff) AM_READ(data_r)

	AM_RANGE(0x0a000000, 0x0a0fffff) AM_READWRITE(spr_r, spr_w) AM_BASE(&spriteram32)
	AM_RANGE(0x0a100000, 0x0a1fffff) AM_RAM AM_BASE(&paletteram32)
	AM_RANGE(0x0a200000, 0x0a3fffff) AM_READWRITE(tileram_r, tileram_w) AM_BASE(&videoram32)

	AM_RANGE(0x1eff0000, 0x1eff001f) AM_WRITE(SMH_RAM)
	AM_RANGE(0x1eff003c, 0x1eff003f) AM_READNOP
	AM_RANGE(0x1fc00000, 0x1fdfffff) AM_READ(SMH_ROM) AM_WRITE(SMH_ROM) AM_REGION(REGION_USER1, 0)
	AM_RANGE(0x2fc00000, 0x2fdfffff) AM_READ(SMH_ROM) AM_WRITE(SMH_ROM) AM_REGION(REGION_USER1, 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START( st0016_mem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_ROMBANK(1)
	AM_RANGE(0xc000, 0xcfff) AM_READ(st0016_sprite_ram_r) AM_WRITE(st0016_sprite_ram_w)
	AM_RANGE(0xd000, 0xdfff) AM_READ(st0016_sprite2_ram_r) AM_WRITE(st0016_sprite2_ram_w)
	AM_RANGE(0xe000, 0xe7ff) AM_RAM
	AM_RANGE(0xe800, 0xe87f) AM_RAM
	AM_RANGE(0xe900, 0xe9ff) AM_RAM AM_WRITE(st0016_snd_w) AM_BASE(&st0016_sound_regs)
	AM_RANGE(0xea00, 0xebff) AM_READ(st0016_palette_ram_r) AM_WRITE(st0016_palette_ram_w)
	AM_RANGE(0xec00, 0xec1f) AM_READ(st0016_character_ram_r) AM_WRITE(st0016_character_ram_w)
	AM_RANGE(0xf000, 0xffff) AM_RAM
ADDRESS_MAP_END

READ8_HANDLER(st0016_dma_r);

static ADDRESS_MAP_START( st0016_io, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0xbf) AM_READ(st0016_vregs_r) AM_WRITE(st0016_vregs_w)
	AM_RANGE(0xc0, 0xc3) AM_NOP // data from other cpu ?
	AM_RANGE(0xe0, 0xe0) AM_WRITENOP
	AM_RANGE(0xe1, 0xe1) AM_WRITE(st0016_rom_bank_w)
	AM_RANGE(0xe2, 0xe2) AM_WRITE(st0016_sprite_bank_w)
	AM_RANGE(0xe3, 0xe4) AM_WRITE(st0016_character_bank_w)
	AM_RANGE(0xe5, 0xe5) AM_WRITE(st0016_palette_bank_w)
	AM_RANGE(0xe6, 0xe6) AM_WRITENOP
	AM_RANGE(0xe7, 0xe7) AM_WRITENOP
	AM_RANGE(0xf0, 0xf0) AM_READ(st0016_dma_r)
ADDRESS_MAP_END


static const struct r3000_config config =
{
	1,	/* 1 if we have an FPU, 0 otherwise */
	4096,	/* code cache size */
	4096	/* data cache size */
};

static INPUT_PORTS_START( srmp5 )
	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "0-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "0-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "0-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "0-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "0-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "0-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "0-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "0-7" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, "0-8" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "0-9" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "0-a" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "0-b" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "0-c" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "0-d" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
  PORT_DIPNAME( 0x4000, 0x4000, "0-e" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "0-f" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00010000, 0x00010000, "0-10" )
	PORT_DIPSETTING(      0x00010000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00020000, 0x00020000, "0-11" )
	PORT_DIPSETTING(      0x00020000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00040000, 0x00040000, "0-12" )
	PORT_DIPSETTING(      0x00040000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00080000, 0x00080000, "0-13" )
	PORT_DIPSETTING(      0x00080000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00100000, 0x00100000, "0-14" )
	PORT_DIPSETTING(      0x00100000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00200000, 0x00200000, "0-15" )
	PORT_DIPSETTING(      0x00200000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00400000, 0x00400000, "0-16" )
	PORT_DIPSETTING(      0x00400000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00800000, 0x00800000, "0-17" )
	PORT_DIPSETTING(      0x00800000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x01000000, 0x01000000, "0-18" )
	PORT_DIPSETTING(      0x01000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x02000000, 0x02000000, "0-19" )
	PORT_DIPSETTING(      0x02000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x04000000, 0x04000000, "0-1a" )
	PORT_DIPSETTING(      0x04000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x08000000, 0x08000000, "0-1b" )
	PORT_DIPSETTING(      0x08000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x10000000, 0x10000000, "0-1c" )
	PORT_DIPSETTING(      0x10000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x20000000, 0x20000000, "0-1d" )
	PORT_DIPSETTING(      0x20000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
  PORT_DIPNAME( 0x40000000, 0x40000000, "0-1e" )
	PORT_DIPSETTING(      0x40000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x80000000, 0x80000000, "0-1f" )
	PORT_DIPSETTING(      0x80000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "1-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "1-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "1-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "1-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "1-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "1-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "1-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Test mode" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, "1-8" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "1-9" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "1-a" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "1-b" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "1-c" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "1-d" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
  PORT_DIPNAME( 0x4000, 0x4000, "1-e" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "1-f" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_DIPNAME( 0x00010000, 0x00010000, "1-10" )
	PORT_DIPSETTING(      0x00010000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00020000, 0x00020000, "1-11" )
	PORT_DIPSETTING(      0x00020000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00040000, 0x00040000, "1-12" )
	PORT_DIPSETTING(      0x00040000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00080000, 0x00080000, "1-13" )
	PORT_DIPSETTING(      0x00080000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00100000, 0x00100000, "1-14" )
	PORT_DIPSETTING(      0x00100000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00200000, 0x00200000, "1-15" )
	PORT_DIPSETTING(      0x00200000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00400000, 0x00400000, "1-16" )
	PORT_DIPSETTING(      0x00400000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00800000, 0x00800000, "1-17" )
	PORT_DIPSETTING(      0x00800000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x01000000, 0x01000000, "1-18" )
	PORT_DIPSETTING(      0x01000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x02000000, 0x02000000, "1-19" )
	PORT_DIPSETTING(      0x02000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x04000000, 0x04000000, "1-1a" )
	PORT_DIPSETTING(      0x04000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x08000000, 0x08000000, "1-1b" )
	PORT_DIPSETTING(      0x08000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x10000000, 0x10000000, "1-1c" )
	PORT_DIPSETTING(      0x10000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x20000000, 0x20000000, "1-1d" )
	PORT_DIPSETTING(      0x20000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
  PORT_DIPNAME( 0x40000000, 0x40000000, "1-1e" )
	PORT_DIPSETTING(      0x40000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x80000000, 0x80000000, "1-1f" )
	PORT_DIPSETTING(      0x80000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )

PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "2-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "2-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "2-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "2-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "2-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x0040, 0x0040, "2-5" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_DIPNAME( 0x0100, 0x0100, "2-8" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "2-9" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "2-a" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "2-b" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "2-c" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "2-d" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
  PORT_DIPNAME( 0x4000, 0x4000, "2-e" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "2-f" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )


	PORT_DIPNAME( 0x00010000, 0x00010000, "2-10" )
	PORT_DIPSETTING(      0x00010000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00020000, 0x00020000, "2-11" )
	PORT_DIPSETTING(      0x00020000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00040000, 0x00040000, "2-12" )
	PORT_DIPSETTING(      0x00040000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00080000, 0x00080000, "2-13" )
	PORT_DIPSETTING(      0x00080000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00100000, 0x00100000, "2-14" )
	PORT_DIPSETTING(      0x00100000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00200000, 0x00200000, "2-15" )
	PORT_DIPSETTING(      0x00200000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00400000, 0x00400000, "2-16" )
	PORT_DIPSETTING(      0x00400000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00800000, 0x00800000, "2-17" )
	PORT_DIPSETTING(      0x00800000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x01000000, 0x01000000, "2-18" )
	PORT_DIPSETTING(      0x01000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x02000000, 0x02000000, "2-19" )
	PORT_DIPSETTING(      0x02000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x04000000, 0x04000000, "2-1a" )
	PORT_DIPSETTING(      0x04000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x08000000, 0x08000000, "2-1b" )
	PORT_DIPSETTING(      0x08000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x10000000, 0x10000000, "2-1c" )
	PORT_DIPSETTING(      0x10000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x20000000, 0x20000000, "2-1d" )
	PORT_DIPSETTING(      0x20000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
  PORT_DIPNAME( 0x40000000, 0x40000000, "2-1e" )
	PORT_DIPSETTING(      0x40000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x80000000, 0x80000000, "2-1f" )
	PORT_DIPSETTING(      0x80000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "3-0" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, "3-1" )
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0004, 0x0004, "3-2" )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, "3-3" )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, "3-4" )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, "3-5" )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, "3-6" )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "3-7" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, "3-8" )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "3-9" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "3-a" )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "3-b" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "3-c" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "3-d" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
  PORT_DIPNAME( 0x4000, 0x4000, "3-e" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "3-f" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_DIPNAME( 0x00010000, 0x00010000, "3-10" )
	PORT_DIPSETTING(      0x00010000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00020000, 0x00020000, "3-11" )
	PORT_DIPSETTING(      0x00020000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00040000, 0x00040000, "3-12" )
	PORT_DIPSETTING(      0x00040000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00080000, 0x00080000, "3-13" )
	PORT_DIPSETTING(      0x00080000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00100000, 0x00100000, "3-14" )
	PORT_DIPSETTING(      0x00100000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00200000, 0x00200000, "3-15" )
	PORT_DIPSETTING(      0x00200000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00400000, 0x00400000, "3-16" )
	PORT_DIPSETTING(      0x00400000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x00800000, 0x00800000, "3-17" )
	PORT_DIPSETTING(      0x00800000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x01000000, 0x01000000, "3-18" )
	PORT_DIPSETTING(      0x01000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x02000000, 0x02000000, "3-19" )
	PORT_DIPSETTING(      0x02000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x04000000, 0x04000000, "3-1a" )
	PORT_DIPSETTING(      0x04000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x08000000, 0x08000000, "3-1b" )
	PORT_DIPSETTING(      0x08000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x10000000, 0x10000000, "3-1c" )
	PORT_DIPSETTING(      0x10000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x20000000, 0x20000000, "3-1d" )
	PORT_DIPSETTING(      0x20000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
  PORT_DIPNAME( 0x40000000, 0x40000000, "3-1e" )
	PORT_DIPSETTING(      0x40000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x80000000, 0x80000000, "3-1f" )
	PORT_DIPSETTING(      0x80000000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00000000, DEF_STR( On ) )
INPUT_PORTS_END

static const struct ST0016interface st0016_interface =
{
	&st0016_charram
};

static MACHINE_DRIVER_START( srmp5 )
	MDRV_CPU_ADD_TAG("main",Z80,8000000)
	MDRV_CPU_PROGRAM_MAP(st0016_mem,0)
	MDRV_CPU_IO_MAP(st0016_io,0)
	MDRV_CPU_VBLANK_INT("main", irq0_line_hold)

	MDRV_CPU_ADD(R3000LE, 40000000)
	MDRV_CPU_CONFIG(config)
	MDRV_CPU_VBLANK_INT_HACK(irq4_line_hold,2)
	MDRV_CPU_PROGRAM_MAP(srmp5_mem,0)


	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 350, 30, 280)

	MDRV_PALETTE_LENGTH(16*16*4+1)

	MDRV_VIDEO_START(st0016)
	MDRV_VIDEO_UPDATE(srmp5)

	MDRV_SPEAKER_STANDARD_STEREO("left", "right")

	MDRV_SOUND_ADD(ST0016, 0)
	MDRV_SOUND_CONFIG(st0016_interface)
	MDRV_SOUND_ROUTE(0, "left", 1.0)
	MDRV_SOUND_ROUTE(1, "right", 1.0)
MACHINE_DRIVER_END

ROM_START( srmp5 )
	ROM_REGION( 0x410000, REGION_CPU1, 0 )
	ROM_LOAD( "sx008-08.bin",   0x010000, 0x200000,   CRC(d4ac54f4) SHA1(c3dc76cd71485796a0b6a960294ea96eae8c946e) )
	ROM_LOAD( "sx008-09.bin",   0x200000, 0x200000,   CRC(5a3e6560) SHA1(92ea398f3c5e3035869f0ca5dfe7b05c90095318) )
	ROM_COPY( REGION_CPU1,  0x10000, 0x00000, 0x08000 )

	ROM_REGION32_BE( 0x200000, REGION_USER1, 0 )
	ROM_LOAD32_BYTE( "sx008-14.bin",   0x00000, 0x80000,   CRC(b5c55120) SHA1(0a41351c9563b2c6a00709189a917757bd6e0a24) )
	ROM_LOAD32_BYTE( "sx008-13.bin",   0x00001, 0x80000,   CRC(0af475e8) SHA1(24cddffa0f8c81832ae8870823d772e3b7493194) )
	ROM_LOAD32_BYTE( "sx008-12.bin",   0x00002, 0x80000,   CRC(43e9bb98) SHA1(e46dd98d2e1babfa12ddf2fa9b31377e8691d3a1) )
	ROM_LOAD32_BYTE( "sx008-11.bin",   0x00003, 0x80000,   CRC(ca15ff45) SHA1(5ee610e0bb835568c36898210a6f8394902d5b54) )

	ROM_REGION( 0x200000*8, REGION_USER2,0) /* gfx ? */
	ROM_LOAD( "sx008-01.bin",   0x000000, 0x200000,   CRC(82dabf48) SHA1(c53e9ed0056c431eab13ab362936c25d3cc5abba) )
	ROM_LOAD( "sx008-02.bin",   0x200000, 0x200000,   CRC(cfd2be0f) SHA1(a21f2928e08047c97443123aceba7ff4e95c6d3d) )
	ROM_LOAD( "sx008-03.bin",   0x400000, 0x200000,   CRC(d7323b10) SHA1(94ecc17b6b8b071cf2c61bbef4aec2c6c7693c62) )
	ROM_LOAD( "sx008-04.bin",   0x600000, 0x200000,   CRC(b10d3067) SHA1(21c36307780d4f38ec54d87cd222d65e4f8c00a5) )
	ROM_LOAD( "sx008-05.bin",   0x800000, 0x200000,   CRC(0ff5e6f5) SHA1(ab7d021757f341d28db6d7d009c20ec9d7bd83c1) )
	ROM_LOAD( "sx008-06.bin",   0xa00000, 0x200000,   CRC(ba6fd7c4) SHA1(f086195c5c647e07e77ce2a23e94d28e6ad9ff4f) )
	ROM_LOAD( "sx008-07.bin",   0xc00000, 0x200000,   CRC(3564485d) SHA1(12464de4e2b6c4df1595183996d1987f0ecffb01) )
ROM_END

static DRIVER_INIT(srmp5)
{
	st0016_game=9;
	tileram=(UINT16*)auto_malloc(0x100000);
	sprram=(UINT16*)auto_malloc(0x080000);
}

GAME( 1994, srmp5,	0,	  srmp5,    srmp5,    srmp5,    ROT0, "Seta",  "Super Real Mahjong P5", GAME_NOT_WORKING)
