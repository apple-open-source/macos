/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
const char * gIOMacRISC2PMTree = "
  <array>

    <dict>				<!-- put internal USB together for power budgeting -->
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>IOPMUSBMacRISC2</string>
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>name</key>
            <string>usb</string>
            <key>AAPL,clock-id</key>
            <string>usb0u048</string>
          </dict>
          <key>children</key>
          <array>
            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb0u048</string>
              </dict>
              <key>multiple-parent</key>
              <integer>0</integer>		<!-- this OHCI will also have a USBKeyLargo parent -->
            </dict>
          </array>        
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>name</key>
            <string>usb</string>
            <key>AAPL,clock-id</key>
            <string>usb1u148</string>
          </dict>
          <key>children</key>
          <array>
            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb1u148</string>
              </dict>
              <key>multiple-parent</key>
              <integer>1</integer>		<!-- this OHCI will also have a USBKeyLargo parent -->
            </dict>
          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>name</key>
            <string>usb</string>
            <key>AAPL,clock-id</key>
            <string>usb2u248</string>
          </dict>
          <key>children</key>
          <array>
            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb2u248</string>
              </dict>
              <key>multiple-parent</key>
              <integer>2</integer>		<!-- this OHCI will also have a USBKeyLargo parent -->
            </dict>
          </array>
        </dict>

      </array>
    </dict>

    <dict>
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>KeyLargo</string>		<!-- internal KeyLargo -->
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>	<!-- a KeyLargo gets two USBKeyLargos - three for Intrepid -->
            <key>usb</key>
            <integer>0</integer>		<!-- USBKeyLargo zero gets -->
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>	<!-- the OHCI for USB controller zero -->
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb0u048</string>
              </dict>
              <key>multiple-parent</key>	<!-- this OHCI is also child of IOPMUSBMacRISC2 -->
              <integer>0</integer>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>1</integer>		<!-- USBKeyLargo one gets -->
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>	<!-- the OHCI for USB controller one -->
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb1u148</string>
              </dict>
              <key>multiple-parent</key>	<!-- this OHCI is also child of IOPMUSBMacRISC2 -->
              <integer>1</integer>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>2</integer>		<!-- USBKeyLargo two gets -->
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>	<!-- the OHCI for USB controller two -->
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb2u248</string>
              </dict>
              <key>multiple-parent</key>	<!-- this OHCI is also child of IOPMUSBMacRISC2 -->
              <integer>2</integer>
            </dict>

          </array>
        </dict>

      </array>
    </dict>

    <dict>
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>KeyLargo</string>		<!-- external KeyLargo -->
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>0</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb2u048</string>
              </dict>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>1</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb3u148</string>
              </dict>
            </dict>

          </array>
        </dict>

      </array>
    </dict>

    <dict>
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>KeyLargo</string>		<!-- external KeyLargo -->
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>0</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb4u048</string>
              </dict>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>1</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb5u148</string>
              </dict>
            </dict>

          </array>
        </dict>

      </array>
    </dict>

    <dict>
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>KeyLargo</string>		<!-- external KeyLargo -->
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>0</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb6u048</string>
              </dict>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>1</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb7u148</string>
              </dict>
            </dict>

          </array>
        </dict>

      </array>
    </dict>

    <dict>
      <key>device</key>
      <dict>
        <key>IOClass</key>
        <string>KeyLargo</string>		<!-- external KeyLargo -->
      </dict>
      <key>children</key>
      <array>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>0</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb8u048</string>
              </dict>
            </dict>

          </array>
        </dict>

        <dict>
          <key>device</key>
          <dict>
            <key>IOClass</key>
            <string>USBKeyLargo</string>
            <key>usb</key>
            <integer>1</integer>
          </dict>
          <key>children</key>
          <array>

            <dict>
              <key>device</key>
              <dict>
                <key>IOClass</key>
                <string>AppleUSBOHCI</string>
              </dict>
              <key>provider</key>
              <dict>
                <key>AAPL,clock-id</key>
                <string>usb9u148</string>
              </dict>
            </dict>

          </array>
        </dict>

      </array>
    </dict>
  
  </array>
";
