package org.jboss.test.deadlock.interfaces;

public class BeanOrder
   implements java.io.Serializable
{
   public String[] order;
   public int next;

   public BeanOrder()
   {
   }

   public BeanOrder(String[] order)
   {
      this.order = order;
      next = 0;
   }
}
