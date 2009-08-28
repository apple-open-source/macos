
/*
 * Copyright (C) 2000-2002 Kare Sjolander <kare@speech.kth.se>
 *
 * This file is part of the Snack Sound Toolkit.
 * The latest version can be found at http://www.speech.kth.se/snack/
 *

 This file is derived from

 amp MPEG audio decoder (version 0.7.3)
 (C) Tomislav Uzelac  1996,1997

This software can be used freely for any purpose. It can be distributed
freely, as long as it is not sold commercially without permission from
Tomislav Uzelac <tuzelac@rasip.fer.hr>. However, including this software
on CD_ROMs containing other free software is explicitly permitted even
when a modest distribution fee is charged for the CD, as long as this
software is not a primary selling argument for the CD.

Building derived versions of this software is permitted, as long as they
are not sold commercially without permission from Tomislav Uzelac
<tuzelac@rasip.fer.hr>. Any derived versions must be clearly marked as
such, and must be called by a name other than amp. Any derived versions
must retain this copyright notice.
*/

#include <stdlib.h>
#include "snack.h"
#include "jkFormatMP3.h"
#include <string.h>
#define FRAS2(is,a) ((is) > 0 ? t_43[(is)]*(a):-t_43[-(is)]*(a))
#define MAXFRAMESIZE 2106  /* frame size starting at header */
#define roundf(x) (floor((x)+(float )0.5f))
static char *gblOutputbuf;
static char *gblReadbuf;
static int gblBufind = 0;
static Tcl_Channel gblGch;
static int fool_opt = 0;
extern int debugLevel;

extern int useOldObjAPI;
/* "getbits.c" */

static int
_fillbfr(int size)
{
  if (gblGch != NULL) {
    size = Tcl_Read(gblGch, (char *) _buffer, size);
  } else {
    memcpy((char *) _buffer, &gblReadbuf[gblBufind], size);
    gblBufind += size;
  }
  _bptr = 0;
  return(size);
}

static unsigned int
_getbits(int n)
{
  unsigned int pos,ret_value;

  pos = _bptr >> 3;
  ret_value = _buffer[pos] << 24 |
    _buffer[pos+1] << 16 |
    _buffer[pos+2] << 8 |
    _buffer[pos+3];
  ret_value <<= _bptr & 7;
  ret_value >>= 32 - n;
  _bptr += n;
  return ret_value;
}

static int
fillbfr(int advance)
{
  int overflow;

  if (gblGch != NULL) {
    int read = Tcl_Read(gblGch, (char *) &gblBuffer[gblAppend], advance);

    if (read <= 0) return(read);
  } else {
    memcpy((char *) &gblBuffer[gblAppend], &gblReadbuf[gblBufind], advance);
    gblBufind += advance;
  }

  if ( gblAppend + advance >= BUFFER_SIZE ) {
    overflow = gblAppend + advance - BUFFER_SIZE;
    memcpy (gblBuffer,&gblBuffer[BUFFER_SIZE], overflow);
    if (overflow < 4) memcpy(&gblBuffer[BUFFER_SIZE],gblBuffer,4);
    gblAppend = overflow;
  } else gblAppend+=advance;
  return advance;
}

/* these functions read from the buffer. a separate viewbits/flushbits
 * functions are there primarily for the new huffman decoding scheme
 */
static
unsigned int viewbits(int n)
{
  unsigned int pos,ret_value;

  pos = gblData >> 3;
  ret_value = gblBuffer[pos] << 24 |
    gblBuffer[pos+1] << 16 |
    gblBuffer[pos+2] << 8 |
    gblBuffer[pos+3];
  ret_value <<= gblData & 7;
  ret_value >>= 32 - n;

  return ret_value;
}

static void
sackbits(int n)
{
  gblData += n;
  gblData &= 8*BUFFER_SIZE-1;
}

static unsigned int
getbits(int n)
{
  if (n) {
    unsigned int ret_value;
    ret_value=viewbits(n);
    sackbits(n);
    return ret_value;
  } else
    return 0;
}
/*
 * gethdr reads and validates the current 4 bytes are a valid header.
 */
static int
gethdr(struct AUDIO_HEADER *header)
{
  int s,br;
  int bitrate,fs,mean_frame_size;
  /* check for frame sync first */
  if ((s=_getbits(11)) != 0x7ff) {
    return GETHDR_ERR;
  }
  header->fullID = _getbits(2);
  if (header->fullID == 1) {
    return GETHDR_ERR;  /* invalid ID */
  }
  header->ID    =header->fullID&0x1;
  header->layer=_getbits(2);
  /* only support layer3
  if (header->layer == 0) {
    return GETHDR_ERR;
  }
  */
  if (header->layer != 1) {
    return GETHDR_ERR;
  }
  header->protection_bit=_getbits(1);
  header->bitrate_index=_getbits(4);
  if (header->bitrate_index == 0xFF || header->bitrate_index == 0) {
    /* free bitrate=0 is also invalid */
    return GETHDR_ERR;
  }
  header->sampling_frequency=_getbits(2);
  if (header->sampling_frequency == 3) {
    return GETHDR_ERR;       /* 3 is invalid */
  }
  header->padding_bit=_getbits(1);
  header->private_bit=_getbits(1);
  header->mode=_getbits(2); /* channel mode */
  bitrate=t_bitrate[header->ID][3-header->layer][header->bitrate_index];
  fs=t_sampling_frequency[header->fullID][header->sampling_frequency];
  mean_frame_size = (bitrate * sr_lookup[header->ID] / fs);   /* This frame size */
  if (mean_frame_size > MAXFRAMESIZE) {
     return GETHDR_ERR;
  }
  /*
   * Validate bitrate channel mode combinations for layer 2
   * layer 2 not yet supported.
   */
  if (header->layer == 2) {
     br=bitrate;
     switch (header->mode) {
        case 0x00: /* stereo */
        case 0x01: /* intensity */
        case 0x02: /* dual channel */
           if (br==32 || br==48 || br==56 || br == 80) {
              if (debugLevel > 0) {
                 Snack_WriteLogInt("1 Invalid channel/mode combo",header->mode);
              }
               return GETHDR_ERR;
           };
           break;
        case 0x03: /* single channel */
           if (br>=224) {
              if (debugLevel > 0) {
                 Snack_WriteLogInt("2 Invalid channel/mode combo",header->mode);
              }
               return GETHDR_ERR;
           };
           break;
     }
  }

  header->mode_extension=_getbits(2);
  if (header->mode!=1) header->mode_extension=0; /* ziher je.. */
  header->copyright=_getbits(1);
  header->original=_getbits(1);
  header->emphasis=_getbits(2);
/*printf("gethdr %x %x %x %x\n",_buffer[0], _buffer[1],_buffer[2], _buffer[3]);*/
  return 0;
}

/* dummy function, to get crc out of the way
*/
static void
getcrc()
{
  _fillbfr(2);
  _getbits(16);
}

/* sizes of side_info:
 * MPEG1   1ch 17    2ch 32
 * MPEG2   1ch  9    2ch 17
 */
static void
getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info)
{
  int gr,ch,scfsi_band,region,window;
  int nch,bv;
  info->error[0]=0;
  info->error[1]=0;
  if (header->mode==3) {
    nch=1;
    if (header->ID) {
      _fillbfr(17);
      info->main_data_begin=_getbits(9);
      _getbits(5);
    } else {
      _fillbfr(9);
      info->main_data_begin=_getbits(8);
      _getbits(1);
    }
  } else {
    nch=2;
    if (header->ID) {
      _fillbfr(32);
      info->main_data_begin=_getbits(9);
      _getbits(3);
    } else {
      _fillbfr(17);
      info->main_data_begin=_getbits(8);
      _getbits(2);
    }
  }
  /* scalefactors to granules */
  if (header->ID) for (ch=0;ch<nch;ch++)
    for (scfsi_band=0;scfsi_band<4;scfsi_band++)
      info->scfsi[ch][scfsi_band]=_getbits(1);
  for (gr=0;gr<(header->ID ? 2:1);gr++)
    for (ch=0;ch<nch;ch++) {
     /*
      * Number of bits used for scalefactors (part2)
      * Huffman encoded data (part3) of the appropriate granule/channel
      */
      info->part2_3_length[gr][ch]=_getbits(12);
      if (info->part2_3_length[gr][ch] == 0 && debugLevel > 1) {
         Snack_WriteLogInt("  blank part 2/3 length gr=",gr);
      }
      bv = _getbits(9);
      info->global_gain[gr][ch]=_getbits(8);
      /* big_value can't be > 576 (288 << 1), usually denotes a stream error */
      if (bv > 288) {
         if (debugLevel > 0) {
            Snack_WriteLogInt("  Invalid big value ",bv);
            Snack_WriteLogInt("         on channel ",ch);
         }
         for (ch=0;ch<nch;ch++)
            info->error[ch] = 1; /* force error on all channels */
         info->big_values[gr][ch]=0;
      } else {
         info->big_values[gr][ch]=bv;
      }
      if (header->ID)
         info->scalefac_compress[gr][ch]=_getbits(4);
      else
         info->scalefac_compress[gr][ch]=_getbits(9);
      info->window_switching_flag[gr][ch]=_getbits(1);

      if (info->window_switching_flag[gr][ch]) {
        info->block_type[gr][ch]=_getbits(2);
        info->mixed_block_flag[gr][ch]=_getbits(1);

        for (region=0;region<2;region++)
          info->table_select[gr][ch][region]=_getbits(5);
        info->table_select[gr][ch][2]=0;

        for (window=0;window<3;window++)
          info->subblock_gain[gr][ch][window]=_getbits(3);
      } else {
        for (region=0;region<3;region++)
          info->table_select[gr][ch][region]=_getbits(5);

        info->region0_count[gr][ch]=_getbits(4);
        info->region1_count[gr][ch]=_getbits(3);
        info->block_type[gr][ch]=0;
      }

      if (header->ID) info->preflag[gr][ch]=_getbits(1);
      info->scalefac_scale[gr][ch]=_getbits(1);
      info->count1table_select[gr][ch]=_getbits(1);
    }
  return;
}

/* "getdata.c" */

/* layer3 scalefactor decoding. should we check for the number
 * of bits read, just in case?
 */
static int
decode_scalefactors(mp3Info *ext, struct SIDE_INFO *info,struct AUDIO_HEADER *header,int gr,int ch)
{
  int sfb,window;
  int slen1,slen2;
  int i1,i2,i=0;
  int j,k;
  if (header->ID==1) {
    /* this is MPEG-1 scalefactors format, quite different than
     * the MPEG-2 format.
     */
    slen1=t_slen1[info->scalefac_compress[gr][ch]];
    slen2=t_slen2[info->scalefac_compress[gr][ch]];
    i1=3*slen1;
    i2=3*slen2;

    if (info->window_switching_flag[gr][ch] && info->block_type[gr][ch]==2) {
      if (info->mixed_block_flag[gr][ch]) {
        for (sfb=0;sfb<8;sfb++) {
          ext->scalefac_l[gr][ch][sfb]=getbits(slen1);
          i+=slen1;
        }
        for (sfb=3;sfb<6;sfb++) {
          for (window=0;window<3;window++)
            ext->scalefac_s[gr][ch][sfb][window]=getbits(slen1);
          i+=i1;
        }
        for (;sfb<12;sfb++) {
          for (window=0;window<3;window++)
            ext->scalefac_s[gr][ch][sfb][window]=getbits(slen2);
          i+=i2;
        }
      } else { /* !mixed_block_flag */
        for (sfb=0;sfb<6;sfb++) {
          for (window=0;window<3;window++)
            ext->scalefac_s[gr][ch][sfb][window]=getbits(slen1);
          i+=i1;
        }
        for (;sfb<12;sfb++) {
          for (window=0;window<3;window++)
            ext->scalefac_s[gr][ch][sfb][window]=getbits(slen2);
          i+=i2;
        }
      }
      for (window=0;window<3;window++)
        ext->scalefac_s[gr][ch][12][window]=0;
    } else { /* block_type!=2 */
      if ( !info->scfsi[ch][0] || !gr )
        for (sfb=0;sfb<6;sfb++) {
          ext->scalefac_l[gr][ch][sfb]=getbits(slen1);
          i+=slen1;
        }
      else for (sfb=0;sfb<6;sfb++) {
        ext->scalefac_l[1][ch][sfb]=ext->scalefac_l[0][ch][sfb];
      }
      if ( !info->scfsi[ch][1] || !gr )
        for (sfb=6;sfb<11;sfb++) {
          ext->scalefac_l[gr][ch][sfb]=getbits(slen1);
          i+=slen1;
        }
      else for (sfb=6;sfb<11;sfb++) {
        ext->scalefac_l[1][ch][sfb]=ext->scalefac_l[0][ch][sfb];
      }
      if ( !info->scfsi[ch][2] || !gr )
        for (sfb=11;sfb<16;sfb++) {
          ext->scalefac_l[gr][ch][sfb]=getbits(slen2);
          i+=slen2;
        }
      else for (sfb=11;sfb<16;sfb++) {
        ext->scalefac_l[1][ch][sfb]=ext->scalefac_l[0][ch][sfb];
      }
      if ( !info->scfsi[ch][3] || !gr )
        for (sfb=16;sfb<21;sfb++) {
          ext->scalefac_l[gr][ch][sfb]=getbits(slen2);
          i+=slen2;
        }
      else for (sfb=16;sfb<21;sfb++) {
        ext->scalefac_l[1][ch][sfb]=ext->scalefac_l[0][ch][sfb];
      }
      ext->scalefac_l[gr][ch][21]=0;
    }
  } else { /* ID==0 */
    int index = 0,index2,spooky_index;
    int slen[5],nr_of_sfb[5]; /* actually, there's four of each, not five, labelled 1 through 4, but
                               * what's a word of storage compared to one's sanity. so [0] is irellevant.
                               */

    /* ok, so we got 3 indexes.
     * spooky_index - indicates whether we use the normal set of slen eqs and nr_of_sfb tables
     *                or the one for the right channel of intensity stereo coded frame
     * index        - corresponds to the value of scalefac_compress, as listed in the standard
     * index2       - 0 for long blocks, 1 for short wo/ mixed_block_flag, 2 for short with it
     */
    if ( (header->mode_extension==1 || header->mode_extension==3) && ch==1) { /* right ch... */
      int int_scalefac_compress=info->scalefac_compress[0][ch]>>1;
      ext->intensity_scale=info->scalefac_compress[0][1]&1;
      spooky_index=1;
      if (int_scalefac_compress < 180) {
        slen[1]=int_scalefac_compress/36;
        slen[2]=(int_scalefac_compress%36)/6;
        slen[3]=(int_scalefac_compress%36)%6;
        slen[4]=0;
        info->preflag[0][ch]=0;
        index=0;
      }
      if ( 180 <= int_scalefac_compress && int_scalefac_compress < 244) {
        slen[1]=((int_scalefac_compress-180)%64)>>4;
        slen[2]=((int_scalefac_compress-180)%16)>>2;
        slen[3]=(int_scalefac_compress-180)%4;
        slen[4]=0;
        info->preflag[0][ch]=0;
        index=1;
      }
      if ( 244 <= int_scalefac_compress && int_scalefac_compress < 255) {
        slen[1]=(int_scalefac_compress-244)/3;
        slen[2]=(int_scalefac_compress-244)%3;
        slen[3]=0;
        slen[4]=0;
        info->preflag[0][ch]=0;
        index=2;
      }
    } else { /* the usual */
      spooky_index=0;
      if (info->scalefac_compress[0][ch] < 400) {
        slen[1]=(info->scalefac_compress[0][ch]>>4)/5;
        slen[2]=(info->scalefac_compress[0][ch]>>4)%5;
        slen[3]=(info->scalefac_compress[0][ch]%16)>>2;
        slen[4]=info->scalefac_compress[0][ch]%4;
        info->preflag[0][ch]=0;
        index=0;
      }
      if (info->scalefac_compress[0][ch] >= 400 && info->scalefac_compress[0][ch] < 500) {
        slen[1]=((info->scalefac_compress[0][ch]-400)>>2)/5;
        slen[2]=((info->scalefac_compress[0][ch]-400)>>2)%5;
        slen[3]=(info->scalefac_compress[0][ch]-400)%4;
        slen[4]=0;
        info->preflag[0][ch]=0;
        index=1;
      }
      if (info->scalefac_compress[0][ch] >= 500 && info->scalefac_compress[0][ch] < 512) {
        slen[1]=(info->scalefac_compress[0][ch]-500)/3;
        slen[2]=(info->scalefac_compress[0][ch]-500)%3;
        slen[3]=0;
        slen[4]=0;
        info->preflag[0][ch]=1;
        index=2;
      }
    }

    if (info->window_switching_flag[0][ch] && info->block_type[0][ch]==2)
      if (info->mixed_block_flag[0][ch]) index2=2;
      else index2=1;
    else index2=0;

    for (j=1;j<=4;j++) nr_of_sfb[j]=spooky_table[spooky_index][index][index2][j-1];

    /* now we'll do some actual scalefactor extraction, and a little more.
     * for each scalefactor band we'll set the value of ext->is_max to indicate
     * illegal is_pos, since with MPEG2 it's not 'hardcoded' to 7.
     */
    if (!info->window_switching_flag[0][ch] || (info->window_switching_flag[0][ch] && info->block_type[0][ch]!=2)) {
      sfb=0;
      for (j=1;j<=4;j++) {
        for (k=0;k<nr_of_sfb[j];k++) {
          ext->scalefac_l[0][ch][sfb]=getbits(slen[j]);
          i+=slen[j];
          if (ch) ext->is_max[sfb]=(1<<slen[j])-1;
          sfb++;
        }
      }
      /* There may be a bug here, sfb is left at 21
         indicating there are 21 scale factors,
         last one is ?always? blank and
         later causes a crash if garbage
         scalefac_l[0][ch][21] is not used.
      */
    } else if (info->block_type[0][ch]==2) {
      if (!info->mixed_block_flag[0][ch]) {
        sfb=0;
        for (j=1;j<=4;j++) {
          for (k=0;k<nr_of_sfb[j];k+=3) {
            /* we assume here that nr_of_sfb is divisible by 3. it is.
             */
            ext->scalefac_s[0][ch][sfb][0]=getbits(slen[j]);
            ext->scalefac_s[0][ch][sfb][1]=getbits(slen[j]);
            ext->scalefac_s[0][ch][sfb][2]=getbits(slen[j]);
            i+=3*slen[j];
            if (ch) ext->is_max[sfb+6]=(1<<slen[j])-1;
            sfb++;
          }
        }
      } else {
        /* what we do here is:
         * 1. assume that for every fs, the two lowest subbands are equal to the
         *    six lowest scalefactor bands for long blocks/MPEG2. they are.
         * 2. assume that for every fs, the two lowest subbands are equal to the
         *    three lowest scalefactor bands for short blocks. they are.
         */
        sfb=0;
        for (k=0;k<6;k++) {
          ext->scalefac_l[0][ch][sfb]=getbits(slen[1]);
          i+=slen[j];
          if (ch) ext->is_max[sfb]=(1<<slen[1])-1;
          sfb++;
        }
        nr_of_sfb[1]-=6;
        sfb=3;
        for (j=1;j<=4;j++) {
          for (k=0;k<nr_of_sfb[j];k+=3) {
            ext->scalefac_s[0][ch][sfb][0]=getbits(slen[j]);
            ext->scalefac_s[0][ch][sfb][1]=getbits(slen[j]);
            ext->scalefac_s[0][ch][sfb][2]=getbits(slen[j]);
            i+=3*slen[j];
            if (ch) ext->is_max[sfb+6]=(1<<slen[j])-1;
            sfb++;
          }
        }
      }
    }
  }
  return i;
}

/* this is for huffman decoding, but inlined funcs have to go first
 */
static int
_qsign(int x,int *q)
{
  int ret_value=0,i;
  for (i=3;i>=0;i--)
    if ((x>>i) & 1) {
      if (getbits(1)) *q++=-1;
      else *q++=1;
      ret_value++;
    }
    else *q++=0;
  return ret_value;
}

static int
decode_huffman_data(mp3Info *ext,struct SIDE_INFO *info,int gr,int ch,int ssize)
{
  int l,i,cnt,x=0,y=0, cmp=0;
  int q[4],r[3],linbits[3],tr[4]={0,0,0,0};
  int big_value = info->big_values[gr][ch] << 1;
  for (l=0;l<3;l++) {
    tr[l]=info->table_select[gr][ch][l];
    linbits[l]=t_linbits[info->table_select[gr][ch][l]];
  }

  tr[3]=32+info->count1table_select[gr][ch];

  /* we have to be careful here because big_values are not necessarily
   * aligned with sfb boundaries
   */
  if (!info->window_switching_flag[gr][ch] && info->block_type[gr][ch]==0) {

    /* this code needed some cleanup
     */
    r[0]=ext->t_l[info->region0_count[gr][ch]] + 1;
    if (r[0] > big_value)
      r[0]=r[1]=big_value;
    else {
      r[1]=ext->t_l[ info->region0_count[gr][ch] + info->region1_count[gr][ch] + 1 ] + 1;
      if (r[1] > big_value)
        r[1]=big_value;
    }
    r[2]=big_value;

  } else {

    if (info->block_type[gr][ch]==2 && info->mixed_block_flag[gr][ch]==0)
      r[0]=3*(ext->t_s[2]+1);
    else
      r[0]=ext->t_l[7]+1;

    if (r[0] > big_value)
      r[0]=big_value;

    r[1]=r[2]=big_value;
  }

  l=0; cnt=0;
  for (i=0;i<3;i++) {
    for (;l<r[i];l+=2) {
      int j = linbits[i];

      cnt+=huffman_decode(tr[i],&x,&y);

      if (x==15 && j>0) {
        x+=getbits(j);
        cnt+=j;
      }
      if (x) {
        if (getbits(1)) x=-x;
        cnt++;
      }
      if (y==15 && j>0) {
        y+=getbits(j);
        cnt+=j;
      }
      if (y) {
        if (getbits(1)) y=-y;
        cnt++;
      }

      /*      if (SHOW_HUFFBITS) printf(" (%d,%d) %d\n",x,y, SHOW_HUFFBITS);*/
      ext->is[ch][l]=x;
      ext->is[ch][l+1]=y;
    }
  }
  cmp = (info->part2_3_length[gr][ch] - ssize);
  while ((cnt < cmp) && (l<576)) {
    cnt+=huffman_decode(tr[3],&x,&y);
    cnt+=_qsign(x,q);
    for (i=0;i<4;i++)
      ext->is[ch][l+i]=q[i]; /* ziher je ziher, is[578]*/
    l+=4;
    /* if (SHOW_HUFFBITS) printf(" (%d,%d,%d,%d)\n",q[0],q[1],q[2],q[3]); */
  }
  /*
   * If we detected an error in the header for this frame, blank it out
   * to prevent audible spikes.
   */
   if (info->error[ch]) {
      if (debugLevel > 0) {
         Snack_WriteLogInt ("  blanking gain",cnt-cmp);
      }
      info->global_gain[gr][ch]=0;
   } else {
     /*
      * Debug code to print out excessive mismatches in bits
      */
      if (cnt > cmp) {
         if ((cnt - cmp) > 100) {
            if (debugLevel > 0)
               Snack_WriteLogInt ("  BITS DISCARDED",cnt - cmp);
         }
      }
      else if (cnt < cmp) {
         if ((cmp-cnt) > 800) {
            if (debugLevel > 0) {
               Snack_WriteLogInt ("  BITS NOT USED",cmp - cnt);
               Snack_WriteLogInt ("           GAIN",info->global_gain[gr][ch]);
            }

         }
      }
   }
  /*  set position to start of the next gr/ch, only needed on a mismatch
   */
  if (cnt != cmp ) {
    gblData-=cnt-cmp;
    gblData&= 8*BUFFER_SIZE - 1;
  }
  if (l<576) {
     ext->non_zero[ch]=l;
     /* zero out everything else
      */
     for (;l<576;l++)
        ext->is[ch][l]=0;
  } else {
     ext->non_zero[ch]=576;
  }
  return 1;
}

/*
 * fras == Formula for Requantization and All Scaling **************************
 */
static float
fras_l(int sfb,int global_gain,int scalefac_scale,int scalefac,int preflag)
{
  int a,scale;
  if (scalefac_scale) scale=2;
  else scale=1;
  a=global_gain - 210 - (scalefac << scale);
  if (preflag) a-=(t_pretab[sfb] << scale);

  /* bugfix, Mar 13 97: shifting won't produce a legal result if we shift by more than 31
   * since global_gain<256, this can only occur for (very) negative values of a.
   */
  if (a < -127) return 0;

  /* a minor change here as well, no point in abs() if we now that a<0
   */
  if (a>=0) return tab[a&3]*(1 << (a>>2));
  else return tabi[(-a)&3]/(1 << ((-a) >> 2));
}

static float
fras_s(int global_gain,int subblock_gain,int scalefac_scale,int scalefac)
{
  int a;
  a=global_gain - 210 - (subblock_gain << 3);
  if (scalefac_scale) a-= (scalefac << 2);
  else a-= (scalefac << 1);

  if (a < -127) return 0;

  if (a>=0) return tab[a&3]*(1 << (a>>2));
  else return tabi[(-a)&3]/(1 << ((-a) >> 2));
}

/* this should be faster than pow()
 */
/*
static float
fras2(int is,float a)
{
  if (is > 0) return t_43[is]*a;
  else return -t_43[-is]*a;
}
*/
/*
 * requantize_mono *************************************************************
 */

/* generally, the two channels do not have to be of the same block type - that's why we do two passes with requantize_mono.
 * if ms or intensity stereo is enabled we do a single pass with requantize_ms because both channels are same block type
 */

static void
requantize_mono(mp3Info *ext, int gr,int ch,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
  int l,i,sfb;
  float a;
  int global_gain=info->global_gain[gr][ch];
  int scalefac_scale=info->scalefac_scale[gr][ch];
  int sfreq=header->sampling_frequency;
  /* TFW: sfreq must be less than 3, used as an index into t_reorder */
  if (sfreq >= 3) return;

  /* TFW - Note: There needs to be error checking here on header info */
  no_of_imdcts[0]=no_of_imdcts[1]=32;

  if (info->window_switching_flag[gr][ch] && info->block_type[gr][ch]==2)
    if (info->mixed_block_flag[gr][ch]) {
      /*
       * requantize_mono - mixed blocks/long block part **********************
       */
      int window,window_len,preflag=0; /* pretab is all zero in this low frequency area */
      int scalefac=ext->scalefac_l[gr][ch][0];

      l=0;sfb=0;
      a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
      while (l<36) {
        ext->xr[ch][0][l]=FRAS2(ext->is[ch][l],a);
        if (l==ext->t_l[sfb]) {
          scalefac=ext->scalefac_l[gr][ch][++sfb];
          a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
        }
        l++;
      }
      /*
       * requantize_mono - mixed blocks/short block part *********************
       */
      sfb=3;
      window_len=ext->t_s[sfb]-ext->t_s[sfb-1];
      while (l<ext->non_zero[ch]) {
        for (window=0;window<3;window++) {
          int scalefac=ext->scalefac_s[gr][ch][sfb][window];
          int subblock_gain=info->subblock_gain[gr][ch][window];
          a=fras_s(global_gain,subblock_gain,scalefac_scale,scalefac);
          for (i=0;i<window_len;i++) {
            ext->xr[ch][0][t_reorder[header->ID][sfreq][l]]=FRAS2(ext->is[ch][l],a);
            l++;
          }
        }
        sfb++;
        window_len=ext->t_s[sfb]-ext->t_s[sfb-1];
      }
      while (l<576) ext->xr[ch][0][t_reorder[header->ID][sfreq][l++]]=0;
    } else {
      /*
       * requantize mono - short blocks **************************************
       */
      int window,window_len;

      sfb=0; l=0;
      window_len=ext->t_s[0]+1;
      while (l<ext->non_zero[ch]) {
        for (window=0;window<3;window++) {
          int scalefac=ext->scalefac_s[gr][ch][sfb][window];
          int subblock_gain=info->subblock_gain[gr][ch][window];
          float a=fras_s(global_gain,subblock_gain,scalefac_scale,scalefac);
          for (i=0;i<window_len;i++) {
            ext->xr[ch][0][t_reorder[header->ID][sfreq][l]]=FRAS2(ext->is[ch][l],a);
            l++;
          }
        }
        sfb++;
        window_len=ext->t_s[sfb]-ext->t_s[sfb-1];
      }
      while (l<576) ext->xr[ch][0][t_reorder[header->ID][sfreq][l++]]=0;
    }
  else {
    /* long blocks */
    int preflag=info->preflag[gr][ch];
    int scalefac=ext->scalefac_l[gr][ch][0];

    sfb=0; l=0;
    a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
    while (l<ext->non_zero[ch]) {
      ext->xr[ch][0][l]=FRAS2(ext->is[ch][l],a);
      if (l==ext->t_l[sfb]) {
        scalefac=ext->scalefac_l[gr][ch][++sfb];
        a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
      }
      l++;
    }
    while (l<576) ext->xr[ch][0][l++]=0;
  }
}

/*
 * stereo stuff ****************************************************************
 */
static int
find_isbound(mp3Info *ext, int isbound[3],int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
  int sfb,window,window_len,ms_flag,tmp,i;

  isbound[0]=isbound[1]=isbound[2]=-1;
  no_of_imdcts[0]=no_of_imdcts[1]=32;

  if (header->mode_extension==1 || header->mode_extension==3) {
    if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2) {

      /* find that isbound!
       */
      tmp=ext->non_zero[1];
      sfb=0; while ((3*ext->t_s[sfb]+2) < tmp  && sfb < 12) sfb++;
      while ((isbound[0]<0 || isbound[1]<0 || isbound[2]<0) && !(info->mixed_block_flag[gr][0] && sfb<3) && sfb) {
        for (window=0;window<3;window++) {
          if (sfb==0) {
            window_len=ext->t_s[0]+1;
            tmp=(window+1)*window_len - 1;
          } else {
            window_len=ext->t_s[sfb]-ext->t_s[sfb-1];
            tmp=(3*ext->t_s[sfb-1]+2) + (window+1)*window_len;
          }
          if (isbound[window] < 0)
            for (i=0;i<window_len;i++)
              if (ext->is[1][tmp--] != 0) {
                isbound[window]=ext->t_s[sfb]+1;
                break;
              }
        }
        sfb--;
      }

      /* mixed block magic now...
       */
      if (sfb==2 && info->mixed_block_flag[gr][0]) {
        if (isbound[0]<0 && isbound[1]<0 && isbound[2]<0) {
          tmp=35;
          while (ext->is[1][tmp] == 0) tmp--;
          sfb=0; while (ext->t_l[sfb] < tmp  && sfb < 21) sfb++;
          isbound[0]=isbound[1]=isbound[2]=ext->t_l[sfb]+1;
        } else for (window=0;window<3;window++)
          if (isbound[window]<0) isbound[window]=36;
      }
      if (header->ID==1) isbound[0]=isbound[1]=isbound[2]=max(isbound[0],max(isbound[1],isbound[2]));

      /* just how many imdcts?
       */
      tmp=ext->non_zero[0];
      sfb=0; while ((3*ext->t_s[sfb]+2) < tmp && sfb < 12) sfb++;
      no_of_imdcts[0]=no_of_imdcts[1]=(ext->t_s[sfb]-1)/6+1; /* 18?????? */

    } else {

      /* long blocks now
       */
      tmp=ext->non_zero[1];
      while (ext->is[1][tmp] == 0) tmp--;
      sfb=0; while (ext->t_l[sfb] < tmp && sfb < 21) sfb++;
      isbound[0]=isbound[1]=isbound[2]=ext->t_l[sfb]+1;
      no_of_imdcts[0]=no_of_imdcts[1]=(ext->non_zero[0]-1)/18+1; /* left channel should have more elements here */
    }
    if (header->mode_extension==1) ms_flag=0;
    else ms_flag=1;
  } else {

    /* intensity stereo is, regretably, turned off
     */
    ms_flag=1;

    /* i really put a lot of work in this, but it still looks like shit (works, though)
     */
    if (!info->window_switching_flag[gr][0] || (info->window_switching_flag[gr][0] && info->block_type[gr][0]!=2))
      isbound[0]=isbound[1]=isbound[2]=(max(ext->non_zero[0],ext->non_zero[1]));
    else isbound[0]=isbound[1]=isbound[2]=576;

    if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2) {
      /* should do for mixed blocks too, though i havent tested... */
      tmp=(max(ext->non_zero[0],ext->non_zero[1]))/3;
      sfb=0; while (ext->t_s[sfb]<tmp && sfb<12) sfb++;
      no_of_imdcts[0]=no_of_imdcts[1]=(ext->t_s[sfb]-1)/6+1;
    }
    else no_of_imdcts[0]=no_of_imdcts[1]=(isbound[0]-1)/18+1;

  }

  return ms_flag;
}

static void
stereo_s(mp3Info *ext, int l,float a[2],int pos,int ms_flag,int is_pos,struct AUDIO_HEADER *header)
{
  float ftmp,Mi,Si;

  if (l>=576) {
     if (debugLevel > 0) {
        Snack_WriteLogInt("stereo_s: big value too big",l);
     }
     return; /* brrr... */
  }

  if ((is_pos != IS_ILLEGAL) && (header->ID==1)) {
    ftmp=FRAS2(ext->is[0][l],a[0]);
    ext->xr[0][0][pos]=(1-t_is[is_pos])*ftmp;
    ext->xr[1][0][pos]=t_is[is_pos]*ftmp;
    return;
  }

  if ((is_pos != IS_ILLEGAL) && (header->ID==0)) {
    ftmp=FRAS2(ext->is[0][l],a[0]);
    if (is_pos&1) {
      ext->xr[0][0][pos]= t_is2[ext->intensity_scale][(is_pos+1)>>1] * ftmp;
      ext->xr[1][0][pos]= ftmp;
    } else {
      ext->xr[0][0][pos]= ftmp;
      ext->xr[1][0][pos]= t_is2[ext->intensity_scale][is_pos>>1] * ftmp;
    }
    return;
  }

  if (ms_flag) {
    Mi=FRAS2(ext->is[0][l],a[0]);
    Si=FRAS2(ext->is[1][l],a[1]);
    ext->xr[0][0][pos]=(float) ((Mi+Si)*i_sq2);
    ext->xr[1][0][pos]=(float) ((Mi-Si)*i_sq2);
  } else {
    ext->xr[0][0][pos]=FRAS2(ext->is[0][l],a[0]);
    ext->xr[1][0][pos]=FRAS2(ext->is[1][l],a[1]);
  }
}

static void
stereo_l(mp3Info *ext, int l,float a[2],int ms_flag,int is_pos,struct AUDIO_HEADER *header)
{
  float ftmp,Mi,Si;
  if (l>=576) {
     if (debugLevel > 0) {
        Snack_WriteLogInt("stereo_s: big value too big",l);
     }
     return;
  }

  if ((is_pos != IS_ILLEGAL) && (header->ID==1)) {
    ftmp=FRAS2(ext->is[0][l],a[0]);
    ext->xr[0][0][l]=(1-t_is[is_pos])*ftmp;
    ext->xr[1][0][l]=t_is[is_pos]*ftmp;
    return;
  }

  if ((is_pos != IS_ILLEGAL) && (header->ID==0)) {
    ftmp=FRAS2(ext->is[0][l],a[0]);
    if (is_pos&1) {
      ext->xr[0][0][l]= t_is2[ext->intensity_scale][(is_pos+1)>>1] * ftmp;
      ext->xr[1][0][l]= ftmp;
    } else {
      ext->xr[0][0][l]= ftmp;
      ext->xr[1][0][l]= t_is2[ext->intensity_scale][is_pos>>1] * ftmp;
    }
    return;
  }

  if (ms_flag) {
    Mi=FRAS2(ext->is[0][l],a[0]);
    Si=FRAS2(ext->is[1][l],a[1]);
    ext->xr[0][0][l]=(float) ((Mi+Si)*i_sq2);
    ext->xr[1][0][l]=(float) ((Mi-Si)*i_sq2);
  } else {
    ext->xr[0][0][l]=FRAS2(ext->is[0][l],a[0]);
    ext->xr[1][0][l]=FRAS2(ext->is[1][l],a[1]);
  }

}
/*
#ifdef WIN
#pragma optimize("", off)
#endif
*/
/* requantize_ms */

static void
requantize_ms(mp3Info *ext, int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
  int l,sfb,ms_flag,is_pos,i,ch;
  int *global_gain,subblock_gain[2],*scalefac_scale,scalefac[2]={0,0},isbound[3];
  int sfreq=header->sampling_frequency;
  int id = header->ID;
  float a[2];

  global_gain=info->global_gain[gr];
  scalefac_scale=info->scalefac_scale[gr];
  if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2)
    if (info->mixed_block_flag[gr][0]) {
      /*
       * mixed blocks w/stereo processing - long block part ******************
       */
      int window,window_len;
      int preflag[2]={0,0};

      ms_flag=find_isbound(ext, isbound,gr,info,header);

      sfb=0; l=0;
      for (ch=0;ch<2;ch++) {
        scalefac[ch]=ext->scalefac_l[gr][ch][0];
        a[ch]=fras_l(0,global_gain[ch],scalefac_scale[ch],scalefac[ch],preflag[ch]);
      }


      while (l<36) {
        int is_pos;
        if (l<isbound[0]) is_pos=IS_ILLEGAL;
        else {
          is_pos=scalefac[1];
          if (id==1) { /* MPEG1 */
            if (is_pos==7) is_pos=IS_ILLEGAL;
            else /* MPEG2 */
              if (is_pos==ext->is_max[sfb]) is_pos=IS_ILLEGAL;
          }
        }

        stereo_l(ext, l,a,ms_flag,is_pos,header);

        if (l==ext->t_l[sfb]) {
          sfb++;
          for (ch=0;ch<2;ch++) {
            scalefac[ch]=ext->scalefac_l[gr][ch][sfb];
            a[ch]=fras_l(sfb,global_gain[ch],scalefac_scale[ch],scalefac[ch],preflag[ch]);
          }
        }

        l++;
      }
      /*
       * mixed blocks w/stereo processing - short block part *****************
       */
      sfb=3;
      window_len=ext->t_s[sfb]-ext->t_s[sfb-1];

      while (l<(max(ext->non_zero[0],ext->non_zero[1]))) {
        for (window=0;window<3;window++) {
          subblock_gain[0]=info->subblock_gain[gr][0][window];
          subblock_gain[1]=info->subblock_gain[gr][1][window];
          scalefac[0]=ext->scalefac_s[gr][0][sfb][window];
          scalefac[1]=ext->scalefac_s[gr][1][sfb][window];

          if (ext->t_s[sfb] < isbound[window]) {
            is_pos=IS_ILLEGAL;
            a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
            a[1]=fras_s(global_gain[1],subblock_gain[1],scalefac_scale[1],scalefac[1]);
          } else {
            is_pos=scalefac[1];
            if (id==1) { /* MPEG1 */
              if (is_pos==7) is_pos=IS_ILLEGAL;
              else /* MPEG2 */
                if (is_pos==ext->is_max[sfb+6]) is_pos=IS_ILLEGAL;
            }

            a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
          }

          for (i=0;i<window_len && l < 576;i++) {
            stereo_s(ext, l,a,t_reorder[id][sfreq][l],ms_flag,is_pos,header);
            l++;
          }
        }
        sfb++;
        window_len=ext->t_s[sfb]-ext->t_s[sfb-1];
      }
      while (l<576) {
        int reorder = t_reorder[id][sfreq][l++];
        ext->xr[0][0][reorder]=ext->xr[1][0][reorder]=0;
      }
    } else {
      /*
       * requantize_ms - short blocks w/stereo processing ********************
       */
      int window,window_len;

      ms_flag=find_isbound(ext, isbound,gr,info,header);
      sfb=0; l=0;
      window_len=ext->t_s[0]+1;

      while (l<(max(ext->non_zero[0],ext->non_zero[1]))) {
        for (window=0;window<3;window++) {
          subblock_gain[0]=info->subblock_gain[gr][0][window];
          subblock_gain[1]=info->subblock_gain[gr][1][window];

          scalefac[0]=ext->scalefac_s[gr][0][sfb][window]&0x3F; /* clamp values to < 63 */
          scalefac[1]=ext->scalefac_s[gr][1][sfb][window]&0x3F;

          if (ext->t_s[sfb] < isbound[window]) {
            is_pos=IS_ILLEGAL;
            a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
            a[1]=fras_s(global_gain[1],subblock_gain[1],scalefac_scale[1],scalefac[1]);
          } else {
            is_pos=scalefac[1];
            if (id==1) { /* MPEG1 */
              if (is_pos==7) is_pos=IS_ILLEGAL;
              else /* MPEG2 */
                if (is_pos==ext->is_max[sfb+6]) is_pos=IS_ILLEGAL;
            }

            a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
          }

          for (i=0;i<window_len && l < 576;i++) {
            stereo_s(ext, l,a,t_reorder[id][sfreq][l],ms_flag,is_pos,header);
            l++;
          }
        }
        window_len=-ext->t_s[sfb]+ext->t_s[sfb+1];
        sfb++;
      }
      while (l<576) {
        int reorder = t_reorder[id][sfreq][l++];

        ext->xr[0][0][reorder]=ext->xr[1][0][reorder]=0;
      }
    }
  else {
    /*
     * long blocks w/stereo processing *************************************
     */
    int *preflag=info->preflag[gr];

    ms_flag=find_isbound(ext, isbound,gr,info,header);

    sfb=0; l=0;
    scalefac[0]=ext->scalefac_l[gr][0][sfb];
    a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
    scalefac[1]=ext->scalefac_l[gr][1][sfb];
    a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);
    while (l< isbound[0]) {
      int is_pos=IS_ILLEGAL;
      stereo_l(ext, l,a,ms_flag,is_pos,header);
      if (l==ext->t_l[sfb]) {
        sfb++;
        scalefac[0]=ext->scalefac_l[gr][0][sfb];
        a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
        scalefac[1]=ext->scalefac_l[gr][1][sfb];
        a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);
      }
      l++;
    }
    while (l<(max(ext->non_zero[0],ext->non_zero[1]))) {
      int is_pos=scalefac[1];
      if (id==1) { /* MPEG1 */
        if (is_pos==7) is_pos=IS_ILLEGAL;
        else /* MPEG2 */
          if (is_pos==ext->is_max[sfb]) is_pos=IS_ILLEGAL;
      }

      stereo_l(ext, l,a,ms_flag,is_pos,header);
      if (l==ext->t_l[sfb]) {
        sfb++;
        scalefac[0]=ext->scalefac_l[gr][0][sfb];
        scalefac[1]=ext->scalefac_l[gr][1][sfb];
        a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
      }
      l++;
    }
    while (l<576) {
      ext->xr[0][0][l]=0;
      ext->xr[1][0][l]=0;
      l++;
    }
  }
}
/*
#ifdef WIN
#pragma optimize("", on)
#endif
*/
/*
 * antialiasing butterflies
 *
 */
static void
alias_reduction(mp3Info *ext, int ch)
{
  int sb,i;

  for (sb=1;sb<32;sb++) {
    float *x = ext->xr[ch][sb];

    for (i=0;i<8;i++) {
      float a = x[i];
      float b = x[-1-i];
      x[-1-i] = b * Cs[i] - a * Ca[i];
      x[i]    = a * Cs[i] + b * Ca[i];
    }
  }
}

/* huffman_decode() is supposed to be faster now
 * decodes one codeword and returns no. of bits
 */
static int
huffman_decode(int tbl,int *x,int *y)
{
  unsigned int chunk;
  register unsigned int *h_tab;
  register unsigned int lag;
  register unsigned int half_lag;
  int len;

  h_tab=tables[tbl];
  chunk=viewbits(19);

  h_tab += h_cue[tbl][chunk >> (19-NC_O)];

  /* TFW: Errors in file can cause h_tab to be null,
     return immediatly with 0 len in this case */
  if (h_tab==0)
     return 0;

  len=(*h_tab>>8)&0x1f;

  /* check for an immediate hit, so we can decode those short codes very f
     ast
     */
  if ((*h_tab>>(32-len)) != (chunk>>(19-len))) {
    if (chunk >> (19-NC_O) < N_CUE-1)
      lag=(h_cue[tbl][(chunk >> (19-NC_O))+1] -
           h_cue[tbl][chunk >> (19-NC_O)]);
    else {
      /* we strongly depend on h_cue[N_CUE-1] to point to
       * the last entry in the huffman table, so we should
       * not get here anyway. if it didn't, we'd have to
       * have another table with huffman tables lengths, and
       * it would be a mess. just in case, scream&shout.
       */
      /*      printf(" h_cue clobbered. this is a bug. blip.\n");*/
      exit (-1);
    }
    chunk <<= 32-19;
    chunk |= 0x1ff;

    half_lag = lag >> 1;

    h_tab += half_lag;
    lag -= half_lag;

    while (lag > 1) {
      half_lag = lag >> 1;

      if (*h_tab < chunk)
        h_tab += half_lag;
      else
        h_tab -= half_lag;

      lag -= half_lag;
    }

    len=(*h_tab>>8)&0x1f;
    if ((*h_tab>>(32-len)) != (chunk>>(32-len))) {
      if (*h_tab > chunk)
        h_tab--;
      else
        h_tab++;

      len=(*h_tab>>8)&0x1f;
    }
  }
  sackbits(len);
  *x=(*h_tab>>4)&0xf;
  *y=*h_tab&0xf;
  return len;
}

#include <math.h>

#define PI12      0.261799387f
#define PI36      0.087266462f
#define COSPI3    0.500000000f
#define COSPI6    0.866025403f
#define DCTODD1   0.984807753f
#define DCTODD2  -0.342020143f
#define DCTODD3  -0.642787609f
#define DCTEVEN1  0.939692620f
#define DCTEVEN2 -0.173648177f
#define DCTEVEN3 -0.766044443f

static void
imdct_init()
{
  int i;

  for(i=0;i<36;i++) /* 0 */
    win[0][i] = (float) sin(PI36 *(i+0.5));
  for(i=0;i<18;i++) /* 1 */
    win[1][i] = (float) sin(PI36 *(i+0.5));
  for(i=18;i<24;i++)
    win[1][i] = 1.0f;
  for(i=24;i<30;i++)
    win[1][i] = (float) sin(PI12 *(i+0.5-18));
  for(i=30;i<36;i++)
    win[1][i] = 0.0f;
  for(i=0;i<6;i++) /* 3 */
    win[3][i] = 0.0f;
  for(i=6;i<12;i++)
    win[3][i] = (float) sin(PI12 * (i+ 0.5 - 6.0));
  for(i=12;i<18;i++)
    win[3][i] = 1.0f;
  for(i=18;i<36;i++)
    win[3][i] = (float) sin(PI36 * (i + 0.5));
}

/*
This uses Byeong Gi Lee's Fast Cosine Transform algorithm to
decompose the 36 point and 12 point IDCT's into 9 point and 3
point IDCT's, respectively. Then the 9 point IDCT is computed
by a modified version of Mikko Tommila's IDCT algorithm, based on
the WFTA. See his comments before the first 9 point IDCT. The 3
point IDCT is already efficient to implement. -- Jeff Tsay. */

void imdct(mp3Info *ext,int win_type,int sb,int ch) {
  /*------------------------------------------------------------------*/
  /*                                                                  */
  /*    Function: Calculation of the inverse MDCT                     */
  /*    In the case of short blocks the 3 output vectors are already  */
  /*    overlapped and added in this modul.                           */
  /*                                                                  */
  /*    New layer3                                                    */
  /*                                                                  */
  /*------------------------------------------------------------------*/

   float    tmp[18], save, sum;
   float  pp1, pp2;
   float   *win_bt;
   int     i, p, ss;
   float *in = ext->xr[ch][sb];
   float out[36];

   if (win_type == 2) {
      for (p=0;p<36;p+=9) {
         out[p]   = out[p+1] = out[p+2] = out[p+3] =
                                          out[p+4] = out[p+5] = out[p+6] = out[p+7] =
                                                                           out[p+8] = 0.0f;
      }

      for (ss=0;ss<18;ss+=6) {

      /*
       *  12 point IMDCT
       */

      /* Begin 12 point IDCT */

      /* Input aliasing for 12 pt IDCT*/
         in[5+ss]+=in[4+ss];in[4+ss]+=in[3+ss];in[3+ss]+=in[2+ss];
         in[2+ss]+=in[1+ss];in[1+ss]+=in[0+ss];

                                      /* Input aliasing on odd indices (for 6 point IDCT) */
         in[5+ss] += in[3+ss];  in[3+ss]  += in[1+ss];

                                           /* 3 point IDCT on even indices */

         pp2 = in[4+ss] * 0.500000000f;
         pp1 = in[2+ss] * 0.866025403f;
         sum = in[0+ss] + pp2;
         tmp[1]= in[0+ss] - in[4+ss];
         tmp[0]= sum + pp1;
         tmp[2]= sum - pp1;

                                           /* End 3 point IDCT on even indices */

                                           /* 3 point IDCT on odd indices (for 6 point IDCT) */

         pp2 = in[5+ss] * 0.500000000f;
         pp1 = in[3+ss] * 0.866025403f;
         sum = in[1+ss] + pp2;
         tmp[4] = in[1+ss] - in[5+ss];
         tmp[5] = sum + pp1;
         tmp[3] = sum - pp1;

                                           /* End 3 point IDCT on odd indices*/

                                           /* Twiddle factors on odd indices (for 6 point IDCT)*/

         tmp[3] *= 1.931851653f;
         tmp[4] *= 0.707106781f;
         tmp[5] *= 0.517638090f;

                                           /* Output butterflies on 2 3 point IDCT's (for 6 point IDCT)*/

         save = tmp[0];
         tmp[0] += tmp[5];
         tmp[5] = save - tmp[5];
         save = tmp[1];
         tmp[1] += tmp[4];
         tmp[4] = save - tmp[4];
         save = tmp[2];
         tmp[2] += tmp[3];
         tmp[3] = save - tmp[3];

/* End 6 point IDCT */

/* Twiddle factors on indices (for 12 point IDCT) */

         tmp[0]  *=  0.504314480f;
         tmp[1]  *=  0.541196100f;
         tmp[2]  *=  0.630236207f;
         tmp[3]  *=  0.821339815f;
         tmp[4]  *=  1.306562965f;
         tmp[5]  *=  3.830648788f;

/* End 12 point IDCT */

/* Shift to 12 point modified IDCT, multiply by window type 2 */
         tmp[8]  = -tmp[0] * 0.793353340f;
         tmp[9]  = -tmp[0] * 0.608761429f;
         tmp[7]  = -tmp[1] * 0.923879532f;
         tmp[10] = -tmp[1] * 0.382683432f;
         tmp[6]  = -tmp[2] * 0.991444861f;
         tmp[11] = -tmp[2] * 0.130526192f;

         tmp[0]  =  tmp[3];
         tmp[1]  =  tmp[4] * 0.382683432f;
         tmp[2]  =  tmp[5] * 0.608761429f;

         tmp[3]  = -tmp[5] * 0.793353340f;
         tmp[4]  = -tmp[4] * 0.923879532f;
         tmp[5]  = -tmp[0] * 0.991444861f;

         tmp[0] *= 0.130526192f;

         out[ss + 6]  += tmp[0];
         out[ss + 7]  += tmp[1];
         out[ss + 8]  += tmp[2];
         out[ss + 9]  += tmp[3];
         out[ss + 10] += tmp[4];
         out[ss + 11] += tmp[5];
         out[ss + 12] += tmp[6];
         out[ss + 13] += tmp[7];
         out[ss + 14] += tmp[8];
         out[ss + 15] += tmp[9];
         out[ss + 16] += tmp[10];
         out[ss + 17] += tmp[11];

      }
      for (i=0;i<18;i++) ext->res[sb][i]=out[i] + ext->s[ch][sb][i];
      for (;i<36;i++) ext->s[ch][sb][i-18]=out[i];

   } else {

  /* 36 point IDCT */

  /* input aliasing for 36 point IDCT */
      in[17]+=in[16]; in[16]+=in[15]; in[15]+=in[14]; in[14]+=in[13];
      in[13]+=in[12]; in[12]+=in[11]; in[11]+=in[10]; in[10]+=in[9];
      in[9] +=in[8];  in[8] +=in[7];  in[7] +=in[6];  in[6] +=in[5];
      in[5] +=in[4];  in[4] +=in[3];  in[3] +=in[2];  in[2] +=in[1];
      in[1] +=in[0];

/* 18 point IDCT for odd indices */

/* input aliasing for 18 point IDCT */
      in[17]+=in[15]; in[15]+=in[13]; in[13]+=in[11]; in[11]+=in[9];
      in[9] +=in[7];  in[7] +=in[5];  in[5] +=in[3];  in[3] +=in[1];

/* 9 point IDCT on even indices */

/* original: */

/*   for(i=0; i<9; i++) {
sum = 0.0;

for(j=0;j<18;j+=2)
sum += in[j] * cos(PI36 * (2*i + 1) * j);

tmp[i] = sum;
} */

/* 9 Point Inverse Discrete Cosine Transform
//
// This piece of code is Copyright 1997 Mikko Tommila and is freely usable
// by anybody. The algorithm itself is of course in the public domain.
//
// Again derived heuristically from the 9-point WFTA.
//
// The algorithm is optimized (?) for speed, not for small rounding errors or
// good readability.
//
// 36 additions, 11 multiplications
//
// Again this is very likely sub-optimal.
//
// The code is optimized to use a minimum number of temporary variables,
// so it should compile quite well even on 8-register Intel x86 processors.
// This makes the code quite obfuscated and very difficult to understand.
//
// References:
// [1] S. Winograd: "On Computing the Discrete Fourier Transform",
//     Mathematics of Computation, Volume 32, Number 141, January 1978,
//     Pages 175-199

   Some modifications for maplay by Jeff Tsay */
      {
         float t0, t1, t2, t3, t4, t5, t6, t7;

         t1 = COSPI3 * in[12];
         t2 = COSPI3 * (in[8] + in[16] - in[4]);

         t3 = in[0] + t1;
         t4 = in[0] - t1 - t1;
         t5 = t4 - t2;

         t0 = DCTEVEN1 * (in[4] + in[8]);
         t1 = DCTEVEN2 * (in[8] - in[16]);

         tmp[4] = t4 + t2 + t2;
         t2 = DCTEVEN3 * (in[4] + in[16]);

         t6 = t3 - t0 - t2;
         t0 += t3 + t1;
         t3 += t2 - t1;

         t2 = DCTODD1 * (in[2]  + in[10]);
         t4 = DCTODD2 * (in[10] - in[14]);
         t7 = COSPI6 * in[6];

         t1 = t2 + t4 + t7;
         tmp[0] = t0 + t1;
         tmp[8] = t0 - t1;
         t1 = DCTODD3 * (in[2] + in[14]);
         t2 += t1 - t7;

         tmp[3] = t3 + t2;
         t0 = COSPI6 * (in[10] + in[14] - in[2]);
         tmp[5] = t3 - t2;

         t4 -= t1 + t7;

         tmp[1] = t5 - t0;
         tmp[7] = t5 + t0;
         tmp[2] = t6 + t4;
         tmp[6] = t6 - t4;
      }

/* End 9 point IDCT on even indices*/

    /* original:*/
/*   for(i=0; i<9; i++) {
       sum = 0.0;
       for(j=0;j<18;j+=2)
         sum += in[j+1] * cos(PI36 * (2*i + 1) * j);
       tmp[17-i] = sum;
   } */

/* This includes multiplication by the twiddle factors
   at the end -- Jeff.*/
      {
         float t0, t1, t2, t3, t4, t5, t6, t7;

         t1 = COSPI3 * in[13];
         t2 = COSPI3 * (in[9] + in[17] - in[5]);

         t3 = in[1] + t1;
         t4 = in[1] - t1 - t1;
         t5 = t4 - t2;

         t0 = DCTEVEN1 * (in[5] + in[9]);
         t1 = DCTEVEN2 * (in[9] - in[17]);

         tmp[13] = (t4 + t2 + t2)*0.707106781f;
         t2 = DCTEVEN3 * (in[5] + in[17]);

         t6 = t3 - t0 - t2;
         t0 += t3 + t1;
         t3 += t2 - t1;

         t2 = DCTODD1 * (in[3]  + in[11]);
         t4 = DCTODD2 * (in[11] - in[15]);
         t7 = COSPI6  * in[7];

         t1 = t2 + t4 + t7;
         tmp[17] = (t0 + t1) * 0.501909918f;
         tmp[9]  = (t0 - t1) * 5.736856623f;
         t1 = DCTODD3 * (in[3] + in[15]);
         t2 += t1 - t7;

         tmp[14] = (t3 + t2) * 0.610387294f;
         t0 = COSPI6 * (in[11] + in[15] - in[3]);
         tmp[12] = (t3 - t2) * 0.871723397f;

         t4 -= t1 + t7;

         tmp[16] = (t5 - t0) * 0.517638090f;
         tmp[10] = (t5 + t0) * 1.931851653f;
         tmp[15] = (t6 + t4) * 0.551688959f;
         tmp[11] = (t6 - t4) * 1.183100792f;
      }

/* End 9 point IDCT on odd indices */

/* Butterflies on 9 point IDCT's */
      for (i=0;i<9;i++) {
         save = tmp[i];
         tmp[i] += tmp[17-i];
         tmp[17-i] = save - tmp[17-i];
      }
/* end 18 point IDCT */

/* twiddle factors for 36 point IDCT */

      tmp[0] *=  -0.500476342f;
      tmp[1] *=  -0.504314480f;
      tmp[2] *=  -0.512139757f;
      tmp[3] *=  -0.524264562f;
      tmp[4] *=  -0.541196100f;
      tmp[5] *=  -0.563690973f;
      tmp[6] *=  -0.592844523f;
      tmp[7] *=  -0.630236207f;
      tmp[8] *=  -0.678170852f;
      tmp[9] *=  -0.740093616f;
      tmp[10]*=  -0.821339815f;
      tmp[11]*=  -0.930579498f;
      tmp[12]*=  -1.082840285f;
      tmp[13]*=  -1.306562965f;
      tmp[14]*=  -1.662754762f;
      tmp[15]*=  -2.310113158f;
      tmp[16]*=  -3.830648788f;
      tmp[17]*= -11.46279281f;

/* end 36 point IDCT */

/* shift to modified IDCT */
      win_bt = win[win_type];

      ext->res[sb][0] =-tmp[9]  * win_bt[0] + ext->s[ch][sb][0];
      ext->res[sb][1] =-tmp[10] * win_bt[1] + ext->s[ch][sb][1];
      ext->res[sb][2] =-tmp[11] * win_bt[2] + ext->s[ch][sb][2];
      ext->res[sb][3] =-tmp[12] * win_bt[3] + ext->s[ch][sb][3];
      ext->res[sb][4] =-tmp[13] * win_bt[4] + ext->s[ch][sb][4];
      ext->res[sb][5] =-tmp[14] * win_bt[5] + ext->s[ch][sb][5];
      ext->res[sb][6] =-tmp[15] * win_bt[6] + ext->s[ch][sb][6];
      ext->res[sb][7] =-tmp[16] * win_bt[7] + ext->s[ch][sb][7];
      ext->res[sb][8] =-tmp[17] * win_bt[8] + ext->s[ch][sb][8];

      ext->res[sb][9] = tmp[17] * win_bt[9] + ext->s[ch][sb][9];
      ext->res[sb][10]= tmp[16] * win_bt[10] + ext->s[ch][sb][10];
      ext->res[sb][11]= tmp[15] * win_bt[11] + ext->s[ch][sb][11];
      ext->res[sb][12]= tmp[14] * win_bt[12] + ext->s[ch][sb][12];
      ext->res[sb][13]= tmp[13] * win_bt[13] + ext->s[ch][sb][13];
      ext->res[sb][14]= tmp[12] * win_bt[14] + ext->s[ch][sb][14];
      ext->res[sb][15]= tmp[11] * win_bt[15] + ext->s[ch][sb][15];
      ext->res[sb][16]= tmp[10] * win_bt[16] + ext->s[ch][sb][16];
      ext->res[sb][17]= tmp[9]  * win_bt[17] + ext->s[ch][sb][17];


      ext->s[ch][sb][0]= tmp[8]  * win_bt[18];
      ext->s[ch][sb][1]= tmp[7]  * win_bt[19];
      ext->s[ch][sb][2]= tmp[6]  * win_bt[20];
      ext->s[ch][sb][3]= tmp[5]  * win_bt[21];
      ext->s[ch][sb][4]= tmp[4]  * win_bt[22];
      ext->s[ch][sb][5]= tmp[3]  * win_bt[23];
      ext->s[ch][sb][6]= tmp[2]  * win_bt[24];
      ext->s[ch][sb][7]= tmp[1]  * win_bt[25];
      ext->s[ch][sb][8]= tmp[0]  * win_bt[26];

      ext->s[ch][sb][9]= tmp[0]  * win_bt[27];
      ext->s[ch][sb][10]= tmp[1]  * win_bt[28];
      ext->s[ch][sb][11]= tmp[2]  * win_bt[29];
      ext->s[ch][sb][12]= tmp[3]  * win_bt[30];
      ext->s[ch][sb][13]= tmp[4]  * win_bt[31];
      ext->s[ch][sb][14]= tmp[5]  * win_bt[32];
      ext->s[ch][sb][15]= tmp[6]  * win_bt[33];
      ext->s[ch][sb][16]= tmp[7]  * win_bt[34];
      ext->s[ch][sb][17]= tmp[8]  * win_bt[35];
   }

   if (sb&1) for (i=1;i<18;i+=2) ext->res[sb][i]=-ext->res[sb][i];
}

/* fast DCT according to Lee[84]
 * reordering according to Konstantinides[94]
 */
void poly(mp3Info* ext, const int ch,int f) {
   float c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15;
   float c16,c17,c18,c19,c20,c21,c22,c23,c24,c25,c26,c27,c28,c29,c30,c31;
   float d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15;
   float d16,d17,d18,d19,d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31;
   int start = ext->u_start[ch];
   int div = ext->u_div[ch];
   int nch = ext->nch;
   float (*u_p)[16];

/* step 1: initial reordering and 1st (16 wide) butterflies
 */

   d0 =ext->res[ 0][f]; d16=(d0  - ext->res[31][f]) *  b1; d0 += ext->res[31][f];
   d1 =ext->res[ 1][f]; d17=(d1  - ext->res[30][f]) *  b3; d1 += ext->res[30][f];
   d3 =ext->res[ 2][f]; d19=(d3  - ext->res[29][f]) *  b5; d3 += ext->res[29][f];
   d2 =ext->res[ 3][f]; d18=(d2  - ext->res[28][f]) *  b7; d2 += ext->res[28][f];
   d6 =ext->res[ 4][f]; d22=(d6  - ext->res[27][f]) *  b9; d6 += ext->res[27][f];
   d7 =ext->res[ 5][f]; d23=(d7  - ext->res[26][f]) * b11; d7 += ext->res[26][f];
   d5 =ext->res[ 6][f]; d21=(d5  - ext->res[25][f]) * b13; d5 += ext->res[25][f];
   d4 =ext->res[ 7][f]; d20=(d4  - ext->res[24][f]) * b15; d4 += ext->res[24][f];
   d12=ext->res[ 8][f]; d28=(d12 - ext->res[23][f]) * b17; d12+= ext->res[23][f];
   d13=ext->res[ 9][f]; d29=(d13 - ext->res[22][f]) * b19; d13+= ext->res[22][f];
   d15=ext->res[10][f]; d31=(d15 - ext->res[21][f]) * b21; d15+= ext->res[21][f];
   d14=ext->res[11][f]; d30=(d14 - ext->res[20][f]) * b23; d14+= ext->res[20][f];
   d10=ext->res[12][f]; d26=(d10 - ext->res[19][f]) * b25; d10+= ext->res[19][f];
   d11=ext->res[13][f]; d27=(d11 - ext->res[18][f]) * b27; d11+= ext->res[18][f];
   d9 =ext->res[14][f]; d25=(d9  - ext->res[17][f]) * b29; d9 += ext->res[17][f];
   d8 =ext->res[15][f]; d24=(d8  - ext->res[16][f]) * b31; d8 += ext->res[16][f];

/* step 2: 8-wide butterflies
 */
   c0 = d0 + d8 ; c8 = ( d0 - d8 ) *  b2;
   c1 = d1 + d9 ; c9 = ( d1 - d9 ) *  b6;
   c2 = d2 + d10; c10= ( d2 - d10) * b14;
   c3 = d3 + d11; c11= ( d3 - d11) * b10;
   c4 = d4 + d12; c12= ( d4 - d12) * b30;
   c5 = d5 + d13; c13= ( d5 - d13) * b26;
   c6 = d6 + d14; c14= ( d6 - d14) * b18;
   c7 = d7 + d15; c15= ( d7 - d15) * b22;

   c16=d16 + d24; c24= (d16 - d24) *  b2;
   c17=d17 + d25; c25= (d17 - d25) *  b6;
   c18=d18 + d26; c26= (d18 - d26) * b14;
   c19=d19 + d27; c27= (d19 - d27) * b10;
   c20=d20 + d28; c28= (d20 - d28) * b30;
   c21=d21 + d29; c29= (d21 - d29) * b26;
   c22=d22 + d30; c30= (d22 - d30) * b18;
   c23=d23 + d31; c31= (d23 - d31) * b22;

/* step 3: 4-wide butterflies
 */
   d0 = c0 + c4 ; d4 = ( c0 - c4 ) *  b4;
   d1 = c1 + c5 ; d5 = ( c1 - c5 ) * b12;
   d2 = c2 + c6 ; d6 = ( c2 - c6 ) * b28;
   d3 = c3 + c7 ; d7 = ( c3 - c7 ) * b20;

   d8 = c8 + c12; d12= ( c8 - c12) *  b4;
   d9 = c9 + c13; d13= ( c9 - c13) * b12;
   d10= c10+ c14; d14= (c10 - c14) * b28;
   d11= c11+ c15; d15= (c11 - c15) * b20;

   d16= c16+ c20; d20= (c16 - c20) *  b4;
   d17= c17+ c21; d21= (c17 - c21) * b12;
   d18= c18+ c22; d22= (c18 - c22) * b28;
   d19= c19+ c23; d23= (c19 - c23) * b20;

   d24= c24+ c28; d28= (c24 - c28) *  b4;
   d25= c25+ c29; d29= (c25 - c29) * b12;
   d26= c26+ c30; d30= (c26 - c30) * b28;
   d27= c27+ c31; d31= (c27 - c31) * b20;

/* step 4: 2-wide butterflies
 */
/**/    c0 = d0 + d2 ; c2 = ( d0 - d2 ) *  b8;
   c1 = d1 + d3 ; c3 = ( d1 - d3 ) * b24;
/**/    c4 = d4 + d6 ; c6 = ( d4 - d6 ) *  b8;
   c5 = d5 + d7 ; c7 = ( d5 - d7 ) * b24;
/**/    c8 = d8 + d10; c10= ( d8 - d10) *  b8;
   c9 = d9 + d11; c11= ( d9 - d11) * b24;
/**/    c12= d12+ d14; c14= (d12 - d14) *  b8;
   c13= d13+ d15; c15= (d13 - d15) * b24;
/**/    c16= d16+ d18; c18= (d16 - d18) *  b8;
   c17= d17+ d19; c19= (d17 - d19) * b24;
/**/    c20= d20+ d22; c22= (d20 - d22) *  b8;
   c21= d21+ d23; c23= (d21 - d23) * b24;
/**/    c24= d24+ d26; c26= (d24 - d26) *  b8;
   c25= d25+ d27; c27= (d25 - d27) * b24;
/**/    c28= d28+ d30; c30= (d28 - d30) *  b8;
   c29= d29+ d31; c31= (d29 - d31) * b24;

/* step 5: 1-wide butterflies
 */
   d0 = c0 + c1 ; d1 = ( c0 - c1 ) * b16;
   d2 = c2 + c3 ; d3 = ( c2 - c3 ) * b16;
   d4 = c4 + c5 ; d5 = ( c4 - c5 ) * b16;
   d6 = c6 + c7 ; d7 = ( c6 - c7 ) * b16;
   d8 = c8 + c9 ; d9 = ( c8 - c9 ) * b16;
   d10= c10+ c11; d11= (c10 - c11) * b16;
   d12= c12+ c13; d13= (c12 - c13) * b16;
   d14= c14+ c15; d15= (c14 - c15) * b16;
   d16= c16+ c17; d17= (c16 - c17) * b16;
   d18= c18+ c19; d19= (c18 - c19) * b16;
   d20= c20+ c21; d21= (c20 - c21) * b16;
   d22= c22+ c23; d23= (c22 - c23) * b16;
   d24= c24+ c25; d25= (c24 - c25) * b16;
   d26= c26+ c27; d27= (c26 - c27) * b16;
   d28= c28+ c29; d29= (c28 - c29) * b16;
   d30= c30+ c31; d31= (c30 - c31) * b16;

/* step 6: final resolving & reordering
 * the other 32 are stored for use with the next granule
 */

   u_p = (float (*)[16]) &ext->u[ch][div][0][start];

/*16*/                 u_p[0][0] =+d1 ;
   u_p[31][0] = -(u_p[1][0] =+d16 +d17 +d18 +d22 -d30);
   u_p[30][0] = -(u_p[2][0] =+d8 +d9 +d10 -d14);
   u_p[29][0] = -(u_p[3][0] =-d16 -d17 -d18 -d22 +d24 +d25 +d26);
/*20*/  u_p[28][0] = -(u_p[4][0] =+d4 +d5 -d6);
   u_p[27][0] = -(u_p[5][0] =+d16 +d17 +d18 +d20 +d21 -d24 -d25 -d26);
   u_p[26][0] = -(u_p[6][0] =-d8 -d9 -d10 +d12 +d13);
   u_p[25][0] = -(u_p[7][0] =-d16 -d17 -d18 -d20 -d21 +d28 +d29);
/*24*/  u_p[24][0] = -(u_p[8][0] =-d2 +d3);
   u_p[23][0] = -(u_p[9][0] =+d16 +d17 +d19 +d20 +d21 -d28 -d29);
   u_p[22][0] = -(u_p[10][0] =+d8 +d9 +d11 -d12 -d13);
   u_p[21][0] = -(u_p[11][0] =-d16 -d17 -d19 -d20 -d21 +d24 +d25 +d27);
/*28*/  u_p[20][0] = -(u_p[12][0] =-d4 -d5 +d7);
   u_p[19][0] = -(u_p[13][0] =+d16 +d17 +d19 +d23 -d24 -d25 -d27);
   u_p[18][0] = -(u_p[14][0] =-d8 -d9 -d11 +d15);
   u_p[17][0] = -(u_p[15][0]   =-d16 -d17 -d19 -d23 +d31);
   u_p[16][0] = 0.0f;

/* the other 32 are stored for use with the next granule
 */

   u_p = (float (*)[16]) &ext->u[ch][!div][0][start];

/*0*/   u_p[16][0] = -2*d0;
   u_p[15][0] = u_p[17][0] = -(+d16 );
   u_p[14][0] = u_p[18][0] = -(+d8 );
   u_p[13][0] = u_p[19][0] = -(-d16 +d24 );
/*4*/   u_p[12][0] = u_p[20][0] = -(+d4 );
   u_p[11][0] = u_p[21][0] = -(+d16 +d20 -d24 );
   u_p[10][0] = u_p[22][0] = -(-d8 +d12 );
   u_p[9][0] = u_p[23][0] = -(-d16 -d20 +d28 );
/*8*/   u_p[8][0] = u_p[24][0] = -(+d2 );
   u_p[7][0] = u_p[25][0] = -(+d16 +d18 +d20 -d28 );
   u_p[6][0] = u_p[26][0] = -(+d8 +d10 -d12 );
   u_p[5][0] = u_p[27][0] = -(-d16 -d18 -d20 +d24 +d26 );
/*12*/  u_p[4][0] = u_p[28][0] = -(-d4 +d6 );
   u_p[3][0] = u_p[29][0] = -(+d16 +d18 +d22 -d24 -d26 );
   u_p[2][0] = u_p[30][0] = -(-d8 -d10 +d14 );
   u_p[1][0] = u_p[31][0] = -(-d16 -d18 -d22 +d30 );
   u_p[0][0] = -d1;


/* we're doing dewindowing and calculating final samples now
 */

   {
      int j;
      float out;
      float *dewindow = (float*) t_dewindow;
      float *u_ptr;

      u_p = ext->u[ch][div];

      if (nch == 2) {

         fool_opt = (int) u_p;

         switch (start) {
#if !defined(MEDIUM_STEREO_CACHE) && !defined(SMALL_STEREO_CACHE)
            case 0:
               u_ptr = (float *) u_p;

               for (j=0;j<32;j++) {
                  out  = *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 1:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ?-32768.0f : out;*/
               }
               break;

            case 2:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 3:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 4:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 5:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 6:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 7:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

#endif
#if !defined(SMALL_STEREO_CACHE)
            case 8:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 9:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 10:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 11:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 12:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 13:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 14:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 15:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;

                  ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;
#endif
#if defined(MEDIUM_STEREO_CACHE) || defined(SMALL_STEREO_CACHE)
            default:
               {
                  int i=start;

                  for (j=0;j<32;j++) {
                     u_ptr = u_p[j];

                     out  = u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;

                     ext->stereo_samples[f][j][ch] = out ;;   /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
                  }
               }
               break;
#endif
         }

         ext->u_start[ch] = (ext->u_start[ch]-1)&0xf;
         ext->u_div[ch] = ext->u_div[ch] ? 0 : 1;
      } else {
         switch (start) {
#if !defined(MEDIUM_MONO_CACHE) && !defined(SMALL_MONO_CACHE)
            case 0:
               u_ptr = (float *) u_p;

               for (j=0;j<32;j++) {
                  out  = *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;
                  out += *u_ptr++ * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 1:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 2:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 3:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 4:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 5:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 6:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 7:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

#endif
#if !defined(SMALL_MONO_CACHE)
            case 8:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 9:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 10:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 11:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 12:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 13:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 14:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[14] * *dewindow++;
                  out += u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;

            case 15:
               for (j=0;j<32;j++) {
                  u_ptr = u_p[j];

                  out  = u_ptr[15] * *dewindow++;
                  out += u_ptr[0] * *dewindow++;
                  out += u_ptr[1] * *dewindow++;
                  out += u_ptr[2] * *dewindow++;
                  out += u_ptr[3] * *dewindow++;
                  out += u_ptr[4] * *dewindow++;
                  out += u_ptr[5] * *dewindow++;
                  out += u_ptr[6] * *dewindow++;
                  out += u_ptr[7] * *dewindow++;
                  out += u_ptr[8] * *dewindow++;
                  out += u_ptr[9] * *dewindow++;
                  out += u_ptr[10] * *dewindow++;
                  out += u_ptr[11] * *dewindow++;
                  out += u_ptr[12] * *dewindow++;
                  out += u_ptr[13] * *dewindow++;
                  out += u_ptr[14] * *dewindow++;

                  ext->mono_samples[f][j] = out ;;    /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
               }
               break;
#endif
#if defined(MEDIUM_MONO_CACHE) || defined(SMALL_MONO_CACHE)
            default:
               {
                  int i=start;

                  for (j=0;j<32;j++) {
                     u_ptr = u_p[j];

                     out  = u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;
                     out += u_ptr[i++ & 0xf] * *dewindow++;

                     ext->mono_samples[f][j] = out ;; /*> 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out;*/
                  }
               }
               break;
#endif
         }

         ext->u_start[0] = (ext->u_start[0]-1)&0xf;
         ext->u_div[0] = ext->u_div[0] ? 0 : 1;
      }
   }
}

static void
  premultiply() {
   int i,t;

   for (i = 0; i < 16; ++i)
      for (t = 0; t < 32; ++t)
         t_dewindow[i][t] *= 16383.5f;
}

/* this function decodes one layer3 audio frame, except for the header decoding
 * which is done in main() [audio.c]. returns 0 if everything is ok.
 */

static int
layer3_frame(mp3Info *ext, struct AUDIO_HEADER *header, int len)
{
  static struct SIDE_INFO info;

  int gr,ch,sb,i,tmp;
  int mean_frame_size,bitrate,fs,hsize,ssize;
  int cnt = ext->cnt;
  char *rest = (char *) ext->rest;
  /* we need these later, hsize is the size of header+side_info */

  if (header->ID)
    if (header->mode==3) {
      ext->nch = 1;
      hsize=21;
    } else {
      ext->nch = 2;
      hsize=36;
    }
  else
    if (header->mode==3) {
      ext->nch = 1;
      hsize=13;
    } else {
      ext->nch = 2;
      hsize=21;
    }

  /* crc increases hsize by 2 */

  if (header->protection_bit==0) hsize+=2;


  /* read layer3 specific side_info */

  getinfo(header,&info);

  /* MPEG2 only has one granule  */

  bitrate=t_bitrate[header->ID][3-header->layer][header->bitrate_index];
  fs=t_sampling_frequency[header->fullID][header->sampling_frequency];
  mean_frame_size = (bitrate * (sr_lookup[header->ID]) / fs);
  /* check if mdb is too big for the first few frames. this means that
   * a part of the stream could be missing. We must still fill the buffer
   */
  if (info.main_data_begin > gblAppend)
    if (cnt*mean_frame_size < 960) {
      if (debugLevel > 0) {
         Snack_WriteLogInt(" incomplete frame < 960 bytes ", cnt);
      }
      /*printf(" frame %d discarded, incomplete main_data\n",cnt);*/
      fillbfr(mean_frame_size + header->padding_bit - hsize);
      return 0;
    }

  /* now update the data 'pointer' (counting in bits) according to
   * the main_data_begin information
   */
  gblData = 8 * ((gblAppend - info.main_data_begin) & (BUFFER_SIZE-1));

  /* read into the buffer all bytes up to the start of next header */

  fillbfr(mean_frame_size + header->padding_bit - hsize);
  /* these two should go away */

  ext->t_l=&t_b8_l[header->ID][header->sampling_frequency][0];
  ext->t_s=&t_b8_s[header->ID][header->sampling_frequency][0];

  /* debug/dump stuff */
  /*  show_header(header,bitrate,fs,mean_frame_size,0);*/
  /*if (A_DUMP_BINARY) dump((int *)info.part2_3_length);*/

  /* decode the scalefactors and huffman data
   * this part needs to be enhanced for error robustness
   */
  for (gr=0;gr < ((header->ID) ? 2 : 1);gr++) {
    for (ch=0;ch<ext->nch;ch++) {
      /*       show_side_info(&info,gr,ch,0);*/ /* this is for debug/dump */
      ssize=decode_scalefactors(ext, &info,header,gr,ch);
      decode_huffman_data(ext,&info,gr,ch,ssize);
    }

    /* requantization, stereo processing, reordering(shortbl) */
    if (header->mode!=1 || (header->mode==1 && header->mode_extension==0))
      for (ch=0;ch<ext->nch;ch++) requantize_mono(ext, gr,ch,&info,header);
    else requantize_ms(ext, gr,&info,header);

    /* antialiasing butterflies */
    for (ch=0;ch<ext->nch;ch++) {
      if(!(info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2))
        alias_reduction(ext,ch);
    }

    /* just which window? */
    for (ch=0;ch<ext->nch;ch++) {
      int win_type; /* same as in the standard, long=0, start=1 ,.... */

      if (info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2 && info.mixed_block_flag[gr][ch])
        win_type=0;
      else if (!info.window_switching_flag[gr][ch]) win_type=0;
      else win_type=info.block_type[gr][ch];

      /* imdct ...  */

      for (sb=0;sb<2;sb++)
        imdct(ext,win_type,sb,ch);

      if (info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2 && info.mixed_block_flag[gr][ch])
        win_type=2;

      /* no_of_imdcts tells us how many subbands from the top are all zero
       * it is set by the requantize functions in misc2.c
       */
      for (sb=2;sb<no_of_imdcts[ch];sb++)
        imdct(ext,win_type,sb,ch);

      /* clear s[][][] first so we don't totally blow the cache */

      tmp = sb;
      for (;sb<32;sb++)
        for (i=0;i<18;i++) {
          ext->res[sb][i]=ext->s[ch][sb][i];
          ext->s[ch][sb][i]=0.0f;
        }

      /* polyphase filterbank
       */
      /* if (nch == 2) this was a bug, tomislav */
      for (i=0;i<18;i++)
        poly(ext, ch, i);
    }
    if (ext->nch == 2) {
      int l = min(18*32*2*4, len - ext->ind);
      memcpy(&gblOutputbuf[ext->ind], ext->stereo_samples, l);
      ext->ind += l;
      if (l < 18*32*2*4) {
        memcpy(&rest[ext->restlen],
           &((char *)ext->stereo_samples)[l], 18*32*2*4 - l);
        ext->restlen += (18*32*2*4 - l);
      }
    } else {
      int l = min(18*32*4, len - ext->ind);
      memcpy(&gblOutputbuf[ext->ind], ext->mono_samples, l);
      ext->ind += l;
      if (l < 18*32*4) {
        memcpy(&rest[ext->restlen],
               &((char *)ext->mono_samples)[l], 18*32*4 - l);
        ext->restlen += (18*32*4 - l);
      }
    }
  }    /*  for (gr... */

  return 0;

}
/* calculating t_43 instead of having that big table in misc2.h
 */

void calculate_t43(void)
{
int i;
   for (i=0;i<8207;i++)
      t_43[i] = (float)pow((double)i,(double)4.0/3.0);
}

static int
processHeader(Sound *s, struct AUDIO_HEADER *header, int cnt)
{
  int g,i=0;
  mp3Info *Si = (mp3Info *)s->extHead;

  if (s->debug > 3) Snack_WriteLog("      Enter processHeader\n");

  /*
   * If already read the header, reuse it. Otherwise
   * read it from the stream.
   */
  if (Si->gotHeader) {
    memcpy((char *) _buffer, (char *)&(Si->headerInt), 4);
    _bptr=0;
  } else {
    if (_fillbfr(4) <= 0) return(1);
  }
  Si->gotHeader = 0;

  while ((g=gethdr(header))!=0) {
     /* Loop while the header is invalid, typically this won't happen
      * However it indicates a cut/insertion in the stream just before this.
      */
    if (_fillbfr(4) <= 0) return(1);
    i++;
  }
  if (s->debug > 0 && i > 0) {
     Snack_WriteLogInt("       Synced to valid next frame #",Si->cnt);
     Snack_WriteLogInt("                      bytes skipped",i*4);
  }
  if (header->protection_bit==0) getcrc();

  return(0);
 }

char *
GuessMP3File(char *buf, int len)
{
  int offset = 0;
  int depth = 0;
  int matches = 0;
  int i;
  float energyLIN16 = 1.0, energyLIN16S = 1.0, ratio;

  if (debugLevel > 1) {
     Snack_WriteLogInt(" GuessMP3File Called",len);
  }
  if (len < 4) return(QUE_STRING);
  /* If ID3 tag or RIFF tag then we know it is an MP3 (TFW: Not specifically true, others can have ID3V2)*/
  if (strncmp("ID3", buf, strlen("ID3")) == 0) {
    return(MP3_STRING);
  } else if (strncasecmp("RIFF", buf, strlen("RIFF")) == 0) {
    if (buf[20] == 0x55) {
      return(MP3_STRING);
    }
  }
  /* If an MP3, it has no ID3V2 tag at this point*/
  /* No need to search entire file, a true MP3 will show up quickly */

  for (i = 0; i < len / 2; i++) {
    short sampleLIN16  = ((short *)buf)[i];
    short sampleLIN16S = Snack_SwapShort(sampleLIN16);
    energyLIN16  += (float) sampleLIN16  * (float) sampleLIN16;
    energyLIN16S += (float) sampleLIN16S * (float) sampleLIN16S;
  }
  if (energyLIN16 > energyLIN16S) {
    ratio = energyLIN16 / energyLIN16S;
  } else {
    ratio = energyLIN16S / energyLIN16;
  }
  if (ratio > 10.0) {
    return(NULL);
  }

  depth = min(CHANNEL_HEADER_BUFFER,len);
  while (offset <= depth - 4) {
    /* Validate frame sync and other data to make sure this is a good header */
     if (hasSync(&buf[offset])) {
      int next_frame = locateNextFrame(&buf[offset]);
      if (debugLevel > 1) {
         Snack_WriteLogInt(" GuessMP3File Found a sync at",offset);
      }
      if (offset == 0 || offset == 72) {
       if (debugLevel > 0) {
          Snack_WriteLogInt("GuessMP3File detected MP3 at",offset);
       }
        return(MP3_STRING);
      }
      /* Have one sync but dataset is too short to check for more frames */
      if (offset + next_frame + 4 >= len && len > depth) {
        if (debugLevel > 0) {
           Snack_WriteLogInt(" GuessMP3File Failed at",offset);
        }
        return(NULL);
      }

      /* A valid MP3 should have a header at the next location,
         and they should nearly match (sync + ID/Layer/Pro bit
         just to make sure we need one additional matche.
      */
      if (hasSync(&buf[offset+next_frame])) {
        matches++;
        /* Require at least two frames have this kind of match */
        if (matches > 1) {
          if (debugLevel > 0) {
             Snack_WriteLogInt("GuessMP3File detected MP3 at",offset);
          }
          return(MP3_STRING);
        }
      }
    }
    offset++;
  }
  if (offset <= depth) {
    return(QUE_STRING);
  } else {
     if (debugLevel > 0) {
        Snack_WriteLogInt(" GuessMP3File Final Failed at",offset);
     }
    return(NULL);
  }
}

static int initDone = 0;

static void
InitMP3()
{
  premultiply();
  calculate_t43();
  imdct_init();
}

extern struct Snack_FileFormat *snackFileFormats;

#define SNACK_MP3_INT 18
static int ExtractI4(unsigned char *buf)
{
   int x;
   /*  big endian extract  */
   x = buf[0];
   x <<= 8;
   x |= buf[1];
   x <<= 8;
   x |= buf[2];
   x <<= 8;
   x |= buf[3];
   return x;
}
#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define ENDCHECK        20000
int
  GetMP3Header(Sound *S, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
               char *buf) {
   int offset = 0, okHeader = 0, i, j,k;
   int mean_frame_size = 0, bitrate = 0, fs, ID3Extra = 0;
   int layer, br_index, sr_index = 0, pad, mode, totalFrames=0;
   int passes = 0;
   int head_flags;
   int bufLen=CHANNEL_HEADER_BUFFER;
   int xFrames=0, xBytes=0, xAvgBitrate=0, xAvgFrameSize=0 ;
   float tailAverage=0.0;
   int tailChecked=0,tailTotal=0;
   mp3Info *Si = (mp3Info *)S->extHead;

   if (S->debug > 2) {
      Snack_WriteLog("    Enter GetMP3Header\n");
   }

   if (S->extHead != NULL && S->extHeadType != SNACK_MP3_INT) {
      Snack_FileFormat *ff;

      for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
         if (strcmp(S->fileType, ff->name) == 0) {
            if (ff->freeHeaderProc != NULL) {
               (ff->freeHeaderProc)(S);
            }
         }
      }
   }

   if (Si == NULL) {
      Si = (mp3Info *) ckalloc(sizeof(mp3Info));
      S->extHead = (char *) Si;
      for (i = 0; i < 32; i++) {
         for (j = 0; j < 16; j++) {
            Si->u[0][0][i][j] = 0.0f;
            Si->u[0][1][i][j] = 0.0f;
            Si->u[1][0][i][j] = 0.0f;
            Si->u[1][1][i][j] = 0.0f;
         }
      }
      for (i = 0; i < 32; i++) {
         for (j = 0; j < 18; j++) {
            Si->s[0][i][j] = 0.0f;
            Si->s[1][i][j] = 0.0f;
         }
      }
      Si->u_start[0] = 0;
      Si->u_start[1] = 0;
      Si->u_div[0] = 0;
      Si->u_div[1] = 0;
      Si->cnt = 0;

      if (!initDone) {
         InitMP3();
         initDone = 1;
      }
   }
   /*
    Init scalefactors to 0
    */
   for (i = 0; i < 2; i++) {
      for (j = 0; j < 2; j++) {
         for (k = 0; k < 22; k++) {
            Si->scalefac_l[i][j][k] = 0;
         }
      }
   }
   for (i = 0; i < 2; i++) {
      for (j = 0; j < 13; j++) {
         for (k = 0; k < 3; k++) {
            Si->scalefac_s[0][i][j][k] = 0;
            Si->scalefac_s[1][i][j][k] = 0;
         }
      }
   }

  /**
   * TFW: If any ID3V2 info is to be got, it can be read here
   * Insert code as needed.
   * See: http://www.id3.org/id3v2-00.txt for more details.
   */
   S->length = -1;
   if (strncmp("ID3", buf, strlen("ID3")) == 0) {
    /* ID3 size uses 28 packed bits, or 7 LSBs of words 6-9 */
    /* The extra 10 bytes account for this header length)*/
      long idOffset = (int )((long)(buf[6]&0x7F)*2097152l
                             + (long)(buf[7]&0x7F)*16384l
                             + (long)(buf[8]&0x7F)*128l
                             + (long)buf[9] + 10);
      /* Attempt to read beyond the ID3 offset if it is too large */
      if (idOffset > bufLen*3/4) {

         if (Tcl_Seek(ch, idOffset, SEEK_SET) > 0) {
            bufLen = Tcl_Read(ch, &buf[0], bufLen);
            if (bufLen > 0) {
               /* local buffer is now at end of ID3 tag */
               offset = 0;
               ID3Extra = idOffset;
            } else {
               if (S->debug > 0) Snack_WriteLogInt("ID3 size is in error is too big", offset);
               Tcl_AppendResult(interp, "ID3 size is in error is too big", NULL);
               return TCL_ERROR;
            }

         } else {
            if (S->debug > 0) Snack_WriteLogInt("ID3 Tag is bigger than file size", offset);
            Tcl_AppendResult(interp, "ID3 Tag is bigger than file size", NULL);
            return TCL_ERROR;
         }

      } else {
         offset = idOffset;
      }
   }
   /* RIFF has additional txt attachments */
   else if (strncasecmp("RIFF", buf, strlen("RIFF")) == 0) {
      if (buf[20] == 0x55) {
         offset = 72;
         if (S->storeType == SOUND_IN_CHANNEL) {
            Tcl_Read(ch, &buf[S->firstNRead], 76-S->firstNRead);
         }
      }
   }
  /* Continue to scan until a satisfactory frame is found */
   do {
    /* Valid Frame sync
       Bit Rate not 0000 or 1111 (which are invalid)
       Layer 00 (reserved)
       Sample Rate 11 (reserved)
    */
      if (hasSync(&buf[offset])) {
         /*
          * Have a good frame sync and the header data passed the initial checks
          */
         unsigned char *xBuf = &buf[offset];
         int next_frame = locateNextFrame(&buf[offset]);
         if (next_frame > bufLen) {
            if (S->debug > 0) Snack_WriteLogInt("Could not find MP3 header", next_frame);
            Tcl_AppendResult(interp, "Could not find MP3 header", NULL);
            return TCL_ERROR;
         }
         if (((buf[offset + 3] & 0xc0) >> 6) != 3) {
            S->nchannels = 2;
         } else {
            S->nchannels = 1;
         }
         S->encoding = LIN16;
         S->sampsize = 2;

         Si->id =   (buf[offset+1] & 0x08) >> 3;
         Si->fullID = (buf[offset+1] & 0x18) >> 3;
         layer =    (buf[offset+1] & 0x06) >> 1;
         sr_index = (buf[offset+2] & 0x0c) >> 2;

         br_index = (buf[offset+2] & 0xf0) >> 4;
         pad =      (buf[offset+2] & 0x02) >> 1;
         mode =     (buf[offset+3] & 0xc0) >> 6;

         fs = t_sampling_frequency[Si->fullID][sr_index];
         S->samprate = fs;
         bitrate = t_bitrate[Si->id][3 - layer][br_index];
         /* Xing VBR Check
          * If a Xing VBR header exists, then use the info from there
          * otherwise the length and average bitrate estimate will
          * be incorrect.
          *
          * First, determine offset of header into Aux section
          */
         if ( Si->id ) {                              /* mpeg1 */
            xBuf += 4 + (mode != 3 ? 32 : 17);
         } else {                                       /* mpeg */
            xBuf += 4 + (mode != 3 ? 17 : 9);
         }

         mean_frame_size = (bitrate * sr_lookup[Si->id] / fs);   /* This frame size */

         /* Max should be 2926 */

         if (mean_frame_size > MAXFRAMESIZE) {
            mean_frame_size = MAXFRAMESIZE;
         } else if (mean_frame_size <= 0) {
            offset++;
            continue;
         }

         if (strncmp("Xing", (char *) xBuf,4)==0) {
         /* We have a Xing VBR header */
            xBuf+=4;
            head_flags = ExtractI4(xBuf);
            xBuf+=4;
            if ( head_flags & FRAMES_FLAG ) {
               xFrames   = ExtractI4(xBuf);      /* Number of frames in file */
               xBuf+=4;
            }
            if ( head_flags & BYTES_FLAG ) {
               xBytes = ExtractI4(xBuf);         /* File size (at encoding) */
               xBuf+=4;
            }
            /* Enough info to compute average VBR bitrate and framesize*/
            if ( xFrames > 0 && xBytes > 0 && (head_flags & (BYTES_FLAG | FRAMES_FLAG))) {
               float fAvgFrameSize = (float)xBytes/(float)xFrames;
               xAvgFrameSize =  (int)roundf(fAvgFrameSize);
               xAvgBitrate =  (int)(fAvgFrameSize * (float)fs/(float)sr_lookup[Si->id]);   /* Layer 1 */
            }
         }

         /* End XING stuff */


         /*
            If we didn't find a header where we first expected it
            then the next valid one must match the following one.
         */
         if (passes > 0) {
            /* Verify this frame and next frame headers match */
            if (hasSync(&buf[offset+next_frame]) &&
                (buf[offset+3]|0x30) == (buf[offset+next_frame+3]|0x30) ) {
               okHeader = 1;
            } else {
               offset++;
            }
         } else {
            okHeader = 1;
         }
      } else {
         offset++;
      }
      if (offset > bufLen) {
         if (S->debug > 0) Snack_WriteLogInt("Could not find MP3 header", offset);
         Tcl_AppendResult(interp, "Could not find MP3 header", NULL);
         return TCL_ERROR;
      }
      passes++;
   } while (okHeader == 0);
   /*
    * We have a valid first sync here
    */

   if (S->debug > 0) Snack_WriteLogInt("Found MP3 header at offset", offset);
   /* Compute length */
   if (ch != NULL) {
      /*
       * Now find the end of the last valid frame in the file, skipping
       * the tag cruft that may throw off our length computations.
       */
      int tailsize=0;
      unsigned char *tailbuf = 0;
      int endpos = (int)Tcl_Seek(ch, 0, SEEK_END);
      int tailpos = (int)Tcl_Seek(ch, -ENDCHECK, SEEK_END);
      int indx = 0;
      tailsize = endpos-tailpos;
      if (debugLevel > 2) {
         Snack_WriteLogInt ("  End Pos at",endpos);
         Snack_WriteLogInt ("  Start Pos at",tailpos);
      }
      if (tailsize > 0) {

         tailbuf = Tcl_Alloc(tailsize);
         tailsize = Tcl_Read(ch, tailbuf, tailsize);
         /*
          * Find the first header (verify that there are two in a row for false syncs
          */
         while (indx < tailsize) {
            unsigned char tsr_index = (tailbuf[indx+2] & 0x0c) >> 2;
            if (hasSync(&tailbuf[indx]) && (sr_index == tsr_index)) {
               {
                  int frameLength = locateNextFrame(&tailbuf[indx]);
                  unsigned char tsr_index = (tailbuf[frameLength+indx+2] & 0x0c) >> 2;
                  if (hasSync(&tailbuf[frameLength+indx]) && (sr_index == tsr_index)) {
                     break;
                  }  else {
                     indx++;
                  }
               }
            } else {
               indx++;
            }
         }
         /*
          *  Now we know the first valid frame sync,
          * Walk the frames until we don't see any more
          */
         while (indx < tailsize) {
            unsigned char tsr_index = (tailbuf[indx+2] & 0x0c) >> 2;
            if (hasSync(&tailbuf[indx]) && (sr_index == tsr_index)) {
               int frameLength = locateNextFrame(&tailbuf[indx]);
               indx += frameLength;
               tailChecked++;
               tailTotal+=frameLength;
            } else {
               break;
            }
         }

         Tcl_Free(tailbuf);
      }
      /*
       * Check if tailAverage is valid, if not use mean_frame_size (which is first one found)
       */

      if (tailChecked > 3)
      {
         tailAverage=(float)tailTotal/(float)tailChecked;
      } else  {
         tailAverage=(float)mean_frame_size;
      }
      totalFrames = (int)((float)(tailpos + indx - (offset + ID3Extra)) / tailAverage);
   }
   if (obj != NULL) {
      if (useOldObjAPI) {
         totalFrames = (obj->length - (offset + ID3Extra)) / Si->bytesPerFrame;
      } else {
#ifdef TCL_81_API
         int length = 0;
         Tcl_GetByteArrayFromObj(obj, &length);
         totalFrames = (length - (offset + ID3Extra)) / Si->bytesPerFrame;
#endif
      }
   }

   /*
    * If Xing header, then use this data instead (assume it is accurate)
    */
   if (xAvgBitrate)
   {
      bitrate = xAvgBitrate;
      totalFrames = xFrames;
      mean_frame_size = xAvgFrameSize;
   } else {
      mean_frame_size = (int)roundf(tailAverage);
   }
   Si->bytesPerFrame = mean_frame_size;
   Si->bitrate = bitrate*1000;
   Si->bufind = offset + ID3Extra;
   Si->restlen = 0;
   Si->append = 0;
   Si->data = 0;
   Si->gotHeader = 1;
   memcpy((char *)&Si->headerInt, &buf[offset], 4);
   Si->lastByte = buf[offset+3];                 /* save for later, TFW: Can go away hopefully */
   Si->sr_index = sr_index;

   S->length = (totalFrames * 576) * (Si->id ? 2:1);
   S->headSize = offset + ID3Extra;
   S->swap = 0;
   S->extHead = (char *) Si;                     /* redundant */
   S->extHeadType = SNACK_MP3_INT;
   if (debugLevel > 0) {
       Snack_WriteLogInt ("  Frame Length",Si->bytesPerFrame);
      if (xAvgBitrate==0)
          Snack_WriteLogInt ("  CBR Avg Len x1000",(int)(tailAverage*1000.));
       Snack_WriteLogInt ("  Total Frames",totalFrames);
       Snack_WriteLogInt ("  Bit Rate",Si->bitrate);
  }
   if (S->debug > 2) Snack_WriteLogInt("    Exit GetMP3Header", S->length);

   return TCL_OK;
}

int
 SeekMP3File(Sound *S, Tcl_Interp *interp, Tcl_Channel ch, int pos) {
   int filepos, i, j, tpos;
   unsigned int hInt = 0;
   int framesize=0;
   mp3Info *Si = (mp3Info *)S->extHead;
   unsigned char *seekBuffer = NULL;
   if (S->debug > 0) Snack_WriteLogInt("    Enter SeekMP3File", pos);

   Si->bufind = S->headSize;
   Si->restlen = 0;
   Si->append = 0;
   Si->cnt = 0;
   Si->data = 0;
   for (i = 0; i < 32; i++) {
      for (j = 0; j < 16; j++) {
         Si->u[0][0][i][j] = 0.0f;
         Si->u[0][1][i][j] = 0.0f;
         Si->u[1][0][i][j] = 0.0f;
         Si->u[1][1][i][j] = 0.0f;
      }
   }
   Si->u_start[0] = 0;
   Si->u_start[1] = 0;
   Si->u_div[0] = 0;
   Si->u_div[1] = 0;
   for (i = 0; i < 32; i++) {
      for (j = 0; j < 18; j++) {
         Si->s[0][i][j] = 0.0f;
         Si->s[1][i][j] = 0.0f;
      }
   }

   /* Initally, get approximate position */
   /* TFW Note to self: ID3 Tag is accounted for in headSize */

   framesize = Si->id ? 1152:576; /* samples per frame */
   filepos = (S->headSize + (int)((float)Si->bytesPerFrame/(float)(framesize) * (float)pos) )&0xfffffffc;
   /*
    * TFW: For streams, can't seek very far. In fact, can't seek beyound the header size so all seeks fail
    */
   if (S->debug > 0) Snack_WriteLogInt("    Want to seek to", filepos);
   /* Sync up to next valid frame, put file position at start of data following header */
   hInt = Si->headerInt;
   if (ch != NULL) {
      int seekSize = max(Si->bytesPerFrame*25,20000);
      int index=0;
      tpos = (int)Tcl_Seek(ch, filepos, SEEK_SET);
      if (tpos < 0) {
         if (S->debug > 0) Snack_WriteLogInt("    Failed to seek to", filepos);
         return(filepos);                                  /* Denote seek beyond eof */
      } else {
         filepos = tpos;
      }
      seekBuffer = Tcl_Alloc(seekSize);
      if (seekBuffer == NULL) {
         if (S->debug > 0) Snack_WriteLogInt("    Failed to allocate seek buffer", seekSize);
         return -1;
      }
      seekSize = Tcl_Read(ch, seekBuffer, seekSize);
      if (seekSize <= 0) {
         if (S->debug > 0) Snack_WriteLogInt("    Read beyond EOF", filepos);
         Tcl_Free(seekBuffer);
         return(seekSize);                                      /* Denote seek beyond eof */
      }
      /**
       * Scan through buffer and find N consecutive frames, then
       * position the file channel to the start of the data
       * following the first header.
       * (Si->lastByte|0x30) == (tmp[3]|0x30)
       */
      Si->gotHeader = 0;
      while (index < seekSize) {
         int syncsNeeded = 3;
         int tmpIndex= index;
         unsigned char *ref = &seekBuffer[index];
         /*
          * Make sure we can walk N frame syncs, just to make sure
          * Double check against the sr index and other data we consider
          * static in the file just to make sure we indeed are at the
          * start of a frame.
          */
         while (tmpIndex < seekSize && tmpIndex > 0 && syncsNeeded > 0) {
            unsigned char *tmp = &seekBuffer[tmpIndex];
            unsigned char sr_index = (tmp[2] & 0x0c) >> 2;
            if (hasSync(tmp) && (sr_index == Si->sr_index) && (Si->lastByte|0x7C) == (tmp[3]|0x7C)) {
               int frameLength = locateNextFrame(tmp);
               tmpIndex += frameLength;
               syncsNeeded--;
            } else {
               break;
            }
         }
         if (syncsNeeded <= 0) {
            memcpy((char *)&Si->headerInt, ref, 4);
            Si->gotHeader = 1;
            if (S->debug > 2) Snack_WriteLogInt("    Seek done after", index);
            /*
             * Skip 32 bit header and position at start of data
             * (protection field handled elsewhere)
             */
            Tcl_Seek(ch,filepos + index + 4, SEEK_SET);
            Tcl_Free(seekBuffer);
            return(pos);
         }
         index++;
      }
      /* If we got here, we went past the eof, seek to it */
      Tcl_Seek(ch,0,SEEK_END);
      if (S->debug > 0) Snack_WriteLogInt("    Seek beyond EOF", filepos+index);
      pos = -1;
   }

   /*  pos = (pos - S->headSize) / (S->sampsize * S->nchannels);*/

   if (S->debug > 2) Snack_WriteLogInt("    Exit SeekMP3File", pos);
   Tcl_Free(seekBuffer);
   return(pos);
}
/**
 * Determine if the four words are a candidated for a sync
 * TFW: add sample rate param, since we know that can't change
 * in a stream.
 */
int hasSync (unsigned char *buf) {
   if ((buf[0] & 0xff) == 0xff &&                          /* frame sync, verify valid */
       (buf[1] & 0xe0) == 0xe0 &&                          /* frame sync, verify valid */
       (buf[1] & 0x18) != 0x08 &&                          /* Version, 1 not allowed */
       (buf[1] & 0x06) == 0x02 &&                          /* Layer 3 only (no layer 2 support yet)*/
       (buf[2] & 0x0C) != 0x0c &&                          /* sample rate index, 3 not allowed */
       (buf[2] & 0xf0) != 0xf0) {                           /* bitrate, 15 not allowed (0 is allowed but not supported) */
      return 1;
   }
   else {
      return 0;
   }
}
/**
 * Return the offset to the next potential frame based on
 * this frames data.
 */
int locateNextFrame (unsigned char *tmp) {
   int id       = (tmp[1] & 0x08) >> 3;          /* 2 or 3 (0 or 1)*/
   int fullID   = (tmp[1] & 0x18) >> 3;          /* full ID layer*/
   int layer    = (tmp[1] & 0x06) >> 1;          /* 1 for mp3 */
   int br_index = (tmp[2] & 0xf0) >> 4;          /* 10 for 128K layer 3 */
   int sr_index = (tmp[2] & 0x0c) >> 2;
   int padding  = (tmp[2] & 0x02) >> 1;
   int bitrate  = t_bitrate[id][3 - layer][br_index];
   int fs       = t_sampling_frequency[fullID][sr_index];   /* 44.1K normally */
   int next_frame_pos;
   if (bitrate == 0) {
      next_frame_pos = 1;                        /* (Free bit rate) This will move the scanner one step forward */
   }
   else {
      next_frame_pos = (bitrate * sr_lookup[id] / fs) + padding;
   }
   return next_frame_pos;
}

int
ReadMP3Samples(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, char *ibuf,
               float *obuf, int len)
{
  struct AUDIO_HEADER header;
  int last = -1;
  char *rest = (char *)((mp3Info *)s->extHead)->rest;
  mp3Info *Si = (mp3Info *)s->extHead;

  if (s->debug > 2) Snack_WriteLogInt("    Enter ReadMP3Samples", len);

  len *= sizeof(float);
  /*
   * Restore the sound posititions for the common
   * values for this particular object.
   */
  gblGch = ch;
  gblOutputbuf = (char *) obuf;
  gblReadbuf = ibuf;
  gblBufind = Si->bufind;
  gblBuffer = Si->buffer;
  gblAppend = Si->append;
  gblData   = Si->data;
  Si->ind = 0;
  if (Si->restlen > 0) {
    if (Si->restlen > len) {
      memcpy(gblOutputbuf, rest, len);
      Si->ind = len;
      Si->restlen -= len;
      memcpy(rest, &rest[len], Si->restlen);
    } else {
      memcpy(gblOutputbuf, rest, Si->restlen);
      Si->ind = Si->restlen;
      Si->restlen = 0;
    }
  }
  /* should be already set appropriatly
  if (Si->cnt == 0) {
    Si->gotHeader = 1;
  }
  */
  for (;; Si->cnt++) {
    if (Si->ind >= len) break;
    if (Si->ind == last &&
        Si->ind > 0) break;
    last = Si->ind;
    if (processHeader(s, &header, Si->cnt)) break;
    if (layer3_frame((mp3Info *)s->extHead, &header, len)) break;
  }

  Si->bufind = gblBufind;
  Si->append = gblAppend;
  Si->data = gblData;

  if (s->debug > 2) Snack_WriteLogInt("    Exit ReadMP3Samples", Si->ind);

  return(Si->ind / sizeof(float));
}

char *
ExtMP3File(char *s)
{
  int l1 = strlen(".mp3");
  int l2 = strlen(s);

  if (strncasecmp(".mp3", &s[l2 - l1], l1) == 0) {
    return(MP3_STRING);
  }
  return(NULL);
}

int
OpenMP3File(Sound *S, Tcl_Interp *interp, Tcl_Channel *ch, char *mode)
{
  int i, j;
  mp3Info *Si = NULL;

  if (S->debug > 2) Snack_WriteLog("    Enter OpenMP3File\n");

  if (S->extHead != NULL && S->extHeadType != SNACK_MP3_INT) {
    Snack_FileFormat *ff;

    for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
      if (strcmp(S->fileType, ff->name) == 0) {
        if (ff->freeHeaderProc != NULL) {
          (ff->freeHeaderProc)(S);
        }
      }
    }
  }

  if (S->extHead == NULL) {
    S->extHead = (char *) ckalloc(sizeof(mp3Info));
    S->extHeadType = SNACK_MP3_INT;
  }
  Si = (mp3Info *)S->extHead;
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 16; j++) {
      Si->u[0][0][i][j] = 0.0f;
      Si->u[0][1][i][j] = 0.0f;
      Si->u[1][0][i][j] = 0.0f;
      Si->u[1][1][i][j] = 0.0f;
    }
  }
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 18; j++) {
      Si->s[0][i][j] = 0.0f;
      Si->s[1][i][j] = 0.0f;
    }
  }
  Si->u_start[0] = 0;
  Si->u_start[1] = 0;
  Si->u_div[0] = 0;
  Si->u_div[1] = 0;
  Si->cnt = 0;

  if (!initDone) {
    InitMP3();
    initDone = 1;
  }

  if ((*ch = Tcl_OpenFileChannel(interp, S->fcname, mode, 0)) == 0) {
    return TCL_ERROR;
  }
  Tcl_SetChannelOption(interp, *ch, "-translation", "binary");
#ifdef TCL_81_API
  Tcl_SetChannelOption(interp, *ch, "-encoding", "binary");
#endif

  if (S->debug > 2) Snack_WriteLog("    Exit OpenMP3File\n");

  return TCL_OK;
}

int
CloseMP3File(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch)
{
  if (s->debug > 2) Snack_WriteLog("    Enter CloseMP3File\n");

  Tcl_Close(interp, *ch);
  *ch = NULL;

  if (s->debug > 2) Snack_WriteLog("    Exit CloseMP3File\n");

  return TCL_OK;
}

void
FreeMP3Header(Sound *s)
{
  if (s->debug > 2) Snack_WriteLog("    Enter FreeMP3Header\n");

  if (s->extHead != NULL) {
    ckfree((char *)s->extHead);
    s->extHead = NULL;
    s->extHeadType = 0;
  }

  if (s->debug > 2) Snack_WriteLog("    Exit FreeMP3Header\n");
}

int
  ConfigMP3Header(Sound *s, Tcl_Interp *interp, int objc,
                  Tcl_Obj *CONST objv[]) {
   mp3Info *si = (mp3Info *)s->extHead;
   int arg, index;
  static CONST84 char *optionStrings[] = {
      "-bitrate", NULL
   };
   enum options {
      BITRATE
   };

   if (si == NULL || objc < 3) return 0;

   if (objc == 3) {                              /* get option */
      if (Tcl_GetIndexFromObj(interp, objv[2], optionStrings, "option", 0,
                              &index) != TCL_OK) {
         Tcl_AppendResult(interp, ", or\n", NULL);
         return 0;
      }

      switch ((enum options) index) {
         case BITRATE:
            {
               Tcl_SetObjResult(interp, Tcl_NewIntObj(si->bitrate));
               break;
            }
      }
   } else {
      for (arg = 2; arg < objc; arg+=2) {
         int index;

         if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
                                 &index) != TCL_OK) {
            return TCL_ERROR;
         }

         if (arg + 1 == objc) {
            Tcl_AppendResult(interp, "No argument given for ",
                             optionStrings[index], " option\n", (char *) NULL);
            return 0;
         }

         switch ((enum options) index) {
            case BITRATE:
               {
                  break;
               }
         }
      }
   }

   return 1;
}
