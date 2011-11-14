/******************************************************************************

        avigo.c

        TI "Avigo" PDA


        system driver

        Documentation:
                Hans B Pufal
                Avigo simulator


        MEMORY MAP:
            0x0000-0x03fff: flash 0 block 0
            0x4000-0x07fff: bank 1
            0x8000-0x0bfff: bank 2
            0xc000-0x0ffff: ram block 0

        Bankswitch:
            Bank 1 is controlled by ports 5 and 6, port 5 is the bank select
            and port 6 is the chip select, the known chip select are:
            - 0: flash 0
            - 1: RAM
            - 3: flash 1
            - 5: flash 2 (optional)
            - 6: VideoRAM
            - 7: flash 0

            Bank 2 have the same behavior but is controlled by ports 7 and 8

        Hardware:
            - Z80 CPU
            - 16c500c UART
            - amd29f080 flash-file memory x 3 (3mb)
            - 128k ram
            - stylus pen
            - touch-pad screen

        Flash:
            The following flash ID are checked by Avigo OS, if the returned
            ID is different the PDA doesn't boot.

            Maker ID    Device ID   Description
            0xb0        0x21        Sharp LH28F004
            0x89        0xa6        Sharp LH28F008
            0x89        0xaa        Sharp LH28F016
            0xb0        0x88        Sharp LH28F032
            0x01        0xa4        AMD AM29F040
            0x01        0xd5        AMD AM29F080
            0x01        0xad        AMD AM29F016

        TODO:
            - Found a better way to emulate the touchscreen panel out of the screen
              area (the six buttons at the bottom)
            - Alarm doesn't work
            - Serial communications and IR port.

            I don't have any documentation on the hardware, so a lot of this
            driver has been written using educated guesswork and a lot of help
            from an existing emulation written by Hans Pufal. Hans's emulator
            is also written from educated guesswork.

        MESS Driver by Kevin Thacker and Sandro Ronco

 ******************************************************************************/

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "includes/avigo.h"
#include "avigo.lh"


#define AVIGO_LOG 0
#define LOG(x) do { if (AVIGO_LOG) logerror x; } while (0)


/* memory 0x0000-0x03fff */
READ8_MEMBER(avigo_state::flash_0x0000_read_handler)
{
	return m_flashes[0]->read(offset);
}

/* memory 0x0000-0x03fff */
WRITE8_MEMBER(avigo_state::flash_0x0000_write_handler)
{
	m_flashes[0]->write(offset, data);
}

/* memory 0x04000-0x07fff */
READ8_MEMBER(avigo_state::flash_0x4000_read_handler)
{
	return m_flashes[m_flash_at_0x4000]->read((m_bank1_l<<14) | offset);
}

/* memory 0x04000-0x07fff */
WRITE8_MEMBER(avigo_state::flash_0x4000_write_handler)
{
	m_flashes[m_flash_at_0x4000]->write((m_bank1_l<<14) | offset, data);
}

/* memory 0x08000-0x0bfff */
READ8_MEMBER(avigo_state::flash_0x8000_read_handler)
{
	return m_flashes[m_flash_at_0x8000]->read((m_bank2_l<<14) | offset);
}

/* memory 0x08000-0x0bfff */
WRITE8_MEMBER(avigo_state::flash_0x8000_write_handler)
{
	m_flashes[m_flash_at_0x8000]->write((m_bank2_l<<14) | offset, data);
}


/*
    IRQ bits (port 3) ordered by priority:

    bit 7: power down request       high priority. When it occurs, clear this bit.
    bit 5: real time clock
    bit 3: uart int
    bit 6: pen int                  An interrupt when pen is pressed against screen.
    bit 4: 1 sec int                used for auto power off
    bit 2: synchronisation link interrupt???keyboard int            ;; check bit 5 of port 1,
    bit 1: ???      (cleared in nmi, and then set again)
    bit 0: not checked, probably unused
*/

void avigo_state::refresh_ints()
{
	if (m_irq!=0)
		device_set_input_line(m_maincpu, 0, HOLD_LINE);
	else
		device_set_input_line(m_maincpu, 0, CLEAR_LINE);
}

/* does not do anything yet */
WRITE_LINE_MEMBER( avigo_state::tc8521_alarm_int )
{
//#if 0
	m_irq &=~(1<<5);

	if (state)
	{
		m_irq |= (1<<5);
	}

	refresh_ints();
//#endif
}


static RP5C01_INTERFACE( rtc_intf )
{
	DEVCB_DRIVER_LINE_MEMBER(avigo_state, tc8521_alarm_int)
};


void avigo_state::refresh_memory(UINT8 bank, UINT8 chip_select)
{
	address_space* space = m_maincpu->memory().space(AS_PROGRAM);
	int &active_flash = (bank == 1 ? m_flash_at_0x4000 : m_flash_at_0x8000);
	char bank_tag[6];

	LOG(("Chip %02x mapped at %04x - %04x\n", chip_select, bank * 0x4000, bank * 0x4000 + 0x3fff));

	switch (chip_select)
	{
		case 0x06:	// videoram
			space->install_readwrite_handler(bank * 0x4000, bank * 0x4000 + 0x3fff, read8_delegate(FUNC(avigo_state::vid_memory_r), this), write8_delegate(FUNC(avigo_state::vid_memory_w), this));
			active_flash = -1;
			break;

		case 0x01: // banked RAM
			sprintf(bank_tag,"bank%d", bank);
			memory_set_bankptr(machine(), bank_tag, m_ram_base + (((bank == 1 ? m_bank1_l : m_bank2_l) & 0x07)<<14));
			space->install_readwrite_bank (bank * 0x4000, bank * 0x4000 + 0x3fff, bank_tag);
			active_flash = -1;
			break;

		case 0x00:	// flash 0
		case 0x03:	// flash 1
		case 0x05:	// flash 2
		case 0x07:	// flash 0
			if (active_flash < 0)	// to avoid useless calls to install_readwrite_handler that cause slowdowns
			{
				if (bank == 1)
					space->install_readwrite_handler(0x4000, 0x7fff, read8_delegate(FUNC(avigo_state::flash_0x4000_read_handler), this), write8_delegate(FUNC(avigo_state::flash_0x4000_write_handler), this));
				else
					space->install_readwrite_handler(0x8000, 0xbfff, read8_delegate(FUNC(avigo_state::flash_0x8000_read_handler), this), write8_delegate(FUNC(avigo_state::flash_0x8000_write_handler), this));
			}

			switch (chip_select)
			{
				case 0x00:
				case 0x07: active_flash = 0; break;
				case 0x03: active_flash = 1; break;
				case 0x05: active_flash = 2; break;
			}
			break;

		default:
			logerror("Unknown chip %02x mapped at %04x - %04x\n", chip_select, bank * 0x4000, bank * 0x4000 + 0x3fff);
			space->unmap_readwrite(bank * 0x4000, bank * 0x4000 + 0x3fff);
			active_flash = -1;
			break;
	}
}


WRITE_LINE_MEMBER( avigo_state::com_interrupt )
{
	LOG(("com int\r\n"));

	m_irq &= ~(1<<3);

	if (state)
	{
		m_irq |= (1<<3);
	}

	refresh_ints();
}



static const ins8250_interface avigo_com_interface =
{
	1843200,
	DEVCB_DRIVER_LINE_MEMBER(avigo_state, com_interrupt),
	NULL,
	NULL,
	NULL
};


void avigo_state::machine_reset()
{
	/* if is a cold start initialize flash contents */
	if (!m_warm_start)
	{
		memcpy(m_flashes[0]->space()->get_read_ptr(0), machine().region("bios")->base() + 0x000000, 0x100000);
		memcpy(m_flashes[1]->space()->get_read_ptr(0), machine().region("bios")->base() + 0x100000, 0x100000);
	}

	m_irq = 0;
	m_bank1_l = 0;
	m_bank1_h = 0;
	m_bank2_l = 0;
	m_bank2_h = 0;
	m_flash_at_0x4000 = -1;
	m_flash_at_0x8000 = -1;

	refresh_memory(1, m_bank1_h & 0x07);
	refresh_memory(2, m_bank2_h & 0x07);
}

void avigo_state::machine_start()
{
	m_ram_base = (UINT8*)ram_get_ptr(m_ram);

	// bank3 always first ram bank
	memory_set_bankptr(machine(), "bank3", m_ram_base);

	/* keep machine pointers to flash devices */
	m_flashes[0] = machine().device<intelfsh8_device>("flash0");
	m_flashes[1] = machine().device<intelfsh8_device>("flash1");
	m_flashes[2] = machine().device<intelfsh8_device>("flash2");
}

static ADDRESS_MAP_START( avigo_mem , AS_PROGRAM, 8, avigo_state)
	AM_RANGE(0x0000, 0x3fff) AM_READWRITE(flash_0x0000_read_handler, flash_0x0000_write_handler)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE_BANK("bank1")
	AM_RANGE(0x8000, 0xbfff) AM_READWRITE_BANK("bank2")
	AM_RANGE(0xc000, 0xffff) AM_READWRITE_BANK("bank3")
ADDRESS_MAP_END


READ8_MEMBER(avigo_state::key_data_read_r)
{
	UINT8 data = 0x0f;

	if (!(m_key_line & 0x01))
	{
		data &= input_port_read(machine(), "LINE0");
	}

	if (!(m_key_line & 0x02))
	{
		data &= input_port_read(machine(), "LINE1");
	}

	if (!(m_key_line & 0x04))
	{
		data &= input_port_read(machine(), "LINE2");
	}

	/* bit 3 is cold/warm start */
	data &= ((m_warm_start<<3) ^ 0xff);

	/* if bit 5 is clear shows synchronisation logo! */
	data |= (1<<5);

	return data;
}


/* set key line(s) to read */
/* bit 0 set for line 0, bit 1 set for line 1, bit 2 set for line 2 */
WRITE8_MEMBER(avigo_state::set_key_line_w)
{
	/* 5, 101, read back 3 */
	m_key_line = data;

	m_warm_start = BIT(data, 3);
}

READ8_MEMBER(avigo_state::irq_r)
{
	return m_irq;
}

WRITE8_MEMBER(avigo_state::irq_w)
{
	m_irq &= data;

	refresh_ints();
}

WRITE8_MEMBER(avigo_state::port2_w)
{
	/*
        bit 4     LCD backlight on/off
        bit 5-6   source select for a/d converter
    */

	if ((m_port2 ^ data) & 0x10)
		popmessage("Backlight %s", data & 0x10 ? "on" : "off");

	m_port2 = data;
}

READ8_MEMBER(avigo_state::bank1_r)
{
	return offset ? m_bank1_h: m_bank1_l;
}

READ8_MEMBER(avigo_state::bank2_r)
{
	return offset ? m_bank2_h: m_bank2_l;
}

WRITE8_MEMBER(avigo_state::bank1_w)
{
	if (offset)
	{
		LOG(("bank1 h w: %04x\n", data));
		m_bank1_h = data;
	}
	else
	{
		LOG(("bank1 l w: %04x\n", data));
		m_bank1_l = data & 0x3f;
	}

	refresh_memory(1, m_bank1_h & 0x07);
}

WRITE8_MEMBER(avigo_state::bank2_w)
{
	if (offset)
	{
		LOG(("bank2 h w: %04x\n", data));
		m_bank2_h = data;
	}
	else
	{
		LOG(("bank2 l w: %04x\n", data));
		m_bank2_l = data & 0x3f;
	}

	refresh_memory(2, m_bank2_h & 0x07);
}

READ8_MEMBER(avigo_state::ad_control_status_r)
{
	LOG(("avigo ad control read %02x\n", (int) m_ad_control_status));
	return m_ad_control_status;
}


WRITE8_MEMBER(avigo_state::ad_control_status_w)
{
	LOG(("avigo ad control w %02x\n",data));

	switch (m_port2 & 0x60)
	{
		case 0x20:
			// read main battery status valid range 0x000-0x3ff
			LOG(("a/d main battery status\n"));

			m_ad_value = 0x3ff;
			break;
		case 0x40:
			// read backup battery status valid range 0x000-0x3ff
			LOG(("a/d backup battery status\n"));

			m_ad_value = 0x3ff;
			break;
		case 0x60:
			if ((data & 0x070)==0x070)
			{
				/* bit 3 appears to select between 1 = x coord, 0 = y coord */
				/* when 6,5,4 = 1 */
				if ((data & 0x08)!=0)
				{
					LOG(("a/d select x coordinate\n"));
					LOG(("x coord: %d\n", input_port_read(machine(), "POSX")));

					/* on screen range 0x060->0x03a0 */
					if (input_port_read(machine(), "LINE3") & 0x01)
					{
						/* this might not be totally accurate because hitable screen
                        area may include the border around the screen! */
						m_ad_value = input_port_read(machine(), "POSX");
					}
					else
					{
						m_ad_value = 0;
					}

					LOG(("ad value: %d\n",m_ad_value));

				}
				else
				{
					/* in the avigo rom, the y coordinate is inverted! */
					/* therefore a low value would be near the bottom of the display,
                    and a high value at the top */

					/* total valid range 0x044->0x03a6 */
					/* 0x0350 is also checked */

					/* assumption 0x044->0x0350 is screen area and
                    0x0350->0x03a6 is panel at bottom */

					LOG(("a/d select y coordinate\n"));
					LOG(("y coord: %d\n", input_port_read(machine(), "POSY")));

					if (input_port_read(machine(), "LINE3") & 0x01)
					{
						m_ad_value = input_port_read(machine(), "POSY");
					}
					else
					{
						m_ad_value = 0;
					}

					LOG(("ad value: %d\n",m_ad_value));
				}
			}
	}

	/* bit 0: 1 if a/d complete, 0 if a/d not complete */
	m_ad_control_status = data | 1;
}

READ8_MEMBER(avigo_state::ad_data_r)
{
	UINT8 data = 0;

	/* original */

	/* status AND   11110111 */
	/* status OR    01110000 -> C20F */

	switch (m_ad_control_status & 0x078)
	{
		/* x1110xxx */
		/* read upper 4 bits of 10 bit A/D number */
		case 0x060:
		case 0x070:
		case 0x078:
		{
			/* upper 4 bits of 10 bit A/D number in bits 7-4 of data */
			/* bit 0 must be 0, bit 1 must be 0 */
			/* bit 3 must be 1. bit 2 can have any value */

			LOG(("a/d read upper 4 bits\n"));
			data = ((m_ad_value>>6) & 0x0f)<<4;
			data |= 8;
		}
		break;

		/* x0111xxx */
		case 0x020:
		case 0x038:
		{
			/* lower 6 bits of 10-bit A/D number in bits 7-2 of data */
			/* bit 0 must be 1, bit 1 must be 0 */

			LOG(("a/d lower 6-bits\n"));
			data = ((m_ad_value & 0x03f)<<2);
			data |= 1;
		}
		break;

		default:
			break;
	}

	/* x coord? */
	/* wait for bit 0 of status to become 1 */
	/* read data -> d */


	/* C20f AND 10111111 */
	/* C20f OR  00001000 */
	/* x0111xxx */

	/* bit 1 must be 0, bit 0 must be 1 */
	/* read data -> e */

	/* upper 4 bits of d contain data */
	/* bits 0 and 1 do not contain data of e, but all other bits do */

	/* get bit 5 and 6 of d */
	/* and put into bit 0 and 1 of e */

	/* C20f OR  01000000 */
	/* x1111xxx */

	/* y coord? */
	/* bit 0 must be 0, bit 1 must be 0 */
	/* bit 3 must be 1. bit 2 can have any value */
	/* read data -> d */

	/* C20f AND  10111111 */
	/* x0111xxx */

	/* bit 1 must be 0, bit 0 must be 1 */
	/* read data -> e */


	/* original and 1111100 */
	/* original or  1111000 */
	/* 1111x00 */



	/* if fails! */
	/* original */
	/* AND 1001100 */
	/* OR  1001000 */
	/* 1001x00 */


	/* AND 1101100 */
	/* OR  1101000 */
	/* 1101x00 */

	/* 1111x00 */

	LOG(("avigo ad read %02x\n",data));

	return data;
}


WRITE8_MEMBER(avigo_state::speaker_w)
{
	/* Speaker output state */
	speaker_level_w(m_speaker, BIT(data, 3));
}


/* port 0x04:

  bit 7: ??? if set, does a write 0x00 to 0x02e */

  /* port 0x029:
    port 0x02e */
READ8_MEMBER(avigo_state::port_04_r)
{
	/* must be both 0 for it to boot! */
	return 0x0ff^((1<<7) | (1<<5));
}



static ADDRESS_MAP_START( avigo_io, AS_IO, 8, avigo_state)
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x001, 0x001) AM_READWRITE( key_data_read_r, set_key_line_w )
	AM_RANGE(0x002, 0x002) AM_WRITE( port2_w )
	AM_RANGE(0x003, 0x003) AM_READWRITE( irq_r, irq_w )
	AM_RANGE(0x004, 0x004) AM_READ( port_04_r )
	AM_RANGE(0x005, 0x006) AM_READWRITE( bank1_r, bank1_w )
	AM_RANGE(0x007, 0x008) AM_READWRITE( bank2_r, bank2_w )
	AM_RANGE(0x009, 0x009) AM_READWRITE( ad_control_status_r, ad_control_status_w )
	AM_RANGE(0x010, 0x01f) AM_DEVREADWRITE("rtc", rp5c01_device, read, write)
	AM_RANGE(0x028, 0x028) AM_WRITE( speaker_w )
	AM_RANGE(0x02d, 0x02d) AM_READ( ad_data_r )
	AM_RANGE(0x030, 0x037) AM_DEVREADWRITE_LEGACY("ns16550", ins8250_r, ins8250_w )
ADDRESS_MAP_END


INPUT_CHANGED_MEMBER( avigo_state::pen_irq )
{
	LOG(("pen pressed interrupt\n"));

	// an irq is generated when the pen is pressed down on the screen
	// or lifted up from the screen
	m_irq |= (1<<6);

	refresh_ints();
}

INPUT_CHANGED_MEMBER( avigo_state::pen_move_irq )
{
	// an irq is generated when the pen is down on the screen and is being moved
	if (input_port_read(machine(), "LINE3") & 0x01)
	{
		LOG(("pen move interrupt\n"));
		m_irq |= (1<<6);

		refresh_ints();
	}
}

INPUT_CHANGED_MEMBER( avigo_state::kb_irq )
{
	LOG(("key pressed interrupt\n"));

	if (!newval)
	{
		m_irq |= (1<<2);

		refresh_ints();
	}
}

INPUT_CHANGED_MEMBER( avigo_state::power_down_irq )
{
	if(newval)
	{
		m_irq |= (1<<7);

		refresh_ints();
	}
}

static INPUT_PORTS_START(avigo)
	PORT_START("LINE0")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("PAGE UP")		PORT_CODE(KEYCODE_PGUP)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("PAGE DOWN")	PORT_CODE(KEYCODE_PGDN)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("LIGHT")		PORT_CODE(KEYCODE_L)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0xf8, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("LINE1")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("TO DO")		PORT_CODE(KEYCODE_T)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("ADDRESS")		PORT_CODE(KEYCODE_A)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("SCHEDULE")		PORT_CODE(KEYCODE_S)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0xf8, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("LINE2")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("MEMO") 		PORT_CODE(KEYCODE_M)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, kb_irq, NULL )
	PORT_BIT(0xfe, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START("LINE3")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Pen/Stylus pressed") PORT_CODE(KEYCODE_ENTER) PORT_CODE(MOUSECODE_BUTTON1)  PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, pen_irq, NULL )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("?? Causes a NMI") PORT_CODE(KEYCODE_W) PORT_CODE(JOYCODE_BUTTON2)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Power Down")		 PORT_CODE(KEYCODE_Q) PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, power_down_irq, NULL )

	/* these two ports are used to emulate the position of the pen/stylus on the screen */
	PORT_START("POSX") /* Mouse - X AXIS */
	PORT_BIT(0x3ff, 0x060, IPT_LIGHTGUN_X) PORT_SENSITIVITY(100) PORT_CROSSHAIR(X, 1, 0, 0) PORT_MINMAX(0x060, 0x3a0) PORT_KEYDELTA(10) PORT_PLAYER(1)				PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, pen_move_irq, NULL )

	PORT_START("POSY") /* Mouse - Y AXIS */
	PORT_BIT(0x3ff, 0x044, IPT_LIGHTGUN_Y) PORT_SENSITIVITY(100) PORT_CROSSHAIR(Y, 1, 0, 0) PORT_MINMAX(0x044, 0x3a6) PORT_INVERT PORT_KEYDELTA(10) PORT_PLAYER(1)	PORT_CHANGED_MEMBER( DEVICE_SELF, avigo_state, pen_move_irq, NULL )
INPUT_PORTS_END

/* F4 Character Displayer */
static const gfx_layout avigo_charlayout =
{
	8, 16,					/* 8 x 16 characters */
	96,					/* 96 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*16					/* every char takes 16 bytes */
};

static const gfx_layout avigo_8_by_14 =
{
	8, 14,					/* 8 x 16 characters */
	1024,					/* 1024 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*32					/* every char takes 32 bytes */
};

static const gfx_layout avigo_16_by_15 =
{
	16, 15,					/* 8 x 16 characters */
	1024,					/* 1024 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	/* y offsets */
	{ 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	16*16					/* every char takes 16 bytes */
};

static const gfx_layout avigo_15_by_16 =
{
	15, 16,					/* 8 x 16 characters */
	1024,					/* 1024 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16, 8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	/* y offsets */
	{ 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	16*16					/* every char takes 16 bytes */
};

static const gfx_layout avigo_8_by_8 =
{
	8, 8,					/* 8 x 8 characters */
	256,					/* 256 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8 },
	16*16					/* every char takes 16 bytes */
};

static const gfx_layout avigo_6_by_8 =
{
	6, 8,					/* 6 x 8 characters */
	255,					/* 255 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	/* y offsets */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	16*16					/* every char takes 16 bytes */
};

static GFXDECODE_START( avigo )
	GFXDECODE_ENTRY( "bios", 0x08992, avigo_charlayout, 0, 1 )
	GFXDECODE_ENTRY( "bios", 0x0c020, avigo_8_by_14, 0, 1 )
	GFXDECODE_ENTRY( "bios", 0x0c020, avigo_16_by_15, 0, 1 )
	GFXDECODE_ENTRY( "bios", 0x14020, avigo_15_by_16, 0, 1 )
	GFXDECODE_ENTRY( "bios", 0x1c020, avigo_8_by_8, 0, 1 )
	GFXDECODE_ENTRY( "bios", 0x1e020, avigo_6_by_8, 0, 1 )
GFXDECODE_END


static TIMER_DEVICE_CALLBACK( avigo_scan_timer )
{
	avigo_state *state = timer.machine().driver_data<avigo_state>();

	state->m_irq |= (1<<1);

	state->refresh_ints();
}

static TIMER_DEVICE_CALLBACK( avigo_1hz_timer )
{
	avigo_state *state = timer.machine().driver_data<avigo_state>();

	state->m_irq |= (1<<4);

	state->refresh_ints();
}

static NVRAM_HANDLER(avigo)
{
	avigo_state *state = machine.driver_data<avigo_state>();
	UINT32 ram_size = ram_get_size(state->m_ram);

	if (read_or_write)
	{
		file->write(state->m_ram_base, ram_size);
	}
	else
	{
		if (file)
		{
			file->read(state->m_ram_base, ram_size);
			state->m_warm_start = 1;
		}
		else
		{
			memset(state->m_ram_base, 0, ram_size);
			state->m_warm_start = 0;
		}
	}
}


static QUICKLOAD_LOAD(avigo)
{
	avigo_state *state = image.device().machine().driver_data<avigo_state>();
	address_space* flash1 = state->m_flashes[1]->memory().space(0);
	const char *systemname = image.device().machine().system().name;
	UINT32 first_app_page = (0x50000>>14);
	int app_page;

	// german and spanish language are 4 pages bigger than other
	if ( strcmp( systemname, "avigo_de" ) == 0 || strcmp( systemname, "avigo_es" ) == 0 )
		first_app_page += 4;

	// search the first empty page
	for (app_page = first_app_page + 1; app_page<0x40; app_page++)
	{
		bool empty_page = true;

		for (int offset=0; offset<0x4000; offset++)
		{
			if (flash1->read_byte((app_page<<14) + offset) != 0xff)
			{
				empty_page = false;
				break;
			}
		}

		if (empty_page)
			break;
	}

	// if there is the required free space installs the application
	if ((app_page + (image.length()>>14)) < 0x40)
	{
		logerror("Application loaded at 0x%05x-0x%05x\n", app_page<<14, (app_page<<14) + (UINT32)image.length());

		// copy app file into flash memory
		image.fread((UINT8*)state->m_flashes[1]->space()->get_read_ptr(app_page<<14), image.length());

		// update the application ID
		flash1->write_byte((app_page<<14) + 0x1a5, 0x80 + (app_page - (first_app_page>>14)));

		// reset the CPU for allow at the Avigo OS to recognize the installed app
		state->m_warm_start = 1;
		state->m_maincpu->reset();

		return IMAGE_INIT_PASS;
	}

	return IMAGE_INIT_FAIL;
}


static MACHINE_CONFIG_START( avigo, avigo_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, 4000000)
	MCFG_CPU_PROGRAM_MAP(avigo_mem)
	MCFG_CPU_IO_MAP(avigo_io)
	MCFG_QUANTUM_TIME(attotime::from_hz(60))

	MCFG_NS16550_ADD( "ns16550", avigo_com_interface )

	/* video hardware */
	MCFG_SCREEN_ADD("screen", LCD)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MCFG_SCREEN_SIZE(AVIGO_SCREEN_WIDTH, AVIGO_SCREEN_HEIGHT + AVIGO_PANEL_HEIGHT)
	MCFG_SCREEN_VISIBLE_AREA(0, AVIGO_SCREEN_WIDTH-1, 0, AVIGO_SCREEN_HEIGHT + AVIGO_PANEL_HEIGHT -1)
	MCFG_DEFAULT_LAYOUT(layout_avigo)

	MCFG_GFXDECODE(avigo)
	MCFG_PALETTE_LENGTH(AVIGO_NUM_COLOURS)
	MCFG_PALETTE_INIT( avigo )

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD(SPEAKER_TAG, SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	/* real time clock */
	MCFG_RP5C01_ADD("rtc", XTAL_32_768kHz, rtc_intf)

	/* flash ROMs */
	MCFG_AMD_29F080_ADD("flash0")
	MCFG_AMD_29F080_ADD("flash1")
	MCFG_AMD_29F080_ADD("flash2")

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("128K")

	MCFG_NVRAM_HANDLER(avigo)

	// IRQ 1 is used for scan the pen and for cursor blinking
	MCFG_TIMER_ADD_PERIODIC("scan_timer", avigo_scan_timer, attotime::from_hz(50))

	// IRQ 4 is generated every second, used for auto power off
	MCFG_TIMER_ADD_PERIODIC("1hz_timer", avigo_1hz_timer, attotime::from_hz(1))

	/* quickload */
	MCFG_QUICKLOAD_ADD("quickload", avigo, "app", 0)
MACHINE_CONFIG_END


/***************************************************************************

  Game driver(s)

***************************************************************************/
ROM_START(avigo)
	ROM_REGION(0x200000, "bios", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS( 0, "v1004", "v1.004" )
	ROMX_LOAD("os_1004.rom", 0x000000, 0x0100000, CRC(62acd55c) SHA1(b2be12f5cc1053b6026bff2a265146ba831a7ffa), ROM_BIOS(1))
	ROMX_LOAD("english_1004.rom", 0x100000, 0x050000, CRC(c9c3a225) SHA1(7939993a5615ca59ff2047e69b6d85122d437dca), ROM_BIOS(1))

	ROM_SYSTEM_BIOS( 1, "v1002", "v1.002" )
	ROMX_LOAD("os_1002.rom", 0x000000, 0x0100000, CRC(484bb95c) SHA1(ddc28f22f8cbc99f60f91c58ee0e2d15170024fb), ROM_BIOS(2))
	ROMX_LOAD("english_1002.rom", 0x100000, 0x050000, CRC(31cab0ac) SHA1(87d337830506a12514a4beb9a8502a0de94816f2), ROM_BIOS(2))

	ROM_SYSTEM_BIOS( 2, "v100", "v1.00" )
	ROMX_LOAD("os_100.rom", 0x000000, 0x0100000, CRC(13ea7b38) SHA1(85566ff142d86d504ac72613f169d8758e2daa09), ROM_BIOS(3))
	ROMX_LOAD("english_100.rom", 0x100000, 0x050000, CRC(e2824b44) SHA1(3252454b05c3d3a4d7df1cb48dc3441ae82f2b1c), ROM_BIOS(3))

ROM_END

ROM_START(avigo_de)
	ROM_REGION(0x200000, "bios", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS( 0, "v1004", "v1.004" )
	ROMX_LOAD("os_1004.rom", 0x000000, 0x0100000, CRC(62acd55c) SHA1(b2be12f5cc1053b6026bff2a265146ba831a7ffa), ROM_BIOS(1))
	ROMX_LOAD("german_1004.rom", 0x100000, 0x060000, CRC(0fa437b3) SHA1(e9352aa8fee6d93b898412bd129452b82baa9a21), ROM_BIOS(1))

	ROM_SYSTEM_BIOS( 1, "v1002", "v1.002" )
	ROMX_LOAD("os_1002.rom", 0x000000, 0x0100000, CRC(484bb95c) SHA1(ddc28f22f8cbc99f60f91c58ee0e2d15170024fb), ROM_BIOS(2))
	ROMX_LOAD("german_1002.rom", 0x100000, 0x060000, CRC(c6bf07ba) SHA1(d3185687aa510f6c3b3ab3baaabe7e8ce1a79e3b), ROM_BIOS(2))

	ROM_SYSTEM_BIOS( 2, "v100", "v1.00" )
	ROMX_LOAD("os_100.rom", 0x000000, 0x0100000, CRC(13ea7b38) SHA1(85566ff142d86d504ac72613f169d8758e2daa09), ROM_BIOS(3))
	ROMX_LOAD("german_100.rom", 0x100000, 0x060000, CRC(117d9189) SHA1(7e959ab1381ba831821fcf87973b25d87f12d34e), ROM_BIOS(3))

ROM_END

ROM_START(avigo_fr)
	ROM_REGION(0x200000, "bios", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS( 0, "v1004", "v1.004" )
	ROMX_LOAD("os_1004.rom", 0x000000, 0x0100000, CRC(62acd55c) SHA1(b2be12f5cc1053b6026bff2a265146ba831a7ffa), ROM_BIOS(1))
	ROMX_LOAD("french_1004.rom", 0x100000, 0x050000, CRC(5e4d90f7) SHA1(07df3af8a431ba65e079d6c987fb5d544f6541d8), ROM_BIOS(1))

	ROM_SYSTEM_BIOS( 1, "v1002", "v1.002" )
	ROMX_LOAD("os_1002.rom", 0x000000, 0x0100000, CRC(484bb95c) SHA1(ddc28f22f8cbc99f60f91c58ee0e2d15170024fb), ROM_BIOS(2))
	ROMX_LOAD("french_1002.rom", 0x100000, 0x050000,CRC(caa3eb91) SHA1(ab199986de301d933f069a5e1f5150967e1d7f59), ROM_BIOS(2))

	ROM_SYSTEM_BIOS( 2, "v100", "v1.00" )
	ROMX_LOAD("os_100.rom", 0x000000, 0x0100000, CRC(13ea7b38) SHA1(85566ff142d86d504ac72613f169d8758e2daa09), ROM_BIOS(3))
	ROMX_LOAD("french_100.rom", 0x100000, 0x050000, CRC(fffa2345) SHA1(399447cede3cdd0be768952cb24f7e4431147e3d), ROM_BIOS(3))

ROM_END

ROM_START(avigo_es)
	ROM_REGION(0x200000, "bios", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS( 0, "v1004", "v1.004" )
	ROMX_LOAD("os_1004.rom", 0x000000, 0x0100000, CRC(62acd55c) SHA1(b2be12f5cc1053b6026bff2a265146ba831a7ffa), ROM_BIOS(1))
	ROMX_LOAD("spanish_1004.rom", 0x100000, 0x060000, CRC(235a7f8d) SHA1(94da4ecafb54dcd5d80bc5063cb4024e66e6a21f), ROM_BIOS(1))

	ROM_SYSTEM_BIOS( 1, "v1002", "v1.002" )
	ROMX_LOAD("os_1002.rom", 0x000000, 0x0100000, CRC(484bb95c) SHA1(ddc28f22f8cbc99f60f91c58ee0e2d15170024fb), ROM_BIOS(2))
	ROMX_LOAD("spanish_1002.rom", 0x100000, 0x060000, CRC(a6e80cc4) SHA1(e741657558c11f7bce646ba3d7b5f845bfa275b7), ROM_BIOS(2))

	ROM_SYSTEM_BIOS( 2, "v100", "v1.00" )
	ROMX_LOAD("os_100.rom", 0x000000, 0x0100000, CRC(13ea7b38) SHA1(85566ff142d86d504ac72613f169d8758e2daa09), ROM_BIOS(3))
	ROMX_LOAD("spanish_100.rom", 0x100000, 0x060000, CRC(953a5276) SHA1(b9ba1dbdc2127b1ef419c911ef66313024a7351a), ROM_BIOS(3))

ROM_END

ROM_START(avigo_it)
	ROM_REGION(0x200000, "bios", ROMREGION_ERASEFF)

	ROM_SYSTEM_BIOS( 0, "v1004", "v1.004" )
	ROMX_LOAD("os_1004.rom", 0x000000, 0x0100000, CRC(62acd55c) SHA1(b2be12f5cc1053b6026bff2a265146ba831a7ffa), ROM_BIOS(1))
	ROMX_LOAD("italian_1004.rom", 0x100000, 0x050000, CRC(fb7941ec) SHA1(230e8346a3b0da1ee24568ec090ce6860ebfe995), ROM_BIOS(1))

	ROM_SYSTEM_BIOS( 1, "v1002", "v1.002" )
	ROMX_LOAD("os_1002.rom", 0x000000, 0x0100000, CRC(484bb95c) SHA1(ddc28f22f8cbc99f60f91c58ee0e2d15170024fb), ROM_BIOS(2))
	ROMX_LOAD("italian_1002.rom", 0x100000, 0x050000, CRC(093bc032) SHA1(2c75d950d356a7fd1d058808e5f0be8e15b8ea2a), ROM_BIOS(2))

	ROM_SYSTEM_BIOS( 2, "v100", "v1.00" )
	ROMX_LOAD("os_100.rom", 0x000000, 0x0100000, CRC(13ea7b38) SHA1(85566ff142d86d504ac72613f169d8758e2daa09), ROM_BIOS(3))
	ROMX_LOAD("italian_100.rom", 0x100000, 0x050000, CRC(de359218) SHA1(6185727aba8ffc98723f2df74dda388fd0d70cc9), ROM_BIOS(3))
ROM_END

/*    YEAR  NAME    PARENT  COMPAT  MACHINE INPUT   INIT    COMPANY   FULLNAME */
COMP(1997,	avigo,  	0,  		0,		avigo,	avigo,	0,		"Texas Instruments", "TI Avigo 10 PDA",         	0)
COMP(1997,	avigo_de,	avigo,		0,		avigo,	avigo,	0,		"Texas Instruments", "TI Avigo 10 PDA (German)",	0)
COMP(1997,	avigo_fr,	avigo,		0,		avigo,	avigo,	0,		"Texas Instruments", "TI Avigo 10 PDA (French)",	0)
COMP(1997,	avigo_es,	avigo,		0,		avigo,	avigo,	0,		"Texas Instruments", "TI Avigo 10 PDA (Spanish)",	0)
COMP(1997,	avigo_it,	avigo,		0,		avigo,	avigo,	0,		"Texas Instruments", "TI Avigo 10 PDA (Italian)",	0)

