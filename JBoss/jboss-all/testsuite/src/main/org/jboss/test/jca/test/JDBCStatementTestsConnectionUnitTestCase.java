package org.jboss.test.jca.test;

import junit.framework.*;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.JDBCStatementTestsConnectionSession;
import org.jboss.test.jca.interfaces.JDBCStatementTestsConnectionSessionHome;

/**
 * JDBCStatementTestsConnectionUnitTestCase.java
 *
 *
 * Created: Fri Feb 14 15:15:47 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class JDBCStatementTestsConnectionUnitTestCase extends JBossTestCase {
   public JDBCStatementTestsConnectionUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(JDBCStatementTestsConnectionUnitTestCase.class, "jcatest.jar");
      Test t2 = getDeploySetup(t1, "testadapter-ds.xml");
      Test t3 = getDeploySetup(t2, "testdriver-ds.xml");
      Test t4 = getDeploySetup(t3, "jbosstestdriver.jar");
      return getDeploySetup(t4, "jbosstestadapter.rar");
   }

   /** This test will probably fail with a class cast exception if run
    * twice. The DriverManager appears to be keeping a static
    * reference to the Driver class, so reloading the
    * jbosstestdriver.sar will result in incompatible classes being
    * used.
    */
   public void testJDBCStatementTestsConnection() throws Exception
   {
      JDBCStatementTestsConnectionSessionHome home =
         (JDBCStatementTestsConnectionSessionHome)getInitialContext().lookup("JDBCStatementTestsConnectionSession");
      JDBCStatementTestsConnectionSession s = home.create();
      s.testConnectionObtainable();
   }

}// JDBCStatementTestsConnectionUnitTestCase
