package org.jboss.test.cts.interfaces;

import java.io.*;

/*
 *  A value holder class for holding non-security-related bean context
 *  information.
 */
public class BeanContextInfo
   implements Serializable
{
    public String remoteInterface;
    public String homeInterface;
    public Boolean isRollbackOnly;
}
