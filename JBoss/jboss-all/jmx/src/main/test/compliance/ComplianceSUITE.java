/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Everything under test.compliance is a set of unit tests
 * which should pass as much as possible against the JMX RI
 *
 * Additions to this package are welcome/encouraged - adding a
 * test that fails is a great way to communicate a bug ;-)
 *
 * Anyone contributing to the JBoss JMX impl should seriously
 * consider providing a testcase prior to making code changes
 * in the impl itself - ala XP.
 *
 * The only restriction is that if the tests don't succeed against
 * the RI, the test error message should indicate that the test
 * will fail on the RI (preferred way) or at least comment the testcase
 * stating expected failures.  Either way, you should comment the code
 * justifying why the test is valid despite failing against the RI.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class ComplianceSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Compliance Tests");

      suite.addTest(test.compliance.objectname.ObjectNameSUITE.suite());
      suite.addTest(test.compliance.standard.StandardSUITE.suite());
      suite.addTest(test.compliance.registration.RegistrationSUITE.suite());
      suite.addTest(test.compliance.server.ServerSUITE.suite());
      suite.addTest(test.compliance.modelmbean.ModelMBeanSUITE.suite());
      suite.addTest(test.compliance.notcompliant.NCMBeanSUITE.suite());
      suite.addTest(test.compliance.loading.LoadingSUITE.suite());
      suite.addTest(test.compliance.varia.VariaSUITE.suite());
      suite.addTest(test.compliance.query.QuerySUITE.suite());
      suite.addTest(test.compliance.metadata.MetaDataSUITE.suite());
      suite.addTest(test.compliance.relation.RelationSUITE.suite());
      suite.addTest(test.compliance.openmbean.OpenMBeanSUITE.suite());
      
      return suite;
   }
}
