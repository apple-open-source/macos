/*
 * Any bit blitters are not included in FreeType 1.x.
 * Because bitmap glyph is visible for client aplictions and
 * FreeType is not provide any text renderers.
 */

#include "xttblit.h"

/*
 * SBit blitter
 * Derived from ttsbit.c of FreeType 2 (with modifications)
 */
/***************************************************************************/
/*                                                                         */
/*  ttsbit.c                                                               */
/*                                                                         */
/*    TrueType and OpenType embedded bitmap support (body).                */
/*                                                                         */
/*  Copyright 1996-1999 by                                                 */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used        */
/*  modified and distributed under the terms of the FreeType project       */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/*                                                                         */
/*  WARNING: This file should not be compiled directly, it is meant to be  */
/*           included in the source of several font drivers (i.e., the TTF */
/*           and OTF drivers).                                             */
/*                                                                         */
/***************************************************************************/

  static
  void blit_stream( TT_Raster_Map*  target,
                    TT_Byte*        source,
                    TT_Int          line_bits,
                    TT_Int          height,
                    TT_Bool         align,
                    TT_Int          x_offset,
                    TT_Int          y_offset,
                    TT_Bool         inverted )
  {
    TT_Byte*  line_buff;
    TT_Int    line_incr;
    TT_Int    start_incr;
    TT_Int    cursor_sign;

    TT_UInt   acc;
    TT_Byte   loaded;

    TT_Int    y;

    /* first of all, compute starting write position */
    if ( target->flow == TT_Flow_Down )
    {
      line_buff  = (TT_Byte*)target->bitmap;
      start_incr = target->cols;
    }
    else
    {
      line_buff  = (TT_Byte*)target->bitmap + (target->rows-1)*target->cols;
      start_incr = -target->cols;
    }

    /* We must set height when inverted is true */
    if (inverted)
    {
      if (height < 0)
        return;

      line_incr = -target->cols;
    }
    else
      line_incr = target->cols;

    cursor_sign = (( target->flow == TT_Flow_Down ) ^ inverted ) ? 1 : -1;

    /* preperation for clipping */
    if ( x_offset > 0 )
      line_buff += x_offset / 8;
    if ( y_offset > 0 )
      line_buff += y_offset * start_incr;

    if ( height < 0 )
      height = target->rows;

    /*********************************************************************/
    /*                                                                   */
    /* We use the extra-classic 'accumulator' trick to extract the       */
    /* bits from the source byte stream.                                 */
    /*                                                                   */
    /* Namely, the variable 'acc' is a 16-bit accumulator containing     */
    /* the last 'loaded' bits from the input stream. The bits are        */
    /* shifted to the upmost position in 'acc'..                         */
    /*                                                                   */

    acc    = 0;  /* clear accumulator   */
    loaded = 0;  /* no bits were loaded */

    /* clip */
    if ( y_offset < 0 )
    {
      switch ( align )
      {
        TT_Int step;

      case -1:
        source -= ((TT_Int)( line_bits + 7 ) / 8 ) * y_offset;
        break;

      case 0:
        step = ( - line_bits * y_offset ) & 7;

        source += ( - line_bits * y_offset ) / 8;
        if (step)
        {
          acc     = ((TT_UShort)*source++) << (step+8);
          loaded  = 8 - step;
        }
        break;

      default:
        source += - align * y_offset;
        break;
      }
    }

    /* y axis */
    for ( y = 0 ; y < height ; y++ )
    {
      TT_Byte*  cur   = line_buff;    /* current write cursor               */
      TT_Int    count = line_bits;    /* number of bits to extract per line */
      TT_Byte   shift = x_offset & 7; /* current write shift                */
      TT_Byte   space = 8 - shift;
      TT_Byte*  source_position = source;
      TT_Int    byte_count;

      if ( x_offset >= 0 )
        byte_count = x_offset / 8;
      else
        byte_count = ( x_offset - 7 ) / 8 ;

      if (( y_offset + cursor_sign * y ) >= target->rows )
        break;

      /* x axis */
      if ( byte_count < 0 )
      {
        TT_Int skip_byte = - byte_count - 1;

        if (!skip_byte)
          acc = loaded = 0;

        source += skip_byte;
        if (loaded < 8)
        {
          acc    |= ((TT_UShort)*source++) << (8-loaded);
          loaded += 8;
        }
        acc <<= space;
        loaded -= space;
        count += x_offset;
        /* In this case, start cursor is always left-sided */
        byte_count = 0;
        shift = 0;
        space = 8;
      }


      /* first of all, read individual source bytes */
      if ( count >= 8 )
      {
        count -= 8;
#if 0
        /* This part is not modified yet */

        /* small speedups for trivial cases */
        if ( loaded == 0 )
        {
          /* super trivial case :-) */
          if ( shift == 0 )
          {
            do
            {
              *cur++ |= *source++;
              count  -= 8;
            }
            while (count >= 0);
          }

          /* otherwise .... */
          else
          {
            TT_Byte  cache = cur[0];
            do
            {
              TT_Byte  val = *source++;

              cur[0] = cache | (val >> shift);
              cache  = val << space;
              cur++;
              count -= 8;
            }
            while (count >= 0);

            cur[0] |= cache;
          }
        }
        else /* normal cases */
#endif /* 0 */
        {
          do
          {
            TT_Byte  val;

            /* ensure that there are at least 8 bits in the accumulator */
            if (loaded < 8)
            {
              acc    |= ((TT_UShort)*source++) << (8-loaded);
              loaded += 8;
            }

            /* now write one byte */
            val     = (TT_Byte)(acc >> 8);
            if ( byte_count < target->cols )
            {
              cur[0] |= val >> shift;
              if ( byte_count < target->cols - 1 )
                cur[1] |= val << space;
            }

            cur++;
            byte_count++;
            acc   <<= 8;  /* remove bits from accumulator */
            loaded -= 8;
            count  -= 8;
          }
          while ( count >= 0 );
        }

        /* restore 'count' to correct value */
        count += 8;
      }

      /* now write remaining bits (count < 8) */
      if ( count > 0 )
      {
        TT_Byte  val;

        /* ensure that there are at least 'count' bits in the accumulator */
        if (loaded < count)
        {
          acc    |= ((TT_UShort)*source++) << (8-loaded);
          loaded += 8;
        }

        /* now write remaining bits */
        val     = ((TT_Byte)(acc >> 8)) & ~(0xFF >> count);
        if ( byte_count < target->cols )
        {
          cur[0] |= val >> shift;

          if ((count > space) && (byte_count < target->cols - 1))
                  cur[1] |= val << space;
        }

        acc   <<= count;
        loaded -= count;
      }

      /* now, skip to next line */
      if ( align )
      {
        acc = loaded = 0;   /* clear accumulator on byte-padded lines */
        if ( align > 0 )
        {
          source_position += align;
          source = source_position;
        }
      }

      line_buff += line_incr;
    }
  }
/* */

/*
 * This function is not included in FreeType 1 engine.
 * Because SBit_Image is able to access from any applications directory.
 */
  TT_Error XTT_Get_SBit_Bitmap( TT_Raster_Map* target,
                                TT_SBit_Image* source_sbit,
                                TT_Int         x_offset,
                                TT_Int         y_offset )
  {
    TT_Raster_Map* source = &source_sbit->map;
    TT_Int   inverted = 0;
    TT_Int   line_bits;
    TT_Int   height;

    if ((target->flow != TT_Flow_Up) && (target->flow != TT_Flow_Down))
      return TT_Err_Invalid_Argument;

    /* calculate offset */
    x_offset += ( source_sbit->metrics.bbox.xMin & -64 ) / 64;
    x_offset *= source_sbit->bit_depth;

    y_offset = target->rows
      - ( y_offset + (( source_sbit->metrics.bbox.yMax + 63 ) & -64 ) / 64 );

    inverted = (( source->flow * target->flow ) == -1 );
    /* correct??? */
    line_bits = source->width * source_sbit->bit_depth;

    /* return if offsets are obvious mismatched */
    if (( x_offset + line_bits < 0 ) ||
        ( x_offset >= target->width ) ||
        ( y_offset >= target->rows ) ||
        ( y_offset + source->rows < 0 ))
      return TT_Err_Invalid_Argument;

    /* return if the size of source is invalid */
    if (( source->cols <= 0 ) ||
        ( source->rows <= 0 ) ||
        ( source->size <= 0 ))
      return TT_Err_Invalid_Argument;

    height = ( y_offset >= 0 ) ? source->rows : ( source->rows + y_offset );

    blit_stream( target, source->bitmap, line_bits, height,
                 source->cols, x_offset, y_offset, inverted );

    return TT_Err_Ok;
  }
