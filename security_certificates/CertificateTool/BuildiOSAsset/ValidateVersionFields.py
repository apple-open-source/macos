#!/usr/bin/python
#
# ValidateVersionFields.py
# Copyright 2020 Apple Inc. All rights reserved.
#
# Ensure that version fields across the bundles match

import sys
import os
import argparse
import re
import plistlib
import sqlite3
import json

def readPlist(filename):
    f = open(filename, mode='rb')
    plist = plistlib.load(f)
    return plist

def readXcconfigTrustStoreVersion(srcroot):
    f = open(srcroot + "/config/security_certificates.xcconfig")
    pattern = re.compile("^TRUST_STORE_VERSION = ([0-9]*)$")
    for line in f:
        match = pattern.search(line)
        if match:
            return int(match.group(1))
    return None

def checkTrustStoreVersion(srcroot):
    assetVersionPlist = readPlist(srcroot + "/config/AssetVersion.plist")
    infoAssetPlist = readPlist(srcroot + "/config/Info-Asset.plist")
    infoAssetProperties = infoAssetPlist["MobileAssetProperties"]
    if assetVersionPlist["VersionNumber"] != infoAssetProperties["ContentVersion"]:
        raise ValueError("Trust Store Version in config/AssetVersion.plist does not match version in config/Info-Asset.plist")
    if assetVersionPlist["VersionNumber"] != readXcconfigTrustStoreVersion(srcroot):
        raise ValueError("Trust Store Version in config/AssetVersion.plist does not match version in config/security_certificates.xconfig")

def readVersionFromAssetMakefile(fullpath):
    f = open(fullpath)
    pattern = re.compile("\$\(shell \/usr\/bin\/xcrun --find assettool\) stage -p \. -s staged -b \$\{BASE_URL\} -v ([0-9]*)")
    for line in f:
        match = pattern.search(line)
        if match:
            return int(match.group(1))
    return None

def readJson(filename):
    f = open(filename, mode='rb')
    object = json.load(f)
    return object

def checkPinningVersion(srcroot):
    pinningPlist = readPlist(srcroot + "/Pinning/CertificatePinning.plist")
    infoPlist = readPlist(srcroot + "/Pinning/Info.plist")
    infoProperties = infoPlist["MobileAssetProperties"]
    if pinningPlist[0] != infoProperties["_ContentVersion"]:
        raise ValueError("Pinning DB Version in Pinning/CertificatePinning.plist does not match version in Pinning/Info.plist")
    if pinningPlist[0] != readVersionFromAssetMakefile(srcroot + "/Pinning/Makefile"):
        raise ValueError("Pinning DB Version in Pinning/CertificatePinning.plist does not match version in Pinning/Makefile")

def checkValidVersion(srcroot):
    valideUpdatePlist = readPlist(srcroot + "/valid_db_snapshot/ValidUpdate.plist")
    conn = sqlite3.connect(srcroot + "/valid_db_snapshot/valid.sqlite3")
    cursor = conn.cursor()
    cursor.execute("SELECT ival FROM admin WHERE key='version'")
    if valideUpdatePlist["Version"] != int(cursor.fetchone()[0]):
        raise ValueError("Valid DB Version in valid_db_snapshot/ValidUpdate.plist does not match version in valid_db_snapshot/valid.sqlite3")

def checkSupplementalsAssetVersion(srcroot):
    assetVersionPlist = readPlist(srcroot + "/config/AssetVersion.plist")
    infoPlist = readPlist(srcroot + "/TrustSupplementalsAsset/Info.plist")
    infoProperties = infoPlist["MobileAssetProperties"]
    if assetVersionPlist["MobileAssetContentVersion"] != infoProperties["_ContentVersion"]:
        raise ValueError("Trust Supplementals Asset Version in config/AssetVersion.plist does not match version in TrustSupplementalsAsset/Info.plist")
    if assetVersionPlist["MobileAssetContentVersion"] != readVersionFromAssetMakefile(srcroot + "/TrustSupplementalsAsset/Makefile"):
        raise ValueError("Trust Supplementals Asset Version in config/AssetVersion.plist does not match version in TrustSupplementalsAsset/Makefile")
    log_list = readJson(srcroot + "/certificate_transparency/log_list.json")
    if assetVersionPlist["MobileAssetContentVersion"] != log_list["assetVersion"]:
        raise ValueError("Trust Supplementals Asset Version in config/AssetVersion.plist does not match version in certificate_transparency/log_list.json")


parser = argparse.ArgumentParser(description="Ensure that version fields across the bundles match")
parser.add_argument('-srcroot', help="The source root path", required=True)

args = parser.parse_args()
checkTrustStoreVersion(args.srcroot)
checkPinningVersion(args.srcroot)
checkValidVersion(args.srcroot)
checkSupplementalsAssetVersion(args.srcroot)
