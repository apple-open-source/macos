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

/**
 * Static method repository class.
 *
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 *
 * @version $Revision: 1.1.2.2 $
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
         target = new URL( ourPath );
      }
      catch( MalformedURLException mue )
      {
         log.error( "Can't construct URL for: " + ourPath );
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
         log.error( "Got Exception when looking for DeploymentInfo: " + e );
         return null;
      }

      if( targetInfo == null )
      {
         log.error( "Can't locate deploymentInfo for target: " + target );
         return null;
      }

      if( log.isTraceEnabled() )
      {
         log.trace( "Found appropriate DeploymentInfo: " + targetInfo );
      }

      if( targetInfo.metaData instanceof ApplicationMetaData )
      {
         ApplicationMetaData appMD = (ApplicationMetaData)targetInfo.metaData;
         BeanMetaData beanMD = appMD.getBeanByEjbName( ejbName );

         if( beanMD != null )
         {
            if( isLocal )
            {
               return beanMD.getLocalJndiName();
            }
            else
            {
               return beanMD.getJndiName();
            }
         }

         log.error( "No Bean named '" + ejbName + "' found in '" + path +
            "'!" );
      }
      else
      {
         log.error( "DeploymentInfo " + targetInfo + " is not an EJB .jar " +
            "file!" );
      }

      return null;

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
            if( isLocal )
            {
               ejbName = beanMD.getLocalJndiName();
            }
            else
            {
               ejbName = beanMD.getJndiName();
            }

            if( log.isTraceEnabled() )
            {
               log.trace( "Found Bean: " + beanMD + ", resolves to: " + ejbName );
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

}
/*
vim:ts=3:sw=3:et
*/
