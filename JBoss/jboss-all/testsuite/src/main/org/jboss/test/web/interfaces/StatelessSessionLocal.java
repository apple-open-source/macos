package org.jboss.test.web.interfaces;

import javax.ejb.EJBLocalObject;
import java.rmi.RemoteException;

/** A trivial SessionBean local interface.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.6.1 $
 */
public interface StatelessSessionLocal extends EJBLocalObject
{
   /** A method that returns its arg */
   public String echo(String arg);

   /** A method that does nothing. It is used to test call optimization.
    */
   public void noop(ReferenceTest test, boolean optimized);

   /** Forward a request to another StatelessSession's echo method */
   public String forward(String echoArg);

   /** A method deployed with no method permissions */
   public void unchecked();

   /** A method deployed with method permissions such that only a run-as
    * assignment will allow access. 
    */
   public void checkRunAs();
}
