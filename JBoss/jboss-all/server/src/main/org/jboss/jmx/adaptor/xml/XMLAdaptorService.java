/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.xml;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;

import org.jboss.naming.NonSerializableFactory;

import org.jboss.system.ServiceMBeanSupport;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

/**
 * MBean Wrapper for the XML Adaptor.
 *
 * @jmx:mbean name="jboss.jmx:type=XMLAdaptorService"
 *            extends="org.jboss.system.ServiceMBean"
 * 
 * @created June 22, 2001
 * @version <tt>$Revision: 1.4 $</tt>
 * @author  Andreas Schaefer (andreas.schaefer@madplanet.com)
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class XMLAdaptorService
  extends ServiceMBeanSupport
  implements XMLAdaptorServiceMBean
{
   public static final String JNDI_NAME = "jmx:xml";

   protected XMLAdaptor mAdaptor;

   /**
    * jmx:managed-operation
    */
   public Object[] invokeXML(Document pJmxOperations)
      throws Exception
   {
      return mAdaptor.invokeXML(pJmxOperations);
   }

   /**
    * jmx:managed-operation
    */
   public Object invokeXML(Element pJmxOperation)
      throws Exception
   {
      return mAdaptor.invokeXML( pJmxOperation );
   }

   protected ObjectName getObjectName(MBeanServer pServer, ObjectName pName)
      throws MalformedObjectNameException
   {
      return pName == null ? OBJECT_NAME : pName;
   }

   protected void startService() throws Exception
   {
      mAdaptor = new XMLAdaptorImpl(server);

      Context ctx = new InitialContext();

      try {
         // Ah ! JBoss Server isn't serializable, so we use a helper class
         NonSerializableFactory.bind( JNDI_NAME, mAdaptor );

         //AS Don't ask me what I am doing here
         Name lName = ctx.getNameParser("").parse( JNDI_NAME );
         while( lName.size() > 1 ) {
            String ctxName = lName.get( 0 );
            try {
               ctx = (Context) ctx.lookup(ctxName);
            }
            catch( NameNotFoundException e ) {
               ctx = ctx.createSubcontext(ctxName);
            }
            lName = lName.getSuffix( 1 );
         }

         // The helper class NonSerializableFactory uses address type nns, we go on to
         // use the helper class to bind the javax.mail.Session object in JNDI
         StringRefAddr lAddress = new StringRefAddr( "nns", JNDI_NAME );
         Reference lReference = new Reference(
            XMLAdaptorImpl.class.getName(),
            lAddress,
            NonSerializableFactory.class.getName(),
            null
         );
         ctx.bind( lName.get( 0 ), lReference );
      }
      finally {
         ctx.close();
      }
   }

   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();
      try {
         ctx.unbind(JNDI_NAME);
         NonSerializableFactory.unbind(JNDI_NAME);
      }
      finally {
         ctx.close();
      }
   }
}

