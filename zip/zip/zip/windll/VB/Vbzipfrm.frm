VERSION 5.00
Begin VB.Form Form1
   AutoRedraw      =   -1  'True
   Caption         =   "Form1"
   ClientHeight    =   3150
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   6570
   BeginProperty Font
      Name            =   "MS Sans Serif"
      Size            =   9.75
      Charset         =   0
      Weight          =   700
      Underline       =   0   'False
      Italic          =   0   'False
      Strikethrough   =   0   'False
   EndProperty
   LinkTopic       =   "Form1"
   ScaleHeight     =   3150
   ScaleWidth      =   6570
   StartUpPosition =   1  'CenterOwner
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

Option Explicit

'---------------------------------------------------------------
'-- Please Do Not Remove These Comments!!!
'---------------------------------------------------------------
'-- Sample VB 5 code to drive zip32.dll
'-- Contributed to the Info-ZIP project by Mike Le Voi
'--
'-- Contact me at: mlevoi@modemss.brisnet.org.au
'--
'-- Visit my home page at: http://modemss.brisnet.org.au/~mlevoi
'--
'-- Use this code at your own risk. Nothing implied or warranted
'-- to work on your machine :-)
'---------------------------------------------------------------
'--
'-- The Source Code Is Freely Available From Info-ZIP At:
'-- http://www.cdrom.com/pub/infozip/infozip.html
'--
'-- A Very Special Thanks To Mr. Mike Le Voi
'-- And Mr. Mike White Of The Info-ZIP project
'-- For Letting Me Use And Modify His Orginal
'-- Visual Basic 5.0 Code! Thank You Mike Le Voi.
'---------------------------------------------------------------
'--
'-- Contributed To The Info-ZIP Project By Raymond L. King
'-- Modified June 21, 1998
'-- By Raymond L. King
'-- Custom Software Designers
'--
'-- Contact Me At: king@ntplx.net
'-- ICQ 434355
'-- Or Visit Our Home Page At: http://www.ntplx.net/~king
'--
'---------------------------------------------------------------

Private Sub Form_Click()

  Dim retcode As Integer  ' For Return Code From ZIP32.DLL

  Cls

  '-- Set Options - Only The Common Ones Are Shown Here
  '-- These Must Be Set Before Calling The VBZip32 Function
  zDate = vbNullString
  zJunkDir = 0     ' 1 = Throw Away Path Names
  zRecurse = 0     ' 1 = Recurse -R 2 = Recurse -r 2 = Most Useful :)
  zUpdate = 0      ' 1 = Update Only If Newer
  zFreshen = 0     ' 1 = Freshen - Overwrite Only
  zLevel = Asc(9)  ' Compression Level (0 - 9)
  zEncrypt = 1     ' Encryption = 1 For Password Else 0
  zComment = 1     ' Comment = 1 if required

  '-- Select Some Files - Wildcards Are Supported
  '-- Change The Paths Here To Your Directory
  '-- And Files!!!
  zArgc = 2           ' Number Of Elements Of mynames Array
  zZipFileName = "c:\work\MyFirst.zip"
  zZipFileNames.zFiles(0) = "e:\wizbeta\about.c"
  zZipFileNames.zFiles(1) = "e:\wizbeta\action.c"
  zRootDir = "e:\"    ' This Affects The Stored Path Name

  '-- Go Zip Them Up!
  retcode = VBZip32

  '-- Display The Returned Code Or Error!
  Print "Return code:" & Str(retcode)

End Sub

Private Sub Form_Load()

  Me.Show

  Print "Click me!"

End Sub
