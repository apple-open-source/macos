/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

import java.net.URL;
import java.util.Properties;

import javax.swing.*;

import org.jboss.console.manager.interfaces.SimpleTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.impl.HttpLinkTreeAction;
import org.jboss.console.remote.AppletRemoteMBeanInvoker;
import org.jboss.console.remote.SimpleRemoteMBeanInvoker;

/**
 * AdminTreeBrowser container for applets
 *
 * @see org.jboss.console.navtree.AdminTreeBrowser
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 */
public class AppletBrowser extends javax.swing.JApplet
{
   AdminTreeBrowser treeBrowser = null;   
   AppletAdminContext ctx = null;   
   
   public static final String RIGHT_FRAME_NAME = "right";
   protected String sessionId = null;
   protected String pmJmxName = null;
   

   public AppletBrowser () 
   {            
   }
   
   public void start ()
   {
      try
      {
         ctx = new AppletAdminContext ();
         
         initAppletParams();
         
         treeBrowser = new AdminTreeBrowser (ctx);
         
         initComponents ();
         
         initRefreshThread ();
         
      }
      catch (Exception e)
      {
         e.printStackTrace ();
      }
   }
   
   public void refreshTree (boolean force)
   {
      treeBrowser.refreshTree(force);
   }
   
   protected void initAppletParams()
   {
      sessionId = getParameter("SessionId");
      if (sessionId != null)
         sessionId = "jsessionid=" + sessionId;
      else
         sessionId = "";

      this.pmJmxName = getParameter("PMJMXName");
      if( pmJmxName == null )
         pmJmxName = "jboss.admin:service=PluginManager";
   }

   protected void initComponents()
   {
      javax.swing.JTree tree = treeBrowser.getTree();
      javax.swing.JScrollPane scrollPane = new javax.swing.JScrollPane(tree);
      
      scrollPane.setBorder(javax.swing.BorderFactory.createEmptyBorder(0,0,0,0));
      
      getContentPane().add(scrollPane, java.awt.BorderLayout.CENTER);
      
      //getContentPane().add(tree, java.awt.BorderLayout.CENTER);
   }
   
   protected void initRefreshThread ()
   {
      try
      {         
         String strRefreshSec = getParameter("RefreshTime");
         if (strRefreshSec != null && !"".equals(strRefreshSec))
         {
            final long refresh = Long.parseLong(strRefreshSec);
            Thread t = new Thread ( new Runnable()
               {
                  public synchronized void run ()
                  {
                     long timeout = refresh*1000;
                     while (true)
                     {
                        try
                        {
                           this.wait(timeout);
                           treeBrowser.refreshTree(false);
                        }
                        catch (Exception displayed)
                        {
                           //displayed.printStackTrace();
                        }
                     }
                  }
               }
            );
            
            t.start();
         }
      }
      catch (Exception displayed)
      {
         displayed.printStackTrace();
      }      
   }

   /** Allow the applet to be run as an application:
    * java -cp applet.jar org.jboss.console.navtree.AppletBrowser
    * @param args
    * @throws Exception
    */
   public static void main(String[] args) throws Exception
   {
      JApplet applet = new AppletBrowser();
      applet.setStub(new MainAppletStub());
      JFrame frame = new JFrame("Administration Console");
      frame.getContentPane().add(applet);
      frame.setSize(600, 500);
      frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
      applet.init();
      applet.start();
      frame.setVisible(true);
   }

   public class AppletAdminContext implements TreeContext
   {
      
      String webHost = null;
      String hostname = null;
      
      org.jboss.console.remote.SimpleRemoteMBeanInvoker invoker = null;
      
      public AppletAdminContext ()
      {
         //webHost = getCodeBase ().toString ();   
         java.net.URL root = getCodeBase();                        
         webHost = root.getProtocol() + ":";
         if (root.getAuthority() != null && root.getAuthority().length() > 0) {
            webHost+="//";
            webHost+=root.getAuthority();
         }       
         
         if (!webHost.endsWith ("/"))
            webHost = webHost + "/";
            
         hostname = getCodeBase ().getHost ();
      }
      
      public synchronized SimpleRemoteMBeanInvoker getRemoteMBeanInvoker ()
      {
         if (invoker == null)
         {
            System.out.println (getCodeBase().toString() + "Invoker");
            try
            {
               invoker = new AppletRemoteMBeanInvoker (getCodeBase().toString() + "Invoker");
            }
            catch (Exception displayed)
            {
               displayed.printStackTrace ();
            }
         }
         
         return invoker;
      }
      
      public void doAdminTreeAction (TreeAction action)
      {
         if (action != null && action instanceof HttpLinkTreeAction)
         {
            HttpLinkTreeAction act = (HttpLinkTreeAction)action;
            openLink (act.getTarget (), act.getFrame());
         }
      }
      
      public void doPopupMenuAction (SimpleTreeNodeMenuEntry entry)
      {
      
         TreeAction ta = entry.getAction ();

         if (ta instanceof HttpLinkTreeAction)
         {
            HttpLinkTreeAction act = (HttpLinkTreeAction)ta;
            openLink ( act.getTarget (), act.getFrame());
         }
         else if (ta instanceof AppletTreeAction)
         {
            ((AppletTreeAction)ta).doAction(ctx, AppletBrowser.this);
         }
      }
      
      public java.util.Properties getJndiProperties ()
      {
         Properties props = new Properties (); // to be improved? (to read from Applet properties and pass as parameter
         props.setProperty ("java.naming.factory.initial", "org.jnp.interfaces.NamingContextFactory");
         props.setProperty ("java.naming.factory.url.pkgs", "org.jboss.naming:org.jnp.interfaces");
         props.setProperty ("java.naming.provider.url", hostname);
         
         return props;
      }
      
      public String getServiceJmxName () { return pmJmxName; }
         
      public void openLink (String target, String frame)
      {
         try
         {
            if (target == null)
            {
               return;
            }
            else
            {
               System.out.println(target);
               if (frame == null)
                  getAppletContext ().showDocument ( new URL(localizeUrl(target)), RIGHT_FRAME_NAME);
               else
                  getAppletContext ().showDocument ( new URL(localizeUrl(target)), frame);
            }
         }
         catch (Exception tobad) { tobad.printStackTrace (); }
      }
      
      public String localizeUrl (String sourceUrl)
      {
         String target = sourceUrl;
         
         if (target == null)
            return null;
         
         if (!target.toLowerCase ().startsWith ("http"))
         {
            if (target.startsWith ("/"))
               target = target.substring (1);
            target = webHost + target;
            
            if (target.indexOf("?") >= 0)
                target = target + "&" + sessionId;
            else
                target = target + ";" + sessionId;
         }
         
         return target;
      }
   


   }
}
