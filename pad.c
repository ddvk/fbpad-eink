#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "draw.h"
#include "fbpad.h"

#include <stdint.h>
#include "refresh.h"

static int rows, cols;
static int fnrows, fncols;
static struct font *fonts[3];

static int invalid_nonempty = 0;
// invalid region is measured in character cells and does NOT include
// the right or bottom edge
static int invalid_top = 0, invalid_left = 0, invalid_right = 0, invalid_bottom = 0;

int pad_init(void)
{
	if (pad_font(FR, FI, FB))
		return 1;
	rows = fb_rows() / fnrows;
	cols = fb_cols() / fncols;
	return 0;
}

void pad_free(void)
{
	int i;
	for (i = 0; i < 3; i++)
		if (fonts[i])
			font_free(fonts[i]);
}

#define CR(a)		(((a) >> 16) & 0x0000ff)
#define CG(a)		(((a) >> 8) & 0x0000ff)
#define CB(a)		((a) & 0x0000ff)
#define COLORMERGE(f, b, c)		((b) + (((f) - (b)) * (c) >> 8u))

static unsigned mixed_color(int fg, int bg, unsigned val)
{
	unsigned char r = COLORMERGE(CR(fg), CR(bg), val);
	unsigned char g = COLORMERGE(CG(fg), CG(bg), val);
	unsigned char b = COLORMERGE(CB(fg), CB(bg), val);
	return FB_VAL(r, g, b);
}

static unsigned color2fb(int c)
{
	return FB_VAL(CR(c), CG(c), CB(c));
}

/* glyph bitmap cache */
#define GCLCNT		(1 << 7)	/* glyph cache list count */
#define GCLLEN		(1 << 4)	/* glyph cache list length */
#define GCIDX(c)	((c) & (GCLCNT - 1))

static fbval_t gc_mem[GCLCNT][GCLLEN][NDOTS];
static int gc_next[GCLCNT];
static struct glyph {
	int c;
	int fg, bg;
} gc_info[GCLCNT][GCLLEN];

static fbval_t *gc_get(int c, int fg, int bg)
{
	struct glyph *g = gc_info[GCIDX(c)];
	int i;
	for (i = 0; i < GCLLEN; i++)
		if (g[i].c == c && g[i].fg == fg && g[i].bg == bg)
			return gc_mem[GCIDX(c)][i];
	return NULL;
}

static fbval_t *gc_put(int c, int fg, int bg)
{
	int idx = GCIDX(c);
	int pos = gc_next[idx]++;
	struct glyph *g = &gc_info[idx][pos];
	if (gc_next[idx] >= GCLLEN)
		gc_next[idx] = 0;
	g->c = c;
	g->fg = fg;
	g->bg = bg;
	return gc_mem[idx][pos];
}

static void bmp2fb(fbval_t *d, char *s, int fg, int bg, int nr, int nc)
{
	int i, j;
	for (i = 0; i < fnrows; i++) {
		for (j = 0; j < fncols; j++) {
			unsigned v = i < nr && j < nc ?
				(unsigned char) s[i * nc + j] : 0;
			d[i * fncols + j] = mixed_color(fg, bg, v);
		}
	}
}

static fbval_t *ch2fb(int fn, int c, int fg, int bg)
{
	char bits[NDOTS];
	fbval_t *fbbits;
	if (c < 0 || (c < 128 && (!isprint(c) || isspace(c))))
		return NULL;
	if ((fbbits = gc_get(c, fg, bg)))
		return fbbits;
	if (font_bitmap(fonts[fn], bits, c))
		return NULL;
	fbbits = gc_put(c, fg, bg);
	bmp2fb(fbbits, bits, fg & FN_C, bg & FN_C,
		font_rows(fonts[fn]), font_cols(fonts[fn]));
	return fbbits;
}

static void fb_set(int r, int c, void *mem, int len)
{
	int bpp = FBM_BPP(fb_mode());
	memcpy(fb_mem(r) + c * bpp, mem, len * bpp);
}

static void fb_box(int sr, int er, int sc, int ec, fbval_t val)
{
	static fbval_t line[32 * NCOLS];
	int i;
	for (i = sc; i < ec; i++)
		line[i - sc] = val;
	for (i = sr; i < er; i++)
		fb_set(i, sc, line, ec - sc);
}

static int fnsel(int fg, int bg)
{
	if ((fg | bg) & FN_B)
		return fonts[2] ? 2 : 0;
	if ((fg | bg) & FN_I)
		return fonts[1] ? 1 : 0;
	return 0;
}

void pad_put(int ch, int r, int c, int fg, int bg)
{
	if (invalid_nonempty) {
          if (c < invalid_left) invalid_left = c;
          if (c >= invalid_right) invalid_right = c + 1;
          if (r < invalid_top) invalid_top = r;
          if (r >= invalid_bottom) invalid_bottom = r + 1;
        } else {
          invalid_nonempty = 1;
          invalid_left = c;
          invalid_right = c+1;
          invalid_top = r;
          invalid_bottom = r+1;
        }

	int sr = fnrows * r;
	int sc = fncols * c;
	fbval_t *bits;
	int i;
	bits = ch2fb(fnsel(fg, bg), ch, fg, bg);
	if (!bits)
		bits = ch2fb(0, ch, fg, bg);
	if (!bits)
		fb_box(sr, sr + fnrows, sc, sc + fncols, color2fb(bg & FN_C));
	else
		for (i = 0; i < fnrows; i++)
			fb_set(sr + i, sc, bits + (i * fncols), fncols);
}

void pad_fill(int sr, int er, int sc, int ec, int c)
{
	int fber = er >= 0 ? er * fnrows : fb_rows();
	int fbec = ec >= 0 ? ec * fncols : fb_cols();
	fb_box(sr * fnrows, fber, sc * fncols, fbec, color2fb(c & FN_C));

        if (invalid_nonempty) {
          if (sc < invalid_left) invalid_left = sc;
          if (ec >= invalid_right) invalid_right = ec + 1;
          if (sr < invalid_top) invalid_top = sr;
          if (er >= invalid_bottom) invalid_bottom = er + 1;
        } else {
          invalid_nonempty = 1;
          invalid_left = sc;
          invalid_right = ec + 1;
          invalid_top = sr;
          invalid_bottom = er + 1;
        }
}

int pad_refresh(void) {
  printf( "refresh: (%d,%d)+(%d,%d)\n", invalid_left, invalid_top, invalid_right - invalid_left, invalid_bottom - invalid_top );

  fbink_refresh( fb_fd(),
                 invalid_top * fnrows,
                 invalid_left * fncols,
                 (invalid_right - invalid_left) * fncols,
                 (invalid_bottom - invalid_top) * fnrows,
                 0 );

  invalid_nonempty = 0;
  invalid_top = invalid_bottom = invalid_left = invalid_right = 0;
}

int pad_rows(void)
{
	return rows;
}

int pad_cols(void)
{
	return cols;
}

int pad_font(char *fr, char *fi, char *fb)
{
	struct font *r = fr ? font_open(fr) : NULL;
	if (!r)
		return 1;
	fonts[0] = r;
	fonts[1] = fi ? font_open(fi) : NULL;
	fonts[2] = fb ? font_open(fb) : NULL;
	memset(gc_info, 0, sizeof(gc_info));
	fnrows = font_rows(fonts[0]);
	fncols = font_cols(fonts[0]);
	return 0;
}

