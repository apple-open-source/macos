# snack.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Snack library via the stubs table.
#	This file is used to generate the snackDecls.h file.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

library snack

# Define the snack interface with several sub interfaces:
#     snackPlat	 - platform specific public

interface snack
#hooks {snackPlat}

# Declare each of the functions in the public Snack interface. Note that
# an index should never be reused for a different function in order
# to preserve backwards compatibility.

#declare 0 generic {
#
#}
#declare 1 generic {
#
#}
declare 2 generic {
  int Snack_AddSubCmd(int snackCmd, char *cmdName, Snack_CmdProc *cmdProc, \
		      Snack_DelCmdProc *delCmdProc)
}
declare 3 generic {
  int Snack_AddFileFormat(char *formatName, guessFileTypeProc *guessProc, \
			  getHeaderProc *GetHeaderProc, \
			  extensionFileTypeProc *extProc, \
			  putHeaderProc *PutHeaderProc, \
			  openProc *OpenProc, closeProc *CloseProc, \
			  readSamplesProc *ReadSamplesProc, \
			  writeSamplesProc *WriteSamplesProc, \
			  seekProc *SeekProc)
}
declare 4 generic {
  int Snack_AddCallback(Sound *s, updateProc *proc, ClientData cd)
}
declare 5 generic {
  void Snack_RemoveCallback(Sound *s, int id)
}
declare 6 generic {
  void Snack_ExecCallbacks(Sound *s, int flag)
}
declare 7 generic {
  void Snack_UpdateExtremes(Sound *s, int start, int end, int flag)
}
declare 8 generic {
  Sound * Snack_GetSound(Tcl_Interp *interp, char *name)
}
declare 9 generic {
  Sound * Snack_NewSound(int frequency, int format, int nchannels)
}
declare 10 generic {
  int Snack_ResizeSoundStorage(Sound *s, int len)
}
declare 11 generic {
  void Snack_DeleteSound(Sound *s)
}
declare 12 generic {
  void Snack_PutSoundData(Sound *s, int pos, void *buf, int nBytes)
}
declare 13 generic {
  void Snack_GetSoundData(Sound *s, int pos, void *buf, int nBytes)
}
declare 14 generic {
  unsigned char Snack_Lin2Alaw(short pcm_val)
}
declare 15 generic {
  unsigned char Snack_Lin2Mulaw(short pcm_val)
}
declare 16 generic {
  short Snack_Alaw2Lin(unsigned char a_val)
}
declare 17 generic {
  short Snack_Mulaw2Lin(unsigned char u_val)
}
declare 18 generic {
  short Snack_SwapShort(short s)
}
declare 19 generic {
  int SnackSeekFile(seekProc *SeekProc, Sound *s, Tcl_Interp *interp, \
		    Tcl_Channel ch, int pos)
}
declare 20 generic {
  int SnackOpenFile(openProc *OpenProc, Sound *s, Tcl_Interp *interp, \
		    Tcl_Channel *ch, char *mode)
}
declare 21 generic {
  int SnackCloseFile(closeProc *CloseProc, Sound *s, Tcl_Interp *interp, \
		     Tcl_Channel *ch)
}
declare 22 generic {
  void Snack_WriteLog(char *s)
}
declare 23 generic {
  void Snack_WriteLogInt(char *s, int n)
}
declare 24 generic {
  Snack_FileFormat * Snack_GetFileFormats(void)
}
declare 25 generic {
  void Snack_InitWindow(float *hamwin, int winlen, int fftlen, int type)
}
declare 26 generic {
  int Snack_InitFFT(int n)
}
declare 27 generic {
  void Snack_DBPowerSpectrum(float *x)
}
declare 28 generic {
  void Snack_StopSound(Sound *s, Tcl_Interp *interp)
}
declare 29 generic {
  int Snack_ProgressCallback(Tcl_Obj *cmd, Tcl_Interp *interp, char *type, \
			     double fraction)
}
declare 30 generic {
  void Snack_CreateFileFormat(Snack_FileFormat *typePtr)
}
declare 31 generic {
  long Snack_SwapLong(long s)
}
declare 32 generic {
  int Snack_PlatformIsLittleEndian(void)
}
declare 33 generic {
  void Snack_CreateFilterType(Snack_FilterType *typePtr)
}
declare 34 generic {
 int SaveSound(Sound *s, Tcl_Interp *interp, char *filename,
	     Tcl_Obj *obj, int objc, Tcl_Obj *CONST objv[],
	     int startpos, int len, char *type)
}

# Define the platform specific public Tk interface.  These functions are
# only available on the designated platform.

#interface snackPlat

# Unix specific functions
#   (none)

# Windows specific functions
#   (none)
