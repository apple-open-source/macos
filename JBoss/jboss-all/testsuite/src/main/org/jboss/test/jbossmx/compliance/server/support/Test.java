/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.server.support;

/**
 * <description> 
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Test
         implements TestMBean
{

   public String getThisWillScream() throws MyScreamingException
   {
      throw new MyScreamingException();
   }

   public void setThisWillScream(String str) throws MyScreamingException
   {
      throw new MyScreamingException();
   }
   
   public String getThrowUncheckedException()
   {
      throw new ExceptionOnTheRun();
   }
   
   public void setThrowUncheckedException(String str) 
   {
      throw new ExceptionOnTheRun();
   }
   
   public String getError()
   {
      throw new BabarError();
   }
   
   public void setError(String str)
   {
      throw new BabarError();
   }
   
   public void setAStringAttribute(String str)
   {
      
   }
   
   public void operationWithException() throws MyScreamingException
   {
      throw new MyScreamingException();   
   }
}




