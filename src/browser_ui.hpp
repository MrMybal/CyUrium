// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_view.hpp"
#include "imgui.h"
#include <array>
#include <string>

namespace cyurium {
class BrowserUi final : public BrowserView {
public:
    UiActions draw(BrowserModel& model,const ViewFrame& frame) override;
    UiActions draw(BrowserModel& model, float dpi, bool maximized);
    void focus_address() override { focus_address_ = true; }
    void show_menu() override { menu_requested_ = true; }
    bool editing() const override { return editing_; }
    void draw_drag_preview(const TabDragVisual&,std::string_view,float scale) override;
private:
    ViewFrame frame_;
    ImVec2 origin_{};
    std::array<char, 4096> address_{};
    std::array<char, 4096> search_{};
    std::uint64_t last_tab_ = 0;
    std::string last_url_;
    bool focus_address_ = false, editing_ = false, menu_requested_ = false;
    bool assistant_settings_=false,assistant_loaded_=false;
    float assistant_width_=400;
    std::string assistant_conversation_,assistant_connector_="codex",assistant_transport_="app-server";
    std::string assistant_last_conversation_;
    std::map<std::string,std::string> assistant_drafts_;
    std::array<char,32769> assistant_draft_{};
    std::array<char,257> assistant_model_{};
    std::array<char,2049> assistant_endpoint_{},assistant_executable_{};
    std::array<char,129> assistant_key_env_{};
    int settings_category_ = 0;
    std::array<char,4096> extension_path_{};
    std::array<char,2048> permission_origin_{};
    int permission_type_=0;
    bool remember_permission_=false;
    std::uint64_t prompt_id_=0;
    std::map<std::string,int> device_selection_;
    float scale_ = 1;
    UiActions actions_;
    BrowserModel* model_ = nullptr;
    void tabs(float width, bool maximized);
    void toolbar(float width);
    void bookmarks(float width);
    void home(float width, float top, float height);
    void settings(float width, float top, float height);
    void library(Page page, float width, float top, float height);
    void pending(float width, float top, float height);
    void menu(float width);
    void service_settings(int category,float x,float y,float width,float height);
    void service_library(Page,float width,float top,float height);
    void permission_prompt();
    void assistant_panel(float x,float top,float width,float height);
};
}
