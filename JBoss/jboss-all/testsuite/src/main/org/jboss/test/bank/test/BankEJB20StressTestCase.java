package org.jboss.test.bank.test;

import junit.framework.Test;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;

public class BankEJB20StressTestCase extends JBossTestCase
{
   public static Test suite() throws Exception
   {
      return getDeploySetup(BankStressTestCase.class, "bank-ejb20.jar");
   }

	public BankEJB20StressTestCase(String name)
	{
		super(name);
	}
}

