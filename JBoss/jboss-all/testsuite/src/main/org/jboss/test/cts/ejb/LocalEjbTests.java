package org.jboss.test.cts.ejb;

import java.util.Properties;
import javax.ejb.DuplicateKeyException;
import javax.naming.InitialContext;

import org.jboss.test.util.ejb.EJBTestCase;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cts.interfaces.CtsCmpLocalHome;
import org.jboss.test.cts.interfaces.CtsCmpLocal;
import org.jboss.test.cts.keys.AccountPK;
import org.jboss.logging.Logger;
import junit.framework.Test;

/** Tests of local ejbs
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class LocalEjbTests extends EJBTestCase
{
   Logger log = Logger.getLogger(LocalEjbTests.class);

   public LocalEjbTests(String methodName)
   {
      super(methodName);
   }

   public static Test suite() throws Exception
   {
		return JBossTestCase.getDeploySetup(LocalEjbTests.class, "cts.jar");
   }

   public void setUpEJB(java.util.Properties props) throws Exception
   {
      super.setUpEJB(props);
   }

   public void tearDownEJB(Properties props) throws Exception
   {
      super.tearDownEJB(props);
   }

   public void testEntityIdentity() throws Exception
   {
      InitialContext ctx = new InitialContext();
      CtsCmpLocalHome home = (CtsCmpLocalHome) ctx.lookup("ejbcts/LocalCMPBean");
      AccountPK key1 = new AccountPK("1");
      CtsCmpLocal bean1 = null;
      try
      {
         bean1 = home.create(key1, "testEntityIdentity");
      }
      catch(DuplicateKeyException e)
      {
         bean1 = home.findByPrimaryKey(key1);
      }
      AccountPK key2 = new AccountPK("2");
      CtsCmpLocal bean2 = null;
      try
      {
         bean2 = home.create(key2, "testEntityIdentity");
      }
      catch(DuplicateKeyException e)
      {
         bean2 = home.findByPrimaryKey(key2);
      }
      CtsCmpLocalHome home2 = (CtsCmpLocalHome) ctx.lookup("ejbcts/LocalCMPBean2");
      CtsCmpLocal bean12 = null;
      try
      {
         bean12 = home2.create(key1, "testEntityIdentity");
      }
      catch(DuplicateKeyException e)
      {
         bean12 = home2.findByPrimaryKey(key1);
      }

      boolean isIdentical = false;
      isIdentical = bean1.isIdentical(bean1);
      log.debug(bean1+" isIdentical to "+bean1+" = "+isIdentical);
      assertTrue(bean1+" isIdentical to "+bean1, isIdentical == true);
      isIdentical = bean2.isIdentical(bean1);
      log.debug(bean2+" isIdentical to "+bean1+" = "+isIdentical);
      assertTrue(bean2+" isIdentical to "+bean1, isIdentical == false);
      isIdentical = bean1.isIdentical(bean12);
      log.debug(bean1+" isIdentical to "+bean12+" = "+isIdentical);
      assertTrue(bean1+" isIdentical to "+bean12, isIdentical == false);
   }
}
