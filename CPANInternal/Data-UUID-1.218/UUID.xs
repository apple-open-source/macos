#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "UUID.h"

#if defined __BEOS__ || defined __HAIKU__
#  undef bool
#  include <OS.h>
#endif

#ifdef USE_ITHREADS
# define DU_THREADSAFE 1
#else
# define DU_THREADSAFE 0
#endif

#if DU_THREADSAFE

# define pPTBL   pTHX
# define pPTBL_  pTHX_
# define aPTBL   aTHX
# define aPTBL_  aTHX_

# include "ptable.h"

# define ptable_store(T, K, V)  ptable_store(aTHX_ (T), (K), (V))

static ptable *instances;
static perl_mutex instances_mutex;

static void inc(pTHX_ ptable_ent *ent, void *ud) {
    UV count = (UV)ent->val;
    PERL_UNUSED_VAR(ud);
    ptable_store(instances, ent->key, (void *)++count);
}

#endif

static  perl_uuid_t NameSpace_DNS = { /* 6ba7b810-9dad-11d1-80b4-00c04fd430c8 */
   0x6ba7b810,
   0x9dad,
   0x11d1,
   0x80, 0xb4, { 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 }
};

static  perl_uuid_t NameSpace_URL = { /* 6ba7b811-9dad-11d1-80b4-00c04fd430c8 */
   0x6ba7b811,
   0x9dad,
   0x11d1,
   0x80, 0xb4, { 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 }
};

static  perl_uuid_t NameSpace_OID = { /* 6ba7b812-9dad-11d1-80b4-00c04fd430c8 */
   0x6ba7b812,
   0x9dad,
   0x11d1,
   0x80, 0xb4, { 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 }
};

static  perl_uuid_t NameSpace_X500 = { /* 6ba7b814-9dad-11d1-80b4-00c04fd430c8 */
   0x6ba7b814,
   0x9dad,
   0x11d1,
   0x80, 0xb4, { 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 }
};

static void format_uuid_v1(
   perl_uuid_t     *uuid, 
   unsigned16  clock_seq, 
   perl_uuid_time_t timestamp, 
   uuid_node_t node
) {
   uuid->time_low = (unsigned long)(timestamp & 0xFFFFFFFF);
   uuid->time_mid = (unsigned short)((timestamp >> 32) & 0xFFFF);
   uuid->time_hi_and_version = (unsigned short)((timestamp >> 48) &
      0x0FFF);

   uuid->time_hi_and_version |= (1 << 12);
   uuid->clock_seq_low = clock_seq & 0xFF;
   uuid->clock_seq_hi_and_reserved = (clock_seq & 0x3F00) >> 8;
   uuid->clock_seq_hi_and_reserved |= 0x80;
   memcpy(&uuid->node, &node, sizeof uuid->node);
}

static void get_current_time(perl_uuid_time_t * timestamp) {
   perl_uuid_time_t        time_now;
   static perl_uuid_time_t time_last;
   static unsigned16  uuids_this_tick;
   static int         inited = 0;

   if (!inited) {
      get_system_time(&time_last);
      uuids_this_tick = UUIDS_PER_TICK;
      inited = 1;
   };
   while (1) {
      get_system_time(&time_now);

      if (time_last != time_now) {
         uuids_this_tick = 0;
         time_last = time_now;
         break;
      };
      if (uuids_this_tick < UUIDS_PER_TICK) {
         uuids_this_tick++;
         break;
      };
   };
   *timestamp = time_now + uuids_this_tick;
}

static unsigned16 true_random(void) {
   static int  inited = 0;
   perl_uuid_time_t time_now;

   if (!inited) {
      get_system_time(&time_now);
      time_now = time_now/UUIDS_PER_TICK;
      srand((unsigned int)(((time_now >> 32) ^ time_now)&0xffffffff));
      inited = 1;
    };
    return (rand());
}

static void format_uuid_v3(
   perl_uuid_t        *uuid, 
   unsigned char  hash[16]
) {
   memcpy(uuid, hash, sizeof(perl_uuid_t));

   uuid->time_low            = ntohl(uuid->time_low);
   uuid->time_mid            = ntohs(uuid->time_mid);
   uuid->time_hi_and_version = ntohs(uuid->time_hi_and_version);

   uuid->time_hi_and_version &= 0x0FFF;
   uuid->time_hi_and_version |= (3 << 12);
   uuid->clock_seq_hi_and_reserved &= 0x3F;
   uuid->clock_seq_hi_and_reserved |= 0x80;
}

static void get_system_time(perl_uuid_time_t *perl_uuid_time) {
#if defined __cygwin__ || defined __MINGW32__ || defined WIN32
   /* ULARGE_INTEGER time; */
   LARGE_INTEGER time;

   /* use QeryPerformanceCounter for +ms resolution - as per Paul Stodghill 
   GetSystemTimeAsFileTime((FILETIME *)&time); */
   QueryPerformanceCounter(&time);
   time.QuadPart +=
      (unsigned __int64) (1000*1000*10) * 
      (unsigned __int64) (60 * 60 * 24) * 
      (unsigned __int64) (17+30+31+365*18+5);

   *perl_uuid_time = time.QuadPart;
#else
   struct timeval tp;

   gettimeofday(&tp, (struct timezone *)0);
   *perl_uuid_time = (tp.tv_sec * I64(10000000)) + (tp.tv_usec * I64(10)) +
      I64(0x01B21DD213814000);
#endif
}

static void get_random_info(unsigned char seed[16]) {
   SV* ctx;
#if defined __cygwin__ || defined __MINGW32__ || defined __MSWin32__
   typedef struct {
      MEMORYSTATUS  m;
      SYSTEM_INFO   s;
      FILETIME      t;
      LARGE_INTEGER pc;
      DWORD         tc;
      DWORD         l;
      char          hostname[MAX_COMPUTERNAME_LENGTH + 1];
   } randomness;
#else
   typedef struct {
#if defined __BEOS__ || defined __HAIKU__
      system_info    sys_info;
#else
      long           hostid;
#endif
      struct timeval t;
      char           hostname[257];
   } randomness;
#endif
   randomness r;

#if defined __cygwin__ || defined __MINGW32__ || defined __MSWin32__
   GlobalMemoryStatus(&r.m);
   GetSystemInfo(&r.s);
   GetSystemTimeAsFileTime(&r.t);
   QueryPerformanceCounter(&r.pc);
   r.tc = GetTickCount();
   r.l = MAX_COMPUTERNAME_LENGTH + 1;
   GetComputerName(r.hostname, &r.l );
#else
#  if defined __BEOS__ || defined __HAIKU__
   get_system_info(&r.sys_info);
#  else
   r.hostid = gethostid();
#  endif
   gettimeofday(&r.t, (struct timezone *)0);
   gethostname(r.hostname, 256);
#endif

   ctx = MD5Init();
   MD5Update(ctx, sv_2mortal(newSVpv((char*)&r, sizeof(randomness))));
   MD5Final(seed, ctx);
}

SV* make_ret(const perl_uuid_t u, int type) {
   char           buf[BUFSIZ];
   unsigned char *from, *to;
   STRLEN         len;
   int            i;

   memset(buf, 0x00, BUFSIZ);
   switch(type) {
   case F_BIN:
      memcpy(buf, (void*)&u, sizeof(perl_uuid_t));
      len = sizeof(perl_uuid_t);
      break;
   case F_STR:
      sprintf(buf, "%8.8X-%4.4X-%4.4X-%2.2X%2.2X-", (unsigned int)u.time_low, u.time_mid,
	 u.time_hi_and_version, u.clock_seq_hi_and_reserved, u.clock_seq_low);
      for(i = 0; i < 6; i++ ) 
	 sprintf(buf+strlen(buf), "%2.2X", u.node[i]);
      len = strlen(buf);
      break;
   case F_HEX:
      sprintf(buf, "0x%8.8X%4.4X%4.4X%2.2X%2.2X", (unsigned int)u.time_low, u.time_mid,
	 u.time_hi_and_version, u.clock_seq_hi_and_reserved, u.clock_seq_low);
      for(i = 0; i < 6; i++ ) 
	 sprintf(buf+strlen(buf), "%2.2X", u.node[i]);
      len = strlen(buf);
      break;
   case F_B64:
      for(from = (unsigned char*)&u, to = (unsigned char*)buf, i = sizeof(u); 
	  i > 0; i -= 3, from += 3) {
         *to++ = base64[from[0]>>2];
         switch(i) {
	 case 1:
	    *to++ = base64[(from[0]&0x03)<<4];
	    *to++ = '=';
	    *to++ = '=';
	     break;
         case 2:
	    *to++ = base64[((from[0]&0x03)<<4) | ((from[1]&0xF0)>>4)];
	    *to++ = base64[(from[1]&0x0F)<<2];
	    *to++ = '=';
	     break;
         default:
	    *to++ = base64[((from[0]&0x03)<<4) | ((from[1]&0xF0)>>4)];
	    *to++ = base64[((from[1]&0x0F)<<2) | ((from[2]&0xC0)>>6)];
	    *to++ = base64[(from[2]&0x3F)];
         }
      }	    
      len = strlen(buf);
      break;
   default:
      croak("invalid type: %d\n", type);
      break;
   }
   return sv_2mortal(newSVpv(buf,len));
}

static SV* MD5Init() {
   SV* res;
   int rcount;

   dSP;

   ENTER; SAVETMPS;

   PUSHMARK(SP);
   XPUSHs(sv_2mortal(newSVpv("Digest::MD5", 0)));
   PUTBACK;

   rcount = call_method("new", G_SCALAR);
   SPAGAIN;

   if ( rcount != 1 )
       croak("couldn't construct new Digest::MD5 object");

   res = newSVsv(POPs);

   PUTBACK;
   FREETMPS;
   LEAVE;

   return res;
};

static void MD5Update( SV* ctx, SV* data ) {
   dSP;
   ENTER; SAVETMPS;

   PUSHMARK(SP);
   XPUSHs(ctx);
   XPUSHs(data);
   PUTBACK;

   call_method("add", G_DISCARD);
   SPAGAIN;

   PUTBACK;
   FREETMPS;
   LEAVE;
};

static void MD5Final( unsigned char hash[16], SV* ctx ) {
   int rcount;
   char* tmp;
   STRLEN len;
   SV* retval;
   dSP;

   ENTER; SAVETMPS;

   PUSHMARK(SP);
   XPUSHs(sv_2mortal(ctx));
   PUTBACK;

   rcount = call_method("digest", G_SCALAR);
   SPAGAIN;

   if ( rcount != 1 )
       croak("Digest::MD5->digest hasn't returned a scalar");

   retval = POPs;
   tmp = SvPV(retval, len);
   if ( len != 16 )
       croak("Digest::MD5->digest returned not 16 bytes");

   memcpy(hash, tmp, len);

   PUTBACK;
   FREETMPS;
   LEAVE;
};

MODULE = Data::UUID		PACKAGE = Data::UUID		

PROTOTYPES: DISABLE

uuid_context_t*
new(class)
PREINIT:
   FILE          *fd;
   unsigned char  seed[16];
   perl_uuid_time_t    timestamp;
   mode_t         mask;
CODE:
   RETVAL = (uuid_context_t *)PerlMemShared_malloc(sizeof(uuid_context_t));
   if ((fd = fopen(UUID_STATE_NV_STORE, "rb"))) {
      fread(&(RETVAL->state), sizeof(uuid_state_t), 1, fd);
      fclose(fd);
      get_current_time(&timestamp);
      RETVAL->next_save = timestamp;
   }
   if ((fd = fopen(UUID_NODEID_NV_STORE, "rb"))) {
      pid_t *hate = (pid_t *) &(RETVAL->nodeid); 
      fread(&(RETVAL->nodeid), sizeof(uuid_node_t), 1, fd );
      fclose(fd);
      
      *hate += getpid();
   } else {
      get_random_info(seed);
      seed[0] |= 0x80;
      memcpy(&(RETVAL->nodeid), seed, sizeof(uuid_node_t));
      mask = umask(_DEFAULT_UMASK);
      if ((fd = fopen(UUID_NODEID_NV_STORE, "wb"))) {
         fwrite(&(RETVAL->nodeid), sizeof(uuid_node_t), 1, fd);
         fclose(fd);
      };
      umask(mask);
   }
   errno = 0;
#if DU_THREADSAFE
   MUTEX_LOCK(&instances_mutex);
   ptable_store(instances, RETVAL, (void *)(UV)1);
   MUTEX_UNLOCK(&instances_mutex);
#endif
OUTPUT:
   RETVAL

void
create(self)
   uuid_context_t *self;
ALIAS:
   Data::UUID::create_bin = F_BIN
   Data::UUID::create_str = F_STR
   Data::UUID::create_hex = F_HEX
   Data::UUID::create_b64 = F_B64
PREINIT:
   perl_uuid_time_t  timestamp;
   unsigned16   clockseq;
   perl_uuid_t       uuid;
   FILE        *fd;
   mode_t       mask;
PPCODE:
   clockseq = self->state.cs;
   get_current_time(&timestamp);
   if ( self->state.ts == I64(0) ||
      memcmp(&(self->nodeid), &(self->state.node), sizeof(uuid_node_t)))
      clockseq = true_random();
   else if (timestamp <= self->state.ts)
      clockseq++;

   format_uuid_v1(&uuid, clockseq, timestamp, self->nodeid);
   self->state.node = self->nodeid;
   self->state.ts   = timestamp;
   self->state.cs   = clockseq;
   if (timestamp > self->next_save ) {
      mask = umask(_DEFAULT_UMASK);
      if((fd = fopen(UUID_STATE_NV_STORE, "wb"))) {
	 LOCK(fd);
         fwrite(&(self->state), sizeof(uuid_state_t), 1, fd);
	 UNLOCK(fd);
         fclose(fd);
      }
      umask(mask);
      self->next_save = timestamp + (10 * 10 * 1000 * 1000);
   }
   ST(0) = make_ret(uuid, ix);
   XSRETURN(1);

void
create_from_name(self,nsid,name)
   uuid_context_t *self;
   perl_uuid_t    *nsid;
   SV             *name;
ALIAS:
   Data::UUID::create_from_name_bin = F_BIN
   Data::UUID::create_from_name_str = F_STR
   Data::UUID::create_from_name_hex = F_HEX
   Data::UUID::create_from_name_b64 = F_B64
PREINIT:
   SV *ctx;
   unsigned char hash[16];
   perl_uuid_t        net_nsid; 
   perl_uuid_t        uuid;
PPCODE:
   net_nsid = *nsid;
   net_nsid.time_low            = htonl(net_nsid.time_low);
   net_nsid.time_mid            = htons(net_nsid.time_mid);
   net_nsid.time_hi_and_version = htons(net_nsid.time_hi_and_version);

   ctx = MD5Init();
   MD5Update(ctx, newSVpv((char*)&net_nsid, sizeof(perl_uuid_t)));
   MD5Update(ctx, name);
   MD5Final(hash, ctx);

   format_uuid_v3(&uuid, hash);
   ST(0) = make_ret(uuid, ix);
   XSRETURN(1);

int 
compare(self,u1,u2)
   uuid_context_t *self;
   perl_uuid_t         *u1; 
   perl_uuid_t         *u2;
PREINIT:
   int i;
CODE:
   RETVAL = 0;
   CHECK(u1->time_low, u2->time_low);
   CHECK(u1->time_mid, u2->time_mid);
   CHECK(u1->time_hi_and_version, u2->time_hi_and_version);
   CHECK(u1->clock_seq_hi_and_reserved, u2->clock_seq_hi_and_reserved);
   CHECK(u1->clock_seq_low, u2->clock_seq_low);
   for (i = 0; i < 6; i++) {
      if (u1->node[i] < u2->node[i])
         RETVAL = -1;
      if (u1->node[i] > u2->node[i])
         RETVAL =  1;
   }
OUTPUT:
   RETVAL

void
to_string(self,uuid)
   uuid_context_t *self;
   perl_uuid_t         *uuid;
ALIAS:
   Data::UUID::to_hexstring = F_HEX
   Data::UUID::to_b64string = F_B64
PPCODE:
   ST(0) = make_ret(*uuid, ix ? ix : F_STR);
   XSRETURN(1);

void
from_string(self,str) 
   uuid_context_t *self;
   char           *str;
ALIAS:
   Data::UUID::from_hexstring = F_HEX
   Data::UUID::from_b64string = F_B64
PREINIT:
   perl_uuid_t         uuid;
   char          *from, *to;
   int            c;
   unsigned int   i;
   unsigned char  buf[4];
PPCODE:
   switch(ix) {
   case F_BIN:
   case F_STR:
   case F_HEX:
      from = str;
      memset(&uuid, 0x00, sizeof(perl_uuid_t));
      if ( from[0] == '0' && from[1] == 'x' )
         from += 2;
      for (i = 0; i < sizeof(perl_uuid_t); i++) {
         if (*from == '-')
	    from++; 
         if (sscanf(from, "%2x", &c) != 1) 
	    croak("from_string(%s) failed...\n", str);
         ((unsigned char*)&uuid)[i] = (unsigned char)c;
         from += 2;
      }
      uuid.time_low            = ntohl(uuid.time_low);
      uuid.time_mid            = ntohs(uuid.time_mid);
      uuid.time_hi_and_version = ntohs(uuid.time_hi_and_version);
      break;
   case F_B64:
      from = str; to = (char*)&uuid;
      while(from < (str + strlen(str))) {
	 i = 0; memset(buf, 254, 4);
	 do {
	    c = index64[(int)*from++];
	    if (c != 255) buf[i++] = (unsigned char)c;
	    if (from == (str + strlen(str))) 
	       break;
         } while (i < 4);

	 if (buf[0] == 254 || buf[1] == 254) 
	    break;
         *to++ = (buf[0] << 2) | ((buf[1] & 0x30) >> 4);

	 if (buf[2] == 254) break;
	 *to++ = ((buf[1] & 0x0F) << 4) | ((buf[2] & 0x3C) >> 2);

	 if (buf[3] == 254) break;
	 *to++ = ((buf[2] & 0x03) << 6) | buf[3];
      }
      break;
   default:
      croak("invalid type %d\n", ix);
      break;
   }
   ST(0) = make_ret(uuid, F_BIN);
   XSRETURN(1);

#if DU_THREADSAFE

void
CLONE(klass)
CODE:
   MUTEX_LOCK(&instances_mutex);
   ptable_walk(instances, inc, instances);
   MUTEX_UNLOCK(&instances_mutex);

#endif

void
DESTROY(self)
   uuid_context_t *self;
PREINIT:
#if DU_THREADSAFE
   UV            count;
#endif
   FILE           *fd;
CODE:
#if DU_THREADSAFE
   MUTEX_LOCK(&instances_mutex);
   count = (UV)ptable_fetch(instances, self);
   count--;
   ptable_store(instances, self, (void *)count);
   MUTEX_UNLOCK(&instances_mutex);
   if (count == 0) {
#endif
      if ((fd = fopen(UUID_STATE_NV_STORE, "wb"))) {
         LOCK(fd);
         fwrite(&(self->state), sizeof(uuid_state_t), 1, fd);
         UNLOCK(fd);
         fclose(fd);
      };
      PerlMemShared_free(self);
#if DU_THREADSAFE
   }
#endif

BOOT:
{
  HV *stash = gv_stashpv("Data::UUID", 0);
  STRLEN len = sizeof(perl_uuid_t);
#if DU_THREADSAFE
  instances = ptable_new();
  MUTEX_INIT(&instances_mutex);
#endif
  newCONSTSUB(stash, "NameSpace_DNS", newSVpv((char *)&NameSpace_DNS, len));
  newCONSTSUB(stash, "NameSpace_URL", newSVpv((char *)&NameSpace_URL, len));
  newCONSTSUB(stash, "NameSpace_OID", newSVpv((char *)&NameSpace_OID, len));
  newCONSTSUB(stash, "NameSpace_X500", newSVpv((char *)&NameSpace_X500, len));
}
