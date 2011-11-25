/***************************************************************************

    Tandy 2000

    Skeleton driver.

****************************************************************************/

/*

    TODO:

    - CRT9007
    - CRT9212 Double Row Buffer
    - CRT9021B Attribute Generator
    - 80186
    - keyboard ROM
    - hires graphics board
    - floppy 720K DSQD
    - DMA
    - WD1010
    - hard disk
    - mouse

*/

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "cpu/i86/i86.h"
#include "cpu/mcs48/mcs48.h"
#include "imagedev/flopdrv.h"
#include "machine/ram.h"
#include "machine/ctronics.h"
#include "machine/i8255.h"
#include "machine/i8251.h"
#include "machine/pit8253.h"
#include "machine/pic8259.h"
#include "machine/upd765.h"
#include "sound/speaker.h"
#include "video/crt9007.h"
#include "video/crt9021.h"
#include "video/crt9212.h"
#include "includes/tandy2k.h"

/* Read/Write Handlers */

void tandy2k_state::dma_request(int line, int state)
{

}

void tandy2k_state::speaker_update()
{
	int level = !(m_spkrdata & m_outspkr);

	speaker_level_w(m_speaker, level);
}

READ8_MEMBER( tandy2k_state::videoram_r )
{
	address_space *program = m_maincpu->memory().space(AS_PROGRAM);

	offs_t addr = (m_vram_base << 15) | (offset << 1);
	UINT16 data = program->read_word(addr);

	// character
	m_drb0->write(space, 0, data & 0xff);

	// attributes
	m_drb1->write(space, 0, data >> 8);

	return data & 0xff;
}

READ8_MEMBER( tandy2k_state::enable_r )
{
	/*

        bit     signal      description

        0                   RS-232 ring indicator
        1                   RS-232 carrier detect
        2
        3
        4
        5
        6
        7       _ACLOW

    */

	return 0x80;
}

WRITE8_MEMBER( tandy2k_state::enable_w )
{
	/*

        bit     signal      description

        0       KBEN        keyboard enable
        1       EXTCLK      external baud rate clock
        2       SPKRGATE    enable periodic m_speaker output
        3       SPKRDATA    direct output to m_speaker
        4       RFRQGATE    enable refresh and baud rate clocks
        5       _FDCRESET   reset 8272
        6       TMRIN0      enable 80186 timer 0
        7       TMRIN1      enable 80186 timer 1

    */

	/* keyboard enable */
	m_kben = BIT(data, 0);

	/* external baud rate clock */
	m_extclk = BIT(data, 1);

	/* m_speaker gate */
	pit8253_gate0_w(m_pit, BIT(data, 2));

	/* m_speaker data */
	m_spkrdata = BIT(data, 3);
	speaker_update();

	/* refresh and baud rate clocks */
	pit8253_gate1_w(m_pit, BIT(data, 4));
	pit8253_gate2_w(m_pit, BIT(data, 4));

	/* FDC reset */
	upd765_reset_w(m_fdc, BIT(data, 5));

	/* timer 0 enable */
	device_set_input_line(m_maincpu, INPUT_LINE_TMRIN0, BIT(data, 6));

	/* timer 1 enable */
	device_set_input_line(m_maincpu, INPUT_LINE_TMRIN1, BIT(data, 7));
}

WRITE8_MEMBER( tandy2k_state::dma_mux_w )
{
	/*

        bit     description

        0       DMA channel 0 enable
        1       DMA channel 1 enable
        2       DMA channel 2 enable
        3       DMA channel 3 enable
        4       DMA channel 0 select
        5       DMA channel 1 select
        6       DMA channel 2 select
        7       DMA channel 3 select

    */

	m_dma_mux = data;

	/* check for DMA error */
	int drq0 = 0;
	int drq1 = 0;

	for (int ch = 0; ch < 4; ch++)
	{
		if (BIT(data, ch)) { if (BIT(data, ch + 4)) drq1++; else drq0++; }
	}

	int dme = (drq0 > 2) || (drq1 > 2);

	pic8259_ir6_w(m_pic1, dme);
}

READ16_MEMBER( tandy2k_state::vpac_r )
{
	if (ACCESSING_BITS_0_7)
	{
		return m_vpac->read(space, offset);
	}
	else
	{
		return 0xff00;
	}
}

WRITE16_MEMBER( tandy2k_state::vpac_w )
{
	if (ACCESSING_BITS_0_7)
	{
		m_vpac->write(space, offset, data & 0xff);
	}
	else
	{
		addr_ctrl_w(space, offset, data >> 8);
	}
}

static READ8_DEVICE_HANDLER( fldtc_r )
{
	upd765_tc_w(device, 1);
	upd765_tc_w(device, 0);

	return 0;
}

static WRITE8_DEVICE_HANDLER( fldtc_w )
{
	upd765_tc_w(device, 1);
	upd765_tc_w(device, 0);
}

WRITE8_MEMBER( tandy2k_state::addr_ctrl_w )
{
	/*

        bit     signal      description

        8       A15         A15 of video access
        9       A16         A16 of video access
        10      A17         A17 of video access
        11      A18         A18 of video access
        12      A19         A19 of video access
        13      CLKSPD      clock speed (0 = 22.4 MHz, 1 = 28 MHz)
        14      CLKCNT      dots/char (0 = 10 [800x400], 1 = 8 [640x400])
        15      VIDOUTS     selects the video source for display on monochrome monitor

    */

	/* video access */
	m_vram_base = data & 0x1f;

	/* dots per char */
	int character_width = BIT(data, 6) ? 8 : 10;

	if (m_clkcnt != BIT(data, 6))
	{
		m_vpac->set_hpixels_per_column(character_width);
		m_clkcnt = BIT(data, 6);
	}

	/* video clock speed */
	if (m_clkspd != BIT(data, 5))
	{
		float pixel_clock = BIT(data, 5) ? XTAL_16MHz*28/16 : XTAL_16MHz*28/20;
		float character_clock = pixel_clock / character_width;

		m_vpac->set_unscaled_clock(pixel_clock);
		m_vac->set_unscaled_clock(character_clock);
		m_clkspd = BIT(data, 5);
	}

	/* video source select */
	m_vidouts = BIT(data, 7);

	logerror("Address Control %02x\n", data);
}

READ8_MEMBER( tandy2k_state::keyboard_x0_r )
{
  /*

        bit     description

        0       X0
        1       X1
        2       X2
        3       X3
        4       X4
        5       X5
        6       X6
        7       X7

    */

	UINT8 data = 0xff;

	if (!BIT(m_keylatch, 0)) data &= input_port_read(machine(), "Y0");
	if (!BIT(m_keylatch, 1)) data &= input_port_read(machine(), "Y1");
	if (!BIT(m_keylatch, 2)) data &= input_port_read(machine(), "Y2");
	if (!BIT(m_keylatch, 3)) data &= input_port_read(machine(), "Y3");
	if (!BIT(m_keylatch, 4)) data &= input_port_read(machine(), "Y4");
	if (!BIT(m_keylatch, 5)) data &= input_port_read(machine(), "Y5");
	if (!BIT(m_keylatch, 6)) data &= input_port_read(machine(), "Y6");
	if (!BIT(m_keylatch, 7)) data &= input_port_read(machine(), "Y7");
	if (!BIT(m_keylatch, 8)) data &= input_port_read(machine(), "Y8");
	if (!BIT(m_keylatch, 9)) data &= input_port_read(machine(), "Y9");
	if (!BIT(m_keylatch, 10)) data &= input_port_read(machine(), "Y10");
	if (!BIT(m_keylatch, 11)) data &= input_port_read(machine(), "Y11");

	return ~data;
}

WRITE8_MEMBER( tandy2k_state::keyboard_y0_w )
{
	/*

        bit     description

        0       Y0
        1       Y1
        2       Y2
        3       Y3
        4       Y4
        5       Y5
        6       Y6
        7       Y7

    */

	/* keyboard row select */
	m_keylatch = (m_keylatch & 0xff00) | data;
}

WRITE8_MEMBER( tandy2k_state::keyboard_y8_w )
{
	/*

        bit     description

        0       Y8
        1       Y9
        2       Y10
        3       Y11
        4       LED 2
        5       LED 1
        6
        7

    */

	/* keyboard row select */
	m_keylatch = ((data & 0x0f) << 8) | (m_keylatch & 0xff);
}

/* Memory Maps */

static ADDRESS_MAP_START( tandy2k_mem, AS_PROGRAM, 16, tandy2k_state )
	ADDRESS_MAP_UNMAP_HIGH
//  AM_RANGE(0x00000, 0xdffff) AM_RAM
	AM_RANGE(0xe0000, 0xf7fff) AM_RAM AM_BASE(m_hires_ram)
	AM_RANGE(0xf8000, 0xfbfff) AM_RAM AM_BASE(m_char_ram)
	AM_RANGE(0xfc000, 0xfdfff) AM_MIRROR(0x2000) AM_ROM AM_REGION(I80186_TAG, 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START( tandy2k_io, AS_IO, 16, tandy2k_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00000, 0x00001) AM_READWRITE8(enable_r, enable_w, 0x00ff)
	AM_RANGE(0x00002, 0x00003) AM_WRITE8(dma_mux_w, 0x00ff)
	AM_RANGE(0x00004, 0x00005) AM_DEVREADWRITE8_LEGACY(I8272A_TAG, fldtc_r, fldtc_w, 0x00ff)
	AM_RANGE(0x00010, 0x00013) AM_DEVREADWRITE8(I8251A_TAG, i8251_device, data_r, data_w, 0x00ff)
	AM_RANGE(0x00030, 0x00031) AM_DEVREAD8_LEGACY(I8272A_TAG, upd765_status_r, 0x00ff)
	AM_RANGE(0x00032, 0x00033) AM_DEVREADWRITE8_LEGACY(I8272A_TAG, upd765_data_r, upd765_data_w, 0x00ff)
	AM_RANGE(0x00040, 0x00047) AM_DEVREADWRITE8_LEGACY(I8253_TAG, pit8253_r, pit8253_w, 0x00ff)
	AM_RANGE(0x00050, 0x00057) AM_DEVREADWRITE8(I8255A_TAG, i8255_device, read, write, 0x00ff)
	AM_RANGE(0x00060, 0x00063) AM_DEVREADWRITE8_LEGACY(I8259A_0_TAG, pic8259_r, pic8259_w, 0x00ff)
	AM_RANGE(0x00070, 0x00073) AM_DEVREADWRITE8_LEGACY(I8259A_1_TAG, pic8259_r, pic8259_w, 0x00ff)
	AM_RANGE(0x00080, 0x00081) AM_DEVREADWRITE8_LEGACY(I8272A_TAG, upd765_dack_r, upd765_dack_w, 0x00ff)
//  AM_RANGE(0x00100, 0x0017f) AM_DEVREADWRITE8(CRT9007_TAG, crt9007_device, read, write, 0x00ff) AM_WRITE8(addr_ctrl_w, 0xff00)
	AM_RANGE(0x00100, 0x0017f) AM_READWRITE(vpac_r, vpac_w)
//  AM_RANGE(0x00180, 0x00180) AM_READ8(hires_status_r, 0x00ff)
//  AM_RANGE(0x00180, 0x001bf) AM_WRITE(hires_palette_w)
//  AM_RANGE(0x001a0, 0x001a0) AM_READ8(hires_plane_w, 0x00ff)
//  AM_RANGE(0x0ff00, 0x0ffff) AM_READWRITE(i186_internal_port_r, i186_internal_port_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( tandy2k_hd_io, AS_IO, 16, tandy2k_state )
	AM_IMPORT_FROM(tandy2k_io)
//  AM_RANGE(0x000e0, 0x000ff) AM_WRITE8(hdc_dack_w, 0x00ff)
//  AM_RANGE(0x0026c, 0x0026d) AM_DEVREADWRITE8_LEGACY(WD1010_TAG, hdc_reset_r, hdc_reset_w, 0x00ff)
//  AM_RANGE(0x0026e, 0x0027f) AM_DEVREADWRITE8_LEGACY(WD1010_TAG, wd1010_r, wd1010_w, 0x00ff)
ADDRESS_MAP_END

static ADDRESS_MAP_START( vpac_mem, AS_0, 8, tandy2k_state )
	AM_RANGE(0x0000, 0x3fff) AM_READ(videoram_r)
ADDRESS_MAP_END

static ADDRESS_MAP_START( keyboard_io, AS_IO, 8, tandy2k_state )
  AM_RANGE(MCS48_PORT_P1, MCS48_PORT_P1) AM_WRITE(keyboard_y0_w)
  AM_RANGE(MCS48_PORT_P2, MCS48_PORT_P2) AM_WRITE(keyboard_y8_w)
  AM_RANGE(MCS48_PORT_BUS, MCS48_PORT_BUS) AM_READ(keyboard_x0_r)
ADDRESS_MAP_END

/* Input Ports */

static INPUT_PORTS_START( tandy2k )
	PORT_START("ROW0")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW1")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW2")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW3")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW4")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW5")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW6")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW7")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW8")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW9")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW10")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("ROW11")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
INPUT_PORTS_END

/* Video */

bool tandy2k_state::screen_update(screen_device &screen, bitmap_t &bitmap, const rectangle &cliprect)
{
	if (m_vidouts)
	{
		m_vac->update_screen(&bitmap, &cliprect);
	}

	return 0;
}
/*
static CRT9007_DRAW_SCANLINE( tandy2k_crt9007_display_pixels )
{
    tandy2k_state *state = device->machine().driver_data<tandy2k_state>();
    address_space *program = state->m_maincpu->memory().space(AS_PROGRAM);

    for (int sx = 0; sx < x_count; sx++)
    {
        UINT32 videoram_addr = ((state->m_vram_base << 15) | (va << 1)) + sx;
        UINT8 videoram_data = program->read_word(videoram_addr);
        UINT16 charram_addr = (videoram_data << 4) | sl;
        UINT8 charram_data = state->m_char_ram[charram_addr] & 0xff;

        for (int bit = 0; bit < 10; bit++)
        {
            if (BIT(charram_data, 7))
            {
                *BITMAP_ADDR16(bitmap, y, x + (sx * 10) + bit) = 1;
            }

            charram_data <<= 1;
        }
    }
}
*/

WRITE_LINE_MEMBER( tandy2k_state::vpac_vlt_w )
{
	m_drb0->clrcnt_w(state);
	m_drb1->clrcnt_w(state);
}

WRITE_LINE_MEMBER( tandy2k_state::vpac_drb_w )
{
	m_drb0->tog_w(state);
	m_drb1->tog_w(state);
}

static CRT9007_INTERFACE( vpac_intf )
{
	SCREEN_TAG,
	10,
	DEVCB_DEVICE_LINE(I8259A_1_TAG, pic8259_ir1_w),
	DEVCB_NULL,	// DMAR     80186 HOLD
	DEVCB_DEVICE_LINE_MEMBER(CRT9021B_TAG, crt9021_device, vsync_w), // VS
	DEVCB_NULL,	// HS
	DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, vpac_vlt_w), // VLT
	DEVCB_DEVICE_LINE_MEMBER(CRT9021B_TAG, crt9021_device, cursor_w), // CURS
	DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, vpac_drb_w), // DRB
	DEVCB_DEVICE_LINE_MEMBER(CRT9021B_TAG, crt9021_device, retbl_w), // CBLANK
	DEVCB_DEVICE_LINE_MEMBER(CRT9021B_TAG, crt9021_device, slg_w), // SLG
	DEVCB_DEVICE_LINE_MEMBER(CRT9021B_TAG, crt9021_device, sld_w) // SLD
};

static CRT9212_INTERFACE( drb0_intf )
{
	DEVCB_NULL, // ROF
	DEVCB_NULL, // WOF
	DEVCB_DEVICE_LINE_MEMBER(CRT9007_TAG, crt9007_device, vlt_r), // REN
	DEVCB_DEVICE_LINE_MEMBER(CRT9007_TAG, crt9007_device, wben_r), // WEN
	DEVCB_LINE_VCC // WEN2
};

static CRT9212_INTERFACE( drb1_intf )
{
	DEVCB_NULL, // ROF
	DEVCB_NULL, // WOF
	DEVCB_DEVICE_LINE_MEMBER(CRT9007_TAG, crt9007_device, vlt_r), // REN
	DEVCB_DEVICE_LINE_MEMBER(CRT9007_TAG, crt9007_device, wben_r), // WEN
	DEVCB_LINE_VCC // WEN2
};

static CRT9021_INTERFACE( vac_intf )
{
	SCREEN_TAG,
	DEVCB_DEVICE_MEMBER(CRT9212_0_TAG, crt9212_device, read), // data
	DEVCB_DEVICE_MEMBER(CRT9212_1_TAG, crt9212_device, read), // attributes
	DEVCB_LINE_VCC // ATTEN
};

/* Intel 8251A Interface */

WRITE_LINE_MEMBER( tandy2k_state::rxrdy_w )
{
	m_rxrdy = state;
	pic8259_ir2_w(m_pic0, m_rxrdy | m_txrdy);
}

WRITE_LINE_MEMBER( tandy2k_state::txrdy_w )
{
	m_txrdy = state;
	pic8259_ir2_w(m_pic0, m_rxrdy | m_txrdy);
}

static const i8251_interface i8251_intf =
{
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, rxrdy_w),
	DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, txrdy_w),
	DEVCB_NULL,
	DEVCB_NULL
};

/* Intel 8253 Interface */

WRITE_LINE_MEMBER( tandy2k_state::outspkr_w )
{
	m_outspkr = state;
	speaker_update();
}

WRITE_LINE_MEMBER( tandy2k_state::intbrclk_w )
{
	if (!m_extclk && state)
	{
		m_uart->transmit_clock();
		m_uart->receive_clock();
	}
}

WRITE_LINE_MEMBER( tandy2k_state::rfrqpulse_w )
{
}

static const struct pit8253_config i8253_intf =
{
	{
		{
			XTAL_16MHz/16,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, outspkr_w)
		}, {
			XTAL_16MHz/8,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, intbrclk_w)
		}, {
			XTAL_16MHz/8,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, rfrqpulse_w)
		}
	}
};

/* Intel 8255A Interface */

enum
{
	LPINEN = 0,
	KBDINEN,
	PORTINEN
};

READ8_MEMBER( tandy2k_state::ppi_pb_r )
{
	/*

        bit     signal          description

        0       LPRIN0          auxiliary input 0
        1       LPRIN1          auxiliary input 1
        2       LPRIN2          auxiliary input 2
        3       _LPRACK         acknowledge
        4       _LPRFLT         fault
        5       _LPRSEL         select
        6       LPRPAEM         paper empty
        7       LPRBSY          busy

    */

	UINT8 data = 0;

	switch (m_pb_sel)
	{
	case LPINEN:
		/* printer acknowledge */
		data |= centronics_ack_r(m_centronics) << 3;

		/* printer fault */
		data |= centronics_fault_r(m_centronics) << 4;

		/* printer select */
		data |= centronics_vcc_r(m_centronics) << 5;

		/* paper empty */
		data |= centronics_pe_r(m_centronics) << 6;

		/* printer busy */
		data |= centronics_busy_r(m_centronics) << 7;
		break;

	case KBDINEN:
		/* keyboard data */
		break;

	case PORTINEN:
		/* PCB revision */
		data = 0x03;
		break;
	}

	return data;
}

WRITE8_MEMBER( tandy2k_state::ppi_pc_w )
{
	/*

        bit     signal          description

        0                       port A direction
        1                       port B input select bit 0
        2                       port B input select bit 1
        3       LPRINT13        interrupt
        4       STROBE IN
        5       INBUFFULL
        6       _LPRACK
        7       _LPRDATSTB

    */

	/* input select */
	m_pb_sel = (data >> 1) & 0x03;

	/* interrupt */
	pic8259_ir3_w(m_pic1, BIT(data, 3));

	/* printer strobe */
	centronics_strobe_w(m_centronics, BIT(data, 7));
}

static I8255A_INTERFACE( i8255_intf )
{
	DEVCB_NULL,													// Port A read
	DEVCB_DEVICE_HANDLER(CENTRONICS_TAG, centronics_data_w),	// Port A write
	DEVCB_DRIVER_MEMBER(tandy2k_state, ppi_pb_r),				// Port B write
	DEVCB_NULL,													// Port B write
	DEVCB_NULL,													// Port C read
	DEVCB_DRIVER_MEMBER(tandy2k_state, ppi_pc_w)				// Port C write
};

/* Intel 8259 Interfaces */

/*

    IR0     MEMINT00
    IR1     TMOINT01
    IR2     SERINT02
    IR3     BUSINT03
    IR4     FLDINT04
    IR5     BUSINT05
    IR6     HDCINT06
    IR7     BUSINT07

*/

static const struct pic8259_interface i8259_0_intf =
{
	DEVCB_CPU_INPUT_LINE(I80186_TAG, INPUT_LINE_INT0),
	DEVCB_LINE_VCC,
	DEVCB_NULL
};

/*

    IR0     KBDINT10
    IR1     VIDINT11
    IR2     RATINT12
    IR3     LPRINT13
    IR4     MCPINT14
    IR5     MEMINT15
    IR6     DMEINT16
    IR7     BUSINT17

*/

static const struct pic8259_interface i8259_1_intf =
{
	DEVCB_CPU_INPUT_LINE(I80186_TAG, INPUT_LINE_INT1),
	DEVCB_LINE_VCC,
	DEVCB_NULL
};

/* Floppy Configuration */

static const floppy_interface tandy2k_floppy_interface =
{
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	FLOPPY_STANDARD_5_25_DSQD,
	LEGACY_FLOPPY_OPTIONS_NAME(default),
	NULL,
	NULL
};

/* Intel 8272 Interface */

WRITE_LINE_MEMBER( tandy2k_state::busdmarq0_w )
{
	dma_request(0, state);
}

static const struct upd765_interface i8272_intf =
{
	DEVCB_DEVICE_LINE(I8259A_0_TAG, pic8259_ir4_w),
	DEVCB_DRIVER_LINE_MEMBER(tandy2k_state, busdmarq0_w),
	NULL,
	UPD765_RDY_PIN_CONNECTED,
	{ FLOPPY_0, FLOPPY_1, NULL, NULL }
};

/* Centronics Interface */

static const centronics_interface centronics_intf =
{
	0,												/* is IBM PC? */
	DEVCB_DEVICE_LINE_MEMBER(I8255A_TAG, i8255_device, pc6_w),	/* ACK output */
	DEVCB_NULL,										/* BUSY output */
	DEVCB_NULL										/* NOT BUSY output */
};

/* Machine Initialization */

void tandy2k_state::machine_start()
{
	/* memory banking */
	address_space *program = m_maincpu->memory().space(AS_PROGRAM);
	UINT8 *ram = m_ram->pointer();
	int ram_size = m_ram->size();

	program->install_ram(0x00000, ram_size - 1, ram);

	/* patch out i186 relocation register check */
	UINT8 *rom = machine().region(I80186_TAG)->base();
	rom[0x1f16] = 0x90;
	rom[0x1f17] = 0x90;

	/* register for state saving */
	save_item(NAME(m_dma_mux));
	save_item(NAME(m_kben));
	save_item(NAME(m_keylatch));
	save_item(NAME(m_extclk));
	save_item(NAME(m_rxrdy));
	save_item(NAME(m_txrdy));
	save_item(NAME(m_pb_sel));
	save_item(NAME(m_vidouts));
	save_item(NAME(m_clkspd));
	save_item(NAME(m_clkcnt));
	save_item(NAME(m_outspkr));
	save_item(NAME(m_spkrdata));
}

/* Machine Driver */

static MACHINE_CONFIG_START( tandy2k, tandy2k_state )
    /* basic machine hardware */
	MCFG_CPU_ADD(I80186_TAG, I80186, XTAL_16MHz)
    MCFG_CPU_PROGRAM_MAP(tandy2k_mem)
    MCFG_CPU_IO_MAP(tandy2k_io)

	MCFG_CPU_ADD(I8048_TAG, I8048, 1000000) // ?
	MCFG_CPU_IO_MAP(keyboard_io)
	MCFG_DEVICE_DISABLE()

    /* video hardware */
    MCFG_SCREEN_ADD(SCREEN_TAG, RASTER)
    MCFG_SCREEN_REFRESH_RATE(50)
    MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
    MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
    MCFG_SCREEN_SIZE(640, 480)
    MCFG_SCREEN_VISIBLE_AREA(0, 640-1, 0, 480-1)

	MCFG_PALETTE_LENGTH(2)
    MCFG_PALETTE_INIT(black_and_white)

	MCFG_CRT9007_ADD(CRT9007_TAG, XTAL_16MHz*28/16, vpac_intf, vpac_mem)
	MCFG_CRT9212_ADD(CRT9212_0_TAG, drb0_intf)
	MCFG_CRT9212_ADD(CRT9212_1_TAG, drb1_intf)
	MCFG_CRT9021_ADD(CRT9021B_TAG, XTAL_16MHz*28/16/8, vac_intf)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD(SPEAKER_TAG, SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)

	/* devices */
	MCFG_I8255A_ADD(I8255A_TAG, i8255_intf)
	MCFG_I8251_ADD(I8251A_TAG, i8251_intf)
	MCFG_PIT8253_ADD(I8253_TAG, i8253_intf)
	MCFG_PIC8259_ADD(I8259A_0_TAG, i8259_0_intf)
	MCFG_PIC8259_ADD(I8259A_1_TAG, i8259_1_intf)
	MCFG_UPD765A_ADD(I8272A_TAG, i8272_intf)
	MCFG_LEGACY_FLOPPY_2_DRIVES_ADD(tandy2k_floppy_interface)
	MCFG_CENTRONICS_ADD(CENTRONICS_TAG, standard_centronics)

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("128K")
	MCFG_RAM_EXTRA_OPTIONS("256K,384K,512K,640K,768K,896K")
MACHINE_CONFIG_END

static MACHINE_CONFIG_DERIVED( tandy2k_hd, tandy2k )
    /* basic machine hardware */
	MCFG_CPU_MODIFY(I80186_TAG)
    MCFG_CPU_IO_MAP(tandy2k_hd_io)

	/* Tandon TM502 hard disk */
	//MCFG_WD1010_ADD(WD1010_TAG, wd1010_intf)
MACHINE_CONFIG_END

/* ROMs */

ROM_START( tandy2k )
	ROM_REGION( 0x2000, I80186_TAG, 0 )
	ROM_LOAD16_BYTE( "484a00.u48", 0x0000, 0x1000, CRC(a5ee3e90) SHA1(4b1f404a4337c67065dd272d62ff88dcdee5e34b) )
	ROM_LOAD16_BYTE( "474600.u47", 0x0001, 0x1000, CRC(345701c5) SHA1(a775cbfa110b7a88f32834aaa2a9b868cbeed25b) )

	ROM_REGION( 0x400, I8048_TAG, 0 )
	ROM_LOAD( "keyboard.m1", 0x0000, 0x0400, NO_DUMP )
ROM_END

#define rom_tandy2khd rom_tandy2k

/* System Drivers */

/*    YEAR  NAME        PARENT      COMPAT  MACHINE     INPUT       INIT    COMPANY                 FULLNAME        FLAGS */
COMP( 1983, tandy2k,	0,			0,		tandy2k,	tandy2k,	0,		"Tandy Radio Shack",	"Tandy 2000",	GAME_NOT_WORKING)
COMP( 1983, tandy2khd,	tandy2k,	0,		tandy2k_hd,	tandy2k,	0,		"Tandy Radio Shack",	"Tandy 2000HD",	GAME_NOT_WORKING)
