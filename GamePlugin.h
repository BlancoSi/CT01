#pragma once

#include <CrySystem/ICryPlugin.h>
#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntityClass.h>
#include <CryNetwork/INetwork.h>

class CPlayerComponent;

// 应用程序入口
// 引擎加载此库时会自动创建一个CGamePlugin实例
// 我们在OnClientConnectionReceived被第一次调用后创建本地玩家实体与CPlayerComponent实例
class CGamePlugin 
	: public Cry::IEnginePlugin
	, public ISystemEventListener
	, public INetworkedClientListener
{
public:

	// 声明此插件的继承层次结构，此处为声明我们实现了Cry::IEnginePlugin，以便引擎在加载插件后检测到此实例
	// 以下为#define的简化版本（见定义），若有需要可改用在CRYINTERFACE_BEGIN()和CRYINTERFACE_END()之间添加任意数量CRYINTERFACE_ADD(iname)
	CRYINTERFACE_SIMPLE(Cry::IEnginePlugin)
	// 为我们的插件设置GUID，这在所有使用的插件中应该是唯一的
	CRYGENERATE_SINGLETONCLASS_GUID(CGamePlugin, "Blank", "f01244b0-a4e7-4dc6-91e1-0ed18906fe7c"_cry_guid)

	virtual ~CGamePlugin();
	
	// Cry::IEnginePlugin
	virtual const char* GetCategory() const override { return "Game"; }
	
	// 从磁盘加载插件后不久调用，这通常是初始化任何第三方API和自定义代码的地方
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~Cry::IEnginePlugin

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// INetworkedClientListener
	// 断开连接时发送到本地客户端
	virtual void OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) override {}

	// 一个新客户端开始连接时发送到服务器
	// 返回false拒绝连接
	virtual bool OnClientConnectionReceived(int channelId, bool bIsReset) override;
	// 新客户端完成连接且准备好进行游戏后发送到服务器
	// 返回false拒绝连接并踢出玩家
	virtual bool OnClientReadyForGameplay(int channelId, bool bIsReset) override;
	// 一个客户端断开连接后发送到服务器
	virtual void OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override;
	// 一个客户端超时时发送到服务器(no packets for X seconds)
	// 返回true允许中断连接，返回其他则保持此客户端
	virtual bool OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override { return true; }
	// ~INetworkedClientListener

	// Helper function，用来为每个游戏中的玩家调用特定函数
	void IterateOverPlayers(std::function<void(CPlayerComponent& player)> func) const;

	// Helper function，用来取得CGamePlugin实例
	// 注意CGamePlugin被声明为单例(singleton)，所以CreateClassInstance将总是返回同一指针
	static CGamePlugin* GetInstance()
	{
		return cryinterface_cast<CGamePlugin>(CGamePlugin::s_factory.CreateClassInstance().get());
	}
	
protected:
	// 包含各个玩家组件的Map，键为在OnClientConnectionReceived中接收的频道id
	std::unordered_map<int, EntityId> m_players;
};