/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.ejb;


/**
 * Enum implementations for colors.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ColorEnum
{
   private static int nextOrdinal = 0;

   // Constants

   private static final ColorEnum VALUES[] =  new ColorEnum[3];

   public static final ColorEnum RED = new Red("RED");
   public static final ColorEnum GREEN = new Green("GREEN");
   public static final ColorEnum BLUE = new Blue("BLUE");

   // Attributes

   private final Integer ordinal;
   private final transient String name;

   // Constructor

   private ColorEnum(String name)
   {
      this.name = name;
      this.ordinal = new Integer(nextOrdinal++);
      VALUES[ordinal.intValue()] = this;
   }

   // Public

   public Integer getOrdinal()
   {
      return ordinal;
   }

   public String toString()
   {
      return name;
   }

   public ColorEnum valueOf(int ordinal)
   {
      return VALUES[ordinal];
   }

   // Inner

   private static final class Red extends ColorEnum
   {
      public Red(String name)
      {
         super(name);
      }
   }

   private static final class Green extends ColorEnum
   {
      public Green(String name)
      {
         super(name);
      }
   }

   private static final class Blue extends ColorEnum
   {
      public Blue(String name)
      {
         super(name);
      }
   }
}
