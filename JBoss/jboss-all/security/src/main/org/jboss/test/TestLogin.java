package org.jboss.test;

import java.security.*;
import javax.security.auth.*;

public class TestLogin
{
    public static void main(String[] args) throws Exception
    {
        System.setProperty("java.security.policy", "policy");
        System.out.println("java.security.manager = "+System.getProperty("java.security.manager"));
        Permission p = new AuthPermission("getLoginConfiguration");
        AccessController.checkPermission(p);
    }
}
