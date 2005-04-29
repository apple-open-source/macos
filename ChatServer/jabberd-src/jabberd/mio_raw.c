/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "JOSL").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the JOSL. You
 * may obtain a copy of the JOSL at http://www.jabber.org/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the JOSL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the JOSL
 * for the specific language governing rights and limitations under the
 * JOSL.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2002 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the JOSL,
 * indicate your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.  If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the JOSL or the GPL. 
 * 
 * 
 * --------------------------------------------------------------------------*/

#include <jabberd.h>

void _mio_raw_parser(mio m, const void *buf, size_t bufsz)
{
    (*(mio_raw_cb)m->cb)(m, MIO_BUFFER, m->cb_arg, (char*)buf, bufsz);
}

ssize_t _mio_raw_read(mio m, void *buf, size_t count)
{
    return MIO_READ_FUNC(m->fd, buf, count);
}

ssize_t _mio_raw_write(mio m, void *buf, size_t count)
{
    return MIO_WRITE_FUNC(m->fd, buf, count);
}

int _mio_raw_accept(mio m, struct sockaddr* serv_addr, socklen_t* addrlen)
{
    return MIO_ACCEPT_FUNC(m->fd, serv_addr, addrlen);
}

int _mio_raw_connect(mio m, struct sockaddr* serv_addr, socklen_t  addrlen)
{
    sigset_t set;
    int sig;
    pth_event_t wevt;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    wevt = pth_event(PTH_EVENT_SIGS, &set, &sig);
    pth_fdmode(m->fd, PTH_FDMODE_BLOCK);
    return pth_connect_ev(m->fd, serv_addr, addrlen, wevt);
}
