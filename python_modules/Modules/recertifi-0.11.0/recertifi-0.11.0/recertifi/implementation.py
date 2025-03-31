import hashlib
import logging
import os
import shutil
import ssl
import tempfile

import recertifi.version as version

# From https://certificatemanager.apple.com/#help/caCertificates
APPLE_CA_CERTS = [
    {
        "filename": "apple_corporate_root_ca.pem",
        "fingerprint":
            "50:41:69:C1:76:A2:C3:0D:A2:E9:0E:A9:8A:53:5D:78:EF:42:F3:1A:90:FA:48:B6:CE:C2:45:A4:72:12:7A:D3",
    },
    {
        "filename": "apple_corporate_root_ca2.pem",
        "fingerprint":
            "5E:A8:ED:04:2C:B8:8D:2A:40:A5:AE:E9:CB:C8:F1:C0:D1:93:95:4B:8C:47:1D:3A:05:A0:FA:04:9C:07:88:CB",
    },
    {
        "filename": "apple_corporate_authentication_ca_1.pem",
        "fingerprint":
            "87:D6:67:8C:B8:1B:AC:9E:87:02:32:F7:C7:31:F9:E5:4F:ED:DE:32:23:3C:DE:76:DC:BE:08:BA:2A:36:9D:77",
    },
    {
        "filename": "apple_corporate_authentication_ca_2.pem",
        "fingerprint":
            "86:E1:F6:E1:4E:65:06:FC:48:A8:EC:DD:C1:1F:35:DC:60:62:DA:46:E0:9F:1A:5D:97:D2:BA:06:94:88:B7:51",
    },
    {
        "filename": "apple_corporate_external_authentication_ca_1.pem",
        "fingerprint":
            "B7:87:B8:90:AB:22:FC:43:4B:13:9A:FA:E8:30:6E:81:84:84:4B:AB:7B:03:31:05:CF:BF:F5:84:61:FF:E7:C3",
    },
    {
        "filename": "apple_corporate_external_authentication_ca_2.pem",
        "fingerprint":
            "14:EB:FB:3E:A7:19:FD:5A:5B:42:E3:C1:5C:89:DE:C7:30:13:D8:3B:26:9E:33:20:AC:51:0F:A9:0D:55:92:5F",
    },
    {
        "filename": "apple_corporate_server_ca_1.pem",
        "fingerprint":
            "94:B7:E1:89:55:48:80:E4:C3:42:C9:20:28:53:DB:C7:05:32:9B:31:F7:40:03:80:EB:38:AD:BE:29:0C:00:B7",
    },
    {
        "filename": "apple_corporate_server_ca_2.pem",
        "fingerprint":
            "60:95:13:DB:43:75:11:E4:8D:94:27:1F:4A:FC:CE:9E:DE:DA:61:3D:F8:E1:B7:BE:4C:AA:D0:E4:C7:2D:30:3E",
    },
    {
        "filename": "apple_ist_ca_2.pem",
        "fingerprint":
            "AC:2B:92:2E:CF:D5:E0:17:11:77:2F:EA:8E:D3:72:DE:9D:1E:22:45:FC:E3:F5:7A:9C:DB:EC:77:29:6A:42:4B",
    },
    {
        "filename": "apple_ist_ca_8.pem",
        "fingerprint":
            "A4:FE:7C:7F:15:15:5F:3F:0A:EF:7A:AA:83:CF:6E:06:DE:B9:7C:A3:F9:09:DF:92:0A:C1:49:08:82:D4:88:ED"
    }
]

RECERTIFI_PATH = os.path.join(os.path.dirname(__file__), "certs")

logger = logging.getLogger("recertifi")
logger.addHandler(logging.NullHandler())


def get_cert_fingerprint(cert_file_string):
    der = ssl.PEM_cert_to_DER_cert(cert_file_string.decode())
    sha256 = hashlib.sha256()
    sha256.update(der)
    return sha256.hexdigest()


def fingerprint_match(cert_file_string, fingerprint):
    # Lowercase, strip :'s to get bytes only
    return fingerprint.lower().replace(":", "") == get_cert_fingerprint(cert_file_string)


def replace_certifi_store(where, new_contents):
    # Create a temp file in same dir as certifi store
    temp_file, temp_filename = tempfile.mkstemp(dir=os.path.dirname(where))
    # Should have same permissions as original cert store
    shutil.copystat(where, temp_filename)

    # Write to temp file
    with os.fdopen(temp_file, "wb") as temp:
        temp.write(new_contents)
        logger.debug("Wrote new CA store to temp file")

    # Replace certifi store with temp file
    try:
        os.rename(temp_filename, where)
    except WindowsError:
        logger.debug("WindowsError detected: cannot rename on top of an existing file. "
                     "Removing first, then renaming again.")
        os.remove(where)
        os.rename(temp_filename, where)
    logger.debug("Replaced CA store from temp file")


def check_file_writable(filename):
    if os.path.exists(filename):
        # path exists
        if os.path.isfile(filename):  # is it a file or a dir?
            # also works when file is a link and the target is writable
            return os.access(filename, os.W_OK)
        else:
            return False  # path is a dir, so cannot write as a file
    # target does not exist, check perms on parent dir
    parent_dir = os.path.dirname(filename)
    if not parent_dir:
        parent_dir = '.'
    # target is creatable if parent dir is writable
    return os.access(parent_dir, os.W_OK)


def patch_certifi_ca(path=None):
    # wrapper method to patch CA.
    try:
        logger.debug("reCertifi {}".format(version.__version__))
        logger.debug("Adding Apple Corporate Root CA's")

        if path is not None:
            certifi_store_path = path
        else:
            import certifi
            certifi_store_path = certifi.where()

        if not os.access(certifi_store_path, os.R_OK):
            logger.error("No read permissions to check if CA at {} needs patching. See https://at.apple.com/recertifi "
                         "for more info.".format(certifi_store_path))
            return

        if not check_file_writable(certifi_store_path):
            logger.error("No write permissions to patch CA at {}. See https://at.apple.com/recertifi for more "
                         "info."
                         .format(certifi_store_path))
            return

        with open(certifi_store_path, "rb") as certifi_store_file:
            logger.debug("Reading Certifi CA store {}".format(certifi_store_path))
            certifi_store_contents = certifi_store_file.read()

        changed = False
        for index, cert_definition in enumerate(APPLE_CA_CERTS):
            with open(os.path.join(RECERTIFI_PATH, cert_definition["filename"]), "rb") as cert_file:
                cert_file_string = cert_file.read()

            valid = fingerprint_match(cert_file_string, cert_definition["fingerprint"])

            if valid:
                if cert_file_string not in certifi_store_contents:
                    logger.debug("Adding: {} (not present in Certifi CA store)".format(cert_definition["filename"]))
                    certifi_store_contents += cert_file_string
                    certifi_store_contents += b'\n'  # newline needed to separate certs
                    changed = True
                else:
                    logger.debug("Skipping: {} (already present in Certifi CA store)".format(
                        cert_definition["filename"]))
            else:
                logger.debug("Skipping: {} (fingerprint did not match)".format(cert_definition["filename"]))

        if changed:
            try:
                replace_certifi_store(certifi_store_path, certifi_store_contents)
            except OSError as e:
                logger.error("OSError while overwriting Certifi CA store. Patching did not succeed. {}".format(e))

    except ImportError:
        # Cleanly exit, if certifi isnt installed, it isnt needed.
        logger.debug("certifi import failed, patching not needed.")
        pass


if __name__ == "recertifi.implementation":
    patch_certifi_ca()
