#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <Shlobj.h>
#include <wchar.h>
#include "DesktopIconManager.h"
#include "Resource.h"
#include "AnchorSystem.h"
#include "tinyxml2.h"

#pragma warning(disable:4996)
#define APP_NAME "DesktopRestore"
#define MAX_NAME_LEN 128
char config_file[MAX_PATH]={0};
CDesktopIconManager g_dim;
HINSTANCE ghinstance;
HWND hmain=0;

int get_desktop_name(char *tmp,int size)
{
	int result=FALSE;
	RECT rect={0};
	if(g_dim.GetDesktopSize(&rect)){
		int cnt=_snprintf_s(tmp,size,_TRUNCATE,"%i x %i",rect.bottom,rect.right);
		if(cnt>0)
			result=TRUE;
	}
	return result;
}

int get_appdata_folder(char *path,int size)
{
	int found=FALSE;
	ITEMIDLIST *pidl;
	IMalloc	*palloc;
	HWND hwindow=0;
	if(path==0 || size<MAX_PATH)
		return found;
	if(SHGetSpecialFolderLocation(hwindow,CSIDL_APPDATA,&pidl)==NOERROR){
		if(SHGetPathFromIDListA(pidl,path)){
			_snprintf_s(path,size,_TRUNCATE,"%s\\%s",path,APP_NAME);
			found=TRUE;
		}
		if(SHGetMalloc(&palloc)==NOERROR){
			palloc->Free(pidl);
			palloc->Release();
		}
	}
	return found;
}
int create_folder(char *path)
{
	int result=FALSE;
	DWORD attrib;
	if(0==path[0])
		return result;
	attrib=GetFileAttributesA(path);
	if(attrib==MAXDWORD){
		if(MAXDWORD!=CreateDirectoryA(path,NULL))
			result=TRUE;
	}
	if(attrib&FILE_ATTRIBUTE_DIRECTORY)
		result=TRUE;
	return result;
}
int get_config_fname(char *fname,int size)
{
	int result=FALSE;
	char tmp[MAX_PATH]={0};
	if(get_appdata_folder(tmp,sizeof(tmp))){
		_snprintf_s(fname,size,_TRUNCATE,"%s\\%s.xml",tmp,APP_NAME);
		result=TRUE;
	}
	return result;
}
int log_info(const char *fmt,...)
{
	int result=FALSE;
	HWND hedit;
	if(hmain==0)
		return result;
	hedit=GetDlgItem(hmain,IDC_INFO);
	if(hedit==0)
		return result;
	if(fmt==0){
		result=SetWindowTextA(hedit,"");
	}else{
		char tmp[256];
		va_list vlist;
		va_start(vlist,fmt);
		_vsnprintf(tmp,sizeof(tmp),fmt,vlist);
		va_end(vlist);
		tmp[sizeof(tmp)-1]=0;
		SendMessage(hedit,EM_SETSEL,-1,-1);
		result=(int)SendMessageA(hedit,EM_REPLACESEL,FALSE,(LPARAM)tmp);
	}
	return result;
}
int SaveIconPositions(char *desktop_name,char *fname)
{
	bool result=FALSE;
	int i,count;
	tinyxml2::XMLDocument doc;
	
    count=g_dim.GetNumIcons();
    for(i=0;i<count;i++){
		POINT pt;
		WCHAR name[MAX_NAME_LEN]={0};
		if(g_dim.GetIconPosition(i, &pt)){
			if(g_dim.GetIconText(i,name,_countof(name))){
				char tmp[MAX_NAME_LEN]={0};
				unsigned int count=0;
				result=TRUE;
				tinyxml2::XMLNode *n1,*n2;
				tinyxml2::XMLElement *xml;
				n1=doc.NewElement("ICON");
				n2=doc.NewElement("NAME");
				wcstombs_s(&count,tmp,sizeof(tmp),name,_TRUNCATE);
				xml=n2->ToElement();
				xml->SetText(tmp);
				n1->InsertEndChild(n2);
				n2=doc.NewElement("XPOS");
				xml=n2->ToElement();
				xml->SetText(pt.x);
				n1->InsertEndChild(n2);
				n2=doc.NewElement("YPOS");
				xml=n2->ToElement();
				xml->SetText(pt.y);
				n1->InsertEndChild(n2);
				doc.InsertEndChild(n1);
			}
		}
    }
	tinyxml2::XMLElement *xml;
	xml=doc.NewElement("DESKTOP");
	xml->SetText(desktop_name);
	doc.InsertFirstChild(xml);
	if(tinyxml2::XML_SUCCESS!=doc.SaveFile(fname))
		result=FALSE;
	return result;
}

int RestoreIconPosition(const WCHAR *pszName, int x, int y)
{
	int result=FALSE;
    int nIcons = g_dim.GetNumIcons();
    WCHAR szName[128];

    for (int i = 0; i < nIcons; i++) {
        if (g_dim.GetIconText(i, szName, _countof(szName)) == true) {
            if (lstrcmpi(szName, pszName) == 0) {
                POINT pt;
                pt.x = x;
                pt.y = y;
                result=g_dim.SetIconPosition(i, &pt);
                break;
            }
        }
    }
	return result;
}

int RestoreIconPositions(char *fname)
{
	int result=FALSE;
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement *xml;
	if(tinyxml2::XML_SUCCESS!=doc.LoadFile(fname))
		return result;
	log_info(0);
	xml=doc.FirstChildElement();
	result=TRUE;
	while(xml!=0){
		if(0==strcmp(xml->Value(),"ICON")){
			char name[MAX_NAME_LEN]={0};
			int x,y;
			int have[3]={0};
			tinyxml2::XMLElement *x1;
			x1=xml->FirstChildElement();
			while(x1!=0){
				if(0==strcmp(x1->Value(),"NAME")){
					strncpy(name,x1->GetText(),sizeof(name));
					name[sizeof(name)-1]=0;
					if(name[0]!=0)
						have[0]=1;
				}
				else if(0==strcmp(x1->Value(),"XPOS")){
					x=atoi(x1->GetText());
					have[1]=1;
				}
				else if(0==strcmp(x1->Value(),"YPOS")){
					y=atoi(x1->GetText());
					have[2]=1;
				}
				x1=x1->NextSiblingElement();
			}
			if(have[0] && have[1] && have[2]){
				WCHAR tmp[MAX_NAME_LEN]={0};
				mbstowcs(tmp,name,_countof(tmp));
				if(!RestoreIconPosition(tmp,x,y)){
					log_info("unable to restore:%s\r\n",name);
					result=FALSE;
				}
			}
		}
		xml=xml->NextSiblingElement();
	}
	g_dim.RefreshDesktop();
	return result;
}
int get_col_count(HWND hlview)
{
	int result=0;
	HWND header;
	header=ListView_GetHeader(hlview);
	if(header==NULL)
		return result;
	result=Header_GetItemCount(header);
	return result;
}
int set_columns(HWND hlview)
{
	int result=FALSE;
	int i,count;
	WCHAR *cols[]={
		L"Name",
		L"x pos",
		L"y pos",
		L"current x",
		L"current y"
	};
	count=get_col_count(hlview);
	if(count!=_countof(cols)){
		if(count>0){
			for(i=count-1;i>=0;i--)
				ListView_DeleteColumn(hlview,i);
		}
		result=TRUE;
		for(i=0;i<_countof(cols);i++){
			LV_COLUMN lvcol={0};
			lvcol.mask=LVCF_TEXT;
			lvcol.pszText=cols[i];
			if(0>ListView_InsertColumn(hlview,i,&lvcol))
				result=FALSE;
		}
	}
	else
		result=TRUE;
	return result;
}
int get_string_width(HWND hwnd,WCHAR *str)
{
	if(hwnd!=0 && str!=0){
		SIZE size={0};
		HDC hdc;
		hdc=GetDC(hwnd);
		if(hdc!=0){
			HFONT hfont;
			int len;
			len=(int)wcslen(str);
			hfont=(HFONT)SendMessage(hwnd,WM_GETFONT,0,0);
			if(hfont!=0){
				HGDIOBJ hold=0;
				hold=SelectObject(hdc,hfont);
				GetTextExtentPoint32W(hdc,str,len,&size);
				if(hold!=0)
					SelectObject(hdc,hold);
			}
			else{
				GetTextExtentPoint32W(hdc,str,len,&size);
			}
			ReleaseDC(hwnd,hdc);
			return size.cx;
		}
	}
	return 0;
}
int set_col_widths(HWND hlview)
{
	int i,count,hcount;
	int widths[12]={0};
	HWND header;
	header=ListView_GetHeader(hlview);
	hcount=Header_GetItemCount(header);
	if(hcount>_countof(widths))
		hcount=_countof(widths);
	for(i=0;i<hcount;i++){
		WCHAR tmp[40]={0};
		LV_COLUMN lvc={0};
		int w;
		lvc.pszText=tmp;
		lvc.cchTextMax=_countof(tmp);
		lvc.mask=LVCF_TEXT|LVCF_SUBITEM;
		lvc.iSubItem=i;
		ListView_GetColumn(hlview,i,&lvc);
		w=get_string_width(header,tmp);
		if(w>widths[i])
			widths[i]=w;
	}
	count=ListView_GetItemCount(hlview);
	for(i=0;i<count;i++){
		int w,j;
		for(j=0;j<hcount;j++){
			WCHAR tmp[MAX_NAME_LEN]={0};
			LV_ITEM lvitem={0};
			lvitem.pszText=tmp;
			lvitem.cchTextMax=_countof(tmp);
			lvitem.iItem=i;
			lvitem.iSubItem=j;
			lvitem.mask=LVIF_TEXT;
			ListView_GetItem(hlview,&lvitem);
			w=get_string_width(hlview,tmp);
			if(w>widths[j])
				widths[j]=w;
		}
	}
	for(i=0;i<hcount;i++){
		LV_COLUMN lvc={0};
		lvc.mask=LVCF_WIDTH|LVCF_SUBITEM;
		lvc.iSubItem=i;
		lvc.cx=widths[i]+17;
		ListView_SetColumn(hlview,i,&lvc);
	}
	return TRUE;
}

int populate_listview(HWND hlview,char *fname)
{
	int result=FALSE;
	ListView_DeleteAllItems(hlview);
	log_info(0);
	log_info("%s\r\n",fname);
	result=set_columns(hlview);
	if(result){
		int row=0;
		int pos_wrong=0;
		tinyxml2::XMLDocument doc;
		tinyxml2::XMLElement *xml;
		doc.LoadFile(fname);
		xml=doc.FirstChildElement();
		while(xml!=0){
			LV_ITEM lvitem={0};
			if(0==strcmp(xml->Name(),"DESKTOP")){
				log_info("Desktop size:%s\r\n",xml->GetText());
			}else if(0==strcmp(xml->Name(),"ICON")){
				int x,y,ox,oy;
				int have_current=FALSE;
				WCHAR name[MAX_NAME_LEN]={0};
				tinyxml2::XMLElement *x1;
				x1=xml->FirstChildElement();
				while(x1!=0){
					if(0==strcmp(x1->Name(),"NAME")){
						POINT pt={0};
						const char *n=x1->GetText();
						mbstowcs(name,n,_countof(name));
						name[_countof(name)-1]=0;
						if(g_dim.GetIconPosition(name,&pt)){
							ox=pt.x;
							oy=pt.y;
							have_current=TRUE;
						}
						else{
							log_info("Unable to find icon:%s\r\n",n);
						}
					}else if(0==strcmp(x1->Name(),"XPOS")){
						x=atoi(x1->GetText());
					}else if(0==strcmp(x1->Name(),"YPOS")){
						y=atoi(x1->GetText());
					}
					x1=x1->NextSiblingElement();
				}
				lvitem.iItem=row;
				lvitem.mask=LVIF_TEXT;
				lvitem.pszText=name;
				if(0<=ListView_InsertItem(hlview,&lvitem)){
					int i;
					int vals[]={x,y,ox,oy};
					for(i=0;i<_countof(vals);i++){
						WCHAR tmp[40]={0};
						_snwprintf(tmp,_countof(tmp),L"%i",vals[i]);
						if((!have_current) && i>=2)
							break;
						lvitem.iSubItem=i+1;
						lvitem.pszText=tmp;
						ListView_SetItem(hlview,&lvitem);
					}
					if(have_current){
						if((ox!=x) || (oy!=y)){
							lvitem.mask=LVIF_STATE;
							lvitem.iSubItem=0;
							lvitem.state=LVIS_SELECTED;
							lvitem.stateMask=LVIS_SELECTED;
							ListView_SetItem(hlview,&lvitem);
							pos_wrong++;
						}
					}
					row++;
				}
			}
			xml=xml->NextSiblingElement();
		}
		if(pos_wrong>0)
			log_info("icons in wrong position:%i\r\n",pos_wrong);
	}
	set_col_widths(hlview);
	return result;
}
struct CONTROL_ANCHOR ctrl_anchor[]={
	{IDC_LISTVIEW,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP|ANCHOR_BOTTOM,0,0,0},
	{IDC_SAVE,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
	{IDC_RESTORE,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
	{IDC_RELOAD,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
	{IDC_EXIT,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
	{IDC_GRIPPY,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
	{IDC_INFO,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0}
};

BOOL CALLBACK DialogProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			HWND hlview,hgrippy;
			hmain=hwnd;
			hlview=GetDlgItem(hwnd,IDC_LISTVIEW);
			ListView_SetExtendedListViewStyle(hlview,LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT);
			populate_listview(hlview,config_file);
			hgrippy=GetDlgItem(hwnd,IDC_GRIPPY);
			if(hgrippy!=0){
				DWORD style;
				style=WS_CHILD|WS_VISIBLE|SBS_SIZEGRIP;
				SetWindowLong(hgrippy,GWL_STYLE,style);
			}
			AnchorInit(hwnd,ctrl_anchor,_countof(ctrl_anchor));
		}
		break;
	case WM_SIZE:
		AnchorResize(hwnd,ctrl_anchor,_countof(ctrl_anchor));
		break;
	case WM_HELP:
		{
			static int help=FALSE;
			if(!help){
				help=TRUE;
				MessageBoxA(hwnd,config_file,"CONFIG FILE",MB_OK|MB_SYSTEMMODAL);
				help=FALSE;
			}
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wparam)){
		case IDC_SAVE:
			{
				char tmp[80]={0};
				if(get_desktop_name(tmp,sizeof(tmp))){
					if(SaveIconPositions(tmp,config_file)){
						HWND hlview=GetDlgItem(hwnd,IDC_LISTVIEW);
						populate_listview(hlview,config_file);
					}
				}
			}
			break;
		case IDC_RESTORE:
			RestoreIconPositions(config_file);
			break;
		case IDC_RELOAD:
			{
				HWND hlview;
				hlview=GetDlgItem(hwnd,IDC_LISTVIEW);
				populate_listview(hlview,config_file);
			}
			break;
		case IDC_EXIT:
		case IDCANCEL:
			EndDialog(hwnd,0);
			break;
		}
		break;
	}
	return 0;
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ghinstance = hInstance;
	char tmp[MAX_PATH]={0};
	get_appdata_folder(tmp,sizeof(tmp));
	if(tmp[0]==0)
		MessageBoxA(NULL,"unable to access app data folder","ERROR",MB_OK|MB_SYSTEMMODAL);
	else{
		if(create_folder(tmp))
			get_config_fname(config_file,sizeof(config_file));
		else
			MessageBoxA(NULL,"unable to create app data folder","ERROR",MB_OK|MB_SYSTEMMODAL);
	}
	DialogBox(hInstance,MAKEINTRESOURCE(IDD_DIALOG),NULL,DialogProc);
    return 0;
}
