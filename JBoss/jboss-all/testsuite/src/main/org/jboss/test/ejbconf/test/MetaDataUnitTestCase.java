/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.ejbconf.test;

import java.net.URL;
import java.util.Iterator;
import java.util.Set;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.invocation.InvocationType;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.MethodMetaData;
import org.jboss.metadata.XmlFileLoader;
import org.jboss.security.SimplePrincipal;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

/** Tests of ejb-jar.xml metadata parsing.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class MetaDataUnitTestCase extends TestCase 
{
   public MetaDataUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite(MetaDataUnitTestCase.class);
      return suite;
   }

   public void testMethodPermissions() throws Exception
   {
      URL configURL = getClass().getResource("/ejbconf/ejb-jar-permission.xml");
      if( configURL == null )
         throw new Exception("Failed to find /ejbconf/ejb-jar-permission.xml");
      Document configDoc = XmlFileLoader.getDocument(configURL, true);
      ApplicationMetaData appData = new ApplicationMetaData();
      appData.importEjbJarXml(configDoc.getDocumentElement());

      SimplePrincipal echo = new SimplePrincipal("Echo");
      SimplePrincipal echoLocal = new SimplePrincipal("EchoLocal");
      SimplePrincipal internal = new SimplePrincipal("InternalRole");

      BeanMetaData ss = appData.getBeanByEjbName("StatelessSession");
      Class[] sig = {};
      Set perms = ss.getMethodPermissions("create", sig, InvocationType.HOME);
      assertTrue("Echo can invoke StatelessSessionHome.create", perms.contains(echo));
      assertTrue("EchoLocal cannot invoke StatelessSessionHome.create", perms.contains(echoLocal) == false);

      perms = ss.getMethodPermissions("create", sig, InvocationType.LOCALHOME);
      assertTrue("Echo can invoke StatelessSessionLocalHome.create", perms.contains(echo));
      assertTrue("EchoLocal can invoke StatelessSessionLocalHome.create", perms.contains(echoLocal));
   }

}
