/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.net.ssl;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;

import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.net.ServerSocketFactory;
import javax.net.ssl.SSLSocket;

import org.jboss.security.SecurityDomain;
import org.jboss.security.ssl.DomainServerSocketFactory;

public class JBossSocketFactory
    extends org.apache.tomcat.util.net.ServerSocketFactory {

    private DomainServerSocketFactory socketFactory;

    public void setAttribute(String name, Object value) {
        if (name.equals("algorithm")) {
            try {
                setSecurityDomainName((String) value);
            } catch (Exception e) {
                throw new IllegalArgumentException(e.getMessage());
            }
        }
    }

    public void setSecurityDomainName(String jndiName)
        throws NamingException, IOException {
        InitialContext iniCtx = new InitialContext();
        SecurityDomain securityDomain = 
            (SecurityDomain) iniCtx.lookup(jndiName);
        socketFactory = new DomainServerSocketFactory(securityDomain);
    }

    public ServerSocket createSocket(int port)
        throws IOException {
        return createSocket(port, 50, null);
    }

    public ServerSocket createSocket(int port, int backlog)
        throws IOException {
        return createSocket(port, backlog, null);
    }

    public ServerSocket createSocket(int port, int backlog, 
                                     InetAddress ifAddress)
        throws IOException {
        return socketFactory.createServerSocket(port, backlog, ifAddress);
    }

    public Socket acceptSocket(ServerSocket socket)
        throws IOException {
        return socket.accept();
    }

    public void handshake(Socket sock)
        throws IOException {
        ((SSLSocket)sock).startHandshake();
    }

}
