/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.ejb;

/**
 * Enum implementations for animals.
 *
 * @author <a href="mailto:gturner@unzane.com">Gerald Turner</a>
 */
public abstract class AnimalEnum
{
   private static int nextOrdinal = 0;

   // Constants

   private static final AnimalEnum VALUES[] = new AnimalEnum[3];

   public static final AnimalEnum DOG = new Dog("DOG");
   public static final AnimalEnum CAT = new Cat("CAT");
   public static final AnimalEnum PENGUIN = new Penguin("PENGUIN");

   // Attributes

   private final Integer ordinal;
   private final transient String name;

   // Constructor

   private AnimalEnum(String name)
   {
      this.name = name;
      this.ordinal = new Integer(nextOrdinal++);
      VALUES[ordinal.intValue()] = this;
   }

   // Package

   // Public

   public Integer getOrdinal()
   {
      return ordinal;
   }

   public String toString()
   {
      return name;
   }

   public AnimalEnum valueOf(int ordinal)
   {
      return VALUES[ordinal];
   }

   // Inner

   private static final class Dog extends AnimalEnum
   {
      public Dog(String name)
      {
         super(name);
      }
   }

   private static final class Cat extends AnimalEnum
   {
      public Cat(String name)
      {
         super(name);
      }
   }

   private static final class Penguin extends AnimalEnum
   {
      public Penguin(String name)
      {
         super(name);
      }
   }
}
