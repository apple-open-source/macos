// ========================================================================
// Copyright (c) 1999-2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Version.java,v 1.15.2.8 2003/06/04 04:47:42 starksm Exp $
// ========================================================================

package org.mortbay.http;


/* ------------------------------------------------------------ */
/** Jetty version.
 *
 * This class sets the version data returned in the Server and
 * Servlet-Container headers.   If the
 * java.org.mortbay.http.Version.paranoid System property is set to
 * true, then this information is suppressed.
 *
 * @version $Revision: 1.15.2.8 $
 * @author Greg Wilkins (gregw)
 */
public class Version
{
    public static boolean __paranoid = 
        Boolean.getBoolean("org.mortbay.http.Version.paranoid");
    
    public static String __Version="Jetty/4.2";
    public static String __VersionImpl=__Version+".x";
    public static String __VersionDetail="Unknown";
    public static String __Container=
        System.getProperty("org.mortbay.http.Version.container");
    
    static
    {
        updateVersion();
    }
    
    public static String __notice = "This application is using software from the "+
        __Version+
        " HTTP server and servlet container.\nJetty is Copyright (c) Mort Bay Consulting Pty. Ltd. (Australia) and others.\nJetty is distributed under an open source license.\nThe license and standard release of Jetty are available from http://jetty.mortbay.org\n";


    public static void main(String[] arg)
    {
        System.out.println(__notice);
        System.out.println("org.mortbay.http.Version="+__Version);
        System.out.println("org.mortbay.http.VersionImpl="+__VersionImpl);
        System.out.println("org.mortbay.http.VersionDetail="+__VersionDetail);
    }

    public static void updateVersion()
    {
        Package p = Version.class.getPackage();
        if (p!=null && p.getImplementationVersion()!=null)
            __VersionImpl="Jetty/"+p.getImplementationVersion();
        
        if (!__paranoid)
        {
            __VersionDetail=__VersionImpl+
                " ("+System.getProperty("os.name")+
                "/"+System.getProperty("os.version")+
                " "+System.getProperty("os.arch")+
                " java/"+System.getProperty("java.version")+
                (__Container!=null?(" "+__Container+")"):")");
        }
    }
}

