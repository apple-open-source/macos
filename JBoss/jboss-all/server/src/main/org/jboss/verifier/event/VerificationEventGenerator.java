package org.jboss.verifier.event;

/*
 * Class org.jboss.verifier.event.VerificationEventGenerator
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
 * $Id: VerificationEventGenerator.java,v 1.3 2001/01/03 08:28:43 tobias Exp $
 *
 * You can reach the author by sending email to jplindfo@helsinki.fi.
 */



// non-standard class dependencies
import org.gjt.lindfors.util.EventGenerator;


/**
 * << DESCRIBE THE CLASS HERE >>
 *
 * For more detailed documentation, refer to the
 * <a href="" << INSERT DOC LINK HERE >> </a>
 *
 * @see     << OTHER RELATED CLASSES >>
 *
 * @author 	Juha Lindfors
 * @version $Revision: 1.3 $
 * @since  	JDK 1.3
 */
public interface VerificationEventGenerator extends EventGenerator {

    abstract void addVerificationListener(VerificationListener listener);
    abstract void removeVerificationListener(VerificationListener listener);
    
    abstract void fireBeanChecked(VerificationEvent event);
    abstract void fireSpecViolation(VerificationEvent event);
}
    

