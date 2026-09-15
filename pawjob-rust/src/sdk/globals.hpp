#pragma once
#include <cstdint>
#include <string>
#include <vector>

using u8 = unsigned __int8;
using u16 = unsigned __int16;
using u32 = unsigned __int32;
using u64 = unsigned __int64;
using uptr = uintptr_t;
using i8 = __int8;
using i16 = __int16;
using i32 = __int32;
using i64 = __int64;
using f32 = float;
using f64 = double;

static std::vector<u32> g_materials = {
	0,       // None
	440314,  // CRT HDR (manpad_crt_hdr)
	175576,  // Red Glow (ship_red_glow)
	//98806,   // Green Glow (GreenGlow)
	0,       // Pink
	104312,  // Neon (NeonTR On)
	//570596,  // Blue Holo (holosight.georeticle.blue)
	//570600,  // Green Holo (holosight.georeticle.green)
	//429652,  // Fireworks Glow (Fireworks_Glow)
	654520,  // Purple Zone (BRZone_purple)
	//859392,  // Lightning Bolt (LightningBolt)
	456270,  // Lightning Spark (fx-lightning3)
	492504,  // Star Trail (StarTrail)
	631400,  // Glass (glass)
	//563812,  // Directional Blue Wire (WireMaterial_Directional_Blue)
	//933742,  // Spark (Spark)
	808526,  // Water Drops (vfx_water_drops)
	134962,  // Gold (party_hat_7thbirthday_gold)
	476616,  // Roboto Mono Font (RobotoMono-Regular SDF NoSoftness)
	353868,  // Shockwave (vfx_shockwave)
	//145426,  // Shimmer (vfx_scarecrow_shimmer)
	448084,  // Rainbow (vfx_rainbow)
};

static std::vector<std::string> g_material_names = {
	"None",
	"CRT HDR",
	"Red glow",
	//"Green glow",
	"Pink",
	"Neon",
	//"Blue holo",
	//"Green holo",
	//"Fireworks glow",
	"Purple zone",
	//"Lightning bolt",
	"Lightning spark",
	"Star trail",
	"Glass",
	//"Animated blue wire",
	//"Spark",
	"Water drops",
	"Gold",
	"The fuckin roboto font LMFAO",
	"Shockwave",
	//"Shimmer",
	"Rainbow"
};