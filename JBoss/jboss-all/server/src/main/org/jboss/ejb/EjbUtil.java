/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.net.URL;
import java.net.MalformedURLException;

import java.util.Iterator;
import java.util.StringTokenizer;

import javax.management.MBeanServer;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.MainDeployerMBean;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.util.Strings;

/** Utility methods for resolving ejb-ref and ejb-local-ref within the
 * scope of a deployment.
 *
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 * @author Scott.Stark@jboss.org
 *
 * @version $Revision: 1.1.2.4 $
 */
public final class EjbUtil
{
   private static final Logger log = Logger.getLogger( EjbUtil.class );

   /**
    * Resolves an &lt;ejb-link&gt; target for an &lt;ejb-ref&gt; entry and
    * returns the name of the target in the JNDI tree.
    *
    * @param di DeploymentInfo
    * @param link Content of the &lt;ejb-link&gt; entry.
    *
    * @return The JNDI Entry of the target bean; <code>null</code> if
    *         no appropriate target could be found.
    */
   public static String findEjbLink( MBeanServer server, DeploymentInfo di,
      String link )
   {
      return resolveLink( server, di, link, false );
   }

   /**
    * Resolves an &lt;ejb-link&gt; target for an &lt;ejb-local-ref&gt; entry
    * and returns the name of the target in the JNDI tree.
    *
    * @param di DeploymentInfo
    * @param link Content of the &lt;ejb-link&gt; entry.
    *
    * @return The JNDI Entry of the target bean; <code>null</code> if
    *         no appropriate target could be found.
    */
   public static String findLocalEjbLink( MBeanServer server,
      DeploymentInfo di, String link )
   {
      return resolveLink( server, di, link, true );
   }

   private static String resolveLink( MBeanServer server, DeploymentInfo di,
      String link, boolean isLocal )
   {
      if( link == null )
      {
         return null;
      }

      if( log.isTraceEnabled() )
      {
         log.trace( "resolveLink( {" + di + "}, {" + link + "}, {" + isLocal +
            "}");
      }

      if( di == null )
      {
         // We should throw an IllegalArgumentException here probably?
         return null;
      }

      if( link.indexOf('#') != -1 )
      {
         // <ejb-link> is specified in the form path/file.jar#Bean
         return resolveRelativeLink( server, di, link, isLocal );
      }
      else
      {
         // <ejb-link> contains a Bean Name, scan the DeploymentInfo tree
         DeploymentInfo top = di;
         while( top.parent != null )
         {
            top = top.parent;
         }

         return resolveAbsoluteLink( top, link, isLocal );
      }
   }

   private static String resolveRelativeLink( MBeanServer server,
      DeploymentInfo di, String link, boolean isLocal )
   {

      String path = link.substring( 0, link.indexOf('#') );
      String ejbName = link.substring( link.indexOf('#') + 1 );
      String us = di.url.toString();

      // Remove the trailing slash for unpacked deployments
      if (us.charAt(us.length()-1) == '/')
         us = us.substring(0, us.length()-1);

      String ourPath = us.substring( 0, us.lastIndexOf('/') );

      if( log.isTraceEnabled() )
      {
         log.trace( "Resolving relative link: " + link );
         log.trace( "Looking for: '" + link + "', we're located at: '" +
            ourPath + "'" );
      }

      for( StringTokenizer st = new StringTokenizer(path, "/");
         st.hasMoreTokens(); )
      {
         String s = st.nextToken();
         if( s.equals("..") )
         {
            ourPath = ourPath.substring( 0, ourPath.lastIndexOf('/') );
         }
         else
         {
            ourPath += "/" + s;
         }
      }

      URL target = null;

      try
      {
         target = Strings.toURL(ourPath);
      }
      catch( MalformedURLException mue )
      {
         log.warn( "Can't construct URL for: " + ourPath );
         return null;
      }

      DeploymentInfo targetInfo = null;
      try
      {
         targetInfo = (DeploymentInfo)server.invoke(
            MainDeployerMBean.OBJECT_NAME,
            "getDeployment",
            new Object[] {target},
            new String[] {URL.class.getName()}
            );
      }
      catch( Exception e )
      {
         log.warn( "Got Exception when looking for DeploymentInfo: " + e );
         return null;
      }

      if( targetInfo == null )
      {
         log.warn( "Can't locate deploymentInfo for target: " + target );
         return null;
      }

      if( log.isTraceEnabled() )
      {
         log.trace( "Found appropriate DeploymentInfo: " + targetInfo );
      }

      String linkTarget = null;
      if( targetInfo.metaData instanceof ApplicationMetaData )
      {
         ApplicationMetaData appMD = (ApplicationMetaData)targetInfo.metaData;
         BeanMetaData beanMD = appMD.getBeanByEjbName( ejbName );

         if( beanMD != null )
         {
            linkTarget = getJndiName(beanMD, isLocal);
         }
         else
         {
            log.warn("No Bean named '" + ejbName + "' found in '" + path +"'!");
         }
      }
      else
      {
         log.warn("DeploymentInfo " + targetInfo + " is not an EJB .jar " +"file!");
      }

      return linkTarget;
   }

   private static String resolveAbsoluteLink( DeploymentInfo di,
      String link, boolean isLocal )
   {
      if( log.isTraceEnabled() )
      {
         log.trace( "Resolving absolute link" );
      }

      String ejbName = null;

      // Search current DeploymentInfo
      if( di.metaData instanceof ApplicationMetaData )
      {
         ApplicationMetaData appMD = (ApplicationMetaData)di.metaData;
         BeanMetaData beanMD = appMD.getBeanByEjbName( link );
         if( beanMD != null )
         {
            ejbName = getJndiName(beanMD, isLocal);
            if( log.isTraceEnabled() )
            {
               log.trace("Found Bean: " + beanMD + ", resolves to: " + ejbName);
            }

            return ejbName;
         }
      }

      // Search each subcontext
      Iterator it = di.subDeployments.iterator();
      while( it.hasNext() && ejbName == null )
      {
         DeploymentInfo child = (DeploymentInfo)it.next();
         ejbName = resolveAbsoluteLink( child, link, isLocal );
      }

      return ejbName;
   }

   private static String getJndiName(BeanMetaData beanMD, boolean isLocal)
   {
      String jndiName = null;
      if( isLocal )
      {
         // Validate that there is a local home associated with this bean
         String localHome = beanMD.getLocalHome();
         if( localHome != null )
            jndiName = beanMD.getLocalJndiName();
         else
         {
            log.warn("LocalHome jndi name requested for: '"+beanMD.getEjbName()
               + "' but there is no LocalHome class");
         }
      }
      else
      {
         jndiName = beanMD.getJndiName();
      }
      return jndiName;
   }
}
/*
vim:ts=3:sw=3:et
*/
