/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation;

import java.io.File;
import java.net.URL;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Test suites under <tt>test.implementation</tt> are used
 * to test internal JBossMX implementation and additional
 * functionality not covered in the JMX spec.
 *
 * This suite should be run with the compliance test suite
 * (see <tt>test.compliance</tt> package) whenever new
 * features are being added.
 *
 * @see test.compliance.ComplianceSUITE
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 */

public class ImplementationSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All JBossMX Implementation Tests");

      suite.addTest(test.implementation.util.UtilSUITE.suite());
      suite.addTest(test.implementation.persistence.PersistenceSUITE.suite());
      suite.addTest(test.implementation.loading.LoadingSUITE.suite());
      suite.addTest(test.implementation.server.ServerSUITE.suite());
      suite.addTest(test.implementation.registry.RegistrySUITE.suite());
      suite.addTest(test.implementation.modelmbean.ModelMBeanSUITE.suite());
      suite.addTest(test.implementation.objectname.ObjectNameSUITE.suite());

      return suite;
   }
}
