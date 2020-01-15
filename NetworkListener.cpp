#include "StdAfx.h"
#include "NetworkListener.h"
#include "GamePlugin.h"

#include <CryNetwork/INetwork.h>
#include <unordered_map>
// network & spawn player

CNetworkedClientListener::CNetworkedClientListener()
{
    // 监听客户端连接事件
    gEnv->pGameFramework->AddNetworkedClientListener(*this);
}
	
virtual CNetworkedClientListener::~CNetworkedClientListener()
{
    // 在此处之前移除其他Listener
    gEnv->pGameFramework->RemoveNetworkedClientListener(*this);
}

	
// 新客户端开始连接时发送到服务器
// 返回false拒绝连接
virtual bool CNetworkedClientListener::OnClientConnectionReceived(int channelId, bool bIsReset) override
{

    // 收到客户端连接，创建玩家实体
    SEntitySpawnParams spawnParams;

    // 为此玩家实体赋予默认名称
    // 非必要但对debug有利
    const string playerName = string().Format("Player(%i)", channelId);
    spawnParams.sName = playerName;

    // 声明为完全动态实体
    spawnParams.nFlags |= ENTITY_FLAG_NEVER_NETWORK_STATIC;
		
    // 确认是否为本地玩家
    const bool isLocalPlayer = m_clientEntityIdLookupMap.empty() && !gEnv->IsDedicated();
	
    // 如果为本地玩家则使用预定义id并使用flag ENTITY_FLAG_LOCAL_PLAYER
    if (isLocalPlayer)
    {
        spawnParams.id = LOCAL_PLAYER_ENTITY_ID;
        spawnParams.nFlags |= ENTITY_FLAG_LOCAL_PLAYER;
    }

    // 生成玩家实体
    if (IEntity* pPlayerEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
    {
        // 设定本地玩家频道id并绑定到网络以支持网络上下文
        pPlayerEntity->GetNetEntity()->SetChannelId(channelId);
        pPlayerEntity->GetNetEntity()->BindToNetwork();
        // 将其放入用于存储的Map
        m_clientEntityIdLookupMap.emplace(std::make_pair(channelId, pPlayerEntity->GetId()));
        return true;
    }
    // 生成失败，拒绝连接
    return false;
}
	
// 新客户端完成连接且准备好进行游戏后发送到服务器
// 返回false拒绝连接并踢出玩家
virtual bool CNetworkedClientListener::OnClientReadyForGameplay(int channelId, bool bIsReset) override
{
    /* 此处开始出现游戏逻辑 */
    return true;
}

// 客户端断开连接后发送到服务器
virtual void CNetworkedClientListener::OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override
{
    // 检查是否已经为此客户端创建实体
    decltype(m_clientEntityIdLookupMap)::const_iterator clientIterator = m_clientEntityIdLookupMap.find(channelId);
    if (clientIterator != m_clientEntityIdLookupMap.end())
    {
        // 已创建实体，从场景删除
        const EntityId clientEntityId = clientIterator->second;
        gEnv->pEntitySystem->RemoveEntity(clientEntityId);
    }
}
	
// 客户端超时时发送到服务器(no packets for X seconds)
// 返回true允许中断连接，返回其他拒绝断线
virtual bool CNetworkedClientListener::OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override { return true; }
	
// 断开连接后发动到本地客户端
virtual void CNetworkedClientListener::OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) override {}