/*
 * Copyright (c) 2003,  Intracom S.A. - www.intracom.com
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
**/
package org.jboss.jmx.adaptor.snmp.agent;

/**
 * <tt>DynamicContentAccessor</tt> or DCA for short allows the 
 * introduction of dynamic content in the notification wrappers. The later
 * contain static values keyed on a tag. The problem solved from DCA instances
 * regards the access of volatile content. In other words when a DCA 
 * implementation is accessed, its get method will provide access to whatever 
 * content it is designed to.
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>    
**/    
public interface DynamicContentAccessor
{
   /**
    * Override to provide access to whatever 
   **/ 
   public Object get();

} // interface DynamicContentAccessor
