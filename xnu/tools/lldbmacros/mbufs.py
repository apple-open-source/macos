
""" Please make sure you read the README COMPLETELY BEFORE reading anything below.
    It is very critical that you read coding guidelines in Section E in README file.
"""
from xnu import *
from utils import *

from mbufdefines import *
import xnudefines
import kmemory

def MbufZoneByName(name):
    for i in range(1, int(kern.GetGlobalVariable('num_zones'))):
        z = addressof(kern.globals.zone_array[i])
        zs = addressof(kern.globals.zone_security_array[i])
        if ZoneName(z, zs) == name:
            return (z, zs)
    return None

# Macro: mbuf_stat
@lldb_command('mbuf_stat')
def MBufStat(cmd_args=None):
    """ Print extended mbuf allocator statistics.
    """
    hdr_format = "{0: <16s} {1: >8s} {2: >8s} {3: ^16s} {4: >8s} {5: >12s} {6: >8s} {7: >8s} {8: >8s} {9: >8s}"
    print(hdr_format.format('class', 'total', 'cached', 'uncached', 'inuse', 'failed', 'waiter', 'notified', 'purge', 'max'))
    print(hdr_format.format('name', 'objs', 'objs', 'objs/slabs', 'objs', 'alloc count', 'count', 'count', 'count', 'objs'))
    print(hdr_format.format('-'*16, '-'*8, '-'*8, '-'*16, '-'*8, '-'*12, '-'*8, '-'*8, '-'*8, '-'*8))
    entry_format = "{0: <16s} {1: >8d} {2: >8d} {3:>7d} / {4:<6d} {5: >8d} {6: >12d} {7: >8d} {8: >8d} {9: >8d} {10: >8d}"
    num_items = sizeof(kern.globals.mbuf_table) // sizeof(kern.globals.mbuf_table[0])
    ncpus = int(kern.globals.ncpu)
    mb_uses_mcache = int(kern.globals.mb_uses_mcache)
    for i in range(num_items):
        mbuf = kern.globals.mbuf_table[i]
        mcs = Cast(mbuf.mtbl_stats, 'mb_class_stat_t *')
        if mb_uses_mcache == 0:
            cname = str(mcs.mbcl_cname)
            if cname == "mbuf":
                zone = MbufZoneByName("mbuf")
            elif cname == "cl":
                zone = MbufZoneByName("data.mbuf.cluster.2k")
            elif cname == "bigcl":
                zone = MbufZoneByName("data.mbuf.cluster.4k")
            elif cname == "16kcl":
                zone = MbufZoneByName("data.mbuf.cluster.16k")
            elif cname == "mbuf_cl":
                zone = MbufZoneByName("mbuf.composite.2k")
            elif cname == "mbuf_bigcl":
                zone = MbufZoneByName("mbuf.composite.4k")
            elif cname == "mbuf_16kcl":
                zone = MbufZoneByName("mbuf.composite.16k")
            zone_stats = GetZone(zone[0], zone[1], [], [])
            print(entry_format.format(cname,
                                      int(zone_stats['size'] / mcs.mbcl_size),
                                      zone_stats['cache_element_count'],
                                      zone_stats['free_element_count'],
                                      0,
                                      int(zone_stats['used_size'] / mcs.mbcl_size),
                                      zone_stats['alloc_fail_count'],
                                      0, 0, 0,
                                      mbuf.mtbl_maxlimit))

        else:
            mc = mbuf.mtbl_cache
            total = 0
            total += int(mc.mc_full.bl_total) * int(mc.mc_cpu[0].cc_bktsize)
            ccp_arr = mc.mc_cpu
            for i in range(ncpus):
                ccp = ccp_arr[i]
                if int(ccp.cc_objs) > 0:
                    total += int(ccp.cc_objs)
                if int(ccp.cc_pobjs) > 0:
                    total += int(ccp.cc_pobjs)
            print(entry_format.format(mcs.mbcl_cname, mcs.mbcl_total,  total,
                                      mcs.mbcl_infree, mcs.mbcl_slab_cnt,
                                      (mcs.mbcl_total - total - mcs.mbcl_infree),
                                      mcs.mbcl_fail_cnt, mbuf.mtbl_cache.mc_waiter_cnt,
                                      mcs.mbcl_notified, mcs.mbcl_purge_cnt,
                                      mbuf.mtbl_maxlimit))
# EndMacro: mbuf_stat

def DumpMbufData(mp, count):
    if kern.globals.mb_uses_mcache == 1:
        mdata = mp.m_hdr.mh_data
        mlen = mp.m_hdr.mh_len
        flags = mp.m_hdr.mh_flags
        if flags & M_EXT:
            mdata = mp.M_dat.MH.MH_dat.MH_ext.ext_buf
    else:
        mdata = mp.M_hdr_common.M_hdr.mh_data
        mlen = mp.M_hdr_common.M_hdr.mh_len
        flags = mp.M_hdr_common.M_hdr.mh_flags
        if flags & M_EXT:
            mdata = mp.M_hdr_common.M_ext.ext_buf
    if (count > mlen):
        count = mlen
    cmd = "memory read -force -size 1 -count {0:d} 0x{1:x}".format(count, mdata)
    print(lldb_run_command(cmd))

def DecodeMbufData(in_mp, decode_as="ether"):
    import scapy.all
    err = lldb.SBError()
    scan = in_mp
    while (scan):
        full_buf = b''
        mp = scan
        while (mp):
            if kern.globals.mb_uses_mcache == 1:
                mdata = mp.m_hdr.mh_data
                mlen = unsigned(mp.m_hdr.mh_len)
                flags = mp.m_hdr.mh_flags
                if flags & M_EXT:
                    mdata = mp.M_dat.MH.MH_dat.MH_ext.ext_buf
                mnext = mp.m_hdr.mh_next
            else:
                mdata = mp.M_hdr_common.M_hdr.mh_data
                mlen = unsigned(mp.M_hdr_common.M_hdr.mh_len)
                flags = mp.M_hdr_common.M_hdr.mh_flags
                if flags & M_EXT:
                    mdata = mp.M_hdr_common.M_ext.ext_buf
                mnext = mp.M_hdr_common.M_hdr.mh_next

            addr = mdata.GetSBValue().GetValueAsAddress()
            buf = LazyTarget.GetProcess().ReadMemory(addr, mlen, err)
            full_buf += buf
            if mnext == 0:
                try:
                    if decode_as == "ether":
                        pkt = scapy.layers.l2.Ether(full_buf)
                    elif decode_as == "ip":
                        pkt = scapy.layers.inet.IP(full_buf)
                    elif decode_as == "ip6":
                        pkt = scapy.layers.inet6.IPv6(full_buf)
                    elif decode_as == "tcp":
                        pkt = scapy.layers.inet.TCP(full_buf)
                    elif decode_as == "udp":
                        pkt = scapy.layers.inet.UDP(full_buf)
                    else:
                        print("invalid decoder" + decode_as)
                        return
                    pkt.show()
                    break
                except KeyboardInterrupt:
                    raise KeyboardInterrupt
                except:
                    break
            mp = mnext
        if kern.globals.mb_uses_mcache == 1:
            scan = scan.m_hdr.mh_nextpkt
        else:
            scan = scan.M_hdr_common.M_hdr.mh_nextpkt


# Macro: mbuf_decode
@lldb_command('mbuf_decode', '')
def MbufDecode(cmd_args=None, cmd_options={}):
    """Decode an mbuf using scapy.
        Usage: mbuf_decode <mbuf address>
    """
    if cmd_args is None or len(cmd_args) < 1:
        print("usage: mbuf_decode <address> [decode_as]")
        return
    mp = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    if len(cmd_args) > 1:
        decode_as = cmd_args[1]
        DecodeMbufData(mp, decode_as)
    else:
        DecodeMbufData(mp)
# EndMacro: mbuf_decode

# Macro: mbuf_dumpdata
@lldb_command('mbuf_dumpdata', 'C:')
def MbufDumpData(cmd_args=None, cmd_options={}):
    """Dump the mbuf data
        Usage: mbuf_dumpdata  <mbuf address> [-C <count>]
    """
    if cmd_args is None or len(cmd_args) < 1:
        print(MbufDumpData.__doc__)
        return
    mp = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    if kern.globals.mb_uses_mcache == 1:
        mdata = mp.m_hdr.mh_data
        mhlen = mp.m_hdr.mh_len
    else:
        mdata = mp.M_hdr_common.M_hdr.mh_data
        mhlen = mp.M_hdr_common.M_hdr.mh_len
    mlen = 0
    if "-C" in cmd_options:
        mlen = ArgumentStringToInt(cmd_options["-C"])
        if (mlen > mhlen):
            mlen = mhlen
    else:
        mlen = mhlen
    DumpMbufData(mp, mlen)
# EndMacro: mbuf_dumpdata

def ShowMbuf(prefix, mp, count, total, dump_data_len):
    out_string = ""
    mca = ""
    if kern.globals.mb_uses_mcache == 1:
        mhlen = mp.m_hdr.mh_len
        mhtype = mp.m_hdr.mh_type
        mhflags = mp.m_hdr.mh_flags
        mhdata = mp.m_hdr.mh_data
        if mhflags & M_EXT:
            extbuf = mp.M_dat.MH.MH_dat.MH_ext.ext_buf
        if kern.globals.mclaudit != 0:
            mca += GetMbufBuf2Mca(mp) + ", "
    else:
        mhlen = mp.M_hdr_common.M_hdr.mh_len
        mhtype = mp.M_hdr_common.M_hdr.mh_type
        mhflags = mp.M_hdr_common.M_hdr.mh_flags
        mhdata = mp.M_hdr_common.M_hdr.mh_data
        if mhflags & M_EXT:
            extbuf = mp.M_hdr_common.M_ext.ext_buf
    mbuf_walk_format = "{0:s}{1:d} 0x{2:x} [len {3:d}, type {4:d}, data 0x{5:x}, "
    out_string += mbuf_walk_format.format(prefix, count[0], mp, mhlen, mhtype, mhdata)
    out_string += "flags " + GetMbufFlagsAsString(mhflags) + ", "
    if (mhflags & M_PKTHDR):
        out_string += GetMbufPktCrumbs(mp) + ", "
    if (mhflags & M_EXT):
        m_ext_format = "ext_buf 0x{0:x}, "
        out_string += m_ext_format.format(extbuf)
    if (mca != ""):
        out_string += mca + ", "
    total[0] = total[0] + mhlen
    out_string += "total " + str(total[0]) + "]"
    print(out_string)
    if (dump_data_len > 0):
        DumpMbufData(mp, dump_data_len)

def WalkMufNext(prefix, mp, count, total, dump_data_len):
    remaining_len = dump_data_len
    while (mp):
        if kern.globals.mb_uses_mcache == 1:
            mhlen = mp.m_hdr.mh_len
            mhnext = mp.m_hdr.mh_next
        else:
            mhlen = mp.M_hdr_common.M_hdr.mh_len
            mhnext = mp.M_hdr_common.M_hdr.mh_next
        count[0] += 1
        ShowMbuf(prefix, mp, count, total, remaining_len)
        if (remaining_len > mhlen):
            remaining_len -= mhlen
        else:
            remaining_len = 0
        mp = mhnext

# Macro: mbuf_walkpkt
@lldb_command('mbuf_walkpkt', 'C:')
def MbufWalkPacket(cmd_args=None, cmd_options={}):
    """ Walk the mbuf packet chain (m_nextpkt)
        Usage: mbuf_walkpkt  <mbuf address> [-C <count>]
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")
    mp = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')

    dump_data_len = 0
    if "-C" in cmd_options:
        dump_data_len = ArgumentStringToInt(cmd_options["-C"])

    count_packet = 0
    count_mbuf = 0
    total_len = 0

    while (mp):
        count_packet += 1
        prefix = "{0:d}.".format(count_packet)
        count = [0]
        total = [0]
        WalkMufNext(prefix, mp, count, total, dump_data_len)
        count_mbuf += count[0]
        total_len += total[0]
        if kern.globals.mb_uses_mcache == 1:
            mp = mp.m_hdr.mh_nextpkt
        else:
            mp = mp.M_hdr_common.M_hdr.mh_nextpkt
    out_string = "Total packets: {0:d} mbufs: {1:d} length: {2:d} ".format(count_packet, count_mbuf, total_len)
    print(out_string)
# EndMacro: mbuf_walkpkt

# Macro: mbuf_walk
@lldb_command('mbuf_walk', 'C:')
def MbufWalk(cmd_args=None, cmd_options={}):
    """ Walk the mbuf chain (m_next)
        Usage: mbuf_walk  <mbuf address> [-C <count>]
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")
    mp = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')

    dump_data_len = 0
    if "-C" in cmd_options:
        dump_data_len = ArgumentStringToInt(cmd_options["-C"])

    count = [0]
    total = [0]
    prefix = ""
    WalkMufNext(prefix, mp, count, total, dump_data_len)
# EndMacro: mbuf_walk

# Macro: mbuf_buf2slab
@lldb_command('mbuf_buf2slab')
def MbufBuf2Slab(cmd_args=None):
    """ Given an mbuf object, find its corresponding slab address
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")

    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return

    m = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    slab = GetMbufSlab(m)
    if (kern.ptrsize == 8):
        mbuf_slab_format = "0x{0:<16x}"
        print(mbuf_slab_format.format(slab))
    else:
        mbuf_slab_format = "0x{0:<8x}"
        print(mbuf_slab_format.format(slab))
# EndMacro: mbuf_buf2slab

# Macro: mbuf_buf2mca
@lldb_command('mbuf_buf2mca')
def MbufBuf2Mca(cmd_args=None):
    """ Find the mcache audit structure of the corresponding mbuf
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return

    m = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    print(GetMbufBuf2Mca(m))
    return
# EndMacro: mbuf_buf2mca

# Macro: mbuf_slabs
@lldb_command('mbuf_slabs')
def MbufSlabs(cmd_args=None):
    """ Print all slabs in the group
    """

    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis or zprint")
        return

    out_string = ""
    if not cmd_args:
        raise ArgumentError("Invalid arguments passed.")

    slg = kern.GetValueFromAddress(cmd_args[0], 'mcl_slabg_t *')
    x = 0

    if (kern.ptrsize == 8):
        slabs_string_format = "{0:>4d}: 0x{1:16x} 0x{2:16x} 0x{3:016x} 0x{4:016x} {5:10d} {6:3d} {7:3d} {8:3d} {9:5d} {10:>6s} "
        out_string += "slot  slab               next               obj                mca                tstamp     C   R   N   size   flags\n"
        out_string += "----- ------------------ ------------------ ------------------ ------------------ ---------- --- --- --- ------ -----\n"
    else:
        slabs_string_format = "{0:>4d}: 0x{1:8x} 0x{2:8x} 0x{3:08x} 0x{4:08x} {5:10d} {6:3d} {7:3d} {8:3d} {9:5d} {10:>6s} "
        out_string += "slot  slab       next       obj        mca        tstamp     C   R   N   size   flags\n"
        out_string += "----- ---------- ---------- ---------- ---------- ---------- --- --- --- ------ -----\n"

    mbutl = cast(kern.globals.mbutl, 'unsigned char *')
    nslabspmb = int((1 << MBSHIFT) >> unsigned(kern.globals.page_shift))
    while x < nslabspmb:
        sl = addressof(slg.slg_slab[x])
        mca = 0
        obj = sl.sl_base
        ts = 0

        if (kern.globals.mclaudit != 0 and obj != 0):
            mca = GetMbufMcaPtr(obj, sl.sl_class)
            trn = (mca.mca_next_trn + unsigned(kern.globals.mca_trn_max) - 1) % unsigned(kern.globals.mca_trn_max)
            ts = mca.mca_trns[trn].mca_tstamp

        out_string += slabs_string_format.format((x + 1), sl, sl.sl_next, obj, mca, int(ts), int(sl.sl_class), int(sl.sl_refcnt), int(sl.sl_chunks), int(sl.sl_len), hex(sl.sl_flags))

        if (sl.sl_flags != 0):
            out_string += "<"
            if sl.sl_flags & SLF_MAPPED:
                out_string += "mapped"
            if sl.sl_flags & SLF_PARTIAL:
                out_string += ",partial"
            if sl.sl_flags & SLF_DETACHED:
                out_string += ",detached"
            out_string += ">"
        out_string += "\n"

        if sl.sl_chunks > 1:
            z = 1
            c = sl.sl_len // sl.sl_chunks

            while z < sl.sl_chunks:
                obj = sl.sl_base + (c * z)
                mca = 0
                ts = 0

                if (kern.globals.mclaudit != 0 ):
                    mca = GetMbufMcaPtr(obj, sl.sl_class)
                    trn = (mca.mca_next_trn + unsigned(kern.globals.mca_trn_max) - 1) % unsigned(kern.globals.mca_trn_max)
                    ts = mca.mca_trns[trn].mca_tstamp

                if (kern.ptrsize == 8):
                    chunk_string_format = "                                            0x{0:16x} 0x{1:16x} {2:10d}\n"
                else:
                    chunk_string_format = "                            0x{0:8x} {1:4s} {2:10d}\n"

                out_string += chunk_string_format.format(int(obj), int(mca), int(ts))

                z += 1
        x += 1
    print(out_string)
# EndMacro: mbuf_slabs

# Macro: mbuf_slabstbl
@lldb_command('mbuf_slabstbl')
def MbufSlabsTbl(cmd_args=None):
    """ Print slabs table
    """
    out_string = ""
    x = 0

    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis or zprint")
        return

    if (kern.ptrsize == 8):
        out_string += "slot slabg              slabs range\n"
        out_string += "---- ------------------ -------------------------------------------\n"
    else:
        out_string += "slot slabg      slabs range\n"
        out_string += "---- ---------- ---------------------------\n"

    slabstbl = kern.globals.slabstbl
    slabs_table_blank_string_format = "{0:>3d}: - \n"
    nslabspmb = int(((1 << MBSHIFT) >> unsigned(kern.globals.page_shift)))
    while (x < unsigned(kern.globals.maxslabgrp)):
        slg = slabstbl[x]
        if (slg == 0):
            out_string += slabs_table_blank_string_format.format(x+1)
        else:
            if (kern.ptrsize == 8):
                slabs_table_string_format = "{0:>3d}: 0x{1:16x}  [ 0x{2:16x} - 0x{3:16x} ]\n"
                out_string += slabs_table_string_format.format(x+1, slg, addressof(slg.slg_slab[0]), addressof(slg.slg_slab[nslabspmb-1]))
            else:
                slabs_table_string_format = "{0:>3d}: 0x{1:8x}  [ 0x{2:8x} - 0x{3:8x} ]\n"
                out_string += slabs_table_string_format.format(x+1, slg, addressof(slg.slg_slab[0]), addressof(slg.slg_slab[nslabspmb-1]))

        x += 1
    print(out_string)
# EndMacro: mbuf_slabstbl

def MbufDecode(mbuf, decode_pkt):
    # Ignore free'd mbufs.
    if kern.globals.mb_uses_mcache == 1:
        mhlen = mbuf.m_hdr.mh_len
        mhtype = mbuf.m_hdr.mh_type
        mhflags = mbuf.m_hdr.mh_flags
    else:
        mhlen = mbuf.M_hdr_common.M_hdr.mh_len
        mhtype = mbuf.M_hdr_common.M_hdr.mh_type
        mhflags = mbuf.M_hdr_common.M_hdr.mh_flags
    if mhtype == 0:
        return
    flags = int(mhflags)
    length = int(mhlen)
    if length < 20 or length > 8 * 1024:
        # Likely not a packet.
        return
    out_string = "mbuf found @ 0x{0:x}, length {1:d}, {2:s}, {3:s}".format(mbuf, length, GetMbufFlags(mbuf), GetMbufPktCrumbs(mbuf))
    print(out_string)
    if flags & M_PKTHDR:
        if kern.globals.mb_uses_mcache == 1:
            rcvif = mbuf.M_dat.MH.MH_pkthdr.rcvif
        else:
            rcvif = mbuf.M_hdr_common.M_pkthdr.rcvif
        if rcvif != 0:
            try:
                print("receive interface " + rcvif.if_xname)
            except ValueError:
                pass
    if decode_pkt:
        DecodeMbufData(mbuf)


# Macro: mbuf_walk_slabs
@lldb_command('mbuf_walk_slabs')
def MbufWalkSlabs(cmd_args=None):
    """
    Walks the mbuf slabs table backwards and tries to detect and decode mbufs.
    Use 'mbuf_walk_slabs decode' to decode the mbuf using scapy.
    """
    decode_pkt = False
    if len(cmd_args) > 0 and cmd_args[0] == 'decode':
        decode_pkt = True

    if int(kern.globals.mb_uses_mcache) == 0:
        for mbuf in kmemory.Zone("mbuf").iter_allocated(gettype("mbuf")):
            MbufDecode(value(mbuf.AddressOf()), decode_pkt)
        return

    slabstbl = kern.globals.slabstbl
    nslabspmb = int(((1 << MBSHIFT) >> unsigned(kern.globals.page_shift)))
    mbutl = cast(kern.globals.mbutl, 'unsigned char *')
    x = unsigned(kern.globals.maxslabgrp)
    while x >= 0:
        slg = slabstbl[x]
        if (slg == 0):
            x -= 1
            continue
        j = 0
        while j < nslabspmb:
            sl = addressof(slg.slg_slab[j])
            obj = sl.sl_base
            # Ignore slabs with a single chunk
            # since that's unlikely to contain an mbuf
            # (more likely a cluster).
            if sl.sl_chunks > 1:
                z = 0
                c = sl.sl_len // sl.sl_chunks

                while z < sl.sl_chunks:
                    obj = kern.GetValueFromAddress(sl.sl_base + c * z)
                    mbuf = cast(obj, 'struct mbuf *')
                    MbufDecode(mbuf, decode_pkt)
                    z += 1
            j += 1
        x -= 1

# EndMacro: mbuf_walk_slabs

def GetMbufMcaPtr(m, cl):
    pgshift = int(kern.globals.page_shift)
    ix = int((m - Cast(kern.globals.mbutl, 'char *')) >> pgshift)
    page_addr = (Cast(kern.globals.mbutl, 'char *') + (ix << pgshift))


    if (int(cl) == 0):
        midx = int((m - page_addr) >> 8)
        mca = kern.globals.mclaudit[ix].cl_audit[midx]
    elif (int(cl) == 1):
        midx = int((m - page_addr) >> 11)
        mca = kern.globals.mclaudit[ix].cl_audit[midx]
    elif (int(cl) == 2):
        midx = int((m - page_addr) >> 12)
        mca = kern.globals.mclaudit[ix].cl_audit[midx]
    else:
        mca = kern.globals.mclaudit[ix].cl_audit[0]
    return Cast(mca, 'mcache_audit_t *')

def GetMbufSlab(m):
    pgshift = int(kern.globals.page_shift)
    gix = int((Cast(m, 'char *') - Cast(kern.globals.mbutl, 'char *')) >> MBSHIFT)
    slabstbl = kern.globals.slabstbl
    ix = int((Cast(m, 'char *') - Cast(slabstbl[gix].slg_slab[0].sl_base, 'char *')) >> pgshift)
    return addressof(slabstbl[gix].slg_slab[ix])

def GetMbufBuf2Mca(m):
    sl = GetMbufSlab(m)
    mca = GetMbufMcaPtr(m, sl.sl_class)
    return str(mca)

def GetMbufWalkAllSlabs(show_a, show_f, show_tr):
    out_string = ""

    kern.globals.slabstbl[0]

    x = 0
    total = 0
    total_a = 0
    total_f = 0

    if (show_a and not(show_f)):
        out_string += "Searching only for active... \n"
    if (not(show_a) and show_f):
        out_string += "Searching only for inactive... \n"
    if (show_a and show_f):
        out_string += "Displaying all... \n"

    if (kern.ptrsize == 8):
        show_mca_string_format = "{0:>4s} {1:>4s} {2:>16s} {3:>16s} {4:>16} {5:>12s} {6:12s}"
        out_string += show_mca_string_format.format("slot", "idx", "slab address", "mca address", "obj address", "type", "allocation state\n")
    else:
        show_mca_string_format = "{0:4s} {1:4s} {2:8s} {3:8s} {4:8} {5:12s} {6:12s}"
        out_string += show_mca_string_format.format("slot", "idx", "slab address", "mca address", "obj address", "type", "allocation state\n")

    nslabspmb = unsigned((1 << MBSHIFT) >> unsigned(kern.globals.page_shift))
    while (x < unsigned(kern.globals.slabgrp)):
        slg = kern.globals.slabstbl[x]
        y = 0
        while (y < nslabspmb):
            sl = addressof(slg.slg_slab[y])
            base = sl.sl_base
            if (base == 0):
                break

            mca = GetMbufMcaPtr(base, sl.sl_class)
            first = 1

            while ((Cast(mca, 'int') != 0) and (unsigned(mca.mca_addr) != 0)):
                printmca = 0
                if (mca.mca_uflags & (MB_INUSE | MB_COMP_INUSE)):
                    total_a = total_a + 1
                    printmca = show_a
                    if (show_tr > 2) and (mca.mca_uflags & MB_SCVALID) == 0:
                        printmca = 0
                else:
                    total_f = total_f + 1
                    printmca = show_f

                if (printmca != 0):
                    if (first == 1):
                        if (kern.ptrsize == 8):
                            mca_string_format = "{0:4d} {1:4d} 0x{2:16x} "
                            out_string += mca_string_format.format(x, y, sl)
                        else:
                            mca_string_format = "{0:4d} {1:4d} 0x{02:8x} "
                            out_string += mca_string_format.format(x, y, sl)
                    else:
                        if (kern.ptrsize == 8):
                            out_string += "                             "
                        else:
                            out_string += "                     "

                    if (kern.ptrsize == 8):
                        mca_string_format = "0x{0:16x} 0x{1:16x}"
                        out_string += mca_string_format.format(mca, mca.mca_addr)
                    else:
                        mca_string_format = "0x{0:8x} 0x{1:8x}"
                        out_string += mca_string_format.format(mca, mca.mca_addr)

                    out_string += GetMbufMcaCtype(mca, 0)

                    if (mca.mca_uflags & (MB_INUSE | MB_COMP_INUSE)):
                        out_string += "active        "
                    else:
                        out_string += "       freed "
                    if (show_tr > 1) and (mca.mca_uflags & MB_SCVALID):
                        m = Cast(mca.mca_addr, 'struct mbuf *')
                        mbuf_string = GetMbufFlags(m)
                        mbuf_string += " " + GetMbufPktCrumbs(m)
                        if (mbuf_string != ""):
                            if (kern.ptrsize == 8):
                                out_string += "\n                              " + mbuf_string
                            else:
                                out_string += "\n                      " + mbuf_string
                    if (first == 1):
                        first = 0

                    out_string += "\n"
                    total = total + 1

                    if (show_tr != 0):
                        if (mca.mca_next_trn == 0):
                            trn = 1
                        else:
                            trn = 0
                        out_string += "Transaction " + str(int(trn)) + " at " + str(int(mca.mca_trns[int(trn)].mca_tstamp)) + " by thread: 0x" + str(hex(mca.mca_trns[int(trn)].mca_thread)) + ":\n"
                        cnt = 0
                        while (cnt < mca.mca_trns[int(trn)].mca_depth):
                            kgm_pc = mca.mca_trns[int(trn)].mca_stack[int(cnt)]
                            out_string += str(int(cnt) + 1) + " "
                            out_string += GetPc(kgm_pc)
                            cnt += 1

                    print(out_string)
                    out_string = ""
                mca = mca.mca_next

            y += 1

        x += 1

    if (total and show_a and show_f):
        out_string += "total objects = " + str(int(total)) + "\n"
        out_string += "active/unfreed objects = " + str(int(total_a)) + "\n"
        out_string += "freed/in_cache objects = " + str(int(total_f)) + "\n"

    return out_string

def GetMbufFlagsAsString(mbuf_flags):
    flags = (unsigned)(mbuf_flags & 0xff)
    out_string = ""
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += mbuf_flags_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def GetMbufFlags(m):
    out_string = ""
    if (m != 0):
        if kern.globals.mb_uses_mcache == 1:
            mhflags = m.m_hdr.mh_flags
        else:
            mhflags = m.M_hdr_common.M_hdr.mh_flags
        out_string += "m_flags: " + hex(mhflags)
        if (mhflags != 0):
             out_string += " " + GetMbufFlagsAsString(mhflags)
    return out_string

MBUF_TYPES = [None] * 32
MBUF_TYPES[0] = "MT_FREE"
MBUF_TYPES[1] = "MT_DATA"
MBUF_TYPES[2] = "MT_HEADER"
MBUF_TYPES[3] = "MT_SOCKET"
MBUF_TYPES[4] = "MT_PCB"
MBUF_TYPES[5] = "MT_RTABLE"
MBUF_TYPES[6] = "MT_HTABLE"
MBUF_TYPES[7] = "MT_ATABLE"
MBUF_TYPES[8] = "MT_SONAME"
# 9 not used
MBUF_TYPES[10] = "MT_SOOPTS"
MBUF_TYPES[11] = "MT_FTABLE"
MBUF_TYPES[12] = "MT_RIGHTS"
MBUF_TYPES[13] = "MT_IFADDR"
MBUF_TYPES[14] = "MT_CONTROL"
MBUF_TYPES[15] = "MT_OOBDATA"
MBUF_TYPES[16] = "MT_TAG"

def GetMbufType(m):
    out_string = ""
    if (m != 0):
        if kern.globals.mb_uses_mcache == 1:
            mhtype = m.m_hdr.mh_type
        else:
            mhtype = m.M_hdr_common.M_hdr.mh_type
        out_string += "type: " + MBUF_TYPES[mhtype]
    return out_string

# Macro: mbuf_show_m_flags
@lldb_command('mbuf_show_m_flags')
def MbufShowFlags(cmd_args=None):
    """ Return a formatted string description of the mbuf flags
    """
    m = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    print(GetMbufFlags(m))

def GetMbufPktCrumbsAsString(mbuf_crumbs):
    flags = (unsigned)(mbuf_crumbs & 0xffff)
    out_string = ""
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += mbuf_pkt_crumb_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def GetMbufPktCrumbs(m):
    out_string = ""
    if (m != 0):
        if kern.globals.mb_uses_mcache == 1:
            mhflags = m.m_hdr.mh_flags
        else:
            mhflags = m.M_hdr_common.M_hdr.mh_flags
        if (mhflags & M_PKTHDR) != 0:
            if kern.globals.mb_uses_mcache == 1:
                pktcrumbs = m.M_dat.MH.MH_pkthdr.pkt_crumbs
            else:
                pktcrumbs = m.M_hdr_common.M_pkthdr.pkt_crumbs
            out_string += "pkt_crumbs: 0x{0:x}".format(pktcrumbs)
            if (pktcrumbs != 0):
                out_string += " " + GetMbufPktCrumbsAsString(pktcrumbs)
    return out_string

# Macro: mbuf_showpktcrumbs
@lldb_command('mbuf_showpktcrumbs')
def MbufShowPktCrumbs(cmd_args=None):
    """ Print the packet crumbs of an mbuf object mca
    """
    m = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')
    print(GetMbufPktCrumbs(m))

def GetMbufMcaCtype(mca, vopt):
    cp = mca.mca_cache
    mca_class = unsigned(cp.mc_private)
    csize = unsigned(kern.globals.mbuf_table[mca_class].mtbl_stats.mbcl_size)
    done = 0
    out_string = "    "
    if (csize == MSIZE):
        if (vopt):
            out_string += "M (mbuf) "
        else:
            out_string += "M     "
        return out_string
    if (csize == MCLBYTES):
        if (vopt):
            out_string += "CL (2K cluster) "
        else:
            out_string += "CL     "
        return out_string
    if (csize == MBIGCLBYTES):
        if (vopt):
            out_string += "BCL (4K cluster) "
        else:
            out_string += "BCL     "
        return out_string
    if (csize == M16KCLBYTES):
        if (vopt):
            out_string += "JCL (16K cluster) "
        else:
            out_string += "JCL     "
        return out_string

    if (csize == (MSIZE + MCLBYTES)):
        if (mca.mca_uflags & MB_SCVALID):
            if (mca.mca_uptr):
                out_string += "M+CL  "
                if vopt:
                    out_string += "(paired mbuf, 2K cluster) "
            else:
                out_string += "M-CL  "
                if vopt:
                    out_string += "(unpaired mbuf, 2K cluster) "
        else:
            if (mca.mca_uptr):
                out_string += "CL+M  "
                if vopt:
                    out_string += "(paired 2K cluster, mbuf) "
            else:
                out_string += "CL-M  "
                if vopt:
                    out_string += "(unpaired 2K cluster, mbuf) "
        return out_string

    if (csize == (MSIZE + MBIGCLBYTES)):
        if (mca.mca_uflags & MB_SCVALID):
            if (mca.mca_uptr):
                out_string += "M+BCL  "
                if vopt:
                    out_string += "(paired mbuf, 4K cluster) "
            else:
                out_string += "M-BCL  "
                if vopt:
                    out_string += "(unpaired mbuf, 4K cluster) "
        else:
            if (mca.mca_uptr):
                out_string += "BCL+M  "
                if vopt:
                    out_string += "(paired 4K cluster, mbuf) "
            else:
                out_string += "BCL-m  "
                if vopt:
                    out_string += "(unpaired 4K cluster, mbuf) "
        return out_string

    if (csize == (MSIZE + M16KCLBYTES)):
        if (mca.mca_uflags & MB_SCVALID):
            if (mca.mca_uptr):
                out_string += "M+BCL  "
                if vopt:
                    out_string += "(paired mbuf, 4K cluster) "
            else:
                out_string += "M-BCL  "
                if vopt:
                    out_string += "(unpaired mbuf, 4K cluster) "
        else:
            if (mca.mca_uptr):
                out_string += "BCL+M  "
                if vopt:
                    out_string += "(paired 4K cluster, mbuf) "
            else:
                out_string += "BCL-m  "
                if vopt:
                    out_string += "(unpaired 4K cluster, mbuf) "
        return out_string

    out_string += "unknown: " + cp.mc_name
    return out_string


def GetPointerAsString(kgm_pc):
    if (kern.ptrsize == 8):
        pointer_format_string = "0x{0:<16x} "
    else:
        pointer_format_string = "0x{0:<8x} "
    return pointer_format_string.format(kgm_pc)

def GetPc(kgm_pc):
    out_string = GetSourceInformationForAddress(unsigned(kgm_pc)) + "\n"
    return out_string


def GetMbufWalkZone(show_a, show_f, show_tr):
    out_string = ""
    total = 0
    total_a = 0
    total_f = 0
    if (show_a and not(show_f)):
        out_string += "Searching only for active... \n"
    if (not(show_a) and show_f):
        out_string += "Searching only for inactive... \n"
    if (show_a and show_f):
        out_string += "Displaying all... \n"
    f = "{0:>18s} {1:s}\n"
    out_string += f.format("address", "type flags and crumbs")
    print(out_string)
    if show_a:
        for mbuf_sbv in kmemory.Zone("mbuf").iter_allocated(gettype("mbuf")):
            mbuf = value(mbuf_sbv.AddressOf())
            total_a += 1
            total += 1
            mbuf_string = GetMbufFlags(mbuf)
            mbuf_string += " " + GetMbufType(mbuf)
            mbuf_string += " " + GetMbufPktCrumbs(mbuf)
            if mbuf_string != "":
                out_string = f.format(hex(mbuf), mbuf_string)
            print(out_string)
            if show_tr:
                print(lldb_run_command('kasan whatis {addr}'.format(addr=hex(mbuf))))
    if show_f:
        for mbuf_sbv in kmemory.Zone("mbuf").iter_free(gettype("mbuf")):
            mbuf = value(mbuf_sbv.AddressOf())
            total_f += 1
            total += 1
            mbuf_string = GetMbufFlags(mbuf)
            mbuf_string += " " + GetMbufType(mbuf)
            mbuf_string += " " + GetMbufPktCrumbs(mbuf)
            if mbuf_string != "":
                out_string = f.format(hex(mbuf), mbuf_string)
            print(out_string)
            if show_tr:
                print(lldb_run_command('kasan whatis {addr}'.format(addr=hex(mbuf))))

    if total and show_a and show_f:
        out_string += "total objects = " + str(int(total)) + "\n"
        out_string += "active/unfreed objects = " + str(int(total_a)) + "\n"
        out_string += "freed/in_cache objects = " + str(int(total_f)) + "\n"
        print(out_string)


# Macro: mbuf_showactive
@lldb_command('mbuf_showactive')
def MbufShowActive(cmd_args=None):
    """ Print all active/in-use mbuf objects
        Pass 1 to show the most recent transaction stack trace
        Pass 2 to also display the mbuf flags and packet crumbs
        Pass 3 to limit display to mbuf and skip clusters
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        if cmd_args:
            GetMbufWalkZone(1, 0, ArgumentStringToInt(cmd_args[0]))
        else:
            GetMbufWalkZone(1, 0, 0)
    else:
        if cmd_args:
            print(GetMbufWalkAllSlabs(1, 0, ArgumentStringToInt(cmd_args[0])))
        else:
            print(GetMbufWalkAllSlabs(1, 0, 0))
# EndMacro: mbuf_showactive


# Macro: mbuf_showinactive
@lldb_command('mbuf_showinactive')
def MbufShowInactive(cmd_args=None):
    """ Print all freed/in-cache mbuf objects
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        GetMbufWalkZone(0, 1, 0)
    else:
        print(GetMbufWalkAllSlabs(0, 1, 0))
# EndMacro: mbuf_showinactive

# Macro: mbuf_show_type_summary
@lldb_command('mbuf_show_type_summary')
def MbufShowTypeSummary(cmd_args=None):
    """
    Print types of all allocated mbufs.
    Only supported on Apple Silicon.
    """
    types = [0] * 32
    for mbuf_sbv in kmemory.Zone("mbuf").iter_allocated(gettype("mbuf")):
        mbuf = value(mbuf_sbv.AddressOf())
        mhtype = mbuf.M_hdr_common.M_hdr.mh_type
        types[mhtype] += 1
    for mbuf_sbv in kmemory.Zone("mbuf").iter_free(gettype("mbuf")):
        mbuf = value(mbuf_sbv.AddressOf())
        mhtype = mbuf.M_hdr_common.M_hdr.mh_type
        types[mhtype] += 1

    print("mbuf types allocated and in the caches:")
    for t in range(len(MBUF_TYPES)):
        if types[t] != 0:
            print(MBUF_TYPES[t], types[t])

# EndMacro: mbuf_show_type_summary


# Macro: mbuf_showmca
@lldb_command('mbuf_showmca')
def MbufShowMca(cmd_args=None):
    """ Print the contents of an mbuf mcache audit structure
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis or zstack_findelem")
        return
    out_string = ""
    pgshift = unsigned(kern.globals.page_shift)
    if cmd_args:
        mca = kern.GetValueFromAddress(cmd_args[0], 'mcache_audit_t *')
        cp = mca.mca_cache
        out_string += "object type:\t"
        out_string += GetMbufMcaCtype(mca, 1)
        out_string += "\nControlling mcache :\t" + hex(mca.mca_cache) + " (" + str(cp.mc_name) + ")\n"
        if (mca.mca_uflags & MB_INUSE):
            out_string += " inuse"
        if (mca.mca_uflags & MB_COMP_INUSE):
            out_string += " comp_inuse"
        if (mca.mca_uflags & MB_SCVALID):
            out_string += " scvalid"
        out_string += "\n"
        if (mca.mca_uflags & MB_SCVALID):
            mbutl = Cast(kern.globals.mbutl, 'unsigned char *')
            ix = (mca.mca_addr - mbutl) >> pgshift
            clbase = mbutl + (ix << pgshift)
            mclidx = (mca.mca_addr - clbase) >> 8
            out_string += "mbuf obj :\t\t" + hex(mca.mca_addr) + "\n"
            out_string += "mbuf index :\t\t" + str(mclidx + 1) + " (out of 16) in cluster base " + hex(clbase) + "\n"
            if (int(mca.mca_uptr) != 0):
                peer_mca = cast(mca.mca_uptr, 'mcache_audit_t *')
                out_string += "paired cluster obj :\t" + hex(peer_mca.mca_addr) + " (mca " + hex(peer_mca) + ")\n"
            out_string += "saved contents :\t" + hex(mca.mca_contents) + " (" + str(int(mca.mca_contents_size)) + " bytes)\n"
        else:
            out_string += "cluster obj :\t\t" + hex(mca.mca_addr) + "\n"
            if (mca.mca_uptr != 0):
                peer_mca = cast(mca.mca_uptr, 'mcache_audit_t *')
                out_string += "paired mbuf obj :\t" + hex(peer_mca.mca_addr) + " (mca " + hex(peer_mca) + ")\n"

        for idx in range(unsigned(kern.globals.mca_trn_max), 0, -1):
                trn = (mca.mca_next_trn + idx - 1) % unsigned(kern.globals.mca_trn_max)
                out_string += "transaction {:d} (tstamp {:d}, thread 0x{:x}):\n".format(trn, mca.mca_trns[trn].mca_tstamp, mca.mca_trns[trn].mca_thread)
                cnt = 0
                while (cnt < mca.mca_trns[trn].mca_depth):
                    kgm_pc = mca.mca_trns[trn].mca_stack[cnt]
                    out_string += "  " + str(cnt + 1) + ".  "
                    out_string += GetPc(kgm_pc)
                    cnt += 1

        msc = cast(mca.mca_contents, 'mcl_saved_contents_t *')
        msa = addressof(msc.sc_scratch)
        if (mca.mca_uflags & MB_SCVALID):
            if (msa.msa_depth > 0):
                out_string += "Recent scratch transaction (tstamp {:d}, thread 0x{:x}):\n".format(msa.msa_tstamp, msa.msa_thread)
                cnt = 0
                while (cnt < msa.msa_depth):
                    kgm_pc = msa.msa_stack[cnt]
                    out_string += "  " + str(cnt + 1) + ".  "
                    out_string += GetPc(kgm_pc)
                    cnt += 1

            if (msa.msa_pdepth > 0):
                out_string += "previous scratch transaction (tstamp {:d}, thread 0x{:x}):\n".format(msa.msa_ptstamp, msa.msa_pthread)
        if (msa):
            cnt = 0
            while (cnt < msa.msa_pdepth):
                kgm_pc = msa.msa_pstack[cnt]
                out_string += "  " + str(cnt + 1) + ".  "
                out_string += GetPc(kgm_pc)
                cnt += 1
    else:
        out_string += "Missing argument 0 in user function."

    print(out_string)
# EndMacro: mbuf_showmca


# Macro: mbuf_showall
@lldb_command('mbuf_showall')
def MbufShowAll(cmd_args=None):
    """ Print all mbuf objects
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        GetMbufWalkZone(1, 1, 1)
    else:
        print(GetMbufWalkAllSlabs(1, 1, 1))
# EndMacro: mbuf_showall

# Macro: mbuf_countchain
@lldb_command('mbuf_countchain')
def MbufCountChain(cmd_args=None):
    """ Count the length of an mbuf chain
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")

    mp = kern.GetValueFromAddress(cmd_args[0], 'mbuf *')

    pkt = 0
    nxt = 0

    while (mp):
        pkt = pkt + 1
        if kern.globals.mb_uses_mcache == 1:
            mn = mp.m_hdr.mh_next
        else:
            mn = mp.M_hdr_common.M_hdr.mh_next
        while (mn):
            nxt = nxt + 1
            if kern.globals.mb_uses_mcache == 1:
                mn = mn.m_hdr.mh_next
            else:
                mn = mn.M_hdr_common.M_hdr.mh_next
            print1("mp 0x{:x} mn 0x{:x}".format(mp, mn))

        if kern.globals.mb_uses_mcache == 1:
            mp = mp.m_hdr.mh_nextpkt
        else:
            mp = mp.M_hdr_common.M_hdr.mh_nextpkt

        if (((pkt + nxt) % 50) == 0):
            print(" ..." + str(pkt_nxt))

    print("Total: " + str(pkt + nxt) + " (via m_next: " + str(nxt) + ")")
# EndMacro: mbuf_countchain

# Macro: mbuf_topleak
@lldb_command('mbuf_topleak')
def MbufTopLeak(cmd_args=None):
    """ Print the top suspected mbuf leakers
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use zleak")
        return
    topcnt = 0
    if (int(len(cmd_args)) > 0 and int(cmd_args[0]) < 5):
        maxcnt = cmd_args[0]
    else:
        maxcnt = 5
    while (topcnt < maxcnt):
        print(GetMbufTraceLeak(kern.globals.mleak_top_trace[topcnt]))
        topcnt += 1

# EndMacro: mbuf_topleak

def GetMbufTraceLeak(trace):
    out_string = ""
    if (trace != 0 and trace.allocs != 0):
        out_string += hex(trace) + ":" + str(trace.allocs) + " outstanding allocs\n"
        out_string += "Backtrace saved " + str(trace.depth) + " deep\n"
        if (trace.depth != 0):
            cnt = 0
            while (cnt < trace.depth):
                out_string += str(cnt + 1) + ": "
                out_string += GetPc(trace.addr[cnt])
                out_string += "\n"
                cnt += 1
    return out_string

@lldb_command('mbuf_largefailures')
def MbufLargeFailures(cmd_args=None):
    """ Print the largest allocation failures
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, this macro is not available. use zleak to detect leaks")
        return
    topcnt = 0
    if (int(len(cmd_args)) > 0 and int(cmd_args[0]) < 5):
        maxcnt = cmd_args[0]
    else:
        maxcnt = 5
    while (topcnt < maxcnt):
        trace = kern.globals.mtracelarge_table[topcnt]
        if (trace.size == 0):
            topcnt += 1
            continue
        print(str(trace.size))
        if (trace.depth != 0):
            cnt = 0
            while (cnt < trace.depth):
                print(str(cnt + 1) + ": " + GetPc(trace.addr[cnt]))
                cnt += 1
        topcnt += 1


# Macro: mbuf_traceleak
@lldb_command('mbuf_traceleak')
def MbufTraceLeak(cmd_args=None):
    """ Print the leak information for a given leak address
        Given an mbuf leak trace (mtrace) structure address, print out the
        stored information with that trace
        syntax: (lldb) mbuf_traceleak <addr>
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return

    trace = kern.GetValueFromAddress(cmd_args[0], 'mtrace *')
    print(GetMbufTraceLeak(trace))
# EndMacro: mbuf_traceleak


# Macro: mcache_walkobj
@lldb_command('mcache_walkobj')
def McacheWalkObject(cmd_args=None):
    """ Given a mcache object address, walk its obj_next pointer
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return

    out_string = ""
    p = kern.GetValueFromAddress(cmd_args[0], 'mcache_obj_t *')
    cnt = 1
    total = 0
    while (p):
        mcache_object_format = "{0:>4d}: 0x{1:>16x}"
        out_string += mcache_object_format.format(cnt, p) + "\n"
        p = p.obj_next
        cnt += 1
    print(out_string)
# EndMacro: mcache_walkobj

# Macro: mcache_stat
@lldb_command('mcache_stat')
def McacheStat(cmd_args=None):
    """ Print all mcaches in the system.
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return

    head = kern.globals.mcache_head
    out_string = ""
    mc = cast(head.lh_first, 'mcache *')
    if (kern.ptrsize == 8):
        mcache_stat_format_string = "{0:<24s} {1:>8s} {2:>20s} {3:>5s} {4:>5s} {5:>20s} {6:>30s} {7:>18s}"
    else:
        mcache_stat_format_string = "{0:<24s} {1:>8s} {2:>12s} {3:>5s} {4:>5s} {5:>12s} {6:>30s} {7:>18s}"

    if (kern.ptrsize == 8):
        mcache_stat_data_format_string = "{0:<24s} {1:>12s} {2:>20s} {3:>5s} {4:>5s} {5:>22s} {6:>12d} {7:>8d} {8:>8d} {9:>18d}"
    else:
        mcache_stat_data_format_string = "{0:<24s} {1:>12s} {2:>12s} {3:>5s} {4:>5s} {5:>14s} {6:>12d} {7:>8d} {8:>8d} {9:>18d}"

    out_string += mcache_stat_format_string.format("cache name", "cache state", "cache addr", "buf size", "buf align", "backing zone", "wait     nowait     failed", "bufs incache")
    out_string += "\n"

    ncpu = int(kern.globals.ncpu)
    while mc != 0:
        bktsize = mc.mc_cpu[0].cc_bktsize
        cache_state = ""
        if (mc.mc_flags & MCF_NOCPUCACHE):
            cache_state = "disabled"
        else:
            if (bktsize == 0):
                cache_state = " offline"
            else:
                cache_state = " online"
        if (mc.mc_slab_zone != 0):
            backing_zone = mc.mc_slab_zone
        else:
            if (kern.ptrsize == 8):
                backing_zone = "            custom"
            else:
                backing_zone = "    custom"

        total = 0
        total += mc.mc_full.bl_total * bktsize
        n = 0
        while(n < ncpu):
            ccp = mc.mc_cpu[n]
            if (ccp.cc_objs > 0):
                total += ccp.cc_objs
            if (ccp.cc_pobjs > 0):
                total += ccp.cc_pobjs
            n += 1

        out_string += mcache_stat_data_format_string.format(mc.mc_name, cache_state, hex(mc), str(int(mc.mc_bufsize)), str(int(mc.mc_align)), hex(mc.mc_slab_zone), int(mc.mc_wretry_cnt), int(mc.mc_nwretry_cnt), int(mc.mc_nwfail_cnt), total)
        out_string += "\n"
        mc = cast(mc.mc_list.le_next, 'mcache *')
    print(out_string)
# EndMacro: mcache_stat

# Macro: mcache_showcache
@lldb_command('mcache_showcache')
def McacheShowCache(cmd_args=None):
    """Display the number of objects in cache.
    """
    if int(kern.globals.mb_uses_mcache) == 0:
        print("mcache is disabled, use kasan whatis")
        return
    out_string = ""
    cp = kern.GetValueFromAddress(cmd_args[0], 'mcache_t *')
    bktsize = cp.mc_cpu[0].cc_bktsize
    cnt = 0
    total = 0
    mcache_cache_format = "{0:<4d} {1:>8d} {2:>8d} {3:>8d}"
    out_string += "Showing cache " + str(cp.mc_name) + " :\n\n"
    out_string += " CPU  cc_objs cc_pobjs    total\n"
    out_string += "----  ------- -------- --------\n"
    ncpu = int(kern.globals.ncpu)
    while (cnt < ncpu):
        ccp = cp.mc_cpu[cnt]
        objs = ccp.cc_objs
        if (objs <= 0):
            objs = 0
        pobjs = ccp.cc_pobjs
        if (pobjs <= 0):
            pobjs = 0
        tot_cpu = objs + pobjs
        total += tot_cpu
        out_string += mcache_cache_format.format(cnt, objs, pobjs, tot_cpu)
        out_string += "\n"
        cnt += 1

    out_string += "                       ========\n"
    out_string += "                           " + str(total) + "\n\n"
    total += cp.mc_full.bl_total * bktsize

    out_string += "Total # of full buckets (" + str(int(bktsize)) + " objs/bkt):\t" + str(int(cp.mc_full.bl_total)) + "\n"
    out_string += "Total # of objects cached:\t\t" + str(total) + "\n"
    print(out_string)
# EndMacro: mcache_showcache

# Macro: mbuf_wdlog
@lldb_command('mbuf_wdlog')
def McacheShowWatchdogLog(cmd_args=None):
    """Display the watchdog log
    """
    lldb_run_command('settings set max-string-summary-length 4096')
    print('%s' % lldb_run_command('p/s mbwdog_logging').replace("\\n","\n"))
# EndMacro: mbuf_wdlog
