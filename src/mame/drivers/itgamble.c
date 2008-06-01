/* azionale Elettronica + others (mostly Italian) Gambling games.
.
  These all use MCUs with internal ROM for their programs,
   they can't be dumped easily, and thus we can't emulate
   them at the moment because there is nothing to emulate

  This driver is just a placeholder for the graphic / sound
   ROM loading

*/

#include "driver.h"
#include "cpu/h83002/h83002.h"

VIDEO_START( itgamble )
{
}

VIDEO_UPDATE( itgamble )
{
	return 0;
}

static ADDRESS_MAP_START( itgamble_map, ADDRESS_SPACE_PROGRAM, 16 )
	ADDRESS_MAP_GLOBAL_MASK(0xffffff)
	AM_RANGE(0x000000, 0xffffff) AM_ROM
ADDRESS_MAP_END


static INPUT_PORTS_START( itgamble )
    PORT_START
    PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
    PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
    PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END


static const gfx_layout gfxlayout_8x8x4 =
{
    8,8,
    RGN_FRAC(1,1),
    8,
    { 0,1,2,3,4,5,6,7 },
	{ 0,8,16,24,32,40,48,56 },
    { 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64 },
    8*64
};


static GFXDECODE_START( itgamble )
    GFXDECODE_ENTRY( REGION_GFX1, 0, gfxlayout_8x8x4,   0, 16  )
GFXDECODE_END

MACHINE_RESET( itgamble )
{
	// stop the CPU, we have no code for it anyway
	cpunum_set_input_line(machine, 0, INPUT_LINE_HALT, ASSERT_LINE);
}

static MACHINE_DRIVER_START( itgamble )
	MDRV_CPU_ADD_TAG("main",H83044, 30000000) // probably the wrong CPU

	MDRV_CPU_PROGRAM_MAP(itgamble_map,0)

	MDRV_GFXDECODE(itgamble)


	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(512, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 512-1, 0, 256-1)
	MDRV_MACHINE_RESET( itgamble )

	MDRV_PALETTE_LENGTH(0x200)

	MDRV_VIDEO_START( itgamble )
	MDRV_VIDEO_UPDATE( itgamble )
MACHINE_DRIVER_END

/*
CPU

1x HD64F3048F16 (main)(ic1)(not dumped)
1x ispLSI2064-80LJ (ic12)(not dumped)
1x AD-65 (equivalent to M6295) (ic24)(sound)
1x oscillator 30.00MHz (close to main)
1x red resonator ZTB1000J (close to sound)
ROMs

1x M27C2001 (1)
2x M27C4001 (2,3)

Note

1x 28x2 edge connector
1x 12 legs connector
1x 50 legs flat cable connector
1x 12x2 jumper
1x trimmer (volume)
1x trimmer (spark)

*/

ROM_START( laperla )
	ROM_REGION( 0x1000000, REGION_CPU1, 0 )
	ROM_LOAD( "laperla_ver2.0_hd64f3048f16.mcu", 0x00000, 0x4000, NO_DUMP ) // all the program code is in here

	ROM_REGION( 0x100000, REGION_GFX1, 0 )
	ROM_LOAD( "2jolly.ic18", 0x000000, 0x80000, CRC(7bf3d5f2) SHA1(f3a51dd642358a20f6324f28fdf458e8ceaca7a1) )
	ROM_LOAD( "3jolly.ic17", 0x080000, 0x80000, CRC(c3a8d9a0) SHA1(cc95c56ebc6137e11c82ed17be7c9f83ed7b6cfc) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )
	ROM_LOAD( "1.ic25", 0x00000, 0x40000, CRC(4fe79e43) SHA1(7c154cb00e9b64fbdcc218280f2183b816cef20b) ) // M6295 samples
ROM_END

/*

N.revisione	1.2
CPU	1x

HD64F3048F16 (main)(ic1)(not dumped)
1x ispLSI2064-80LJ (ic12)(not dumped)
1x AD-65 (equivalent to M6295) (ic24)(sound)
1x oscillator 30.00MHz (close to main)
1x blu resonator 1000J (close to sound)

ROMs

1x M27C2001 (1)
2x M27C4001 (2,3)

Note

1x 28x2 edge connector
1x 12 legs connector
1x 50 legs flat cable connector
1x 12x2 jumpers
1x trimmer (volume)
1x trimmer (spark)
--------------------
PCB is labeled Ver 1.3, while EPROMs are labeled Ver 1.2

*/

ROM_START( capunc )
	ROM_REGION( 0x1000000, REGION_CPU1, 0 )
	ROM_LOAD( "capunc.ver1.2.mcu", 0x00000, 0x4000, NO_DUMP ) // all the program code is in here

	ROM_REGION( 0x100000, REGION_GFX1, 0 )
	ROM_LOAD( "2.ver.1.2.ic18", 0x000000, 0x80000, CRC(5030f7eb) SHA1(106b61c84e3647b8d68d6c30ee7e63ec2df1f5fd) )
	ROM_LOAD( "3.ver.1.2.ic17", 0x080000, 0x80000, CRC(2b50e312) SHA1(dc901540a5e1a25fe6e7deb58b0fe01f116aaf63) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )
	ROM_LOAD( "1.ic25", 0x00000, 0x40000, CRC(4fe79e43) SHA1(7c154cb00e9b64fbdcc218280f2183b816cef20b) ) // M6295 samples
ROM_END

/*
CPU

1x HD64F3048F16 (main)(ic1)(not dumped)
1x ispLSI2064-80LJ (ic12)(not dumped)
1x AD-65 (equivalent to M6295) (ic24)(sound)
1x oscillator 30MHz (close to main)
1x orange resonator ZTB1000J (close to sound)

ROMs

1x M27C2001 (1)
2x M27C4001 (2,3)

Note

1x 28x2 edge connector
1x 12 legs connector
1x 50 legs flat cable connector
1x 12x2 switches dip
1x trimmer (volume)
1x trimmer (spark)

*/

ROM_START( capcor )
	ROM_REGION( 0x1000000, REGION_CPU1, 0 )
	ROM_LOAD( "capcor.ver1.3.mcu", 0x00000, 0x4000, NO_DUMP ) // all the program code is in here

	ROM_REGION( 0x100000, REGION_GFX1, 0 )
	ROM_LOAD( "2.ic18", 0x000000, 0x80000, CRC(342bea85) SHA1(885080a9b55d64f9a93e3d5e31e6b13f272bdb93) )
	ROM_LOAD( "3.ic17", 0x080000, 0x80000, CRC(ac530eff) SHA1(7c3a6e322311a1cd93801639a0498d5947fb14f2) )

	ROM_REGION( 0x40000, REGION_SOUND1, 0 )
	ROM_LOAD( "1.ic25", 0x00000, 0x40000, CRC(b9b1aff0) SHA1(35622d7d099a10e5c6bcae152fded1f50692f740) ) // M6295 samples
ROM_END



/***************************************************************************

    Game Drivers

***************************************************************************/

GAME( 2000,  capunc,  0, itgamble, itgamble, 0, ROT0, "Nazionale Elettronica", "Capitan Uncino (Ver 1.2)", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 2001,  capcor,  0, itgamble, itgamble, 0, ROT0, "Nazionale Elettronica", "Capitani Coraggiosi (Ver 1.3)", GAME_NO_SOUND | GAME_NOT_WORKING )
GAME( 2002,  laperla, 0, itgamble, itgamble, 0, ROT0, "Nazionale Elettronica", "La Perla Nera (Ver 2.0)", GAME_NO_SOUND | GAME_NOT_WORKING )
