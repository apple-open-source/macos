/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Constants.java,v 1.2 2002/02/28 08:01:27 cgjung Exp $

package org.jboss.net;

/**
 * Some Constants for the axis package
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 28. September 2001
 * @version $Revision: 1.2 $
 */

public interface Constants {

    static final String INIT_METHOD_NAME="init";
    static final String CREATE_METHOD_NAME="create";
    static final String START_METHOD_NAME="start";
    static final String STOP_METHOD_NAME="stop";
    static final String DESTROY_METHOD_NAME="stop";
    static final String STRING_CLASS_NAME="java.lang.String";
    static final String DEPLOYMENT_INFO_CLASS_NAME="org.jboss.deployment.DeploymentInfo";

}