/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.standard.support;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public interface TortureMBean
{
   String getNiceString();
   void setNiceString(String nice);

   boolean isNiceBoolean();
   void setNiceBoolean(boolean nice);

   void setInt(int foo);
   void setIntArray(int[] foo);
   void setNestedIntArray(int[][][] foo);

   void setInteger(Integer foo);
   void setIntegerArray(Integer[] foo);
   void setNestedIntegerArray(Integer[][][] foo);

   int getMyinteger();
   int[] getMyintegerArray();
   int[][][] getMyNestedintegerArray();

   Integer getMyInteger();
   Integer[] getMyIntegerArray();
   Integer[][][] getMyNestedIntegerArray();

   // these should give an isIs right?
   boolean isready();
   Boolean isReady();

   // these should be operations
   boolean ispeachy(int peachy);
   Boolean isPeachy(int peachy);
   String issuer();
   int settlement(String thing);
   void setMulti(String foo, Integer bar);
   String getResult(String source);
   void setNothing();
   void getNothing();

   // ok, we have an attribute called Something
   // and an operation called getSomething...
   void setSomething(String something);
   void getSomething();

   // ooh yesssss
   String[][] doSomethingCrazy(Object[] args, String[] foo, int[][][] goMental);
}
