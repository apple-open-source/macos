// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Main.java,v 1.14.2.5 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import java.util.Iterator;
import java.util.Set;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.loading.MLet;
import org.mortbay.util.Code;
import org.mortbay.util.Log;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.5 $
 * @author Greg Wilkins (gregw)
 */
public class Main
{
    static MLet mlet;
        
    /* ------------------------------------------------------------ */
    /** 
     * @param arg 
     */
    static void startMLet(String[] arg)
    {
        try
        {
            // Create a MBeanServer
            final MBeanServer server =
                MBeanServerFactory.createMBeanServer(ModelMBeanImpl.getDefaultDomain());
            Code.debug("MBeanServer=",server);
            
            // Create and register the MLet
            mlet = new MLet();
            server.registerMBean(mlet,
                                 new ObjectName(server.getDefaultDomain(),
                                                "service", "MLet"));
            Code.debug("MLet=",mlet);
            
            // Set MLet as classloader for this app
            Thread.currentThread().setContextClassLoader(mlet);

            
            // load config files
            for (int i=0;i<arg.length;i++)
            {
                Log.event("Load "+arg[i]);
                Set beans=mlet.getMBeansFromURL(arg[i]);
                Iterator iter=beans.iterator();
                while(iter.hasNext())
                {
                    Object bean=iter.next();
                    if (bean instanceof Throwable)
                    {
                        iter.remove();
                        Code.warning((Throwable)bean);
                    }
                    else if (bean instanceof ObjectInstance)
                    {
                        ObjectInstance oi = (ObjectInstance)bean;

                        if ("com.sun.jdmk.comm.HtmlAdaptorServer".equals(oi.getClassName()))
                        {
                            Log.event("Starting com.sun.jdmk.comm.HtmlAdaptorServer");
                            try{server.invoke(oi.getObjectName(),"start",null,null);}
                            catch(Exception e){Code.warning(e);}
                        }
                    }
                }
                
                Code.debug("Loaded "+beans.size(),"MBeans: ",beans);
            }
        }
        catch(Exception e)
        {
            Code.warning(e);
        }
    }

    /* ------------------------------------------------------------ */
    public static void main(String[] arg)
        throws Exception
    {
        if (arg.length==0)
        {
            System.err.println("Usage - java org.mortbay.util.jmx.Main <mletURL>...");
            System.exit(1);
        }
        startMLet(arg);
        synchronized(mlet)
        {
            mlet.wait();
        }
    }
}
