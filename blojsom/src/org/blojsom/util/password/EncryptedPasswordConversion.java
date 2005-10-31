/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.util.password;

import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Iterator;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Conversion utility to encrypt blojsom authorization properties files
 * <p></p>
 * <pre>
 * Usage: java -cp blojsom-core-2.24.jar org.blojsom.util.password.EncryptedPasswordConversion full-path-to-authorization.properties-file
 * </pre>
 *
 * @author David Czarnecki
 * @version $Id: EncryptedPasswordConversion.java,v 1.1.2.1 2005/07/21 14:11:04 johnan Exp $
 * @since blojsom 2.24
 */
public class EncryptedPasswordConversion {

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: EncryptedPasswordConversion <Full path to blojsom authorization.properties file> <Digest Algorithm - Defaults to MD5>");
        } else {
            File authorizationFile = new File(args[0]);
            if (!authorizationFile.exists()) {
                System.out.println(authorizationFile.toString() + " does not exist.");
            } else {
                try {
                    String digestAlgorithm = BlojsomConstants.DEFAULT_DIGEST_ALGORITHM;
                    if (args.length == 2) {
                        digestAlgorithm = args[1];
                        System.out.println("Requested digest algorithm: " + digestAlgorithm);
                        try {
                            MessageDigest messageDigest = MessageDigest.getInstance(digestAlgorithm);
                        } catch (NoSuchAlgorithmException e) {
                            digestAlgorithm = BlojsomConstants.DEFAULT_DIGEST_ALGORITHM;
                        }
                    }

                    System.out.println("Using digest algorithm: " + digestAlgorithm);
                    System.out.println("Encrypting authorization file: " + authorizationFile.toString());
                    BlojsomProperties authorization = new BlojsomProperties();
                    BlojsomProperties encryptedAuthorization = new BlojsomProperties();
                    FileInputStream fis = new FileInputStream(authorizationFile);
                    authorization.load(fis);
                    fis.close();

                    Iterator userIterator = authorization.keySet().iterator();
                    String passwordAndEmailProperty;
                    String[] passwordAndEmail;
                    String user;
                    while (userIterator.hasNext()) {
                        user = (String) userIterator.next();
                        passwordAndEmailProperty = authorization.getProperty(user);
                        passwordAndEmail = BlojsomUtils.parseLastComma(passwordAndEmailProperty);
                        if (passwordAndEmail.length == 1) {
                            encryptedAuthorization.setProperty(user, BlojsomUtils.digestString(passwordAndEmail[0], digestAlgorithm));
                        } else if (passwordAndEmail.length == 2) {
                            encryptedAuthorization.setProperty(user, BlojsomUtils.digestString(passwordAndEmail[0], digestAlgorithm) + "," + passwordAndEmail[1]);
                        }
                    }

                    FileOutputStream fos = new FileOutputStream(authorizationFile);
                    encryptedAuthorization.store(fos, null);
                    fos.close();

                    System.out.println("Converted authorization file to use encrypted passwords.");
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }
}