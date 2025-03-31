import logging
import argparse


def cli_main():
    logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(prog="recertifi_cli",
                                     description="Appends Apple Root CA's to the Certifi CA store.")
    parser.add_argument("--patch", action="store_true", help="run the patch method and modify the Certifi CA store",
                        required=True)
    parser.add_argument("--path", help="path to the CA store to be patched (defaults to certifi.where())", default=None)

    args = parser.parse_args()

    if args.patch:
        import recertifi.implementation as r
        r.patch_certifi_ca(args.path)


# extra name so that recertifi_cli still works
if __name__ in ["__main__", "recertifi.__main__"]:
    cli_main()
