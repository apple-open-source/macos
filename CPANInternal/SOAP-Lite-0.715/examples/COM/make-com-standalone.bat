@echo off
copy SOAP-Lite-COM-standalone.ctrl Lite.ctrl >nul
call perlctrl Lite.ctrl @make-com.args -freestanding 