/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright 1996-1999 by                                                  */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*  ftstrtto: Making string text from individual glyph information, using   */
/*            TrueType Open features.                                       */
/*                                                                          */
/*  Keys:                                                                   */
/*                                                                          */
/*    + :   fast scale up                                                   */
/*    - :   fast scale down                                                 */
/*    u :   fine scale down                                                 */
/*    j :   fine scale up                                                   */
/*                                                                          */
/*    h :   toggle hinting                                                  */
/*    K :   toggle kerning                                                  */
/*    B :   toggle sbit                                                     */
/*    G :   toggle GSUB                                                     */
/*                                                                          */
/*    q :                                                                   */
/*  ESC :   exit                                                            */
/*                                                                          */
/*                                                                          */
/*  NOTE:  This is just a test program that is used to show off and         */
/*         debug the current engine.                                        */
/*                                                                          */
/****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arabic.h"
#include "blitter.h"
#include "common.h"  /* for Panic() only */
#include "display.h"

#include "freetype.h"
#include "ftxkern.h"
#include "ftxopen.h"
#include "ftxsbit.h"

#include "gdriver.h"
#include "gevents.h"
#include "gmain.h"


#define MAXPTSIZE  500                 /* dtp */
#define Center_X   ( Bit.width / 2 )   /* dtp */
#define Center_Y   ( Bit.rows  / 2 )   /* dtp */

#define FEATURE_init  MAKE_TT_TAG( 'i', 'n', 'i', 't' )
#define FEATURE_medi  MAKE_TT_TAG( 'm', 'e', 'd', 'i' )
#define FEATURE_fina  MAKE_TT_TAG( 'f', 'i', 'n', 'a' )
#define FEATURE_isol  MAKE_TT_TAG( 'i', 's', 'o', 'l' )


  char  Header[128];

  TT_Engine    engine;
  TT_Face      face;
  TT_Instance  instance;
  TT_Glyph     glyph;
  TT_CharMap   char_map;
  TT_Kerning   directory;

  TTO_GSUBHeader   gsub_;
  TTO_GSUBHeader*  gsub;
  TTO_GDEFHeader   gdef_;
  TTO_GDEFHeader*  gdef;

  TT_Big_Glyph_Metrics  metrics;
  TT_Outline            outline;
  TT_Face_Properties    face_properties;
  TT_Instance_Metrics   imetrics;

  TT_SBit_Image*  sbit;

  int      pt_size;
  int      ttc_index;
  TT_Bool  hinted;
  TT_Bool  gray_render;
  TT_Bool  r2l;
  TT_Bool  vertical;

  TT_Bool  has_kern;
  TT_Bool  use_kern;
  TT_Bool  has_gdef;
  TT_Bool  has_gsub;
  TT_Bool  use_gsub;
  TT_Bool  has_sbit;
  TT_Bool  use_sbit;
  TT_Bool  glyph_has_sbit;

  TT_Bool  default_language_system;

  int  Fail;

  char*       char_string;

  TT_UShort   glyph_code_array[129];
  TT_UShort   char_code[128];
  TT_UShort   properties[128];

  TT_UShort*  glyph_code;
  int         num_glyphs;

  TT_ULong    script_tag;
  char*       script_tag_string;
  TT_UShort   script_index;

  TT_ULong    language_tag;
  char*       language_tag_string;
  TT_UShort   language_index;
  TT_UShort   req_feature_index = 0xFFFF;

  TT_ULong*   feature_tags;
  char**      feature_tag_strings;
  TT_UShort*  feature_indices;
  int         num_features;


  static void  Select_CMap( void )
  {
    TT_UShort  i, n;
    TT_UShort  platform, encoding;


    n = face_properties.num_CharMaps;

    for ( i = 0; i < n; i++ )
    {
      TT_Get_CharMap_ID( face, i, &platform, &encoding );
      if ( platform == 3 && encoding == 1 )
      {
        TT_Get_CharMap( face, i, &char_map );
        break;
      }
    }

    /* we try only pid/eid (0,0) if no (3,1) map is found -- many Windows
       fonts have only rudimentary (0,0) support.                         */

    if ( i == n )
    {
      for ( i = 0; i < n; i++ )
      {
        TT_Get_CharMap_ID( face, i, &platform, &encoding );
        if ( platform == 0 && encoding == 0 )
        {
          TT_Get_CharMap( face, i, &char_map );
          break;
        }
      }

      if ( i == n )
        Panic( "Sorry, but this font doesn't contain"
               " any Unicode mapping table\n" );
    }
  }


  /* Convert a Latin 1 string to a string of glyph indexes.             */
  /*                                                                    */
  /* IMPORTANT NOTE:                                                    */
  /*                                                                    */
  /* There is no portable way to convert from any system's char. code   */
  /* to Unicode.  This function simply takes a char. string as argument */
  /* and `interprets' each character as a Unicode char. index with no   */
  /* further check.                                                     */
  /*                                                                    */
  /* We interpret the command line string as Unicode with the high byte */
  /* set to zero.  This is equivalent to Latin-1.                       */

  static void  Latin1Char_To_Glyph( char*  source )
  {
    TT_UShort  n;


    glyph_code = glyph_code_array + 1;  /* we want to make glyph_code[-1] */
    glyph_code[-1] = 0;                 /* possible.                      */

    for ( n = 0; n < 128 && source[n]; n++ )
    {
      char_code[n]  = (TT_UShort)( (unsigned char)source[n] );
      glyph_code[n] = TT_Char_Index( char_map, char_code[n] );
    }

    num_glyphs = n;
  }


  static void  UTF8Char_To_Glyph( char*  source )
  {
    TT_UShort  in, out, in_code, out_code;
    TT_UShort  count, limit;


    glyph_code = glyph_code_array + 1;  /* we want to make glyph_code[-1] */
    glyph_code[-1] = 0;                 /* possible.                      */

    for ( in = out = 0, count = limit = 1, in_code = out_code = 0;
          in < 128 && source[in]; in++ )
    {
      in_code = (TT_UShort)( (unsigned char)source[in] );

      if ( in_code >= 0xC0 )
      {
        limit = 1;
        count = 1;

        if ( in_code < 0xE0 )           /* U+0080 - U+07FF */
        {
          limit = 2;
          out_code = in_code & 0x1F;
        }
        else if ( in_code < 0xF0 )      /* U+0800 - U+FFFF */
        {
          limit = 3;
          out_code = in_code & 0x0F;
        }
        continue;
      }
      else if ( in_code >= 0x80 )
      {
        count++;

        if ( count <= limit )
        {
          out_code <<= 6;
          out_code  += in_code & 0x3F;
        }
        if ( count != limit )
          continue;
      }
      else
        out_code = in_code;

      char_code[out]    = out_code;
      glyph_code[out++] = TT_Char_Index( char_map, out_code );
    }

    num_glyphs = out;
  }


  static TT_Error  Reset_Scale( int  pointSize )
  {
    TT_Error  error;


    error = TT_Set_Instance_CharSize( instance, pointSize * 64L );
    if ( error )
    {
      RestoreScreen();
      Panic( "Could not reset instance, code = 0x%x.\n", error );
    }

    TT_Get_Instance_Metrics( instance, &imetrics );

    /* now re-allocate the small bitmap */
    if ( gray_render )
    {
      Init_Small( imetrics.x_ppem, imetrics.y_ppem );
      Clear_Small();
    }

    return TT_Err_Ok;
  }


  static TT_Error  Load_TrueType_Char( TT_UShort  idx,
                                       int        hint )
  {
    int       flags;
    TT_Error  error;


    glyph_has_sbit = 0;

    error = TT_Load_Glyph_Bitmap( face, instance, idx, sbit );
    if ( error == TT_Err_Ok )
    {
      has_sbit = 1;
      glyph_has_sbit = 1;
    }

    if ( glyph_has_sbit && use_sbit )
      return TT_Err_Ok;

    flags = TTLOAD_SCALE_GLYPH;
    if ( hint )
      flags |= TTLOAD_HINT_GLYPH;

    return TT_Load_Glyph( instance, glyph, idx, flags );
  }


  static TT_Error  Get_Kern_Values( TT_UShort  idx,
                                    TT_Pos*    x,
                                    TT_Pos*    y )
  {
    TT_UShort         i;
    TT_Kern_Subtable  table;
    TT_Kern_0_Pair*   pairs_0;

    TT_UShort  min, max, new_min, new_max, middle;
    TT_Long    target_idx, current_idx;


    *x = 0;
    *y = 0;

    for ( i = 0; i < directory.nTables; i++ )
    {
      table = directory.tables[i];

      /* handle only horizontal kerning tables */

      if ( table.coverage & 0x0001 )
      {
        switch ( table.format )
        {
        case 0:
          pairs_0    = table.t.kern0.pairs;
          target_idx = ( glyph_code[idx - 1] << 16 ) + glyph_code[idx];

          /* binary search */

          new_min = 0;
          new_max = table.t.kern0.nPairs - 1;

          do
          {
            min    = new_min;
            max    = new_max;
            middle = max - ( ( max - min ) >> 1 );

            current_idx = ( pairs_0[middle].left << 16 ) +
                          pairs_0[middle].right;

            if ( target_idx == current_idx )
            {
              *x += pairs_0[middle].value;
              break;
            }
            else if ( target_idx < current_idx )
            {
              if ( middle == min )
                break;
              new_max = middle - 1;
            }
            else
            {
              if ( middle == max )
                break;
              new_min = middle + 1;
            }
          } while ( min < max );

          break;

        /* we currently ignore format 2 kerning tables */

        case 2:
          break;
        }
      }
    }

    /* scaling and rounding */
 
    *x = ( ( ( *x * imetrics.x_scale ) / 0x10000 ) + 32 ) & -64;
    *y = ( ( ( *y * imetrics.y_scale ) / 0x10000 ) + 32 ) & -64;

    return TT_Err_Ok;
  }


  /* for testing purposes, we always select the last available alternate
     glyph, not using the `data' field.                                  */

  static TT_UShort  alternate_function( TT_ULong    pos,
                                        TT_UShort   glyphID,
                                        TT_UShort   num_alternates,
                                        TT_UShort*  alternates,
                                        void*       data )
  {
    return num_alternates - 1;
  }


  static TT_Error  Render_All( void )
  {
    TT_Pos           x, y, z, min_x, min_y, max_x, max_y;
    TT_Pos           kern_x, kern_y;
    int              i, n;
    TT_UShort*       gc;
    TT_UShort        glyph_property = 0;

    TT_Error         error;

    TTO_GSUB_String  in, out;


    /* On the first pass, we compute the compound bounding box */

    x = y = 0;
    kern_x = kern_y = 0;
    min_x = min_y = max_x = max_y = 0;

    in.length     = num_glyphs;
    in.pos        = 0;
    in.string     = glyph_code;
    in.properties = properties;

    out.pos        = 0;
    out.allocated  = 0;
    out.string     = NULL;
    out.properties = NULL;

    if ( has_gsub && use_gsub )
    {
      error = TT_GSUB_Apply_String( gsub, &in, &out );
      if ( error && error != TTO_Err_Not_Covered )
        return error;

      n  = out.length;
      gc = out.string;
    }
    else
    {
      n  = in.length;
      gc = in.string;
    }

    has_sbit = 0;

    for ( i = 0; i < n; i++ )
    {
      error = Load_TrueType_Char( gc[i], hinted );
      if ( error == TT_Err_Ok )
      {
        if ( glyph_has_sbit && use_sbit )
          metrics = sbit->metrics;
        else
          TT_Get_Glyph_Big_Metrics( glyph, &metrics );
        if ( has_kern && use_kern )
          Get_Kern_Values( i, &kern_x, &kern_y );

        z = x + metrics.bbox.xMin + kern_x;
        if ( min_x > z )
          min_x = z;

        z = x + metrics.bbox.xMax + kern_x;
        if ( max_x < z )
          max_x = z;

        z = y + metrics.bbox.yMin + kern_y;
        if ( min_y > z )
          min_y = z;

        z = y + metrics.bbox.yMax + kern_y;
        if ( max_y < z )
          max_y = z;

        if ( has_gdef )
        {
          error = TT_GDEF_Get_Glyph_Property( gdef, gc[i], &glyph_property );
          if ( error )
            return error;
        }

        /* advance only if it is not a mark glyph */

        if ( !( glyph_property & TTO_MARK ) )
        {
          if ( vertical )
            y += ( metrics.vertAdvance & -64 ) + kern_y;
          else
            x += ( metrics.horiAdvance & -64 ) + kern_x;
        }
      }
      else
        Fail++;
    }

    /* We now center the bbox inside the target bitmap */

    min_x = ( min_x & -64 ) >> 6;
    min_y = ( min_y & -64 ) >> 6;

    max_x = ( (max_x + 63) & -64 ) >> 6;
    max_y = ( (max_y + 63) & -64 ) >> 6;

    max_x -= min_x;
    max_y -= min_y;

    min_x = ( Bit.width - max_x ) / 2;
    min_y = ( Bit.rows  - max_y ) / 2;

    max_x += min_x;
    max_y += min_y;

    /* On the second pass, we render each glyph to its centered position. */
    /* This is slow, because we reload each glyph to render it!           */

    x = vertical ? min_x : ( r2l ? max_x : min_x );
    y = vertical ? ( r2l ? min_y : max_y ) : min_y;

    for ( i = 0; i < n; i++ )
    {
      error = Load_TrueType_Char( gc[i], hinted );
      if ( error == TT_Err_Ok )
      {
        if ( glyph_has_sbit && use_sbit )
          metrics = sbit->metrics;
        else
          TT_Get_Glyph_Big_Metrics( glyph, &metrics );
        if ( has_kern && use_kern )
          Get_Kern_Values( i, &kern_x, &kern_y );

        if ( has_gdef )
          (void)TT_GDEF_Get_Glyph_Property( gdef, gc[i], &glyph_property );

        if ( !( glyph_property & TTO_MARK ) )
        {
          if ( r2l )
          {
            if ( vertical )
              y += metrics.vertAdvance / 64;
            else
              x -= metrics.horiAdvance / 64;
          }
          else
          {
            if ( vertical )
              y -= kern_y / 64;
            else
              x += kern_x / 64;
          }
        }

        /* We must specify the upper left corner of the bitmap, but the
           lower left corner for the outline.  Another complication is that
           Blit_Bitmap() assumes that increasing y values means moving
           downwards.

           For vertical layout, the origin of the horizontal and vertical
           bearings of embedded bitmaps is the top, thus we shift the
           outline glyphs down.                                           */

        if ( glyph_has_sbit && use_sbit )
          Blit_Bitmap( &Bit,
                       &sbit->map,
                       gray_render ? 8 : 1,
                       x +
                         ( vertical ? metrics.vertBearingX :
                                      metrics.horiBearingX ) / 64,
                       Bit.rows - y -
                         ( vertical ? metrics.vertBearingY :
                                      metrics.horiBearingY ) / 64,
                       gray_palette[4] );
        else
          Render_Single_Glyph(
            gray_render,
            glyph,
            x,
            y -
              ( vertical ? metrics.vertBearingY + metrics.bbox.yMax :
                           0                                        ) / 64 );

        if ( !( glyph_property & TTO_MARK ) )
        {
          if ( r2l )
          {
            if ( vertical )
              y += kern_y / 64;
            else
              x -= kern_x / 64;
          }
          else
          {
            if ( vertical )
              y -= metrics.vertAdvance / 64;
            else
              x += metrics.horiAdvance / 64;
          }
        }
      }
    }

    if ( out.string )
      free( out.string );
    if ( out.properties )
      free( out.properties );

    return TT_Err_Ok;
  }


  static int  Process_Event( TEvent*  event )
  {
    switch ( event->what )
    {
    case event_Quit:                    /* ESC or q */
      return 0;

    case event_Keyboard:
      if ( event->info == 'h' )         /* Toggle hinting */
        hinted = !hinted;
      else if ( event->info == 'K' )    /* Toggle kerning */
        use_kern = !use_kern;
      else if ( event->info == 'B' )    /* Toggle sbit    */
        use_sbit = !use_sbit;
      else if ( event->info == 'G' )    /* Toggle gsub    */
        use_gsub = !use_gsub;
      break;

    case event_Rotate_Glyph:
      break;

    case event_Scale_Glyph:
      pt_size += event->info;
      if ( pt_size < 1 )         pt_size = 1;
      if ( pt_size > MAXPTSIZE ) pt_size = MAXPTSIZE;
      break;

    case event_Change_Glyph:
      break;
    }

    return 1;
  }


  static void  Usage( char*  execname )
  {
    fprintf( stderr,
      "\n"
      "ftstrtto: TrueType Open String Test Display -- part of the FreeType project\n"
      "---------------------------------------------------------------------------\n"
      "\n"
      "Usage: %s [options below] ppem fontname[.ttf|.ttc] [string|-]\n"
      "\n"
      "  -c C   use font with index C in TrueType collection (default: 0)\n"
      "  -f F   use feature F (can be specified more than once)\n"
      "  -g     gray-level rendering\n"
      "  -l L   use language L\n"
      "  -r R   use resolution R dpi (default: 96)\n"
      "  -s S   use script S\n"
      "  -u     interpret input data as UTF8-encoded\n"
      "  -v     display string vertically\n"
      "  -x     display string from right to left\n"
      "\n"
      "    F, L, and S must be specified as 4-character tags.\n"
      "    Specifying only F and S selects default language system of S.\n"
      "    Specifying only L and S selects the req. feature of L only (if any).\n"
      "\n"
      "    If `-' is specified as input string, stdin is read instead.\n"
      "\n", execname );

    exit( EXIT_FAILURE );
  }


  static TT_ULong  Make_Tag( char*  tag_string )
  {
    char  t1 = ' ', t2 = ' ', t3 = ' ', t4 = ' ';


    if ( !tag_string )
      return 0;

    t1 = tag_string[0];
    if ( tag_string[1] )
      t2 = tag_string[1];
    if ( tag_string[2] )
      t3 = tag_string[2];
    if ( tag_string[3] )
      t4 = tag_string[3];

    return MAKE_TT_TAG( t1, t2, t3, t4 );
  }


  int  main( int     argc,
             char**  argv )
  {
    int    i, old_pt_size, orig_pt_size, file;

    int    graphics_initialized = 0;

    char   filename[128 + 4];
    char   alt_filename[128 + 4];
    char*  execname;

    int    option;
    int    res = 96;
    int    utf8 = 0;

    TT_Error  error;
    TEvent    event;


    execname = argv[0];

    while ( 1 )
    {
      option = ft_getopt( argc, argv, "c:f:gl:r:s:uvx" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'c':
        ttc_index = atoi( ft_optarg );
        if ( ttc_index < 0 )
          Usage( execname );
        break;

      case 'f':
        num_features++;
        feature_tag_strings = (char**)
                              realloc( feature_tag_strings,
                                       num_features * sizeof ( char* ) );
        feature_tags = (TT_ULong*)
                       realloc( feature_tags,
                                num_features * sizeof ( TT_ULong ) );
        feature_tag_strings[num_features - 1] = ft_optarg;
        if ( !(feature_tags[num_features - 1] = Make_Tag( ft_optarg ) ) )
          Usage( execname );
        break;

      case 'g':
        gray_render = 1;
        break;

      case 'l':
        language_tag_string = ft_optarg;
        if ( !(language_tag = Make_Tag( ft_optarg ) ) )
          Usage( execname );
        break;

      case 'r':
        res = atoi( ft_optarg );
        if ( res < 1 )
          Usage( execname );
        break;

      case 's':
        script_tag_string = ft_optarg;
        if ( !(script_tag = Make_Tag( ft_optarg ) ) )
          Usage( execname );
        break;

      case 'u':
        utf8 = 1;
        break;

      case 'v':
        vertical = 1;
        break;

      case 'x':
        r2l = 1;
        break;

      default:
        Usage( execname );
        break;
      }
    }

    argc -= ft_optind;
    argv += ft_optind;

    if ( argc <= 1 )
      Usage( execname );

    if ( sscanf( argv[0], "%d", &orig_pt_size ) != 1 )
      orig_pt_size = 64;

    file = 1;

    /* Initialize engine */

    error = TT_Init_FreeType( &engine );
    if ( error )
      Panic( "Error while initializing engine, code = 0x%x.\n", error );

    error = TT_Init_Kerning_Extension( engine );
    if ( error )
      Panic( "Error while initializing kerning extension, code = 0x%x.\n",
             error );

    error = TT_Init_SBit_Extension( engine );
    if ( error )
      Panic( "Error while initializing sbit extension, code = 0x%x.\n",
             error );

    error = TT_Init_GDEF_Extension( engine );
    if ( error )
      Panic( "Error while initializing GDEF extension, code = 0x%x.\n",
             error );

    error = TT_Init_GSUB_Extension( engine );
    if ( error )
      Panic( "Error while initializing GSUB extension, code = 0x%x.\n",
             error );

    pt_size  = orig_pt_size;
    hinted   = 1;
    use_gsub = 1;
    use_kern = 1;
    use_sbit = 1;

    i = strlen( argv[file] );
    while ( i > 0 && argv[file][i] != '\\' && argv[file][i] != '/' )
    {
      if ( argv[file][i] == '.' )
        i = 0;
        i--;
    }

    filename[128] = '\0';
    alt_filename[128] = '\0';

    strncpy( filename, argv[file], 128 );
    strncpy( alt_filename, argv[file], 128 );

    if ( i >= 0 )
    {
      strncpy( filename + strlen( filename ), ".ttf", 4 );
      strncpy( alt_filename + strlen( alt_filename ), ".ttc", 4 );
    }

    /* Load face */

    error = TT_Open_Face( engine, filename, &face );
    if ( error == TT_Err_Could_Not_Open_File )
    {
      strcpy( filename, alt_filename );
      error = TT_Open_Face( engine, alt_filename, &face );
    }
    if ( error == TT_Err_Could_Not_Open_File )
      Panic( "Could not find/open `%s'.\n", filename );
    else if ( error )
      Panic( "Error while opening `%s', code = 0x%x.\n",
             filename, error );

    /* get face properties and allocate preload arrays */

    TT_Get_Face_Properties( face, &face_properties );

    /* open font in collection */

    if ( ttc_index >= face_properties.num_Faces )
      Panic( "There is no collection with index %d in this font file.\n",
             ttc_index );

    TT_Close_Face( face );

    error = TT_Open_Collection( engine, filename, ttc_index, &face );
    if ( error )
      Panic( "Error while opening collection %d in `%s', code = 0x%x.\n",
             ttc_index, filename, error );

    /* create glyph */

    error = TT_New_Glyph( face, &glyph );
    if ( error )
      Panic( "Could not create glyph container, code = 0x%x.\n", error );

    /* create sbit slot */

    error = TT_New_SBit_Image( &sbit );
    if ( error )
      Panic( "Could not create sbit slot, code = 0x%x.\n" , error);

    /* create instance */

    error = TT_New_Instance( face, &instance );
    if ( error )
      Panic( "Could not create instance for `%s', code = 0x%x.\n",
             filename, error );

    error = TT_Set_Instance_Resolutions( instance, res, res );
    if ( error )
      Panic( "Could not set device resolutions, code = 0x%x.\n", error );

    error = TT_Get_Kerning_Directory( face, &directory );
    if ( error )
      Panic( "Could not get kerning directory, code = 0x%x.\n", error );

    /* load all kerning tables */

    for ( i = 0; i < directory.nTables; i++ )
    {
      error = TT_Load_Kerning_Table( face, i );
      if ( error )
        Panic( "Could not load kerning table, code = 0x%x.\n", error );
    }

    if ( directory.nTables )
      has_kern = 1;

    Select_CMap();

    /* GDEF support */

    gdef = &gdef_;

    error = TT_Load_GDEF_Table( face, gdef );
    if ( !error )
      has_gdef = 1;
    else if ( error != TT_Err_Table_Missing )
      Panic( "Error while loading GDEF table, code = 0x%x.\n", error );

    /* we must assign glyph properties in case no GDEF table is available */

    if ( !has_gdef )
    {
      Build_Arabic_Glyph_Properties( char_map, face_properties.num_Glyphs,
                                     &gdef );
      if ( gdef )
        has_gdef = 1;
    }
      
    /* GSUB support */

    gsub = &gsub_;

    error = TT_Load_GSUB_Table( face, gsub, gdef );
    if ( !error )
    {
      if ( script_tag && feature_tags )
        has_gsub = 1;
      if ( script_tag && language_tag )
        has_gsub = 1;
    }
    else if ( error != TT_Err_Table_Missing )
      Panic( "Error while loading GSUB table, code = 0x%x.\n", error );

    TT_GSUB_Clear_Features( gsub );

    if ( has_gsub && !language_tag )
      default_language_system = 1;

    feature_indices = (TT_UShort*)
                      malloc( num_features * sizeof ( TT_UShort ) );

    if ( has_gsub )
    {
      error = TT_GSUB_Select_Script( gsub,
                                     script_tag,
                                     &script_index );
      if ( error )
        Panic( "Requested script `%-4.4s' not found.\n",
               script_tag_string );

      if ( default_language_system )
      {
        for ( i = 0; i < num_features; i++ )
        {
          error = TT_GSUB_Select_Feature( gsub,
                                          feature_tags[i],
                                          script_index, 0xFFFF,
                                          &feature_indices[i] );
          if ( error )
            Panic( "Requested feature `%-4.4s'\n"
                   "for default language system of script `%-4.4s' not found.\n",
                   feature_tag_strings[i], script_tag_string );
        }
      }
      else
      {
        error = TT_GSUB_Select_Language( gsub,
                                         language_tag,
                                         script_index,
                                         &language_index,
                                         &req_feature_index );
        if ( error )
          Panic( "Requested language `%-4.4s'\n"
                 "for script `%-4.4s' not found.\n",
                 language_tag_string, script_tag_string );

        for ( i = 0; i < num_features; i++ )
        {
          error = TT_GSUB_Select_Feature( gsub,
                                          feature_tags[i],
                                          script_index, language_index,
                                          &feature_indices[i] );
          if ( error )
            Panic( "Requested feature `%-4.4s'\n"
                   "for script `%-4.4s', language `%-4.4s' not found.\n",
                   feature_tag_strings[i], script_tag_string,
                   language_tag_string );
        }
      }

      if ( req_feature_index != 0xFFFF )
        TT_GSUB_Add_Feature( gsub, req_feature_index, ALL_GLYPHS );
      else if ( !num_features )
        has_gsub = 0;

      for ( i = 0; i < num_features; i++ )
      {
        if ( feature_tags[i] == FEATURE_init )
          TT_GSUB_Add_Feature( gsub, feature_indices[i], initial );
        else if ( feature_tags[i] == FEATURE_medi )
          TT_GSUB_Add_Feature( gsub, feature_indices[i], medial );
        else if ( feature_tags[i] == FEATURE_fina )
          TT_GSUB_Add_Feature( gsub, feature_indices[i], final );
        else if ( feature_tags[i] == FEATURE_isol )
          TT_GSUB_Add_Feature( gsub, feature_indices[i], isolated );
        else
          TT_GSUB_Add_Feature( gsub, feature_indices[i], ALL_GLYPHS );
      }

      TT_GSUB_Register_Alternate_Function( gsub, alternate_function, NULL );
    }


    if ( !graphics_initialized )
    {
      graphics_initialized = 1;

      if ( gray_render )
      {
        if ( !SetGraphScreen( Graphics_Mode_Gray ) )
          Panic( "Could not set up grayscale graphics mode.\n" );

        TT_Set_Raster_Gray_Palette( engine, virtual_palette );
      }
      else
      {
        if ( !SetGraphScreen( Graphics_Mode_Mono ) )
          Panic( "Could not set up mono graphics mode.\n" );
      }
    }

    Init_Display( gray_render );

    Reset_Scale( pt_size );

    old_pt_size = pt_size;

    Fail = 0;

    /* get string to display, if any */

    if ( argv[2] )
    {
      if ( argv[2][0] == '-' )
      {
        int    ch;
        char*  p;


        char_string = (char*)malloc( 128 * sizeof ( char ) );
        p = char_string;

        for ( i = 0; i < 128; i++ )
        {
          ch = getchar();
          if ( ch == '\n' || ch == EOF )
          {
            *p = '\0';
            break;
          }

          *p++ = (char)ch;
        }

        *p = '\0';
      }
      else
        char_string = argv[2];
    }
    else
      char_string = "The quick brown fox jumps over the lazy dog";

    if ( utf8 )
      UTF8Char_To_Glyph( char_string );
    else
      Latin1Char_To_Glyph( char_string );

    /* we assign Arabic script features (e.g. `initial' or `final') */

    Assign_Arabic_Properties( char_code, properties, num_glyphs );

    for ( ;; )
    {
      int  key;


      Clear_Display();

      error = Render_All();
      if ( error )
        Panic( "Error while rendering string, code = 0x%x.\n", error );

      if ( gray_render )
        Convert_To_Display_Palette();

      sprintf( Header,
               "%s:  ptsize: %d  hinting: %s%s%s%s%s%s%s",
               ft_basename( filename ),
               pt_size,
               hinted ? "on" : "off",
               has_kern ? "  kerning: " : "",
               has_kern ? ( use_kern ? "on" : "off" ) : "",
               has_sbit ? "  sbit: " : "",
               has_sbit ? ( use_sbit ? "on" : "off" ) : "",
               has_gsub ? "  GSUB: " : "",
               has_gsub ? ( use_gsub ? "on" : "off" ) : "" );

      Display_Bitmap_On_Screen( Bit.bitmap, Bit.rows, Bit.cols );

#ifndef X11
#ifndef OS2
      Print_XY( 0, 0, Header );
#endif
#endif

      Get_Event( &event );
      if ( !( key = Process_Event( &event ) ) )
        goto Fin;

      if ( pt_size != old_pt_size )
      {
        if ( Reset_Scale( pt_size ) )
          Panic( "Could not resize font.\n" );

        old_pt_size = pt_size;
      }
    }

  Fin:
    RestoreScreen();

    TT_Done_FreeType( engine );

    printf( "Execution completed successfully.\n" );
    printf( "Fails = %d.\n", Fail );

    exit( EXIT_SUCCESS );      /* for safety reasons */

    return 0;       /* never reached */
}


/* End */
