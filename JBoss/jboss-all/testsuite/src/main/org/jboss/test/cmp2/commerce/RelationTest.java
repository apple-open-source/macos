package org.jboss.test.cmp2.commerce;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

public class RelationTest extends TestCase {

	public RelationTest(String name) {
		super(name);
	}

	public static Test suite() {
		TestSuite testSuite = new TestSuite("RelationTest");
		testSuite.addTestSuite(OneToOneUniTest.class);
		testSuite.addTestSuite(OneToManyBiTest.class);
		testSuite.addTestSuite(ManyToOneUniTest.class);
		testSuite.addTestSuite(ManyToManyBiTest.class);
		return testSuite;
	}	
}



