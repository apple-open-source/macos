import os, sys, platform

ver = platform.mac_ver()
ret = os.system('sed -e "s/XXX1XXX/%s/" -e "s/XXX2XXX/%s/" -e "s/XXX3XXX/%s/" %s | ed - %s' % (ver[0], ver[1], ver[2], sys.argv[1], sys.argv[2]))
sys.exit(ret)
