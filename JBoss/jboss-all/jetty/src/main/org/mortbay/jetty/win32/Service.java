// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Service.java,v 1.15.2.3 2003/06/04 04:47:55 starksm Exp $
//
// Derived from SCMEventManager.java by Bill Giel/KC Multimedia and Design Group, Inc.,
// ========================================================================

package org.mortbay.jetty.win32;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.Vector;
import org.mortbay.http.HttpServer;
import org.mortbay.jetty.Server;
import org.mortbay.util.Code;
import org.mortbay.util.Log;
import org.mortbay.util.OutputStreamLogSink;

/* ------------------------------------------------------------ */
/** Run Jetty as a Win32 service.
 *
 * System.out and System.err output can be controlled with java
 * properties:  SERVICE_OUT and SERVICE_ERR.
 * The log file can be controlled with the property SERVICE_LOG_FILE
 * <h4>Example</h4>
 * <pre>
 * jettysvc -c -DSERVICE_OUT="./logs/jettysvc.out" \\
 *             -DSERVICE_ERR="./logs/jettysvc.err" \\
 *             Jetty.xml wrkdir=$JETTY_HOME
 * </pre>
 *
 * @version $Revision: 1.15.2.3 $
 * @author Greg Wilkins (gregw)
 */
public class Service 
{
    /* ------------------------------------------------------------ */
    static String serviceLogFile=
    System.getProperty("SERVICE_LOG_FILE","logs"+File.separator+"yyyy_mm_dd.service.log");
    
    /* ------------------------------------------------------------ */
    public static final int SERVICE_CONTROL_STOP = 1;
    public static final int SERVICE_CONTROL_PAUSE = 2;
    public static final int SERVICE_CONTROL_CONTINUE = 3;
    public static final int SERVICE_CONTROL_INTERROGATE = 4;
    public static final int SERVICE_CONTROL_SHUTDOWN = 5;
    public static final int SERVICE_CONTROL_PARAMCHANGE = 6;

    /* ------------------------------------------------------------ */
    private static Vector _servers;
    private static Vector _configs;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    private Service()
    {}
    
    /* ------------------------------------------------------------ */
    public static void dispatchSCMEvent(int eventID)
    {
        switch(eventID)
        {
          case SERVICE_CONTROL_STOP:
          case SERVICE_CONTROL_PAUSE:
              stopAll();
              break;
              
          case SERVICE_CONTROL_CONTINUE :
              startAll();
              break;
              
          case SERVICE_CONTROL_SHUTDOWN:
              destroyAll();
              break;
              
          case SERVICE_CONTROL_PARAMCHANGE:
              stopAll();
              destroyAll();
              createAll();
              startAll();
              break;

          default:
              break;
        }
    }
    
    /* ------------------------------------------------------------ */
    private static void createAll()
    {
        if (_configs!=null)
        {
            synchronized(_configs)
            {
                _servers=new Vector();
                for(int i=0;i<_configs.size();i++)
                {
                    try
                    {
                        Server server = new Server((String)_configs.get(i));
                        _servers.add(server);
                    }
                    catch(Exception e)
                    {
                        Code.warning(_configs.get(i)+" configuration problem: ",e);
                    }
                }
            }
        }
    }
    
    
    /* ------------------------------------------------------------ */
    private static void startAll()
    {
        try
        {
            if (_configs!=null)
            {
                synchronized(_configs)
                {
                    for(int i=0;i<_servers.size();i++)
                    {
                        HttpServer server = (HttpServer)_servers.get(i);
                        if (!server.isStarted())
                            server.start();
                    }
                }
            }
        }
        catch(Exception e)
        {
            Code.warning(e);
        }
    }
    
    /* ------------------------------------------------------------ */
    private static void stopAll()
    { 
        if (_configs!=null)
        {
            synchronized(_configs)
            {
                for(int i=0;i<_servers.size();i++)
                {
                    HttpServer server = (HttpServer)_servers.get(i);
                    try{server.stop();}catch(InterruptedException e){}
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    private static void destroyAll()
    {
        stopAll();
        if (_servers!=null)
            _servers.clear();
        _servers=null;
    }

    /* ------------------------------------------------------------ */
    public static void stopAndDestroy(String[] arg)
    {
        if (arg==null || arg.length==0)
        {
            stopAll();
            destroyAll();
        }
        else
        {
            Code.notImplemented();
        }
    }
    
    /* ------------------------------------------------------------ */
    public static void main(String[] arg)
    {
        String opt;	
        opt = System.getProperty("SERVICE_OUT");
        if (opt != null)
        {
            try
            {
                PrintStream stdout = new PrintStream(new FileOutputStream(opt));
                System.setOut(stdout);
            }
            catch(Exception e){Code.warning(e);}
        }
		
        opt = System.getProperty("SERVICE_ERR");
        if (opt != null)
        {
            try
            {
                PrintStream stderr = new PrintStream(new FileOutputStream(opt));
                System.setErr(stderr);
            }
            catch(Exception e){Code.warning(e);}
        }
        
        try
        {
            if (Code.debug())
                Log.instance().add(new OutputStreamLogSink());
                             
	    OutputStreamLogSink sink= new OutputStreamLogSink(serviceLogFile);
            sink.start();
            Log.instance().add(sink);
        }
        catch(Exception e)
        {
            Code.warning(e);
        }
        
        if (arg.length==0)
            arg=new String[] {"etc/jetty.xml"};
        
        try
        {
            _configs=new Vector();
            for (int i=0;i<arg.length;i++)
                _configs.add(arg[i]);
            createAll();
            startAll();
        }
        catch(Exception e)
        {
            Code.warning(e);
        }
    }
}

