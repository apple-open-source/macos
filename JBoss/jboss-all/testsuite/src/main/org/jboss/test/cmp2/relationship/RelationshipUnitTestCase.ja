package org.jboss.test.cmp2.relationship;

import junit.framework.Test;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;

public class RelationshipUnitTestCase extends JBossTestCase {

	public RelationshipUnitTestCase(String name) {
		super(name);
	}

	public static Test suite() throws Exception {
		TestSuite testSuite = new TestSuite("RelationshipUnitTestCase");
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.oneToOneBidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.oneToOneUnidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.oneToManyBidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.oneToManyUnidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.manyToOneUnidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.manyToManyBidirectional.ABTest.class);
		testSuite.addTestSuite(org.jboss.test.cmp2.relationship.manyToManyUnidirectional.ABTest.class);
		return getDeploySetup(testSuite, "cmp2-relationship.jar");
	}	
}



