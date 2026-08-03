#include "css_emulator.h"

std::string NormalizeUIPseudoStateName(std::string name) {
    name.erase(
        name.begin(),
        std::find_if(name.begin(),name.end(),[](unsigned char c) {
            return !std::isspace(c) && c != ':';
        })
    );
    while(!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
        name.pop_back();
    }
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

UIPseudoState& UIPseudoStateSet::Define(
    std::string name,
    bool initial_value,
    std::function<void(bool&)> update
) {
    name = NormalizeUIPseudoStateName(std::move(name));
    UIPseudoState& state = states_[name];
    state.active = initial_value;
    state.update = std::move(update);
    state.lifetime = {};
    state.lifetime_tracked = false;
    return state;
}

UIPseudoState& UIPseudoStateSet::DefineTracked(
    std::string name,
    bool initial_value,
    std::weak_ptr<void> lifetime,
    std::function<void(bool&)> update
) {
    name = NormalizeUIPseudoStateName(std::move(name));
    UIPseudoState& state = states_[name];
    state.active = initial_value;
    state.update = std::move(update);
    state.lifetime = std::move(lifetime);
    state.lifetime_tracked = true;
    return state;
}

bool UIPseudoStateSet::Set(const std::string& name, bool value) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    if(normalized.empty()) return false;
    UIPseudoState& state = states_[normalized];
    const bool changed = state.active != value;
    state.active = value;
    return changed;
}

bool UIPseudoStateSet::Toggle(const std::string& name) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    if(normalized.empty()) return false;
    UIPseudoState& state = states_[normalized];
    state.active = !state.active;
    return state.active;
}

bool UIPseudoStateSet::Remove(const std::string& name) {
    return states_.erase(NormalizeUIPseudoStateName(name)) != 0;
}

bool UIPseudoStateSet::Active(const std::string& name) const {
    const auto found = states_.find(NormalizeUIPseudoStateName(name));
    return found != states_.end() && found->second.active;
}

bool UIPseudoStateSet::Contains(const std::string& name) const {
    return states_.find(NormalizeUIPseudoStateName(name)) != states_.end();
}

void UIPseudoStateSet::Clear() {
    states_.clear();
}

void UIPseudoStateSet::Update() {
    for(auto iterator = states_.begin(); iterator != states_.end();) {
        UIPseudoState& state = iterator->second;
        if(state.lifetime_tracked && state.lifetime.expired()) {
            iterator = states_.erase(iterator);
            continue;
        }
        if(state.update) state.update(state.active);
        ++iterator;
    }
}

void UIPseudoStateSet::SyncBuiltIn(
    const std::string& name,
    bool engine_value
) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    UIPseudoState& state = states_[normalized];
    state.active = engine_value;
    if(state.lifetime_tracked && state.lifetime.expired()) {
        states_.erase(normalized);
        states_[normalized].active = engine_value;
        return;
    }
    if(state.update) state.update(state.active);
}

std::vector<std::string> UIPseudoStateSet::ActiveNames() const {
    std::vector<std::string> result;
    result.reserve(states_.size());
    for(const auto& entry : states_) {
        if(entry.second.active) result.push_back(entry.first);
    }
    std::sort(result.begin(),result.end());
    return result;
}

std::size_t UIPseudoStateSet::ActiveHash() const {
    std::size_t seed = 0;
    for(const std::string& name : ActiveNames()) {
        HashCombine(seed,std::hash<std::string>{}(name));
    }
    return seed;
}


std::size_t NextStyleSheetRevision() {
    static std::size_t revision = 1;
    return revision++;
}

void TouchStyleSheet(StyleSheet& style_sheet) {
    style_sheet.revision = NextStyleSheetRevision();
    ClearResolvedStyleCache();
}

StyleSheet MakeDefaultUIStyleSheet() {
    StyleSheet sheet;
    StyleRule body;
    body.selector = SelectorType::Body;
    body.declarations = {
        {PropertyType::Width,CSSLength{100.0f,LengthUnit::ViewWidth}},
        {PropertyType::Height,CSSLength{100.0f,LengthUnit::ViewHeight}},
        {PropertyType::DisplayType,DisplayType::Flex},
        {PropertyType::FlexDirection,FlexDirection::Column},
        {PropertyType::AlignItems,AlignItems::Stretch},
        {PropertyType::Gap,CSSLength{8.0f,LengthUnit::Pixels}},
        {PropertyType::Padding,CSSLength{8.0f,LengthUnit::Pixels}},
        {PropertyType::BackgroundColor,Color{32,34,40,255}},
        {PropertyType::TextColor,Color{235,237,242,255}}
    };

    StyleRule container;
    container.selector = SelectorType::Container;
    container.declarations = {
        {PropertyType::Width,CSSLength{100.0f,LengthUnit::Percent}},
        {PropertyType::Height,CSSLength{120.0f,LengthUnit::Pixels}},
        {PropertyType::DisplayType,DisplayType::Flex},
        {PropertyType::FlexDirection,FlexDirection::Column},
        {PropertyType::AlignItems,AlignItems::Stretch},
        {PropertyType::Gap,CSSLength{8.0f,LengthUnit::Pixels}}
    };

    StyleRule button;
    button.selector = SelectorType::Button;
    button.declarations = {
        {PropertyType::Width,CSSLength{160.0f,LengthUnit::Pixels}},
        {PropertyType::Height,CSSLength{36.0f,LengthUnit::Pixels}},
        {PropertyType::DisplayType,DisplayType::Block},
        {PropertyType::BackgroundColor,Color{63,67,78,255}},
        {PropertyType::BorderColor,Color{105,111,128,255}},
        {PropertyType::BorderWidth,CSSLength{1.0f,LengthUnit::Pixels}},
        {PropertyType::TextColor,Color{245,246,250,255}}
    };

    StyleRule label;
    label.selector = SelectorType::Label;
    label.declarations = {
        {PropertyType::DisplayType,DisplayType::Inline},
        {PropertyType::TextColor,Color{235,237,242,255}},
        {PropertyType::FontSize,CSSLength{20.0f,LengthUnit::Pixels}}
    };

    StyleRule input = button;
    input.selector = SelectorType::Input;
    input.declarations[0] = {PropertyType::Width,CSSLength{200.0f,LengthUnit::Pixels}};
    input.declarations[3] = {PropertyType::BackgroundColor,Color{45,48,57,255}};

    StyleRule selectable = button;
    selectable.selector = SelectorType::Selectable;

    StyleRule combo = input;
    combo.selector = SelectorType::ComboBox;
    combo.declarations.push_back({PropertyType::ItemHeight,CSSLength{30.0f,LengthUnit::Pixels}});
    combo.declarations.push_back({PropertyType::PopupWidth,CSSLength{220.0f,LengthUnit::Pixels}});

    StyleRule range = input;
    range.selector = SelectorType::RangeInput;

    StyleRule canvas = container;
    canvas.selector = SelectorType::Canvas;
    canvas.declarations[1] = {PropertyType::Height,CSSLength{240.0f,LengthUnit::Pixels}};
    canvas.declarations.push_back({PropertyType::BackgroundColor,Color{22,24,30,255}});
    canvas.declarations.push_back({PropertyType::BorderColor,Color{75,80,94,255}});
    canvas.declarations.push_back({PropertyType::BorderWidth,CSSLength{1.0f,LengthUnit::Pixels}});

    StyleRule image = canvas;
    image.selector = SelectorType::Image;
    image.declarations.push_back({PropertyType::ObjectFit,UIImageFit::Contain});

    StyleRule splitter;
    splitter.selector = SelectorType::Splitter;
    splitter.declarations = {
        {PropertyType::Width,CSSLength{6.0f,LengthUnit::Pixels}},
        {PropertyType::Height,CSSLength{100.0f,LengthUnit::Percent}},
        {PropertyType::BackgroundColor,Color{70,74,86,255}},
        {PropertyType::SplitterSize,CSSLength{6.0f,LengthUnit::Pixels}}
    };

    StyleRule tabs = container;
    tabs.selector = SelectorType::TabBar;
    tabs.declarations.push_back({PropertyType::TabHeight,CSSLength{32.0f,LengthUnit::Pixels}});

    auto composite_rule = [&](SelectorType selector) {
        StyleRule rule = container;
        rule.selector = selector;
        rule.declarations.push_back({PropertyType::HeaderHeight,CSSLength{32.0f,LengthUnit::Pixels}});
        rule.declarations.push_back({PropertyType::BackgroundColor,Color{38,41,49,255}});
        return rule;
    };

    StyleRule collapsible = composite_rule(SelectorType::Collapsible);
    StyleRule tree = composite_rule(SelectorType::TreeNode);
    StyleRule menu = button;
    menu.selector = SelectorType::Menu;
    menu.declarations.push_back({PropertyType::HeaderHeight,CSSLength{32.0f,LengthUnit::Pixels}});
    menu.declarations.push_back({PropertyType::PopupWidth,CSSLength{220.0f,LengthUnit::Pixels}});
    StyleRule popup = composite_rule(SelectorType::Popup);
    StyleRule window = composite_rule(SelectorType::Window);

    StyleRule tooltip;
    tooltip.selector = SelectorType::Tooltip;
    tooltip.declarations = {
        {PropertyType::BackgroundColor,Color{24,26,32,248}},
        {PropertyType::BorderColor,Color{92,98,112,255}},
        {PropertyType::BorderWidth,CSSLength{1.0f,LengthUnit::Pixels}},
        {PropertyType::TextColor,Color{245,246,250,255}},
        {PropertyType::FontSize,CSSLength{15.0f,LengthUnit::Pixels}},
        {PropertyType::Padding,CSSLength{7.0f,LengthUnit::Pixels}}
    };

    sheet.rules = {
        std::move(body),
        std::move(container),
        std::move(button),
        std::move(label),
        std::move(input),
        std::move(selectable),
        std::move(combo),
        std::move(range),
        std::move(canvas),
        std::move(image),
        std::move(splitter),
        std::move(tabs),
        std::move(collapsible),
        std::move(tree),
        std::move(menu),
        std::move(popup),
        std::move(window),
        std::move(tooltip)
    };
    TouchStyleSheet(sheet);
    return sheet;
}

void SetUIFontRegistry(const std::unordered_map<std::string,Font>& fonts) {
    CSS_UI_ACTIVE_FONTS = &fonts;
}

Font ResolveUIFont(const std::string& name) {
    if(CSS_UI_ACTIVE_FONTS != nullptr && name != "default") {
        auto found = CSS_UI_ACTIVE_FONTS->find(name);
        if(found != CSS_UI_ACTIVE_FONTS->end() && found->second.texture.id != 0) {
            return found->second;
        }
    }
    return GetFontDefault();
}

bool AddUIFont(
    std::unordered_map<std::string,Font>& fonts,
    const std::string& name,
    const std::string& path,
    int base_size ) {
    if(name.empty() || path.empty()) return false;

    Font loaded = LoadFontEx(path.c_str(),base_size,nullptr,0);
    if(loaded.texture.id == 0) return false;

    auto existing = fonts.find(name);
    if(existing != fonts.end() && existing->second.texture.id != 0) {
        UnloadFont(existing->second);
    }

    fonts[name] = loaded;
    SetTextureFilter(fonts[name].texture,TEXTURE_FILTER_BILINEAR);
    SetUIFontRegistry(fonts);
    ClearTextMeasureCache();
    return true;
}

void UnloadUIFonts(std::unordered_map<std::string,Font>& fonts) {
    for(auto& [name,font] : fonts) {
        (void)name;
        if(font.texture.id != 0) UnloadFont(font);
    }
    fonts.clear();
    if(CSS_UI_ACTIVE_FONTS == &fonts) CSS_UI_ACTIVE_FONTS = nullptr;
    ClearTextMeasureCache();
}

void HashCombine(std::size_t& seed, std::size_t value) {
    seed ^= value + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
        (seed << 6) + (seed >> 2);
}

std::size_t HashFloat(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits,&value,sizeof(value));
    return std::hash<std::uint32_t>{}(bits);
}

std::size_t HashColor(Color color) {
    std::size_t seed = color.r;
    HashCombine(seed,color.g);
    HashCombine(seed,color.b);
    HashCombine(seed,color.a);
    return seed;
}

std::size_t HashCSSLength(const CSSLength& length) {
    std::size_t seed = HashFloat(length.value);
    HashCombine(seed,static_cast<std::size_t>(length.unit));
    return seed;
}

std::size_t HashCSSId(const CSS_Id& id) {
    std::size_t seed = id.index();

    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,int>) {
                HashCombine(seed,std::hash<int>{}(value));
            }
            else if constexpr(std::is_same_v<T,std::string>) {
                HashCombine(seed,std::hash<std::string>{}(value));
            }
        },
        id
    );

    return seed;
}

std::size_t HashComputedStyle(const ComputedStyle& style) {
    std::size_t seed = 0;

    HashCombine(seed,HashColor(style.background_color));
    HashCombine(seed,HashColor(style.border_color));
    HashCombine(seed,HashColor(style.text_color));
    HashCombine(seed,HashFloat(style.border_width));
    HashCombine(seed,HashFloat(style.border_radius));
    HashCombine(seed,HashFloat(style.scale));
    HashCombine(seed,HashFloat(style.width));
    HashCombine(seed,HashFloat(style.height));
    HashCombine(seed,HashFloat(style.min_width));
    HashCombine(seed,HashFloat(style.max_width));
    HashCombine(seed,HashFloat(style.min_height));
    HashCombine(seed,HashFloat(style.max_height));
    HashCombine(seed,HashFloat(style.font_size));
    HashCombine(seed,HashFloat(style.gap));
    HashCombine(seed,HashFloat(style.row_gap));
    HashCombine(seed,HashFloat(style.column_gap));
    HashCombine(seed,HashFloat(style.opacity));
    HashCombine(seed,HashFloat(style.scroll_speed));
    HashCombine(seed,HashFloat(style.scrollbar_size));
    HashCombine(seed,HashFloat(style.header_height));
    HashCombine(seed,HashFloat(style.item_height));
    HashCombine(seed,HashFloat(style.popup_width));
    HashCombine(seed,HashFloat(style.popup_max_height));
    HashCombine(seed,HashFloat(style.tooltip_delay));
    HashCombine(seed,HashFloat(style.indent));
    HashCombine(seed,HashFloat(style.splitter_size));
    HashCombine(seed,HashFloat(style.tab_height));
    HashCombine(seed,HashColor(style.image_tint));
    HashCombine(seed,static_cast<std::size_t>(style.image_fit));
    HashCombine(seed,static_cast<std::size_t>(style.image_sampling));
    HashCombine(seed,style.image_flip_x ? 1u : 0u);
    HashCombine(seed,style.image_flip_y ? 1u : 0u);

    auto hash_length = [](const CSSLength& value) {
        return HashCSSLength(value);
    };

    HashOptional(seed,style.border_width_length,hash_length);
    HashOptional(seed,style.width_length,hash_length);
    HashOptional(seed,style.height_length,hash_length);
    HashOptional(seed,style.min_width_length,hash_length);
    HashOptional(seed,style.max_width_length,hash_length);
    HashOptional(seed,style.min_height_length,hash_length);
    HashOptional(seed,style.max_height_length,hash_length);
    HashOptional(seed,style.font_size_length,hash_length);
    HashOptional(seed,style.gap_length,hash_length);
    HashOptional(seed,style.row_gap_length,hash_length);
    HashOptional(seed,style.column_gap_length,hash_length);
    HashOptional(seed,style.scrollbar_size_length,hash_length);
    HashOptional(seed,style.header_height_length,hash_length);
    HashOptional(seed,style.item_height_length,hash_length);
    HashOptional(seed,style.popup_width_length,hash_length);
    HashOptional(seed,style.popup_max_height_length,hash_length);
    HashOptional(seed,style.indent_length,hash_length);
    HashOptional(seed,style.splitter_size_length,hash_length);
    HashOptional(seed,style.tab_height_length,hash_length);

    HashCombine(seed,static_cast<std::size_t>(style.position));
    HashCombine(seed,HashFloat(style.left));
    HashCombine(seed,HashFloat(style.right));
    HashCombine(seed,HashFloat(style.top));
    HashCombine(seed,HashFloat(style.bottom));

    HashOptional(seed,style.left_length,hash_length);
    HashOptional(seed,style.right_length,hash_length);
    HashOptional(seed,style.top_length,hash_length);
    HashOptional(seed,style.bottom_length,hash_length);

    HashCombine(seed,std::hash<std::string>{}(style.font));
    HashCombine(seed,static_cast<std::size_t>(style.flex_direction));
    HashCombine(seed,static_cast<std::size_t>(style.justify_content));
    HashCombine(seed,static_cast<std::size_t>(style.align_items));
    HashCombine(seed,static_cast<std::size_t>(style.justify_items));
    HashCombine(seed,static_cast<std::size_t>(style.align_self));
    HashCombine(seed,HashFloat(style.flex_grow));
    HashCombine(seed,HashFloat(style.flex_shrink));
    HashCombine(seed,HashFloat(style.flex_basis));
    HashOptional(seed,style.flex_basis_length,hash_length);

    HashCombine(seed,static_cast<std::size_t>(style.text_align));
    HashCombine(seed,static_cast<std::size_t>(style.overflow));
    HashCombine(seed,std::hash<int>{}(style.grid_template_columns));
    HashCombine(seed,std::hash<int>{}(style.grid_template_rows));
    auto hash_track = [&](const GridTrackSize& track) {
        HashCombine(seed,HashFloat(track.value));
        HashCombine(seed,static_cast<std::size_t>(track.unit));
    };
    for(const auto& track : style.grid_column_tracks) hash_track(track);
    for(const auto& track : style.grid_row_tracks) hash_track(track);
    hash_track(style.grid_auto_column);
    hash_track(style.grid_auto_row);
    HashCombine(seed,static_cast<std::size_t>(style.grid_auto_flow));
    HashCombine(seed,std::hash<int>{}(style.grid_column));
    HashCombine(seed,std::hash<int>{}(style.grid_row));
    HashCombine(seed,std::hash<int>{}(style.grid_column_span));
    HashCombine(seed,std::hash<int>{}(style.grid_row_span));

    auto hash_bool = [](bool value) {
        return std::hash<bool>{}(value);
    };
    auto hash_number = [](float value) {
        return HashFloat(value);
    };

    HashOptional(seed,style.input_step_buttons,hash_bool);
    HashOptional(seed,style.input_step,hash_number);
    HashOptional(seed,style.input_value_box,hash_bool);
    HashOptional(seed,style.input_drag_speed,hash_number);
    HashOptional(seed,style.input_slider_show_value,hash_bool);
    HashOptional(seed,style.input_slider_value_width,hash_number);
    HashOptional(seed,style.input_slider_click_to_set,hash_bool);
    HashOptional(seed,style.runtime_movable,hash_bool);
    HashOptional(seed,style.runtime_resizable,hash_bool);
    HashOptional(seed,style.runtime_constrain,hash_bool);
    HashOptional(seed,style.runtime_lock_x,hash_bool);
    HashOptional(seed,style.runtime_lock_y,hash_bool);
    HashOptional(seed,style.runtime_snap,hash_number);
    HashOptional(seed,style.resize_to_content,hash_bool);
    HashOptional(seed,style.resizable,hash_bool);
    HashOptional(seed,style.closable,hash_bool);
    HashOptional(seed,style.collapsible,hash_bool);
    HashOptional(seed,style.tab_closable,hash_bool);

    HashCombine(seed,HashCSSId(style.id));

    for(const std::string& class_name : style.classes) {
        HashCombine(seed,std::hash<std::string>{}(class_name));
    }

    HashCombine(seed,static_cast<std::size_t>(style.display));

    auto hash_edges = [&](const EdgeValues& edges) {
        HashCombine(seed,HashFloat(edges.left));
        HashCombine(seed,HashFloat(edges.right));
        HashCombine(seed,HashFloat(edges.top));
        HashCombine(seed,HashFloat(edges.bottom));
    };

    hash_edges(style.padding);
    hash_edges(style.margin);

    HashOptional(seed,style.padding_lengths.left,hash_length);
    HashOptional(seed,style.padding_lengths.right,hash_length);
    HashOptional(seed,style.padding_lengths.top,hash_length);
    HashOptional(seed,style.padding_lengths.bottom,hash_length);
    HashOptional(seed,style.margin_lengths.left,hash_length);
    HashOptional(seed,style.margin_lengths.right,hash_length);
    HashOptional(seed,style.margin_lengths.top,hash_length);
    HashOptional(seed,style.margin_lengths.bottom,hash_length);

    return seed;
}

void ClearResolvedStyleCache() {
    CSS_UI_RESOLVED_STYLE_CACHE.clear();
}

std::size_t HashTextMeasureKey(
    Font font,
    std::string_view text,
    float font_size,
    float spacing
) {
    std::size_t seed = std::hash<unsigned int>{}(font.texture.id);
    HashCombine(seed,std::hash<int>{}(font.baseSize));
    HashCombine(seed,HashFloat(font_size));
    HashCombine(seed,HashFloat(spacing));
    HashCombine(seed,text.size());
    HashCombine(seed,std::hash<std::string_view>{}(text));
    return seed;
}

void ClearTextMeasureCache() {
    CSS_UI_TEXT_MEASURE_CACHE.clear();
}

Vector2 MeasureTextCached(
    Font font,
    const char* text,
    float font_size,
    float spacing
) {
    std::string_view view = text != nullptr
        ? std::string_view(text)
        : std::string_view{};

    const std::size_t key =
        HashTextMeasureKey(font,view,font_size,spacing);

    auto range = CSS_UI_TEXT_MEASURE_CACHE.equal_range(key);

    for(auto iterator = range.first; iterator != range.second; ++iterator) {
        const TextMeasureCacheEntry& entry = iterator->second;

        if(
            entry.texture_id == font.texture.id &&
            entry.base_size == font.baseSize &&
            entry.font_size == font_size &&
            entry.spacing == spacing &&
            entry.text == view
        ) {
            return entry.measured;
        }
    }

    Vector2 measured = MeasureTextEx(
        font,
        view.empty() ? "" : view.data(),
        font_size,
        spacing
    );

    if(CSS_UI_TEXT_MEASURE_CACHE.size() >= 4096) {
        CSS_UI_TEXT_MEASURE_CACHE.clear();
    }

    CSS_UI_TEXT_MEASURE_CACHE.emplace(
        key,
        TextMeasureCacheEntry{
            font.texture.id,
            font.baseSize,
            font_size,
            spacing,
            std::string(view),
            measured
        }
    );

    return measured;
}

UIContainer& UIContainer::Add(UIContainer child) {
    auto value = std::make_shared<UIContainer>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UICollapsible& UIContainer::Add(UICollapsible child) {
    auto value = std::make_shared<UICollapsible>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UITreeNode& UIContainer::Add(UITreeNode child) {
    auto value = std::make_shared<UITreeNode>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIMenu& UIContainer::Add(UIMenu child) {
    auto value = std::make_shared<UIMenu>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIPopup& UIContainer::Add(UIPopup child) {
    auto value = std::make_shared<UIPopup>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIWindow& UIContainer::Add(UIWindow child) {
    auto value = std::make_shared<UIWindow>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UITabBar& UIContainer::Add(UITabBar child) {
    auto value = std::make_shared<UITabBar>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

void OpenPopupAt(UIPopup& popup, Vector2 position) {
    popup.open = true;
    popup.opened = true;
    popup.closed = false;
    popup.style.position = PositionType::Absolute;
    popup.style.left = position.x;
    popup.style.top = position.y;
    popup.style.left_length = CSSLength{position.x,LengthUnit::Pixels};
    popup.style.top_length = CSSLength{position.y,LengthUnit::Pixels};
    popup.rect.runtime_offset = {};
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

void ClosePopup(UIPopup& popup) {
    popup.closed = popup.open;
    popup.open = false;
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

void OpenContextMenu(UIPopup& popup) {
    popup.modal = false;
    popup.close_on_outside_click = true;
    OpenPopupAt(popup,GetMousePosition());
}

void OpenModal(UIPopup& popup) {
    popup.modal = true;
    popup.open = true;
    popup.opened = true;
    popup.closed = false;
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

std::size_t HashRectangle(Rectangle rectangle) {
    std::size_t seed = HashFloat(rectangle.x);
    HashCombine(seed,HashFloat(rectangle.y));
    HashCombine(seed,HashFloat(rectangle.width));
    HashCombine(seed,HashFloat(rectangle.height));
    return seed;
}

std::string CSSRuntimeId(const CSS_Id& id) {
    if(const auto* text = std::get_if<std::string>(&id)) return "#" + *text;
    if(const auto* number = std::get_if<int>(&id)) return "#" + std::to_string(*number);
    return {};
}

const char* RuntimeSelectorName(SelectorType selector) {
    switch(selector) {
        case SelectorType::Button: return "button";
        case SelectorType::Label: return "label";
        case SelectorType::Input: return "input";
        case SelectorType::Container: return "container";
        case SelectorType::Selectable: return "selectable";
        case SelectorType::ComboBox: return "combo";
        case SelectorType::RangeInput: return "range";
        case SelectorType::Canvas: return "canvas";
        case SelectorType::Image: return "image";
        case SelectorType::Splitter: return "splitter";
        case SelectorType::TabBar: return "tabs";
        case SelectorType::Collapsible: return "collapsible";
        case SelectorType::TreeNode: return "tree-node";
        case SelectorType::Menu: return "menu";
        case SelectorType::Popup: return "popup";
        case SelectorType::Window: return "window";
        case SelectorType::TextBox: return "textbox";
        case SelectorType::Table: return "table";
        case SelectorType::VirtualList: return "virtual-list";
        case SelectorType::DockSpace: return "dock-space";
        default: return "element";
    }
}

