#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>

////////////////////////////////////////////////////////
// 代表游戏中的一个玩家
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	// 输入类型Flag
	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	// 输入Flag
	enum class EInputFlag : uint8
	{
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3
	};

	// 序列化的方面(Aspect)
	static constexpr EEntityAspects InputAspect = eEA_GameClientD;
	
public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() = default;

	// IEntityComponent
	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	
	// 网络序列化
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return InputAspect; }
	// ~IEntityComponent

	// 反射类型，为此组件设定独有id
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
	}

	void OnReadyForGameplayOnServer();
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0; }
	
protected:
	void Revive(const Matrix34& transform);
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);

	// 当实体成为本地玩家时调用，用以创建客户端特化设定比如相机
	void InitializeLocalPlayer();
	
	// 以下为远程(Remote)方法定义
protected:
	// 传递给RemoteReviveOnClient函数的参数
	struct RemoteReviveParams
	{
		// 在服务器上调用一次，以此序列化数据到其他客户端
		// 然后在另一边调用一次以反序列化
		// Called once on the server to serialize data to the other clients
		// Then called once on the other side to deserialize
		void SerializeWith(TSerialize ser)
		{
			// 以'wrld'压缩策略序列化坐标
			ser.Value("pos", position, 'wrld');
			// 以'ori0'压缩策略序列化旋转
			ser.Value("rot", rotation, 'ori0');
		}
		
		Vec3 position;
		Quat rotation;
	};
	// 远程方法，用于当一个玩家在服务器上生成时，在所有远程客户端上调用
	bool RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel);
	
protected:
	bool m_isAlive = false;

	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;

	CEnumFlags<EInputFlag> m_inputFlags;
	Vec2 m_mouseDeltaRotation;
};
