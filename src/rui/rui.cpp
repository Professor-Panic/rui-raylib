#include "rui.h"

#include <fstream>
#include <sstream>

namespace {

bool ReadRuiTextFile(const std::string& path, std::string& output) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    output = buffer.str();
    return true;
}

} // namespace

namespace rui {

bool Context::LoadStyleString(const std::string& source) {
    styles = ParseStyleSheet(source);
    MarkLayoutDirty(ui.root,true);
    return true;
}

bool Context::LoadStyleFile(const std::string& path) {
    std::string source;
    if(!ReadRuiTextFile(path,source)) return false;
    return LoadStyleString(source);
}

bool Context::LoadMarkupString(
    const std::string& source,
    std::string source_name
) {
    if(!runtime.LoadString(source,std::move(source_name))) return false;
    runtime.Rebuild(ui.root,build_options_);
    rebuild_requested_ = false;
    return true;
}

bool Context::LoadMarkupFile(
    const std::string& path,
    const UIXBuildOptions& options
) {
    build_options_ = options;
    rebuild_requested_ = false;
    return runtime.Mount(path,ui.root,build_options_);
}

void Context::SetBuildOptions(UIXBuildOptions options) {
    build_options_ = options;
}

void Context::RequestRebuild() {
    rebuild_requested_ = true;
}

void Context::Rebuild() {
    runtime.Rebuild(ui.root,build_options_);
    rebuild_requested_ = false;
}

void Context::Update(const FrameOptions& options) {
    if(auto_rebuild || rebuild_requested_ || options.rebuild_markup) Rebuild();
    UpdateUIFrame(
        ui.root,
        styles,
        options.width,
        options.height,
        options.force_layout
    );
    runtime.Update(ui.root);
}

void Context::Draw() const {
    DrawElements(
        const_cast<UIContainer&>(ui.root),
        ui.fonts,
        styles
    );
}

} // namespace rui
