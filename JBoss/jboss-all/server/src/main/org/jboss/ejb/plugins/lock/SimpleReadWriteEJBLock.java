/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.ejb.plugins.lock;

import java.util.LinkedList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Stack;
import java.util.Collections;
import java.lang.reflect.Method;

import javax.ejb.EJBObject;
import javax.ejb.EJBException;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.Synchronization;

import org.jboss.invocation.Invocation;

/**
 *
 * This lock allows multiple read locks concurrently.  Once a writer 
 * has requested the lock, future read-lock requests whose transactions 
 * do not already have the read lock will block until all writers are 
 * done -- then all the waiting readers will concurrently go (depending
 * on the reentrant setting / methodLock).  A reader who promotes gets 
 * first crack at the write lock -- ahead of other waiting writers.  If 
 * there is already a reader that is promoting, we throw an inconsistent 
 * read exception.  Of course, writers have to wait for all read-locks 
 * to release before taking the write lock.
 *
 * @author <a href="pete@subx.com">Peter Murray</a>
 *
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2002/6/4: yarrumretep</b>
 *  <ol>
 *  <li>Initial revision
 *  </ol>
 */
public class SimpleReadWriteEJBLock extends BeanLockSupport
{
    int writersWaiting = 0;
    Transaction promotingReader = null;
    Transaction writer = null;
    HashSet readers = new HashSet();
    Object methodLock = new Object();
    boolean trace = log.isTraceEnabled();

    private void trace(Transaction tx, String message)
    {
	trace(tx, message, null);
    }

    private void trace(Transaction tx, String message, Method method)
    {
	if(method != null)
	    log.trace("LOCK(" + id + "):" + message + " : " +  tx + " - " + method.getDeclaringClass().getName() + "." + method.getName());
	else
	    log.trace("LOCK(" + id + "):" + message + " : " +  tx);
    }

    public void schedule(Invocation mi)
    {
	boolean reading = container.getBeanMetaData().isMethodReadOnly(mi.getMethod().getName());
	Transaction miTx = mi.getTransaction();

	sync();
	try
	{
	    if(reading)
	    {
		if(trace)
		    trace(miTx, "READ  (RQ)", mi.getMethod());
		getReadLock(miTx);
		if(trace)
		    trace(miTx, "READ  (GT)", mi.getMethod());
	    }
	    else
	    {
		if(trace)
		    trace(miTx, "WRITE (RQ)", mi.getMethod());
		getWriteLock(miTx);
		if(trace)
		    trace(miTx, "WRITE (GT)", mi.getMethod());
	    }
	}
	finally
	{
	    releaseSync();
	}
    }

    private void getReadLock(Transaction tx)
    {
	boolean done = false;

	while(!done)
	{
	    if(tx == null)
	    {
		done = writer == null;
	    }
	    else if(readers.contains(tx))
	    {
		done = true;
	    }
	    else if(writer == null && promotingReader == null && writersWaiting == 0)
	    {
		try
		{
		    ReadLockReliever reliever = getReliever();
		    reliever.setup(this, tx);
		    tx.registerSynchronization(reliever);
		}
		catch (Exception e)
		{
		    throw new EJBException(e);
		}
		readers.add(tx);
		done = true;
	    }
	    else if (writer != null && writer.equals(tx))
	    {
		done = true;
	    }

	    if(!done)
	    {
		if(trace)
 		    trace(tx, "READ (WT) writer:" + writer + " writers waiting: " + writersWaiting + " reader count: " + readers.size());
		
		waitAWhile(tx);
	    }
	}
    }

    private void getWriteLock(Transaction tx)
    {
	boolean done = false;
	boolean isReader;

	if(tx == null)
	    throw new EJBException("Write lock requested without transaction.");

	isReader = readers.contains(tx);
	writersWaiting++;
	while(!done)
	{
	    if(writer == null && (readers.isEmpty() || (readers.size() == 1 && isReader)))
	    {
		writersWaiting--;
		promotingReader = null;
		writer = tx;
		done = true;
	    }
	    else if (writer != null && writer.equals(tx))
	    {
		writersWaiting--;
		done = true;
	    }
	    else
	    {
		if(isReader)
		{
		    if(promotingReader != null && !promotingReader.equals(tx))
		    {
			writersWaiting--;
			throw new EJBException("Contention on read lock promotion for bean.  Exception in second transaction");
		    }
		    promotingReader = tx;
		}

		if(trace)
 		    trace(tx, "WRITE (WT) writer:" + writer + " writers waiting: " + writersWaiting + " reader count: " + readers.size());

		waitAWhile(tx);
	    }
	}
    }

    /**
     * Use readers as a semaphore object to avoid
     * creating another object
     */
    private void waitAWhile(Transaction tx)
    {
	releaseSync();
	try
	{
	    synchronized(readers)
	    {
		try
		{
		    readers.wait(txTimeout);
		}
		catch(InterruptedException e)
		{}
		checkTransaction(tx);
	    }
	}
	finally
	{
	    sync();
	}
    }
    
    /**
     * Use readers as a semaphore object to avoid
     * creating another object
     */
    private void notifyWaiters()
    {
	synchronized(readers)
	{
	    readers.notifyAll();
	}
    }

    private void releaseReadLock(Transaction transaction)
    {
	if(trace)
	    trace(transaction, "READ  (UL)");

	if(!readers.remove(transaction))
	    throw new IllegalStateException("ReadWriteEJBLock: Read lock released when it wasn't taken");

	notifyWaiters();
    }

    private void releaseWriteLock(Transaction transaction)
    {
	if(trace)
	    trace(transaction, "WRITE (UL)");

	if (synched == null)
	    throw new IllegalStateException("ReadWriteEJBLock: Do not call nextTransaction while not synched!");

	if(writer != null && !writer.equals(transaction))
	    throw new IllegalStateException("ReadWriteEJBLock: can't unlock a write lock with a different transaction");

	writer = null;
	notifyWaiters();
    }

    public void endTransaction(Transaction transaction)
    {
	releaseWriteLock(transaction);
    }
    
    public void wontSynchronize(Transaction transaction)
    {
	releaseWriteLock(transaction);
    }

    public void endInvocation(Invocation mi)
    {
    }

    private static Stack kRecycledRelievers = new Stack();

    static synchronized ReadLockReliever getReliever()
    {
	ReadLockReliever reliever;
	if(!kRecycledRelievers.empty())
	    reliever = (ReadLockReliever)kRecycledRelievers.pop();
	else
	    reliever = new ReadLockReliever();

	return reliever;
    }

    private static class ReadLockReliever implements Synchronization
    {
	SimpleReadWriteEJBLock lock;
	Transaction transaction;
	
	protected void finalize()
	{
	    recycle();
	}

	protected void recycle()
	{
	    lock = null;
	    transaction = null;
	    kRecycledRelievers.push(this);
	}

	void setup(SimpleReadWriteEJBLock lock, Transaction transaction)
	{
	    this.lock = lock;
	    this.transaction = transaction;
	}

	public void beforeCompletion()
	{
	}
	
	public void afterCompletion(int status)
	{
	    lock.sync();
	    try
	    {
		lock.releaseReadLock(transaction);
	    }
	    finally
	    {
		lock.releaseSync();
	    }
	    recycle();
	}
    }
    
    private void checkTransaction(Transaction tx)
    {
	try
	{
	    if(tx != null && tx.getStatus() == Status.STATUS_MARKED_ROLLBACK)
		throw new EJBException ("Transaction marked for rollback - probably a timeout.");
	}
	catch (Exception e)
	{
	    throw new EJBException(e);
	}
    }
}
