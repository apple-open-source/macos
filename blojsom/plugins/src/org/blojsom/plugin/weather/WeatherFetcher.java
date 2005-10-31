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
package org.blojsom.plugin.weather;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.plugin.weather.beans.WeatherInformation;
import org.blojsom.util.BlojsomConstants;
import org.w3c.dom.Document;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import java.io.*;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.zip.GZIPInputStream;


/**
 * WeatherFetcher
 *
 * @author Mark Lussier
 * @version $Id: WeatherFetcher.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 * @since Blojsom 2.23
 */
public class WeatherFetcher {

    private Log _logger = LogFactory.getLog(WeatherFetcher.class);

    private DocumentBuilder _documentBuilder;

    /**
     * Construct a new instance of the <code>WeatherFetcher</code> class
     */
    public WeatherFetcher() {
        DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
        documentBuilderFactory.setValidating(false);
        documentBuilderFactory.setIgnoringElementContentWhitespace(true);
        documentBuilderFactory.setIgnoringComments(true);
        documentBuilderFactory.setCoalescing(true);
        documentBuilderFactory.setNamespaceAware(false);

        try {
            _documentBuilder = documentBuilderFactory.newDocumentBuilder();
        } catch (ParserConfigurationException e) {
            _logger.error(e);
        }
    }

    /**
     * Retrieve the {@link WeatherInformation} from a site containing an XML feed of weather information
     *
     * @param provider {@link WeatherInformation}
     * @return {@link WeatherInformation} populated with data
     * @throws IllegalArgumentException If there is an error parsing weather information
     * @throws IOException              If there is an error parsing weather information
     */
    public WeatherInformation retrieveForecast(WeatherInformation provider) throws IllegalArgumentException, IOException {
        URL forecastUrl = new URL(provider.getProviderUrl());
        URLConnection connection = forecastUrl.openConnection();

        if (!(connection instanceof HttpURLConnection)) {
            throw new IllegalArgumentException(forecastUrl.toExternalForm() + " is not a valid HTTP Url");
        }

        HttpURLConnection httpConnection = (HttpURLConnection) connection;
        httpConnection.setRequestProperty("Accept-Encoding", WeatherConstants.GZIP);
        httpConnection.setRequestProperty("User-Agent", WeatherConstants.BLOJSOM_WEATHER_USER_AGENT);

        httpConnection.connect();
        InputStream is = connection.getInputStream();
        BufferedInputStream bis;

        if (WeatherConstants.GZIP.equalsIgnoreCase(httpConnection.getContentEncoding())) {
            bis = new BufferedInputStream(new GZIPInputStream(is));
        } else {
            bis = new BufferedInputStream(is);
        }

        StringBuffer buffer = new StringBuffer();
        BufferedReader br = new BufferedReader(new InputStreamReader(bis));

        String input = null;
        while ((input = br.readLine()) != null) {
            buffer.append(input).append(BlojsomConstants.LINE_SEPARATOR);
        }

        try {
            Document document = _documentBuilder.parse(new InputSource(new StringReader(buffer.toString())));
            provider.parseDocument(document);
        } catch (SAXException e) {
            _logger.error(e);
        }

        bis.close();

        return provider;
    }
}
