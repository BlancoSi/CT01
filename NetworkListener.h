#pragma once

#include <CryNetwork/INetwork.h>
#include <unordered_map>
// network & spawn player
class CNetworkedClientListener final : public INetworkedClientListener
{
public:
    CNetworkedClientListener();
    virtual ~CNetworkedClientListener();
protected:
    // <频道id, 玩家实体id>
    std::unordered_map<int, EntityId> m_clientEntityIdLookupMap;
	
    // 新客户端开始连接时发送到服务器
    // 返回false拒绝连接
    virtual bool OnClientConnectionReceived(int channelId, bool bIsReset) override;
	
    // 新客户端完成连接且准备好进行游戏后发送到服务器
    // 返回false拒绝连接并踢出玩家
    virtual bool OnClientReadyForGameplay(int channelId, bool bIsReset) override;
	
	// 客户端断开连接后发送到服务器
    virtual void OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override;
	
    // 客户端超时时发送到服务器(no packets for X seconds)
    // 返回true允许中断连接，返回其他则保持此客户端
    virtual bool OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override;
	
    // 断开连接后发动到本地客户端
    virtual void OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) override;
};