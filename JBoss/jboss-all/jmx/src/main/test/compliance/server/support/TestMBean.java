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
public interface TestMBean
{
   public String getThisWillScream() throws MyScreamingException;
   public void setThisWillScream(String str) throws MyScreamingException;
   
   public String getThrowUncheckedException();
   public void setThrowUncheckedException(String str);
   
   public String getError();
   public void setError(String str);
   
   public void setAStringAttribute(String str);
   
   public void operationWithException() throws MyScreamingException;
   
   public boolean opWithPrimBooleanReturn();
   
   public long opWithPrimLongReturn();
   
   public double opWithPrimDoubleReturn();
   
   public void opWithLongSignature(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
                                   int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20);
                                   
}
      



