/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.net.ssl;

import java.net.Socket;
import javax.net.ssl.SSLSocket;

import org.apache.tomcat.util.net.ServerSocketFactory;
import org.apache.tomcat.util.net.jsse.JSSEImplementation;

public class JBossImplementation 
    extends JSSEImplementation {

    public JBossImplementation()
        throws ClassNotFoundException {
        super();
    }

    public String getImplementationName(){
        return "JBoss";
    }
      
    public ServerSocketFactory getServerSocketFactory() {
        return new JBossSocketFactory();
    }

}
