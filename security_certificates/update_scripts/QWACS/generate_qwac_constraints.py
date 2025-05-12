#!/usr/bin/env python3

import argparse
import requests
import base64
import warnings
import sys
import os
import plistlib
import json
import datetime
from lxml import etree
from signxml import XMLVerifier
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes
from jsonschema import validators, Draft7Validator, FormatChecker

EU_LOTL_URI = "https://ec.europa.eu/tools/lotl/eu-lotl.xml"

# download XML from a particular URL string
def downloadXML(url):
    response = requests.get(url)
    if response.status_code == 200:
        return etree.XML(response.content)
    else:
        raise response.raise_for_status()

# validate the xml element against the Trusted List schema
def validateXML(xml, srcroot):
    schemafile = srcroot + "/update_scripts/QWACS/ts_119612v020201_201601xsd.xsd"
    xsd_root = etree.parse(schemafile)
    schema = etree.XMLSchema(xsd_root)
    if not schema.validate(xml):
        raise ValueError("The trusted list xml does not match the schema")

# verify the XML signature with the certs
def verifyXML(xml, certs):
    for cert in certs:
        try:
            expect_refs = etree.tostring(xml).decode().count("<ds:Reference")
            results = XMLVerifier().verify(xml, x509_cert=cert, ignore_ambiguous_key_info=True, expect_references=expect_refs)
            return results[0].signed_xml
        except:
            continue
    raise RuntimeError("XML is not signed with one of the expected certs")

# validate the EU LOTL against the schema and verify version and type
def validateEULOTL(xml, srcroot):
    validateXML(xml, srcroot)
    schemeInfo = xml.find("{http://uri.etsi.org/02231/v2#}SchemeInformation")
    version = schemeInfo.find("{http://uri.etsi.org/02231/v2#}TSLVersionIdentifier")
    type = schemeInfo.find("{http://uri.etsi.org/02231/v2#}TSLType")
    if version.text != "5":
        raise ValueError("EU LOTL has wrong version: " + version.text)
    if type.text != "http://uri.etsi.org/TrstSvc/TrustedList/TSLType/EUlistofthelists":
        raise ValueError("EU LOTL has wrong TSL Type: " + type.text)

# determine whether the TSL pointer is for a TSL XML for a supported country
def isXmlTSL(tslPointer):
    addlInfo = tslPointer.find("{http://uri.etsi.org/02231/v2#}AdditionalInformation")
    otherInfos = addlInfo.findall("{http://uri.etsi.org/02231/v2#}OtherInformation")
    itemType = None
    itemTerritory = None
    itemMimeType = None
    for otherInfo in otherInfos:
        type = otherInfo.find("{http://uri.etsi.org/02231/v2#}TSLType")
        territory = otherInfo.find("{http://uri.etsi.org/02231/v2#}SchemeTerritory")
        mimeType = otherInfo.find("{http://uri.etsi.org/02231/v2/additionaltypes#}MimeType")
        if type is not None:
            itemType = type
        if territory is not None:
            itemTerritory = territory
        if mimeType is not None:
            itemMimeType = mimeType
    # Skip over TSLs in wrong format and the UK
    if itemType.text != "http://uri.etsi.org/TrstSvc/TrustedList/TSLType/EUgeneric":
        return False
    if itemTerritory.text == "UK":
        return False
    if itemMimeType.text != "application/vnd.etsi.tsl+xml":
        return False
    return True

# Extract certs from XML
def getCertsFromServiceIdentity(serviceIdentity):
    certs = []
    cert_elems = serviceIdentity.findall(".//{http://uri.etsi.org/02231/v2#}X509Certificate")
    for cert_elem in cert_elems:
        certData = base64.b64decode(cert_elem.text)
        cert = x509.load_der_x509_certificate(certData)
        certs.append(cert)
    return certs

# Get TLS signing certs from the TSL pointer
def getSigningCerts(tslPointer):
    serviceIdentities = tslPointer.find("{http://uri.etsi.org/02231/v2#}ServiceDigitalIdentities")
    return getCertsFromServiceIdentity(serviceIdentities)

# Determine if the service is "Qualified"
def isQualifiedService(serviceInfo):
    serviceType = serviceInfo.find("{http://uri.etsi.org/02231/v2#}ServiceTypeIdentifier")
    serviceStatus = serviceInfo.find("{http://uri.etsi.org/02231/v2#}ServiceStatus")
    if serviceType.text != "http://uri.etsi.org/TrstSvc/Svctype/CA/QC":
        return False
    if serviceStatus.text != "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/granted":
        return False
    return True

# Determine if the service provides Web Authentication Certificates
def isQWACService(serviceInfo):
    extensions = serviceInfo.find("{http://uri.etsi.org/02231/v2#}ServiceInformationExtensions")
    for extension in extensions:
        addlServiceInfo = extension.find("{http://uri.etsi.org/02231/v2#}AdditionalServiceInformation")
        if addlServiceInfo is None:
            continue
        extType = addlServiceInfo.find("{http://uri.etsi.org/02231/v2#}URI")
        if extType is not None and extType.text == "http://uri.etsi.org/TrstSvc/TrustedList/SvcInfoExt/ForWebSiteAuthentication":
            return True
    return False

# Get QWAC anchor certificate from the TSL proivder list
def getQWACAnchors(tsl, uri):
    certs = []
    providerList = tsl.find("{http://uri.etsi.org/02231/v2#}TrustServiceProviderList")
    for provider in providerList:
        services = provider.find("{http://uri.etsi.org/02231/v2#}TSPServices")
        for service in services:
            serviceInfo = service.find("{http://uri.etsi.org/02231/v2#}ServiceInformation")
            serviceName = serviceInfo.find("{http://uri.etsi.org/02231/v2#}ServiceName")
            if not isQualifiedService(serviceInfo):
                continue
            if not isQWACService(serviceInfo):
                continue
            try:
                serviceIdentity = serviceInfo.find("{http://uri.etsi.org/02231/v2#}ServiceDigitalIdentity")
                serviceCerts = getCertsFromServiceIdentity(serviceIdentity)
                certs += serviceCerts
            except Exception as inst:
                name = serviceName.find("{http://uri.etsi.org/02231/v2#}Name")
                warnings.warn("Did not find certificate for " + name.text + " in " + uri)
    return certs

def readPlist(filename):
    if not os.path.isfile(filename):
        raise ValueError("file \"" + filename + "\" does not exist")
    try:
        f = open(filename, mode='rb')
        plist = plistlib.load(f)
    except:
        raise ValueError("file \"" + filename + "\" is not valid plist")
    return plist

def readJson(filename):
    if not os.path.isfile(filename):
        raise("file \"" + filename + "\" does not exist")
    try:
        f = open(filename, mode='rb')
        object = json.load(f)
    except:
        raise ValueError("file \"" + filename + "\" is not valid JSON")
    return object

def readTrustStoreVersion(srcroot):
    version_file = srcroot + "/config/AssetVersion.plist"
    versionPlist = readPlist(version_file)
    return versionPlist["VersionNumber"]

def addEntryForQWACAnchor(certData):
    anchor = {}
    anchor["anchorCertificate"] = base64.b64encode(certData).decode("utf-8")
    operation = {}
    operation["anchorType"] = "custom"
    operation["constraints"] = ["1.2.840.113635.100.1.120"]
    anchor["operation"] = { "add" : operation }
    return anchor

def removeEntryForNonQWACAnchor(certData):
    anchor = {}
    anchor["anchorCertificate"] = base64.b64encode(certData).decode("utf-8")
    anchor["operation"] = {"remove":"remove"}
    return anchor

def validate_update_against_schema(filename, schema_filename):
    schema = readJson(schema_filename)
    update = readJson(filename)
    try:
        validator = Draft7Validator(schema, format_checker=FormatChecker())
        validator.validate(update)
        print("File \"" + filename + "\" conforms to schema",file=sys.stderr)
    except Exception as e:
        raise ValueError("File \"" + filename + "\" does not conform to schema: " + str(e))

def main():
    parser = argparse.ArgumentParser(description="Create a trust store update json for QWAC anchors based on the EU LOTL",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--srcroot', help="source root path", required=True)
    parser.add_argument('--output', help="the filename of the output update JSON", required=True)
    args = parser.parse_args()

    qwac_certs = []
    update = {}
    # Download, validate, and verify EU LOTL
    try:
        lotl = downloadXML(EU_LOTL_URI)
        validateEULOTL(lotl, args.srcroot)
        #verifyXML(lotl, None) # No externally provided certs are available that successfully verify the LOTL
    except:
        raise RuntimeError("Failed to download and validate the EU LOTL")

    # iterate through the TSL pointers
    schemeInfo = lotl.find("{http://uri.etsi.org/02231/v2#}SchemeInformation")
    tslPointers = schemeInfo.find("{http://uri.etsi.org/02231/v2#}PointersToOtherTSL")
    for item in tslPointers:
        # Get signers and URI
        if not isXmlTSL(item):
            continue
        certs = getSigningCerts(item)
        uri = item.find("{http://uri.etsi.org/02231/v2#}TSLLocation").text
        # Download
        try:
            tsl = downloadXML(uri)
        except:
            raise RuntimeError("Failed to download " + uri)
        # Validate against schema
        try:
            validateXML(tsl, args.srcroot)
        except Exception as inst:
            warnings.warn("Failed to validate " + uri + " " + str(inst)) # Some of the TSLs fail validation
        # Verify signature
        try:
            verifyXML(tsl, certs)
        except Exception as inst:
            warnings.warn("Failed to verify " + uri + " " + str(inst)) # Some of the TSLs fail verification
        # Get QWAC certs from TSL
        tsl_qwac_certs = getQWACAnchors(tsl, uri)
        qwac_certs += tsl_qwac_certs

    # Create the trust store update json
    update["oldVersion"] = readTrustStoreVersion(args.srcroot)
    now = datetime.datetime.now()
    update["newVersion"] = int(now.strftime("%Y%m%d00"))

    anchors = []

    # Read existing constraints to determine which need to be added
    constraints_file = args.srcroot + "/certificates/constraints.json"
    constraints = readJson(constraints_file)
    for cert in qwac_certs:
        fingerprint = cert.fingerprint(hashes.SHA256())
        certHashStr = fingerprint.hex().upper()
        if not certHashStr in constraints:
            certData = cert.public_bytes(serialization.Encoding.DER)
            anchors.append(addEntryForQWACAnchor(certData))
        else:
            # check if existing constraint is for QWAC and remove
            if "1.2.840.113635.100.1.120" in constraints[certHashStr]:
                del constraints[certHashStr]
            else:
                raise NotImplementedError("adding QWAC constraint to existing constrained entry not supported")

    # if there are remaining pre-existing QWAC constraints, we need to remove those certs
    if constraints:
        for hash in constraints:
            oids = constraints[hash]
            if "1.2.840.113635.100.1.120" in oids:
                cert_file = args.srcroot + "/certificates/custom/" + hash + ".cer"
                with open(cert_file,"r+b") as f:
                    certData = f.read()
                anchors.append(removeEntryForNonQWACAnchor(certData))

    update["anchors"] = anchors

    # write and check the output update json
    with open(args.output, "w") as f:
        json.dump(update, f, indent=4, separators=(',', ': '), sort_keys=True)
    schema_file = args.srcroot + "/update_scripts/root_store_updates_schema.json"
    validate_update_against_schema(args.output, schema_file)

if __name__ == "__main__":
    main()
