/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina;

import java.io.File;
import java.io.FileInputStream;
import java.net.URL;
import java.util.Iterator;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.WebMetaData;
import org.jboss.util.file.JarUtils;
import org.jboss.web.AbstractWebContainer;
import org.jboss.web.AbstractWebContainer.WebDescriptorParser;
import org.jboss.web.WebApplication;

import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.Attribute;

/** 
 * An implementation of the AbstractWebContainer for the Jakarta Tomcat5
 * servlet container. It has no code dependency on tomcat - only the new JMX
 * model is used.
 * 
 * Tomcat5 is organized as a set of mbeans - just like jboss.
 * 
 * @see AbstractWebContainer
 * 
 * @author Scott.Stark@jboss.org
 * @authro Costin Manolache
 * @version $Revision: 1.1.2.6 $
 */
public class Tomcat5 extends AbstractWebContainer
   implements Tomcat5MBean {


    // Constants -----------------------------------------------------
    public static final String NAME = "Tomcat5";

    // XXX We could make this configurable - so it can support other containers
    // that provide JMX-based deployment.
    private String contextClassName="org.apache.catalina.core.StandardContext";

    /** Domain for tomcat5 mbeans */
    private String catalinaDomain="Catalina";


    public Tomcat5()
    {
    }

    public String getName()
    {
        return NAME;
    }

    public String getDomain()
    {
        return this.catalinaDomain;
    }

    /** 
     * The most important atteribute - defines the managed domain.
     * A catalina instance (engine) corresponds to a JMX domain, that's
     * how we know where to deploy webapps.
     *
     * @param catalinaHome
     */
    public void setDomain(String catalinaHome) {
        this.catalinaDomain = catalinaHome;
    }

    public void setContextMBeanCode(String className) {
    }

    public String getContextMBeanCode() {
        return null;
    }


    public void startService() 
        throws Exception {

        // Invoke the super method to register as a deployer
        super.startService();

    }


    public void stopService() 
        throws Exception {
        super.stopService();
        // nothing special here - tomcat is started by an mbean service,
        // it has its own life
    }


    /** 
     * Perform the tomcat specific deployment steps.
     */
    protected void performDeploy(WebApplication appInfo, String warUrl,
                                 WebDescriptorParser webAppParser) 
        throws Exception {

        WebMetaData metaData = appInfo.getMetaData();

        String ctxPath = metaData.getContextRoot();
        if (ctxPath.equals("/") || ctxPath.equals("/ROOT")) {
            log.info("deploy root context="+ctxPath);
            ctxPath = "";
            metaData.setContextRoot(ctxPath);
        }
        log.info("deploy, ctxPath="+ctxPath+", warUrl="+warUrl);

        URL url = new URL(warUrl);

        // Servlet engines needs a war in a dir so extract the nested war
        if ( url.getProtocol().equals("njar") ) {
            url = org.jboss.net.protocol.njar.Handler.njarToFile(url);
            log.debug("Extracted war from njar, warUrl="+url);
            File warFile = new File(url.getFile());
            String warFileName = warFile.getName();
            warFileName = warFileName.substring(0, warFileName.length()-3);
            warFileName += "war";
            File warDir = new File(warFile.getParent(), warFileName);
            FileInputStream warStream = new FileInputStream(warFile);
            JarUtils.unjar(warStream, warDir);
            warStream.close();
            log.debug("Unpacked war into dir: "+warDir);
            url = warDir.toURL();
        }

        ClassLoader loader = Thread.currentThread().getContextClassLoader();

        appInfo.setName(url.getPath());
        appInfo.setClassLoader(loader);
        appInfo.setURL(url);

        String hostName = metaData.getVirtualHost();

        if (ctxPath.equals(""))
            ctxPath="/";
        if (ctxPath.equals("/ROOT"))
            ctxPath="/";

        // XXX How do I find the j2eeApp name ???
        // XXX How can I find the jboss "id" if any ??
        String objectNameS=catalinaDomain + ":j2eeType=WebModule,name=//" +
            ((hostName==null)?"localhost":hostName) + ctxPath +
            ",j2eeApp=none,j2eeServer=jboss";

        ObjectName objectName = new ObjectName( objectNameS );

        if (server.isRegistered(objectName)) {
            // workaround
            server.invoke(objectName, "destroy", new Object[] {},
                          new String [] {});
            log.info("Already exists, unregistering " + objectName);
            server.unregisterMBean(objectName);
        }

        server.createMBean("org.apache.commons.modeler.BaseModelMBean",
                           objectName, new Object[] {contextClassName},
                           new String[] {"java.lang.String"});

        server.setAttribute(objectName, new Attribute
                            ("docBase", url.getFile()));

        server.setAttribute(objectName, new Attribute
                            ("defaultWebXml", "web.xml"));

        server.setAttribute(objectName, new Attribute
                            ("parentClassLoader", loader));

        String[] jspCP = getCompileClasspath(loader);
        StringBuffer classpath = new StringBuffer();
        for (int u = 0; u < jspCP.length; u ++) {
            String repository = jspCP[u];
            if (repository.startsWith("file://"))
                repository = repository.substring(7);
            else if (repository.startsWith("file:"))
                repository = repository.substring(5);
            else
                continue;
            if (repository == null)
                continue;
            if (u > 0)
                classpath.append(File.pathSeparator);
            classpath.append(repository);
        }

        server.setAttribute(objectName, new Attribute
                            ("compilerClasspath", classpath.toString()));

        // We need to establish the JNDI ENC prior to the start 
        // of the web container so that init on startup servlets are able 
        // to interact with their ENC. We hook into the context lifecycle 
        // events to be notified of the start of the
        // context as this occurs before the servlets are started.
        webAppParser.parseWebAppDescriptors(loader, appInfo.getMetaData());

        server.invoke( objectName, "init", new Object[]{}, new String[] {});
        //server.invoke( objectName, "start", new Object[]{}, new String[] {});

        appInfo.setAppData( objectName );

        ObjectName queryObjectName = new ObjectName
            (catalinaDomain + ":host=" 
             + ((hostName==null)?"localhost":hostName) 
             + ",path=" + ctxPath + ",type=Valve,*");

        Iterator iterator = 
            server.queryMBeans(queryObjectName, null).iterator();

        while (iterator.hasNext()) {
            ObjectName valveObjectName = 
                ((ObjectInstance) iterator.next()).getObjectName();
            String name = valveObjectName.getKeyProperty("name");
            if ((name != null) && (name.indexOf("Authenticator") >= 0)) {
                log.info("Set auth base caching off on " + valveObjectName);
                server.setAttribute(valveObjectName, new Attribute
                                    ("cache", Boolean.FALSE));
            }
        }

        log.debug("Initialized: "+appInfo+ " " + objectName);

    }


    /** 
     * Perform the tomcat specific deployment steps.
     */
    public void performUndeploy(String warUrl) 
        throws Exception {

        WebApplication appInfo = getDeployedApp(warUrl);
        if( appInfo == null ) {
            log.debug("performUndeploy, no WebApplication found for URL "
                      + warUrl);
            return;
        }

        log.info("undeploy, ctxPath="+appInfo.getMetaData().getContextRoot()
                 +", warUrl="+warUrl);

        Object context = (Object) appInfo.getAppData();

        if (context == null) {
            throw new DeploymentException
                ("URL " + warUrl + " is not deployed");
        }

        server.invoke((ObjectName) context, "destroy", new Object[] {},
                      new String [] {});
        server.unregisterMBean((ObjectName)context);

    }


    /**
     * Initialize the JNDI names from the web descriptor
     */
    private void initENC(WebApplication appInfo, 
                         WebDescriptorParser webAppParser)
        throws Exception {
        ClassLoader tcl = Thread.currentThread().getContextClassLoader();
        WebMetaData metaData = appInfo.getMetaData();
        webAppParser.parseWebAppDescriptors(tcl, metaData);
    }


//            /* We need to go through the context valves and set the cache flag
//             on any AuthenticatorBase to false or else the JBossSecurityMgrRealm
//             is not asked to authenticate every request. This can result in
//             an authenticated user thread not receiving its authenticated
//             Subject and this results in an authorization failure.
//             */
//            Valve[] valves = ((StandardContext)context).getValves();
//            for(int v = 0; v < valves.length; v ++)
//            {
//               Valve valve = valves[v];
//               if( valve instanceof AuthenticatorBase )
//               {
//                  AuthenticatorBase auth = (AuthenticatorBase) valve;
//                  auth.setCache(false);
//               }
//            }


//            // Setup the wep app JNDI java:comp/env namespace
//            ClassLoader scl = context.getLoader().getClassLoader();
//            // Enable parent delegation class loading
//            try
//            {
//               Class[] signature = {boolean.class};
//               Method setDelegate = scl.getClass().getMethod("setDelegate", signature);
//               Object[] args = {new Boolean(this.useParentDelegation)};
//               setDelegate.invoke(scl, args);
//               log.info("Using Java2 parent classloader delegation: "+useParentDelegation);
//            }
}
