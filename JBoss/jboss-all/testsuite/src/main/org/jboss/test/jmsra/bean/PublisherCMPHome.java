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
 * PublisherCMPHome.java
 *
 *
 * Created: Tue Apr 24 22:39:21 2001
 *
 * @author 
 * @version
 */

public interface PublisherCMPHome     extends EJBHome
{
    public PublisherCMP create(Integer nr)
	throws RemoteException, CreateException;
    public PublisherCMP findByPrimaryKey(Integer nr)
	throws RemoteException, FinderException;
    
} // PublisherCMPHome
