/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.client.test;

import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;

import org.jboss.test.JBossTestCase;
import org.jboss.metadata.ClientMetaData;
import org.jboss.metadata.EjbRefMetaData;
import org.jboss.metadata.EnvEntryMetaData;
import org.jboss.metadata.ResourceRefMetaData;
import org.jboss.metadata.ResourceEnvRefMetaData;
import org.jboss.metadata.XmlFileLoader;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

/** Unit tests of the application-client metadata parsing

@author Scott.Stark@jboss.org
@version $Revision: 1.1.2.1 $
**/
public class MetaDataUnitTestCase extends JBossTestCase
{
   ClientMetaData metaData = new ClientMetaData();

   public MetaDataUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL appClientXml = loader.getResource("client/application-client.xml");
      Document appClientDoc = XmlFileLoader.getDocument(appClientXml);
      Element appClient = appClientDoc.getDocumentElement();
      metaData.importClientXml(appClient);      
      URL jbossClientXml = loader.getResource("client/jboss-client.xml");
      Document jbossClientDoc = XmlFileLoader.getDocument(jbossClientXml);
      Element jbossClient = jbossClientDoc.getDocumentElement();
      metaData.importJbossClientXml(jbossClient);      
   }

   public void testEnvEntries()
      throws Exception
   {
      getLog().debug("+++ testEnvEntries");
      ArrayList envEntries = metaData.getEnvironmentEntries();
      assertTrue("There are 3 env-entries", envEntries.size() == 3);
      EnvEntryMetaData str0 = (EnvEntryMetaData) envEntries.get(0);
      assertTrue("Env[0].name == String0", str0.getName().equals("String0"));
      assertTrue("Env[0].type == java.lang.String", str0.getType().equals("java.lang.String"));
      assertTrue("Env[0].value == String0Value", str0.getValue().equals("String0Value"));

      EnvEntryMetaData flt0 = (EnvEntryMetaData) envEntries.get(1);
      assertTrue("Env[1].name == Float0", flt0.getName().equals("Float0"));
      assertTrue("Env[1].type == java.lang.Float", flt0.getType().equals("java.lang.Float"));
      Float pi = new Float(3.14);
      int ftest = pi.compareTo(new Float(flt0.getValue()));
      assertTrue("Env[1].value == 3.14, test="+ftest, ftest == 0);

      EnvEntryMetaData long0 = (EnvEntryMetaData) envEntries.get(2);
      Long n = new Long(123456789);
      int ltest = n.compareTo(new Long(long0.getValue()));
      assertTrue("Env[2].name == Long0", long0.getName().equals("Long0"));
      assertTrue("Env[2].type == java.lang.Long", long0.getType().equals("java.lang.Long"));
      assertTrue("Env[2].value == 123456789, test="+ltest, ltest == 0);
   }

   public void testEjbRefs() throws Exception
   {
      getLog().debug("+++ testEjbRefs");
      HashMap ejbRefs = metaData.getEjbReferences();
      assertTrue("There are 2 ejb-refs", ejbRefs.size() == 2);
      EjbRefMetaData ref = (EjbRefMetaData) ejbRefs.get("ejb/StatelessSessionBean");
      assertTrue("ejb/StatelessSessionBean ejb-ref exists", ref != null);
      assertTrue("ejb-ref-name: StatelessSessionBean", ref.getName().equals("ejb/StatelessSessionBean"));
      assertTrue("home: org.jboss.test.cts.interfaces.StatelessSessionHome",
         ref.getHome().equals("org.jboss.test.cts.interfaces.StatelessSessionHome"));
      assertTrue("remote: org.jboss.test.cts.interfaces.StatelessSession",
         ref.getRemote().equals("org.jboss.test.cts.interfaces.StatelessSession"));
      assertTrue("ejb-ref-type: Session",
         ref.getType().equals("Session"));
      assertTrue("jnd-name: ejbcts/StatelessSessionHome",
         ref.getJndiName().equals("ejbcts/StatelessSessionHome"));

      ref = (EjbRefMetaData) ejbRefs.get("ejb/StatelessSessionBean2");
      assertTrue("ejb/StatelessSessionBean ejb-ref exists", ref != null);
      assertTrue("ejb-ref-name: StatelessSessionBean2", ref.getName().equals("ejb/StatelessSessionBean2"));
      assertTrue("home: org.jboss.test.cts.interfaces.StatelessSessionHome",
         ref.getHome().equals("org.jboss.test.cts.interfaces.StatelessSessionHome"));
      assertTrue("remote: org.jboss.test.cts.interfaces.StatelessSession",
         ref.getRemote().equals("org.jboss.test.cts.interfaces.StatelessSession"));
      assertTrue("ejb-ref-type: Session",
         ref.getType().equals("Session"));
      assertTrue("ejb-link: StatelessSessionBean",
         ref.getLink().equals("StatelessSessionBean"));
   }

   public void testResourceRefs() throws Exception
   {
      getLog().debug("+++ testResourceRefs");
      HashMap resRefs = metaData.getResourceReferences();
      assertTrue("There are 1 resource-ref", resRefs.size() == 1);
      ResourceRefMetaData ref = (ResourceRefMetaData) resRefs.get("url/JBossHome");
      assertTrue("res-ref-name == url/JBossHome",
         ref.getRefName().equals("url/JBossHome"));
      assertTrue("res-type == java.net.URL",
         ref.getType().equals("java.net.URL"));
      assertTrue("jnd-name: http://www.jboss.org",
         ref.getJndiName().equals("http://www.jboss.org"));
   }

   public void testResourceEnvRefs() throws Exception
   {
      getLog().debug("+++ testResourceRefs");
      HashMap resRefs = metaData.getResourceEnvReferences();
      assertTrue("There are 1 resource-ref", resRefs.size() == 1);
      ResourceEnvRefMetaData ref = (ResourceEnvRefMetaData) resRefs.get("jms/aQueue");
      assertTrue("resource-env-ref-name == jms/aQueue",
         ref.getRefName().equals("jms/aQueue"));
      assertTrue("resource-env-ref-type == javax.jms.Queue",
         ref.getType().equals("javax.jms.Queue"));
      assertTrue("jnd-name: queue/testQueue",
         ref.getJndiName().equals("queue/testQueue"));
   }

   /** Override the testServerFound since these test don't need the JBoss server
    */
   public void testServerFound()
   {
   }
   public void initDelegate()
   {
   }
}
