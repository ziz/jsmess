/***************************************************************************

		Specialist video driver by Miodrag Milanovic

		15/03/2008 Preliminary driver.
		     
****************************************************************************/


#include "driver.h"
  
UINT8 *specialist_video_ram;

VIDEO_START( special )
{
}
 
VIDEO_UPDATE( special )
{
  UINT8 code;
	int y, x, b;
	
	for (x = 0; x < 48; x++)
	{			
		for (y = 0; y < 256; y++)
		{
			code = specialist_video_ram[y + x*256];
			for (b = 7; b >= 0; b--)
			{								
				*BITMAP_ADDR16(bitmap, y, x*8+(7-b)) =  (code >> b) & 0x01;
			}
		}				
	}
	return 0;
}


const rgb_t specimx_palette[16] = {
	MAKE_RGB(0x00, 0x00, 0x00), // 0
	MAKE_RGB(0x00, 0x00, 0xaa), // 1
	MAKE_RGB(0x00, 0xaa, 0x00), // 2 
	MAKE_RGB(0x00, 0xaa, 0xaa), // 3  
	MAKE_RGB(0xaa, 0x00, 0x00), // 4  
	MAKE_RGB(0xaa, 0x00, 0xaa), // 5  
	MAKE_RGB(0xaa, 0x55, 0x00), // 6  
	MAKE_RGB(0xaa, 0xaa, 0xaa), // 7  
	MAKE_RGB(0x55, 0x55, 0x55), // 8  
	MAKE_RGB(0x55, 0x55, 0xff), // 9  
	MAKE_RGB(0x55, 0xff, 0x55), // A  
	MAKE_RGB(0x55, 0xff, 0xff), // B  
	MAKE_RGB(0xff, 0x55, 0x55), // C  
	MAKE_RGB(0xff, 0x55, 0xff), // D  
	MAKE_RGB(0xff, 0xff, 0x55), // E  
	MAKE_RGB(0xff, 0xff, 0xff)	// F  
};

PALETTE_INIT( specimx )
{
	palette_set_colors(machine, 0, specimx_palette, ARRAY_LENGTH(specimx_palette));
}


extern UINT8 *specimx_colorram;

VIDEO_START( specimx )
{	
	specimx_colorram = auto_malloc (0x3000);
	memset(specimx_colorram,0x70,0x3000);
} 

VIDEO_UPDATE( specimx )
{
  	UINT8 code,color;
	int y, x, b;
		
	for (x = 0; x < 48; x++)
	{			
		for (y = 0; y < 256; y++)
		{
			code = mess_ram[0x9000 + y + x*256];
			color = specimx_colorram[y + x*256];
			for (b = 7; b >= 0; b--)
			{								
				
				*BITMAP_ADDR16(bitmap, y, x*8+(7-b)) =  ((code >> b) & 0x01)==0 ? color & 0x0f : (color >> 4)& 0x0f ;
			}
		}				
	}
	return 0;
}

