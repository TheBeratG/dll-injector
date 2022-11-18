#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <tchar.h>
#include "resource.h"

HWND procList; //Handle to our process list box on the GUI
HWND pathLbl;  //Handle to the display ot the path of our DLL

//Window actions:
constexpr size_t UPDATE_PROC_LIST = (WM_APP + 100);
constexpr size_t OPEN_FILE_BTN = (WM_APP + 101);
constexpr size_t INJECT_BTN = (WM_APP + 102);

TCHAR procName[MAX_MODULE_NAME32] = TEXT("sauerbraten.exe"); //module name 
TCHAR dllPath[MAX_PATH] = TEXT("C:\\hack.dll"); //selected .dll

//function to get the proc id of our game:
inline DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);

        if (Process32First(hSnap, &procEntry)) {
            do {
                if (!wcscmp(procEntry.szExeFile, procName)){
                    procId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

//open a dialog to select the dll we want to inject:
inline void openDll() {
    OPENFILENAME openDlg{}; //create the dialog structure
    openDlg.lStructSize = sizeof(OPENFILENAME);
    openDlg.lpstrFile = dllPath; //where to save the path of the file we have selected
    openDlg.nMaxFile = MAX_PATH;
    openDlg.lpstrFilter = TEXT("DLL-File (Internal Cheat)\0*.dll\0All Files\0*.*");
    openDlg.lpstrTitle = TEXT("Open Internal Hack to inject");
    openDlg.Flags = OFN_FILEMUSTEXIST | OFN_FORCESHOWHIDDEN;
    
    if (GetOpenFileName(&openDlg)) { //open the dialog
        SetWindowText(pathLbl, dllPath); //set the text on the display if we selected a new file
    }
}

//function to inject our dll into the game:
inline void inject() {
    //get the count of processes in our list box
    size_t count = SendMessage(procList, LB_GETCOUNT, 0, 0);
   
    //find the selected process
    for (size_t i = 0; i < count; i++){ //loop over all processes on our list
        if (SendMessage(procList, LB_GETSEL, i, 0) > 0) { //if the curent one is selected
            SendMessage(procList, LB_GETTEXT, i, (LPARAM)procName); //save the neame of the process in procName
            break; //we have found our process and can exot out of the loop now
        }
    }

    DWORD procId{ GetProcId(procName) }; //get the procId of the selected process
    if (!procId) return; //if we didnt find it we just quit
   
    //Open a handle with all access to our game
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, procId);
    
    if (hProc && hProc != INVALID_HANDLE_VALUE) { //if we have a valid handle
        //Allocate memory in the heap of our games process to fit the path to our dll in
        void* loc = VirtualAllocEx(hProc, 0, (_tcslen(dllPath)+1)*sizeof(TCHAR), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (loc != 0) { //if we have successfully allocated memory
            //write the path to our dll to the allocated memory
            WriteProcessMemory(hProc, loc, dllPath, (_tcslen(dllPath)+1)*sizeof(TCHAR), nullptr);
 
            //Start a new thread in our games process that loads our dll
            HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary), loc, NULL, NULL);
           
            if (hThread) { //if we have successfully created a thread
                CloseHandle(hThread); //close the handle to this thread
            }
        }
    }

    if (hProc) { //if a handle to the exists
        CloseHandle(hProc); //Close the handle to our game
    }
}

//function to update the list of processes:
inline int refreshProcList() {
    SendMessage(procList, LB_RESETCONTENT, 0, 0); //clear out list box

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //create a snapshot of processes

    if (hSnap != INVALID_HANDLE_VALUE) { //if we have created a valid snapshot
        PROCESSENTRY32 procEntry; 
        procEntry.dwSize = sizeof(procEntry);

        if (Process32First(hSnap, &procEntry)) { //save the first entry of our snapshot in procEntry
            do {
                //add procEntry to our listBox
                SendMessage(procList, LB_ADDSTRING, 0, (LPARAM)procEntry.szExeFile);
            } while (Process32Next(hSnap, &procEntry)); //save the next entry into proxEntry 
            //this will continue until there is no more next Entry
        }
    }
    CloseHandle(hSnap); //delete our snapshot
    return 0; //everything went fine 
}

//Message Handler that handels all messages that get send from our GUI:
LRESULT CALLBACK MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0); //Close the GUI
		break;
	case WM_COMMAND: //whenever something (regular) happens with the GUI
        switch (LOWORD(wParam)) {
        case UPDATE_PROC_LIST: //refresh button was pressed
            refreshProcList();
            break;

        case INJECT_BTN: //inject button was pressed
            inject();
            break;

        case OPEN_FILE_BTN: //open button was pressed
            openDll();
            break;
        }
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int main(){
    ShowWindow(GetConsoleWindow(), SW_HIDE); //Hide the console window 

    HINSTANCE hInstance = GetModuleHandle(0); //get a handle to this current module
    WNDCLASSEX wc{}; //window struct
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MessageHandler;
    wc.hInstance = hInstance;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = TEXT("DLL_Injector");
    
    //register our window struct
    RegisterClassEx(&wc);

    //Create the main window for our dll injector
    HWND hWnd{ CreateWindowEx(0, TEXT("DLL_Injector"), TEXT("Simple DLL Injector"), WS_VISIBLE | WS_EX_LAYERED | WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, 500, 700, 0, 0, hInstance, 0) };
    
    //Create the process list box
    procList = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("listbox"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOHSCROLL, 10, 10, 465, 500, hWnd, NULL, 0, 0);

    //Create the update Button
    HWND updateBtn{ CreateWindowEx(0, TEXT("button"), TEXT("Update Process List"), WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_FLAT, 10, 520, 465, 30, hWnd, (HMENU)UPDATE_PROC_LIST, hInstance, 0) };
    
    //Create the open Button
    HWND openBtn{ CreateWindowEx(0, TEXT("button"), TEXT("Open"), WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_FLAT, 10, 560, 100, 30, hWnd, (HMENU)OPEN_FILE_BTN, hInstance, 0) };
    
    //Create the path display
    pathLbl = CreateWindowEx(0, TEXT("static"), dllPath, WS_TABSTOP | WS_CHILD | WS_VISIBLE, 120, 560, 355, 30, hWnd, NULL, hInstance, 0);
    HWND injectBtn{ CreateWindowEx(0, TEXT("button"), TEXT("INJECT"), WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_FLAT, 10, 600, 465, 50, hWnd, (HMENU)INJECT_BTN, hInstance, 0) };
    
    //custom Icon:
    HICON hicon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

    //get list of currently running processes
    refreshProcList();

    MSG msg; //hold the message we sent from our GUI
    DWORD dwExit{ STILL_ACTIVE };

    //as long as our window is still open
    while (dwExit == STILL_ACTIVE) {
        BOOL result = GetMessage(&msg, 0, 0, 0); //try to get a new message from the GUI
        if (result > 0) { //if we where able to get a new message
            TranslateMessage(&msg);
            DispatchMessage(&msg); //send to our message handler
        }
        else { //else return the error code
            return result;
        }
    }
   
    return 0;
}