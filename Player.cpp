#include "StdAfx.h"
#include "Player.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

void CPlayerComponent::Initialize()
{
	// 标记需要在网络上复制的实体
	m_pEntity->GetNetEntity()->BindToNetwork();
	
	// 注册RemoteReviveOnClient函数为RMI(Remote Method Invocation)(可以被服务器执行于各客户端)
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
}

// 初始化本地玩家
void CPlayerComponent::InitializeLocalPlayer()
{
	// 生成相机组件，每帧自动刷新
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	// 取得输入组件，wraps access to action mapping so we can easily get callbacks when inputs are triggered
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	
	// 注册ActionMap组、Action以及触发时调用的回调函数(ActionMap组名，Action名，回调函数)
	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode);  }); 
	// 绑定按键
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("player", "mouse_rotateyaw", [this](int activationMode, float value) { m_mouseDeltaRotation.x -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value) { m_mouseDeltaRotation.y -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);
}

// 引擎用其获取事件遮罩
// 只有匹配的事件才会被发送到ProcessEvent方法
Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::BecomeLocalPlayer |
			Cry::Entity::EEvent::Update;
}

// 事件处理函数
void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		
	// 成为本地玩家
	case Cry::Entity::EEvent::BecomeLocalPlayer:
	{
		InitializeLocalPlayer();
	}
	break;
	
	// 每帧更新
	case Cry::Entity::EEvent::Update:
	{
		// 确认玩家是否生成
		if(!m_isAlive)
			return;
		
		const float frameTime = event.fParam[0];

		const float moveSpeed = 20.5f;
		Vec3 velocity = ZERO;

		// 检查输入，计算本地空间速度
		if (m_inputFlags & EInputFlag::MoveLeft)
		{
			velocity.x -= moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveRight)
		{
			velocity.x += moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveForward)
		{
			velocity.y += moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveBack)
		{
			velocity.y -= moveSpeed * frameTime;
		}

		// 更新玩家位移
		Matrix34 transformation = m_pEntity->GetWorldTM();
		transformation.AddTranslation(transformation.TransformVector(velocity));

		// 根据最后的输入更新实体旋转
		Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(transformation));

		const float rotationSpeed = 0.002f;
		ypr.x += m_mouseDeltaRotation.x * rotationSpeed;
		ypr.y += m_mouseDeltaRotation.y * rotationSpeed;

		// 禁用滚动
		ypr.z = 0;

		transformation.SetRotation33(CCamera::CreateOrientationYPR(ypr));

		// 重置鼠标位移增量
		m_mouseDeltaRotation = ZERO;

		// 应用位移与旋转
		m_pEntity->SetWorldTM(transformation);
	}
	break;
	}
}

// Network serialization. Will be called for each active aspect for both reading and writing.
// 每次网络资源读写时调用
bool CPlayerComponent::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if(aspect == InputAspect)
	{
		ser.BeginGroup("PlayerInput");

		const CEnumFlags<EInputFlag> prevInputFlags = m_inputFlags;

		ser.Value("m_inputFlags", m_inputFlags.UnderlyingValue(), 'ui8');

		if (ser.IsReading())
		{
			const CEnumFlags<EInputFlag> changedKeys = prevInputFlags ^ m_inputFlags;

			const CEnumFlags<EInputFlag> pressedKeys = changedKeys & prevInputFlags;
			if (!pressedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnPress);
			}

			const CEnumFlags<EInputFlag> releasedKeys = changedKeys & prevInputFlags;
			if (!releasedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnRelease);
			}
		}

		ser.EndGroup();
	}
	
	return true;
}

// 服务器上准备好进行游戏
void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");
	
	Vec3 playerScale = Vec3(1.f);
	Quat playerRotation = IDENTITY;

	// 把玩家放在地图中心
	const float heightOffset = 20.f;
	const float terrainCenter = gEnv->p3DEngine->GetTerrainSize() / 2.f;
	const float height = gEnv->p3DEngine->GetTerrainZ(terrainCenter, terrainCenter);
	const Vec3 playerPosition = Vec3(terrainCenter, terrainCenter, height + heightOffset);

	const Matrix34 newTransform = Matrix34::Create(playerScale, playerRotation, playerPosition);
	
	Revive(newTransform);
	
	// 在所有远程客户端调用RemoteReviveOnClient函数，保证Revive在全网被调用
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this, RemoteReviveParams{ playerPosition, playerRotation });
	
	// 遍历其他玩家，发送它们各自实例的RemoteReviveOnClient到准备好游戏的新玩家
	const int channelId = m_pEntity->GetNetEntity()->GetChannelId();
	CGamePlugin::GetInstance()->IterateOverPlayers([this, channelId](CPlayerComponent& player)
	{
		// 不发送到自身(handled in the RemoteReviveOnClient event above sent to all clients)
		if (player.GetEntityId() == GetEntityId())
			return;

		// 仅发送Revive事件到服务器上已经生成的玩家
		if (!player.m_isAlive)
			return;

		// 在新玩家机器上复活此玩家，位于其现在所处的位置
		const QuatT currentOrientation = QuatT(player.GetEntity()->GetWorldTM());
		SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnClient(&player, RemoteReviveParams{ currentOrientation.t, currentOrientation.q }, channelId);
	});
}

bool CPlayerComponent::RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel)
{
	// 在此客户端调用Revive函数
	Revive(Matrix34::Create(Vec3(1.f), params.rotation, params.position));

	return true;
}

// Revive函数
void CPlayerComponent::Revive(const Matrix34& transform)
{
	m_isAlive = true;
	
	// 设定实体位移除非位于编辑器中
	// 位于编辑器中时在视口所在位置生成玩家
	if(!gEnv->IsEditor())
	{
		m_pEntity->SetWorldTM(transform);
	}
	
	// 既然玩家已经生成，重置输入
	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);
	
	m_mouseDeltaRotation = ZERO;
}

// 与m_pInputComponent->RegisterAction配和使用
void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
	{
		if (activationMode == eAAM_OnRelease)
		{
			m_inputFlags &= ~flags;
		}
		else
		{
			m_inputFlags |= flags;
		}
	}
	break;
	case EInputFlagType::Toggle:
	{
		if (activationMode == eAAM_OnRelease)
		{
			// Toggle the bit(s)
			m_inputFlags ^= flags;
		}
	}
	break;
	}
	
	if(IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
	}
}