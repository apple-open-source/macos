package org.jboss.verifier.event;

/*
 * Class org.jboss.verifier.event.VerificationEventGeneratorSupport
 * Copyright (C) 2000  Juha Lindfors
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
 *
 * This package and its source code is available at www.jboss.org
 * $Id: VerificationEventGeneratorSupport.java,v 1.4 2001/12/01 17:29:30 juhalindfors Exp $
 *
 * You can reach the author by sending email to jplindfo@helsinki.fi.
 */


// standard imports
import java.util.Enumeration;


// non-standard class dependencies
import org.gjt.lindfors.util.EventGeneratorSupport;
import org.jboss.verifier.strategy.VerificationContext;


/**
 * << DESCRIBE THE CLASS HERE >>
 *
 * For more detailed documentation, refer to the
 * <a href="" << INSERT DOC LINK HERE >> </a>
 *
 * @see     << OTHER RELATED CLASSES >>
 *
 * @author 	Juha Lindfors
 * @version $Revision: 1.4 $
 * @since  	JDK 1.3
 */
public class VerificationEventGeneratorSupport extends EventGeneratorSupport {

    /*
     * Default constructor
     */
    public VerificationEventGeneratorSupport() {

        super();

    }
    
    public void addVerificationListener(VerificationListener listener) {

        super.addListener(listener);

    }
    
    public void removeVerificationListener(VerificationListener listener) {

        super.removeListener(listener);

    }    
    
    
    /*
     * Fires the event to all VerificationListeners. Listeners implements the
     * beanChecked method and can pull the information from the event object
     * and decide how to handle the situation by themselves
     */
    public void fireBeanChecked(VerificationEvent event) {    
            
        Enumeration e = super.getListeners();

        while (e.hasMoreElements()) {
            VerificationListener listener = (VerificationListener) e.nextElement();
            listener.beanChecked(event);
        }
    }
    
    public void fireSpecViolation(VerificationEvent event) {
        
        Enumeration e = super.getListeners();
        
        while (e.hasMoreElements()) {
            VerificationListener listener = (VerificationListener) e.nextElement();
            listener.specViolation(event);
        }
    }

}

