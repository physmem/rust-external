#include "texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.hpp"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <d3d11.h>

#include <app/app.hpp>

// trollface
bool is_gif(const unsigned char* buffer, size_t size)
{
	if (size < 6)
		return false;
	return (buffer[0] == 'G' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == '8' && (buffer[4] == '7' || buffer[4] == '9') &&
		buffer[5] == 'a');
}

texture_t::~texture_t()
{
	cleanup();
}

void texture_t::cleanup()
{
	for (auto& frame : m_frames) {
		if (frame.m_srv) {
			frame.m_srv->Release();
			frame.m_srv = nullptr;
		}
	}
	m_frames.clear();
	m_current_frame_idx = 0;
	m_is_animated = false;
}

void texture_t::release()
{
	cleanup();
}

bool create_texture_from_pixels(unsigned char* pixels, int width, int height, ID3D11ShaderResourceView** out_srv)
{
	if (!pixels || !out_srv || !app::device)
		return false;

	D3D11_TEXTURE2D_DESC desc = { };
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 0; // Allocate full mip chain
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* pTexture = nullptr;
	HRESULT hr = app::device->CreateTexture2D(&desc, nullptr, &pTexture);
	if (FAILED(hr) || !pTexture)
		return false;

	ID3D11DeviceContext* ctx = nullptr;
	app::device->GetImmediateContext(&ctx);
	if (!ctx) {
		pTexture->Release();
		return false;
	}

	ctx->UpdateSubresource(pTexture, 0, nullptr, pixels, width * 4, 0);

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = (UINT)-1;

	hr = app::device->CreateShaderResourceView(pTexture, &srv_desc, out_srv);
	pTexture->Release();

	if (SUCCEEDED(hr)) {
		ctx->GenerateMips(*out_srv);
	}

	ctx->Release();
	return SUCCEEDED(hr);
}

bool texture_t::load_from_file(const std::string& path)
{
	cleanup();

	if (!std::filesystem::exists(path)) {
		return false;
	}

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return false;

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector< char > buffer(size);
	if (file.read(buffer.data(), size)) {
		return load_from_memory(buffer.data(), size);
	}

	return false;
}

bool texture_t::load_from_memory(const void* data, size_t size)
{
	cleanup();

	if (!data || size == 0)
		return false;

	const unsigned char* buffer = reinterpret_cast<const unsigned char*>(data);

	if (is_gif(buffer, size)) {
		int* delays = nullptr;
		int x, y, z, role;
		stbi_uc* pixels = stbi_load_gif_from_memory(buffer, (int)size, &delays, &x, &y, &z, &role, 4);

		if (!pixels) {
			return false;
		}

		m_is_animated = true;
		m_frames.resize(z);

		size_t stride = x * y * 4;

		for (int i = 0; i < z; ++i) {
			unsigned char* frame_pixels = pixels + (i * stride);

			int current_width = x;
			int current_height = y;

			// crazy fix for gifs that are tall as hell boi
			//if (y > x) {
			//    int y_offset = (y - x) / 2;
			//    frame_pixels += (y_offset * x * 4);
			//    current_height = x;
			//}

			// Manual Flip
//#ifdef STABLE
//            utils::flip_image_vertically(frame_pixels, current_width, current_height);
//#endif

			if (!create_texture_from_pixels(frame_pixels, current_width, current_height, &m_frames[i].m_srv)) {
				stbi_image_free(pixels);
				free(delays);
				cleanup();
				return false;
			}

			m_frames[i].m_width = current_width;
			m_frames[i].m_height = current_height;
			m_frames[i].m_delay_ms = delays[i];
		}

		stbi_image_free(pixels);
		free(delays);

		m_last_frame_time = std::chrono::high_resolution_clock::now();
		return true;

	}
	else {
		int x, y, channels;
		unsigned char* pixels = stbi_load_from_memory(buffer, (int)size, &x, &y, &channels, 4);

		if (!pixels) {
			return false;
		}

		// Manual Flip
//#ifdef STABLE
//        utils::flip_image_vertically(pixels, x, y);
//#endif

		m_frames.resize(1);
		if (create_texture_from_pixels(pixels, x, y, &m_frames[0].m_srv)) {
			m_frames[0].m_width = x;
			m_frames[0].m_height = y;
			m_frames[0].m_delay_ms = 0;
			stbi_image_free(pixels);
			return true;
		}

		stbi_image_free(pixels);
		return false;
	}
}

void texture_t::update()
{
	if (!m_is_animated || m_frames.size() <= 1)
		return;

	auto now = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_frame_time).count();

	if (elapsed >= m_frames[m_current_frame_idx].m_delay_ms) {
		m_current_frame_idx = (m_current_frame_idx + 1) % m_frames.size();
		m_last_frame_time = now;
	}
}

ID3D11ShaderResourceView* texture_t::get_srv() const
{
	if (m_frames.empty())
		return nullptr;
	return m_frames[m_current_frame_idx].m_srv;
}

int texture_t::get_width() const
{
	if (m_frames.empty())
		return 0;
	return m_frames[m_current_frame_idx].m_width;
}

int texture_t::get_height() const
{
	if (m_frames.empty())
		return 0;
	return m_frames[m_current_frame_idx].m_height;
}

bool texture_t::is_valid() const
{
	return !m_frames.empty();
}
