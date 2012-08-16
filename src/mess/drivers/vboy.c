/***************************************************************************

    Nintendo Virtual Boy

	driver by Miodrag Milanovic & Angelo Salese

    Great info at http://www.goliathindustries.com/vb/
    and http://www.vr32.de/modules/dokuwiki/doku.php?

    TODO:
    - fix graphics (many things to add!)
	- vleague / vproyak: keeps going into auto pause with 100 usec timer?

****************************************************************************/

#include "emu.h"
#include "cpu/v810/v810.h"
#include "imagedev/cartslot.h"
#include "vboy.lh"

typedef struct _vboy_regs_t vboy_regs_t;

/* FIXME: most if not all of these must be UINT8 */
struct _vboy_regs_t
{
	UINT32 lpc, lpc2, lpt, lpr;
	UINT32 khb, klb;
	UINT8 thb, tlb;
	UINT32 tcr, wcr, kcr;
};

typedef struct _vip_regs_t vip_regs_t;
struct _vip_regs_t
{

	UINT16 INTPND;
	UINT16 INTENB;
	UINT16 DPSTTS;
	UINT16 DPCTRL;
	UINT16 BRTA;
	UINT16 BRTB;
	UINT16 BRTC;
	UINT16 REST;
	UINT16 FRMCYC;
	UINT16 CTA;
	UINT16 XPSTTS;
	UINT16 XPCTRL;
	UINT16 VER;
	UINT16 SPT[4];
	UINT16 GPLT[4];
	UINT16 JPLT[4];
	UINT16 BKCOL;
};

struct _vboy_timer_t
{
	UINT16 count;
	UINT16 latch;
};

typedef struct _vboy_timer_t vboy_timer_t;

class vboy_state : public driver_device
{
public:
	vboy_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		  m_maincpu(*this, "maincpu"),
		  m_l_frame_0(*this,"l_frame_0"),
		  m_l_frame_1(*this,"l_frame_1"),
		  m_r_frame_0(*this,"r_frame_0"),
		  m_r_frame_1(*this,"r_frame_1") { }

	required_device<cpu_device> m_maincpu;

	DECLARE_READ32_MEMBER(port_02_read);
	DECLARE_WRITE32_MEMBER(port_02_write);
	DECLARE_READ16_MEMBER(vip_r);
	DECLARE_WRITE16_MEMBER(vip_w);
	DECLARE_WRITE16_MEMBER(vboy_font0_w);
	DECLARE_WRITE16_MEMBER(vboy_font1_w);
	DECLARE_WRITE16_MEMBER(vboy_font2_w);
	DECLARE_WRITE16_MEMBER(vboy_font3_w);
	DECLARE_READ16_MEMBER(vboy_font0_r);
	DECLARE_READ16_MEMBER(vboy_font1_r);
	DECLARE_READ16_MEMBER(vboy_font2_r);
	DECLARE_READ16_MEMBER(vboy_font3_r);
	DECLARE_WRITE16_MEMBER(vboy_bgmap_w);
	DECLARE_READ16_MEMBER(vboy_bgmap_r);
	UINT16 *m_font;
	UINT16 *m_bgmap;
	required_shared_ptr<UINT32> m_l_frame_0;
	required_shared_ptr<UINT32> m_l_frame_1;
	required_shared_ptr<UINT32> m_r_frame_0;
	required_shared_ptr<UINT32> m_r_frame_1;
	UINT16 *m_world;
	UINT16 *m_columntab1;
	UINT16 *m_columntab2;
	UINT16 *m_objects;
	vboy_regs_t m_vboy_regs;
	vip_regs_t m_vip_regs;
	vboy_timer_t m_vboy_timer;
	bitmap_ind16 m_bg_map[16];
	bitmap_ind16 m_screen_output;
	UINT16 m_frame_count;
	UINT8 m_displayfb;
	UINT8 m_display_count;
	UINT8 m_row_num;
	attotime m_input_latch_time;

	void m_timer_tick(UINT8 setting);
	void m_scanline_tick(int scanline, UINT8 screen_type);
	void m_set_irq(UINT16 irq_vector);

	void m_pcg_debug(UINT16 offset,UINT16 data,UINT16 mem_mask);

};

READ32_MEMBER( vboy_state::port_02_read )
{
	UINT32 value = 0x00;

	switch ((offset << 2))
	{
		case 0x10:	// KLB (Keypad Low Byte)
			value = m_vboy_regs.klb;	// 0x02 is always 1
			break;
		case 0x14:	// KHB (Keypad High Byte)
			value = m_vboy_regs.khb;
			break;
		case 0x18:	// TLB (Timer Low Byte)
			value = m_vboy_regs.tlb;
			break;
		case 0x1c:	// THB (Timer High Byte)
			value = m_vboy_regs.thb;
			break;
		case 0x20:	// TCR (Timer Control Reg)
			value = m_vboy_regs.tcr;
			break;
		case 0x24:	// WCR (Wait State Control Reg)
			value = m_vboy_regs.wcr;
			break;
		case 0x28:	// KCR (Keypad Control Reg)
			{
//				attotime new_time = machine().time();

//				if((new_time - m_input_latch_time) < m_maincpu->cycles_to_attotime(640))
					value |= machine().rand() & 2;

				value = m_vboy_regs.kcr | 0x4c;
			}
			break;
		case 0x00:	// LPC (Link Port Control Reg)
		case 0x04:	// LPC2 (Link Port Control Reg)
		case 0x08:	// LPT (Link Port Transmit)
		case 0x0c:	// LPR (Link Port Receive)
		default:
			logerror("Unemulated read: offset %08x\n", 0x02000000 + (offset << 2));
			break;
	}
	return value;
}
WRITE32_MEMBER( vboy_state::port_02_write )
{
	switch (offset<<2)
	{
		case 0x0c:	// LPR (Link Port Receive)
		case 0x10:	// KLB (Keypad Low Byte)
		case 0x14:	// KHB (Keypad High Byte)
			//logerror("Ilegal write: offset %02x should be only read\n", offset);
			break;
		case 0x18:	// TLB (Timer Low Byte)
			m_vboy_regs.tlb = data;
			m_vboy_timer.latch = m_vboy_regs.tlb | (m_vboy_timer.latch & 0xff00);
			break;
		case 0x1c:	// THB (Timer High Byte)
			m_vboy_regs.thb = data;
			m_vboy_timer.latch = (m_vboy_regs.thb<<8) | (m_vboy_timer.latch & 0xff);
			break;
		case 0x20:	// TCR (Timer Control Reg)
			/*
				111- ---- always 1
				---x ---- timer select (1=20 us, 0=100 us)
				---- x--- timer irq
				---- -x-- resets timer zero flag
				---- --x- timer is zero flag
				---- ---x enables timer
			*/
			if(data & 1)
			{
				m_vboy_regs.tlb = m_vboy_timer.latch & 0xff;
				m_vboy_regs.thb = m_vboy_timer.latch >> 8;
				m_vboy_timer.count = m_vboy_timer.latch;
			}

			m_vboy_regs.tcr = (data & 0xfd) | (0xe4) | (m_vboy_regs.tcr & 2);	// according to docs: bits 5, 6 & 7 are unused and set to 1, bit 1 is read only.
			if(data & 4)
				m_vboy_regs.tcr &= 0xfd;
			break;
		case 0x24:	// WCR (Wait State Control Reg)
			m_vboy_regs.wcr = data | 0xfc;	// according to docs: bits 2 to 7 are unused and set to 1.
			break;
		case 0x28:	// KCR (Keypad Control Reg)
			if (data & 0x04 )
			{
				m_vboy_regs.klb = (ioport("INPUT")->read() & 0x00ff);
				m_vboy_regs.khb = (ioport("INPUT")->read() & 0xff00) >> 8;
				//m_input_latch_time = machine().time();
			}

			if (data & 1)
			{
				m_vboy_regs.klb = 0;
				m_vboy_regs.khb = 0;
				//m_input_latch_time = attotime::zero;
			}


			m_vboy_regs.kcr = (data | 0x48) & 0xfd;	// according to docs: bit 6 & bit 3 are unused and set to 1, bit 1 is read only.
			break;
		case 0x00:	// LPC (Link Port Control Reg)
		case 0x04:	// LPC2 (Link Port Control Reg)
		case 0x08:	// LPT (Link Port Transmit)
		default:
			logerror("Unemulated write: offset %08x, data %04x\n", 0x02000000 + (offset << 2), data);
			break;
	}
}

READ16_MEMBER( vboy_state::vip_r )
{
	switch(offset << 1) {
		case 0x00:	//INTPND
					return m_vip_regs.INTPND;
		case 0x02:	//INTENB
					return m_vip_regs.INTENB;
		case 0x04:	//INTCLR
					logerror("Error reading INTCLR\n");
					break;
		case 0x20:	//DPSTTS
		{
			UINT16 res;

			res = m_vip_regs.DPCTRL & 0x0702;

			if(m_vip_regs.DPCTRL & 2)
			{
				UINT8 DPBSY = machine().rand() & 3;

				if(DPBSY & 2)
					res |= 0x20;
				else if(DPBSY & 1)
					res |= 0x08;
			}

			res |= 0x40;
			//printf("%04x\n",res);
			return res;
		}
		case 0x22:	//DPCTRL
					return m_vip_regs.DPCTRL;
		case 0x24:	//BRTA
					return m_vip_regs.BRTA;
		case 0x26:	//BRTB
					return m_vip_regs.BRTB;
		case 0x28:	//BRTC
					return m_vip_regs.BRTC;
		case 0x2A:	//REST
					return m_vip_regs.REST;
		case 0x2E:	//FRMCYC
					return m_vip_regs.FRMCYC;
		case 0x30:	//CTA
					return m_vip_regs.CTA;
		case 0x40:	//XPSTTS, piXel Processor STaTuS
		{
			/*
			---- ---- ---- x--- XPBSY1 (second framebuffer busy flag)
			---- ---- ---- -x-- XPBSY0 (first framebfuffer busy flag)
			*/
			UINT8 drawfb = ((m_displayfb ^ 1) + 1) << 2;
			UINT16 res;

			//printf("%d\n",row_num);

			res =  m_vip_regs.XPSTTS & 0x00f3; // empty ^^'
			if(m_vip_regs.XPCTRL & 2 && m_row_num < 224/8) // screen active
				res |= drawfb;

			{
				res |= 0x8000;
				res |= m_row_num<<8;
			}
			//if(m_vip_regs.DPCTRL & 2)
			//	res |= ((row_num)<<8);

			return res;
		}
		case 0x42:	//XPCTRL
					return m_vip_regs.XPCTRL;
		case 0x44:	//VER
					return m_vip_regs.VER;
		case 0x48:	//SPT0
					return m_vip_regs.SPT[0];
		case 0x4A:	//SPT1
					return m_vip_regs.SPT[1];
		case 0x4C:	//SPT2
					return m_vip_regs.SPT[2];
		case 0x4E:	//SPT3
					return m_vip_regs.SPT[3];
		case 0x60:	//GPLT0
					return m_vip_regs.GPLT[0];
		case 0x62:	//GPLT1
					return m_vip_regs.GPLT[1];
		case 0x64:	//GPLT2
					return m_vip_regs.GPLT[2];
		case 0x66:	//GPLT3
					return m_vip_regs.GPLT[3];
		case 0x68:	//JPLT0
					return m_vip_regs.JPLT[0];
		case 0x6A:	//JPLT1
					return m_vip_regs.JPLT[1];
		case 0x6C:	//JPLT2
					return m_vip_regs.JPLT[2];
		case 0x6E:	//JPLT3
					return m_vip_regs.JPLT[3];
		case 0x70:	//BKCOL
					return m_vip_regs.BKCOL;
		default:
					logerror("Unemulated read: addr %08x\n", offset * 2 + 0x0005f800);
					break;
	}
	return 0xffff;
}

WRITE16_MEMBER( vboy_state::vip_w )
{
	switch(offset << 1) {
		/*
			x--- ---- ---- ---- TIME_ERR
			-x-- ---- ---- ---- XP_END
			--x- ---- ---- ---- SB_HIT
			---- ---- ---x ---- FRAME_START
			---- ---- ---- x--- GAME_START
			---- ---- ---- -x-- RFB_END
			---- ---- ---- --x- LFB_END
			---- ---- ---- ---x SCAN_ERR
		*/
		case 0x00:	//INTPND
					logerror("Error writing INTPND\n");
					break;
		case 0x02:	//INTENB
					m_vip_regs.INTENB = data;
					m_set_irq(0);
					//printf("%04x ENB\n",data);
					break;
		case 0x04:	//INTCLR
					m_vip_regs.INTPND &= ~data;
					m_set_irq(0);
					//else
					//	printf("%04x\n",m_vip_regs.INTPND);
					break;
		case 0x20:	//DPSTTS
					logerror("Error writing DPSTTS\n");
					break;
		case 0x22:	//DPCTRL
					m_vip_regs.DPCTRL = data & 0x0702;

					if(data & 1)
					{
						m_vip_regs.INTPND &= 0xe000; // reset FRAME_START, GAME_START, RFB_END, LFB_END and SCAN_ERR irqs
						m_set_irq(0);
					}
					break;
		case 0x24:	//BRTA
					m_vip_regs.BRTA = data;
					palette_set_color_rgb(machine(), 1,(m_vip_regs.BRTA) & 0xff,0,0);
					//printf("%02x A\n",data);
					break;
		case 0x26:	//BRTB
					m_vip_regs.BRTB = data;
					palette_set_color_rgb(machine(), 2,(m_vip_regs.BRTA + m_vip_regs.BRTB) & 0xff,0,0);
					//printf("%02x B\n",data);
					break;
		case 0x28:	//BRTC
					m_vip_regs.BRTC = data;
					palette_set_color_rgb(machine(), 3,(m_vip_regs.BRTA + m_vip_regs.BRTB + m_vip_regs.BRTC) & 0xff,0,0);
					//printf("%02x C\n",data);
					break;
		case 0x2A:	//REST
					m_vip_regs.REST = data;
					//printf("%02x %08x D\n",data,cpu_get_pc(m_maincpu));
					break;
		case 0x2E:	//FRMCYC
					//printf("%d\n",data);
					m_vip_regs.FRMCYC = data;
					break;
		case 0x30:	//CTA
					m_vip_regs.CTA = data;
					break;
		case 0x40:	//XPSTTS
					logerror("Error writing XPSTTS\n");
					break;
		case 0x42:	//XPCTRL, w/o
					/*
					---- ---- ---- --x-
					---- ---- ---- ---x Reset Pixel Processor
					*/
					m_vip_regs.XPCTRL = data & 0x1f02;

					//if(data & 0x1f00)
					//	printf("%04x SBCMP\n",data);

					if(data & 1)
					{
						m_vip_regs.INTPND &= 0x1fff; // reset SB_HIT, XP_END and TIME_ERR irqs
						m_set_irq(0);
					}
					break;
		case 0x44:	//VER
					//m_vip_regs.VER = data;
					break;
		case 0x48:	//SPT0
					m_vip_regs.SPT[0] = data & 0x3ff;
					break;
		case 0x4A:	//SPT1
					m_vip_regs.SPT[1] = data & 0x3ff;
					break;
		case 0x4C:	//SPT2
					m_vip_regs.SPT[2] = data & 0x3ff;
					break;
		case 0x4E:	//SPT3
					m_vip_regs.SPT[3] = data & 0x3ff;
					break;
		case 0x60:	//GPLT0
					m_vip_regs.GPLT[0] = data;
					break;
		case 0x62:	//GPLT1
					m_vip_regs.GPLT[1] = data;
					break;
		case 0x64:	//GPLT2
					m_vip_regs.GPLT[2] = data;
					break;
		case 0x66:	//GPLT3
					m_vip_regs.GPLT[3] = data;
					break;
		case 0x68:	//JPLT0
					m_vip_regs.JPLT[0] = data & 0xfc;
					break;
		case 0x6A:	//JPLT1
					m_vip_regs.JPLT[1] = data & 0xfc;
					break;
		case 0x6C:	//JPLT2
					m_vip_regs.JPLT[2] = data & 0xfc;
					break;
		case 0x6E:	//JPLT3
					m_vip_regs.JPLT[3] = data & 0xfc;
					break;
		case 0x70:	//BKCOL
					m_vip_regs.BKCOL = data & 3;
					break;
		default:
					logerror("Unemulated write: addr %08x, data %04x\n", offset * 2 + 0x0005f800, data);
					break;
	}
}


void vboy_state::m_pcg_debug(UINT16 offset,UINT16 data,UINT16 mem_mask)
{
	UINT8 *pcg_ram = memregion("pcg")->base();

	if(mem_mask & 0x00ff)
		pcg_ram[(offset<<1)+1] = (data & 0x00ff);

	if(mem_mask & 0xff00)
		pcg_ram[(offset<<1)+0] = (data & 0xff00) >> 8;

	gfx_element_mark_dirty(machine().gfx[0], offset >> 4);
}

WRITE16_MEMBER( vboy_state::vboy_font0_w )
{
	m_font[offset] = data | (m_font[offset] & (mem_mask ^ 0xffff));

	if(1)
		m_pcg_debug(offset,data,mem_mask);
}

WRITE16_MEMBER( vboy_state::vboy_font1_w )
{
	m_font[offset + 0x1000] = data | (m_font[offset + 0x1000] & (mem_mask ^ 0xffff));

	if(1)
		m_pcg_debug(offset+0x1000,data,mem_mask);
}

WRITE16_MEMBER( vboy_state::vboy_font2_w )
{
	m_font[offset + 0x2000] = data | (m_font[offset + 0x2000] & (mem_mask ^ 0xffff));

	if(1)
		m_pcg_debug(offset+0x2000,data,mem_mask);
}

WRITE16_MEMBER( vboy_state::vboy_font3_w )
{
	m_font[offset + 0x3000] = data | (m_font[offset + 0x3000] & (mem_mask ^ 0xffff));

	if(1)
		m_pcg_debug(offset+0x3000,data,mem_mask);
}

READ16_MEMBER( vboy_state::vboy_font0_r )
{
	return m_font[offset];
}

READ16_MEMBER( vboy_state::vboy_font1_r )
{
	return m_font[offset + 0x1000];
}

READ16_MEMBER( vboy_state::vboy_font2_r )
{
	return m_font[offset + 0x2000];
}

READ16_MEMBER( vboy_state::vboy_font3_r )
{
	return m_font[offset + 0x3000];
}

WRITE16_MEMBER( vboy_state::vboy_bgmap_w )
{
	m_bgmap[offset] = data | (m_bgmap[offset] & (mem_mask ^ 0xffff));
}

READ16_MEMBER( vboy_state::vboy_bgmap_r )
{
	return m_bgmap[offset];
}

static ADDRESS_MAP_START( vboy_mem, AS_PROGRAM, 32, vboy_state )
	ADDRESS_MAP_GLOBAL_MASK(0x07ffffff)
	AM_RANGE( 0x00000000, 0x00005fff ) AM_RAM AM_SHARE("l_frame_0") // L frame buffer 0
	AM_RANGE( 0x00006000, 0x00007fff ) AM_READWRITE16(vboy_font0_r, vboy_font0_w, 0xffffffff) // Font 0-511
	AM_RANGE( 0x00008000, 0x0000dfff ) AM_RAM AM_SHARE("l_frame_1") // L frame buffer 1
	AM_RANGE( 0x0000e000, 0x0000ffff ) AM_READWRITE16(vboy_font1_r, vboy_font1_w, 0xffffffff) // Font 512-1023
	AM_RANGE( 0x00010000, 0x00015fff ) AM_RAM AM_SHARE("r_frame_0") // R frame buffer 0
	AM_RANGE( 0x00016000, 0x00017fff ) AM_READWRITE16(vboy_font2_r, vboy_font2_w, 0xffffffff) // Font 1024-1535
	AM_RANGE( 0x00018000, 0x0001dfff ) AM_RAM AM_SHARE("r_frame_1") // R frame buffer 1
	AM_RANGE( 0x0001e000, 0x0001ffff ) AM_READWRITE16(vboy_font3_r, vboy_font3_w, 0xffffffff) // Font 1536-2047

	AM_RANGE( 0x00020000, 0x0003ffff ) AM_READWRITE16(vboy_bgmap_r,vboy_bgmap_w, 0xffffffff) // VIPC memory

	//AM_RANGE( 0x00040000, 0x0005ffff ) AM_RAM // VIPC
	AM_RANGE( 0x0005f800, 0x0005f87f ) AM_READWRITE16(vip_r, vip_w, 0xffffffff)

	AM_RANGE( 0x00078000, 0x00079fff ) AM_READWRITE16(vboy_font0_r, vboy_font0_w, 0xffffffff) // Font 0-511 mirror
	AM_RANGE( 0x0007a000, 0x0007bfff ) AM_READWRITE16(vboy_font1_r, vboy_font1_w, 0xffffffff) // Font 512-1023 mirror
	AM_RANGE( 0x0007c000, 0x0007dfff ) AM_READWRITE16(vboy_font2_r, vboy_font2_w, 0xffffffff) // Font 1024-1535 mirror
	AM_RANGE( 0x0007e000, 0x0007ffff ) AM_READWRITE16(vboy_font3_r, vboy_font3_w, 0xffffffff) // Font 1536-2047 mirror

	AM_RANGE( 0x01000000, 0x010005ff ) AM_RAM // Sound RAM
	AM_RANGE( 0x02000000, 0x0200002b ) AM_MIRROR(0x0ffff00) AM_READWRITE(port_02_read, port_02_write) // Hardware control registers mask 0xff
	//AM_RANGE( 0x04000000, 0x04ffffff ) // Expansion area
	AM_RANGE( 0x05000000, 0x0500ffff ) AM_MIRROR(0x0ff0000) AM_RAM // Main RAM - 64K mask 0xffff
	AM_RANGE( 0x06000000, 0x06003fff ) AM_RAM // Cart RAM - 8K
	AM_RANGE( 0x07000000, 0x071fffff ) AM_MIRROR(0x0e00000) AM_ROM AM_REGION("user1", 0) /* ROM */
ADDRESS_MAP_END

static ADDRESS_MAP_START( vboy_io, AS_IO, 32, vboy_state )
	ADDRESS_MAP_GLOBAL_MASK(0x07ffffff)
	AM_RANGE( 0x00000000, 0x00005fff ) AM_RAM AM_SHARE("l_frame_0") // L frame buffer 0
	AM_RANGE( 0x00006000, 0x00007fff ) AM_READWRITE16(vboy_font0_r, vboy_font0_w, 0xffffffff) // Font 0-511
	AM_RANGE( 0x00008000, 0x0000dfff ) AM_RAM AM_SHARE("l_frame_1") // L frame buffer 1
	AM_RANGE( 0x0000e000, 0x0000ffff ) AM_READWRITE16(vboy_font1_r, vboy_font1_w, 0xffffffff) // Font 512-1023
	AM_RANGE( 0x00010000, 0x00015fff ) AM_RAM AM_SHARE("r_frame_0") // R frame buffer 0
	AM_RANGE( 0x00016000, 0x00017fff ) AM_READWRITE16(vboy_font2_r, vboy_font2_w, 0xffffffff) // Font 1024-1535
	AM_RANGE( 0x00018000, 0x0001dfff ) AM_RAM AM_SHARE("r_frame_1") // R frame buffer 1
	AM_RANGE( 0x0001e000, 0x0001ffff ) AM_READWRITE16(vboy_font3_r, vboy_font3_w, 0xffffffff) // Font 1536-2047

	AM_RANGE( 0x00020000, 0x0003ffff ) AM_READWRITE16(vboy_bgmap_r,vboy_bgmap_w, 0xffffffff) // VIPC memory

	//AM_RANGE( 0x00040000, 0x0005ffff ) AM_RAM // VIPC
	AM_RANGE( 0x0005f800, 0x0005f87f ) AM_READWRITE16(vip_r, vip_w, 0xffffffff)

	AM_RANGE( 0x00078000, 0x00079fff ) AM_READWRITE16(vboy_font0_r, vboy_font0_w, 0xffffffff) // Font 0-511 mirror
	AM_RANGE( 0x0007a000, 0x0007bfff ) AM_READWRITE16(vboy_font1_r, vboy_font1_w, 0xffffffff) // Font 512-1023 mirror
	AM_RANGE( 0x0007c000, 0x0007dfff ) AM_READWRITE16(vboy_font2_r, vboy_font2_w, 0xffffffff) // Font 1024-1535 mirror
	AM_RANGE( 0x0007e000, 0x0007ffff ) AM_READWRITE16(vboy_font3_r, vboy_font3_w, 0xffffffff) // Font 1536-2047 mirror

	AM_RANGE( 0x01000000, 0x010005ff ) AM_RAM // Sound RAM
	AM_RANGE( 0x02000000, 0x0200002b ) AM_MIRROR(0x0ffff00) AM_READWRITE(port_02_read, port_02_write) // Hardware control registers mask 0xff
	//AM_RANGE( 0x04000000, 0x04ffffff ) // Expansion area
	AM_RANGE( 0x05000000, 0x0500ffff ) AM_MIRROR(0x0ff0000) AM_RAM // Main RAM - 64K mask 0xffff
	AM_RANGE( 0x06000000, 0x06003fff ) AM_RAM // Cart RAM - 8K
	AM_RANGE( 0x07000000, 0x071fffff ) AM_MIRROR(0x0e00000) AM_ROM AM_REGION("user1", 0) /* ROM */
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( vboy )
	PORT_START("INPUT")
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_PLAYER(1)
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_START ) PORT_PLAYER(1)
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP ) PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP ) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("L") PORT_PLAYER(1) // Left button on back
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("R") PORT_PLAYER(1) // Right button on back
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("A") PORT_PLAYER(1) // A button (or B?)
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("B") PORT_PLAYER(1) // B button (or A?)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_UNUSED ) // Always 1
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_UNUSED ) // Battery low
INPUT_PORTS_END


static MACHINE_RESET(vboy)
{
	vboy_state *state = machine.driver_data<vboy_state>();

	/* Initial values taken from Reality Boy, to be verified when emulation improves */
	state->m_vboy_regs.lpc = 0x6d;
	state->m_vboy_regs.lpc2 = 0xff;
	state->m_vboy_regs.lpt = 0x00;
	state->m_vboy_regs.lpr = 0x00;
	state->m_vboy_regs.klb = 0x00;
	state->m_vboy_regs.khb = 0x00;
	state->m_vboy_regs.tlb = 0xff;
	state->m_vboy_regs.thb = 0xff;
	state->m_vboy_regs.tcr = 0xe4;
	state->m_vboy_regs.wcr = 0xfc;
	state->m_vboy_regs.kcr = 0x4c;
}


static VIDEO_START( vboy )
{
	vboy_state *state = machine.driver_data<vboy_state>();
	int i;

	// Allocate memory for temporary screens
	for(i = 0; i < 16; i++)
		state->m_bg_map[i].allocate(512, 512, BITMAP_FORMAT_IND16);

	state->m_screen_output.allocate(384, 224, BITMAP_FORMAT_IND16);
	state->m_font  = auto_alloc_array(machine, UINT16, 2048 * 8);
	state->m_bgmap = auto_alloc_array(machine, UINT16, 0x20000 >> 1);
	state->m_objects = state->m_bgmap + (0x1E000 >> 1);
	state->m_columntab1 = state->m_bgmap + (0x1dc00 >> 1);
	state->m_columntab2 = state->m_bgmap + (0x1de00 >> 1);
	state->m_world = state->m_bgmap + (0x1d800 >> 1);
}

static void put_char(vboy_state *state, bitmap_ind16 &bitmap, int x, int y, UINT16 ch, bool flipx, bool flipy, bool trans, UINT8 pal)
{
	UINT16 data, code = ch;
	UINT8 yi, xi, dat;

	for (yi = 0; yi < 8; yi++)
	{
		if (!flipy)
			 data = state->m_font[code * 8 + yi];
		else
			 data = state->m_font[code * 8 + (7-yi)];

		for (xi = 0; xi < 8; xi++)
		{
			int res_x,res_y;

			if (!flipx)
				dat = ((data >> (xi << 1)) & 0x03);
			else
				dat = ((data >> ((7-xi) << 1)) & 0x03);

			//col = (pal >> (dat*2)) & 3; // TODO: check what this is for!

			res_x = x + xi;
			res_y = y + yi;

			//if(res_x > (384) || res_y > 224 || res_x < (0) || res_y < 0)
			//	continue;

			if (!trans || (trans && dat))
				bitmap.pix16((res_y), (res_x)) = state->machine().pens[dat];
		}
	}
}

static void fill_bg_map(vboy_state *state, int num, bitmap_ind16 &bitmap)
{
	int i, j;

	// Fill background map
	for (i = 0; i < 64; i++)
	{
		for (j = 0; j < 64; j++)
		{
			UINT16 val = state->m_bgmap[j + 64 * i + (num * 0x1000)];
			put_char(state, bitmap, j * 8, i * 8, val & 0x7ff, BIT(val,13), BIT(val,12), false, state->m_vip_regs.GPLT[(val >> 14) & 3]);
		}
	}
}

static UINT8 display_world(vboy_state *state, int num, bitmap_ind16 &bitmap, bool right, int &cur_spt)
{
	num <<= 4;
	UINT16 def = state->m_world[num];
	UINT8 lon = (def >> 15) & 1;
	UINT8 ron = (def >> 14) & 1;
	INT16 gx  = state->m_world[num+1];
	INT16 gp  = state->m_world[num+2];
	INT16 gy  = state->m_world[num+3];
	INT16 mx  = state->m_world[num+4];
	INT16 mp  = state->m_world[num+5];
	INT16 my  = state->m_world[num+6];
	UINT16 w  = state->m_world[num+7];
	UINT16 h  = state->m_world[num+8];
	UINT16 param_base = state->m_world[num+9] & 0xfff0;
//  UINT16 overplane = state->m_world[num+10];
	UINT8 bg_map_num = def & 0x0f;
	INT16 x,y;
	UINT8 mode = (def >> 12) & 3;
	UINT16 *vboy_paramtab;
	int i;

	vboy_paramtab = state->m_bgmap + param_base;

	if(def & 0x40) // END flag
		return 1;

	if (mode < 3)
	{
		fill_bg_map(state, bg_map_num, state->m_bg_map[bg_map_num]);
		if (BIT(def,15) && (!right))
		{
			// Left screen
			for(y=0;y<=h;y++)
			{
				for(x=0;x<=w;x++)
				{
					INT16 y1 = (y+gy);
					INT16 x1 = (x+gx-gp);
					UINT16 pix = 0;
					if (mode==1)
						x1 += vboy_paramtab[y*2];

					pix = state->m_bg_map[bg_map_num].pix16((y+my) & 0x1ff, (x+mx-mp) & 0x1ff);
					if (pix)
						if (y1>=0 && y1<224)
							if (x1>=0 && x1<384)
								bitmap.pix16(y1, x1) = pix;
				}
			}
		}

		if (BIT(def,14) && (right))
		{
			// Right screen
			for(y=0;y<=h;y++)
			{
				for(x=0; x<=w; x++)
				{
					INT16 y1 = (y+gy);
					INT16 x1 = (x+gx+gp);
					UINT16 pix = 0;
					if (mode==1)
						x1 += vboy_paramtab[y*2+1];

					pix = state->m_bg_map[bg_map_num].pix16((y+my) & 0x1ff, (x+mx+mp) & 0x1ff);
					if (pix)
						if (y1>=0 && y1<224)
							if (x1>=0 && x1<384)
								bitmap.pix16(y1, x1) = pix;
				}
			}
		}
	}
	else
	if (mode==2)
	{
		//popmessage("Mode 2 used");
	}
	else
	if (mode==3)
	{
		int start_offs, end_offs;

		if(cur_spt == -1)
		{
			popmessage("Cur spt used with -1 pointer!");
			return 0;
		}

		start_offs = state->m_vip_regs.SPT[cur_spt];

		end_offs = 0x3ff;
		if(cur_spt != 0)
			end_offs = state->m_vip_regs.SPT[cur_spt-1];

		i = start_offs;
		do
		{
			UINT16 start_ndx = i * 4;
			INT16 jx = state->m_objects[start_ndx+0];
			INT16 jp = state->m_objects[start_ndx+1] & 0x3fff;
			INT16 jy = state->m_objects[start_ndx+2] & 0x1ff;
			UINT16 val = state->m_objects[start_ndx+3];
			UINT8 jlon = (state->m_objects[start_ndx+1] & 0x8000) >> 15;
			UINT8 jron = (state->m_objects[start_ndx+1] & 0x4000) >> 14;

			if (!right && jlon)
				put_char(state, bitmap, (jx-jp) & 0x1ff, jy, val & 0x7ff, BIT(val,13), BIT(val,12), true, state->m_vip_regs.JPLT[(val>>14) & 3]);

			if(right && jron)
				put_char(state, bitmap, (jx+jp) & 0x1ff, jy, val & 0x7ff, BIT(val,13), BIT(val,12), true, state->m_vip_regs.JPLT[(val>>14) & 3]);

			i --;
			i &= 0x3ff;
		}while(i != end_offs);

		if((lon && !right) || (ron && right))
			cur_spt --;
	}

	return 0;
}

static SCREEN_UPDATE_IND16( vboy_left )
{
	vboy_state *state = screen.machine().driver_data<vboy_state>();
	state->m_screen_output.fill(state->m_vip_regs.BKCOL, cliprect);
	int cur_spt;

	cur_spt = 3;
	for(int i=31; i>=0; i--)
		if (display_world(state, i, state->m_screen_output, 0,cur_spt)) break;

	copybitmap(bitmap, state->m_screen_output, 0, 0, 0, 0, cliprect);
	return 0;
}

static SCREEN_UPDATE_IND16( vboy_right )
{
	vboy_state *state = screen.machine().driver_data<vboy_state>();
	state->m_screen_output.fill(state->m_vip_regs.BKCOL, cliprect);
	int cur_spt;

	cur_spt = 3;
	for(int i=31; i>=0; i--)
		if (display_world(state, i, state->m_screen_output, 1,cur_spt)) break;

	copybitmap(bitmap, state->m_screen_output, 0, 0, 0, 0, cliprect);
	return 0;
}

void vboy_state::m_timer_tick(UINT8 setting)
{
	if(m_vboy_regs.tcr & 1 && ((m_vboy_regs.tcr & 0x10) == setting))
	{
		if(m_vboy_timer.count != 0)
		{
			m_vboy_timer.count--;
			m_vboy_regs.tlb = m_vboy_timer.count & 0xff;
			m_vboy_regs.thb = m_vboy_timer.count >> 8;
		}
		else
		{
			m_vboy_timer.count = m_vboy_timer.latch;
			//printf("Zero timer trigger\n");
			m_vboy_regs.tcr |= 0x02;
			if(m_vboy_regs.tcr & 8)
				device_set_input_line(m_maincpu, 1, HOLD_LINE);
		}
	}
}

static TIMER_DEVICE_CALLBACK( timer_100us_tick )
{
	vboy_state *state = timer.machine().driver_data<vboy_state>();

	state->m_timer_tick(0x00);
}

static TIMER_DEVICE_CALLBACK( timer_20us_tick )
{
	vboy_state *state = timer.machine().driver_data<vboy_state>();

	state->m_timer_tick(0x10);
}

static PALETTE_INIT( vboy )
{
	palette_set_color(machine, 0, RGB_BLACK);
	palette_set_color(machine, 1, RGB_BLACK);
	palette_set_color(machine, 2, RGB_BLACK);
	palette_set_color(machine, 3, RGB_BLACK);
}

void vboy_state::m_set_irq(UINT16 irq_vector)
{
	m_vip_regs.INTPND |= irq_vector;

	if(m_vip_regs.INTENB & m_vip_regs.INTPND)
		device_set_input_line(m_maincpu, 4, ASSERT_LINE);

	if((m_vip_regs.INTENB & m_vip_regs.INTPND) == 0)
		device_set_input_line(m_maincpu, 4, CLEAR_LINE);
}

/* TODO: obviously all of this needs clean-ups and better implementation ... */
void vboy_state::m_scanline_tick(int scanline, UINT8 screen_type)
{
	//int frame_num = machine().primary_screen->frame_number();

	if(screen_type == 0)
		m_row_num = (scanline / 8) & 0x1f;

	if(scanline == 0)
	{
		if(m_vip_regs.DPCTRL & 2)
			m_set_irq(0x0010); // FRAME_START

		m_frame_count++;

		m_display_count ++;
		m_display_count &= 3;

		if(m_frame_count > m_vip_regs.FRMCYC)
		{
			m_set_irq(0x0008); // GAME_START
			m_frame_count = 0;
		}

		if(m_vip_regs.XPCTRL & 2)
			m_displayfb ^= 1;
	}

	if(scanline == 224)
		m_set_irq(0x4000); // XPEND

	if(scanline == 232)
		m_set_irq(0x0002); // LFBEND

	if(scanline == 240)
		m_set_irq(0x0004); // RFBEND

	if(scanline == 248)
	{
		//m_set_irq(0x2000); // SBHIT
	}

}

static TIMER_DEVICE_CALLBACK( vboy_scanlineL )
{
	vboy_state *state = timer.machine().driver_data<vboy_state>();
	int scanline = param;

	state->m_scanline_tick(scanline,0);
}

#if 0
static TIMER_DEVICE_CALLBACK( vboy_scanlineR )
{
	vboy_state *state = timer.machine().driver_data<vboy_state>();
	int scanline = param;

	//state->m_scanline_tick(scanline,1);
}
#endif


static const gfx_layout vboy_pcg_8x8 =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 0, 1 },
	{ 7*2, 6*2, 5*2, 4*2, 3*2, 2*2, 1*2, 0*2 },
	{ STEP8(0,8*2) },
	8*8*2
};

/* decoded for debugging purpose, this will be nuked in the end... */
static GFXDECODE_START( vboy )
	GFXDECODE_ENTRY( "pcg",     0x00000, vboy_pcg_8x8,      0, 1 )
GFXDECODE_END


static MACHINE_CONFIG_START( vboy, vboy_state )

	/* basic machine hardware */
	MCFG_CPU_ADD( "maincpu", V810, XTAL_20MHz )
	MCFG_CPU_PROGRAM_MAP(vboy_mem)
	MCFG_CPU_IO_MAP(vboy_io)
	MCFG_TIMER_ADD_SCANLINE("scantimer_l", vboy_scanlineL, "3dleft", 0, 1)
	//MCFG_TIMER_ADD_SCANLINE("scantimer_r", vboy_scanlineR, "3dright", 0, 1)

	MCFG_MACHINE_RESET(vboy)

	MCFG_TIMER_ADD_PERIODIC("timer_100us", timer_100us_tick, attotime::from_usec(100))
	MCFG_TIMER_ADD_PERIODIC("timer_20us", timer_20us_tick, attotime::from_usec(20))

	/* video hardware */
	MCFG_DEFAULT_LAYOUT(layout_vboy)
	MCFG_VIDEO_START(vboy)
	MCFG_PALETTE_LENGTH(4)
	MCFG_PALETTE_INIT(vboy)

	/* Left screen */
	MCFG_SCREEN_ADD("3dleft", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50.2)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500))
	MCFG_SCREEN_SIZE(512, 264) // FIXME: hblank duration
	MCFG_SCREEN_VISIBLE_AREA(0, 384-1, 0, 224-1)
	MCFG_SCREEN_UPDATE_STATIC(vboy_left)

	/* Right screen */
	MCFG_SCREEN_ADD("3dright", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50.2)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500))
	MCFG_SCREEN_SIZE(512, 264) // FIXME: hblank duration
	MCFG_SCREEN_VISIBLE_AREA(0, 384-1, 0, 224-1)
	MCFG_SCREEN_UPDATE_STATIC(vboy_right)

	/* cartridge */
	MCFG_CARTSLOT_ADD("cart")
	MCFG_CARTSLOT_EXTENSION_LIST("vb,bin")
	MCFG_CARTSLOT_MANDATORY
	MCFG_CARTSLOT_INTERFACE("vboy_cart")

	MCFG_GFXDECODE(vboy)

	/* software lists */
	MCFG_SOFTWARE_LIST_ADD("cart_list","vboy")
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( vboy )
	ROM_REGION( 0x200000, "user1", 0 )
	ROM_CART_LOAD("cart", 0x0000, 0x200000, ROM_MIRROR)

	ROM_REGION( 0x8000, "pcg", ROMREGION_ERASE00 )
ROM_END

/* Driver */

/*    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT    INIT    COMPANY     FULLNAME       FLAGS */
CONS( 1995, vboy,   0,      0,       vboy,      vboy, driver_device,    0,    "Nintendo", "Virtual Boy", GAME_NOT_WORKING | GAME_NO_SOUND)
