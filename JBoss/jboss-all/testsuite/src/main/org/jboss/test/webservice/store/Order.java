/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Order.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

import java.util.Date;
import java.util.Collection;
import java.util.Arrays;

/**
 * A kind of entity-value-object that is mapped via Axis to 
 * a corresponding remote entity bean.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class Order
{
   protected String id;
   protected Date confirmationDate;
   protected Date dueDate;
   protected BusinessPartner businessPartner;
   protected Collection lines = new java.util.ArrayList(1);

   public String getId()
   {
      return id;
   }

   public Date getConfirmationDate()
   {
      return confirmationDate;
   }

   public Date getDueDate()
   {
      return dueDate;
   }

   public void setDueDate()
   {
      this.dueDate = dueDate;
   }

   public BusinessPartner getBusinessPartner()
   {
      return businessPartner;
   }

   public Line[] getLines()
   {
      return (Line[]) lines.toArray(new Line[lines.size()]);
   }

   public void setLines(Line[] lines)
   {
      this.lines = Arrays.asList(lines);
   }

   public void addLine(Line line)
   {
      lines.add(line);
   }

   public void removeLine(Line line)
   {
      lines.remove(line);
   }
}
