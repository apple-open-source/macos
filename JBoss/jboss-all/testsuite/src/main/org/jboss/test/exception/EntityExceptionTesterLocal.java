/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.exception;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

/**
 * A test of entity beans exceptions.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface EntityExceptionTesterLocal
   extends EJBLocalObject
{
    public String getKey();

    public void applicationExceptionInTx()
       throws ApplicationException;

    public void applicationExceptionInTxMarkRollback()
       throws ApplicationException;

    public void applicationErrorInTx();

    public void ejbExceptionInTx();

    public void runtimeExceptionInTx();

    public void applicationExceptionNewTx()
       throws ApplicationException;

    public void applicationExceptionNewTxMarkRollback()
       throws ApplicationException;

    public void applicationErrorNewTx();

    public void ejbExceptionNewTx();

    public void runtimeExceptionNewTx();

    public void applicationExceptionNoTx()
       throws ApplicationException;

    public void applicationErrorNoTx();

    public void ejbExceptionNoTx();

    public void runtimeExceptionNoTx();
} 