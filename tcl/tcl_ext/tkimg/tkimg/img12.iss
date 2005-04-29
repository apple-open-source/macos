; setup for Img 1.2.4, using Martijn Laan's "My Inno Setup Extensions"
[Setup]
AppName=Img
AppVerName=Img 1.2.4
AppVersion=1.2.4
AppMutex=ImgDllMutex
AppPublisher=Jan Nijtmans
AppPublisherURL=http://purl.oclc.org/net/nijtmans/
AppSupportURL=http://purl.oclc.org/net/nijtmans/img.html
AppUpdatesURL=http://purl.oclc.org/net/nijtmans/img.html
AppCopyright=copyright © 1997-2000 Jan Nijtmans
CompressLevel=9
DefaultDirName={pf}\Tcl
DefaultGroupName=Tcl
DirExistsWarning=no
DisableProgramGroupPage=yes
DisableAppendDir=yes
EnableDirDoesntExistWarning=yes
MinVersion=4,3.51
OutputBaseFilename=img124
OutputDir=c:\jan
UninstallFilesDir={app}\lib\img1.2
UninstallDisplayName=Img 1.2
WizardImageFile=WizImg.bmp
WizardSmallImageFile=img.bmp

[Messages]
DiskSpaceMBLabel=The program requires at least [kb] KB of disk space.

[Types]
Name: default; Description: Default installation
Name: compact; Description: Compact installation
Name: full; Description: Full installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: main; Description: Img; Types: default compact full custom; Flags: fixed
Name: tcl80; Description: support for Tcl/Tk 8.0; Types: full
Name: doc; Description: documentation; Types: default full
Name: demo; Description: demos and tests; Types: default full
Name: msgs; Description: international messages; Types: default full
Name: jpeg; Description: jpeg support; Types: default full compact
Name: tiff; Description: tiff support; Types: default full compact
Name: png; Description: png support; Types: default full compact
Name: devm; Description: development for VC++ 5.0/6.0 (headers and libs); Types: full
Name: devc; Description: development for Mingw/Cygwin (headers and libs); Types: full

[Dirs]
Name: {app}\lib\img1.2\doc; Components: doc
Name: {app}\lib\img1.2\msgs; Components: msgs
Name: {app}\lib\img1.2\tests; Components: demo

[Files]
Source: img12.dll; DestDir: {app}\lib\img1.2; CopyMode: alwaysoverwrite; Components: main
Source: pkgIndex.tcl; DestDir: {app}\lib\img1.2; CopyMode: alwaysoverwrite; Components: main
Source: img1280.dll; DestDir: {app}\lib\img1.2; CopyMode: alwaysoverwrite; Components: tcl80
Source: doc\*.htm; DestDir: {app}\lib\img1.2\doc; CopyMode: alwaysoverwrite; Components: doc
Source: demo.tcl; DestDir: {app}\lib\img1.2; CopyMode: alwaysoverwrite; Components: demo
Source: tkv.tcl; DestDir: {app}\lib\img1.2; CopyMode: alwaysoverwrite; Components: demo
Source: tests\*; DestDir: {app}\lib\img1.2\tests; CopyMode: alwaysoverwrite; Components: demo
Source: msgs\*.msg; DestDir: {app}\lib\img1.2\msgs; CopyMode: alwaysoverwrite; Components: msgs
Source: zlib.dll; DestDir: {app}\bin; CopyMode: alwaysoverwrite; Components: png tiff
Source: jpeg62.dll; DestDir: {app}\bin; CopyMode: alwaysoverwrite; Components: jpeg
Source: tiff.dll; DestDir: {app}\bin; CopyMode: alwaysoverwrite; Components: tiff
Source: png.dll; DestDir: {app}\bin; CopyMode: alwaysoverwrite; Components: png
Source: libz\zlib.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libz\zconf.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libz\zlib.lib; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devm
Source: libz.a; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devc
Source: libpng\png.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libpng\pngconf.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libpng\png.lib; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devm
Source: libpng.a; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devc
Source: libjpeg\jpeglib.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libjpeg\jconfig.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libjpeg\jmorecfg.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libjpeg\jerror.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libjpeg\jpeg.lib; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devm
Source: libjpeg.a; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devc
Source: libtiff\tiff.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libtiff\tiffio.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libtiff\tiffconf.h; DestDir: {app}\include; CopyMode: alwaysoverwrite; Components: devc devm
Source: libtiff\tiff.lib; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devm
Source: libtiff.a; DestDir: {app}\lib; CopyMode: alwaysoverwrite; Components: devc

[Icons] 
Name: "{group}\Demos\Img demo"; Filename: "{app}\lib\img1.2\demo.tcl"; IconFilename: "{app}\lib\img1.2\img12.dll"; WorkingDir: "{app}\lib\img1.2"; Components: demo; MinVersion:4,4
