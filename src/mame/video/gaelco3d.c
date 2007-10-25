/*************************************************************************

    Driver for Gaelco 3D games

    driver by Aaron Giles

**************************************************************************/

#include "driver.h"
#include "gaelco3d.h"
#include "video/polynew.h"

UINT8 *gaelco3d_texture;
UINT8 *gaelco3d_texmask;
UINT32 gaelco3d_texture_size;
UINT32 gaelco3d_texmask_size;


#define MAX_POLYGONS		4096
#define MAX_POLYDATA		(MAX_POLYGONS * 21)

#define BILINEAR_FILTER		1			/* should be 1, but can be set to 0 for speed */

#define DISPLAY_TEXTURE		0
#define LOG_POLYGONS		0
#define DISPLAY_STATS		0

#define IS_POLYEND(x)		(((x) ^ ((x) >> 1)) & 0x4000)


static mame_bitmap *screenbits;
static mame_bitmap *zbuffer;
static UINT16 *palette;
static UINT32 *polydata_buffer;
static UINT32 polydata_count;

static int polygons;
static int lastscan;
static int video_changed;

static poly_manager *poly;


typedef struct _poly_extra_data poly_extra_data;
struct _poly_extra_data
{
	UINT32 tex, color;
	float ooz_dx, ooz_dy, ooz_base;
	float uoz_dx, uoz_dy, uoz_base;
	float voz_dx, voz_dy, voz_base;
	float z0;
};


static void render_noz_noperspective(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid);
static void render_normal(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid);
static void render_alphablend(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid);



/*************************************
 *
 *  Video init
 *
 *************************************/

static void gaelco3d_exit(running_machine *machine)
{
	poly_free(poly);
}


VIDEO_START( gaelco3d )
{
	poly = poly_alloc(2000, sizeof(poly_extra_data), 0);
	add_exit_callback(machine, gaelco3d_exit);

	screenbits = auto_bitmap_alloc(machine->screen[0].width, machine->screen[0].height, machine->screen[0].format);

	zbuffer = auto_bitmap_alloc(machine->screen[0].width, machine->screen[0].height, BITMAP_FORMAT_INDEXED16);

	palette = auto_malloc(32768 * sizeof(palette[0]));
	polydata_buffer = auto_malloc(MAX_POLYDATA * sizeof(polydata_buffer[0]));

	/* save states */

	state_save_register_global_pointer(palette, 32768);
	state_save_register_global_pointer(polydata_buffer, MAX_POLYDATA);
	state_save_register_global(polydata_count);

	state_save_register_global(polygons);
	state_save_register_global(lastscan);

	state_save_register_bitmap("video", 0, "screenbits", screenbits);
	state_save_register_bitmap("video", 0, "zbuffer", zbuffer);
}



/*************************************
 *
 *  TMS32031 floating point conversion
 *
 *************************************/

typedef union int_double
{
	double d;
	float f[2];
	UINT32 i[2];
} int_double;


static float dsp_to_float(UINT32 val)
{
	INT32 _mantissa = val << 8;
	INT8 _exponent = (INT32)val >> 24;
	int_double id;

	if (_mantissa == 0 && _exponent == -128)
		return 0;
	else if (_mantissa >= 0)
	{
		int exponent = (_exponent + 127) << 23;
		id.i[0] = exponent + (_mantissa >> 8);
	}
	else
	{
		int exponent = (_exponent + 127) << 23;
		INT32 man = -_mantissa;
		id.i[0] = 0x80000000 + exponent + ((man >> 8) & 0x00ffffff);
	}
	return id.f[0];
}


/*************************************
 *
 *  Polygon rendering
 *
 *************************************/

/*
    Polygon data stream format:

    data[0]  (float) = scale factor to map Z to fixed Z buffer value
    data[1]  (float) = dvoz/dy (change in v/z per Y)
    data[2]  (float) = dvoz/dx (change in v/z per X)
    data[3]  (float) = dooz/dy (change in 1/z per Y)
    data[4]  (float) = dooz/dx (change in 1/z per X)
    data[5]  (float) = duoz/dy (change in u/z per Y)
    data[6]  (float) = duoz/dx (change in u/z per X)
    data[7]  (float) = voz origin (value of v/z at coordinate (0,0))
    data[8]  (float) = ooz origin (value of 1/z at coordinate (0,0))
    data[9]  (float) = uoz origin (value of u/z at coordinate (0,0))
    data[10] (int)   = palette base (bits 14-8)
    data[11] (int)   = texture address
    data[12] (int)   = start point (bits 15-8 = X coordinate, bits 12-0 = Y coordinate)
    data[13] (int)   = next point (bits 15-8 = X coordinate, bits 12-0 = Y coordinate)
    data[14] (int)   = 16.16 dx/dy from previous point to this point
    (repeat these two for each additional point in the fan)
*/

static void render_poly(UINT32 *polydata)
{
	float midx = Machine->screen[0].width / 2;
	float midy = Machine->screen[0].height / 2;
	float z0 = dsp_to_float(polydata[0]);
	float voz_dy = dsp_to_float(polydata[1]);
	float voz_dx = dsp_to_float(polydata[2]);
	float ooz_dy = dsp_to_float(polydata[3]);
	float ooz_dx = dsp_to_float(polydata[4]);
	float uoz_dy = dsp_to_float(polydata[5]);
	float uoz_dx = dsp_to_float(polydata[6]);
	float voz_base = dsp_to_float(polydata[7]) - midx * voz_dx - midy * voz_dy;
	float ooz_base = dsp_to_float(polydata[8]) - midx * ooz_dx - midy * ooz_dy;
	float uoz_base = dsp_to_float(polydata[9]) - midx * uoz_dx - midy * uoz_dy;
	int color = (polydata[10] & 0x7f) << 8;
	poly_vertex vert[3];
	int i;

	if (LOG_POLYGONS)
	{
		int t;
		logerror("poly: %12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %12.2f %08X %08X (%4d,%4d) %08X",
				(double)dsp_to_float(polydata[0]),
				(double)dsp_to_float(polydata[1]),
				(double)dsp_to_float(polydata[2]),
				(double)dsp_to_float(polydata[3]),
				(double)dsp_to_float(polydata[4]),
				(double)dsp_to_float(polydata[5]),
				(double)dsp_to_float(polydata[6]),
				(double)dsp_to_float(polydata[7]),
				(double)dsp_to_float(polydata[8]),
				(double)dsp_to_float(polydata[9]),
				polydata[10],
				polydata[11],
				(INT16)(polydata[12] >> 16), (INT16)(polydata[12] << 2) >> 2, polydata[12]);

		logerror(" (%4d,%4d) %08X %08X", (INT16)(polydata[13] >> 16), (INT16)(polydata[13] << 2) >> 2, polydata[13], polydata[14]);
		for (t = 15; !IS_POLYEND(polydata[t - 2]); t += 2)
			logerror(" (%4d,%4d) %08X %08X", (INT16)(polydata[t] >> 16), (INT16)(polydata[t] << 2) >> 2, polydata[t], polydata[t+1]);
		logerror("\n");
	}

	/* extract the first two vertices */
	vert[0].x = midx + (float)((INT32)polydata[13] >> 16) + 0.5f;
	vert[0].y = midy + ((float)((INT32)(polydata[13] << 18) >> 18) + 0.5f);
	vert[1].x = midx + (float)((INT32)polydata[15] >> 16) + 0.5f;
	vert[1].y = midy + ((float)((INT32)(polydata[15] << 18) >> 18) + 0.5f);

	/* loop over the remaining verticies */
	for (i = 17; !IS_POLYEND(polydata[i - 2]) && i < 1000; i += 2)
	{
		poly_extra_data *extra = poly_get_extra_data(poly);

		/* fill in core extra data */
		extra->tex = polydata[11];
		extra->color = color;
		extra->ooz_dx = ooz_dx;
		extra->ooz_dy = ooz_dy;
		extra->ooz_base = ooz_base;
		extra->uoz_dx = uoz_dx;
		extra->uoz_dy = uoz_dy;
		extra->uoz_base = uoz_base;
		extra->voz_dx = voz_dx;
		extra->voz_dy = voz_dy;
		extra->voz_base = voz_base;
		extra->z0 = z0;

		/* extract vertex 2 */
		vert[2].x = midx + (float)((INT32)polydata[i] >> 16) + 0.5f;
		vert[2].y = midy + (float)((INT32)(polydata[i] << 18) >> 18) + 0.5f;

		/* special case: no Z buffering and no perspective correction */
		if (color != 0x7f00 && z0 < 0 && ooz_dx == 0 && ooz_dy == 0)
			poly_render_triangle(poly, screenbits, &Machine->screen[0].visarea, render_noz_noperspective, 0, &vert[0], &vert[1], &vert[2]);

		/* general case: non-alpha blended */
		else if (color != 0x7f00)
			poly_render_triangle(poly, screenbits, &Machine->screen[0].visarea, render_normal, 0, &vert[0], &vert[1], &vert[2]);

		/* color 0x7f seems to be hard-coded as a 50% alpha blend */
		else
			poly_render_triangle(poly, screenbits, &Machine->screen[0].visarea, render_alphablend, 0, &vert[0], &vert[1], &vert[2]);

		/* copy vertex 2 to vertex 1 -- this hardware draws in fans */
		vert[1] = vert[2];
		polygons++;
	}
}



static void render_noz_noperspective(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid)
{
	poly_extra_data *extra = poly->extra;
	mame_bitmap *bitmap = destbase;
	UINT16 *dest = BITMAP_ADDR16(bitmap, scanline, 0);
	UINT16 *zbuf = BITMAP_ADDR16(zbuffer, scanline, 0);
	float zbase = 1.0f / extra->ooz_base;
	float uoz = (extra->uoz_base + scanline * extra->uoz_dy + startx * extra->uoz_dx) * zbase;
	float voz = (extra->voz_base + scanline * extra->voz_dy + startx * extra->voz_dx) * zbase;
	float uoz_step = extra->uoz_dx * zbase;
	float voz_step = extra->voz_dx * zbase;
	int zbufval = (int)(-extra->z0 * zbase);
	offs_t endmask = gaelco3d_texture_size - 1;
	UINT16 color = extra->color;
	UINT32 tex = extra->tex;
	int x;

	for (x = startx; x < stopx; x++)
	{
		int pixeloffs, u, v;

		if (!BILINEAR_FILTER)
		{
			u = (int)(uoz + 0.5f); v = (int)(voz + 0.5f);
			pixeloffs = (tex + v * 4096 + u) & endmask;
			if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
			{
				dest[x] = palette[color | gaelco3d_texture[pixeloffs]];
				zbuf[x] = zbufval;
			}
		}
		else
		{
			u = (int)(uoz * 256.0f); v = (int)(voz * 256.0f);
			pixeloffs = (tex + (v >> 8) * 4096 + (u >> 8)) & endmask;
			if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
			{
				int paldata, r, g, b, f, tf;

				paldata = palette[color | gaelco3d_texture[pixeloffs]];
				tf = f = (~u & 0xff) * (~v & 0xff);
				r = (paldata & 0x7c00) * f; g = (paldata & 0x03e0) * f; b = (paldata & 0x001f) * f;

				paldata = palette[color | gaelco3d_texture[(pixeloffs + 1) & endmask]];
				tf += f = (u & 0xff) * (~v & 0xff);
				r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

				paldata = palette[color | gaelco3d_texture[(pixeloffs + 4096) & endmask]];
				tf += f = (~u & 0xff) * (v & 0xff);
				r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

				paldata = palette[color | gaelco3d_texture[(pixeloffs + 4097) & endmask]];
				f = 0x10000 - tf;
				r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

				dest[x] = ((r >> 16) & 0x7c00) | ((g >> 16) & 0x03e0) | (b >> 16);
				zbuf[x] = zbufval;
			}
		}

		/* advance texture params to the next pixel */
		uoz += uoz_step;
		voz += voz_step;
	}
}


static void render_normal(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid)
{
	poly_extra_data *extra = poly->extra;
	mame_bitmap *bitmap = destbase;
	UINT16 *dest = BITMAP_ADDR16(bitmap, scanline, 0);
	UINT16 *zbuf = BITMAP_ADDR16(zbuffer, scanline, 0);
	float ooz_dx = extra->ooz_dx;
	float uoz_dx = extra->uoz_dx;
	float voz_dx = extra->voz_dx;
	float ooz = extra->ooz_base + scanline * extra->ooz_dy + startx * ooz_dx;
	float uoz = extra->uoz_base + scanline * extra->uoz_dy + startx * uoz_dx;
	float voz = extra->voz_base + scanline * extra->voz_dy + startx * voz_dx;
	offs_t endmask = gaelco3d_texture_size - 1;
	UINT16 color = extra->color;
	UINT32 tex = extra->tex;
	float z0 = extra->z0;
	int x;

	for (x = startx; x < stopx; x++)
	{
		if (ooz > 0)
		{
			/* compute Z and check the Z buffer value first */
			float z = 1.0f / ooz;
			int zbufval = (int)(z0 * z);
			if (zbufval < zbuf[x] || zbufval < 0)
			{
				int pixeloffs, u, v;

				if (!BILINEAR_FILTER)
				{
					u = (int)(uoz * z + 0.5f); v = (int)(voz * z + 0.5f);
					pixeloffs = (tex + v * 4096 + u) & endmask;
					if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
					{
						dest[x] = palette[color | gaelco3d_texture[pixeloffs]];
						zbuf[x] = (zbufval < 0) ? -zbufval : zbufval;
					}
				}
				else
				{
					u = (int)(uoz * z * 256.0f); v = (int)(voz * z * 256.0f);
					pixeloffs = (tex + (v >> 8) * 4096 + (u >> 8)) & endmask;
					if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
					{
						int paldata, r, g, b, f, tf;

						paldata = palette[color | gaelco3d_texture[pixeloffs]];
						tf = f = (~u & 0xff) * (~v & 0xff);
						r = (paldata & 0x7c00) * f; g = (paldata & 0x03e0) * f; b = (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 1) & endmask]];
						tf += f = (u & 0xff) * (~v & 0xff);
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 4096) & endmask]];
						tf += f = (~u & 0xff) * (v & 0xff);
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 4097) & endmask]];
						f = 0x10000 - tf;
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						dest[x] = ((r >> 16) & 0x7c00) | ((g >> 16) & 0x03e0) | (b >> 16);
						zbuf[x] = (zbufval < 0) ? -zbufval : zbufval;
					}
				}
			}
		}

		/* advance texture params to the next pixel */
		ooz += ooz_dx;
		uoz += uoz_dx;
		voz += voz_dx;
	}
}


static void render_alphablend(void *destbase, INT32 scanline, INT32 startx, INT32 stopx, const poly_params *poly, int threadid)
{
	poly_extra_data *extra = poly->extra;
	mame_bitmap *bitmap = destbase;
	UINT16 *dest = BITMAP_ADDR16(bitmap, scanline, 0);
	UINT16 *zbuf = BITMAP_ADDR16(zbuffer, scanline, 0);
	float ooz_dx = extra->ooz_dx;
	float uoz_dx = extra->uoz_dx;
	float voz_dx = extra->voz_dx;
	float ooz = extra->ooz_base + extra->ooz_dy * scanline + startx * ooz_dx;
	float uoz = extra->uoz_base + extra->uoz_dy * scanline + startx * uoz_dx;
	float voz = extra->voz_base + extra->voz_dy * scanline + startx * voz_dx;
	offs_t endmask = gaelco3d_texture_size - 1;
	UINT16 color = extra->color;
	UINT32 tex = extra->tex;
	float z0 = extra->z0;
	int x;

	for (x = startx; x < stopx; x++)
	{
		if (ooz > 0)
		{
			/* compute Z and check the Z buffer value first */
			float z = 1.0f / ooz;
			int zbufval = (int)(z0 * z);
			if (zbufval < zbuf[x] || zbufval < 0)
			{
				int pixeloffs, u, v;

				if (!BILINEAR_FILTER)
				{
					u = (int)(uoz * z + 0.5f); v = (int)(voz * z + 0.5f);
					pixeloffs = (tex + v * 4096 + u) & endmask;
					if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
					{
						dest[x] = ((dest[x] >> 1) & 0x3def) + ((palette[color | gaelco3d_texture[pixeloffs]] >> 1) & 0x3def);
						zbuf[x] = (zbufval < 0) ? -zbufval : zbufval;
					}
				}
				else
				{
					u = (int)(uoz * z * 256.0f); v = (int)(voz * z * 256.0f);
					pixeloffs = (tex + (v >> 8) * 4096 + (u >> 8)) & endmask;
					if (pixeloffs >= gaelco3d_texmask_size || !gaelco3d_texmask[pixeloffs])
					{
						int paldata, r, g, b, f, tf;

						paldata = palette[color | gaelco3d_texture[pixeloffs]];
						tf = f = (~u & 0xff) * (~v & 0xff);
						r = (paldata & 0x7c00) * f; g = (paldata & 0x03e0) * f; b = (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 1) & endmask]];
						tf += f = (u & 0xff) * (~v & 0xff);
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 4096) & endmask]];
						tf += f = (~u & 0xff) * (v & 0xff);
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						paldata = palette[color | gaelco3d_texture[(pixeloffs + 4097) & endmask]];
						f = 0x10000 - tf;
						r += (paldata & 0x7c00) * f; g += (paldata & 0x03e0) * f; b += (paldata & 0x001f) * f;

						paldata = ((r >> 17) & 0x7c00) | ((g >> 17) & 0x03e0) | (b >> 17);
						dest[x] = ((dest[x] >> 1) & 0x3def) + (paldata & 0x3def);
						zbuf[x] = (zbufval < 0) ? -zbufval : zbufval;
					}
				}
			}
		}

		/* advance texture params to the next pixel */
		ooz += ooz_dx;
		uoz += uoz_dx;
		voz += voz_dx;
	}
}


/*************************************
 *
 *  Scene rendering
 *
 *************************************/

void gaelco3d_render(void)
{
	/* wait for any queued stuff to complete */
	poly_wait(poly, "Time to render");

#if DISPLAY_STATS
{
	int scan = video_screen_get_vpos(0);
	popmessage("Polys = %4d  Timeleft = %3d", polygons, (lastscan < scan) ? (scan - lastscan) : (scan + (lastscan - Machine->screen[0].visarea.max_y)));
}
#endif

	polydata_count = 0;
	polygons = 0;
	lastscan = -1;
}



/*************************************
 *
 *  Renderer access
 *
 *************************************/

WRITE32_HANDLER( gaelco3d_render_w )
{
	/* append the data to our buffer */
	polydata_buffer[polydata_count++] = data;
	if (polydata_count >= MAX_POLYDATA)
		fatalerror("Out of polygon buffer space!");

	/* if we've accumulated a completed poly set of data, queue it */
	if (!video_skip_this_frame())
	{
		if (polydata_count >= 18 && (polydata_count % 2) == 1 && IS_POLYEND(polydata_buffer[polydata_count - 2]))
		{
			render_poly(&polydata_buffer[0]);
			polydata_count = 0;
		}
		video_changed = TRUE;
	}

#if DISPLAY_STATS
	lastscan = video_screen_get_vpos(0);
#endif
}



/*************************************
 *
 *  Palette access
 *
 *************************************/

WRITE16_HANDLER( gaelco3d_paletteram_w )
{
	poly_wait(poly, "Palette change");
	COMBINE_DATA(&paletteram16[offset]);
	palette[offset] = paletteram16[offset];
}


WRITE32_HANDLER( gaelco3d_paletteram_020_w )
{
	poly_wait(poly, "Palette change");
	COMBINE_DATA(&paletteram32[offset]);
	palette[offset*2+0] = paletteram32[offset] >> 16;
	palette[offset*2+1] = paletteram32[offset];
}



/*************************************
 *
 *  Video update
 *
 *************************************/

VIDEO_UPDATE( gaelco3d )
{
	int x, y, ret;

	if (DISPLAY_TEXTURE && (input_code_pressed(KEYCODE_Z) || input_code_pressed(KEYCODE_X)))
	{
		static int xv = 0, yv = 0x1000;
		UINT8 *base = gaelco3d_texture;
		int length = gaelco3d_texture_size;

		if (input_code_pressed(KEYCODE_X))
		{
			base = gaelco3d_texmask;
			length = gaelco3d_texmask_size;
		}

		if (input_code_pressed(KEYCODE_LEFT) && xv >= 4)
			xv -= 4;
		if (input_code_pressed(KEYCODE_RIGHT) && xv < 4096 - 4)
			xv += 4;

		if (input_code_pressed(KEYCODE_UP) && yv >= 4)
			yv -= 4;
		if (input_code_pressed(KEYCODE_DOWN) && yv < 0x40000)
			yv += 4;

		for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		{
			UINT16 *dest = BITMAP_ADDR16(bitmap, y, 0);
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
			{
				int offs = (yv + y - cliprect->min_y) * 4096 + xv + x - cliprect->min_x;
				if (offs < length)
					dest[x] = base[offs];
				else
					dest[x] = 0;
			}
		}
		popmessage("(%04X,%04X)", xv, yv);
	}
	else
	{
		if (video_changed)
			copybitmap(bitmap, screenbits, 0,1, 0,0, cliprect, TRANSPARENCY_NONE, 0);
		ret = video_changed;
		video_changed = FALSE;
	}

	logerror("---update---\n");
	return (!ret);
}
