@echo off
copy SOAP-Lite-COM-minimal.ctrl Lite.ctrl >nul
call perlctrl Lite.ctrl @make-com.args -dependent 