/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import org.jboss.util.NestedException;

/**
 * Generic clustering exception that can be used to replace other exceptions
 * that occur on the server. Thus, only this class is needed on the client side
 * and some specific server side exceptions class are not needed on the client side
 * (such as JMX exceptions for example). 
 * Furhtermore, it has support for "COMPLETED" status flag a la IIOP.
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>8 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class GenericClusteringException
   extends NestedException
{
   // When an invocation reaches a node, it may be invoked on the actual
   // target or not (or not completely). If COMPLETED_NO and working in 
   // a clustered environment, the client proxy is allowed to invoke
   // the same invocation on a different node. Otherwise, it will depend
   // if the target method is idempotent: this is no more the problem of
   // this class but rather the meta-data of the business environment
   // that can give this information
   //
   public final static int COMPLETED_YES = 0;
   public final static int COMPLETED_NO = 1;
   public final static int COMPLETED_MAYBE = 2;
   
   public boolean isDefinitive = true;
   
   // if yes, then the invocation may be retried against another node
   // because the state has not been modified by the current invocation
   //
   public int completed;
   
   public GenericClusteringException ()
   {
      this.completed = this.COMPLETED_MAYBE;
   }
   
   public GenericClusteringException (int CompletionStatus)
   {
      this.completed = CompletionStatus;
   }

   public GenericClusteringException (int CompletionStatus, String s)
   {
      super(s);
      this.completed = CompletionStatus;
   }

   public GenericClusteringException (int CompletionStatus, String s, boolean isDefinitive)
   {
      super(s);
      this.completed = CompletionStatus;
      this.isDefinitive = isDefinitive;
   }

   public GenericClusteringException (int CompletionStatus, Throwable nested, boolean isDefinitive)
   {
      super(nested);
      this.completed = CompletionStatus;
      this.isDefinitive = isDefinitive;
   }
   
   public GenericClusteringException (int CompletionStatus, Throwable nested)
   {
      super(nested);
      this.completed = CompletionStatus;
   }
   
   public int getCompletionStatus ()
   {
      return this.completed;
   }
   
   public void setCompletionStatus (int completionStatus)
   {
      this.completed = completionStatus;
   }
   
   // Indicates if the exception will most probably be repetitive (definitive)
   // or if it is just a temporary failure and new attempts should be tried
   //
   public boolean isDefinitive ()
   {
      return this.isDefinitive;      
   }
   
   public void setIsDefinitive (boolean definitive)
   {
      this.isDefinitive = definitive;
   }
}
