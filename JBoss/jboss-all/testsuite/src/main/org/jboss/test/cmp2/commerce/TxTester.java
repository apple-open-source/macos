package org.jboss.test.cmp2.commerce;

import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;

public interface TxTester extends EJBLocalObject
{
  /**
    *  Test modification of a CMR collection outside of the transaction
    *  context in which it was initially materialized.
    *  According to EJB2.0 10.3.8 this should throw
    *  <code>java.lang.IllegalStateException</code>.
    *
    *  @return <code>true</code> iff this test passed.
    */
   boolean accessCMRCollectionWithoutTx();
}
