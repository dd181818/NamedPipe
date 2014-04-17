// NamePipeClientDemo.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "..\NamePipeServerDemo\Core\NamedPipeClient.h"
#include "..\NamePipeServerDemo\Core\IIPCInterface.h"
#include <conio.h>
#include <locale.h>

BOOL g_bExit = FALSE;

class CNamedPipeEvent : public IIPCEvent
{
public:
    CNamedPipeEvent()
    {

    }

    virtual ~CNamedPipeEvent()
    {

    }

    virtual void OnConnect(IIPCObject* pServer, IIPCConnector* pClient)
    {
//         TCHAR* sSendMsg = _T("�ͻ������ӵ���������\r\n");
//         pClient->PostMessage(sSendMsg, _tcslen(sSendMsg)*sizeof(TCHAR));
    }

    virtual void OnDisConnect(IIPCObject* pServer, IIPCConnector* pClient)
    {

    }

    virtual void OnCreate(IIPCObject* pServer)
    {

    }

    virtual void OnClose(IIPCObject* pServer)
    {

    }

    virtual void OnRecv(IIPCObject* pServer, IIPCConnector* pClient, LPCVOID lpBuf, DWORD dwBufSize)
    {
        _tsetlocale(LC_ALL, _T("chs"));
        _tprintf_s(_T("%s"), lpBuf);

//        TCHAR* sReply = _T("Hello,Server\r\n");

		static int x=0;
		TCHAR aBuf[MAX_PATH]={0};
		_stprintf_s(aBuf,_T("�ͻ��� %d �������� %d \r\n"),GetCurrentProcessId(),x++);
        if (!pClient->SendMessage(aBuf, _tcslen(aBuf)*sizeof(TCHAR)))
			return ;
    }

    virtual void OnSend(IIPCObject* pServer, IIPCConnector* pClient, LPVOID lpBuf, DWORD dwBufSize)
    {

    }
};

DWORD __stdcall SendThread(LPVOID lpParam)
{
	IIPCObject* pServer = (IIPCObject*)lpParam;

	IIPCConnectorIterator* pClientIterator = pServer->GetClients();

	while(FALSE == g_bExit)
	{
		for(pClientIterator->Begin(); !pClientIterator->End(); pClientIterator->Next())
		{
			IIPCConnector* aClient = pClientIterator->GetCurrent();

			if(NULL == aClient)
				continue;

			TCHAR* sReply = _T("Hello,Server\r\n");
			aClient->SendMessage(sReply, _tcslen(sReply)*sizeof(TCHAR));
		}

//		Sleep(10);
//		g_bExit=TRUE;
	}

	return 0;
}

DWORD __stdcall PostThread(LPVOID lpParam)
{
    IIPCObject* pServer = (IIPCObject*)lpParam;

    IIPCConnectorIterator* pClientIterator = pServer->GetClients();

    while(FALSE == g_bExit)
    {
        for(pClientIterator->Begin(); !pClientIterator->End(); pClientIterator->Next())
        {
            IIPCConnector* aClient = pClientIterator->GetCurrent();

            if(NULL == aClient)
                continue;

            TCHAR* sReply = _T("Hello,Server\r\n");
            aClient->PostMessage(sReply, _tcslen(sReply)*sizeof(TCHAR));
        }

        Sleep(10);
		g_bExit=TRUE;
    }

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    IIPCEvent* pEvent = new CNamedPipeEvent;

    IIPCObject* pNamedPipeClient = new CNamedPipeClient(pEvent);

    if(NULL == pNamedPipeClient)
        return -1;

    if(!pNamedPipeClient->Create(_T("NamedPipeServer")))
        return -1;

    HANDLE hThread = CreateThread(NULL, 0, SendThread, pNamedPipeClient, 0, NULL);

    _getch();

    g_bExit = TRUE;
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    pNamedPipeClient->Close();
    delete pNamedPipeClient;
    pNamedPipeClient = NULL;

    delete pEvent;
    pEvent = NULL;
    return 0;
}
