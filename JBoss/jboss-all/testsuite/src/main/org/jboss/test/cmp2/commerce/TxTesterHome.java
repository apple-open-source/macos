package org.jboss.test.cmp2.commerce;

import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;

public interface TxTesterHome extends EJBLocalHome
{
   TxTester create() throws CreateException;
}
