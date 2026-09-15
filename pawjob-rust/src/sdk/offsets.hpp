#pragma once
#include "globals.hpp"

namespace offsets {
	namespace il2cpp {
		inline constexpr uptr get_handle = 0xe93e2a0;
	} //  il2cpp

	namespace base_networkable {
		inline constexpr uptr typeinfo = 0xe5fa1f0;

		inline constexpr u32 static_fields = 0xb8;
		inline constexpr u32 client_entities = 0x8;
		inline constexpr u32 entity_list = 0x10;
		inline constexpr u32 buffer = 0x18;
		inline constexpr u32 entListBase = 0x10;
		inline constexpr u32 entLS = 0x18;
	} // base_networkable

	namespace main_camera {
		inline constexpr uptr typeinfo = 0xe601c80;

		inline constexpr u32 static_fields = 0xb8;
		inline constexpr u32 instance = 0x40;
		inline constexpr u32 buffer = 0x10;
		inline constexpr u32 view_matrix = 0x30C;
	} // main_camera

	/*
	  "Name": "FlashbangOverlay_TypeInfo",
	  "Signature": "FlashbangOverlay_c*"
	*/
	namespace FlashBangOverlay
	{
		inline constexpr uptr typeinfo = 0xe4493d8;
		inline constexpr u32 instance = 0x8;
		inline constexpr u32 flashLength = 0x48;
	}

	namespace BasePlayer {
		inline constexpr uptr username = 0x368;
		inline constexpr uptr team = 0x4e8;
		inline constexpr uptr baseMovement = 0x360;
		inline constexpr uptr waterBody = 0x4c8;
		inline constexpr uptr playerModel = 0x448;
		inline constexpr uptr playerFlags = 0x660;
		inline constexpr uptr playerInput = 0x4f0;
		inline constexpr uptr playerEyes = 0x3a0;
		inline constexpr uptr clActiveItem = 0x518;
		inline constexpr uptr playerInventory = 0x730;
		inline constexpr uptr userID = 0x5a0;
	} // BasePlayer

	namespace PlayerInput {
		inline constexpr uptr bodyAngles = 0x44;
	} // PlayerInput

	namespace BaseEntity {
		inline constexpr uptr model = 0xf0;
	} // BaseEntity

	namespace Item {
		inline constexpr uptr itemDefinition = 0x90;
		inline constexpr uptr itemUid = 0x30;
		inline constexpr uptr itemUid2 = 0x3c;
		inline constexpr uptr itemUid3 = 0x48;
		inline constexpr uptr itemUid4 = 0xd8;
		inline constexpr uptr heldEntity = 0x80;
		inline constexpr uptr health = 0;
		inline constexpr uptr maxHealth = 0;
	} // Item

	namespace BaseCombatEntity {
		inline constexpr uptr lifeState = 0x278;
		inline constexpr uptr health = 0x284;
		inline constexpr uptr maxHealth = 0x288;
	} // BaseCombatEntity

	namespace BaseProjectile {
		inline constexpr uptr recoilProp = 0x3b0;
		inline constexpr uptr primaryMagazine = 0x388;
		inline constexpr uptr viewModel = 0x270;
		inline constexpr uptr aimCone = 0x3c0;
		inline constexpr uptr hipAimCone = 0x3c4;
		inline constexpr uptr aimconePenaltyPerShot = 0x3c8;
		inline constexpr uptr aimConePenaltyMax = 0x3cc;
		inline constexpr uptr aimconePenaltyRecoverTime = 0x3d0;
		inline constexpr uptr aimconePenaltyRecoverDelay = 0x3d4;
		inline constexpr uptr stancePenaltyScale = 0x3d8;
		inline constexpr uptr internalBurstAimConeScale = 0x3f4;
		inline constexpr uptr automatic = 0x340;
		inline constexpr uptr isBurstWeapon = 0x3e7;
		inline constexpr uptr isCharged = 0x468; // BowWeapon -> first System.Boolean

	} // BaseProjectile

	namespace Projectile
	{
		inline constexpr uptr currentThickness = 0x158;
	} // Projectile

	namespace FlintStrikeWeapon
	{
		inline constexpr uptr successFraction = 0x468;
		inline constexpr uptr didSparkThisFrame = 0x478; // first bool in FlintStrikeWeapon
	} // FlintStrikeWeapon

	namespace BaseViewModel {
		inline constexpr uptr BaseViewModel_C = 0xe56af48;
		inline constexpr uptr animationEvents = 0x88;
		inline constexpr uptr list = 0x1c0;
	} // BaseViewModel

	namespace ItemContainer {
		inline constexpr uptr list = 0x28;
	} // ItemContainer

	namespace ItemDefinition {
		inline constexpr uptr shortName = 0x28;
		inline constexpr uptr itemDisplayName = 0x40;
		inline constexpr uptr itemModWearable = 0x158;
	} // ItemDefinition

	namespace ListComponent_Projectile {
		inline constexpr uptr ListComponent_C = 0xe577958;
		inline constexpr uptr static_fields = 0xb8;
		inline constexpr uptr parent_static = 0x10;
		inline constexpr uptr buffer = 0x10;
	} // ListComponent_Projectile

	namespace Magazine {
		inline constexpr uptr Capacity = 0x18;
		inline constexpr uptr Contents = 0x1c;
	} // Magazine

	namespace Model {
		inline constexpr uptr rootBone = 0x28;
		inline constexpr uptr headBone = 0x30;
		inline constexpr uptr boneTransforms = 0x50;
	} // Model

	namespace PlayerEyes {
		inline constexpr uptr viewOffset = 0x40;
		inline constexpr uptr bodyRotation = 0x50;
		inline constexpr uptr eyeRotation = 0;
		inline constexpr uptr unkQuanternion = 0x6c;
	} // PlayerEyes

	namespace PlayerInventory {
		inline constexpr uptr container1 = 0x30;
		inline constexpr uptr container2 = 0x58;
		inline constexpr uptr container3 = 0x78;
	} // PlayerInvetory

	namespace PlayerModel {
		inline constexpr uptr position = 0x210;
		inline constexpr uptr velocity = 0x234;
		inline constexpr uptr newVelocity = 0;
		inline constexpr uptr SkinnedMultiMesh = 0x180;
	} // PlayerModel

	namespace RecoilProperties {
		inline constexpr uptr recoilYawMin = 0x18;
		inline constexpr uptr recoilYawMax = 0x1c;
		inline constexpr uptr recoilPitchMin = 0x20;
		inline constexpr uptr recoilPitchMax = 0x24;
		inline constexpr uptr newRecoilOverride = 0x80;
	} // RecoilProperties

	namespace SkinnedMultiMesh {
		inline constexpr uptr rendererList = 0x50;
	} // SkinnedMultiMesh

	namespace TOD_Sky {
		inline constexpr uptr TOD_Sky_C = 0xe5a7350;
		inline constexpr uptr cycle = 0x40;
		inline constexpr uptr world = 0x48;
		inline constexpr uptr atmosphere = 0x50;
		inline constexpr uptr day = 0x58;
		inline constexpr uptr night = 0x60;
		inline constexpr uptr sun = 0x68;
		inline constexpr uptr moon = 0x70;
		inline constexpr uptr stars = 0x78;
		inline constexpr uptr clouds = 0x80;
		inline constexpr uptr light = 0x88;
		inline constexpr uptr fog = 0x90;
		inline constexpr uptr ambient = 0x98;
		inline constexpr uptr reflection = 0xA0;
	} // namespace TOD_Sky

	namespace BaseMelee {
		inline constexpr uptr maxDistance = 0x348;
		inline constexpr uptr attackRadius = 0x34c;
	} // BaseMelee
} // namespace offsets