/*
 * JORAM: Java(TM) Open Reliable Asynchronous Messaging
 * Copyright (C) 2002 INRIA
 * Contact: joram-team@objectweb.org
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 * 
 * Initial developer(s): Jeff Mesnil (jmesnil@inrialpes.fr)
 * Contributor(s): ______________________________________.
 */

package org.objectweb.jtests.jms.admin;

import javax.naming.*;
import java.util.Properties;

public class AdminFactory {

    private static final String PROP_NAME = "jms.provider.admin.class";
    private static final String PROP_FILE_NAME = "provider.properties";

    protected static String getAdminClassName() {
	String adminClassName;
	try {
	    Properties props = new Properties();
	    props.load(ClassLoader.getSystemResourceAsStream(PROP_FILE_NAME));
	    adminClassName =  props.getProperty(PROP_NAME);
	} catch (Exception e) {
	    //XXX
	    e.printStackTrace();
	    adminClassName = null;
	}
	return adminClassName;
    }
    
    public static Admin getAdmin() {
	String adminClassName = getAdminClassName();
	Admin admin = null;
	if (adminClassName == null) {
	    System.err.println ("Property "+ PROP_NAME +" has not been found in the file "+ PROP_FILE_NAME +".");
	    //XXX
	    System.exit(1);
	}
	try {
	    Class adminClass = Class.forName(adminClassName);
	    admin = (Admin)adminClass.newInstance();
	} catch (ClassNotFoundException e) {
	    //XXX
	    System.err.println("Class "+ adminClassName +" not found.");
	    System.exit(1);	
	} catch (Exception e) {
	    //XXX
	    e.printStackTrace();
	    System.exit(1);
	}
        
        // System.out.println("JMS Provider: "+ admin.getName());
        
	return admin;
    }
}
    
