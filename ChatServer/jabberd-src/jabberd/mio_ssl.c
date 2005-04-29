/*  Portions (c) Copyright 2005 Apple Computer, Inc.
*/

/* Seriously cheap hack to work around type conflict */
#define uint32 j_uint32
#include "jabberd.h"
#undef uint32

#ifdef HAVE_SSL
#include <err.h>

HASHTABLE ssl__ctxs;
extern int mio__errno;
extern int mio__ssl_reread;

void syslog_err(char* in_keypath, char *in_host)
{

    unsigned long e = 0;
    char *buf = NULL;
    char logbuff[1024] = "";
    logbuff[sizeof(logbuff) -1 ] = 0;    

    e = ERR_get_error();
    buf = ERR_error_string(e, NULL);
    snprintf(logbuff, sizeof(logbuff) -1, "%s: [error] (-configure): SSL Connections are disabled. [%s] while using SSL certificate file '%s' for host '%s'. ",jutil_timestamp(), buf, in_keypath, in_host );
    syslog(LOG_ERR | LOG_DAEMON ,"%s", logbuff); 
    log_debug(ZONE, "Connections disabled: error [%s] using SSL certificate file '%s' for host '%s' ",buf, in_keypath, in_host );

}

#ifndef NO_RSA
/* This function will generate a temporary key for us */
RSA *_ssl_tmp_rsa_cb(SSL *ssl, int export, int keylength)
{
    RSA *rsa_tmp = NULL;

    rsa_tmp = RSA_generate_key(keylength, RSA_F4, NULL, NULL);
    if(!rsa_tmp)
	{
        log_debug(ZONE, "Error generating temp RSA key");
        return NULL;
    }

    return rsa_tmp;
}
#endif /* NO_RSA */


#ifdef __APPLE__
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>

/* This function pulls the password from the system keychain */
int apple_password_callback(char *inBuf, int inSize, int in_rwflag, void *inUserData)
{
	OSStatus status = noErr;
	void *pwdBuf = NULL;  /* will be allocated and filled in by SecKeychainFindGenericPassword */
	UInt32 pwdLen = 0;
	char *service = "certificateManager"; /* defined by Apple */
	const char *label = (const char *)inUserData;
	size_t len = strlen(label);

	if(inBuf == NULL || inUserData == NULL || len >= FILENAME_MAX || len <= 0)
	{
		log_error(ZONE, "Invalid arguments in callback");
		return 0;
	}

	/* Set the domain to System (daemon) */
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if(status != noErr)
	{
		log_error(ZONE, "SecKeychainSetPreferenceDomain returned status: %d", status);
		return 0;
	}

	// Passwords created by cert management have the keychain access dialog suppressed.
	status = SecKeychainFindGenericPassword(NULL, strlen(service), service, len, label, &pwdLen, &pwdBuf, NULL);
	if(status == noErr && pwdBuf != NULL)
	{
		if(pwdLen > inSize)
		{
			log_error(ZONE, "Invalid buffer size callback (size:%d, len:%d)", inSize, pwdLen);
			pwdLen = 0;
		}
		if(pwdLen > 0)
			memcpy(inBuf, pwdBuf, pwdLen);
		inBuf[pwdLen] = 0;
		SecKeychainItemFreeContent(NULL, pwdBuf);
		return pwdLen;
	}
	if(status == errSecNotAvailable)
		log_error(ZONE, "SecKeychainFindGenericPassword: No keychain is available");
	else if(status == errSecItemNotFound)
		log_error(ZONE, "SecKeychainFindGenericPassword: Requested key not in system keychain");
	else if(status != noErr)
		log_error(ZONE, "SecKeychainFindGenericPassword returned status %d", status);

	return 0 ;
}
#endif /* __APPLE__ */


/***************************************************************************
 * This can return whatever we need, it is just designed to read a xmlnode
 * and hash the SSL contexts it creates from the keys in the node
 *
 * Sample node:
 * <ssl>
 *   <key ip='192.168.1.100'>/path/to/the/key/file.pem</key>
 *   <key ip='192.168.1.1'>/path/to/the/key/file.pem</key>
 * </ssl>   
 **************************************************************************/
void mio_ssl_init(xmlnode x)
{
/* PSEUDO CODE

  for $key in children(xmlnode x)
  {
      - SSL init
      - Load key into SSL ctx
      - Hash ctx based on hostname
  }

  register a cleanup function to free our contexts
*/

    SSL_CTX *ctx = NULL;
    xmlnode cur;
    char *host;
    char *keypath;
    log_debug(ZONE, "MIO SSL init");

    /* Make sure we have a valid xmlnode to play with */
    if(x == NULL && xmlnode_has_children(x))
    {
        log_debug(ZONE, "SSL Init called with invalid xmlnode");
        return;
    }

    log_debug(ZONE, "Handling configuration using: %s", xmlnode2str(x));
    /* Generic SSL Inits */
    OpenSSL_add_all_algorithms();    
    SSL_load_error_strings();

    /* Setup our hashtable */
    ssl__ctxs = ghash_create(19,(KEYHASHFUNC)str_hash_code,
                             (KEYCOMPAREFUNC)j_strcmp);

    /* Walk our node and add the created contexts */
    for(cur = xmlnode_get_tag(x, "key"); cur != NULL; 
                    cur = xmlnode_get_nextsibling(cur))
    {
        host = xmlnode_get_attrib(cur, "ip");
        keypath = xmlnode_get_data(cur);

        if(!host || !keypath)
            continue;

        log_debug(ZONE, "Handling: %s", xmlnode2str(cur));

        ctx=SSL_CTX_new(SSLv23_server_method());
        if(ctx == NULL)
        {
            syslog_err(keypath, host);
            return;
        }

#ifndef NO_RSA
        log_debug(ZONE, "Setting temporary RSA callback");
        SSL_CTX_set_tmp_rsa_callback(ctx, _ssl_tmp_rsa_cb);
#endif /* NO_RSA */

        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_BOTH);

        /* XXX I would like to make this a configurable option */
        /* 
         SSL_CTX_set_timeout(ctx, session_timeout);
         */

        /* Setup the keys and certs */
        log_debug(ZONE, "Loading SSL certificate %s for %s", keypath, host);
        if(!SSL_CTX_use_certificate_file(ctx, keypath, SSL_FILETYPE_PEM)) 
        {
            syslog_err(keypath, host);
            SSL_CTX_free(ctx);
            continue;
        }
#ifdef __APPLE__
        log_debug(ZONE, "Adding Apple-custom SSL password callback");
		{
			char *label = NULL;
			char *tmp = strrchr(keypath, '/');
#warning "This is a leak because there is no way to free the userdata!"
			if(tmp != NULL)
				label = strdup(++tmp);
			else
				label = strdup(keypath);
			tmp = strrchr(label, '.');
			if(tmp != NULL)
				*tmp = '\0';
			if(strlen(label))
			{
				SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)label);
				SSL_CTX_set_default_passwd_cb(ctx, &apple_password_callback);
			}
			else
				log_error(ZONE, "Could not set custom callback for %s", keypath);
			log_debug(ZONE, "Apple-custom SSL password callback enabled for %s", label);
		}
#endif /* __APPLE__ */

#warning "Since we have to use the keychain, consider using keychain for private key."
        if(!SSL_CTX_use_PrivateKey_file(ctx, keypath, SSL_FILETYPE_PEM)) 
        {
            syslog_err(keypath, host);
            SSL_CTX_free(ctx);
            continue;
        }
        ghash_put(ssl__ctxs, host, ctx);
        log_debug(ZONE, "Added context %x for %s", ctx, host);
    }
        
}

void _mio_ssl_cleanup(void *arg)
{
    SSL *ssl = (SSL *)arg;

    log_debug(ZONE, "SSL Cleanup for %x", ssl);
    SSL_free(ssl);
}

ssize_t _mio_ssl_read(mio m, void *buf, size_t count)
{
    SSL *ssl;
    ssize_t ret;
    int sret;

    ssl = m->ssl;
    
    if(count <= 0)
        return 0;

    log_debug(ZONE, "Asked to read %d bytes from %d", count, m->fd);
    mio__ssl_reread = 0;
    if(SSL_get_state(ssl) != SSL_ST_OK)
    {
        sret = SSL_accept(ssl);
        if(sret <= 0)
        {
            unsigned long e;
            static char *buf;
            
            if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
               SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE)
            {
                log_debug(ZONE, "Read blocked, returning");

                mio__errno = EAGAIN;
                return -1;
            }
            e = ERR_get_error();
            buf = ERR_error_string(e, NULL);
            log_debug(ZONE, "Error from SSL: %s", buf);
            log_debug(ZONE, "SSL Error in SSL_accept call");
            close(m->fd);
            return -1;
        }       
    }
    ret = SSL_read(ssl, (char *)buf, count);
    if (ret == count)
    {
        mio__ssl_reread = 1;
        log_debug(ZONE, "SSL Asked to reread from %d", m->fd);
    }
    return ret;
}

ssize_t _mio_ssl_write(mio m, const void *buf, size_t count)
{
    SSL *ssl;
    ssl = m->ssl;
    
    if(SSL_get_state(ssl) != SSL_ST_OK)
    {
        int sret;

        sret = SSL_accept(ssl);
        if(sret <= 0){
            unsigned long e;
            static char *buf;
            
            if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
               SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE)
            {
                log_debug(ZONE, "Write blocked, returning");
                
                mio__errno = EAGAIN;
                return -1;
            }
            e = ERR_get_error();
            buf = ERR_error_string(e, NULL);
            log_debug(ZONE, "Error from SSL: %s", buf);
            log_debug(ZONE, "SSL Error in SSL_accept call");
            close(m->fd);
            return -1;
        }       
    }
    return SSL_write(ssl, buf, count);    
}

int _mio_ssl_accept(mio m, struct sockaddr *serv_addr, socklen_t *addrlen)
{
    SSL *ssl=NULL;
    SSL_CTX *ctx = NULL;
    int fd;
    int sret;
    int flags;

    fd = accept(m->fd, serv_addr, addrlen);

    /* set the socket to non-blocking as this is not
       inherited */
    flags =  fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    if(m->ip == NULL)
    {
        log_warn(ZONE, "SSL accept but no IP given in configuration");
        return -1;
    }

    ctx = ghash_get(ssl__ctxs, m->ip);
    if(ctx == NULL)
    {
        log_debug(ZONE, "No SSL key configured for IP %s", m->ip);
        return -1;
    }
    ssl = SSL_new(ctx);
    log_debug(ZONE, "SSL accepting socket from %s with new session %x",
                    m->ip, ssl);
    SSL_set_fd(ssl, fd);
    SSL_set_accept_state(ssl);
    sret = SSL_accept(ssl);
    if(sret <= 0)
    {
        unsigned long e;
        static char *buf;
        
        if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
           (SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE))
        {
            m->ssl = ssl;
            log_debug(ZONE, "Accept blocked, returning");
            return fd;
        }
        e = ERR_get_error();
        buf = ERR_error_string(e, NULL);
        log_debug(ZONE, "Error from SSL: %s", buf);
        log_debug(ZONE, "SSL Error in SSL_accept call");
        SSL_free(ssl);
        close(fd);
        return -1;
    }

    m->k.val = 100;
    m->ssl = ssl;

    log_debug(ZONE, "Accepted new SSL socket %d for %s", fd, m->ip);

    return fd;
}

int _mio_ssl_connect(mio m, struct sockaddr *serv_addr, socklen_t addrlen)
{

    /* PSEUDO
     I need to actually look this one up, but I assume it's similar to the
       SSL accept stuff.
    */
    SSL *ssl=NULL;
    SSL_CTX *ctx = NULL;
    int fd;

    log_debug(ZONE, "Connecting new SSL socket for %s", m->ip);
    ctx = ghash_get(ssl__ctxs, m->ip);
    
    fd = connect(m->fd, serv_addr, addrlen);
    SSL_set_fd(ssl, fd);
    if(SSL_connect(ssl) <= 0){
        log_debug(ZONE, "SSL Error in SSL_connect call");
        SSL_free(ssl);
        close(fd);
        return -1;
    }

    pool_cleanup(m->p, _mio_ssl_cleanup, (void *)ssl);

    m->ssl = ssl;

    return fd;
}

#endif /* HAVE_SSL */
