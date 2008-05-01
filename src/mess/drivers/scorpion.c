/***************************************************************************

	NOTE: ****** Specbusy: press N, R, or E to boot *************


        Spectrum/Inves/TK90X etc. memory map:

    CPU:
        0000-3fff ROM
        4000-ffff RAM

        Spectrum 128/+2/+2a/+3 memory map:

        CPU:
                0000-3fff Banked ROM/RAM (banked rom only on 128/+2)
                4000-7fff Banked RAM
                8000-bfff Banked RAM
                c000-ffff Banked RAM

        TS2068 memory map: (Can't have both EXROM and DOCK active)
        The 8K EXROM can be loaded into multiple pages.

    CPU:
                0000-1fff     ROM / EXROM / DOCK (Cartridge)
                2000-3fff     ROM / EXROM / DOCK
                4000-5fff \
                6000-7fff  \
                8000-9fff  |- RAM / EXROM / DOCK
                a000-bfff  |
                c000-dfff  /
                e000-ffff /


Interrupts:

Changes:

29/1/2000   KT -    Implemented initial +3 emulation.
30/1/2000   KT -    Improved input port decoding for reading and therefore
            correct keyboard handling for Spectrum and +3.
31/1/2000   KT -    Implemented buzzer sound for Spectrum and +3.
            Implementation copied from Paul Daniel's Jupiter driver.
            Fixed screen display problems with dirty chars.
            Added support to load .Z80 snapshots. 48k support so far.
13/2/2000   KT -    Added Interface II, Kempston, Fuller and Mikrogen
            joystick support.
17/2/2000   DJR -   Added full key descriptions and Spectrum+ keys.
            Fixed Spectrum +3 keyboard problems.
17/2/2000   KT -    Added tape loading from WAV/Changed from DAC to generic
            speaker code.
18/2/2000   KT -    Added tape saving to WAV.
27/2/2000   KT -    Took DJR's changes and added my changes.
27/2/2000   KT -    Added disk image support to Spectrum +3 driver.
27/2/2000   KT -    Added joystick I/O code to the Spectrum +3 I/O handler.
14/3/2000   DJR -   Tape handling dipswitch.
26/3/2000   DJR -   Snapshot files are now classifed as snapshots not
            cartridges.
04/4/2000   DJR -   Spectrum 128 / +2 Support.
13/4/2000   DJR -   +4 Support (unofficial 48K hack).
13/4/2000   DJR -   +2a Support (rom also used in +3 models).
13/4/2000   DJR -   TK90X, TK95 and Inves support (48K clones).
21/4/2000   DJR -   TS2068 and TC2048 support (TC2048 Supports extra video
            modes but doesn't have bank switching or sound chip).
09/5/2000   DJR -   Spectrum +2 (France, Spain), +3 (Spain).
17/5/2000   DJR -   Dipswitch to enable/disable disk drives on +3 and clones.
27/6/2000   DJR -   Changed 128K/+3 port decoding (sound now works in Zub 128K).
06/8/2000   DJR -   Fixed +3 Floppy support
10/2/2001   KT  -   Re-arranged code and split into each model emulated.
            Code is split into 48k, 128k, +3, tc2048 and ts2048
            segments. 128k uses some of the functions in 48k, +3
            uses some functions in 128, and tc2048/ts2048 use some
            of the functions in 48k. The code has been arranged so
            these functions come in some kind of "override" order,
            read functions changed to use  READ8_HANDLER and write
            functions changed to use WRITE8_HANDLER.
            Added Scorpion256 preliminary.
18/6/2001   DJR -   Added support for Interface 2 cartridges.
xx/xx/2001  KS -    TS-2068 sound fixed.
            Added support for DOCK cartridges for TS-2068.
            Added Spectrum 48k Psycho modified rom driver.
            Added UK-2086 driver.
23/12/2001  KS -    48k machines are now able to run code in screen memory.
                Programs which keep their code in screen memory
                like monitors, tape copiers, decrunchers, etc.
                works now.
                Fixed problem with interrupt vector set to 0xffff (much
            more 128k games works now).
                A useful used trick on the Spectrum is to set
                interrupt vector to 0xffff (using the table
                which contain 0xff's) and put a byte 0x18 hex,
                the opcode for JR, at this address. The first
                byte of the ROM is a 0xf3 (DI), so the JR will
                jump to 0xfff4, where a long JP to the actual
                interrupt routine is put. Due to unideal
                bankswitching in MAME this JP were to 0001 what
                causes Spectrum to reset. Fixing this problem
                made much more software runing (i.e. Paperboy).
            Corrected frames per second value for 48k and 128k
            Sincalir machines.
                There are 50.08 frames per second for Spectrum
                48k what gives 69888 cycles for each frame and
                50.021 for Spectrum 128/+2/+2A/+3 what gives
                70908 cycles for each frame.
            Remaped some Spectrum+ keys.
                Presing F3 to reset was seting 0xf7 on keyboard
                input port. Problem occured for snapshots of
                some programms where it was readed as pressing
                key 4 (which is exit in Tapecopy by R. Dannhoefer
                for example).
            Added support to load .SP snapshots.
            Added .BLK tape images support.
                .BLK files are identical to .TAP ones, extension
                is an only difference.
08/03/2002  KS -    #FF port emulation added.
                Arkanoid works now, but is not playable due to
                completly messed timings.

Initialisation values used when determining which model is being emulated:
 48K        Spectrum doesn't use either port.
 128K/+2    Bank switches with port 7ffd only.
 +3/+2a     Bank switches with both ports.

Notes:
 1. No contented memory.
 2. No hi-res colour effects (need contended memory first for accurate timing).
 3. Multiface 1 and Interface 1 not supported.
 4. Horace and the Spiders cartridge doesn't run properly.
 5. Tape images not supported:
    .TZX, .SPC, .ITM, .PAN, .TAP(Warajevo), .VOC, .ZXS.
 6. Snapshot images not supported:
    .ACH, .PRG, .RAW, .SEM, .SIT, .SNX, .ZX, .ZXS, .ZX82.
 7. 128K emulation is not perfect - the 128K machines crash and hang while
    running quite a lot of games.
 8. Disk errors occur on some +3 games.
 9. Video hardware of all machines is timed incorrectly.
10. EXROM and HOME cartridges are not emulated.
11. The TK90X and TK95 roms output 0 to port #df on start up.
12. The purpose of this port is unknown (probably display mode as TS2068) and
    thus is not emulated.

Very detailed infos about the ZX Spectrum +3e can be found at

http://www.z88forever.org.uk/zxplus3e/

*******************************************************************************/

#include "driver.h"
#include "includes/spectrum.h"
#include "eventlst.h"
#include "devices/snapquik.h"
#include "devices/cartslot.h"
#include "devices/cassette.h"
#include "sound/ay8910.h"
#include "sound/speaker.h"
#include "formats/tzx_cas.h"

/* +3 hardware */
#include "machine/nec765.h"
#include "devices/dsk.h"

/****************************************************************************************************/
/* BETADISK/TR-DOS disc controller emulation */
/* microcontroller KR1818VG93 is a russian wd179x clone */
#include "machine/wd17xx.h"

/*
DRQ (D6) and INTRQ (D7).
DRQ - signal showing request of data by microcontroller
INTRQ - signal of completion of execution of command.
*/

static int betadisk_status;
static int betadisk_active;

static void betadisk_wd179x_callback(running_machine *machine, wd17xx_state_t state, void *param)
{
	switch (state)
	{
		case WD17XX_DRQ_SET:
		{
			betadisk_status |= (1<<6);
		}
		break;

		case WD17XX_DRQ_CLR:
		{
			betadisk_status &=~(1<<6);
		}
		break;

		case WD17XX_IRQ_SET:
		{
			betadisk_status |= (1<<7);
		}
		break;

		case WD17XX_IRQ_CLR:
		{
			betadisk_status &=~(1<<7);
		}
		break;
	}
}

MACHINE_START( scorpion )
{
	betadisk_active = 0;
	betadisk_status = 0x03f;
	wd17xx_init(machine, WD_TYPE_179X, betadisk_wd179x_callback, NULL);
}

/****************************************************************************************************/
/* Zs Scorpion 256 */

/*
port 7ffd. full compatibility with Zx spectrum 128. digits are:

D0-D2 - number of RAM page to put in C000-FFFF
D3    - switch of address for RAM of screen. 0 - 4000, 1 - c000
D4    - switch of ROM : 0-zx128, 1-zx48
D5    - 1 in this bit will block further output in port 7FFD, until reset.
*/

/*
port 1ffd - additional port for resources of computer.

D0    - block of ROM in 0-3fff. when set to 1 - allows read/write page 0 of RAM
D1    - selects ROM expansion. this rom contains main part of service monitor.
D2    - not used
D3    - used for output in RS-232C
D4    - extended RAM. set to 1 - connects RAM page with number 8-15 in
    C000-FFFF. number of page is given in gidits D0-D2 of port 7FFD
D5    - signal of strobe for interface centronics. to form the strobe has to be
    set to 1.
D6-D7 - not used. ( yet ? )
*/

/* rom 0=zx128, 1=zx48, 2 = service monitor, 3=tr-dos */

static int scorpion_256_port_1ffd_data = 0;

static void scorpion_update_memory(running_machine *machine)
{
	unsigned char *ChosenROM;
	int ROMSelection;
	read8_machine_func rh;
	write8_machine_func wh;

	if (spectrum_128_port_7ffd_data & 8)
	{
		logerror("SCREEN 1: BLOCK 7\n");
		spectrum_128_screen_location = mess_ram + (7<<14);
	}
	else
	{
		logerror("SCREEN 0: BLOCK 5\n");
		spectrum_128_screen_location = mess_ram + (5<<14);
	}

	/* select ram at 0x0c000-0x0ffff */
	{
		int ram_page;
		unsigned char *ram_data;

		ram_page = (spectrum_128_port_7ffd_data & 0x07) | ((scorpion_256_port_1ffd_data & 0x10)>>1);
		ram_data = mess_ram + (ram_page<<14);

		memory_set_bankptr(4, ram_data);

		logerror("RAM at 0xc000: %02x\n",ram_page);
	}

	if (scorpion_256_port_1ffd_data & 0x01)
	{
		/* ram at 0x0000 */
		logerror("RAM at 0x0000\n");

		/* connect page 0 of ram to 0x0000 */
		rh = SMH_BANK1;
		wh = SMH_BANK1;
		memory_set_bankptr(1, mess_ram+(8<<14));
	}
	else
	{
		/* rom at 0x0000 */
		logerror("ROM at 0x0000\n");

		/* connect page 0 of rom to 0x0000 */
		rh = SMH_BANK1;
		wh = SMH_NOP;

		if (scorpion_256_port_1ffd_data & 0x02)
		{
			ROMSelection = 2;
		}
		else
		{
			/* ROM switching */
			ROMSelection = ((spectrum_128_port_7ffd_data>>4) & 0x01) ;
		}
		/* rom 0 is 128K rom, rom 1 is 48 BASIC */
		ChosenROM = memory_region(REGION_CPU1) + 0x010000 + (ROMSelection<<14);

		memory_set_bankptr(1, ChosenROM);

		logerror("rom switch: %02x\n", ROMSelection);
	}
	memory_install_read8_handler(machine, 0, ADDRESS_SPACE_PROGRAM, 0x0000, 0x3fff, 0, 0, rh);
	memory_install_write8_handler(machine, 0, ADDRESS_SPACE_PROGRAM, 0x0000, 0x3fff, 0, 0, wh);
}


static WRITE8_HANDLER(scorpion_port_7ffd_w)
{
	logerror("scorpion 7ffd w: %02x\n", data);

	/* disable paging? */
	if (spectrum_128_port_7ffd_data & 0x20)
		return;

	/* store new state */
	spectrum_128_port_7ffd_data = data;

	/* update memory */
	scorpion_update_memory(machine);
}

static WRITE8_HANDLER(scorpion_port_1ffd_w)
{
	logerror("scorpion 1ffd w: %02x\n", data);

	scorpion_256_port_1ffd_data = data;

	/* disable paging? */
	if ((spectrum_128_port_7ffd_data & 0x20)==0)
	{
		scorpion_update_memory(machine);
	}
}


/* not sure if decoding is full or partial on scorpion */
/* TO BE CHECKED! */
static  READ8_HANDLER(scorpion_port_r)
{

	 /* KT: the following is not decoded exactly, need to check what
     is correct */
	 if ((offset & 2)==0)
	 {
		 switch ((offset>>8) & 0xff)
		 {
				case 0xff: return spectrum_128_port_fffd_r(machine, offset);
				case 0x1f: return spectrum_port_1f_r(machine, offset);
				case 0x7f: return spectrum_port_7f_r(machine, offset);
				case 0xdf: return spectrum_port_df_r(machine, offset);
		 }
	 }
	 switch (offset & 0x0ff)
	 {
	 	case 0x0fe:
	 		return spectrum_port_fe_r(machine, offset);
/*		case 0x01f:
			return wd17xx_status_r(machine, offset);
		case 0x03f:
			return wd17xx_track_r(machine, offset);
		case 0x05f:
			return wd17xx_sector_r(machine, offset);
		case 0x07f:
			return wd17xx_data_r(machine, offset);
		case 0x0ff:
			return betadisk_status;*/
	 }
	 return 0xff;
}


/* not sure if decoding is full or partial on scorpion */
/* TO BE CHECKED! */
static WRITE8_HANDLER(scorpion_port_w)
{
	 switch (offset & 0x0ff)
	 {
		case 0xfe : spectrum_port_fe_w(machine, offset, data);
					break;
		default:
			 if ((offset & 2)==0)
			{
					switch ((offset>>8) & 0xf0)
					{
						case 0x70:
								scorpion_port_7ffd_w(machine, offset, data);
								break;
						case 0xb0:
								spectrum_128_port_bffd_w(machine, offset, data);
								break;
						case 0xf0:
								spectrum_128_port_fffd_w(machine, offset, data);
								break;
						case 0x10:
								scorpion_port_1ffd_w(machine, offset, data);
								break;
						default:
								logerror("Write %02x to scorpion port: %04x\n", data, offset);
					}
			}
			else
			{
				logerror("Write %02x to scorpion port: %04x\n", data, offset);
			}
	}
}



/* ports are not decoded full.
The function decodes the ports appropriately */
static ADDRESS_MAP_START (scorpion_io, ADDRESS_SPACE_IO, 8)
	AM_RANGE(0x0000, 0xffff) AM_READWRITE(scorpion_port_r, scorpion_port_w)
ADDRESS_MAP_END


static MACHINE_RESET( scorpion )
{
	memset(mess_ram,0,256*1024);
	
	/* Bank 5 is always in 0x4000 - 0x7fff */
	memory_set_bankptr(2, mess_ram + (5<<14));

	/* Bank 2 is always in 0x8000 - 0xbfff */
	memory_set_bankptr(3, mess_ram + (2<<14));

	spectrum_128_port_7ffd_data = 0;
	scorpion_256_port_1ffd_data = 0;

	scorpion_update_memory(machine);	
}

static MACHINE_DRIVER_START( scorpion )
	MDRV_IMPORT_FROM( spectrum_128 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_IO_MAP(scorpion_io, 0)
	MDRV_MACHINE_START( scorpion )
	MDRV_MACHINE_RESET( scorpion )
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START(scorpion)
	ROM_REGION(0x020000, REGION_CPU1, 0)
	ROM_LOAD("scorp0.rom",0x010000, 0x4000, CRC(0eb40a09))
	ROM_LOAD("scorp1.rom",0x014000, 0x4000, CRC(9d513013))
	ROM_LOAD("scorp2.rom",0x018000, 0x4000, CRC(fd0d3ce1))
	ROM_LOAD("scorp3.rom",0x01c000, 0x4000, CRC(1fe1d003))
	ROM_CART_LOAD(0, "rom", 0x0000, 0x4000, ROM_NOCLEAR | ROM_NOMIRROR | ROM_OPTIONAL)
ROM_END


SYSTEM_CONFIG_EXTERN(spectrum)

SYSTEM_CONFIG_START(scorpion)
	CONFIG_IMPORT_FROM(spectrum)
	CONFIG_RAM_DEFAULT(256 * 1024)
SYSTEM_CONFIG_END

/*    YEAR  NAME      PARENT    COMPAT  MACHINE     INPUT       INIT    CONFIG      COMPANY     FULLNAME */
COMP( ????, scorpion, 0,		 0,		scorpion,		spectrum,	0,		scorpion,	"Zonov and Co.",		"Zs Scorpion 256", 0 )
