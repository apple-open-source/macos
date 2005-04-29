//---------------------------------------------------------------------------
#include <vcl\vcl.h>
#pragma hdrstop

#include "kview.h"
#include <stdlib.h>
//---------------------------------------------------------------------------
#pragma link "Grids"
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
	: TForm(Owner), _colNum (-1)
{
	if (ParamCount() >= 1)
    	Caption = ParamStr(1);
    else if (OpenDialog->Execute())
        Caption = OpenDialog->FileName;
    else
    	return;

    if (_strategy.DataOpen(Caption.c_str(), false))
        _storage = c4_Storage (_strategy, true);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormActivate(TObject *Sender)
{
    c4_View root = _storage;
    TTreeNode* top = StructTree->TopItem;

    _path.SetSize(0);

    const char* desc = _storage.Description();
    SetupTree(top, desc);

    StructTree->FullExpand();
    StructTree->Selected = StructTree->Items->Item[0];

    SetupData();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SetupTree(TTreeNode* node_, const char*& desc_)
{
    // new logic as of MK 2.3.2 beta: parse dscription directly
    for (;;)
    {
        desc_ += strspn(desc_, " ,");
        if (*desc_ == 0 || *desc_ == ']')
            break;
        const char* e = desc_ + strcspn(desc_, "[,]");
        char buf [100];
        strncpy(buf, desc_, e - desc_ + 1);
        buf[strcspn(desc_, " :[,]")] = 0;
        TTreeNode* node = StructTree->Items->AddChild(node_, buf);
        desc_ = e;
        if (*desc_ == '[')
        {
            SetupTree(node, ++desc_);
            if (*desc_ == ']')
                ++desc_;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SetupData()
{
    c4_View saveView = _data;
    DataGrid->Invalidate();

	_data = c4_View ();

    c4_String title = Caption.c_str();
    int w = title.Find(" - ");
    if (w > 0)
    	Caption = (const char*) title.Left(w);

	int n = _path.GetSize();
	if (n == 0)
    {
	    DataGrid->ColCount = 1;
    	DataGrid->RowCount = 1;
    	return;
    }

    _data = _storage;

    c4_IntProp pItem ("item");
    c4_StringProp pName ("name");

    for (c4_Cursor curr = &_path[0]; curr < &_path[n]; ++curr)
    {
        int item = pItem (*curr);
        c4_String name (pName (*curr));

        if (item >= _data.GetSize())
           	_data = c4_View ();

        _colNum = -1; // assume we'll be looking at an entire view

        for (int i = 0; i < _data.NumProperties(); ++i)
        {
        	c4_Property prop (_data.NthProperty(i));
            if (prop.Name() == name)
            {
    			if (prop.Type() == 'V')
        			_data = ((c4_ViewProp&) prop) (_data[item]);
                else
                	_colNum = i; // wrong assumption, just one column
            	break;
            }
        }

        char buf [10];
        if (curr == &_path[0])
        	strcpy(buf, " - ");
        else if (_colNum >= 0)
        	strcpy(buf, ".");
        else
        	wsprintf(buf, "[%d].", item);

   		Caption = Caption + buf;
        Caption = Caption + (const char*) name;
    }

    DataGrid->ColCount = _colNum >= 0 ? 2 : _data.NumProperties() + 1;
    if (DataGrid->ColCount > 1)
    	DataGrid->FixedCols = 1;

    DataGrid->ColWidths[0] = 40;
    if (_colNum >= 0)
    	DataGrid->ColWidths[1] = 250;

    if (&saveView[0] != &_data[0])
    {
        DataGrid->RowCount = 1;	// force a reset to top
	    DataGrid->RowCount = _data.GetSize() + 1;
	    if (DataGrid->RowCount > 1)
	    	DataGrid->FixedRows = 1;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::StructTreeChange(TObject *Sender, TTreeNode *Node)
{
	int oLevel = _path.GetSize() - 1;

    	// if it was a single column display, treat it as one level less
    if (_colNum >= 0)
    	--oLevel;

    int nLevel = Node->Level;
    _path.SetSize(nLevel + 1);

    c4_IntProp pItem ("item");
    c4_StringProp pName ("name");

    for (TTreeNode* p = Node; p; p = p->Parent)
    {
    	int k = p->Level;
    	c4_RowRef row = _path[k];

        c4_String old (pName (row));
        pName (row) = p->Text.c_str();

        if (pName (row) != old)
			nLevel = k;
    }

    for (int i = nLevel; i < _path.GetSize(); ++i)
    	pItem (_path[i]) = 0;

    if (nLevel > oLevel && DataGrid->Row > 0)
    	pItem (_path[oLevel+1]) = DataGrid->Row - 1;

    SetupData();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::DataGridDrawCell(TObject *Sender, int Col,
      int Row, TRect &Rect, TGridDrawState State)
{
	char buf [100];
	const char* s = buf;

    if (Col > 0)
    {
    	int c = _colNum >= 0 ? _colNum : Col - 1;
        if (c >= _data.NumProperties())
        	return; // empty storage

        c4_Property prop (_data.NthProperty(c));
        char type = prop.Type();

    	if (Row > 0)
        {
        	c4_RowRef r = _data[Row-1];

        	switch (type)
            {
        case 'I':	wsprintf(buf, "%ld", (long) ((c4_IntProp&) prop)(r));
                    break;
        case 'F':	gcvt(((c4_FloatProp&) prop)(r), 6, buf);
                    break;
        case 'D':	gcvt(((c4_DoubleProp&) prop)(r), 12, buf);
                    break;
        case 'S':	s = ((c4_StringProp&) prop)(r);
                    //{ static c4_String t; t = s; s = t; }
                    //wsprintf(buf, "%ld", strlen(s)); s = buf;
          			break;
        case 'B':
        case 'M':	wsprintf(buf, "(%db)", prop(r).GetSize());
                    break;
        case 'V':	wsprintf(buf, "[#%d]",
              				((c4_View) ((c4_ViewProp&) prop)(r)).GetSize());
                    break;
            }
        }
        else
        {
            strcpy(buf, (c4_String) prop.Name() + ":" + (c4_String) type);
        }
    }
    else if (Row > 0)
    {
        wsprintf(buf, "%ld", Row - 1);

        	// "poor man's right alignment"
        int n = strlen(s);
        if (n < 5) // tricky, because two spaces is the width of one digit
            strcpy(buf, c4_String (' ', 10 - 2 * n) + s);
    }
    else
    {
        s = "";
    }

    AnsiString as (s);
    if (as.Length() > 1000) // TextRect seems to crash with huge strings
        as.SetLength(1000);
	DataGrid->Canvas->TextRect(Rect, Rect.Left + 3, Rect.Top, as);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    exit(0);
}
//---------------------------------------------------------------------------

