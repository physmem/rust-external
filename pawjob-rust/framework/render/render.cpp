#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <imgui_internal.h>
#include <numbers>
#include <render/render.hpp>

#include <print>

#include "assets/container_header.hpp"
#include "assets/font_awesome.hpp"
#include "assets/font_awesome_data.hpp"
#include "assets/menu_bg.hpp"
#include "assets/noto_sans.hpp"
#include "assets/pixel_mix.hpp"
#include "assets/rounded_elegance.hpp"
#include "assets/steam_avatar.hpp"
#include "assets/watermark.hpp"
#include <misc/freetype/imgui_freetype.h>

namespace render
{
	void setup()
	{
		auto load = [](const FontId& f_id, std::uint8_t* data, std::size_t size, const ImWchar* ranges = nullptr, bool anti_alias = true) {
			FontId dynamic_id = f_id;
			dynamic_id.data = data;
			dynamic_id.data_size = size;
			fonts::create_font(dynamic_id, ranges, anti_alias);
			};

		ImGui::GetIO().Fonts->FontLoader = ImGuiFreeType::GetFontLoader();

		load(Fonts::NotoSans10px, noto_sans_bold, sizeof(noto_sans_bold));
		load(Fonts::NotoSans12px, noto_sans_bold, sizeof(noto_sans_bold));
		load(Fonts::NotoSans14px, noto_sans_bold, sizeof(noto_sans_bold));
		load(Fonts::NotoSans16px, noto_sans_semi_bold, sizeof(noto_sans_semi_bold)); // best fix ive ever done
		load(Fonts::NotoSans18px, noto_sans_bold, sizeof(noto_sans_bold));
		load(Fonts::NotoSans20px, noto_sans_bold, sizeof(noto_sans_bold));
		load(Fonts::Pixelmix10px, pixelmix_ttf, sizeof(pixelmix_ttf));

		fonts::create_font(Fonts::Arial14px, "segoeui.ttf", nullptr);

		load(Fonts::RoundedElegance24px, rounded_elegance, sizeof(rounded_elegance));

		static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		load(Fonts::Icons16px, font_awesome_data, sizeof(font_awesome_data), icon_ranges);
		load(Fonts::Icons20px, font_awesome_data, sizeof(font_awesome_data), icon_ranges);

		ImGui::GetIO().Fonts->Build();

		while (!menu_bg.is_valid())
			menu_bg.load_from_memory(menu_bg_data, sizeof(menu_bg_data));

		while (!container_header.is_valid())
			container_header.load_from_memory(container_header_data, sizeof(container_header_data));

		while (!watermark.is_valid())
			watermark.load_from_memory(watermark_data, sizeof(watermark_data));

		while (!steam_avatar.is_valid())
			steam_avatar.load_from_memory(steam_avatar_data, sizeof(steam_avatar_data));
	}

	Font* fonts::create_font(const FontId& config, const std::string_view& font_name, const ImWchar* glyph_ranges, bool anti_alias)

	{
		if (f.contains(config.hash))
			return f[config.hash];

		ImFontConfig im_config;
		if (glyph_ranges == nullptr) {
			static ImVector<ImWchar> ranges;
			static bool initialized = false;
			if (!initialized) {
				ImFontGlyphRangesBuilder builder;
				builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
				builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
				builder.BuildRanges(&ranges);
				initialized = true;
			}
			im_config.GlyphRanges = ranges.Data;
		}
		else {
			im_config.GlyphRanges = glyph_ranges;
		}

		if (!anti_alias)
			im_config.FontLoaderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;


		std::string font_path = std::format("C:\\Windows\\Fonts\\{}", font_name);
		f[config.hash] = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(), config.size, &im_config, im_config.GlyphRanges);

		return f[config.hash];
	}

	Font* fonts::create_font(const FontId& config, const ImWchar* glyph_ranges, bool anti_alias)
	{
		if (f.contains(config.hash))
			return f[config.hash];

		ImFontConfig im_config;
		if (glyph_ranges == nullptr) {
			static ImVector<ImWchar> ranges;
			static bool initialized = false;
			if (!initialized) {
				ImFontGlyphRangesBuilder builder;
				builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
				builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
				builder.BuildRanges(&ranges);
				initialized = true;
			}
			im_config.GlyphRanges = ranges.Data;
		}
		else {
			im_config.GlyphRanges = glyph_ranges;
		}

		if (config.data == nullptr || config.data_size == 0)
			return nullptr;

		ImFontConfig font_config;
		font_config.FontData = std::malloc(config.data_size);
		font_config.FontDataSize = static_cast<int>(config.data_size);
		font_config.SizePixels = config.size;
		font_config.GlyphRanges = im_config.GlyphRanges;
		if (!anti_alias)
			font_config.FontLoaderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;

		if (font_config.FontData)
		{
			std::memcpy(font_config.FontData, config.data, config.data_size);
		}

		f[config.hash] = ImGui::GetIO().Fonts->AddFont(&font_config);

		return f[config.hash];
	}

	void set_to_background()
	{
		draw_list = ImGui::GetBackgroundDrawList();
	}

	void set_to_foreground()
	{
		draw_list = ImGui::GetForegroundDrawList();
	}

	void add_text(const FontId& font_id, const std::string_view& text, const glm::vec2& input_position, const Color& color, const TextFlags& text_flags, const glm::vec2& align)
	{
		Font* f = get_font(font_id);
		if (!f) return;

		glm::vec2 position = input_position;
		const float font_size = f->LegacySize;
		glm::vec2 text_size = f->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.data());

		position -= glm::vec2(text_size.x * align.x, text_size.y * align.y);
		position.x = std::floor(position.x);
		position.y = std::floor(position.y);

		if (text_flags & TextFlagsDropShadow)
			draw_list->AddText(f, font_size, ImVec2{ std::floor(position.x), std::floor(position.y) } + ImVec2{ 1.0f, 1.0f }, Color::black().scale_alpha(0.75f * color.scalable_alpha()), text.data());

		if (text_flags & TextFlagsOutline)
		{
			draw_list->AddText(f, font_size, ImVec2{ std::floor(position.x), std::floor(position.y) } - ImVec2(1.0f, 0.0f), Color::black().scale_alpha(0.5f * color.scalable_alpha()), text.data());
			draw_list->AddText(f, font_size, ImVec2{ std::floor(position.x), std::floor(position.y) } - ImVec2(0.0f, 1.0f), Color::black().scale_alpha(0.5f * color.scalable_alpha()), text.data());
			draw_list->AddText(f, font_size, ImVec2{ std::floor(position.x), std::floor(position.y) } + ImVec2(1.0f, 0.0f), Color::black().scale_alpha(0.5f * color.scalable_alpha()), text.data());
			draw_list->AddText(f, font_size, ImVec2{ std::floor(position.x), std::floor(position.y) } + ImVec2(0.0f, 1.0f), Color::black().scale_alpha(0.5f * color.scalable_alpha()), text.data());
		}

		draw_list->AddText(f, font_size, { std::floor(position.x), std::floor(position.y) }, color, text.data());
	}

	glm::vec2 get_text_size(const FontId& font_id, const std::string_view& text)
	{
		Font* font = get_font(font_id);
		if (!font) return { 0.f, 0.f };

		ImVec2 font_size = font->CalcTextSizeA(font->LegacySize, FLT_MAX, 0.0f, text.data());
		return { font_size.x, font_size.y };
	}

	void add_rect(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& rounding, const float& thickness, const std::int32_t flags)
	{
		draw_list->AddRect({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, color, rounding, flags, thickness);
	}

	void add_rect_filled(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& rounding, const std::int32_t flags)
	{
		draw_list->AddRectFilled({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, color, rounding, flags);
	}

	void add_line(const glm::vec2& from, const glm::vec2& to, const Color& color, const float& thickness)
	{
		draw_list->AddLine({ std::floor(from.x), std::floor(from.y) }, { std::floor(to.x), std::floor(to.y) }, color, thickness);
	}

	void add_polyline(const std::vector<glm::vec2>& points, const Color& color, const float& thickness, const bool& closed)
	{
		if (points.size() < 2)
			return;

		draw_list->AddPolyline((const ImVec2*)points.data(), static_cast<int>(points.size()), color, closed ? ImDrawFlags_Closed : 0, thickness);
	}

	void add_shadow_rect_fast(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& shadow_thick, const float& rounding)
	{
		draw_list->AddShadowRect({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, color, shadow_thick, ImVec2(0.0f, 0.0f), 0, rounding);
	}

	void add_circle(const glm::vec2& position, const float& radius, const Color& color, const float& thickness)
	{
		draw_list->AddCircle({ std::floor(position.x), std::floor(position.y) }, radius, color, 0, thickness);
	}

	void add_circle_filled(const glm::vec2& position, const float& radius, const Color& color)
	{
		draw_list->AddCircleFilled({ std::floor(position.x), std::floor(position.y) }, radius, color);
	}

	void add_circle_shadow(const glm::vec2& position, const float& radius, const Color& color, const float& shadow_thick)
	{
		draw_list->AddShadowCircle({ std::floor(position.x), std::floor(position.y) }, radius, color, shadow_thick, { 0.0f, 0.0f });
	}

	void add_capsule(const glm::vec2& position, const glm::vec2& size, const Color& color)
	{
		const float radius = size.x * 0.5f;
		draw_list->PathClear();
		draw_list->PathArcTo({ std::floor(position.x + radius), std::floor(position.y + radius) }, radius, IM_PI, IM_PI * 2.0f);
		draw_list->PathArcTo({ std::floor(position.x + radius), std::floor(position.y + size.y - radius) }, radius, 0.0f, IM_PI);
		draw_list->PathFillConvex(color);
	}

	void push_clip_rect(const glm::vec2& position, const glm::vec2& size)
	{
		draw_list->PushClipRect({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, true);
	}

	void pop_clip_rect()
	{
		draw_list->PopClipRect();
	}

	void add_checkmark(glm::vec2 position, const Color& color, float size, const float& progression)
	{
		const float thickness = std::max(size / 5.0f, 1.0f);
		const float cap_radius = thickness * 0.5f;

		size -= thickness * 0.5f;
		position += glm::vec2(thickness * 0.25f, thickness * 0.25f);

		const float third = size / 3.0f;

		const glm::vec2 p0 = glm::vec2(position.x + third - third, position.y + size - third * 0.5f - third);         // start
		const glm::vec2 p1 = glm::vec2(position.x + third, position.y + size - third * 0.5f);                         // middle
		const glm::vec2 p2 = glm::vec2(position.x + third + third * 2, position.y + size - third * 0.5f - third * 2); // end

		if (progression <= 0.0f)
			return;

		draw_list->PathClear();

		const float total_len = ImLength(p0 - p1, 0.0f) + ImLength(p1 - p2, 0.0f);
		const float draw_len = total_len * progression;

		const float first_seg_len = ImLength(p0 - p1, 0.0f);
		const float second_seg_len = ImLength(p1 - p2, 0.0f);

		glm::vec2 current_end;

		if (draw_len <= first_seg_len)
		{
			float t = draw_len / first_seg_len;
			current_end = ImLerp(p0, p1, t);
			draw_list->PathLineTo(p0);
			draw_list->PathLineTo(current_end);
		}
		else
		{
			draw_list->PathLineTo(p0);
			draw_list->PathLineTo(p1);

			float t = (draw_len - first_seg_len) / second_seg_len;
			current_end = ImLerp(p1, p2, std::clamp(t, 0.0f, 1.0f));
			draw_list->PathLineTo(current_end);
		}

		draw_list->PathStroke(color, false, thickness);

		if (draw_len <= first_seg_len)
		{
			float t = draw_len / first_seg_len;
			current_end = ImLerp(p0, p1, t);
			draw_list->AddLine(p0, current_end, color, 1.0f);
		}
		else
		{
			draw_list->AddLine(p0, p1, color, 1.0f);
			float t = (draw_len - first_seg_len) / second_seg_len;
			current_end = ImLerp(p1, p2, std::clamp(t, 0.0f, 1.0f));
			draw_list->AddLine(p1, current_end, color, 1.0f);

		}

		if (progression > 0.01f)
		{
			draw_list->AddCircleFilled(p0, cap_radius, color, 12);
		}
		if (progression >= 1.0f)
		{
			draw_list->AddCircleFilled(p2, cap_radius, color, 12);
		}
		else
		{
			draw_list->AddCircleFilled(current_end, cap_radius, color, 12);
		}
	}

	std::vector<glm::vec2> get_points_for_box(const glm::vec2& position, const glm::vec2& size, float rounding, const int& segments_per_corner = 8)
	{
		const float max_rounding = std::min(size.x * 0.5f, size.y * 0.5f);
		rounding = std::min(rounding, max_rounding);

		if (rounding <= 0.0f || segments_per_corner < 1)
		{
			return { {position.x, position.y},
					{position.x + size.x, position.y},
					{position.x + size.x, position.y + size.y},
					{position.x, position.y + size.y} };
		}

		const float x = position.x;
		const float y = position.y;
		const float w = size.x;
		const float h = size.y;

		const int total_points = (segments_per_corner + 1) * 4;
		std::vector<glm::vec2> points;
		points.reserve(total_points);

		// corners
		const glm::vec2 centers[4] = {
			{x + w - rounding, y + rounding},     // top right
			{x + w - rounding, y + h - rounding}, // bottom right
			{x + rounding, y + h - rounding},     // bottom left
			{x + rounding, y + rounding}          // top left
		};

		// arc ranges
		constexpr float pi = std::numbers::pi_v<float>;
		constexpr float angle_starts[4] = { 1.5f * pi, 0.0f, 0.5f * pi, pi };
		constexpr float angle_ends[4] = { 2.0f * pi, 0.5f * pi, pi, 1.5f * pi };

		// make corner arcs :3
		for (int c = 0; c < 4; c++)
		{
			const float a0 = angle_starts[c];
			const float a1 = angle_ends[c];
			const glm::vec2 center = centers[c];

			for (int i = 0; i <= segments_per_corner; i++)
			{
				const float t = static_cast<float>(i) / static_cast<float>(segments_per_corner);
				const float a = a0 + t * (a1 - a0);
				points.emplace_back(center.x + std::cos(a) * rounding, center.y + std::sin(a) * rounding);
			}
		}

		return points;
	}

	void add_shadow_poly(const std::vector<glm::vec2>& points, const Color& color, const float& shadow_thick, bool cut_background)
	{
		draw_list->AddShadowConvexPoly((const ImVec2*)points.data(), static_cast<int>(points.size()), color, shadow_thick, glm::vec2{ 0.0f, 0.0f }, cut_background ? ImDrawFlags_ShadowCutOutShapeBackground : 0);
	}

	void add_shadow_rect(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& shadow_thick, const float& rounding)
	{
		add_shadow_poly(get_points_for_box(position, size, rounding), color, shadow_thick, true);
	}

	void add_rect_gradient(const glm::vec2& position, const glm::vec2& size, GradientType type, const Color& color1,
		const Color& color2, float rounding, int flags)
	{

		auto shade_verts_linear_color_gradient =
			[&](int vert_start_idx, int vert_end_idx, glm::vec2 pos, glm::vec2 sz, Color top_left, Color top_right, Color bottom_right, Color bottom_left)
			{
				auto clamp_vec = [](const glm::vec2& to_clamp, float min, float max)
					{
						return glm::vec2((to_clamp.x < min) ? min
							: (to_clamp.x > max) ? max
							: to_clamp.x,
							(to_clamp.y < min) ? min
							: (to_clamp.y > max) ? max
							: to_clamp.y);
					};

				auto vert_start = draw_list->VtxBuffer.Data + vert_start_idx;
				auto vert_end = draw_list->VtxBuffer.Data + vert_end_idx;
				glm::vec2 gradient_extent = glm::vec2(sz.x, sz.y);

				for (auto vert = vert_start; vert < vert_end; vert++)
				{
					ImColor imcol_col = vert->col;
					float vert_alpha = imcol_col.Value.w;

					glm::vec2 dt = clamp_vec((static_cast<glm::vec2>(vert->pos) - pos) / gradient_extent, 0.f, 1.f);

					Color left_color = top_left.lerp(bottom_left, dt.y);
					Color right_color = top_right.lerp(bottom_right, dt.y);

					ImColor result_color = ImColor{ left_color.lerp(right_color, dt.x) };

					result_color.Value.w *= vert_alpha;

					vert->col = result_color;
				}
			};

		if (rounding == 0.f)
		{
			switch (type)
			{
			case GradientType::Horizontal:
				draw_list->AddRectFilledMultiColor({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, color1, color2, color2, color1);
				break;
			case GradientType::Vertical:
				draw_list->AddRectFilledMultiColor({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, color1, color1, color2, color2);
				break;
			}

			return;
		}

		const int vtx_idx_0 = draw_list->VtxBuffer.Size;
		draw_list->AddRectFilled({ std::floor(position.x), std::floor(position.y) }, { std::floor(position.x + size.x), std::floor(position.y + size.y) }, Color::white(), rounding, flags);
		const int vtx_idx_1 = draw_list->VtxBuffer.Size;

		switch (type)
		{
		case GradientType::Horizontal:
			shade_verts_linear_color_gradient(vtx_idx_0, vtx_idx_1, { std::floor(position.x), std::floor(position.y) }, { std::floor(size.x), std::floor(size.y) }, color1, color2, color2, color1);
			break;
		case GradientType::Vertical:
			shade_verts_linear_color_gradient(vtx_idx_0, vtx_idx_1, { std::floor(position.x), std::floor(position.y) }, { std::floor(size.x), std::floor(size.y) }, color1, color1, color2, color2);
			break;
		}
	}

	void gradient_items(const glm::vec2& position, const glm::vec2& size, const Color& first_color, const Color& second_color, const std::function< void() >& fn, const float& rotation)
	{
		const int vtx_idx_0 = draw_list->VtxBuffer.Size;

		fn();

		const int vtx_idx_1 = draw_list->VtxBuffer.Size;


		auto shade_verts_linear_color_gradient = [&](const int& vert_start_idx, const int& vert_end_idx, const glm::vec2& pos, const glm::vec2& sz,
			const Color& left_color, const Color& right_color) {
				auto clamp_vec = [](const glm::vec2& to_clamp, float min, float max) {
					return glm::vec2((to_clamp.x < min) ? min
						: (to_clamp.x > max) ? max
						: to_clamp.x,
						(to_clamp.y < min) ? min
						: (to_clamp.y > max) ? max
						: to_clamp.y);
					};

				const auto vert_start = draw_list->VtxBuffer.Data + vert_start_idx;
				const auto vert_end = draw_list->VtxBuffer.Data + vert_end_idx;

				const float angle = rotation * std::numbers::pi_v< float > *2.0f;
				const float cos_a = std::cos(angle);
				const float sin_a = std::sin(angle);

				for (auto vert = vert_start; vert < vert_end; vert++) {
					const float vert_alpha = static_cast<ImColor>(vert->col).Value.w;

					glm::vec2 uv = (vert->pos - pos) / sz;
					uv = clamp_vec(uv, 0.0f, 1.0f);

					uv.x -= 0.50f;
					uv.y -= 0.50f;

					glm::vec2 rot_uv = { uv.x * cos_a - uv.y * sin_a, uv.x * sin_a + uv.y * cos_a };

					rot_uv.x += 0.50f;
					rot_uv.y += 0.50f;
					rot_uv = clamp_vec(rot_uv, 0.0f, 1.0f);

					ImColor result_color{ left_color.lerp(right_color, rot_uv.x) };
					result_color.Value.w *= vert_alpha;

					vert->col = result_color;
				}
			};

		shade_verts_linear_color_gradient(vtx_idx_0, vtx_idx_1, position, size, first_color, second_color);
	}

	int start_idx;
	glm::vec2 get_rotation_center()
	{
		glm::vec2 l{ FLT_MAX, FLT_MAX }, u{ -FLT_MAX, -FLT_MAX }; // bounds

		const auto& buf = draw_list->VtxBuffer;
		for (int i = start_idx; i < buf.Size; i++)
			l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);

		return glm::vec2{ (l.x + u.x) / 2.0f, (l.y + u.y) / 2.0f }; // or use _ClipRectStack?
	}

	void rotate_vertices(float rotation, std::function< void() > items_to_rotate)
	{
		start_idx = draw_list->VtxBuffer.Size;
		rotation *= IM_PI * 2.0f;

		items_to_rotate();

		float s = ImSin(rotation), c = ImCos(rotation);
		auto center = get_rotation_center();
		const auto rotated_center = ImRotate(center, s, c) - center;
		center = glm::vec2(rotated_center.x, rotated_center.y);

		auto& buf = draw_list->VtxBuffer;
		for (int i = start_idx; i < buf.Size; i++)
			buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
	}

	void modify_alpha(int start_idx_param, int end_idx_param, float alpha)
	{
		if (alpha >= 1.0f)
			return;

		for (int i = start_idx_param; i < end_idx_param; i++) {
			ImDrawVert& vtx = draw_list->VtxBuffer[i];
			const auto a = static_cast<unsigned int>((vtx.col >> 24) & 0xFF);
			vtx.col = (vtx.col & 0x00FFFFFF) | (static_cast<unsigned int>(a * alpha) << 24);
		}
	}

	void add_image(ID3D11ShaderResourceView* srv, const glm::vec2& position, const glm::vec2& size, const float& rounding, const Color& color)
	{
		draw_list->AddImageRounded(srv, glm::vec2{ std::floorf(position.x), std::floorf(position.y) }, glm::vec2{ std::floorf(position.x + size.x), std::floorf(position.y + size.y) }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, color, rounding);
	}

	void add_triangle(const glm::vec2& one, const glm::vec2& two, const glm::vec2& three, const Color& color, const float& thickness)
	{
		draw_list->AddTriangle(one, two, three, color, thickness);
	}

	void add_triangle_filled(const glm::vec2& one, const glm::vec2& two, const glm::vec2& three, const Color& color)
	{
		draw_list->AddTriangleFilled(one, two, three, color);
	}
}
