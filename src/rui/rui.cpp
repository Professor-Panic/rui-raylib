#include "rui.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <cctype>

namespace {

bool ReadRuiTextFile(const std::string& path, std::string& output) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    output = buffer.str();
    return true;
}

std::string TrimRuiText(std::string value) {
    std::size_t first = 0;
    std::size_t last = value.size();
    while(first < last && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    while(last > first && std::isspace(static_cast<unsigned char>(value[last-1]))) --last;
    return value.substr(first,last-first);
}

bool ParseRuiImportTarget(const std::string& statement, std::string& target) {
    std::string value = TrimRuiText(statement);
    if(value.empty()) return false;

    auto strip_quotes = [](std::string text) -> std::optional<std::string> {
        text = TrimRuiText(std::move(text));
        if(text.size() < 2) return std::nullopt;
        const char first = text.front();
        const char last = text.back();
        if(!((first == '"' && last == '"') || (first == '\'' && last == '\''))) {
            return std::nullopt;
        }
        return text.substr(1,text.size()-2);
    };

    std::string lower = value;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if(lower.rfind("url(",0) == 0 && value.back() == ')') {
        std::string inside = TrimRuiText(value.substr(4,value.size()-5));
        if(auto quoted = strip_quotes(inside)) target = *quoted;
        else target = inside;
        return !target.empty();
    }

    if(auto quoted = strip_quotes(value)) {
        target = *quoted;
        return !target.empty();
    }
    return false;
}

bool ExpandRuiStyleImports(
    const std::filesystem::path& file_path,
    std::string& output,
    std::unordered_set<std::string>& active_files,
    int depth
) {
    if(depth > 64) {
        TraceLog(LOG_WARNING,"RCSS @import nesting exceeded 64 files");
        return false;
    }

    const std::filesystem::path normalized = file_path.lexically_normal();
    const std::string key = normalized.generic_string();
    if(active_files.find(key) != active_files.end()) {
        TraceLog(LOG_WARNING,"RCSS @import cycle detected at: %s",key.c_str());
        return false;
    }

    std::string source;
    if(!ReadRuiTextFile(normalized.string(),source)) {
        TraceLog(LOG_WARNING,"Unable to load RCSS file: %s",key.c_str());
        return false;
    }

    active_files.insert(key);
    std::string expanded;
    expanded.reserve(source.size());

    int brace_depth = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool in_comment = false;

    for(std::size_t index=0; index<source.size();) {
        if(in_comment) {
            if(index+1 < source.size() && source[index] == '*' && source[index+1] == '/') {
                expanded.append("*/");
                index += 2;
                in_comment = false;
            }
            else expanded.push_back(source[index++]);
            continue;
        }

        if(!in_single_quote && !in_double_quote && index+1 < source.size() &&
           source[index] == '/' && source[index+1] == '*') {
            in_comment = true;
            expanded.append("/*");
            index += 2;
            continue;
        }

        const char character = source[index];
        if(character == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if(character == '"' && !in_single_quote) in_double_quote = !in_double_quote;

        if(!in_single_quote && !in_double_quote) {
            if(character == '{') ++brace_depth;
            else if(character == '}') brace_depth = std::max(0,brace_depth-1);

            if(brace_depth == 0 && character == '@' && index+7 <= source.size()) {
                std::string keyword = source.substr(index,7);
                std::transform(keyword.begin(),keyword.end(),keyword.begin(),[](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if(keyword == "@import") {
                    std::size_t end = index+7;
                    bool statement_single = false;
                    bool statement_double = false;
                    int paren_depth = 0;
                    for(; end<source.size(); ++end) {
                        const char c = source[end];
                        if(c == '\'' && !statement_double) statement_single = !statement_single;
                        else if(c == '"' && !statement_single) statement_double = !statement_double;
                        else if(!statement_single && !statement_double) {
                            if(c == '(') ++paren_depth;
                            else if(c == ')') paren_depth = std::max(0,paren_depth-1);
                            else if(c == ';' && paren_depth == 0) break;
                        }
                    }
                    if(end >= source.size()) {
                        TraceLog(LOG_WARNING,"RCSS @import is missing ';' in: %s",key.c_str());
                        active_files.erase(key);
                        return false;
                    }

                    std::string import_target;
                    if(!ParseRuiImportTarget(source.substr(index+7,end-index-7),import_target)) {
                        TraceLog(LOG_WARNING,"Invalid RCSS @import in: %s",key.c_str());
                        active_files.erase(key);
                        return false;
                    }

                    std::filesystem::path imported_path(import_target);
                    if(imported_path.is_relative()) imported_path = normalized.parent_path()/imported_path;
                    std::string imported_source;
                    if(!ExpandRuiStyleImports(imported_path,imported_source,active_files,depth+1)) {
                        active_files.erase(key);
                        return false;
                    }
                    expanded += imported_source;
                    if(!expanded.empty() && expanded.back() != '\n') expanded.push_back('\n');
                    index = end+1;
                    continue;
                }
            }
        }

        expanded.push_back(character);
        ++index;
    }

    active_files.erase(key);
    output = std::move(expanded);
    return true;
}

bool ReadRuiStyleFileWithImports(const std::string& path, std::string& output) {
    std::unordered_set<std::string> active_files;
    return ExpandRuiStyleImports(std::filesystem::path(path),output,active_files,0);
}

struct RuiFocusLookup {
    UIElementBase* element = nullptr;
    const CSS_Id* id = nullptr;
};

RuiFocusLookup FindRuiFocusedElement(UIContainer& container);

RuiFocusLookup FindRuiFocusedDockNode(
    UIDockSpace& dock,
    const std::shared_ptr<UIDockNode>& node
) {
    if(!node) return {};
    if(node->type == UIDockNodeType::Split) {
        if(auto found = FindRuiFocusedDockNode(dock,node->first); found.element) {
            return found;
        }
        return FindRuiFocusedDockNode(dock,node->second);
    }

    if(node->tabs.empty()) return {};
    const int active = std::clamp(
        node->active_tab,
        0,
        static_cast<int>(node->tabs.size())-1
    );
    UIDockPanel* panel = dock.FindPanel(node->tabs[static_cast<std::size_t>(active)]);
    if(panel == nullptr || !panel->open || panel->floating) return {};
    return FindRuiFocusedElement(panel->content);
}

RuiFocusLookup FindRuiFocusedElement(UIContainer& container) {
    if(container.style.display == DisplayType::None) return {};

    for(UIElement& element : container.elements) {
        RuiFocusLookup found = std::visit([&](auto& widget) -> RuiFocusLookup {
            using T = std::decay_t<decltype(widget)>;

            if constexpr(
                std::is_same_v<T,UIButton> ||
                IsUIInputV<T> ||
                std::is_same_v<T,UISelectable> ||
                std::is_same_v<T,UIComboBox> ||
                IsUIRangeInputV<T> ||
                std::is_same_v<T,UICanvas> ||
                std::is_same_v<T,UIImage>
            ) {
                return widget.focused
                    ? RuiFocusLookup{&widget.rect,&widget.style.id}
                    : RuiFocusLookup{};
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget || widget->style.display == DisplayType::None) return {};

                // Prefer a focused retained descendant over its composite wrapper.
                if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    if(auto child = FindRuiFocusedElement(viewport->content); child.element) {
                        return child;
                    }
                }
                else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    if(item->visible) {
                        if(auto child = FindRuiFocusedElement(item->content); child.element) {
                            return child;
                        }
                    }
                }
                else if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(auto it = dock->panels.rbegin(); it != dock->panels.rend(); ++it) {
                        if(!it->open || !it->floating) continue;
                        if(auto child = FindRuiFocusedElement(it->content); child.element) {
                            return child;
                        }
                    }
                    if(auto child = FindRuiFocusedDockNode(*dock,dock->root); child.element) {
                        return child;
                    }
                }

                bool* focused = widget->FocusFlag();
                return focused != nullptr && *focused
                    ? RuiFocusLookup{&widget->rect,&widget->style.id}
                    : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget ? FindRuiFocusedElement(*widget) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                return widget && widget->open
                    ? FindRuiFocusedElement(widget->content) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                return widget && widget->open
                    ? FindRuiFocusedElement(widget->content) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                return widget && widget->open
                    ? FindRuiFocusedElement(widget->content) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                return widget && widget->open
                    ? FindRuiFocusedElement(widget->content) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                return widget && widget->open && !widget->collapsed
                    ? FindRuiFocusedElement(widget->content) : RuiFocusLookup{};
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return {};
                if(UITab* tab = widget->SelectedTab()) {
                    return FindRuiFocusedElement(tab->content);
                }
                return {};
            }
            else {
                return {};
            }
        },element);

        if(found.element != nullptr) return found;
    }

    return {};
}

bool RuiContainerHasPointerCapture(UIContainer& container);

bool RuiDockHasPointerCapture(UIDockSpace& dock) {
    if(!dock.dragging_panel.empty()) return true;
    for(UIDockPanel& panel : dock.panels) {
        if(panel.dragging || panel.resizing) return true;
        if(panel.open && RuiContainerHasPointerCapture(panel.content)) return true;
    }
    return false;
}

bool RuiContainerHasPointerCapture(UIContainer& container) {
    if(
        container.dragging_horizontal_scrollbar ||
        container.dragging_vertical_scrollbar
    ) return true;

    for(UIElement& element : container.elements) {
        const bool captured = std::visit([&](auto& widget) -> bool {
            using T = std::decay_t<decltype(widget)>;

            if constexpr(std::is_same_v<T,UIButton>) {
                return widget.active || widget.pressed_inside;
            }
            else if constexpr(IsUIInputV<T>) {
                return widget.active || widget.dragging || widget.pressed_inside ||
                    widget.picker_open || widget.text_mouse_selecting;
            }
            else if constexpr(std::is_same_v<T,UISelectable>) {
                return widget.active || widget.pressed_inside;
            }
            else if constexpr(std::is_same_v<T,UIComboBox>) {
                return widget.open || widget.active || widget.pressed_inside;
            }
            else if constexpr(IsUIRangeInputV<T>) {
                return widget.active_handle != 0;
            }
            else if constexpr(std::is_same_v<T,UICanvas>) {
                if(widget.dragging || widget.pressed) return true;
                return std::any_of(
                    widget.mouse_pressed_inside.begin(),
                    widget.mouse_pressed_inside.end(),
                    [](bool value) { return value; }
                );
            }
            else if constexpr(std::is_same_v<T,UIImage>) {
                return widget.interactive && widget.pressed;
            }
            else if constexpr(std::is_same_v<T,UISplitter>) {
                return widget.dragging;
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return false;
                if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    return viewport->panning || RuiContainerHasPointerCapture(viewport->content);
                }
                if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    return item->visible && RuiContainerHasPointerCapture(item->content);
                }
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    return RuiDockHasPointerCapture(*dock);
                }
                if(auto* text_box = dynamic_cast<UITextBox*>(widget.get())) {
                    return text_box->mouse_selecting;
                }
                if(auto* table = dynamic_cast<UITable*>(widget.get())) {
                    return table->resizing_column >= 0;
                }
                return false;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && RuiContainerHasPointerCapture(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                return widget && widget->open && RuiContainerHasPointerCapture(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                return widget && widget->open && RuiContainerHasPointerCapture(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                return widget && widget->open && (
                    widget->dragging || widget->resizing ||
                    (!widget->collapsed && RuiContainerHasPointerCapture(widget->content))
                );
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return false;
                if(UITab* tab = widget->SelectedTab()) {
                    return RuiContainerHasPointerCapture(tab->content);
                }
                return false;
            }
            else {
                return false;
            }
        },element);
        if(captured) return true;
    }

    return false;
}

bool RuiContainerHasMouseInterest(UIContainer& container) {
    if(RuiContainerHasPointerCapture(container)) return true;

    for(UIElement& element : container.elements) {
        const bool interested = std::visit([&](auto& widget) -> bool {
            using T = std::decay_t<decltype(widget)>;

            if constexpr(
                std::is_same_v<T,UIButton> ||
                IsUIInputV<T> ||
                std::is_same_v<T,UISelectable> ||
                std::is_same_v<T,UIComboBox> ||
                IsUIRangeInputV<T> ||
                std::is_same_v<T,UICanvas> ||
                std::is_same_v<T,UISplitter>
            ) {
                return widget.hovered;
            }
            else if constexpr(std::is_same_v<T,UIImage>) {
                return widget.interactive && widget.hovered;
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return false;
                if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    return viewport->hovered || RuiContainerHasMouseInterest(viewport->content);
                }
                if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    return item->visible && RuiContainerHasMouseInterest(item->content);
                }
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    return RuiDockHasPointerCapture(*dock);
                }
                if(auto* text_box = dynamic_cast<UITextBox*>(widget.get())) {
                    return text_box->hovered;
                }
                if(auto* list = dynamic_cast<UIVirtualList*>(widget.get())) {
                    return list->hovered_index >= 0;
                }
                if(auto* table = dynamic_cast<UITable*>(widget.get())) {
                    return table->hovered_row >= 0 || table->resizing_column >= 0;
                }
                return false;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && RuiContainerHasMouseInterest(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                return widget && widget->open && RuiContainerHasMouseInterest(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                return widget && widget->open && RuiContainerHasMouseInterest(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                return widget && widget->open && !widget->collapsed &&
                    RuiContainerHasMouseInterest(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return false;
                if(UITab* tab = widget->SelectedTab()) {
                    return RuiContainerHasMouseInterest(tab->content);
                }
                return false;
            }
            else {
                return false;
            }
        },element);
        if(interested) return true;
    }

    return false;
}

bool RuiContainerWantsKeyboard(UIContainer& container) {
    if(container.style.display == DisplayType::None) return false;

    for(UIElement& element : container.elements) {
        const bool wants_keyboard = std::visit([&](auto& widget) -> bool {
            using T = std::decay_t<decltype(widget)>;

            if constexpr(IsUIInputV<T>) {
                // Sliders, triggers and checkboxes may retain focus after a click,
                // but they do not own general keyboard input. Instant inputs are
                // editable fields, so focused Instant inputs do own the keyboard.
                return widget.picker_open ||
                    (widget.focused && widget.input_type == InputType::Instant);
            }
            else if constexpr(std::is_same_v<T,UIComboBox>) {
                // A closed combo can remain focused without blocking gameplay keys.
                return widget.open;
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget || widget->style.display == DisplayType::None) return false;

                if(auto* text_box = dynamic_cast<UITextBox*>(widget.get())) {
                    return text_box->editable && !text_box->disabled && text_box->focused;
                }
                if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    return RuiContainerWantsKeyboard(viewport->content);
                }
                if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    return item->visible && RuiContainerWantsKeyboard(item->content);
                }
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(panel.open && RuiContainerWantsKeyboard(panel.content)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && RuiContainerWantsKeyboard(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                return widget && widget->open &&
                    RuiContainerWantsKeyboard(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                return widget && widget->open &&
                    RuiContainerWantsKeyboard(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                return widget && widget->open && !widget->collapsed &&
                    RuiContainerWantsKeyboard(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return false;
                if(UITab* tab = widget->SelectedTab()) {
                    return RuiContainerWantsKeyboard(tab->content);
                }
                return false;
            }
            else {
                return false;
            }
        },element);

        if(wants_keyboard) return true;
    }

    return false;
}

bool RuiContainerHasKeyboardOverlay(UIContainer& container) {
    for(UIElement& element : container.elements) {
        const bool captured = std::visit([&](auto& widget) -> bool {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(std::is_same_v<T,UIComboBox>) {
                return widget.open;
            }
            else if constexpr(IsUIInputV<T>) {
                return widget.picker_open;
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return false;
                if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    return RuiContainerHasKeyboardOverlay(viewport->content);
                }
                if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    return item->visible && RuiContainerHasKeyboardOverlay(item->content);
                }
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(panel.open && RuiContainerHasKeyboardOverlay(panel.content)) return true;
                    }
                }
                return false;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && RuiContainerHasKeyboardOverlay(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                return widget && widget->open && RuiContainerHasKeyboardOverlay(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                return widget && widget->open && RuiContainerHasKeyboardOverlay(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                return widget && widget->open;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                return widget && widget->open && !widget->collapsed &&
                    RuiContainerHasKeyboardOverlay(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return false;
                if(UITab* tab = widget->SelectedTab()) {
                    return RuiContainerHasKeyboardOverlay(tab->content);
                }
                return false;
            }
            else {
                return false;
            }
        },element);
        if(captured) return true;
    }
    return false;
}

} // namespace

namespace rui {

bool Context::LoadStyleString(const std::string& source) {
    style_source_ = source;
    RefreshStyles();
    return true;
}

void Context::RefreshStyles() {
    std::string combined = style_source_;
    const std::string& embedded = runtime.EmbeddedStyleSource();
    if(!combined.empty() && !embedded.empty()) combined.push_back('\n');
    combined += embedded;
    styles = ParseStyleSheet(combined);
    MarkLayoutDirty(ui.root,true);
}

bool Context::LoadStyleFile(const std::string& path) {
    std::string source;
    if(!ReadRuiStyleFileWithImports(path,source)) return false;
    return LoadStyleString(source);
}

bool Context::AppendStyleFile(const std::string& path) {
    std::string source;
    if(!ReadRuiStyleFileWithImports(path,source)) return false;
    if(!style_source_.empty() && !source.empty()) style_source_.push_back('\n');
    style_source_ += source;
    RefreshStyles();
    return true;
}

bool Context::PrependStyleFile(const std::string& path) {
    std::string source;
    if(!ReadRuiStyleFileWithImports(path,source)) return false;
    if(!source.empty() && !style_source_.empty()) source.push_back('\n');
    style_source_ = source+style_source_;
    RefreshStyles();
    return true;
}

bool Context::LoadMarkupString(
    const std::string& source,
    std::string source_name
) {
    if(!runtime.LoadString(source,std::move(source_name))) return false;
    runtime.Rebuild(ui.root,build_options_);
    RefreshStyles();
    rebuild_requested_ = false;
    return true;
}

bool Context::LoadMarkupFile(
    const std::string& path,
    const UIXBuildOptions& options
) {
    build_options_ = options;
    rebuild_requested_ = false;
    const bool loaded = runtime.Mount(path,ui.root,build_options_);
    if(loaded) RefreshStyles();
    return loaded;
}

void Context::SetBuildOptions(UIXBuildOptions options) {
    build_options_ = options;
}

void Context::RequestRebuild() {
    rebuild_requested_ = true;
}

void Context::Rebuild() {
    runtime.Rebuild(ui.root,build_options_);
    RefreshStyles();
    rebuild_requested_ = false;
}

void Context::Update(const FrameOptions& options) {
    if(auto_rebuild || rebuild_requested_ || options.rebuild_markup ||
       runtime.RebuildRequested()) Rebuild();
    UpdateUIFrame(
        ui.root,
        styles,
        options.width,
        options.height,
        options.force_layout,
        options.detect_layout_mutations
    );
    runtime.Update(ui.root);
    DispatchFocusEvents();
    DispatchKeyEvents();
}

void Context::DispatchKeyEvents() {
    UIElementBase* focused = FindRuiFocusedElement(ui.root).element;
    if(focused == nullptr || !focused->on_key) return;
    // Copy first: the handler may change state that rebuilds the tree.
    const std::function<bool()> handler = focused->on_key;
    handler();
}

void Context::DispatchFocusEvents() {
    const UIElementBase* focused = FindRuiFocusedElement(ui.root).element;
    const std::string key = focused != nullptr ? focused->key : std::string{};
    if(key == focus_key_) return;

    const std::string previous = std::move(focus_key_);
    focus_key_ = key;

    // Copy both callbacks before running either: a handler may rebuild the UI
    // and destroy the widgets they came from.
    std::function<void()> blur;
    std::function<void()> focus;
    if(!previous.empty()) {
        if(const UIElementBase* old = FindUIElementByKey(ui.root,previous)) blur = old->on_blur;
    }
    if(focused != nullptr) focus = focused->on_focus;

    if(blur) blur();
    if(focus) focus();
}

void Context::Draw() const {
    DrawElements(
        const_cast<UIContainer&>(ui.root),
        ui.fonts,
        styles
    );
}

bool Context::HasFocus() const {
    return HasKeyboardFocus();
}

bool Context::HasKeyboardFocus() const {
    return FindRuiFocusedElement(const_cast<UIContainer&>(ui.root)).element != nullptr;
}

bool Context::HasPointerCapture() const {
    return CSS_UI_POINTER_INPUT_CONSUMED ||
        GetUIDragDropContext().Dragging() ||
        RuiContainerHasPointerCapture(const_cast<UIContainer&>(ui.root));
}

bool Context::WantsKeyboard() const {
    return RuiContainerWantsKeyboard(const_cast<UIContainer&>(ui.root)) ||
        RuiContainerHasKeyboardOverlay(const_cast<UIContainer&>(ui.root));
}

bool Context::WantsMouse() const {
    return HasPointerCapture() ||
        CSS_UI_SCROLL_INPUT_CONSUMED ||
        RuiContainerHasMouseInterest(const_cast<UIContainer&>(ui.root));
}

UIElementBase* Context::FocusedElement() {
    return FindRuiFocusedElement(ui.root).element;
}

const UIElementBase* Context::FocusedElement() const {
    return FindRuiFocusedElement(const_cast<UIContainer&>(ui.root)).element;
}

std::string Context::FocusedKey() const {
    const UIElementBase* focused =
        FindRuiFocusedElement(const_cast<UIContainer&>(ui.root)).element;
    return focused != nullptr ? focused->key : std::string{};
}

UIXValue Context::FocusedUserData() const {
    const UIElementBase* focused =
        FindRuiFocusedElement(const_cast<UIContainer&>(ui.root)).element;
    if(focused == nullptr) return {};
    if(const auto* value = std::any_cast<UIXValue>(&focused->user_data)) return *value;
    return {};
}

std::string Context::FocusedId() const {
    const RuiFocusLookup focused =
        FindRuiFocusedElement(const_cast<UIContainer&>(ui.root));

    if(focused.id == nullptr) return {};
    if(const auto* text = std::get_if<std::string>(focused.id)) return *text;
    if(const auto* number = std::get_if<int>(focused.id)) return std::to_string(*number);
    return {};
}

} // namespace rui
// ---------------------------------------------------------------------------
// Descriptor-driven custom RCSS property overlay (RUI 0.9.1 patch)
// ---------------------------------------------------------------------------

void UIComponent::BeginRegisteredStyleOverrides(const ComputedStyle& resolved_style) {
    registered_style_backups_.clear();
    if(!descriptor_ || !registry_ || !resolved_style.component_style_properties) return;

    const auto properties=registry_->GetProperties(*descriptor_);
    registered_style_backups_.reserve(properties.size());

    for(const UIPropertyDescriptor* property:properties) {
        if(!property || property->style_name.empty() ||
           !property->component_getter || !property->component_setter) continue;

        const std::string style_name=LowerCSS(TrimCSS(property->style_name));
        auto found=resolved_style.component_style_properties->find(style_name);
        if(found==resolved_style.component_style_properties->end()) continue;

        std::string backup_error;
        const UIPropertyValue current=property->component_getter(*this);
        auto backup=FormatUIPropertyValue(current,*registry_,&backup_error);
        if(!backup) continue;

        std::string conversion_error;
        auto converted=ConvertUIPropertyText(found->second,*property,*registry_,&conversion_error);
        if(!converted) {
            TraceLog(
                LOG_WARNING,
                "Invalid RCSS component property '%s: %s' on %s: %s",
                property->style_name.c_str(),found->second.c_str(),
                component_name_.c_str(),conversion_error.c_str()
            );
            continue;
        }

        UIPropertyValue normalized;
        if(!property->ValidateValue(*converted,&normalized,&conversion_error)) {
            TraceLog(
                LOG_WARNING,
                "Rejected RCSS component property '%s' on %s: %s",
                property->style_name.c_str(),component_name_.c_str(),conversion_error.c_str()
            );
            continue;
        }

        std::string setter_error;
        if(!property->component_setter(*this,normalized,setter_error)) {
            TraceLog(
                LOG_WARNING,
                "Unable to apply RCSS component property '%s' on %s: %s",
                property->style_name.c_str(),component_name_.c_str(),setter_error.c_str()
            );
            continue;
        }

        registered_style_backups_.push_back({property->name,std::move(*backup)});
    }
}

void UIComponent::EndRegisteredStyleOverrides() {
    if(!descriptor_ || !registry_ || registered_style_backups_.empty()) return;

    // Restore in reverse order in case a future descriptor deliberately aliases
    // more than one RCSS name onto related underlying state.
    for(auto iterator=registered_style_backups_.rbegin();
        iterator!=registered_style_backups_.rend();++iterator) {
        const UIPropertyDescriptor* property=registry_->FindProperty(*descriptor_,iterator->first);
        if(!property || !property->component_setter) continue;

        std::string error;
        auto converted=ConvertUIPropertyText(iterator->second,*property,*registry_,&error);
        if(!converted) continue;
        UIPropertyValue normalized;
        if(!property->ValidateValue(*converted,&normalized,&error)) continue;
        property->component_setter(*this,normalized,error);
    }
    registered_style_backups_.clear();
}
