/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

/**
 * <description> 
 *
 * @see <related>
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
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
   
   /**
    * returns true
    */
   public boolean opWithPrimBooleanReturn()
   {
      return true;
   }
   
   /**
    * Returns value 1234567890123
    */
   public long opWithPrimLongReturn()
   {
      return 1234567890123l;
   }
   
   /**
    * Returns 0.1234567890123
    */
   public double opWithPrimDoubleReturn()
   {
      return 0.1234567890123d;
   }
   
   public void opWithLongSignature(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
                                   int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20)
   {
      
   }
}




