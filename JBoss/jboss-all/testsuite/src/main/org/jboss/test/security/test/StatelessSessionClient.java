/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.io.IOException;
import java.rmi.RemoteException;
import java.util.Properties;
import javax.ejb.CreateException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.security.auth.callback.*;
import javax.security.auth.login.*;

import org.jboss.test.security.interfaces.StatelessSession;
import org.jboss.test.security.interfaces.StatelessSessionHome;
import org.jboss.test.util.AppCallbackHandler;

/** Run with -Djava.security.auth.login.config=url_to_jaas_login_conf

@author Scott.Stark@jboss.org
@version $Revision: 1.4.4.1 $
*/
public class StatelessSessionClient
{
   static org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(StatelessSessionClient.class);
   
    static void runMT()
    {
        Thread t0 = new Thread()
        {
            public void run()
            {
                try
                {
                    runAs("scott", "echoman".toCharArray());
                }
                catch(Exception e)
                {
                }
            }
        };
        Thread t1 = new Thread()
        {
            public void run()
            {
                try
                {
                    runAs("stark", "javaman".toCharArray());
                }
                catch(Exception e)
                {
                }
            }
        };
        t0.start();
        t1.start();
    }

    static void runAs(String username, char[] password) throws Exception
    {
        LoginContext lc = null;
        String confName = System.getProperty("conf.name", "other");
        boolean loggedIn = false;
        try
        {
            AppCallbackHandler handler = new AppCallbackHandler(username, password);
            log.debug("Creating LoginContext("+confName+")");
            lc = new LoginContext(confName, handler);
            lc.login();
            log.debug("Created LoginContext, subject="+lc.getSubject());
            loggedIn = true;

            /* The properties are for passing credentials to j2ee-ri which
               are not used with jboss */
            Properties props = new Properties();
            props.setProperty(Context.SECURITY_PRINCIPAL, username);
            props.setProperty(Context.SECURITY_CREDENTIALS, new String(password));
            props.setProperty(Context.SECURITY_PROTOCOL, "simple");
            InitialContext jndiContext = new InitialContext(props);
            Object obj = jndiContext.lookup("StatelessSession2");
            Class[] ifaces = obj.getClass().getInterfaces();
            StatelessSessionHome home = (StatelessSessionHome) obj;
            log.debug("Found StatelessSessionHome");
            StatelessSession bean = home.create();
            log.debug("Created StatelessSession");
            log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
        }
        finally
        {
            if( lc != null && loggedIn )
                lc.logout();
        }
    }

    public static void main(String args[]) throws Exception
    {
        if( args.length == 1 && args[0].equals("-mt") )
        {
            log.debug("Running multi-threaded with simultaneous logins");
            runMT();
        }
        else
        {
            log.debug("Running single-threaded with sequential logins");
            runAs("scott", "echoman".toCharArray());
            runAs("stark", "javaman".toCharArray());
        }
    }
}
