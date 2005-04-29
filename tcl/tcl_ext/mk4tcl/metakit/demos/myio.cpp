//  This code demonstrates:
//
//  - A class derived from c4_Strategy to implement encrypted storage.
//  - Disabling the Flush calls issued during Commit() for speed.
//  - Using c4_Strategy objects as the basis of all file I/O in Metakit.

#include "mk4.h"
#include "mk4io.h"

#include <string.h>

/////////////////////////////////////////////////////////////////////////////
// This derived strategy encrypts its data on disk and omits flushes

class CEncryptStrategy : public c4_FileStrategy
{
public:
  CEncryptStrategy (const char* fileName_, bool rw_);
  virtual ~CEncryptStrategy ();

  // Reading and writing of course messes around with all the data
  virtual int  DataRead(long, void*, int);
  virtual void DataWrite(long, const void*, int);

  // For this example, we also disable all explicit file flushes
  virtual void DataCommit(long) { }

  // Cannot use memory mapped file access when decoding on the fly
  virtual void ResetFileMapping() { }
  
private:
  // This example uses a trivial encoding, incorporating offsets.
  static char Encode(long pos, char c_)
    { return (char) (c_ ^ pos ^ 211); }
  static char Decode(long pos, char c_)
    { return (char) (c_ ^ pos ^ 211); }
};

CEncryptStrategy::CEncryptStrategy (const char* fileName_, bool rw_)
{
  c4_FileStrategy::DataOpen(fileName_, rw_);
}

CEncryptStrategy::~CEncryptStrategy ()
{
}

int CEncryptStrategy::DataRead(long lOff, void* lpBuf, int nCount)
{
  int result = 0;

  if (nCount > 0)
  {
    char* ptr = (char*) lpBuf;
  
    result = c4_FileStrategy::DataRead(lOff, ptr, nCount);
  
    for (int i = 0; i < result; ++i)
      ptr[i] = Decode(lOff++, ptr[i]);
  }

  return result;
}

void CEncryptStrategy::DataWrite(long lOff, const void* lpBuf, int nCount)
{
  if (nCount > 0)
  {
    c4_Bytes buf;
    char* ptr = (char*) buf.SetBuffer(nCount);
  
    memcpy(ptr, lpBuf, nCount);
  
    for (int i = 0; i < nCount; ++i)
      ptr[i] = Encode(lOff++, ptr[i]);
  
    c4_FileStrategy::DataWrite(lOff - nCount, ptr, nCount);
  }
}

/////////////////////////////////////////////////////////////////////////////

int main()
{
  // This property could just as well have been declared globally.
  c4_StringProp pLine ("line");

  {
      // This is where the magic takes place.
    CEncryptStrategy efile ("secret.dat", true);
    
    c4_Storage storage (efile);
  
    static const char* message[] = {
      "This is a small message which will be encrypted on file.",
      "As a result, none of the other Metakit utilities can read it.",
      "Furthermore, a hex dump of this file will produce gibberish.",
      "The encryption used here is ridiculously simple, however.",
      "Beware of naive encryption schemes, cracking them is a sport.",
      0
    };
  
      // Store the text lines as separate entries in the view.
    c4_View vText;
  
    for (const char** p = message; *p; ++p)
      vText.Add(pLine [*p]);
  
      // changed 2000-03-15: Store is gone
    //storage.Store("text", vText);
    c4_View v2 = storage.GetAs("text[line:S]");
    v2.InsertAt(0, vText);

    storage.Commit();
  }

  // The end of the preceding block will flush out all data to file.
  {
      // Repeat the process when accessing the encrypted file again.
    CEncryptStrategy efile ("secret.dat", false);
  
    c4_Storage storage (efile);
    c4_View vText = storage.View("text");
  
    for (int i = 0; i < vText.GetSize(); ++i)
    {
      const char* s = pLine (vText[i]);
      puts(s);
    }
  }

  // At this point, an encrypted data file is left behind on the disk.

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
