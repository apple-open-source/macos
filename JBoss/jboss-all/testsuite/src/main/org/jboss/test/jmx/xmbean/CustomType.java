package org.jboss.test.jmx.xmbean;

import java.io.Serializable;

/** An object with an x.y string representation
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class CustomType implements Serializable
{
   int x;
   int y;

   public CustomType(int x, int y)
   {
      this.x = x;
      this.y = y;
   }

   public int getX()
   {
      return x;
   }
   public int getY()
   {
      return y;
   }
   public String toString()
   {
      return "{"+x+"."+y+"}";
   }
}
