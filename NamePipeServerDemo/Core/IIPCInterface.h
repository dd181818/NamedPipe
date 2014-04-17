#pragma once
#include <windows.h>
#include <map>

enum NAMEDPIPE_STATUS
{
    NAMED_PIPE_CONNECT,
    NAMED_PIPE_READING,
    NAMED_PIPE_WRIEING,
    NAMED_PIPE_DISCONNECT
};
const int SYELOG_MAXIMUM_MESSAGE = 4086;
typedef struct _SYELOG_MESSAGE
{
    USHORT      nBytes;
    BYTE        nFacility;
    BYTE        nSeverity;
    DWORD       nProcessId;
    FILETIME    ftOccurance;
    BOOL        fTerminate;
    TCHAR        szMessage[SYELOG_MAXIMUM_MESSAGE];
} SYELOG_MESSAGE, *PSYELOG_MESSAGE;

typedef struct _CLIENT : OVERLAPPED
{
    HANDLE          hPipe;
    NAMEDPIPE_STATUS            emPipeStatus;
    PVOID           Zero;
    SYELOG_MESSAGE  Message;
} CLIENT, *PCLIENT;

#define pure_virtual __declspec(novtable)

struct pure_virtual IIPCConnector
{
    virtual ~IIPCConnector() = 0 {};
    virtual HANDLE GetHandle() = 0;
    virtual BOOL SendMessage(LPCVOID lpBuf, DWORD dwBufSize) = 0;
    virtual BOOL PostMessage(LPCVOID lpBuf, DWORD dwBufSize) = 0;
};

struct pure_virtual IIPCConnectorIterator
{
    virtual ~IIPCConnectorIterator() = 0 {};
    virtual void Begin() = 0;
    virtual BOOL End() = 0;
    virtual void Next() = 0;
    virtual IIPCConnector* GetCurrent() = 0;
};

typedef std::map<HANDLE, IIPCConnector*> ConnectorMap;

struct pure_virtual IIPCObject
{
    virtual ~IIPCObject() = 0 {};
    virtual BOOL Create(LPCTSTR lpPipeName) = 0;
    virtual void Close() = 0;
    virtual IIPCConnectorIterator* GetClients() = 0;
};

struct pure_virtual IIPCEvent
{
    virtual ~IIPCEvent() = 0 {};
    virtual void OnConnect(IIPCObject* pServer, IIPCConnector* pClient) = 0;
    virtual void OnDisConnect(IIPCObject* pServer, IIPCConnector* pClient) = 0;
    virtual void OnCreate(IIPCObject* pServer) = 0;
    virtual void OnClose(IIPCObject* pServer) = 0;
    virtual void OnRecv(IIPCObject* pServer, IIPCConnector* pClient, LPCVOID lpBuf, DWORD dwBufSize) = 0;
    virtual void OnSend(IIPCObject* pServer, IIPCConnector* pClient, LPVOID lpBuf, DWORD dwBufSize) = 0;
};