#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <chrono>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;

struct texture_frame_t {
    ID3D11ShaderResourceView* m_srv = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_delay_ms = 0;
};

class texture_t {
public:
    texture_t() = default;
    ~texture_t();

    bool load_from_file(const std::string& path);
    bool load_from_memory(const void* data, size_t size);

    void update();
    ID3D11ShaderResourceView* get_srv() const;
    int get_width() const;
    int get_height() const;
    bool is_valid() const;
    void release();

private:
    void cleanup();

    std::vector<texture_frame_t> m_frames;
    size_t m_current_frame_idx = 0;
    std::chrono::steady_clock::time_point m_last_frame_time;
    bool m_is_animated = false;
};
