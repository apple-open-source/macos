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

public class Torture implements TortureMBean
{
   public Torture()
   {
   }

   public Torture(String[][] something)
   {
   }

   Torture(int foo)
   {
   }

   protected Torture(String wibble)
   {
   }

   private Torture(double trouble)
   {
   }

   public String getNiceString()
   {
      return null;
   }

   public void setNiceString(String nice)
   {
   }

   public boolean isNiceBoolean()
   {
      return false;
   }

   public void setNiceBoolean(boolean nice)
   {
   }

   public void setInt(int foo)
   {
   }

   public void setIntArray(int[] foo)
   {
   }

   public void setNestedIntArray(int[][][] foo)
   {
   }

   public void setInteger(Integer foo)
   {
   }

   public void setIntegerArray(Integer[] foo)
   {
   }

   public void setNestedIntegerArray(Integer[][][] foo)
   {
   }

   public int getMyinteger()
   {
      return 0;
   }

   public int[] getMyintegerArray()
   {
      return new int[0];
   }

   public int[][][] getMyNestedintegerArray()
   {
      return new int[0][][];
   }

   public Integer getMyInteger()
   {
      return null;
   }

   public Integer[] getMyIntegerArray()
   {
      return new Integer[0];
   }

   public Integer[][][] getMyNestedIntegerArray()
   {
      return new Integer[0][][];
   }

   // these should give an isIs right?
   public boolean isready()
   {
      return false;
   }

   public Boolean isReady()
   {
      return null;
   }

   // these should be operations
   public boolean ispeachy(int peachy)
   {
      return false;
   }

   public Boolean isPeachy(int peachy)
   {
      return null;
   }

   public String issuer()
   {
      return null;
   }

   public int settlement(String thing)
   {
      return 0;
   }

   public void setMulti(String foo, Integer bar)
   {
   }

   public String getResult(String source)
   {
      return null;
   }

   public void setNothing()
   {
   }

   public void getNothing()
   {
   }

   // ok, we have an attribute called Something
   // and an operation called getSomething...
   public void setSomething(String something)
   {
   }

   public void getSomething()
   {
   }

   // ooh yesssss
   public String[][] doSomethingCrazy(Object[] args, String[] foo, int[][][] goMental)
   {
      return new String[0][];
   }
}
