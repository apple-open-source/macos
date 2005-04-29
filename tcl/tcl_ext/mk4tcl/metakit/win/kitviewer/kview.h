//---------------------------------------------------------------------------
#ifndef kviewH
#define kviewH
//---------------------------------------------------------------------------
#include <vcl\Classes.hpp>
#include <vcl\Controls.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\ComCtrls.hpp>
#include "Grids.hpp"
//---------------------------------------------------------------------------

#include <mk4.h>
#include <mk4io.h>
#include <mk4str.h>
#include <vcl\Dialogs.hpp>
#include <vcl\ExtCtrls.hpp>

class TMainForm : public TForm
{
__published:	// IDE-managed Components
    TOpenDialog *OpenDialog;
    TTreeView *StructTree;
    TSplitter *Splitter;
    TDrawGrid *DataGrid;

	void __fastcall StructTreeChange(TObject *Sender, TTreeNode *Node);
    void __fastcall FormActivate(TObject *Sender);
    void __fastcall DataGridDrawCell(TObject *Sender, int ACol, int ARow,
          TRect &Rect, TGridDrawState State);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
	
	
private:	// User declarations

    c4_FileStrategy _strategy;
	c4_Storage _storage;

    	// defines the current data view
    c4_View _path;		// used as a stack
    c4_View _data;		// the top view, currently visible
    int _colNum;		// >= 0 to show a single column, else -1

	void __fastcall SetupTree(TTreeNode* node_, const char*& desc_);
	void __fastcall SetupData();

public:		// User declarations
	__fastcall TMainForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
