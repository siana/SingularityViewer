/** 
 * @file llfontgl.cpp
 * @brief Wrapper around FreeType
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llfontgl.h"

// Linden library includes
#include "llfasttimer.h"
#include "llfontfreetype.h"
#include "llfontbitmapcache.h"
#include "llfontregistry.h"
#include "llgl.h"
#include "llrender.h"
#include "llstl.h"
#include "v4color.h"
#include "lltexture.h"
#include "lldir.h"

// Third party library includes
#include <boost/tokenizer.hpp>

#if LL_WINDOWS
#include "llwin32headerslean.h"
#include <shlobj.h>
#endif

const S32 BOLD_OFFSET = 1;

// static class members
F32 LLFontGL::sVertDPI = 96.f;
F32 LLFontGL::sHorizDPI = 96.f;
F32 LLFontGL::sScaleX = 1.f;
F32 LLFontGL::sScaleY = 1.f;
BOOL LLFontGL::sDisplayFont = TRUE ;
std::string LLFontGL::sFontDir;

LLColor4U LLFontGL::sShadowColor(0, 0, 0, 255);
LLFontRegistry* LLFontGL::sFontRegistry = NULL;

LLCoordGL LLFontGL::sCurOrigin;
F32 LLFontGL::sCurDepth;
std::vector<std::pair<LLCoordGL, F32> > LLFontGL::sOriginStack;

const F32 EXT_X_BEARING = 1.f;
const F32 EXT_Y_BEARING = 0.f;
const F32 EXT_KERNING = 1.f;
const F32 PIXEL_BORDER_THRESHOLD = 0.0001f;
const F32 PIXEL_CORRECTION_DISTANCE = 0.01f;

const F32 PAD_UVY = 0.5f; // half of vertical padding between glyphs in the glyph texture
const F32 DROP_SHADOW_SOFT_STRENGTH = 0.3f;

const U32 GLYPH_VERTICES = 6;

LLFontGL::LLFontGL()
{
	clearEmbeddedChars();
}

LLFontGL::~LLFontGL()
{
	clearEmbeddedChars();
}

void LLFontGL::reset()
{
	mFontFreetype->reset(sVertDPI, sHorizDPI);
}

void LLFontGL::destroyGL()
{
	mFontFreetype->destroyGL();
}

BOOL LLFontGL::loadFace(const std::string& filename, const F32 point_size, const F32 vert_dpi, const F32 horz_dpi, const S32 components, BOOL is_fallback)
{
	if(mFontFreetype == reinterpret_cast<LLFontFreetype*>(NULL))
	{
		mFontFreetype = new LLFontFreetype;
	}

	return mFontFreetype->loadFace(filename, point_size, vert_dpi, horz_dpi, components, is_fallback);
}

static LLTrace::BlockTimerStatHandle FTM_RENDER_FONTS("Fonts");

S32 LLFontGL::render(const LLWString &wstr, S32 begin_offset, const LLRect& rect, const LLColor4 &color, HAlign halign, VAlign valign, U8 style, 
					 ShadowType shadow, S32 max_chars, F32* right_x, BOOL use_embedded, BOOL use_ellipses) const
{
	F32 x = rect.mLeft;
	F32 y = 0.f;

	switch(valign)
	{
	case TOP:
		y = rect.mTop;
		break;
	case VCENTER:
		y = rect.getCenterY();
		break;
	case BASELINE:
	case BOTTOM:
		y = rect.mBottom;
		break;
	default:
		y = rect.mBottom;
		break;
	}
	return render(wstr, begin_offset, x, y, color, halign, valign, style, shadow, max_chars, rect.getWidth(), right_x, use_embedded, use_ellipses);
}

S32 LLFontGL::render(const LLWString &wstr, S32 begin_offset, F32 x, F32 y, const LLColor4 &color, HAlign halign, VAlign valign, U8 style, 
					 ShadowType shadow, S32 max_chars, S32 max_pixels, F32* right_x, BOOL use_embedded, BOOL use_ellipses) const
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_FONTS);

	if(!sDisplayFont) //do not display texts
	{
		return wstr.length() ;
	}

	if (wstr.empty() || !max_pixels)
	{
		return 0;
	} 

	if (max_chars == -1)
		max_chars = S32_MAX;

	const S32 max_index = llmin(llmax(max_chars, begin_offset + max_chars), S32(wstr.length()));
	if (max_index <= 0 || begin_offset >= max_index || max_pixels <= 0)
		return 0;

	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);

	S32 scaled_max_pixels = max_pixels == S32_MAX ? S32_MAX : llceil((F32)max_pixels * sScaleX);

	// determine which style flags need to be added programmatically by stripping off the
	// style bits that are drawn by the underlying Freetype font
	U8 style_to_add = (style | mFontDescriptor.getStyle()) & ~mFontFreetype->getStyle();

	F32 drop_shadow_strength = 0.f;
	if (shadow != NO_SHADOW)
	{
		F32 luminance;
		color.calcHSL(NULL, NULL, &luminance);
		drop_shadow_strength = clamp_rescale(luminance, 0.35f, 0.6f, 0.f, 1.f);
		if (luminance < 0.35f)
		{
			shadow = NO_SHADOW;
		}
	}

	gGL.pushUIMatrix();

	gGL.loadUIIdentity();
	
	LLVector2 origin(floorf(sCurOrigin.mX*sScaleX), floorf(sCurOrigin.mY*sScaleY));

	// Depth translation, so that floating text appears 'in-world'
	// and is correctly occluded.
	gGL.translatef(0.f,0.f,sCurDepth);

	S32 chars_drawn = 0;
	S32 i;
	S32 length = max_index - begin_offset;

	F32 cur_x, cur_y, cur_render_x, cur_render_y;

 	// Not guaranteed to be set correctly
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	
	cur_x = ((F32)x * sScaleX) + origin.mV[VX];
	cur_y = ((F32)y * sScaleY) + origin.mV[VY];

	// Offset y by vertical alignment.
	// use unscaled font metrics here
	switch (valign)
	{
	case TOP:
		cur_y -= llceil(mFontFreetype->getAscenderHeight());
		break;
	case BOTTOM:
		cur_y += llceil(mFontFreetype->getDescenderHeight());
		break;
	case VCENTER:
		cur_y -= llceil((llceil(mFontFreetype->getAscenderHeight()) - llceil(mFontFreetype->getDescenderHeight())) / 2.f);
		break;
	case BASELINE:
		// Baseline, do nothing.
		break;
	default:
		break;
	}

	switch (halign)
	{
	case LEFT:
		break;
	case RIGHT:
	  	cur_x -= llmin(scaled_max_pixels, ll_pos_round(getWidthF32(wstr.c_str(), begin_offset, length) * sScaleX));
		break;
	case HCENTER:
	    cur_x -= llmin(scaled_max_pixels, ll_pos_round(getWidthF32(wstr.c_str(), begin_offset, length) * sScaleX)) / 2;
		break;
	default:
		break;
	}

	cur_render_y = cur_y;
	cur_render_x = cur_x;

	F32 start_x = (F32)ll_round(cur_x);

	const LLFontBitmapCache* font_bitmap_cache = mFontFreetype->getFontBitmapCache();

	F32 inv_width = 1.f / font_bitmap_cache->getBitmapWidth();
	F32 inv_height = 1.f / font_bitmap_cache->getBitmapHeight();

	const S32 LAST_CHARACTER = LLFontFreetype::LAST_CHAR_FULL;


	BOOL draw_ellipses = FALSE;
	if (use_ellipses && halign == LEFT)
	{
		// check for too long of a string
		S32 string_width = ll_pos_round(getWidthF32(wstr, begin_offset, max_chars) * sScaleX);
		if (string_width > scaled_max_pixels)
		{
			// use four dots for ellipsis width to generate padding
			const LLWString dots(utf8str_to_wstring(std::string("....")));
			scaled_max_pixels = llmax(0, scaled_max_pixels - ll_pos_round(getWidthF32(dots.c_str())));
			draw_ellipses = TRUE;
		}
	}

	const LLFontGlyphInfo* next_glyph = NULL;

	const S32 GLYPH_BATCH_SIZE = 30;
	static LL_ALIGN_16(LLVector4a vertices[GLYPH_BATCH_SIZE * GLYPH_VERTICES]);
	static LLVector2 uvs[GLYPH_BATCH_SIZE * GLYPH_VERTICES];
	static LLColor4U colors[GLYPH_BATCH_SIZE * GLYPH_VERTICES];

	LLColor4U text_color(color);

	S32 bitmap_num = -1;
	S32 glyph_count = 0;
	for (i = begin_offset; i < begin_offset + length; i++)
	{
		llwchar wch = wstr[i];

		// Handle embedded characters first, if they're enabled.
		// Embedded characters are a hack for notecards
		const embedded_data_t* ext_data = use_embedded ? getEmbeddedCharData(wch) : NULL;
		if (ext_data)
		{
			LLImageGL* ext_image = ext_data->mImage;
			const LLWString& label = ext_data->mLabel;

			F32 ext_height = (F32)ext_image->getHeight() * sScaleY;

			F32 ext_width = (F32)ext_image->getWidth() * sScaleX;
			F32 ext_advance = (EXT_X_BEARING * sScaleX) + ext_width;

			if (!label.empty())
			{
				ext_advance += (EXT_X_BEARING + getFontExtChar()->getWidthF32( label.c_str() )) * sScaleX;
			}

			if (start_x + scaled_max_pixels < cur_x + ext_advance)
			{
				// Not enough room for this character.
				break;
			}

			gGL.getTexUnit(0)->bind(ext_image);

			// snap origin to whole screen pixel
			const F32 ext_x = (F32)ll_round(cur_render_x + (EXT_X_BEARING * sScaleX));
			const F32 ext_y = (F32)ll_round(cur_render_y + (EXT_Y_BEARING * sScaleY + mFontFreetype->getAscenderHeight() - mFontFreetype->getLineHeight()));

			LLRectf uv_rect(0.f, 1.f, 1.f, 0.f);
			LLRectf screen_rect(ext_x, ext_y + ext_height, ext_x + ext_width, ext_y);

			if (glyph_count > 0)
			{
				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.vertexBatchPreTransformed(vertices, uvs, colors, glyph_count * GLYPH_VERTICES);
				}
				gGL.end();
				glyph_count = 0;
			}
			renderQuad(vertices, uvs, colors, screen_rect, uv_rect, LLColor4U::white, 0);
			//No batching here. It will never happen.
			gGL.begin(LLRender::TRIANGLES);
			{
				gGL.vertexBatchPreTransformed(vertices, uvs, colors, GLYPH_VERTICES);
			}
			gGL.end();

			if (!label.empty())
			{
				gGL.pushMatrix();
				getFontExtChar()->render(label, 0,
									 /*llfloor*/(ext_x / sScaleX) + ext_image->getWidth() + EXT_X_BEARING - sCurOrigin.mX, 
									 /*llfloor*/(cur_render_y / sScaleY) - sCurOrigin.mY,
									 color,
									 halign, BASELINE, UNDERLINE, NO_SHADOW, S32_MAX, S32_MAX, NULL,
									 TRUE );
				gGL.popMatrix();
			}

			chars_drawn++;
			cur_x += ext_advance;
			if (((i + 1) < length) && wstr[i+1])
			{
				cur_x += EXT_KERNING * sScaleX;
			}
			cur_render_x = cur_x;
		}
		else
		{
			const LLFontGlyphInfo* fgi = next_glyph;
			next_glyph = NULL;
			if(!fgi)
			{
				fgi = mFontFreetype->getGlyphInfo(wch);
			}
			if (!fgi)
			{
				LL_ERRS() << "Missing Glyph Info" << LL_ENDL;
				break;
			}
			// Per-glyph bitmap texture.
			S32 next_bitmap_num = fgi->mBitmapNum;
			if (next_bitmap_num != bitmap_num)
			{
				// Actually draw the queued glyphs before switching their texture;
				// otherwise the queued glyphs will be taken from wrong textures.
				if (glyph_count > 0)
				{
					gGL.begin(LLRender::TRIANGLES);
					{
						gGL.vertexBatchPreTransformed(vertices, uvs, colors, glyph_count * GLYPH_VERTICES);
					}
					gGL.end();
					glyph_count = 0;
				}

				bitmap_num = next_bitmap_num;
				LLImageGL *font_image = font_bitmap_cache->getImageGL(bitmap_num);
				gGL.getTexUnit(0)->bind(font_image);
			}

			if ((start_x + scaled_max_pixels) < (cur_x + fgi->mXBearing + fgi->mWidth))
			{
				// Not enough room for this character.
				break;
			}

			// Draw the text at the appropriate location
			//Specify vertices and texture coordinates
			LLRectf uv_rect((fgi->mXBitmapOffset) * inv_width,
					(fgi->mYBitmapOffset + fgi->mHeight + PAD_UVY) * inv_height,
					(fgi->mXBitmapOffset + fgi->mWidth) * inv_width,
				(fgi->mYBitmapOffset - PAD_UVY) * inv_height);
			// snap glyph origin to whole screen pixel
			LLRectf screen_rect((F32)ll_round(cur_render_x + (F32)fgi->mXBearing),
				    (F32)ll_round(cur_render_y + (F32)fgi->mYBearing),
				    (F32)ll_round(cur_render_x + (F32)fgi->mXBearing) + (F32)fgi->mWidth,
				    (F32)ll_round(cur_render_y + (F32)fgi->mYBearing) - (F32)fgi->mHeight);
			
			if (glyph_count >= GLYPH_BATCH_SIZE)
			{
				gGL.begin(LLRender::TRIANGLES);
				{
					gGL.vertexBatchPreTransformed(vertices, uvs, colors, glyph_count * GLYPH_VERTICES);
				}
				gGL.end();

				glyph_count = 0;
			}

			drawGlyph(glyph_count, vertices, uvs, colors, screen_rect, uv_rect, text_color, style_to_add, shadow, drop_shadow_strength);

			chars_drawn++;
			cur_x += fgi->mXAdvance;
			cur_y += fgi->mYAdvance;

			llwchar next_char = wstr[i+1];
			if (next_char && (next_char < LAST_CHARACTER))
			{
				// Kern this puppy.
				next_glyph = mFontFreetype->getGlyphInfo(next_char);
				cur_x += mFontFreetype->getXKerning(fgi, next_glyph);
			}

			// Round after kerning.
			// Must do this to cur_x, not just to cur_render_x, otherwise you
			// will squish sub-pixel kerned characters too close together.
			// For example, "CCCCC" looks bad.
			cur_x = (F32)ll_round(cur_x);
			//cur_y = (F32)ll_round(cur_y);

			cur_render_x = cur_x;
			cur_render_y = cur_y;
		}
	}

	if(glyph_count)
	{
		gGL.begin(LLRender::TRIANGLES);
		{
			gGL.vertexBatchPreTransformed(vertices, uvs, colors, glyph_count * GLYPH_VERTICES);
		}
		gGL.end();
	}


	if (right_x)
	{
		*right_x = (cur_x - origin.mV[VX]) / sScaleX;
	}

	//FIXME: add underline as glyph?
	if (style_to_add & UNDERLINE)
	{
		F32 descender = (F32)llfloor(mFontFreetype->getDescenderHeight());

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.begin(LLRender::LINES);
		gGL.vertex2f(start_x, cur_y - descender);
		gGL.vertex2f(cur_x, cur_y - descender);
		gGL.end();
	}

	if (draw_ellipses)
	{

		// recursively render ellipses at end of string
		// we've already reserved enough room
		gGL.pushUIMatrix();
		renderUTF8(std::string("..."), 
				0,
				(cur_x - origin.mV[VX]) / sScaleX, (F32)y,
				color,
				LEFT, valign,
				style_to_add,
				shadow,
				S32_MAX, max_pixels,
				right_x,
				FALSE); 
		gGL.popUIMatrix();
	}

	gGL.popUIMatrix();

	return chars_drawn;
}

S32 LLFontGL::renderUTF8(const std::string &text, S32 begin_offset, F32 x, F32 y, const LLColor4 &color, HAlign halign,  VAlign valign, U8 style, ShadowType shadow, S32 max_chars, S32 max_pixels,  F32* right_x, BOOL use_ellipses) const
{
	return render(utf8str_to_wstring(text), begin_offset, x, y, color, halign, valign, style, shadow, max_chars, max_pixels, right_x, use_ellipses);
}

// font metrics - override for LLFontFreetype that returns units of virtual pixels
F32 LLFontGL::getAscenderHeight() const
{ 
	return mFontFreetype->getAscenderHeight() / sScaleY;
}

F32 LLFontGL::getDescenderHeight() const
{ 
	return mFontFreetype->getDescenderHeight() / sScaleY;
}

F32 LLFontGL::getLineHeight() const
{ 
	return (F32)ll_pos_round(mFontFreetype->getLineHeight() / sScaleY);
}

S32 LLFontGL::getWidth(const std::string& utf8text, const S32 begin_offset, const S32 max_chars, BOOL use_embedded) const
{
	return getWidth(utf8str_to_wstring(utf8text), begin_offset, max_chars, use_embedded);
}

S32 LLFontGL::getWidth(const LLWString& utf32text, const S32 begin_offset, const S32 max_chars, BOOL use_embedded) const
{
	F32 width = getWidthF32(utf32text, begin_offset, max_chars, use_embedded);
	return ll_pos_round(width);
}

F32 LLFontGL::getWidthF32(const std::string& utf8text, const S32 begin_offset, const S32 max_chars, BOOL use_embedded) const
{
	return getWidthF32(utf8str_to_wstring(utf8text), begin_offset, max_chars, use_embedded);
}

F32 LLFontGL::getWidthF32(const LLWString& utf32text, const S32 begin_offset, const S32 max_chars, BOOL use_embedded) const
{
	const S32 LAST_CHARACTER = LLFontFreetype::LAST_CHAR_FULL;

	const S32 max_index = llmin(llmax(max_chars, begin_offset + max_chars), S32(utf32text.length()));
	if (max_index <= 0 || begin_offset >= max_index)
		return 0;

	F32 cur_x = 0;

	const LLFontGlyphInfo* next_glyph = NULL;

	F32 width_padding = 0.f;
	for (S32 i = begin_offset; i < max_index; i++)
	{
		const llwchar wch = utf32text[i];
		const embedded_data_t* ext_data = use_embedded ? getEmbeddedCharData(wch) : NULL;
		if (ext_data)
		{
			// Handle crappy embedded hack
			cur_x += getEmbeddedCharAdvance(ext_data);

			if(i+1 < max_index)
			{
				cur_x += EXT_KERNING * sScaleX;
			}
		}
		else
		{
			const LLFontGlyphInfo* fgi = next_glyph;
			next_glyph = NULL;
			if(!fgi)
			{
				fgi = mFontFreetype->getGlyphInfo(wch);
			}

			F32 advance = mFontFreetype->getXAdvance(fgi);

			// for the last character we want to measure the greater of its width and xadvance values
			// so keep track of the difference between these values for the each character we measure
			// so we can fix things up at the end
			width_padding = llmax(	0.f,											// always use positive padding amount
									width_padding - advance,						// previous padding left over after advance of current character
									(F32)(fgi->mWidth + fgi->mXBearing) - advance);	// difference between width of this character and advance to next character

			cur_x += advance;
			if ((i + 1) < max_index)
			{
				llwchar next_char = utf32text[i+1];
				if (next_char < LAST_CHARACTER)
				{
					// Kern this puppy.
					next_glyph = mFontFreetype->getGlyphInfo(next_char);
					cur_x += mFontFreetype->getXKerning(fgi, next_glyph);
				}
			}
			// Round after kerning.
			cur_x = (F32)ll_pos_round(cur_x);
		}
	}

	// add in extra pixels for last character's width past its xadvance
	cur_x += width_padding;

	return cur_x / sScaleX;
}



// Returns the max number of complete characters from text (up to max_chars) that can be drawn in max_pixels
S32 LLFontGL::maxDrawableChars(const LLWString& utf32text, F32 max_pixels, S32 max_chars,
							   EWordWrapStyle end_on_word_boundary, const BOOL use_embedded,
							   F32* drawn_pixels) const
{
	const S32 max_index = llmin(max_chars, S32(utf32text.length()));
	if (max_index <= 0 || max_pixels <= 0.f)
		return 0;
	
	BOOL clip = FALSE;
	F32 cur_x = 0;
	F32 drawn_x = 0;

	S32 start_of_last_word = 0;
	BOOL in_word = FALSE;

	// avoid S32 overflow when max_pixels == S32_MAX by staying in floating point
	F32 scaled_max_pixels =	max_pixels * sScaleX;
	F32 width_padding = 0.f;
	
	LLFontGlyphInfo* next_glyph = NULL;

	S32 i;
	for (i=0; (i < max_index); i++)
	{
		llwchar wch = utf32text[i];
			
		const embedded_data_t* ext_data = use_embedded ? getEmbeddedCharData(wch) : NULL;
		if (ext_data)
		{
			if (in_word)
			{
				in_word = FALSE;
			}
			else
			{
				start_of_last_word = i;
			}
			cur_x += getEmbeddedCharAdvance(ext_data);
			
			if (scaled_max_pixels < cur_x)
			{
				clip = TRUE;
				break;
			}
			
			if ((i+1) < max_index)
			{
				cur_x += EXT_KERNING * sScaleX;
			}

			if( scaled_max_pixels < cur_x )
			{
				clip = TRUE;
				break;
			}
		}
		else
		{
			if (in_word)
			{
				if (iswspace(wch))
				{
					in_word = FALSE;
				}
			}
			else
			{
				start_of_last_word = i;
				if (!iswspace(wch))
				{
					in_word = TRUE;
				}
			}

			LLFontGlyphInfo* fgi = next_glyph;
			next_glyph = NULL;
			if(!fgi)
			{
				fgi = mFontFreetype->getGlyphInfo(wch);
			}

			// account for glyphs that run beyond the starting point for the next glyphs
			width_padding = llmax(	0.f,													// always use positive padding amount
									width_padding - fgi->mXAdvance,							// previous padding left over after advance of current character
									(F32)(fgi->mWidth + fgi->mXBearing) - fgi->mXAdvance);	// difference between width of this character and advance to next character

			cur_x += fgi->mXAdvance;
		
			// clip if current character runs past scaled_max_pixels (using width_padding)
			if (scaled_max_pixels < cur_x + width_padding)
			{
				clip = TRUE;
				break;
			}

			if ((i+1) < max_index)
			{
				// Kern this puppy.
				next_glyph = mFontFreetype->getGlyphInfo(utf32text[i + 1]);
				cur_x += mFontFreetype->getXKerning(fgi, next_glyph);
			}
		}
		// Round after kerning.
		cur_x = (F32)ll_pos_round(cur_x);
		drawn_x = cur_x;
	}


	if( clip )
	{
		switch (end_on_word_boundary)
		{
		case ONLY_WORD_BOUNDARIES:
			i = start_of_last_word;
			break;
		case WORD_BOUNDARY_IF_POSSIBLE:
			if (start_of_last_word != 0)
			{
				i = start_of_last_word;
			}
			break;
		default:
		case ANYWHERE:
			// do nothing
			break;
		}
	}

	if (drawn_pixels)
	{
		*drawn_pixels = drawn_x;
	}
	return i;
}


S32	LLFontGL::firstDrawableChar(const LLWString& utf32text, F32 max_pixels, S32 start_pos, S32 max_chars) const
{
	const S32 max_index = llmin(llmax(max_chars, start_pos + max_chars), S32(utf32text.length()));
	if (max_index <= 0 || start_pos >= max_index || max_pixels <= 0.f || start_pos < 0)
		return 0;
	
	F32 total_width = 0.0;
	S32 drawable_chars = 0;

	F32 scaled_max_pixels =	max_pixels * sScaleX;

	S32 start = llmin(start_pos, max_index - 1);
	for (S32 i = start; i >= 0; i--)
	{
		llwchar wch = utf32text[i];

		const embedded_data_t* ext_data = getEmbeddedCharData(wch);
		F32 width = 0;
		
		if(ext_data)
		{
			width = getEmbeddedCharAdvance(ext_data);
		}
		else
		{
			const LLFontGlyphInfo* fgi= mFontFreetype->getGlyphInfo(wch);

			// last character uses character width, since the whole character needs to be visible
			// other characters just use advance
			width = (i == start) 
				? (F32)(fgi->mWidth + fgi->mXBearing)  	// use actual width for last character
				: fgi->mXAdvance;						// use advance for all other characters										
		}

		if( scaled_max_pixels < (total_width + width) )
		{
			break;
		}

		total_width += width;
		drawable_chars++;

		if( max_index >= 0 && drawable_chars >= max_index )
		{
			break;
		}

		if ( i > 0 )
		{
			// kerning
			total_width += ext_data ? (EXT_KERNING * sScaleX) : mFontFreetype->getXKerning(utf32text[i - 1], wch);
		}

		// Round after kerning.
		total_width = (F32)ll_pos_round(total_width);
	}

	if (drawable_chars == 0)
	{
		return start_pos; // just draw last character
	}
	else
	{
		// if only 1 character is drawable, we want to return start_pos as the first character to draw
		// if 2 are drawable, return start_pos and character before start_pos, etc.
		return start_pos + 1 - drawable_chars;
	}
	
}


S32 LLFontGL::charFromPixelOffset(const LLWString& utf32text, const S32 begin_offset, F32 target_x, F32 max_pixels, S32 max_chars, BOOL round, BOOL use_embedded) const
{
	const S32 max_index = llmin(llmax(max_chars,begin_offset + max_chars), S32(utf32text.length()));
	if (max_index <= 0 || begin_offset >= max_index || max_pixels <= 0.f)
		return 0;
	
	F32 cur_x = 0;

	target_x *= sScaleX;

	F32 scaled_max_pixels =	max_pixels * sScaleX;
	
	const LLFontGlyphInfo* next_glyph = NULL;

	S32 pos;
	for (pos = begin_offset; pos < max_index; pos++)
	{
		llwchar wch = utf32text[pos];
		if (!wch)
		{
			break; // done
		}

		const embedded_data_t* ext_data = use_embedded ? getEmbeddedCharData(wch) : NULL;
		const LLFontGlyphInfo* glyph = next_glyph;
		next_glyph = NULL;
		if(!glyph && !ext_data)
		{
			glyph = mFontFreetype->getGlyphInfo(wch);
		}
		
		F32 char_width = ext_data ? getEmbeddedCharAdvance(ext_data) : mFontFreetype->getXAdvance(glyph);

		if (round)
		{
			// Note: if the mouse is on the left half of the character, the pick is to the character's left
			// If it's on the right half, the pick is to the right.
			if (target_x  < cur_x + char_width*0.5f)
			{
				break;
			}
		}
		else if (target_x  < cur_x + char_width)
		{
			break;
		}

		if (scaled_max_pixels < cur_x + char_width)
		{
			break;
		}

		cur_x += char_width;

		if ((pos + 1) < max_index)
		{
			
			if(ext_data)
			{
				cur_x += EXT_KERNING * sScaleX;
			}
			else
			{
				next_glyph = mFontFreetype->getGlyphInfo(utf32text[pos + 1]);
				cur_x += mFontFreetype->getXKerning(glyph, next_glyph);
			}
		}


		// Round after kerning.
		cur_x = (F32)ll_pos_round(cur_x);
		
	}

	return pos - begin_offset;
}

const LLFontDescriptor& LLFontGL::getFontDesc() const
{
	return mFontDescriptor;
}

const LLFontGL::embedded_data_t* LLFontGL::getEmbeddedCharData(const llwchar wch) const
{
	// Handle crappy embedded hack
	embedded_map_t::const_iterator iter = mEmbeddedChars.find(wch);
	if (iter != mEmbeddedChars.end())
	{
		return iter->second;
	}
	return NULL;
}


F32 LLFontGL::getEmbeddedCharAdvance(const embedded_data_t* ext_data) const
{
	const LLWString& label = ext_data->mLabel;
	LLImageGL* ext_image = ext_data->mImage;

	F32 ext_width = (F32)ext_image->getWidth();
	if( !label.empty() )
	{
		ext_width += (EXT_X_BEARING + getFontExtChar()->getWidthF32(label.c_str())) * sScaleX;
	}

	return (EXT_X_BEARING * sScaleX) + ext_width;
}


void LLFontGL::clearEmbeddedChars()
{
	for_each(mEmbeddedChars.begin(), mEmbeddedChars.end(), DeletePairedPointer());
	mEmbeddedChars.clear();
}

void LLFontGL::addEmbeddedChar( llwchar wc, LLTexture* image, const std::string& label ) const
{
	LLWString wlabel = utf8str_to_wstring(label);
	addEmbeddedChar(wc, image, wlabel);
}

void LLFontGL::addEmbeddedChar( llwchar wc, LLTexture* image, const LLWString& wlabel ) const
{
	embedded_data_t* ext_data = new embedded_data_t(image->getGLTexture(), wlabel);
	mEmbeddedChars[wc] = ext_data;
}

void LLFontGL::removeEmbeddedChar( llwchar wc ) const
{
	embedded_map_t::iterator iter = mEmbeddedChars.find(wc);
	if (iter != mEmbeddedChars.end())
	{
		delete iter->second;
		mEmbeddedChars.erase(wc);
	}
}

// static
void LLFontGL::initClass(F32 screen_dpi, F32 x_scale, F32 y_scale, const std::string& app_dir, bool create_gl_textures)
{
	sVertDPI = (F32)llfloor(screen_dpi * y_scale);
	sHorizDPI = (F32)llfloor(screen_dpi * x_scale);
	sScaleX = x_scale;
	sScaleY = y_scale;
	sFontDir = app_dir;

	// Font registry init
	if (!sFontRegistry)
	{
		sFontRegistry = new LLFontRegistry(create_gl_textures);
		sFontRegistry->parseFontInfo("fonts.xml");
	}
	else
	{
		sFontRegistry->reset();
	}
}

// Force standard fonts to get generated up front.
// This is primarily for error detection purposes.
// Don't do this during initClass because it can be slow and we want to get
// the viewer window on screen first. JC
// static
bool LLFontGL::loadDefaultFonts()
{
	bool succ = true;
	succ &= (NULL != getFontSansSerifSmall());
	succ &= (NULL != getFontSansSerif());
	succ &= (NULL != getFontSansSerifBig());
	succ &= (NULL != getFontSansSerifHuge());
	succ &= (NULL != getFontSansSerifBold());
	succ &= (NULL != getFontMonospace());
	succ &= (NULL != getFontExtChar());
	return succ;
}

// static
void LLFontGL::destroyDefaultFonts()
{
	// Remove the actual fonts.
	delete sFontRegistry;
	sFontRegistry = NULL;
}

//static 
void LLFontGL::destroyAllGL()
{
	if (sFontRegistry)
	{
		if (LLFontFreetype::sOpenGLcrashOnRestart)
		{
			// This will leak memory but will prevent a crash...
			sFontRegistry = NULL;
		}
		else
		{
		sFontRegistry->destroyGL();
		}
	}
}

// static
U8 LLFontGL::getStyleFromString(const std::string &style)
{
	U8 ret = 0;
	if (style.find("NORMAL") != style.npos)
	{
		ret |= NORMAL;
	}
	if (style.find("BOLD") != style.npos)
	{
		ret |= BOLD;
	}
	if (style.find("ITALIC") != style.npos)
	{
		ret |= ITALIC;
	}
	if (style.find("UNDERLINE") != style.npos)
	{
		ret |= UNDERLINE;
	}
	return ret;
}

// static
std::string LLFontGL::getStringFromStyle(U8 style)
{
	std::string style_string;
	if (style & NORMAL)
	{
		style_string += "|NORMAL";
	}
	if (style & BOLD)
	{
		style_string += "|BOLD";
	}
	if (style & ITALIC)
	{
		style_string += "|ITALIC";
	}
	if (style & UNDERLINE)
	{
		style_string += "|UNDERLINE";
	}
	return style_string;
}

// static
std::string LLFontGL::nameFromFont(const LLFontGL* fontp)
{
	return fontp->mFontDescriptor.getName();
}


// static
std::string LLFontGL::sizeFromFont(const LLFontGL* fontp)
{
	return fontp->getFontDesc().getSize();
}

// static
std::string LLFontGL::nameFromHAlign(LLFontGL::HAlign align)
{
	if (align == LEFT)			return std::string("left");
	else if (align == RIGHT)	return std::string("right");
	else if (align == HCENTER)	return std::string("center");
	else return std::string();
}

// static
LLFontGL::HAlign LLFontGL::hAlignFromName(const std::string& name)
{
	LLFontGL::HAlign gl_hfont_align = LLFontGL::LEFT;
	if (name == "left")
	{
		gl_hfont_align = LLFontGL::LEFT;
	}
	else if (name == "right")
	{
		gl_hfont_align = LLFontGL::RIGHT;
	}
	else if (name == "center")
	{
		gl_hfont_align = LLFontGL::HCENTER;
	}
	//else leave left
	return gl_hfont_align;
}

// static
std::string LLFontGL::nameFromVAlign(LLFontGL::VAlign align)
{
	if (align == TOP)			return std::string("top");
	else if (align == VCENTER)	return std::string("center");
	else if (align == BASELINE)	return std::string("baseline");
	else if (align == BOTTOM)	return std::string("bottom");
	else return std::string();
}

// static
LLFontGL::VAlign LLFontGL::vAlignFromName(const std::string& name)
{
	LLFontGL::VAlign gl_vfont_align = LLFontGL::BASELINE;
	if (name == "top")
	{
		gl_vfont_align = LLFontGL::TOP;
	}
	else if (name == "center")
	{
		gl_vfont_align = LLFontGL::VCENTER;
	}
	else if (name == "baseline")
	{
		gl_vfont_align = LLFontGL::BASELINE;
	}
	else if (name == "bottom")
	{
		gl_vfont_align = LLFontGL::BOTTOM;
	}
	//else leave baseline
	return gl_vfont_align;
}

//static
LLFontGL* LLFontGL::getFontMonospace()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("Monospace","Monospace",0));
	return fontp;
}

//static
LLFontGL* LLFontGL::getFontSansSerifSmall()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("SansSerif","Small",0));
	return fontp;
}

//static
LLFontGL* LLFontGL::getFontSansSerif()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("SansSerif","Medium",0));
	return fontp;
}

//static
LLFontGL* LLFontGL::getFontSansSerifBig()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("SansSerif","Large",0));
	return fontp;
}

//static 
LLFontGL* LLFontGL::getFontSansSerifHuge()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("SansSerif","Huge",0));
	return fontp;
}

//static 
LLFontGL* LLFontGL::getFontSansSerifBold()
{
	static LLFontGL* fontp = getFont(LLFontDescriptor("SansSerif","Medium",BOLD));
	return fontp;
}

//static
LLFontGL* LLFontGL::getFontExtChar()
{
	return getFontSansSerif();
}

//static 
LLFontGL* LLFontGL::getFont(const LLFontDescriptor& desc)
{
	return sFontRegistry->getFont(desc);
}

// static
LLFontGL* LLFontGL::getFontByName(const std::string& name)
{
	// check for most common fonts first
	if (name == "SANSSERIF")
	{
		return getFontSansSerif();
	}
	else if (name == "SANSSERIF_SMALL")
	{
		return getFontSansSerifSmall();
	}
	else if (name == "SANSSERIF_BIG")
	{
		return getFontSansSerifBig();
	}
	else if (name == "SMALL" || name == "OCRA")
	{
		// *BUG: Should this be "MONOSPACE"?  Do we use "OCRA" anymore?
		// Does "SMALL" mean "SERIF"?
		return getFontMonospace();
	}
	else
	{
		return NULL;
	}
}

//static
LLFontGL* LLFontGL::getFontDefault()
{
	return getFontSansSerif(); // Fallback to sans serif as default font
}

static std::string sSystemFontPath;

// static 
std::string LLFontGL::getFontPathSystem()
{
	if (!sSystemFontPath.empty()) return sSystemFontPath;

#if LL_WINDOWS
	wchar_t* pPath = nullptr;
	if (SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &pPath) == S_OK)
	{
		sSystemFontPath = ll_convert_wide_to_string(pPath, CP_UTF8) + gDirUtilp->getDirDelimiter();
        LL_INFOS() << "from SHGetKnownFolderPath(): " << sSystemFontPath << LL_ENDL;
		CoTaskMemFree(pPath);
		pPath = nullptr;
	}
	else
	{
		// Try to figure out where the system's font files are stored.
    	auto system_root = LLStringUtil::getenv("SystemRoot");
    	if (! system_root.empty())
    	{
			sSystemFontPath = gDirUtilp->add(system_root, "fonts") + gDirUtilp->getDirDelimiter();
        	LL_INFOS() << "from SystemRoot: " << sSystemFontPath << LL_ENDL;
		}
		else
		{
			LL_WARNS() << "SystemRoot not found, attempting to load fonts from default path." << LL_ENDL;
			// HACK for windows 98/Me
			sSystemFontPath = "/WINDOWS/FONTS/";
		}
	}

#elif LL_DARWIN
		// HACK for Mac OS X
	sSystemFontPath = "/System/Library/Fonts/";
#endif
	return sSystemFontPath;
}

static std::string sLocalFontPath;

// static 
std::string LLFontGL::getFontPathLocal()
{
	if (!sLocalFontPath.empty()) return sLocalFontPath;

	// Backup files if we can't load from system fonts directory.
	// We could store this in an end-user writable directory to allow
	// end users to switch fonts.
	if (!LLFontGL::sFontDir.empty())
	{
		// use specified application dir to look for fonts
		sLocalFontPath = gDirUtilp->add(LLFontGL::sFontDir, "fonts") + gDirUtilp->getDirDelimiter();
	}
	else
	{
		// assume working directory is executable directory
		sLocalFontPath = "./fonts/";
	}
	return sLocalFontPath;
}

LLFontGL::LLFontGL(const LLFontGL &source)
{
	LL_ERRS() << "Not implemented!" << LL_ENDL;
}

LLFontGL &LLFontGL::operator=(const LLFontGL &source)
{
	LL_ERRS() << "Not implemented" << LL_ENDL;
	return *this;
}

void LLFontGL::renderQuad(LLVector4a* vertex_out, LLVector2* uv_out, LLColor4U* colors_out, const LLRectf& screen_rect, const LLRectf& uv_rect, const LLColor4U& color, F32 slant_amt) const
{
	S32 index = 0;

	vertex_out[index].set(screen_rect.mLeft, screen_rect.mTop, 0.f);
	uv_out[index] = LLVector2(uv_rect.mLeft, uv_rect.mTop);
	colors_out[index] = color;
	index++;

	vertex_out[index].set(screen_rect.mLeft + slant_amt, screen_rect.mBottom, 0.f);
	uv_out[index] = LLVector2(uv_rect.mLeft, uv_rect.mBottom);
	colors_out[index] = color;
	index++;

	vertex_out[index].set(screen_rect.mRight, screen_rect.mTop, 0.f);
	uv_out[index] = LLVector2(uv_rect.mRight, uv_rect.mTop);
	colors_out[index] = color;
	index++;

	vertex_out[index].set(screen_rect.mRight, screen_rect.mTop, 0.f);
	uv_out[index] = LLVector2(uv_rect.mRight, uv_rect.mTop);
	colors_out[index] = color;
	index++;

	vertex_out[index].set(screen_rect.mLeft + slant_amt, screen_rect.mBottom, 0.f);
	uv_out[index] = LLVector2(uv_rect.mLeft, uv_rect.mBottom);
	colors_out[index] = color;
	index++;

	vertex_out[index].set(screen_rect.mRight + slant_amt, screen_rect.mBottom, 0.f);
	uv_out[index] = LLVector2(uv_rect.mRight, uv_rect.mBottom);
	colors_out[index] = color;
}

void LLFontGL::drawGlyph(S32& glyph_count, LLVector4a* vertex_out, LLVector2* uv_out, LLColor4U* colors_out, const LLRectf& screen_rect, const LLRectf& uv_rect, const LLColor4U& color, U8 style, ShadowType shadow, F32 drop_shadow_strength) const
{
	F32 slant_offset;
	slant_offset = ((style & ITALIC) ? ( -mFontFreetype->getAscenderHeight() * 0.2f) : 0.f);

	//FIXME: bold and drop shadow are mutually exclusive only for convenience
	//Allow both when we need them.
	if (style & BOLD)
	{
		for (S32 pass = 0; pass < 2; pass++)
		{
			LLRectf screen_rect_offset = screen_rect;

			screen_rect_offset.translate((F32)(pass * BOLD_OFFSET), 0.f);
			const U32 idx = glyph_count * GLYPH_VERTICES;
			renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect_offset, uv_rect, color, slant_offset);
			glyph_count++;
		}
	}
	else if (shadow == DROP_SHADOW_SOFT)
	{
		LLColor4U& shadow_color = LLFontGL::sShadowColor;
		shadow_color.mV[VALPHA] = U8(color.mV[VALPHA] * drop_shadow_strength * DROP_SHADOW_SOFT_STRENGTH);
		for (S32 pass = 0; pass < 5; pass++)
		{
			LLRectf screen_rect_offset = screen_rect;

			switch(pass)
			{
			case 0:
				screen_rect_offset.translate(-1.f, -1.f);
				break;
			case 1:
				screen_rect_offset.translate(1.f, -1.f);
				break;
			case 2:
				screen_rect_offset.translate(1.f, 1.f);
				break;
			case 3:
				screen_rect_offset.translate(-1.f, 1.f);
				break;
			case 4:
				screen_rect_offset.translate(0, -2.f);
				break;
			}
		
			const U32 idx = glyph_count * GLYPH_VERTICES;
			renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect_offset, uv_rect, shadow_color, slant_offset);
			glyph_count++;
		}
		const U32 idx = glyph_count * GLYPH_VERTICES;
		renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect, uv_rect, color, slant_offset);
		glyph_count++;
	}
	else if (shadow == DROP_SHADOW)
	{
		LLColor4U& shadow_color = LLFontGL::sShadowColor;
		shadow_color.mV[VALPHA] = U8(color.mV[VALPHA] * drop_shadow_strength);
		LLRectf screen_rect_shadow = screen_rect;
		screen_rect_shadow.translate(1.f, -1.f);
		U32 idx = glyph_count * GLYPH_VERTICES;
		renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect_shadow, uv_rect, shadow_color, slant_offset);
		glyph_count++;
		idx = glyph_count * GLYPH_VERTICES;
		renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect, uv_rect, color, slant_offset);
		glyph_count++;
	}
	else // normal rendering
	{
		const U32 idx = glyph_count * GLYPH_VERTICES;
		renderQuad(&vertex_out[idx], &uv_out[idx], &colors_out[idx], screen_rect, uv_rect, color, slant_offset);
		glyph_count++;
	}
}
