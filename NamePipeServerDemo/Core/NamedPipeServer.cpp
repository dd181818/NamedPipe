#include "stdafx.h"
#include "NamedPipeServer.h"
#include <assert.h>
#include <stddef.h>
#include <tchar.h>


CNamedPipeServer::CNamedPipeServer(IIPCEvent* pEvent): m_pEventHandler(pEvent)
    , m_hCompletionPort(NULL)
{
    m_sNamedPipe[MAX_PATH - 1] = _T('\0');
    InitializeCriticalSection(&m_csReadIng);
}


CNamedPipeServer::~CNamedPipeServer(void)
{
    if(NULL != m_hCompletionPort)
    {
        CloseHandle(m_hCompletionPort);
        m_hCompletionPort = NULL;
    }

    DeleteCriticalSection(&m_csReadIng);
}

BOOL CNamedPipeServer::Create(LPCTSTR lpPipeName)
{
    assert(NULL == m_hCompletionPort);
    assert(NULL != lpPipeName);

    _tcscpy_s(m_sNamedPipe, MAX_PATH, _T("\\\\.\\pipe\\"));
    _tcscat_s(m_sNamedPipe, lpPipeName);

    m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

    if(NULL == m_hCompletionPort)
        return FALSE;

    if(!_CreateIOCPThreadPool(0))
        return FALSE;

    if(NULL == WaitPipeConnection())
        return FALSE;

    return TRUE;
}

void CNamedPipeServer::Close()
{
    if(NULL != m_hCompletionPort)
    {
        CloseHandle(m_hCompletionPort);
        m_hCompletionPort = NULL;
    }
}

BOOL CNamedPipeServer::_CreateIOCPThreadPool(DWORD dwThreadNum)
{
    DWORD dwThread;
    HANDLE hThread;
    DWORD i;
    SYSTEM_INFO SystemInfo;

    GetSystemInfo(&SystemInfo);

    DWORD aThreadNum = (0 == dwThreadNum ? SystemInfo.dwNumberOfProcessors : dwThreadNum);

    for(i = 0; i < 2 * aThreadNum; i++)
    {
        hThread = CreateThread(NULL, 0, _IOCPThreadProc, this, 0, &dwThread);

        if(NULL == hThread)
            return FALSE;

        CloseHandle(hThread);
    }

    return TRUE;
}

DWORD __stdcall CNamedPipeServer::_IOCPThreadProc(LPVOID lpParam)
{
    CNamedPipeServer* pThis = (CNamedPipeServer*)lpParam;

    if(NULL == pThis)
        return -1;

    return pThis->_IOCPThread();
}

DWORD CNamedPipeServer::_IOCPThread()
{
    PCLIENT pClient;
    BOOL b;
    LPOVERLAPPED lpo;
    DWORD nBytes;

    TCHAR sRecvBuf[SYELOG_MAXIMUM_MESSAGE] = {0};

    for(BOOL fKeepLooping = TRUE; fKeepLooping;)
    {
        pClient = NULL;
        lpo = NULL;
        nBytes = 0;
        b = GetQueuedCompletionStatus(m_hCompletionPort, &nBytes, (PULONG_PTR)&pClient, &lpo, INFINITE);

        if(!b && lpo == NULL)
        {
            fKeepLooping = FALSE;
            break;
        }
        else if(!b)
        {
            CloseConnection(pClient);
            pClient = NULL;
            continue;
        }

        if(pClient->emPipeStatus == NAMED_PIPE_CONNECT)
        {
            CreateConnection(pClient);
            pClient->emPipeStatus = NAMED_PIPE_READING;
            b = ReadFile(pClient->hPipe, sRecvBuf, SYELOG_MAXIMUM_MESSAGE, &nBytes, pClient);

            if(!b)
            {
                if(GetLastError() != ERROR_IO_PENDING)
                    continue;
            }

            WaitPipeConnection();
        }
        else
        {
//             if(nBytes < offsetof(SYELOG_MESSAGE, szMessage))
//             {
//                 CloseConnection(pClient);
//                 continue;
//             }

            if(pClient->emPipeStatus == NAMED_PIPE_READING)
            {
                IIPCConnector* pConnector = FindClient(pClient->hPipe);
                OnRecv(this, pConnector, sRecvBuf, nBytes);

                b = ReadFile(pClient->hPipe, sRecvBuf, SYELOG_MAXIMUM_MESSAGE, &nBytes, pClient);

                if(!b && GetLastError() == ERROR_BROKEN_PIPE)
                    CloseConnection(pClient);
            }
        }
    }

    return 0;
}

BOOL CNamedPipeServer::WaitPipeConnection()
{
    PCLIENT pClient = NULL;
    BOOL bSuccess = FALSE;

    do
    {
        HANDLE hPipe = CreateNamedPipe(m_sNamedPipe, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, NULL);

        if(hPipe == INVALID_HANDLE_VALUE)
            break;

        PCLIENT pClient = new CLIENT;

        if(pClient == NULL)
            break;

        ZeroMemory(pClient, sizeof(CLIENT));
        pClient->hPipe = hPipe;
        pClient->emPipeStatus = NAMED_PIPE_CONNECT;

        if(NULL == CreateIoCompletionPort(hPipe, m_hCompletionPort, (ULONG_PTR)pClient, 0))
            break;

        if(!ConnectNamedPipe(hPipe, pClient))
        {
            if(GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_LISTENING)
                break;
        }

        bSuccess = TRUE;
    }
    while(FALSE);

    if(!bSuccess)
    {
        if(NULL != pClient)
        {
            delete pClient;
            pClient = NULL;
        }
    }

    return bSuccess;
}

BOOL CNamedPipeServer::CloseConnection(PCLIENT pClient)
{
    EnterCriticalSection(&m_csReadIng);

    IIPCConnector* pConnector = FindClient(pClient->hPipe);

    if(NULL != pConnector)
    {
        if(pClient != NULL)
        {
            if(pClient->hPipe != INVALID_HANDLE_VALUE)
            {
                FlushFileBuffers(pClient->hPipe);
                DisconnectNamedPipe(pClient->hPipe);

                pClient->emPipeStatus = NAMED_PIPE_DISCONNECT;
                OnDisConnect(this, pConnector);
                RemoveClient(pClient->hPipe);
                CloseHandle(pClient->hPipe);
                pClient->hPipe = NULL;
                pClient->hPipe = INVALID_HANDLE_VALUE;
            }

            delete pClient;
            pClient = NULL;
        }
    }

    LeaveCriticalSection(&m_csReadIng);
    return TRUE;
}

void CNamedPipeServer::AddClient(HANDLE hPort, IIPCConnector* pClient)
{
    EnterCriticalSection(&m_csReadIng);
    m_connectorMap.insert(std::make_pair(hPort, pClient));
    LeaveCriticalSection(&m_csReadIng);
}

void CNamedPipeServer::RemoveClient(HANDLE hPort)
{
    EnterCriticalSection(&m_csReadIng);

    IIPCConnector* pConnectorFind = FindClient(hPort);

    if(NULL != pConnectorFind)
    {
        m_connectorMap.erase(hPort);
        delete pConnectorFind;
        pConnectorFind = NULL;
    }

    LeaveCriticalSection(&m_csReadIng);
}

IIPCConnector* CNamedPipeServer::FindClient(HANDLE hPort)
{
    EnterCriticalSection(&m_csReadIng);

    IIPCConnector* pConnectorFind = NULL;
    ConnectorMap::const_iterator cit = m_connectorMap.begin();

    for(cit; cit != m_connectorMap.end(); cit++)
    {
        pConnectorFind = cit->second;

        if(NULL == pConnectorFind)
            continue;

        if(pConnectorFind->GetHandle() == hPort)
            break;
    }

    LeaveCriticalSection(&m_csReadIng);
    return pConnectorFind;
}

void CNamedPipeServer::OnConnect(IIPCObject* pServer, IIPCConnector* pClient)
{
    if(NULL != m_pEventHandler)
        m_pEventHandler->OnConnect(pServer, pClient);
}

void CNamedPipeServer::OnDisConnect(IIPCObject* pServer, IIPCConnector* pClient)
{
    if(NULL != m_pEventHandler)
        m_pEventHandler->OnDisConnect(pServer, pClient);
}

void CNamedPipeServer::OnCreate(IIPCObject* pServer)
{
    if(NULL != m_pEventHandler)
        m_pEventHandler->OnCreate(pServer);
}

void CNamedPipeServer::OnClose(IIPCObject* pServer)
{
    if(NULL != m_pEventHandler)
        m_pEventHandler->OnClose(pServer);
}

void CNamedPipeServer::OnRecv(IIPCObject* pServer, IIPCConnector* pClient, LPCVOID lpBuf, DWORD dwBufSize)
{
    if(0 == dwBufSize || NULL == lpBuf)
        return ;

    if(NULL != m_pEventHandler)
        m_pEventHandler->OnRecv(pServer, pClient, lpBuf, dwBufSize);
}

void CNamedPipeServer::OnSend(IIPCObject* pServer, IIPCConnector* pClient, LPVOID lpBuf, DWORD dwBufSize)
{
    if(NULL != m_pEventHandler)
        m_pEventHandler->OnSend(pServer, pClient, lpBuf, dwBufSize);
}

void CNamedPipeServer::Begin()
{
    EnterCriticalSection(&m_csReadIng);
    m_citCurrent = m_connectorMap.begin();
    LeaveCriticalSection(&m_csReadIng);
}

BOOL CNamedPipeServer::End()
{
    BOOL bEnd = TRUE;
    EnterCriticalSection(&m_csReadIng);
    bEnd = (m_citCurrent == m_connectorMap.end());
    LeaveCriticalSection(&m_csReadIng);
    return bEnd;
}

void CNamedPipeServer::Next()
{
    EnterCriticalSection(&m_csReadIng);

    if(!End())
    {
        m_citCurrent++;
    }

    LeaveCriticalSection(&m_csReadIng);
}

IIPCConnector* CNamedPipeServer::GetCurrent()
{
    IIPCConnector* pConnector = NULL;
    EnterCriticalSection(&m_csReadIng);

    if(!End())
        pConnector = m_citCurrent->second;

    LeaveCriticalSection(&m_csReadIng);
    return pConnector;
}

IIPCConnectorIterator* CNamedPipeServer::GetClients()
{
    return this;
}

void CNamedPipeServer::CreateConnection(PCLIENT pClient)
{
    IIPCConnector* pConnector = new CNamedPipeConnector(pClient->hPipe, this, m_pEventHandler);
    AddClient(pClient->hPipe, pConnector);
    OnConnect(this, pConnector);
}

BOOL CNamedPipeConnector::SendMessage(LPCVOID lpBuf, DWORD dwBufSize)
{
    if(NULL == m_hPipe)
        return FALSE;

    if(NULL != m_pEventSensor)
        m_pEventSensor->OnSend(m_pServer, this, (LPVOID)lpBuf, dwBufSize);

    DWORD dwWrited = 0;
    BOOL bSucess =::WriteFile(m_hPipe, lpBuf, dwBufSize, &dwWrited, NULL);

    return bSucess;
}

BOOL CNamedPipeConnector::PostMessage(LPCVOID lpBuf, DWORD dwBufSize)
{
    if(NULL == m_hPipe)
        return FALSE;

    DWORD dwWrited = 0;
    BOOL bSucess =::WriteFile(m_hPipe, lpBuf, dwBufSize, &dwWrited, &m_ovPostMessage);

    return (bSucess || GetLastError() == ERROR_IO_PENDING);
}

CNamedPipeConnector::CNamedPipeConnector(HANDLE hPipe, IIPCObject* pServer, IIPCEvent* pEvent): m_hPipe(hPipe), m_pServer(pServer), m_pEventSensor(pEvent)
{
    ZeroMemory(&m_ovPostMessage, sizeof(OVERLAPPED));
    m_ovPostMessage.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CNamedPipeConnector::~CNamedPipeConnector()
{
    if(NULL != m_ovPostMessage.hEvent)
    {
        CloseHandle(m_ovPostMessage.hEvent);
        m_ovPostMessage.hEvent = NULL;
    }
}

HANDLE CNamedPipeConnector::GetHandle()
{
    return m_hPipe;
}