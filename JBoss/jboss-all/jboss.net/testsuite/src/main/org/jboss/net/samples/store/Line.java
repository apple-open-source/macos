/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Line.java,v 1.1 2002/04/02 13:48:41 cgjung Exp $

package org.jboss.net.samples.store;

/**
 * A kind of entity-value-object that is mapped via Axis to 
 * a corresponding remote entity bean.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1 $
 */

public class Line {
   protected String id;
   protected Item item;
   protected double quantity;
   protected Unit unit;

   public String getId() {
      return id;
   }

   public Item getItem() {
      return item;
   }
   public void setItem(Item item) {
      this.item = item;
   }

   public void setQuantity(double quantity) {
      this.quantity = quantity;
   }

   public double getQuantity() {
      return quantity;
   }

   public void setUnit(Unit unit) {
      this.unit = unit;
   }

   public Unit getUnit() {
      return unit;
   }
}