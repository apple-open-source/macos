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

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBException;

import org.jboss.test.util.ejb.EntitySupport;

/**
 * 3rdparty bean to help test JMS RA transactions.
 *
 * <p>Created: Tue Apr 24 22:32:41 2001
 *
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.3 $
 */
public class PublisherCMPBean
    extends EntitySupport
{
    public Integer nr;
    
    public PublisherCMPBean() {
        // empty
    }
    
    public Integer getNr() {
        return nr;
    }

    public void setNr(Integer nr) {
        this.nr = nr;
    }

    public void ok(int nr) {
        // Do nothing
    }

    public void error(int nr) {
        // Roll back throug an exception
        throw new EJBException("Roll back!");
    }
    
    // EntityBean implementation -------------------------------------
    
    public Integer ejbCreate(Integer nr)
        throws CreateException
    {
        this.nr = nr;
        return null;
    }

    public void ejbPostCreate(Integer nr)
        throws CreateException
    {
    }

    public void ejbLoad()
    {
    }
    
} // PublisherCMPBean
