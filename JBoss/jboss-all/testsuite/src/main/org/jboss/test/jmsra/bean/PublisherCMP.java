/*
 * Copyright (c) 2001 Peter Antman Tim <peter.antman@tim.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.test.jmsra.bean;

import java.rmi.*;
import javax.ejb.*;
/**
 * PublisherCMP.java
 *
 *
 * Created: Tue Apr 24 22:37:41 2001
 *
 * @author 
 * @version
 */

public interface PublisherCMP extends EJBObject {
    public Integer getNr() throws RemoteException;
   
   public void setNr(Integer nr)throws RemoteException;

   public void ok(int nr)throws RemoteException;

    public void error(int nr)throws RemoteException;
} // PublisherCMP
