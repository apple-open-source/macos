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

/* *******************************************
 * Internal Expat Callbacks
 * *******************************************/

void _mio_xstream_startElement(mio m, const char* name, const char** attribs)
{
    /* If stacknode is NULL, we are starting a new packet and must
       setup for by pre-allocating some memory */
    if (m->stacknode == NULL)
    {
	    pool p = pool_heap(5 * 1024); /* 5k, typically 1-2k each, plus copy of self and workspace */
	    m->stacknode = xmlnode_new_tag_pool(p, name);
	    xmlnode_put_expat_attribs(m->stacknode, attribs);

	    /* If the root is 0, this must be the root node.. */
	    if (m->root == 0)
	    {
            if(m->cb != NULL)
	            (*(mio_xml_cb)m->cb)(m, MIO_XML_ROOT, m->cb_arg, m->stacknode);
            else
                xmlnode_free(m->stacknode);
	        m->stacknode = NULL;
            m->root = 1; 
	    }
    }
    else 
    {
	    m->stacknode = xmlnode_insert_tag(m->stacknode, name);
	    xmlnode_put_expat_attribs(m->stacknode, attribs);
    }
    /* check for start of new stanzas */
    if (xmlnode_get_parent(m->stacknode) == NULL)
    {
        log_debug("mio", "[%s] _mio_xstream_startElement(name=%s) new stanza", ZONE, name);
        m->message = !strcmp(name, "message");
        m->bytes_read = 0;
    }
}

void _mio_xstream_endElement(mio m, const char* name)
{
    /* If the stacknode is already NULL, then this closing element
       must be the closing ROOT tag, so notify and exit */
    if (m->stacknode == NULL)
    {
        mio_close(m);
    }
    else
    {
	    xmlnode parent = xmlnode_get_parent(m->stacknode);
	    /* Fire the NODE event if this closing element has no parent */
	    if (parent == NULL)
	    {
            if(m->cb != NULL)
	            (*(mio_xml_cb)m->cb)(m, MIO_XML_NODE, m->cb_arg, m->stacknode);
            else
                xmlnode_free(m->stacknode);
	    }
	    m->stacknode = parent;
    }
}

void _mio_xstream_CDATA(mio m, const char* cdata, int len)
{
    if (m->stacknode != NULL)
	    xmlnode_insert_cdata(m->stacknode, cdata, len);
}

void _mio_xstream_cleanup(void* arg)
{
    mio m = (void*)arg;

    xmlnode_free(m->stacknode);
    XML_ParserFree(m->parser);
    m->parser = NULL;
}

void _mio_xstream_init(mio m)
{
    if (m != NULL)
    {
	    /* Initialize the parser */
	    m->parser = XML_ParserCreate(NULL);
	    XML_SetUserData(m->parser, m);
	    XML_SetElementHandler(m->parser, (void*)_mio_xstream_startElement, (void*)_mio_xstream_endElement);
	    XML_SetCharacterDataHandler(m->parser, (void*)_mio_xstream_CDATA);
	    /* Setup a cleanup routine to release the parser when everything is done */
	    pool_cleanup(m->p, _mio_xstream_cleanup, (void*)m);
    }
}

/* this function is called when a socket reads data */
void _mio_xml_parser(mio m, const void *vbuf, size_t bufsz)
{
    char *nul, *buf = (char*)vbuf;
	enum XML_Status result;

    /* init the parser if this is the first read call */
    if(m->parser == NULL)
    {
        _mio_xstream_init(m);
        /* XXX pretty big hack here, if the initial read contained a nul, assume nul-packet-terminating format stream */
        if((nul = strchr(buf,'\0')) != NULL && (nul - buf) < bufsz)
        {
            m->type = type_NUL;
            nul[-2] = ' '; /* assume it's .../>0 and make the stream open again */
        }
        /* XXX another big hack/experiment, for bypassing dumb proxies */
        if(*buf == 'P')
            m->type = type_HTTP;
    }

    /* XXX more http hack to catch the end of the headers */
    if(m->type == type_HTTP)
    {
        if((nul = strstr(buf,"\r\n\r\n")) == NULL)
            return;
        nul += 4;
        bufsz = bufsz - (nul - buf);
        buf = nul;
        mio_write(m,NULL,"HTTP/1.0 200 Ok\r\nServer: jabber/xmlstream-hack-0.1\r\nExpires: Fri, 10 Oct 1997 10:10:10 GMT\r\nPragma: no-cache\r\nCache-control: private\r\nConnection: close\r\n\r\n",-1);
        m->type = type_NORMAL;
    }

    /* XXX more nul-term hack to ditch the nul's whenever */
    if(m->type == type_NUL)
        while((nul = strchr(buf,'\0')) != NULL && (nul - buf) < bufsz)
        {
            memmove(nul,nul+1,strlen(nul+1));
            bufsz--;
        }

    result = XML_Parse(m->parser, buf, bufsz, 0);

    /* bail if there's no way to report */
    if(m->cb == NULL)
        return;

    if(result == XML_STATUS_ERROR)
    {
        log_error(ZONE, "[%s] XML Parsing Error: %s", XML_ErrorString(XML_GetErrorCode(m->parser)));
        (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb_arg);
        mio_write(m, NULL, "<stream:error>Invalid XML</stream:error>", -1);
        mio_close(m);
        return;
    }

    log_debug("mio", "[%s] XML_Parse(bufsz=%d) returned OK", ZONE, bufsz);
    if(m->max_stanza_bytes > 0)
    {
        m->bytes_read += bufsz;
        if(m->bytes_read > m->max_stanza_bytes)
        {
            log_error(ZONE, "Stanza exceeds maximum size; aborting connection on socket %d", m->fd);
            (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb_arg);
            mio_write(m, NULL, "<stream:error>Stanza too large</stream:error>", -1);
            mio_close(m);
        }
        else if(m->message && (m->max_message_bytes > 0) && (m->bytes_read > m->max_message_bytes))
        {
            log_error(ZONE, "Message exceeds maximum size; aborting connection on socket %d", m->fd);
            (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb_arg);
            mio_write(m, NULL, "<stream:error>Message too large</stream:error>", -1);
            mio_close(m);
        }
    }
}
