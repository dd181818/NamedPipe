#pragma once
#include <windows.h>
#include <map>

enum IPC_MESSAGE_TYPE
{
    IPC_MESSAGE_UKNOWN,
    IPC_MESSAGE_CLIENTCONNECT,
    IPC_MESSAGE_READ,
    IPC_MESSAGE_WRITE,
};

const int SYELOG_MAXIMUM_MESSAGE = 4096;

typedef struct _IPC_DATA_PACKAGE
{
    DWORD dwTotalSize;                                      // ͨ�Ű������С
    DWORD dwProcessID;                                      // ��ǰ����ID
    FILETIME ftOccurance;                                   // �첽Ͷ�ݷ���ʱ��
    DWORD dwDataSize;                                       // �û���Ӧ������
    TCHAR lpData[SYELOG_MAXIMUM_MESSAGE];                   // �û���Ӧ������

} IPC_DATA_PACKAGE, *LPIPC_DATA_PACKAGE;


typedef struct _IPC_DATA_OVERLAPPEDEX : OVERLAPPED
{
    IPC_MESSAGE_TYPE emMessageType;                         // ��Ϣ����
    IPC_DATA_PACKAGE ipcDataPackage;
} IPC_DATA_OVERLAPPEDEX, *LPIPC_DATA_OVERLAPPEDEX;

typedef struct _USER_DATA_PACKAGE
{
    DWORD dwPackageType;
    TCHAR lpBuf[MAX_PATH];
} USER_DATA_PACKAGE, *LPUSER_DATA_PACKAGE;

#define pure_virtual __declspec(novtable)

struct pure_virtual IIPCConnector
{
    virtual ~IIPCConnector() = 0 {};
    virtual DWORD GetSID() = 0;
    virtual LPCTSTR GetName() = 0;
    virtual BOOL PostMessage(LPCVOID lpBuf, DWORD dwBufSize) = 0;
    virtual BOOL RequestAndReply(LPVOID lpSendBuf, DWORD dwSendBufSize, LPVOID lpReplyBuf, DWORD dwReplyBufSize, LPDWORD dwTransactSize) = 0;
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
    virtual void OnRequest(IIPCObject* pServer, IIPCConnector* pClient, LPCVOID lpBuf, DWORD dwBufSize) = 0;
};