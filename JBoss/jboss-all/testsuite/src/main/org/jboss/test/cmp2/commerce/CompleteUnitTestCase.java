package org.jboss.test.cmp2.commerce;

import junit.framework.Test;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;

public class CompleteUnitTestCase extends JBossTestCase {

	public CompleteUnitTestCase(String name) {
		super(name);
	}

	public static Test suite() throws Exception {
		TestSuite testSuite = new TestSuite("CompleteTest");
		testSuite.addTestSuite(CascadeDeleteTest.class);
		testSuite.addTestSuite(TxTesterTest.class);
		testSuite.addTestSuite(QueryTest.class);
		testSuite.addTestSuite(LimitOffsetTest.class);
		testSuite.addTestSuite(CommerceTest.class);
		testSuite.addTestSuite(UserLocalTest.class);
		testSuite.addTestSuite(UserTest.class);
		testSuite.addTestSuite(OneToOneUniTest.class);
		testSuite.addTestSuite(OneToManyBiTest.class);
		testSuite.addTestSuite(ManyToOneUniTest.class);
		testSuite.addTestSuite(ManyToManyBiTest.class);
		return getDeploySetup(testSuite, "cmp2-commerce.jar");
	}	
}



