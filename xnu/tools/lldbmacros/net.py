
""" Please make sure you read the README COMPLETELY BEFORE reading anything below.
    It is very critical that you read coding guidelines in Section E in README file.
"""
from xnu import *
from utils import *
from string import *
from socket import *
import tempfile

import xnudefines
from netdefines import *
from routedefines import *
from mbufdefines import *

def GetDlilIfFlagsAsString(dlil_if_flags):
    """ Return a formatted string description of the dlil interface flags
    """
    out_string = ""
    flags = (unsigned)(dlil_if_flags & 0xffff)
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += dlil_if_flags_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def GetIfFlagsAsString(if_flags):
    """ Return a formatted string description of the interface flags
    """
    out_string = ""
    flags = (unsigned)(if_flags & 0xffff)
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += if_flags_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")


def ShowIfConfiguration(ifnet):
    """ Display ifconfig-like output for the ifnet
    """
    iface = Cast(ifnet, 'ifnet *')
    dlifnet = Cast(ifnet, 'dlil_ifnet *')
    out_string = ""
    format_string = "{0: <s}: flags={1: <x} <{2: <s}> index {3: <d} mtu {4: <d}"
    if iface :
        out_string += format_string.format(iface.if_xname, (iface.if_flags & 0xffff), GetIfFlagsAsString(iface.if_flags), iface.if_index, iface.if_data.ifi_mtu)
        out_string += "\n\tdlil flags=" + hex(dlifnet.dl_if_flags)+ " <" + GetDlilIfFlagsAsString(dlifnet.dl_if_flags) + ">"
        out_string += "\n\t(struct ifnet *)" + hex(ifnet)
        if iface.if_snd and iface.if_snd.ifcq_len :
            out_string += "\n\t" + str(iface.if_snd.ifcq_len)
        if dlifnet.dl_if_inpstorage.dlth_pkts.qlen :
            out_string += "\n\t" + str(dlifnet.dl_if_inpstorage.dlth_pkts.qlen)
    print(out_string)

def GetIfConfiguration(ifname):
    """ Return ifnet structure corresponding to the ifname passed in
    """
    global kern
    ifnets = kern.globals.ifnet_head
    for ifnet in IterateTAILQ_HEAD(ifnets, "if_link") :
        if str(ifnet.if_xname) == ifname :
            return ifnet
    return None

# Macro: net_get_always_on_pktap
@lldb_command('net_get_always_on_pktap')
def NetGetAlwaysOnPktap(cmd_args=None):
    """ Dump the always-on packet capture to /tmp/dump.pktap
    """
    for i in range(0, 10):
        ifnet = GetIfConfiguration("pktap"+str(i))
        if not ifnet:
            continue
        if ifnet.if_bpf == 0:
            ifnet = None
            continue
        if ifnet.if_bpf.bif_dlist.bd_headdrop == 0:
            ifnet = None
            continue

        break

    if not ifnet:
        print("Could not find a pktap interface")
        return

    bpf_d = ifnet.if_bpf.bif_dlist

    f = tempfile.NamedTemporaryFile(prefix="dump-", suffix=".pktap", dir="/tmp/", mode="wb", delete=False)

    err = lldb.SBError()

    if bpf_d.bd_hbuf != 0:
        addr = bpf_d.bd_hbuf[0].GetSBValue().GetLoadAddress()
        hlen = (unsigned(bpf_d.bd_hlen)+(4-1))&~(4-1)
        buf = LazyTarget.GetProcess().ReadMemory(addr, hlen, err)
        if err.fail:
            print("Error, getting sbuf")
        f.write(buf)

    addr = bpf_d.bd_sbuf[0].GetSBValue().GetLoadAddress()
    slen = (unsigned(bpf_d.bd_slen)+(4-1))&~(4-1)
    buf = LazyTarget.GetProcess().ReadMemory(addr, slen, err)
    if err.fail:
        print("Error, getting sbuf")
    f.write(buf)

    print(f.name)
    f.close()
# EndMacro: net_get_always_on_pktap

# Macro: ifconfig
@lldb_command('ifconfig')
def ShowIfconfig(cmd_args=None) :
    """ Display ifconfig-like output, and print the (struct ifnet *) pointers for further inspection
    """
    if cmd_args != None and len(cmd_args) > 0:
        showall = 1
    else:
        showall = 0

    ifnets = kern.globals.ifnet_head
    for ifnet in IterateTAILQ_HEAD(ifnets, "if_link"):
        ShowIfConfiguration(ifnet)
        if (showall == 1):
            print(GetIfaddrs(ifnet))
# EndMacro: ifconfig

#Macro: ifconfig_dlil
@lldb_command('ifconfig_dlil')
def ShowIfconfigDlil(cmd_args=None) :
    """ Display ifconfig-like output for DLIL interface list, print (struct ifnet *) pointer and dlil info for further inspection
    """
    dlil_ifnets = kern.globals.dlil_ifnet_head
    for dlil_ifnet in IterateTAILQ_HEAD(dlil_ifnets, "dl_if_link"):
        ShowIfConfiguration(dlil_ifnet)
        print(GetIfaddrs(Cast(dlil_ifnet, 'ifnet *')))
# EndMacro: ifconfig_dlil

def GetAddressAsStringColonHex(addr, count):
    out_string = ""
    i = 0
    addr_format_string = "{0:02x}"
    while (i < count):
        if (i == 0):
            out_string += addr_format_string.format(unsigned(addr[i]))[-2:]
        else:
            out_string += ":" + addr_format_string.format(unsigned(addr[i]))[-2:]
        i += 1
    return out_string

def GetSocketAddrAsStringUnspec(sockaddr):
    out_string = ""
    out_string += GetAddressAsStringColonHex(sockaddr.sa_data, sockaddr.sa_len - 2)
    return out_string

def GetSocketAddrAsStringUnix(sockaddr):
    sock_unix = Cast(sockaddr, 'sockaddr_un *')
    if (sock_unix == 0):
        return "(null)"
    else:
        if (len(str(sock_unix.sun_path)) > 0):
            return str(sock_unix.sun_path)
        else:
            return "\"\""

def GetInAddrAsString(ia):
    out_string = ""
    inaddr = Cast(ia, 'in_addr *')

    packed_value = struct.pack('I', unsigned(ia.s_addr))
    out_string = inet_ntoa(packed_value)
    return out_string

def GetIn6AddrAsString(ia):
    addr_raw_string = ":".join(["{0:02x}{1:02x}".format(unsigned(ia[i]),
        unsigned(ia[i+1])) for i in range(0, 16, 2)])
    return inet_ntop(AF_INET6, inet_pton(AF_INET6, addr_raw_string))

def GetSocketAddrAsStringInet(sockaddr):
    sock_in = Cast(sockaddr, 'sockaddr_in *')
    return GetInAddrAsString(addressof(sock_in.sin_addr))

def GetSocketAddrAsStringInet6(sockaddr):
    sock_in6 = Cast(sockaddr, 'sockaddr_in6 *')
    return GetIn6AddrAsString(sock_in6.sin6_addr.__u6_addr.__u6_addr8)

def GetSocketAddrAsStringLink(sockaddr):
    sock_link = Cast(sockaddr, 'sockaddr_dl *')
    if sock_link is None:
        return "(null)"
    else:
        out_string = ""
        if (sock_link.sdl_nlen == 0 and sock_link.sdl_alen == 0 and sock_link.sdl_slen == 0):
            out_string = "link#" + str(int(sock_link.sdl_index))
        else:
            out_string += GetAddressAsStringColonHex(addressof(sock_link.sdl_data[sock_link.sdl_nlen]), sock_link.sdl_alen)
    return out_string

def GetSocketAddrAsStringAT(sockaddr):
    out_string = ""
    sock_addr = Cast(sockaddr, 'sockaddr *')
    out_string += GetAddressAsStringColonHex(sockaddr.sa_data, sockaddr.sa_len - 2)
    return out_string

def GetSocketAddrAsString(sockaddr):
    if sockaddr is None :
        return "(null)"
    out_string = ""
    if (sockaddr.sa_family == 0):
        out_string += "UNSPC "
        GetSocketAddrAsStringUnspec(sockaddr)
    elif (sockaddr.sa_family == 1):
        out_string += "UNIX "
        out_string += GetSocketAddrAsStringUnix(sockaddr)
    elif (sockaddr.sa_family == 2):
        out_string += "INET "
        out_string += GetSocketAddrAsStringInet(sockaddr)
    elif (sockaddr.sa_family == 30):
        out_string += "INET6 "
        out_string += GetSocketAddrAsStringInet6(sockaddr)
    elif (sockaddr.sa_family == 18):
        out_string += "LINK "
        out_string += GetSocketAddrAsStringLink(sockaddr)
    elif (sockaddr.sa_family == 16):
        out_string += "ATLK "
        out_string += GetSocketAddrAsStringAT(sockaddr)
    else:
        out_string += "FAM " + str(sockaddr.sa_family)
        out_string += GetAddressAsStringColonHex(sockaddr.sa_data, sockaddr.sa_len)
    return out_string

# Macro: showifaddrs
@lldb_command('showifaddrs')
def ShowIfaddrs(cmd_args=None):
    """ Show the (struct ifnet).if_addrhead list of addresses for the given ifp
    """
    if cmd_args != None and len(cmd_args) > 0 :
        ifp = kern.GetValueFromAddress(cmd_args[0], 'ifnet *')
        if not ifp:
            print("Unknown value passed as argument.")
            return
        i = 1
        for ifaddr in IterateTAILQ_HEAD(ifp.if_addrhead, "ifa_link"):
            format_string = "\t{0: <d}: 0x{1: <x} {2: <s} [{3: <d}]"
            print(format_string.format(i, ifaddr, GetSocketAddrAsString(ifaddr.ifa_addr), ifaddr.ifa_refcnt))
            i += 1
    else :
        print("Missing argument 0 in user function.")
# EndMacro: showifaddrs

def GetIfaddrs(ifp):
    out_string = ""
    if (ifp != 0):
        i = 1
        for ifaddr in IterateTAILQ_HEAD(ifp.if_addrhead, "ifa_link"):
            format_string = "\t{0: <d}: 0x{1: <x} {2: <s} [{3: <d}]"
            out_string += format_string.format(i, ifaddr, GetSocketAddrAsString(ifaddr.ifa_addr), ifaddr.ifa_refcnt) + "\n"
            i += 1
    else:
        out_string += "Missing argument 0 in user function."
    return out_string


def GetCapabilitiesAsString(flags):
    """ Return a formatted string description of the interface flags
    """
    out_string = ""
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += if_capenable_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def GetIfEflagsAsString(if_eflags):
    """ Return a formatted string description of the interface extra flags
    """
    out_string = ""
    flags = unsigned(if_eflags)
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += if_eflags_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def GetIfXflagsAsString(if_xflags):
    """ Return a formatted string description of the interface extended flags
    """
    out_string = ""
    flags = unsigned(if_xflags)
    i = 0
    num = 1
    while num <= flags:
        if flags & num:
            out_string += if_xflags_strings[i] + ","
        i += 1
        num = num << 1
    return out_string.rstrip(",")

def ShowDlilIfnetConfiguration(dlil_ifnet, show_all) :
    """ Formatted display of dlil_ifnet structures
    """
    DLIF_INUSE = 0x1
    DLIF_REUSE = 0x2

    if dlil_ifnet is None :
        return

    dlil_iface = Cast(dlil_ifnet, 'dlil_ifnet *')
    iface = Cast(dlil_ifnet, 'ifnet *')
    out_string = ""
    if (dlil_iface.dl_if_flags & DLIF_REUSE) :
        out_string  += "*"
    format_string = "{0: <s}: flags={1: <x} <{2: <s}> index {3: <d} mtu {4: <d}"
    extended_flags_format_string = "\n\teflags={0: <x} <{1: <s}>"
    extra_flags_format_string = "\n\txflags={0: <x} <{1: <s}>"
    capenabled_format_string = "\n\toptions={0: <x} <{1: <s}>"
    if (dlil_iface.dl_if_flags & DLIF_INUSE) :
        out_string += format_string.format(iface.if_xname, (iface.if_flags & 0xffff), GetIfFlagsAsString(iface.if_flags), iface.if_index, iface.if_data.ifi_mtu)
    else :
        out_string += format_string.format("[" + str(iface.if_name) + str(int(iface.if_unit)) + "]", (iface.if_flags & 0xffff), GetIfFlagsAsString(iface.if_flags), iface.if_index, iface.if_data.ifi_mtu)
    if (iface.if_eflags) :
        out_string += extended_flags_format_string.format(iface.if_eflags, GetIfEflagsAsString(iface.if_eflags))
    if (iface.if_xflags) :
        out_string += extra_flags_format_string.format(iface.if_xflags, GetIfXflagsAsString(iface.if_xflags))
    if (iface.if_capenable) :
        out_string += capenabled_format_string.format(iface.if_capenable, GetCapabilitiesAsString(iface.if_capenable))
    out_string += "\n\t(struct ifnet *)" + hex(dlil_ifnet) + "\n"
    if show_all :
        out_string += GetIfaddrs(iface)
        out_string += "\n"
    print(out_string)

# Macro: showifnets
@lldb_command('showifnets')
def ShowIfnets(cmd_args=None) :
    """ Display ifconfig-like output for all attached and detached interfaces
    """
    showall = 0
    if cmd_args != None and len(cmd_args) > 0 :
        showall = 1
    dlil_ifnets = kern.globals.dlil_ifnet_head
    for dlil_ifnet in IterateTAILQ_HEAD(dlil_ifnets, "dl_if_link"):
        ShowDlilIfnetConfiguration(dlil_ifnet, showall)
# EndMacro: showifnets

# Macro: showifmultiaddrs
@lldb_command('showifmultiaddrs')
def ShowIfMultiAddrs(cmd_args=None) :
    """ Show the list of multicast addresses for the given ifp
    """
    out_string = ""
    if cmd_args != None and len(cmd_args) > 0 :
        ifp = kern.GetValueFromAddress(cmd_args[0], 'ifnet *')
        if not ifp:
            print("Unknown value passed as argument.")
            return
        ifmulti = cast(ifp.if_multiaddrs.lh_first, 'ifmultiaddr *')
        i = 0
        while ifmulti != 0:
            ifma_format_string = "\t{0: <d}: 0x{1: <x} "
            out_string += (ifma_format_string.format(i + 1, ifmulti))
            if (ifmulti.ifma_addr.sa_family == 2):
                if (ifmulti.ifma_ll != 0):
                    out_string += GetSocketAddrAsStringLink(ifmulti.ifma_ll.ifma_addr) + " "
                out_string += GetSocketAddrAsStringInet(ifmulti.ifma_addr)
            if (ifmulti.ifma_addr.sa_family == 30):
                if (ifmulti.ifma_ll != 0):
                    out_string += GetSocketAddrAsStringLink(ifmulti.ifma_ll.ifma_addr) + " "
                out_string += GetSocketAddrAsStringInet6(ifmulti.ifma_addr) + " "
            if (ifmulti.ifma_addr.sa_family == 18):
                out_string += GetSocketAddrAsStringLink(ifmulti.ifma_addr) + " "
            if (ifmulti.ifma_addr.sa_family == 0):
                out_string += GetSocketAddrAsStringUnspec(ifmulti.ifma_addr) + " "
            out_string += "[" + str(int(ifmulti.ifma_refcount)) + "]\n"
            ifmulti = cast(ifmulti.ifma_link.le_next, 'ifmultiaddr *')
            i += 1
        print(out_string)
    else :
        print("Missing argument 0 in user function.")
# EndMacro: showifmultiaddrs

# Macro: showinmultiaddrs
@lldb_command('showinmultiaddrs')
def ShowInMultiAddrs(cmd_args=None) :
    """ Show the contents of IPv4 multicast address records
    """
    out_string = ""
    inmultihead = kern.globals.in_multihead
    inmulti = cast(inmultihead.lh_first, 'in_multi *')
    i = 0
    while inmulti != 0:
        ifp = inmulti.inm_ifp
        inma_format_string = "\t{0: <d}: 0x{1: <x} "
        out_string += inma_format_string.format(i + 1, inmulti) + " "
        out_string += GetInAddrAsString(addressof(inmulti.inm_addr)) + " "
        ifma_format_string = "(ifp 0x{0: <x} [{1: <s}] ifma {2: <x})"
        out_string += ifma_format_string.format(ifp, ifp.if_xname, inmulti.inm_ifma) + "\n"
        inmulti = cast(inmulti.inm_link.le_next, 'in_multi *')
        i += 1
    print(out_string)
# EndMacro: showinmultiaddrs

# Macro: showin6multiaddrs
@lldb_command('showin6multiaddrs')
def ShowIn6MultiAddrs(cmd_args=None) :
    """ Show the contents of IPv6 multicast address records
    """
    out_string = ""
    in6multihead = kern.globals.in6_multihead
    in6multi = cast(in6multihead.lh_first, 'in6_multi *')
    i = 0
    while in6multi != 0:
        ifp = in6multi.in6m_ifp
        inma_format_string = "\t{0: <d}: 0x{1: <x} "
        out_string += inma_format_string.format(i + 1, in6multi) + " "
        out_string += GetIn6AddrAsString((in6multi.in6m_addr.__u6_addr.__u6_addr8)) + " "
        ifma_format_string = "(ifp 0x{0: <x} [{1: <s}] ifma {2: <x})"
        out_string += ifma_format_string.format(ifp, ifp.if_xname, in6multi.in6m_ifma) + "\n"
        in6multi = cast(in6multi.in6m_entry.le_next, 'in6_multi *')
        i += 1
    print(out_string)
# EndMacro: showin6multiaddrs

def GetTcpState(tcpcb):
    out_string = ""
    tp = Cast(tcpcb, 'tcpcb *')
    if (int(tp) != 0):
        if tp.t_state == 0:
            out_string += "CLOSED\t"
        if tp.t_state == 1:
            out_string += "LISTEN\t"
        if tp.t_state == 2:
            out_string += "SYN_SENT\t"
        if tp.t_state == 3:
            out_string += "SYN_RCVD\t"
        if tp.t_state == 4:
            out_string += "ESTABLISHED\t"
        if tp.t_state == 5:
            out_string += "CLOSE_WAIT\t"
        if tp.t_state == 6:
            out_string += "FIN_WAIT_1\t"
        if tp.t_state == 7:
            out_string += "CLOSING\t"
        if tp.t_state == 8:
            out_string += "LAST_ACK\t"
        if tp.t_state == 9:
            out_string += "FIN_WAIT_2\t"
        if tp.t_state == 10:
            out_string += "TIME_WAIT\t"
    return out_string

def GetSocketProtocolAsString(sock):
    out_string = ""
    inpcb = Cast(sock.so_pcb, 'inpcb *')
    if sock.so_proto.pr_protocol == 6:
        out_string += " TCP "
        out_string += GetTcpState(inpcb.inp_ppcb)
    if sock.so_proto.pr_protocol == 17:
        out_string += " UDP "
    if sock.so_proto.pr_protocol == 1:
        out_string += " ICMP "
    if sock.so_proto.pr_protocol == 254:
        out_string += " DIVERT "
    if sock.so_proto.pr_protocol == 255:
        out_string += " RAW "
    return out_string

def GetInAddr4to6AsString(inaddr):
    out_string = ""
    if (inaddr is not None):
        ia = Cast(inaddr, 'unsigned char *')
        inaddr_format_string = "{0:d}:{1:d}:{2:d}:{3:d}"
        out_string += inaddr_format_string.format(unsigned(ia[0]), unsigned(ia[1]), unsigned(ia[2]), unsigned(ia[3]))
    return out_string

def GetInPortAsString(port):
    out_string = ""
    port_string = Cast(port, 'char *')
    port_unsigned = dereference(Cast(port, 'unsigned short *'))

    if ((((port_unsigned & 0xff00) >> 8) == port_string[0])) and (((port_unsigned & 0x00ff) == port_string[1])):
        out_string += ":" + str(int(port_unsigned))
    else:
        out_string += ":" + str(int(((port_unsigned & 0xff00) >> 8) | ((port_unsigned & 0x00ff) << 8)))

    return out_string

def GetIPv4SocketAsString(sock) :
    out_string = ""
    pcb = Cast(sock.so_pcb, 'inpcb *')
    if (pcb == 0):
        out_string += "inpcb: (null) "
    else:
        out_string += "inpcb: " + hex(pcb)
        out_string += GetSocketProtocolAsString(sock)

        out_string += GetInAddr4to6AsString(addressof(pcb.inp_dependladdr.inp46_local.ia46_addr4))
        out_string += GetInPortAsString(addressof(pcb.inp_lport))
        out_string += " -> "
        out_string += GetInAddr4to6AsString(addressof(pcb.inp_dependfaddr.inp46_foreign.ia46_addr4))
        out_string += GetInPortAsString(addressof(pcb.inp_fport))
    return out_string

def GetIPv6SocketAsString(sock) :
    out_string = ""
    pcb = Cast(sock.so_pcb, 'inpcb *')
    if (pcb == 0):
        out_string += "inpcb: (null) "
    else:
        out_string += "inpcb: " + hex(pcb) + " "
        out_string += GetSocketProtocolAsString(sock)

        out_string += GetIn6AddrAsString((pcb.inp_dependladdr.inp6_local.__u6_addr.__u6_addr8))
        out_string += GetInPortAsString(addressof(pcb.inp_lport))
        out_string += " -> "
        out_string += GetIn6AddrAsString((pcb.inp_dependfaddr.inp6_foreign.__u6_addr.__u6_addr8))
        out_string += GetInPortAsString(addressof(pcb.inp_fport))
    return out_string

def GetUnixDomainSocketAsString(sock) :
    out_string = ""
    pcb = Cast(sock.so_pcb, 'unpcb *')
    if (pcb == 0):
        out_string += "unpcb: (null) "
    else:
        out_string += "unpcb: " + hex(pcb)  + " "
        out_string += "unp_vnode: " + hex(pcb.unp_vnode) + " "
        out_string += "unp_conn: " + hex(pcb.unp_conn) + " "
        out_string += "unp_addr: " + GetSocketAddrAsStringUnix(pcb.unp_addr)
    return out_string

def GetVsockSocketAsString(sock) :
    out_string = ""
    pcb = Cast(sock.so_pcb, 'vsockpcb *')
    if (pcb == 0):
        out_string += "vsockpcb: (null) "
    else:
        out_string += "vsockpcb: " + hex(pcb) + " "
        out_string += str(pcb.local_address) + " "
        out_string += str(pcb.remote_address)
    return out_string

def GetSocket(socket) :
    """ Show the contents of a socket
    """
    so = kern.GetValueFromAddress(unsigned(socket), 'socket *')
    if (so):
        out_string = ""
        sock_format_string = "so: 0x{0:<x} options 0x{1:<x} state 0x{2:<x}"
        out_string += sock_format_string.format(so, so.so_options, so.so_state)
        domain = so.so_proto.pr_domain
        domain_name_format_string = " {0:<s} "
        out_string += domain_name_format_string.format(domain.dom_name)
        if (domain.dom_family == 1):
            out_string += GetUnixDomainSocketAsString(so)
        if (domain.dom_family == 2):
            out_string += GetIPv4SocketAsString(so)
        if (domain.dom_family == 30):
            out_string += GetIPv6SocketAsString(so)
        if (domain.dom_family == 40):
            out_string += GetVsockSocketAsString(so)
        out_string += " s=" + str(int(so.so_snd.sb_cc)) + " r=" + str(int(so.so_rcv.sb_cc)) + " usecnt=" + str(int(so.so_usecount))
    else:
        out_string += "(null)"
    return out_string
# EndMacro: showsocket


# Macro: showsocket
@lldb_command('showsocket')
def ShowSocket(cmd_args=None) :
    """ Show the contents of a socket
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    so = kern.GetValueFromAddress(cmd_args[0], 'socket *')
    if (len(str(cmd_args[0])) > 0):
        out_string = ""
        sock_format_string = "so: 0x{0:<x} options 0x{1:<x} state 0x{2:<x}"
        out_string += sock_format_string.format(so, so.so_options, so.so_state)
        domain = so.so_proto.pr_domain
        domain_name_format_string = " {0:<s} "
        out_string += domain_name_format_string.format(domain.dom_name)
        if (domain.dom_family == 1):
            out_string += GetUnixDomainSocketAsString(so)
        if (domain.dom_family == 2):
            out_string += GetIPv4SocketAsString(so)
        if (domain.dom_family == 30):
            out_string += GetIPv6SocketAsString(so)
        if (domain.dom_family == 40):
            out_string += GetVsockSocketAsString(so)
        print(out_string)
    else:
        print("Unknown value passed as argument.")
        return
# EndMacro: showsocket

def GetProcSockets(proc, total_snd_cc, total_rcv_cc, total_sock_fd):
    """ Given a proc_t pointer, display information about its sockets
    """
    out_string = ""

    if proc is None:
        out_string += "Unknown value passed as argument."
    else:
        snd_cc = 0
        rcv_cc = 0
        sock_fd_seen = 0
        """struct  filedesc *"""
        proc_filedesc = addressof(proc.p_fd)
        """struct  fileproc **"""
        proc_ofiles = proc_filedesc.fd_ofiles
        """ high-water mark of fd_ofiles """
        if proc_filedesc.fd_nfiles != 0:
            for fd in range(0, unsigned(proc_filedesc.fd_afterlast)):
                if (unsigned(proc_ofiles[fd]) != 0 and proc_ofiles[fd].fp_glob != 0):
                        fg = proc_ofiles[fd].fp_glob
                        fg_data = Cast(fg.fg_data, 'void *')
                        if (int(fg.fg_ops.fo_type) == 2):
                            if (proc_filedesc.fd_ofileflags[fd] & 4):
                                out_string += "U: "
                            else:
                                out_string += " "
                            out_string += "fd = " + str(fd) + " "
                            if (fg_data != 0):
                                out_string += GetSocket(fg_data)
                                out_string += "\n"

                                so = Cast(fg_data, 'socket *')
                                snd_cc += int(so.so_snd.sb_cc)
                                total_snd_cc[0] += int(so.so_snd.sb_cc)
                                rcv_cc += int(so.so_rcv.sb_cc)
                                total_rcv_cc[0] += int(so.so_rcv.sb_cc)
                                sock_fd_seen += 1
                            else:
                                out_string += ""
        out_string += "total sockets " + str(int(sock_fd_seen)) + " snd_cc " + str(int(snd_cc)) + " rcv_cc " + str(int(rcv_cc)) + "\n"
        total_sock_fd[0] = sock_fd_seen
    return out_string


# Macro: showprocsockets
@lldb_command('showprocsockets')
def ShowProcSockets(cmd_args=None):
    """ Given a proc_t pointer, display information about its sockets
    """
    total_snd_cc = [0]
    total_rcv_cc = [0]
    sock_fd_seen = [0]
    out_string = ""
    if cmd_args != None and len(cmd_args) > 0 :
        proc = kern.GetValueFromAddress(cmd_args[0], 'proc *')

        if not proc:
            print("Unknown value passed as argument.")
            return
        else:
            print(GetProcInfo(proc))
            print(GetProcSockets(proc, total_snd_cc, total_rcv_cc, sock_fd_seen))
    else:
        print("Missing argument 0 in user function.")
# EndMacro: showprocsockets

# Macro: showallprocsockets
@lldb_command('showallprocsockets')
def ShowAllProcSockets(cmd_args=None):
    """Display information about the sockets of all the processes
    """
    total_snd_cc = [0]
    total_rcv_cc = [0]
    for proc in kern.procs:
        sock_fd_seen = [0]
        out_str = ""
        out_str += GetProcSockets(proc, total_snd_cc, total_rcv_cc, sock_fd_seen)
        if sock_fd_seen[0] != 0:
            print("================================================================================")
            print(GetProcInfo(proc))
            print(out_str)
    print ("total_snd_cc: " + str(int(total_snd_cc[0])) + " total_rcv_cc: " + str(int(total_rcv_cc[0])) + "\n")
# EndMacro: showallprocsockets


def GetRtEntryPrDetailsAsString(rte):
    out_string = ""
    rt = Cast(rte, 'rtentry *')
    dst = Cast(rt.rt_nodes[0].rn_u.rn_leaf.rn_Key, 'sockaddr *')
    isv6 = 0
    dst_string_format = "{0:<18s}"
    if (dst.sa_family == AF_INET):
        out_string += dst_string_format.format(GetSocketAddrAsStringInet(dst)) + " "
    else:
        if (dst.sa_family == AF_INET6):
            out_string += dst_string_format.format(GetSocketAddrAsStringInet6(dst)) + " "
            isv6 = 1
        else:
            if (dst.sa_family == AF_LINK):
                out_string += dst_string_format.format(GetSocketAddrAsStringLink(dst))
                if (isv6 == 1):
                    out_string += "                       "
                else:
                    out_string += " "
            else:
                out_string += dst_string_format.format(GetSocketAddrAsStringUnspec(dst)) + " "

    gw = Cast(rt.rt_gateway, 'sockaddr *')
    if (gw.sa_family == AF_INET):
        out_string += dst_string_format.format(GetSocketAddrAsStringInet(gw)) + " "
    else:
        if (gw.sa_family == 30):
            out_string += dst_string_format.format(GetSocketAddrAsStringInet6(gw)) + " "
            isv6 = 1
        else:
            if (gw.sa_family == 18):
                out_string += dst_string_format.format(GetSocketAddrAsStringLink(gw)) + " "
                if (isv6 == 1):
                    out_string += "                       "
                else:
                    out_string += " "
            else:
                dst_string_format.format(GetSocketAddrAsStringUnspec(gw))

    if (rt.rt_flags & RTF_WASCLONED):
        if (kern.ptrsize == 8):
            rt_flags_string_format = "0x{0:<16x}"
            out_string += rt_flags_string_format.format(rt.rt_parent) + " "
        else:
            rt_flags_string_format = "0x{0:<8x}"
            out_string += rt_flags_string_format.format(rt.rt_parent) + " "
    else:
        if (kern.ptrsize == 8):
            out_string += "                   "
        else:
            out_string += "           "

    rt_refcnt_rmx_string_format = "{0:<d} {1:>10d}  "
    out_string += rt_refcnt_rmx_string_format.format(rt.rt_refcnt, rt.rt_rmx.rmx_pksent) + "   "

    rtf_string_format = "{0:>s}"
    if (rt.rt_flags & RTF_UP):
        out_string += rtf_string_format.format("U")
    if (rt.rt_flags & RTF_GATEWAY):
        out_string += rtf_string_format.format("G")
    if (rt.rt_flags & RTF_HOST):
        out_string += rtf_string_format.format("H")
    if (rt.rt_flags & RTF_REJECT):
        out_string += rtf_string_format.format("R")
    if (rt.rt_flags & RTF_DYNAMIC):
        out_string += rtf_string_format.format("D")
    if (rt.rt_flags & RTF_MODIFIED):
        out_string += rtf_string_format.format("M")
    if (rt.rt_flags & RTF_CLONING):
        out_string += rtf_string_format.format("C")
    if (rt.rt_flags & RTF_PRCLONING):
        out_string += rtf_string_format.format("c")
    if (rt.rt_flags & RTF_LLINFO):
        out_string += rtf_string_format.format("L")
    if (rt.rt_flags & RTF_STATIC):
        out_string += rtf_string_format.format("S")
    if (rt.rt_flags & RTF_PROTO1):
        out_string += rtf_string_format.format("1")
    if (rt.rt_flags & RTF_PROTO2):
        out_string += rtf_string_format.format("2")
    if (rt.rt_flags & RTF_PROTO3):
        out_string += rtf_string_format.format("3")
    if (rt.rt_flags & RTF_WASCLONED):
        out_string += rtf_string_format.format("W")
    if (rt.rt_flags & RTF_BROADCAST):
        out_string += rtf_string_format.format("b")
    if (rt.rt_flags & RTF_MULTICAST):
        out_string += rtf_string_format.format("m")
    if (rt.rt_flags & RTF_XRESOLVE):
        out_string += rtf_string_format.format("X")
    if (rt.rt_flags & RTF_BLACKHOLE):
        out_string += rtf_string_format.format("B")
    if (rt.rt_flags & RTF_IFSCOPE):
        out_string += rtf_string_format.format("I")
    if (rt.rt_flags & RTF_CONDEMNED):
        out_string += rtf_string_format.format("Z")
    if (rt.rt_flags & RTF_IFREF):
        out_string += rtf_string_format.format("i")
    if (rt.rt_flags & RTF_PROXY):
        out_string += rtf_string_format.format("Y")
    if (rt.rt_flags & RTF_ROUTER):
        out_string += rtf_string_format.format("r")

    out_string +=  "/"
    out_string += str(rt.rt_ifp.if_name)
    out_string += str(int(rt.rt_ifp.if_unit))
    out_string += "\n"
    return out_string


RNF_ROOT = 2
def GetRtTableAsString(rt_tables):
    out_string = ""
    rn = Cast(rt_tables.rnh_treetop, 'radix_node *')
    rnh_cnt = rt_tables.rnh_cnt

    while (rn.rn_bit >= 0):
        rn = rn.rn_u.rn_node.rn_L

    while 1:
        base = Cast(rn, 'radix_node *')
        while ((rn.rn_parent.rn_u.rn_node.rn_R == rn) and (rn.rn_flags & RNF_ROOT == 0)):
            rn = rn.rn_parent
        rn = rn.rn_parent.rn_u.rn_node.rn_R
        while (rn.rn_bit >= 0):
            rn = rn.rn_u.rn_node.rn_L
        next_rn = rn
        while (base != 0):
            rn = base
            base = rn.rn_u.rn_leaf.rn_Dupedkey
            if ((rn.rn_flags & RNF_ROOT) == 0):
                rt = Cast(rn, 'rtentry *')
                if (kern.ptrsize == 8):
                    rtentry_string_format = "0x{0:<18x}"
                    out_string += rtentry_string_format.format(rt) + " "
                else:
                    rtentry_string_format = "0x{0:<10x}"
                    out_string += rtentry_string_format.format(rt) + " "
                out_string += GetRtEntryPrDetailsAsString(rt) + " "

        rn = next_rn
        if ((rn.rn_flags & RNF_ROOT) != 0):
            break
    return out_string

def GetRtInetAsString():
    rt_tables = kern.globals.rt_tables[2]
    if (kern.ptrsize == 8):
        rt_table_header_format_string = "{0:<18s} {1: <16s} {2:<20s} {3:<16s} {4:<8s} {5:<8s} {6:<8s}"
        print(rt_table_header_format_string.format("rtentry", " dst", "gw", "parent", "Refs", "Use", "flags/if"))
        print(rt_table_header_format_string.format("-" * 18, "-" * 16, "-" * 16, "-" * 16, "-" * 8, "-" * 8, "-" * 8))
        print(GetRtTableAsString(rt_tables))
    else:
        rt_table_header_format_string = "{0:<8s} {1:<16s} {2:<18s} {3:<8s} {4:<8s} {5:<8s} {6:<8s}"
        print(rt_table_header_format_string.format("rtentry", "dst", "gw", "parent", "Refs", "Use", "flags/if"))
        print(rt_table_header_format_string.format("-" * 8, "-" * 16, "-" * 16, "-" * 8, "-" * 8, "-" * 8, "-" * 8))
        print(GetRtTableAsString(rt_tables))

def GetRtInet6AsString():
    rt_tables = kern.globals.rt_tables[30]
    if (kern.ptrsize == 8):
        rt_table_header_format_string = "{0:<18s} {1: <16s} {2:<20s} {3:<16s} {4:<8s} {5:<8s} {6:<8s}"
        print(rt_table_header_format_string.format("rtentry", " dst", "gw", "parent", "Refs", "Use", "flags/if"))
        print(rt_table_header_format_string.format("-" * 18, "-" * 16, "-" * 16, "-" * 16, "-" * 8, "-" * 8, "-" * 8))
        print(GetRtTableAsString(rt_tables))
    else:
        rt_table_header_format_string = "{0:<8s} {1:<16s} {2:<18s} {3:<8s} {4:<8s} {5:<8s} {6:<8s}"
        print(rt_table_header_format_string.format("rtentry", "dst", "gw", "parent", "Refs", "Use", "flags/if"))
        print(rt_table_header_format_string.format("-" * 8, "-" * 16, "-" * 18, "-" * 8, "-" * 8, "-" * 8, "-" * 8))
        print(GetRtTableAsString(rt_tables))

# Macro: show_rt_inet
@lldb_command('show_rt_inet')
def ShowRtInet(cmd_args=None):
    """ Display the IPv4 routing table
    """
    print(GetRtInetAsString())
# EndMacro: show_rt_inet

# Macro: show_rt_inet6
@lldb_command('show_rt_inet6')
def ShowRtInet6(cmd_args=None):
    """ Display the IPv6 routing table
    """
    print(GetRtInet6AsString())
# EndMacro: show_rt_inet6

# Macro: rtentry_showdbg
@lldb_command('rtentry_showdbg')
def ShowRtEntryDebug(cmd_args=None):
    """ Print the debug information of a route entry
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    rtd = kern.GetValueFromAddress(cmd_args[0], 'rtentry_dbg *')
    rtd_summary_format_string = "{0:s} {1:d}"
    out_string += rtd_summary_format_string.format("Total holds : ", rtd.rtd_refhold_cnt) + "\n"
    out_string += rtd_summary_format_string.format("Total releases : ", rtd.rtd_refrele_cnt) + "\n"

    ix = 0
    while (ix < CTRACE_STACK_SIZE):
        kgm_pc = rtd.rtd_alloc.pc[ix]
        if (kgm_pc != 0):
            if (ix == 0):
                out_string += "\nAlloc: (thread " + hex(rtd.rtd_alloc.th) + "):\n"
            out_string += str(int(ix + 1)) + ": "
            out_string += GetSourceInformationForAddress(kgm_pc)
            out_string += "\n"
        ix += 1

    ix = 0
    while (ix < CTRACE_STACK_SIZE):
        kgm_pc = rtd.rtd_free.pc[ix]
        if (kgm_pc != 0):
            if (ix == 0):
                out_string += "\nFree: (thread " + hex(rtd.rtd_free.th) + "):\n"
            out_string += str(int(ix + 1)) + ": "
            out_string += GetSourceInformationForAddress(kgm_pc)
            out_string += "\n"
        ix += 1

    while (cnt < RTD_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = rtd.rtd_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(rtd.rtd_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1

    cnt = 0
    while (cnt < RTD_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = rtd.rtd_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(rtd.rtd_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1

    out_string += "\nTotal locks : " + str(int(rtd.rtd_lock_cnt))
    out_string += "\nTotal unlocks : " + str(int(rtd.rtd_unlock_cnt))

    cnt = 0
    while (cnt < RTD_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = rtd.rtd_lock[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nLock [" + str(int(cnt)) + "] (thread " + hex(rtd.rtd_lock[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1

    cnt = 0
    while (cnt < RTD_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = rtd.rtd_unlock[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nUnlock [" + str(int(cnt)) + "] (thread " + hex(rtd.rtd_unlock[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1

    print(out_string)
# EndMacro: rtentry_showdbg

# Macro: inm_showdbg
@lldb_command('inm_showdbg')
def InmShowDebug(cmd_args=None):
    """ Print the debug information of an IPv4 multicast address
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    inm = kern.GetValueFromAddress(cmd_args[0], 'in_multi_dbg *')
    in_multi_summary_format_string = "{0:s} {1:d}"
    out_string += in_multi_summary_format_string.format("Total holds : ", inm.inm_refhold_cnt)
    out_string += in_multi_summary_format_string.format("Total releases : ", inm.inm_refrele_cnt)

    while (cnt < INM_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = inm.inm_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(inm.inm_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < INM_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = inm.inm_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(inm.inm_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: inm_showdbg

# Macro: ifma_showdbg
@lldb_command('ifma_showdbg')
def IfmaShowDebug(cmd_args=None):
    """ Print the debug information of a link multicast address
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    ifma = kern.GetValueFromAddress(cmd_args[0], 'ifmultiaddr_dbg *')
    link_multi_summary_format_string = "{0:s} {1:d}"
    out_string += link_multi_summary_format_string.format("Total holds : ", ifma.ifma_refhold_cnt) + "\n"
    out_string += link_multi_summary_format_string.format("Total releases : ", ifma.ifma_refrele_cnt) + "\n"

    while (cnt < IFMA_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = ifma.ifma_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(ifma.ifma_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < IFMA_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = ifma.ifma_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(ifma.ifma_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: ifma_showdbg

# Macro: ifpref_showdbg
@lldb_command('ifpref_showdbg')
def IfpRefShowDebug(cmd_args=None):
    """ Print the debug information of an interface ref count
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    dl_if = kern.GetValueFromAddress(cmd_args[0], 'dlil_ifnet_dbg *')
    dl_if_summary_format_string = "{0:s} {1:d}"
    out_string +=  dl_if_summary_format_string.format("Total holds : ", dl_if.dldbg_if_refhold_cnt)
    out_string += dl_if_summary_format_string.format("Total releases : ", dl_if.dldbg_if_refrele_cnt)

    while (cnt < IF_REF_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = dl_if.dldbg_if_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(dl_if.dldbg_if_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < IF_REF_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = dl_if.dldbg_if_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(dl_if.dldbg_if_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: ifpref_showdbg

# Macro: ndpr_showdbg
@lldb_command('ndpr_showdbg')
def ndprShowDebug(cmd_args=None):
    """ Print the debug information of a nd_prefix structure
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    ndpr = kern.GetValueFromAddress(cmd_args[0], 'nd_prefix_dbg *')
    ndpr_summary_format_string = "{0:s} {1:d}"
    out_string += ndpr_summary_format_string.format("Total holds : ", ndpr.ndpr_refhold_cnt)
    out_string += ndpr_summary_format_string.format("Total releases : ", ndpr.ndpr_refrele_cnt)

    while (cnt < NDPR_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = ndpr.ndpr_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(ndpr.ndpr_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < NDPR_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = ndpr.ndpr_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(ndpr.ndpr_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: ndpr_showdbg

# Macro: nddr_showdbg
@lldb_command('nddr_showdbg')
def nddrShowDebug(cmd_args=None):
    """ Print the debug information of a nd_defrouter structure
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    nddr = kern.GetValueFromAddress(cmd_args[0], 'nd_defrouter_dbg *')
    nddr_summary_format_string = "{0:s} {1:d}"
    out_string += nddr_summary_format_string.format("Total holds : ", nddr.nddr_refhold_cnt)
    out_string += nddr_summary_format_string.format("Total releases : ", nddr.nddr_refrele_cnt)

    while (cnt < NDDR_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = nddr.nddr_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(nddr.nddr_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < NDDR_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = nddr.nddr_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(nddr.nddr_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: nddr_showdbg

# Macro: imo_showdbg
@lldb_command('imo_showdbg')
def IpmOptions(cmd_args=None):
    """ Print the debug information of a ip_moptions structure
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    imo = kern.GetValueFromAddress(cmd_args[0], 'ip_moptions_dbg *')
    imo_summary_format_string = "{0:s} {1:d}"
    out_string += imo_summary_format_string.format("Total holds : ", imo.imo_refhold_cnt)
    out_string += imo_summary_format_string.format("Total releases : ", imo.imo_refrele_cnt)

    while (cnt < IMO_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = imo.imo_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(imo.imo_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < IMO_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = imo.imo_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(imo.imo_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: imo_showdbg

# Macro: im6o_showdbg
@lldb_command('im6o_showdbg')
def IpmOptions(cmd_args=None):
    """ Print the debug information of a ip6_moptions structure
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    out_string = ""
    cnt = 0
    im6o = kern.GetValueFromAddress(cmd_args[0], 'ip6_moptions_dbg *')
    im6o_summary_format_string = "{0:s} {1:d}"
    out_string += im6o_summary_format_string.format("Total holds : ", im6o.im6o_refhold_cnt)
    out_string += im6o_summary_format_string.format("Total releases : ", im6o.im6o_refrele_cnt)

    while (cnt < IM6O_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = im6o.im6o_refhold[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nHold [" + str(int(cnt)) + "] (thread " + hex(im6o.im6o_refhold[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    cnt = 0
    while (cnt < IM6O_TRACE_HIST_SIZE):
        ix = 0
        while (ix < CTRACE_STACK_SIZE):
            kgm_pc = im6o.im6o_refrele[cnt].pc[ix]
            if (kgm_pc != 0):
                if (ix == 0):
                    out_string += "\nRelease [" + str(int(cnt)) + "] (thread " + hex(im6o.im6o_refrele[cnt].th) + "):\n"
                out_string += str(int(ix + 1)) + ": "
                out_string += GetSourceInformationForAddress(kgm_pc)
                out_string += "\n"
            ix += 1
        cnt += 1
    print(out_string)
# EndMacro: im6o_showdbg

# Macro: rtentry_trash
@lldb_command('rtentry_trash')
def RtEntryTrash(cmd_args=None):
    """ Walk the list of trash route entries
    """
    out_string = ""
    rt_trash_head = kern.globals.rttrash_head
    rtd = Cast(rt_trash_head.tqh_first, 'rtentry_dbg *')
    rt_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(rtd) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                rtentry ref   hold   rele             dst    gw             parent flags/if\n")
                print("      ----------------- --- ------ ------ --------------- ----- ------------------ -----------\n")
            else:
                print("        rtentry ref   hold   rele             dst    gw     parent flags/if\n")
                print("      --------- --- ------ ------ --------------- ----- ---------- -----------\n")
        out_string += rt_trash_format_string.format(cnt, rtd, rtd.rtd_refhold_cnt - rtd.rtd_refrele_cnt, rtd.rtd_refhold_cnt, rtd.rtd_refrele_cnt) + "   "
        out_string += GetRtEntryPrDetailsAsString(rtd) + "\n"
        rtd = rtd.rtd_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: rtentry_trash

# Macro: show_rtentry
@lldb_command('show_rtentry')
def ShRtEntry(cmd_args=None):
    """ Print rtentry.
    """
    out_string = ""
    rt = kern.GetValueFromAddress(cmd_args[0], 'rtentry *')
    out_string += GetRtEntryPrDetailsAsString(rt) + "\n"
    print(out_string)
# EndMacro: show_rtentry

# Macro: inifa_trash
@lldb_command('inifa_trash')
def InIfaTrash(cmd_args=None):
    """ Walk the list of trash in_ifaddr entries
    """
    out_string = ""
    ifa_trash_head = kern.globals.inifa_trash_head
    ifa = Cast(ifa_trash_head.tqh_first, 'in_ifaddr_dbg *')
    inifa_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(ifa) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                  in_ifa  ref   hold   rele")
                print("      ------------------  ---  ------  ----")
            else:
                print("          in_ifa  ref   hold   rele")
                print("      ----------  ---  ----- ------")
        out_string += inifa_trash_format_string.format(cnt + 1, ifa, ifa.inifa_refhold_cnt - ifa.inifa_refrele_cnt, ifa.inifa_refhold_cnt, ifa.inifa_refrele_cnt) + "   "
        out_string += GetSocketAddrAsStringInet(ifa.inifa.ia_ifa.ifa_addr) + "\n"
        ifa = ifa.inifa_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: inifa_trash

# Macro: in6ifa_trash
@lldb_command('in6ifa_trash')
def In6IfaTrash(cmd_args=None):
    """ Walk the list of trash in6_ifaddr entries
    """
    out_string = ""
    in6ifa_trash_head = kern.globals.in6ifa_trash_head
    ifa = Cast(in6ifa_trash_head.tqh_first, 'in6_ifaddr_dbg *')
    in6ifa_trash_format_string = "{0:4d}: 0x{1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(ifa) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                 in6_ifa  ref   hold   rele")
                print("      ------------------  --- ------ ------")
            else:
                print("         in6_ifa  ref   hold   rele")
                print("      ----------  --- ------ ------")
        out_string += in6ifa_trash_format_string.format(cnt + 1, ifa, ifa.in6ifa_refhold_cnt - ifa.in6ifa_refrele_cnt, ifa.in6ifa_refhold_cnt, ifa.in6ifa_refrele_cnt) + "   "
        out_string += GetSocketAddrAsStringInet6(ifa.in6ifa.ia_ifa.ifa_addr) + "\n"
        ifa = ifa.in6ifa_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: in6ifa_trash

# Macro: inm_trash
@lldb_command('inm_trash')
def InmTrash(cmd_args=None):
    """ Walk the list of trash in_multi entries
    """
    out_string = ""
    inm_trash_head = kern.globals.inm_trash_head
    inm = Cast(inm_trash_head.tqh_first, 'in_multi_dbg *')
    inm_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(inm) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                     inm  ref   hold   rele")
                print("      ------------------  --- ------ ------")
            else:
                print("             inm  ref   hold   rele")
                print("      ----------  --- ------ ------")
        out_string += inm_trash_format_string.format(cnt + 1, inm, inm.inm_refhold_cnt - inm.inm_refrele_cnt, inm.inm_refhold_cnt, inm.inm_refrele_cnt) + "   "
        out_string += GetInAddrAsString(addressof(inm.inm.inm_addr)) + "\n"
        inm = inm.inm_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: inm_trash

# Macro: in6m_trash
@lldb_command('in6m_trash')
def In6mTrash(cmd_args=None):
    """ Walk the list of trash in6_multi entries
    """
    out_string = ""
    in6m_trash_head = kern.globals.in6m_trash_head
    in6m = Cast(in6m_trash_head.tqh_first, 'in6_multi_dbg *')
    in6m_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(in6m) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                    in6m  ref   hold   rele")
                print("      ------------------  --- ------ ------")
            else:
                print("            in6m  ref   hold   rele")
                print("      ----------  --- ------ ------")
        out_string += in6m_trash_format_string.format(cnt + 1, in6m, in6m.in6m_refhold_cnt - in6m.in6m_refrele_cnt, in6m.in6m_refhold_cnt, in6m.in6m_refrele_cnt) + "   "
        out_string += GetIn6AddrAsString(addressof(in6m.in6m.in6m_addr)) + "\n"
        in6m = in6m.in6m_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: in6m_trash

# Macro: ifma_trash
@lldb_command('ifma_trash')
def IfmaTrash(cmd_args=None):
    """ Walk the list of trash ifmultiaddr entries
    """
    out_string = ""
    ifma_trash_head = kern.globals.ifma_trash_head
    ifma = Cast(ifma_trash_head.tqh_first, 'ifmultiaddr_dbg *')
    ifma_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (int(ifma) != 0):
        if (cnt == 0):
            if (kern.ptrsize == 8):
                print("                    ifma  ref   hold   rele")
                print("      ------------------  --- ------ ------")
            else:
                print("            ifma  ref   hold   rele")
                print("      ----------  --- ------ ------")
        out_string += ifma_trash_format_string.format(cnt + 1, ifma, ifma.ifma_refhold_cnt - ifma.ifma_refrele_cnt, ifma.ifma_refhold_cnt, ifma.ifma_refrele_cnt) + "   "
        out_string += GetSocketAddrAsString(ifma.ifma.ifma_addr) + "\n"
        out_string += " @ " + ifma.ifma.ifma_ifp.if_xname
        ifma = ifma.ifma_trash_link.tqe_next
        cnt += 1
    print(out_string)
# EndMacro: ifma_trash

def GetInPcb(pcb, proto):
    out_string = ""
    out_string += hex(pcb)

    if (proto == IPPROTO_TCP):
        out_string +=  " tcp"
    elif (proto == IPPROTO_UDP):
        out_string += " udp"
    elif (proto == IPPROTO_RAW):
        out_string += " raw"
    else:
        out_string += str(proto) +  "."

    if (pcb.inp_vflag & INP_IPV4):
        out_string += "4 "
    if (pcb.inp_vflag & INP_IPV6):
        out_string += "6 "

    if (pcb.inp_vflag & INP_IPV4):
        out_string += "                                       "
        out_string += GetInAddrAsString(addressof(pcb.inp_dependladdr.inp46_local.ia46_addr4))
    else:
        out_string += "  "
        out_string += GetIn6AddrAsString((pcb.inp_dependladdr.inp6_local.__u6_addr.__u6_addr8))

    out_string += " "
    out_string += Getntohs(pcb.inp_lport)
    out_string += " "

    if (pcb.inp_vflag & INP_IPV4):
        out_string += "                                 "
        out_string += GetInAddrAsString(addressof(pcb.inp_dependfaddr.inp46_foreign.ia46_addr4))
    else:
        out_string += GetIn6AddrAsString((pcb.inp_dependfaddr.inp6_foreign.__u6_addr.__u6_addr8))

    out_string += " "
    out_string += Getntohs(pcb.inp_fport)
    out_string += " "

    if (proto == IPPROTO_TCP):
        out_string += GetTcpState(pcb.inp_ppcb)

    out_string += "\n\t"
    if (pcb.inp_flags & INP_RECVOPTS):
        out_string += "recvopts "
    if (pcb.inp_flags & INP_RECVRETOPTS):
        out_string += "recvretopts "
    if (pcb.inp_flags & INP_RECVDSTADDR):
        out_string += "recvdstaddr "
    if (pcb.inp_flags & INP_HDRINCL):
        out_string += "hdrincl "
    if (pcb.inp_flags & INP_HIGHPORT):
        out_string += "highport "
    if (pcb.inp_flags & INP_LOWPORT):
        out_string += "lowport "
    if (pcb.inp_flags & INP_ANONPORT):
        out_string += "anonport "
    if (pcb.inp_flags & INP_RECVIF):
        out_string += "recvif "
    if (pcb.inp_flags & INP_MTUDISC):
        out_string += "mtudisc "
    if (pcb.inp_flags & INP_STRIPHDR):
        out_string += "striphdr "
    if (pcb.inp_flags & INP_RECV_ANYIF):
        out_string += "recv_anyif "
    if (pcb.inp_flags & INP_INADDR_ANY):
        out_string += "inaddr_any "
    if (pcb.inp_flags & INP_RECVTTL):
        out_string += "recvttl "
    if (pcb.inp_flags & INP_UDP_NOCKSUM):
        out_string += "nocksum "
    if (pcb.inp_flags & INP_BOUND_IF):
        out_string += "boundif "
    if (pcb.inp_flags & IN6P_IPV6_V6ONLY):
        out_string += "v6only "
    if (pcb.inp_flags & IN6P_PKTINFO):
        out_string += "pktinfo "
    if (pcb.inp_flags & IN6P_HOPLIMIT):
        out_string += "hoplimit "
    if (pcb.inp_flags & IN6P_HOPOPTS):
        out_string += "hopopts "
    if (pcb.inp_flags & IN6P_DSTOPTS):
        out_string += "dstopts "
    if (pcb.inp_flags & IN6P_RTHDR):
        out_string += "rthdr "
    if (pcb.inp_flags & IN6P_RTHDRDSTOPTS):
        out_string += "rthdrdstopts "
    if (pcb.inp_flags & IN6P_TCLASS):
        out_string += "rcv_tclass "
    if (pcb.inp_flags & IN6P_AUTOFLOWLABEL):
        out_string += "autoflowlabel "
    if (pcb.inp_flags & IN6P_BINDV6ONLY):
        out_string += "bindv6only "
    if (pcb.inp_flags & IN6P_RFC2292):
        out_string += "RFC2292 "
    if (pcb.inp_flags & IN6P_MTU):
        out_string += "rcv_pmtu "
    if (pcb.inp_flags & INP_PKTINFO):
        out_string += "pktinfo "
    if (pcb.inp_flags & INP_FLOW_SUSPENDED):
        out_string += "suspended "
    if (pcb.inp_flags & INP_NO_IFT_CELLULAR):
        out_string += "nocellular "
    if (pcb.inp_flags & INP_FLOW_CONTROLLED):
        out_string += "flowctld "
    if (pcb.inp_flags & INP_FC_FEEDBACK):
        out_string += "fcfeedback "
    if (pcb.inp_flags2 & INP2_TIMEWAIT):
        out_string += "timewait "
    if (pcb.inp_flags2 & INP2_IN_FCTREE):
        out_string += "in_fctree "
    if (pcb.inp_flags2 & INP2_WANT_APP_POLICY):
        out_string += "want_app_policy "

    out_string += "\n\t"
    so = pcb.inp_socket
    if (so != 0):
        out_string += "so=" + str(so) + " s=" + str(int(so.so_snd.sb_cc)) + " r=" + str(int(so.so_rcv.sb_cc))
        if proto == IPPROTO_TCP :
            tcpcb = cast(pcb.inp_ppcb, 'tcpcb *')
            out_string += " reass=" + str(int(tcpcb.t_reassqlen))

        out_string += " usecnt=" + str(int(so.so_usecount)) + ", "

    if (pcb.inp_state == 0 or pcb.inp_state == INPCB_STATE_INUSE):
        out_string += "inuse"
    else:
        if (pcb.inp_state == INPCB_STATE_DEAD):
            out_string += "dead"
        else:
            out_string += "unknown (" + str(int(pcb.inp_state)) + ")"

    return out_string

def CalcMbufInList(mpkt, pkt_cnt, buf_byte_cnt, mbuf_cnt, mbuf_cluster_cnt):
    while (mpkt != 0):
        mp = mpkt
        if kern.globals.mb_uses_mcache == 1:
            mpkt = mp.m_hdr.mh_nextpkt
        else:
            mpkt = mp.M_hdr_common.M_hdr.mh_nextpkt
        pkt_cnt[0] +=1
        while (mp != 0):
            if kern.globals.mb_uses_mcache == 1:
                mnext = mp.m_hdr.mh_next
                mflags = mp.m_hdr.mh_flags
                mtype = mp.m_hdr.mh_type
            else:
                mnext = mp.M_hdr_common.M_hdr.mh_next
                mflags = mp.M_hdr_common.M_hdr.mh_flags
                mtype = mp.M_hdr_common.M_hdr.mh_type
            mbuf_cnt[0] += 1
            buf_byte_cnt[int(mtype)] += 256
            buf_byte_cnt[Mbuf_Type.MT_LAST] += 256
            if (mflags & 0x01):
                mbuf_cluster_cnt[0] += 1
                if kern.globals.mb_uses_mcache == 1:
                    extsize = mp.M_dat.MH.MH_dat.MH_ext.ext_size
                else:
                    extsize = mp.M_hdr_common.M_ext.ext_size
                buf_byte_cnt[int(mtype)] += extsize
                buf_byte_cnt[Mbuf_Type.MT_LAST] += extsize
            mp = mnext

def CalcMbufInSB(so, snd_cc, snd_buf, rcv_cc, rcv_buf, snd_record_cnt, rcv_record_cnt, snd_mbuf_cnt, rcv_mbuf_cnt, snd_mbuf_cluster_cnt, rcv_mbuf_cluster_cnt):
    snd_cc[0] += so.so_snd.sb_cc
    mpkt = so.so_snd.sb_mb
    CalcMbufInList(mpkt, snd_record_cnt, snd_buf, snd_mbuf_cnt, snd_mbuf_cluster_cnt)
    rcv_cc[0] += so.so_rcv.sb_cc
    mpkt = so.so_rcv.sb_mb
    CalcMbufInList(mpkt, rcv_record_cnt, rcv_buf, rcv_mbuf_cnt, rcv_mbuf_cluster_cnt)

# Macro: show_socket_sb_mbuf_usage
@lldb_command('show_socket_sb_mbuf_usage')
def ShowSocketSbMbufUsage(cmd_args=None):
    """ Display for a socket the mbuf usage of the send and receive socket buffers
    """
    if cmd_args is None or len(cmd_args) == 0:
            print("Missing argument 0 in user function.")
            return
    so = kern.GetValueFromAddress(cmd_args[0], 'socket *')
    out_string = ""
    if (so != 0):
        snd_mbuf_cnt = [0]
        snd_mbuf_cluster_cnt = [0]
        snd_record_cnt = [0]
        snd_cc = [0]
        snd_buf = [0] * (Mbuf_Type.MT_LAST + 1)
        rcv_mbuf_cnt = [0]
        rcv_mbuf_cluster_cnt = [0]
        rcv_record_cnt = [0]
        rcv_cc = [0]
        rcv_buf = [0] * (Mbuf_Type.MT_LAST + 1)
        total_mbuf_bytes = 0
        CalcMbufInSB(so, snd_cc, snd_buf, rcv_cc, rcv_buf, snd_record_cnt, rcv_record_cnt, snd_mbuf_cnt, rcv_mbuf_cnt, snd_mbuf_cluster_cnt, rcv_mbuf_cluster_cnt)
        out_string += "total send mbuf count: " + str(int(snd_mbuf_cnt[0])) + " receive mbuf count: " + str(int(rcv_mbuf_cnt[0])) + "\n"
        out_string += "total send mbuf cluster count: " + str(int(snd_mbuf_cluster_cnt[0])) + " receive mbuf cluster count: " + str(int(rcv_mbuf_cluster_cnt[0])) + "\n"
        out_string += "total send record count: " + str(int(snd_record_cnt[0])) + " receive record count: " + str(int(rcv_record_cnt[0])) + "\n"
        out_string += "total snd_cc (total bytes in send buffers): " + str(int(snd_cc[0])) + " rcv_cc (total bytes in receive buffers): " + str(int(rcv_cc[0])) + "\n"
        out_string += "total snd_buf bytes " + str(int(snd_buf[Mbuf_Type.MT_LAST])) + " rcv_buf bytes " + str(int(rcv_buf[Mbuf_Type.MT_LAST])) + "\n"
        for x in range(Mbuf_Type.MT_LAST):
            if (snd_buf[x] != 0 or rcv_buf[x] != 0):
                out_string += "total snd_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(snd_buf[x])) + " total recv_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(rcv_buf[x])) + "\n"
                total_mbuf_bytes += snd_buf[x] + rcv_buf[x]
    print(out_string)
# EndMacro:  show_socket_sb_mbuf_usage


def GetMptcpInfo():
    mptcp = kern.globals.mtcbinfo

    mppcb = cast(mptcp.mppi_pcbs.tqh_first, 'mppcb *')
    pcbseen = 0
    reinject_cnt=[0]
    reinject_byte_cnt=[0] * (Mbuf_Type.MT_LAST + 1)
    reinject_mbuf_cnt=[0]
    reinject_mbuf_cluster_cnt=[0]

    snd_mbuf_cnt = [0]
    snd_mbuf_cluster_cnt = [0]
    snd_record_cnt = [0]
    snd_cc = [0]
    snd_buf = [0] * (Mbuf_Type.MT_LAST + 1)
    rcv_mbuf_cnt = [0]
    rcv_mbuf_cluster_cnt = [0]
    rcv_record_cnt = [0]
    rcv_cc = [0]
    rcv_buf = [0] * (Mbuf_Type.MT_LAST + 1)
    total_mbuf_bytes = 0
    while mppcb != 0:
        mpte = mppcb.mpp_pcbe
        pcbseen += 1
        CalcMbufInList(mpte.mpte_reinjectq, reinject_cnt, reinject_byte_cnt, reinject_mbuf_cnt, reinject_mbuf_cluster_cnt)

        socket = mppcb.mpp_socket
        if socket != 0:
            CalcMbufInSB(socket, snd_cc, snd_buf, rcv_cc, rcv_buf, snd_record_cnt, rcv_record_cnt, snd_mbuf_cnt, rcv_mbuf_cnt, snd_mbuf_cluster_cnt, rcv_mbuf_cluster_cnt)

        mppcb = cast(mppcb.mpp_entry.tqe_next, 'mppcb *')

    out_string = ""
    out_string += "total pcbs seen: " + str(int(pcbseen)) + "\n"
    out_string += "total reinject mbuf count: " + str(int(reinject_mbuf_cnt[0])) + "\n"
    out_string += "total reinject mbuf cluster count: " + str(int(reinject_mbuf_cluster_cnt[0])) + "\n"
    out_string += "total reinject record count: " + str(int(reinject_cnt[0])) + "\n"
    for x in range(Mbuf_Type.MT_LAST):
        if (reinject_byte_cnt[x] != 0):
            out_string += "total reinject bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(reinject_byte_cnt[x])) + "\n"
            total_mbuf_bytes += reinject_byte_cnt[x]


    out_string += "total send mbuf count: " + str(int(snd_mbuf_cnt[0])) + " receive mbuf count: " + str(int(rcv_mbuf_cnt[0])) + "\n"
    out_string += "total send mbuf cluster count: " + str(int(snd_mbuf_cluster_cnt[0])) + " receive mbuf cluster count: " + str(int(rcv_mbuf_cluster_cnt[0])) + "\n"
    out_string += "total send record count: " + str(int(snd_record_cnt[0])) + " receive record count: " + str(int(rcv_record_cnt[0])) + "\n"
    out_string += "total snd_cc (total bytes in send buffers): " + str(int(snd_cc[0])) + " rcv_cc (total bytes in receive buffers): " + str(int(rcv_cc[0])) + "\n"
    out_string += "total snd_buf bytes " + str(int(snd_buf[Mbuf_Type.MT_LAST])) + " rcv_buf bytes " + str(int(rcv_buf[Mbuf_Type.MT_LAST])) + "\n"
    for x in range(Mbuf_Type.MT_LAST):
        if (snd_buf[x] != 0 or rcv_buf[x] != 0):
            out_string += "total snd_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(snd_buf[x])) + " total recv_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(rcv_buf[x])) + "\n"
            total_mbuf_bytes += snd_buf[x] + rcv_buf[x]

    out_string += "total mbuf bytes used by MPTCP: "+ str(total_mbuf_bytes) + "\n"
    print(out_string)

def GetPcbInfo(pcbi, proto):
    tcp_reassqlen = [0]
    tcp_reassq_bytes = 0
    mbuf_reassq_cnt = [0]
    mbuf_reassq_bytes = [0] * (Mbuf_Type.MT_LAST + 1)
    mbuf_reassq_cluster = [0]
    out_string = ""
    snd_mbuf_cnt = [0]
    snd_mbuf_cluster_cnt = [0]
    snd_record_cnt = [0]
    snd_cc = [0]
    snd_buf = [0] * (Mbuf_Type.MT_LAST + 1)
    rcv_mbuf_cnt = [0]
    rcv_mbuf_cluster_cnt = [0]
    rcv_record_cnt = [0]
    rcv_cc = [0]
    rcv_buf = [0] * (Mbuf_Type.MT_LAST + 1)
    pcbseen = 0
    out_string += "lastport " + str(int(pcbi.ipi_lastport)) + " lastlow " + str(int(pcbi.ipi_lastlow)) + " lasthi " + str(int(pcbi.ipi_lasthi)) + "\n"
    out_string += "active pcb count is " + str(int(pcbi.ipi_count)) + "\n"
    hashsize = pcbi.ipi_hashmask + 1
    out_string += "hash size is " + str(int(hashsize)) + "\n"
    out_string += str(pcbi.ipi_hashbase) + " has the following inpcb(s):\n"
    if (kern.ptrsize == 8):
        out_string += "pcb                proto  source                                        port  destination                                 port\n"
    else:
        out_string += "pcb            proto  source           address  port  destination         address  port\n\n"

    if proto == IPPROTO_RAW:
        head = cast(pcbi.ipi_listhead, 'inpcbhead *')
        pcb = cast(head.lh_first, 'inpcb *')
        while pcb != 0:
            pcbseen += 1
            out_string += GetInPcb(pcb, proto) + "\n"
            so = pcb.inp_socket
            if so != 0:
                CalcMbufInSB(so, snd_cc, snd_buf, rcv_cc, rcv_buf, snd_record_cnt, rcv_record_cnt, snd_mbuf_cnt, rcv_mbuf_cnt, snd_mbuf_cluster_cnt, rcv_mbuf_cluster_cnt)
            pcb = cast(pcb.inp_list.le_next, 'inpcb *')
    else:
        i = 0
        hashbase = pcbi.ipi_hashbase
        while (i < hashsize):
            head = hashbase[i]
            pcb = cast(head.lh_first, 'inpcb *')
            while pcb != 0:
                pcbseen += 1
                out_string += GetInPcb(pcb, proto) + "\n"
                so = pcb.inp_socket
                if so != 0:
                    CalcMbufInSB(so, snd_cc, snd_buf, rcv_cc, rcv_buf, snd_record_cnt, rcv_record_cnt, snd_mbuf_cnt, rcv_mbuf_cnt, snd_mbuf_cluster_cnt, rcv_mbuf_cluster_cnt)
                if proto == IPPROTO_TCP and pcb.inp_ppcb:
                    tcpcb = cast(pcb.inp_ppcb, 'tcpcb *')
                    reass_entry = cast(tcpcb.t_segq.lh_first, 'tseg_qent *')
                    curr_reass = 0
                    while reass_entry != 0:
                        CalcMbufInList(reass_entry.tqe_m, tcp_reassqlen, mbuf_reassq_bytes, mbuf_reassq_cnt, mbuf_reassq_cluster)
                        tcp_reassq_bytes += reass_entry.tqe_len
                        curr_reass += reass_entry.tqe_len

                        reass_entry = reass_entry.tqe_q.le_next

                pcb = cast(pcb.inp_hash.le_next, 'inpcb *')
            i += 1

    out_string += "total pcbs seen: " + str(int(pcbseen)) + "\n"
    out_string += "total send mbuf count: " + str(int(snd_mbuf_cnt[0])) + " receive mbuf count: " + str(int(rcv_mbuf_cnt[0])) + "\n"
    out_string += "total send mbuf cluster count: " + str(int(snd_mbuf_cluster_cnt[0])) + " receive mbuf cluster count: " + str(int(rcv_mbuf_cluster_cnt[0])) + "\n"
    out_string += "total send record count: " + str(int(snd_record_cnt[0])) + " receive record count: " + str(int(rcv_record_cnt[0])) + "\n"
    out_string += "total snd_cc (total bytes in send buffers): " + str(int(snd_cc[0])) + " rcv_cc (total bytes in receive buffers): " + str(int(rcv_cc[0])) + "\n"
    out_string += "total snd_buf bytes " + str(int(snd_buf[Mbuf_Type.MT_LAST])) + " rcv_buf bytes " + str(int(rcv_buf[Mbuf_Type.MT_LAST])) + "\n"
    for x in range(Mbuf_Type.MT_LAST):
        if (snd_buf[x] != 0 or rcv_buf[x] != 0):
            out_string += "total snd_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(snd_buf[x])) + " total recv_buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(rcv_buf[x])) + "\n"
    out_string += "port hash base is " + hex(pcbi.ipi_porthashbase) + "\n"
    if proto == IPPROTO_TCP:
        out_string += "TCP reassembly queue length: " + str(tcp_reassqlen[0]) + " TCP-payload bytes: " + str(tcp_reassq_bytes) + "\n"

        for x in range(Mbuf_Type.MT_LAST):
            if mbuf_reassq_bytes[x] != 0:
                out_string += "total reassq bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(mbuf_reassq_bytes[x]) + "\n"

    i = 0
    hashbase = pcbi.ipi_porthashbase
    while (i < hashsize):
        head = hashbase[i]
        pcb = cast(head.lh_first, 'inpcbport *')
        while pcb != 0:
            out_string += "\t"
            out_string += GetInPcbPort(pcb)
            out_string += "\n"
            pcb = cast(pcb.phd_hash.le_next, 'inpcbport *')
        i += 1

    return out_string

def GetInPcbPort(ppcb):
    out_string = ""
    out_string += hex(ppcb) + ": lport "
    out_string += Getntohs(ppcb.phd_port)
    return out_string


def Getntohs(port):
    out_string = ""
    #p = unsigned(int(port) & 0x0000ffff)
    p = ((port & 0x0000ff00) >> 8)
    p |= ((port & 0x000000ff) << 8)
    return str(p)


# Macro: mbuf_list_usage_summary
@lldb_command('mbuf_list_usage_summary')
def ShowMbufListUsageSummary(cmd_args=None):
    """ Print mbuf list usage summary
    """
    out_string = ""
    pkt_cnt = [0]
    buf_byte_cnt = [0] * (Mbuf_Type.MT_LAST + 1)
    mbuf_cnt = [0]
    mbuf_cluster_cnt = [0]

    mpkt = kern.GetValueFromAddress(cmd_args[0], 'struct mbuf *')
    CalcMbufInList(mpkt, pkt_cnt, buf_byte_cnt, mbuf_cnt, mbuf_cluster_cnt)

    out_string += "Total packet count is " + str(int(pkt_cnt[0])) + "\n"
    for x in range(Mbuf_Type.MT_LAST):
        if (buf_byte_cnt[x] != 0):
            out_string += "Total buf bytes of type " + Mbuf_Type.reverse_mapping[x] + " : " + str(int(buf_byte_cnt[x])) + "\n"
    out_string += "Total mbuf count " + str(int(mbuf_cnt[0])) + "\n"
    out_string += "Total mbuf cluster count " + str(int(mbuf_cluster_cnt[0])) + "\n"
    print(out_string)

# Macro: show_kern_event_pcbinfo
def GetKernEventPcbInfo(kev_pcb_head):
    out_string = ""
    pcb = Cast(kev_pcb_head.lh_first, 'kern_event_pcb *')
    if (kern.ptrsize == 8):
        kev_pcb_format_string = "0x{0:<16x} {1:12d} {2:16d} {3:16d} {4:16d} {5:16d}"
        out_string += "  evp socket         vendor code      class filter      subclass filter     so_rcv.sb_cc      so_rcv.sb_mbcnt\n"
        out_string += "--------------       -----------      ------------      ---------------     ------------      ---------------\n"
    else:
        kev_pcb_format_string = "0x{0:<8x} {1:12d} {2:16d} {3:16d} {4:16d} {5:16d}"
        out_string += "evp socket       vendor code      class filter      subclass filter     so_rcv.sb_cc      so_rcv.sb_mbcnt\n"
        out_string += "----------       -----------      ------------      ---------------     ------------      ---------------\n"
    while (pcb != 0):
        out_string += kev_pcb_format_string.format(pcb.evp_socket, pcb.evp_vendor_code_filter, pcb.evp_class_filter, pcb.evp_subclass_filter, pcb.evp_socket.so_rcv.sb_cc, pcb.evp_socket.so_rcv.sb_mbcnt)
        out_string += "\n"
        pcb = pcb.evp_link.le_next
    return out_string

@lldb_command('show_kern_event_pcbinfo')
def ShowKernEventPcbInfo(cmd_args=None):
    """ Display the list of Kernel Event protocol control block information
    """
    print(GetKernEventPcbInfo(addressof(kern.globals.kern_event_head)))
# EndMacro:  show_kern_event_pcbinfo

# Macro: show_kern_control_pcbinfo
def GetKernControlPcbInfo(ctl_head):
    out_string = ""
    kctl = Cast(ctl_head.tqh_first, 'kctl *')
    if (kern.ptrsize == 8):
        kcb_format_string = "0x{0:<16x} {1:10d} {2:10d} {3:10d}\n"
    else:
        kcb_format_string = "0x{0:<8x} {1:10d} {2:10d} {3:10d}\n"
    while unsigned(kctl) != 0:
        kctl_name = "controller: " + str(kctl.name) + "\n"
        out_string += kctl_name
        kcb = Cast(kctl.kcb_head.tqh_first, 'ctl_cb *')
        if unsigned(kcb) != 0:
            if (kern.ptrsize == 8):
                out_string += "socket               usecount     snd_cc     rcv_cc\n"
                out_string += "------               --------     ------     ------\n"
            else:
                out_string += "socket       usecount     snd_cc     rcv_cc\n"
                out_string += "------       --------     ------     ------\n"
        while unsigned(kcb) != 0:
            so = Cast(kcb.so, 'socket *')
            snd_cc = so.so_snd.sb_cc
            rcv_cc = so.so_rcv.sb_cc
            out_string += kcb_format_string.format(kcb.so, kcb.usecount, snd_cc, rcv_cc)
            kcb = kcb.next.tqe_next
        out_string += "\n"
        kctl = kctl.next.tqe_next
    return out_string

@lldb_command('show_kern_control_pcbinfo')
def ShowKernControlPcbInfo(cmd_args=None):
    """ Display the list of Kernel Control protocol control block information
    """
    print(GetKernControlPcbInfo(addressof(kern.globals.ctl_head)))
# EndMacro:  show_kern_control_pcbinfo

# Macro: show_unix_domain_pcbinfo
def GetUnixDomainPCBAsString(unp, type) :
    out_string = ""
    pcb = Cast(unp, 'unpcb *')
    out_string += "unpcb: " + hex(pcb)  + " " + str(type)
    out_string += " unp_socket: " + hex(pcb.unp_socket)
    out_string += " unp_vnode: " + hex(pcb.unp_vnode)
    out_string += " unp_conn: " + hex(pcb.unp_conn)
    out_string += " unp_addr: " + GetSocketAddrAsStringUnix(pcb.unp_addr)
    out_string += " unp_gencnt: " + str(int(pcb.unp_gencnt))
    out_string += " unp_flags: " + hex(pcb.unp_flags)
    if pcb.unp_socket != 0:
        so = Cast(pcb.unp_socket, 'socket *')
        out_string += " s=" + str(int(so.so_snd.sb_cc)) + " r=" + str(int(so.so_rcv.sb_cc)) + " usecnt=" + str(int(so.so_usecount))
    return out_string

def GetUnixDomainPcbInfo(unp_head, type):
    out_string = ""
    unp = Cast(unp_head.lh_first, 'unpcb *')
    while unsigned(unp) != 0:
        out_string += GetUnixDomainPCBAsString(unp, type)
        out_string += "\n"
        unp = unp.unp_link.le_next
    return out_string

@lldb_command('show_unix_domain_pcbinfo')
def ShowUnixDomainPcbInfo(cmd_args=None):
    """ Display the list of unix domain pcb
    """
    print(GetUnixDomainPcbInfo(addressof(kern.globals.unp_dhead), "dgram"))
    print(GetUnixDomainPcbInfo(addressof(kern.globals.unp_shead), "stream"))
# EndMacro:  show_kern_control_pcbinfo

# Macro: show_tcp_pcbinfo
@lldb_command('show_tcp_pcbinfo')
def ShowTcpPcbInfo(cmd_args=None):
    """ Display the list of TCP protocol control block information
    """
    print(GetPcbInfo(addressof(kern.globals.tcbinfo), IPPROTO_TCP))
# EndMacro:  show_tcp_pcbinfo

# Macro: show_udp_pcbinfo
@lldb_command('show_udp_pcbinfo')
def ShowUdpPcbInfo(cmd_args=None):
    """ Display the list of UDP protocol control block information
    """
    print(GetPcbInfo(addressof(kern.globals.udbinfo), IPPROTO_UDP))
# EndMacro:  show_udp_pcbinfo

# Macro: show_rip_pcbinfo
@lldb_command('show_rip_pcbinfo')
def ShowRipPcbInfo(cmd_args=None):
    """ Display the list of Raw IP protocol control block information
    """
    print(GetPcbInfo(addressof(kern.globals.ripcbinfo), IPPROTO_RAW))
# EndMacro:  show_rip_pcbinfo

# Macro: show_mptcp_pcbinfo
@lldb_command('show_mptcp_pcbinfo')
def ShowMptcpPcbInfo(cmd_args=None):
    """ Display the list of MPTCP protocol control block information
    """
    GetMptcpInfo()
# EndMacro:  show_mptcp_pcbinfo

# Macro: show_domains
@lldb_command('show_domains')
def ShowDomains(cmd_args=None):
    """ Display the list of the domains
    """
    out_string = ""
    domains = kern.globals.domains
    dp = Cast(domains.tqh_first, 'domain *')
    ifma_trash_format_string = "{0:4d}: {1:x} {2:3d} {3:6d} {4:6d}"
    cnt = 0
    while (dp != 0):
        out_string += "\"" + str(dp.dom_name) + "\"" + "[" + str(int(dp.dom_refs)) + " refs] domain " + hex(dp) + "\n"
        out_string += "    family:\t" + str(int(dp.dom_family)) + "\n"
        out_string += "    flags:0x\t" + str(int(dp.dom_flags)) + "\n"
        out_string += "    rtparams:\toff=" + str(int(dp.dom_rtoffset)) + ", maxrtkey=" + str(int(dp.dom_maxrtkey)) + "\n"

        if (dp.dom_init):
            out_string += "    init:\t"
            out_string += GetSourceInformationForAddress(dp.dom_init) + "\n"
        if (dp.dom_externalize):
            out_string += "    externalize:\t"
            out_string += GetSourceInformationForAddress(dp.dom_externalize) + "\n"
        if (dp.dom_dispose):
            out_string += "    dispose:\t"
            out_string += GetSourceInformationForAddress(dp.dom_dispose) + "\n"
        if (dp.dom_rtattach):
            out_string += "    rtattach:\t"
            out_string += GetSourceInformationForAddress(dp.dom_rtattach) + "\n"
        if (dp.dom_old):
            out_string += "    old:\t"
            out_string += GetSourceInformationForAddress(dp.dom_old) + "\n"

        pr = Cast(dp.dom_protosw.tqh_first, 'protosw *')
        while pr != 0:
            pru = pr.pr_usrreqs
            out_string += "\ttype " + str(int(pr.pr_type)) + ", protocol " + str(int(pr.pr_protocol)) + ", protosw " + hex(pr) + "\n"
            out_string += "\t    flags:0x\t" + hex(pr.pr_flags) + "\n"
            if (pr.pr_input):
                out_string += "\t    input:\t"
                out_string += GetSourceInformationForAddress(pr.pr_input) + "\n"
            if (pr.pr_output):
                out_string += "\t    output:\t"
                out_string += GetSourceInformationForAddress(pr.pr_output) + "\n"
            if (pr.pr_ctlinput):
                out_string += "\t    ctlinput:\t"
                out_string += GetSourceInformationForAddress(pr.pr_ctlinput) + "\n"
            if (pr.pr_ctloutput):
                out_string += "\t    ctloutput:\t"
                out_string += GetSourceInformationForAddress(pr.pr_ctloutput) + "\n"
            if (pr.pr_init):
                out_string += "\t    init:\t"
                out_string += GetSourceInformationForAddress(pr.pr_init) + "\n"
            if (pr.pr_drain):
                out_string += "\t    drain:\t"
                out_string += GetSourceInformationForAddress(pr.pr_drain) + "\n"
            if (pr.pr_sysctl):
                out_string += "\t    sysctl:\t"
                out_string += GetSourceInformationForAddress(pr.pr_sysctl) + "\n"
            if (pr.pr_lock):
                out_string += "\t    lock:\t"
                out_string += GetSourceInformationForAddress(pr.pr_lock) + "\n"
            if (pr.pr_unlock):
                out_string += "\t    unlock:\t"
                out_string += GetSourceInformationForAddress(pr.pr_unlock) + "\n"
            if (pr.pr_getlock):
                out_string += "\t    getlock:\t"
                out_string += GetSourceInformationForAddress(pr.pr_getlock) + "\n"
            if (pr.pr_old):
                out_string += "\t    old:\t"
                out_string += GetSourceInformationForAddress(pr.pr_old) + "\n"

            out_string += "\t    pru_flags:0x\t" + hex(pru.pru_flags) + "\n"
            out_string += "\t    abort:\t"
            out_string += GetSourceInformationForAddress(pru.pru_abort) + "\n"
            out_string += "\t    accept:\t"
            out_string += GetSourceInformationForAddress(pru.pru_accept) + "\n"
            out_string += "\t    attach:\t"
            out_string += GetSourceInformationForAddress(pru.pru_attach) + "\n"
            out_string += "\t    bind:\t"
            out_string += GetSourceInformationForAddress(pru.pru_bind) + "\n"
            out_string += "\t    connect:\t"
            out_string += GetSourceInformationForAddress(pru.pru_connect) + "\n"
            out_string += "\t    connect2:\t"
            out_string += GetSourceInformationForAddress(pru.pru_connect2) + "\n"
            out_string += "\t    connectx:\t"
            out_string += GetSourceInformationForAddress(pru.pru_connectx) + "\n"
            out_string += "\t    control:\t"
            out_string += GetSourceInformationForAddress(pru.pru_control) + "\n"
            out_string += "\t    detach:\t"
            out_string += GetSourceInformationForAddress(pru.pru_detach) + "\n"
            out_string += "\t    disconnect:\t"
            out_string += GetSourceInformationForAddress(pru.pru_disconnect) + "\n"
            out_string += "\t    listen:\t"
            out_string += GetSourceInformationForAddress(pru.pru_listen) + "\n"
            out_string += "\t    peeraddr:\t"
            out_string += GetSourceInformationForAddress(pru.pru_peeraddr) + "\n"
            out_string += "\t    rcvd:\t"
            out_string += GetSourceInformationForAddress(pru.pru_rcvd) + "\n"
            out_string += "\t    rcvoob:\t"
            out_string += GetSourceInformationForAddress(pru.pru_rcvoob) + "\n"
            out_string += "\t    send:\t"
            out_string += GetSourceInformationForAddress(pru.pru_send) + "\n"
            out_string += "\t    sense:\t"
            out_string += GetSourceInformationForAddress(pru.pru_sense) + "\n"
            out_string += "\t    shutdown:\t"
            out_string += GetSourceInformationForAddress(pru.pru_shutdown) + "\n"
            out_string += "\t    sockaddr:\t"
            out_string += GetSourceInformationForAddress(pru.pru_sockaddr) + "\n"
            out_string += "\t    sopoll:\t"
            out_string += GetSourceInformationForAddress(pru.pru_sopoll) + "\n"
            out_string += "\t    soreceive:\t"
            out_string += GetSourceInformationForAddress(pru.pru_soreceive) + "\n"
            out_string += "\t    sosend:\t"
            out_string += GetSourceInformationForAddress(pru.pru_sosend) + "\n"
            pr = pr.pr_entry.tqe_next
        dp = dp.dom_entry.tqe_next

        print(out_string)
# EndMacro: show_domains

# Macro: tcp_count_rxt_segments
@lldb_command('tcp_count_rxt_segments')
def TCPCountRxtSegments(cmd_args=None):
    """ Size of the t_rxt_segments chain
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")

    tp = kern.GetValueFromAddress(cmd_args[0], 'tcpcb *')
    rxseg = cast(tp.t_rxt_segments.slh_first, 'tcp_rxt_seg *')
    cnt = 0
    while rxseg != 0:
        cnt += 1
        rxseg = rxseg.rx_link.sle_next
        if (cnt % 1000 == 0):
            print(" running count: {:d}".format(cnt))
    print(" total count: {:d}".format(cnt))
# EndMacro: tcp_count_rxt_segments

# Macro: tcp_walk_rxt_segments
@lldb_command('tcp_walk_rxt_segments')
def TCPWalkRxtSegments(cmd_args=None):
    """ Walk the t_rxt_segments chain
    """
    if not cmd_args:
        raise ArgumentError("Missing argument 0 in user function.")

    tp = kern.GetValueFromAddress(cmd_args[0], 'tcpcb *')
    rxseg = cast(tp.t_rxt_segments.slh_first, 'tcp_rxt_seg *')
    cnt = 0
    while rxseg != 0:
        cnt += 1
        rxseg = rxseg.rx_link.sle_next
        if (cnt % 1000 == 0):
            print(" running count: {:d}".format(cnt))
    print(" total count: {:d}".format(cnt))
    rxseg = cast(tp.t_rxt_segments.slh_first, 'tcp_rxt_seg *')
    cnt = 0
    while rxseg != 0:
        cnt += 1
        out_string = ""
        span = rxseg.rx_end - rxseg.rx_start
        rxseg_format = "{0:4d} 0x{1:x} rx_start 0x{2:x} rx_end 0x{3:x} rx_count {4:4d} rx_flags 0x{5:x} span {6:d}"
        out_string += rxseg_format.format(cnt, rxseg, rxseg.rx_start, rxseg.rx_end, rxseg.rx_count, rxseg.rx_flags, abs(span))
        print(out_string)
        rxseg = rxseg.rx_link.sle_next
# EndMacro: tcp_walk_rxt_segments
