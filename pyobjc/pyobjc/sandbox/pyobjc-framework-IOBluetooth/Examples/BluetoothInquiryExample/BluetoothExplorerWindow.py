from IOBluetooth import *
from AppKit import *

from PyObjCTools import NibClassBuilder
import objc

NibClassBuilder.extractClasses("MainMenu")

gCellSize = NSSize(329, 70)
gCellImageSize = NSSize(48, 48)


class BluetoothExlorerWindow (NibClassBuilder.AutoBaseClass):
    _inquery        = objc.ivar()
    _busy           = objc.ivar(type=objc._C_BOOL)
    _foundDevices   = objc.ivar()

    def awakeFromNib(self):
        NSApp.setDelegate_(self)

        #  Setup the button matrix to have zero buttons since we cannot 
        # do this in IB.
        self._buttonMatrix.removeColumn_(0)
        self._buttonMatrix.renewRows_columns_(0, 1)
        self._buttonMatrix.sizeToCells()
        self._buttonMatrix.setCellSize_(gCellSize)
        self._buttonMatrix.setDoubleAction("deviceListenDoubleAction")

        self._matrixView.setNeedsDisplay_(True)

        if IOBluetoothValidateHardware(None) != kIOReturnSuccess:
            NSApp.terminate_(self)

    @objc.IBAction
    def clearFoundDevices_(self, sender):
        numberOfRows = self._buttonMatrix.numberOfRows()

        for _ in range(numberOfRows):
            self._buttonMatrix.removeRow_(0)

        self._foundDevices.removeAllObjects()
        self._foundDevices = None

    @objc.IBAction
    def handleSearchButton_(self, sender):
        if not self._busy:
            if not IOBluetoothLocalDeviceAvailable():
                return

            self._searchButton.setEnabled_(True)
            self.startInquiry()

        else:
            self._searchButton.setEnabled_(False)
            self.stopInquiry()

    def startInquiry(self):
        self.stopInquiry()

        self._inquiry = IOBluetoothDeviceInquiry.inquiryWithDelegate_(self)

        status = self._inquiry.start()
        if status == kIOReturnSuccess:
            self._processBar.startAnimation_(self)
            self._searchButton.setTitle_("Stop")
            self._busy = True

        else:
            self._messageText.setOBjectValue_("Idle (Search Failed).")

        return status

    def stopInquiry(self):
        if self._inquiry:
            ret = self._inquiry.stop()
            self._inquiry = None
            return ret

        else:
            return kIOReturnNotOpen


    def deviceInquiryDeviceFound_device_(self, sender, device):
        self.addDeviceToList_(device)
        self._messageText.setObjectValue_("Found %d devices..."%(sender.foundDevices().count()))

    def deviceInquiryUpdatingDeviceNamesStarted_devicesRemaining_(self, sender, devicesRemaining):
        self._messageText.setObjectValue_("Refreshing %d device names..."%(devicesRemaining,))

    def deviceInquiryDeviceNameUpdated_device_devicesRemaining_(self, sender, device, devicesRemaining):
        self._messageText.setObjectValue_("Refreshing %d device names..."%(devicesRemaining,))

        self.updateDeviceInfoInList_(device)

    def deviceInquiryComplete_error_aborted_(self, sender, error, aborted):
        if aborted:
            self._messageText.setObjectValue_("Idle (inquiry stopped).")

        else:
            self._messageText.setObjectValue_("Idle (inquiry complete).")

        self._progressBar.stopAnimation_(self)
        self._searchButton.setTitle_("Search")
        self._searchButton.setEnabled_(True)

        self._busy = False


    def deviceListDoubleAction(self):
        NSBeep()

    def addDeviceToList_(self, inDevice):
        addressPtr = inDevice.getAddress()

        deviceAddressString = None
        if addressPtr:
            deviceAddessString = "%02x-%02x-%02x-%02x-%02x-%02x"%(
                    addressPtr.data[0],
                    addressPtr.data[1],
                    addressPtr.data[2],
                    addressPtr.data[3],
                    addressPtr.data[4],
                    addressPtr.data[5])

        deviceNameString = inDevice.getName()
        if not deviceNameString:
            deviceNameString = "<Name not yet known>"

        if not self.saveNewDeviceIfAcceptable_(inDevice):
            # Already seen.
            return

        # Create info string
        deviceCODString = "Major class: %s (%#02x)\nMinor Class: %s (%#02x)"%(
            GetStringForMajorCOD(inDevice.getDeviceClassMajor()),
            inDevice.getDeviceClassMajor(),
            GetStringForMinorCOD(inDevice.getDeviceClassMajor(), inDevice.getDeviceClassMinor()),
            inDevice.getDeviceClassMinor())

        if inDevice.isConnected():
            connectionInfo = "Connected, ConnectionHandle = %#04x"%(
                    inDevice.getConnectionHandle())
        else:
            connectionInfo = "Not Connected"

        buttonIcon = NSImage.imageNamed_("BluetoothLogo.tiff")
        buttonIcon.setScalesWhenResized_(True)
        buttonIcon.setSzie_(gCellImageSize)

        self._buttonMatrix.addRow()
        
        newButton = self._buttonMatrix.cellAtRow_column_(
                self._buttonMatrix.numberOfRows()-1, 0)
        if not newButton:
            return

        newButton.setImage_(buttonIcon)
        newButton.setTitle_("%s / %s\n%s\n%s"%(
            deviceAddressString.upper(),
            deviceNameString,
            deviceCODString,
            connectionInfo))
        newButton.setTag_(id(inDevice))

        self._buttonMatrix.sizeToCell()
        self._matrixView.setNeedsDisplay_(True)

    def updateDeviceInfoInList_(self, inDevice):
        button = self._buttonMatrix.cellWithTag_(id(inDevice))
        if button:
            addressPtr = inDevice.getAddress()
            deviceAddressString = None
            name = inDevice.getName()
            connectionInfo = None

            deviceCODString = "Major Class %s (%#02x)\nMinor Class: %s (%#02x)"%(
                GetStringForMajorCOD(inDevice.getDeviceClassMajor()),
                inDevice.getDeviceClassMajor(),
                GetStringForMinorCOD(inDevice.getDeviceClassMajor(), inDevice.getDeviceClassMinor()),
                inDevice.getDeviceClassMinor())

            if inDevice.isConnected():
                connectionInfo = "Conneted, Connection Handle %#04x"%(
                        inDevice.getConnectionHandle())
            else:
                connectionInfo = "Not Connected"

            if addressPtr:
                deviceAddessString = "%02x-%02x-%02x-%02x-%02x-%02x"%(
                        addressPtr.data[0],
                        addressPtr.data[1],
                        addressPtr.data[2],
                        addressPtr.data[3],
                        addressPtr.data[4],
                        addressPtr.data[5])

            if not deviceAddressString:
                deviceAddressString = "Could not be retrieved"

            button.setTitle_("%s / %s\n%s\n%s"%(
                deviceAddressString.upper(), name, deviceCODString, connectionInfo))
        else:
            NSLog("Nop, tag could not be found in matrix")

    def saveNewDeviceIfAcceptable_(self, inNewDevice):
        if not self._foundDevices:
            self._foundDevices = []

        newDeviceAddress = inNewDevice.getAddress()
            
        for tmpDevice in self._foundDevices:
            tempAddress = tmpDevice.getAddress()
            if tempAddress == newDeviceAddress:
                # Already have it
                return False

        self._foundDevices.append(inNewDevice)
        return True

_MajorStrings={
        kBluetoothDeviceClassMajorMiscellaneous:    "Miscellaneous",
        kBluetoothDeviceClassMajorComputer:         "Computer",
        kBluetoothDeviceClassMajorPhone:            "Phone",
        kBluetoothDeviceClassMajorLANAccessPoint:   "LAN Access Point",
        kBluetoothDeviceClassMajorAudio:            "Audio",
        kBluetoothDeviceClassMajorPeripheral:       "Peripheral",
        kBluetoothDeviceClassMajorImaging:          "Imaging",
}

_MinorString={
        kBluetoothDeviceClassMajorComputer: {
            0: "Unclassified",
            1: "Desktop Workstation",
            2: "Server",
            3: "Laptop",
            4: "Handheld",
            5: "Palmsized",
            6: "Wearable",
        },
        kBluetoothDeviceClassMajorPhone: {
            0: "Unclassified",
            1: "Cellular",
            2: "Cordless",
            3: "SmartPhone",
            4: "Wired Modem or Voice Gateway",
            5: "Common ISDN Access",
        },
        kBluetoothDeviceClassMajorLANAccessPoint: {
            0: "0 used",
            1: "1-17 used",
            2: "18-33 used",
            3: "34-50 used",
            4: "51-67 used",
            5: "68-83 used",
            6: "84-99 used",
            7: "No Service",
        },
        kBluetoothDeviceClassMajorAudio: {
            0: "Unclassified",
            1: "Headset",
            2: "Hands Free",
            3: "Reserved 1",
            4: "Microphone",
            5: "Loudspeaker",
            6: "Headphones",
            7: "Portable",
            8: "Car",
            9: "Set-top Box",
            10: "HiFi",
            11: "VCR",
            12: "Video Camera",
            13: "CamCorder",
            14: "Video Monitor",
            15: "Video Display and Loudspeaker",
            16: "Conferencing",
            17: "Reserved2",
            18: "Gaming Toy",
        },
}


def GetStringForMajorCOD(inDeviceClassMajor):
    return _MajorStrings.get(inDeviceClassMajor, "Unclassified")

def GetStringForMinorCOD(inDeviceClassMajor, inDeviceClassMinor):
    return _MinorStrings.get(inDeviceClassMajor, {}
            ).get(inDeviceClassMinor, "Unclassified")
