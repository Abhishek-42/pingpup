#include "gui.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <string>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ── DX11 globals ─────────────────────────────────────────────────────────────
static ID3D11Device*           g_device           = nullptr;
static ID3D11DeviceContext*    g_ctx               = nullptr;
static IDXGISwapChain*         g_swap_chain        = nullptr;
static ID3D11RenderTargetView* g_render_target     = nullptr;
static HWND                    g_hwnd              = nullptr;

static void create_render_target() {
    ID3D11Texture2D* back = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_render_target);
    back->Release();
}

static void cleanup_render_target() {
    if (g_render_target) { g_render_target->Release(); g_render_target = nullptr; }
}

static bool create_dx11(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL lvl;
    const D3D_FEATURE_LEVEL lvls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                      lvls, 2, D3D11_SDK_VERSION, &sd,
                                      &g_swap_chain, &g_device, &lvl, &g_ctx) != S_OK)
        return false;
    create_render_target();
    return true;
}

static void cleanup_dx11() {
    cleanup_render_target();
    if (g_swap_chain) { g_swap_chain->Release(); g_swap_chain = nullptr; }
    if (g_ctx)        { g_ctx->Release();        g_ctx        = nullptr; }
    if (g_device)     { g_device->Release();     g_device     = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wp != SIZE_MINIMIZED) {
            cleanup_render_target();
            g_swap_chain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp),
                                        DXGI_FORMAT_UNKNOWN, 0);
            create_render_target();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Style ─────────────────────────────────────────────────────────────────────
static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 10.0f;
    s.FrameRounding     = 6.0f;
    s.GrabRounding      = 6.0f;
    s.ItemSpacing       = ImVec2(10, 8);
    s.WindowPadding     = ImVec2(20, 20);
    s.FramePadding      = ImVec2(10, 6);
    s.ScrollbarSize     = 10.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.24f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_SliderGrab]       = ImVec4(0.38f, 0.62f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Button]           = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.30f, 0.30f, 0.38f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.38f, 0.62f, 1.00f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.28f, 0.28f, 0.36f, 1.00f);
    c[ImGuiCol_TitleBg]          = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_Text]             = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
    c[ImGuiCol_Separator]        = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.24f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static void status_dot(bool active) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  r = 5.0f;
    ImU32  col = active
        ? IM_COL32(80, 220, 120, 255)
        : IM_COL32(80,  80, 100, 255);
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(p.x + r, p.y + ImGui::GetTextLineHeight() * 0.5f), r, col);
    ImGui::Dummy(ImVec2(r * 2 + 6, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
}

static void buf_bar(const char* label, int pct) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(110);
    char overlay[16];
    snprintf(overlay, sizeof(overlay), "%d%%", pct);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.38f, 0.62f, 1.00f, 0.85f));
    ImGui::ProgressBar(pct / 100.0f, ImVec2(-1, 6), overlay);
    ImGui::PopStyleColor();
}

// ── Main GUI loop ─────────────────────────────────────────────────────────────
void run_gui(AppState& state) {
    WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, wnd_proc, 0, 0,
                    GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                    L"LanAudioBridge", nullptr };
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName,
                              L"LAN Audio Bridge",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              520, 560,
                              nullptr, nullptr, wc.hInstance, nullptr);

    if (!create_dx11(g_hwnd)) { DestroyWindow(g_hwnd); return; }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini file
    io.Fonts->AddFontDefault();

    apply_style();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    const ImVec4 clear_color(0.08f, 0.08f, 0.10f, 1.00f);

    float phone_vol  = state.phone_vol.load();
    float system_vol = state.system_vol.load();

    MSG msg{};
    while (state.running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) { state.running = false; }
        }
        if (!state.running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Full-screen window (no title bar, no resize handle — just the DX11 window chrome)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize   |
                     ImGuiWindowFlags_NoMove     |
                     ImGuiWindowFlags_NoScrollbar);

        // ── Title ──────────────────────────────────────────────────────────
        ImGui::SetCursorPosX((io.DisplaySize.x - ImGui::CalcTextSize("LAN Audio Bridge").x) * 0.5f);
        ImGui::TextColored(ImVec4(0.38f, 0.62f, 1.00f, 1.00f), "LAN Audio Bridge");
        ImGui::SetCursorPosX((io.DisplaySize.x - ImGui::CalcTextSize("& Mixer").x) * 0.5f);
        ImGui::TextDisabled("& Mixer");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ── Sources ────────────────────────────────────────────────────────
        ImGui::BeginChild("##sources", ImVec2(0, 80), true);
        ImGui::TextDisabled("SOURCES");
        ImGui::Spacing();

        status_dot(state.phone_active);
        ImGui::Text("Phone Audio");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (state.phone_active)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "ACTIVE");
        else
            ImGui::TextDisabled("waiting");

        status_dot(state.system_active);
        ImGui::Text("System Audio");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (state.system_active)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "ON");
        else
            ImGui::TextDisabled("off");

        ImGui::EndChild();
        ImGui::Spacing();

        // ── Volume Controls ────────────────────────────────────────────────
        ImGui::BeginChild("##volumes", ImVec2(0, 110), true);
        ImGui::TextDisabled("VOLUME");
        ImGui::Spacing();

        ImGui::Text("Phone ");
        ImGui::SameLine(80);
        ImGui::PushItemWidth(-60);
        if (ImGui::SliderFloat("##pvol", &phone_vol, 0.0f, 2.0f, "%.2f")) {
            state.phone_vol.store(phone_vol);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::TextDisabled("%.2f", phone_vol);

        ImGui::Spacing();

        ImGui::Text("System");
        ImGui::SameLine(80);
        ImGui::PushItemWidth(-60);
        if (ImGui::SliderFloat("##svol", &system_vol, 0.0f, 2.0f, "%.2f")) {
            state.system_vol.store(system_vol);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::TextDisabled("%.2f", system_vol);

        ImGui::EndChild();
        ImGui::Spacing();

        // ── Stats ──────────────────────────────────────────────────────────
        ImGui::BeginChild("##stats", ImVec2(0, 110), true);
        ImGui::TextDisabled("STATS");
        ImGui::Spacing();

        float lat = state.latency_ms.load();
        ImGui::Text("Latency");
        ImGui::SameLine(110);
        ImGui::TextColored(
            lat < 30.0f ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f) : ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
            "~%.0f ms", lat);

        ImGui::Spacing();
        buf_bar("Phone buf",  state.phone_buf_pct.load());
        buf_bar("System buf", state.system_buf_pct.load());
        buf_bar("Output buf", state.output_buf_pct.load());

        ImGui::EndChild();
        ImGui::Spacing();

        // ── Log ────────────────────────────────────────────────────────────
        ImGui::BeginChild("##log", ImVec2(0, 120), true);
        ImGui::TextDisabled("LOG");
        ImGui::Spacing();
        {
            std::lock_guard<std::mutex> lk(state.log_mutex);
            for (auto& line : state.log_lines)
                ImGui::TextDisabled("%s", line.c_str());
        }
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::Spacing();

        // ── Stop Button ────────────────────────────────────────────────────
        float btn_w = 120.0f;
        ImGui::SetCursorPosX((io.DisplaySize.x - btn_w) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f, 0.30f, 0.30f, 1.00f));
        if (ImGui::Button("  STOP  ", ImVec2(btn_w, 36)))
            state.running = false;
        ImGui::PopStyleColor(3);

        ImGui::End();

        // ── Render ─────────────────────────────────────────────────────────
        ImGui::Render();
        const float cc[4] = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        g_ctx->OMSetRenderTargets(1, &g_render_target, nullptr);
        g_ctx->ClearRenderTargetView(g_render_target, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);  // vsync on (1) — no need to burn CPU
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_dx11();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}
