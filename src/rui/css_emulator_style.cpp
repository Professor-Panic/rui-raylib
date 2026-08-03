#include "css_emulator.h"

bool HasCSSId(const CSS_Id& id) {
    return !std::holds_alternative<std::monostate>(id);
}

bool HasCSSClass(const std::vector<std::string>& classes, const std::string& class_name) {
    return std::find(classes.begin(), classes.end(), class_name) != classes.end();
}

void AddCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    if(!class_name.empty() && !HasCSSClass(classes, class_name)) {
        classes.push_back(class_name);
    }
}

bool RemoveCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    auto iterator = std::remove(classes.begin(), classes.end(), class_name);

    if(iterator == classes.end()) {
        return false;
    }

    classes.erase(iterator, classes.end());
    return true;
}

bool ToggleCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    if(HasCSSClass(classes, class_name)) {
        RemoveCSSClass(classes, class_name);
        return false;
    }

    AddCSSClass(classes, class_name);
    return true;
}

void ClearCSSClasses(std::vector<std::string>& classes) {
    classes.clear();
}

Color WithOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(
        std::clamp(opacity, 0.0f, 1.0f) * 255.0f
    );

    return color;
}

Color MixColors(Color first, Color second, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);

    return Color{
        static_cast<unsigned char>(Lerp(first.r, second.r, amount)),
        static_cast<unsigned char>(Lerp(first.g, second.g, amount)),
        static_cast<unsigned char>(Lerp(first.b, second.b, amount)),
        static_cast<unsigned char>(Lerp(first.a, second.a, amount))
    };
}

Color LightenColor(Color color, float amount) {
    return MixColors(color, WHITE, amount);
}

Color DarkenColor(Color color, float amount) {
    return MixColors(color, BLACK, amount);
}

Color ApplyStyleOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(
        static_cast<float>(color.a) * std::clamp(opacity,0.0f,1.0f)
    );
    return color;
}

float GetTextX(Rectangle content, float text_width, TextAlign alignment) {
    if(alignment == TextAlign::Left) return content.x;
    if(alignment == TextAlign::Right) return content.x + content.width - text_width;
    return content.x + (content.width - text_width) * 0.5f;
}

bool HasAllCSSClasses(
    const std::vector<std::string>& element_classes,
    const std::vector<std::string>& rule_classes
) {
    return std::all_of(rule_classes.begin(), rule_classes.end(), [&](const std::string& class_name) {
        return HasCSSClass(element_classes, class_name);
    });
}

std::string LegacySelectorStateName(SelectorState state) {
    switch(state) {
        case SelectorState::Hover: return "hover";
        case SelectorState::Active: return "active";
        case SelectorState::Focus: return "focus";
        case SelectorState::Disabled: return "disabled";
        default: return {};
    }
}

bool SelectorIdentityMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes
) {
    if(rule.selector != SelectorType::Any && rule.selector != selector_type) return false;
    if(HasCSSId(rule.id) && rule.id != id) return false;
    return HasAllCSSClasses(classes,rule.classes);
}

bool SelectorMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state
) {
    if(!SelectorIdentityMatches(rule,selector_type,id,classes)) return false;
    if(!rule.pseudo_states.empty()) {
        const std::string requested = LegacySelectorStateName(selector_state);
        return rule.pseudo_states.size() == 1 && rule.pseudo_states.front() == requested;
    }
    return rule.state == selector_state;
}

bool SelectorMatchesPseudoStates(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    const std::unordered_set<std::string>& active_states
) {
    if(!SelectorIdentityMatches(rule,selector_type,id,classes)) return false;
    if(!rule.pseudo_states.empty()) {
        return std::all_of(
            rule.pseudo_states.begin(),rule.pseudo_states.end(),
            [&](const std::string& state) {
                return active_states.find(NormalizeUIPseudoStateName(state)) != active_states.end();
            }
        );
    }
    if(rule.state == SelectorState::None) return true;
    return active_states.find(LegacySelectorStateName(rule.state)) != active_states.end();
}

int GetSelectorSpecificity(const StyleRule& rule) {
    int specificity = 0;

    if(HasCSSId(rule.id)) {
        specificity += 100;
    }

    specificity += static_cast<int>(rule.classes.size()) * 10;
    specificity += static_cast<int>(
        rule.pseudo_states.empty()
            ? (rule.state == SelectorState::None ? 0 : 1)
            : rule.pseudo_states.size()
    ) * 10;

    if(rule.selector != SelectorType::Any) {
        specificity += 1;
    }

    return specificity;
}

std::optional<CSSLength> ResolveLengthValue(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    PropertyType property_type
) {
    auto length = ResolveValue<CSSLength>(
        style_sheet,
        selector_type,
        id,
        classes,
        selector_state,
        property_type
    );

    if(length.has_value()) {
        return length;
    }

    auto number = ResolveValue<float>(
        style_sheet,
        selector_type,
        id,
        classes,
        selector_state,
        property_type
    );

    if(number.has_value()) {
        return CSSLength{*number,LengthUnit::Pixels};
    }

    return std::nullopt;
}

float ResolveCSSLength(
    const CSSLength& length,
    float percent_reference
) {
    switch(length.unit) {
        case LengthUnit::Auto:
            return 0.0f;

        case LengthUnit::Percent:
            return percent_reference * length.value / 100.0f;

        case LengthUnit::ViewWidth:
            return static_cast<float>(GetScreenWidth()) *
                length.value / 100.0f;

        case LengthUnit::ViewHeight:
            return static_cast<float>(GetScreenHeight()) *
                length.value / 100.0f;

        default:
            return length.value;
    }
}

float ComputedMetricScale(
    const std::optional<CSSLength>& source,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale
) {
    // Relative lengths already use a containing block that has inherited its
    // parent's scale. Multiplying them by inherited_scale again would double
    // their size. Pixel values and C++ defaults still need the full factor.
    const bool relative =
        source.has_value() &&
        source->unit != LengthUnit::Pixels &&
        source->unit != LengthUnit::Auto;

    if(relative) {
        return relative_reference_includes_local_scale
            ? 1.0f
            : local_scale;
    }

    return inherited_scale * local_scale;
}

void ScaleComputedStyleMetrics(
    ComputedStyle& style,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale,
    bool inverse ) {
    inherited_scale = std::max(0.0001f,inherited_scale);
    local_scale = std::max(0.0001f,local_scale);

    auto multiply = [&](float& value, float factor) {
        value *= inverse ? 1.0f / factor : factor;
    };

    auto dimension = [&] (
        float& value,
        const std::optional<CSSLength>& source = std::nullopt
    ) {
        multiply(
            value,
            ComputedMetricScale(
                source,
                inherited_scale,
                local_scale,
                relative_reference_includes_local_scale
            )
        );
    };

    // Position belongs to the containing coordinate system. A widget's own
    // scale changes its size, not where its top-left anchor is placed.
    auto position = [&] (
        float& value,
        const std::optional<CSSLength>& source
    ) {
        const bool relative =
            source.has_value() &&
            source->unit != LengthUnit::Pixels;
        multiply(value,relative ? 1.0f : inherited_scale);
    };

    dimension(style.border_width,style.border_width_length);
    dimension(style.width,style.width_length);
    dimension(style.height,style.height_length);
    dimension(style.min_width,style.min_width_length);
    dimension(style.max_width,style.max_width_length);
    dimension(style.min_height,style.min_height_length);
    dimension(style.max_height,style.max_height_length);
    dimension(style.font_size,style.font_size_length);
    dimension(style.gap,style.gap_length);
    dimension(style.row_gap,style.row_gap_length);
    dimension(style.column_gap,style.column_gap_length);
    dimension(style.scroll_speed);
    dimension(style.scrollbar_size,style.scrollbar_size_length);
    dimension(style.header_height,style.header_height_length);
    dimension(style.item_height,style.item_height_length);
    dimension(style.popup_width,style.popup_width_length);
    dimension(style.popup_max_height,style.popup_max_height_length);
    dimension(style.indent,style.indent_length);
    dimension(style.splitter_size,style.splitter_size_length);
    dimension(style.tab_height,style.tab_height_length);
    dimension(style.flex_basis,style.flex_basis_length);

    position(style.left,style.left_length);
    position(style.right,style.right_length);
    position(style.top,style.top_length);
    position(style.bottom,style.bottom_length);

    dimension(style.padding.left,style.padding_lengths.left);
    dimension(style.padding.right,style.padding_lengths.right);
    dimension(style.padding.top,style.padding_lengths.top);
    dimension(style.padding.bottom,style.padding_lengths.bottom);
    dimension(style.margin.left,style.margin_lengths.left);
    dimension(style.margin.right,style.margin_lengths.right);
    dimension(style.margin.top,style.margin_lengths.top);
    dimension(style.margin.bottom,style.margin_lengths.bottom);

    auto scale_track = [&](GridTrackSize& track) {
        if(track.unit == GridTrackUnit::Pixels) {
            multiply(track.value,inherited_scale * local_scale);
        }
    };

    for(GridTrackSize& track : style.grid_column_tracks) {
        scale_track(track);
    }
    for(GridTrackSize& track : style.grid_row_tracks) {
        scale_track(track);
    }
    scale_track(style.grid_auto_column);
    scale_track(style.grid_auto_row);
}

void ResetComputedStyleScale(ComputedStyle& style) {
    ScaleComputedStyleMetrics(
        style,
        style.applied_inherited_scale,
        style.applied_scale,
        style.applied_relative_reference_includes_local_scale,
        true
    );
    style.applied_scale = 1.0f;
    style.applied_inherited_scale = 1.0f;
    style.applied_relative_reference_includes_local_scale = false;
}

void ApplyComputedStyleScale(
    ComputedStyle& style,
    float inherited_scale,
    bool relative_reference_includes_local_scale ) {
    const float local_scale = std::clamp(style.scale,0.01f,100.0f);
    const float safe_inherited_scale = std::max(0.0001f,inherited_scale);

    ScaleComputedStyleMetrics(
        style,
        safe_inherited_scale,
        local_scale,
        relative_reference_includes_local_scale,
        false
    );
    style.applied_scale = local_scale;
    style.applied_inherited_scale = safe_inherited_scale;
    style.applied_relative_reference_includes_local_scale =
        relative_reference_includes_local_scale;
}

float EffectiveComputedStyleScale(const ComputedStyle& style) {
    return style.applied_inherited_scale * style.applied_scale;
}

void ResolveComputedLengths(
    ComputedStyle& style,
    float parent_width,
    float parent_height,
    float parent_font_size ,
    float inherited_scale ,
    bool relative_reference_includes_local_scale ) {
    ResetComputedStyleScale(style);

    if(style.border_width_length) {
        style.border_width = ResolveCSSLength(
            *style.border_width_length,
            std::min(parent_width,parent_height)
        );
    }

    if(
        style.width_length &&
        style.width_length->unit != LengthUnit::Auto
    ) {
        style.width = ResolveCSSLength(
            *style.width_length,
            parent_width
        );
    }

    if(
        style.height_length &&
        style.height_length->unit != LengthUnit::Auto
    ) {
        style.height = ResolveCSSLength(
            *style.height_length,
            parent_height
        );
    }

    if(style.min_width_length) {
        style.min_width = ResolveCSSLength(*style.min_width_length,parent_width);
    }
    if(style.max_width_length) {
        style.max_width = ResolveCSSLength(*style.max_width_length,parent_width);
    }
    if(style.min_height_length) {
        style.min_height = ResolveCSSLength(*style.min_height_length,parent_height);
    }
    if(style.max_height_length) {
        style.max_height = ResolveCSSLength(*style.max_height_length,parent_height);
    }

    style.max_width = std::max(style.min_width,style.max_width);
    style.max_height = std::max(style.min_height,style.max_height);
    style.width = std::clamp(style.width,style.min_width,style.max_width);
    style.height = std::clamp(style.height,style.min_height,style.max_height);

    if(style.font_size_length) {
        style.font_size = ResolveCSSLength(
            *style.font_size_length,
            parent_font_size
        );
    }

    if(style.gap_length) {
        style.gap = ResolveCSSLength(
            *style.gap_length,
            parent_width
        );
        if(!style.row_gap_length) style.row_gap = style.gap;
        if(!style.column_gap_length) style.column_gap = style.gap;
    }
    if(style.row_gap_length) {
        style.row_gap = ResolveCSSLength(*style.row_gap_length,parent_height);
    }
    if(style.column_gap_length) {
        style.column_gap = ResolveCSSLength(*style.column_gap_length,parent_width);
    }
    if(style.scrollbar_size_length) {
        style.scrollbar_size = ResolveCSSLength(*style.scrollbar_size_length,parent_width);
    }
    if(style.header_height_length) {
        style.header_height = ResolveCSSLength(*style.header_height_length,parent_height);
    }
    if(style.item_height_length) {
        style.item_height = ResolveCSSLength(*style.item_height_length,parent_height);
    }
    if(style.popup_width_length) {
        style.popup_width = ResolveCSSLength(*style.popup_width_length,parent_width);
    }
    if(style.popup_max_height_length) {
        style.popup_max_height = ResolveCSSLength(*style.popup_max_height_length,parent_height);
    }
    if(style.indent_length) {
        style.indent = ResolveCSSLength(*style.indent_length,parent_width);
    }
    if(style.splitter_size_length) {
        style.splitter_size = ResolveCSSLength(*style.splitter_size_length,parent_width);
    }
    if(style.tab_height_length) {
        style.tab_height = ResolveCSSLength(*style.tab_height_length,parent_height);
    }

    if(style.left_length) {
        style.left = ResolveCSSLength(
            *style.left_length,
            parent_width
        );
    }

    if(style.right_length) {
        style.right = ResolveCSSLength(
            *style.right_length,
            parent_width
        );
    }

    if(style.top_length) {
        style.top = ResolveCSSLength(
            *style.top_length,
            parent_height
        );
    }

    if(style.bottom_length) {
        style.bottom = ResolveCSSLength(
            *style.bottom_length,
            parent_height
        );
    }

    if(style.flex_basis_length) {
        style.flex_basis =
            style.flex_basis_length->unit == LengthUnit::Auto
                ? -1.0f
                : ResolveCSSLength(
                    *style.flex_basis_length,
                    parent_width
                );
    }

    if(style.padding_lengths.left) {
        style.padding.left = ResolveCSSLength(
            *style.padding_lengths.left,
            parent_width
        );
    }

    if(style.padding_lengths.right) {
        style.padding.right = ResolveCSSLength(
            *style.padding_lengths.right,
            parent_width
        );
    }

    if(style.padding_lengths.top) {
        style.padding.top = ResolveCSSLength(
            *style.padding_lengths.top,
            parent_height
        );
    }

    if(style.padding_lengths.bottom) {
        style.padding.bottom = ResolveCSSLength(
            *style.padding_lengths.bottom,
            parent_height
        );
    }

    if(style.margin_lengths.left) {
        style.margin.left = ResolveCSSLength(
            *style.margin_lengths.left,
            parent_width
        );
    }

    if(style.margin_lengths.right) {
        style.margin.right = ResolveCSSLength(
            *style.margin_lengths.right,
            parent_width
        );
    }

    if(style.margin_lengths.top) {
        style.margin.top = ResolveCSSLength(
            *style.margin_lengths.top,
            parent_height
        );
    }

    if(style.margin_lengths.bottom) {
        style.margin.bottom = ResolveCSSLength(
            *style.margin_lengths.bottom,
            parent_height
        );
    }

    ApplyComputedStyleScale(
        style,
        inherited_scale,
        relative_reference_includes_local_scale
    );
}

void ApplyResolvedDeclaration(
    ComputedStyle& style,
    const Declaration& declaration
) {
    const PropertyValueType& value = declaration.value;

    auto assign_length = [&](std::optional<CSSLength>& target) {
        if(std::holds_alternative<CSSLength>(value)) {
            target = std::get<CSSLength>(value);
        }
        else if(std::holds_alternative<float>(value)) {
            target = CSSLength{
                std::get<float>(value),
                LengthUnit::Pixels
            };
        }
    };

    switch(declaration.property) {
        case PropertyType::BackgroundColor:
            if(std::holds_alternative<Color>(value)) {
                style.background_color = std::get<Color>(value);
            }
            break;

        case PropertyType::BorderColor:
            if(std::holds_alternative<Color>(value)) {
                style.border_color = std::get<Color>(value);
            }
            break;

        case PropertyType::TextColor:
            if(std::holds_alternative<Color>(value)) {
                style.text_color = std::get<Color>(value);
            }
            break;

        case PropertyType::BorderRadius:
            if(std::holds_alternative<float>(value)) {
                style.border_radius = std::get<float>(value);
            }
            break;

        case PropertyType::BorderWidth:
            assign_length(style.border_width_length);
            break;

        case PropertyType::Width:
            assign_length(style.width_length);
            break;

        case PropertyType::Height:
            assign_length(style.height_length);
            break;

        case PropertyType::MinWidth:
            assign_length(style.min_width_length);
            break;

        case PropertyType::MaxWidth:
            assign_length(style.max_width_length);
            break;

        case PropertyType::MinHeight:
            assign_length(style.min_height_length);
            break;

        case PropertyType::MaxHeight:
            assign_length(style.max_height_length);
            break;

        case PropertyType::FontSize:
            assign_length(style.font_size_length);
            break;

        case PropertyType::Gap:
            assign_length(style.gap_length);
            break;

        case PropertyType::RowGap:
            assign_length(style.row_gap_length);
            break;

        case PropertyType::ColumnGap:
            assign_length(style.column_gap_length);
            break;

        case PropertyType::ScrollbarSize:
            assign_length(style.scrollbar_size_length);
            break;

        case PropertyType::HeaderHeight:
            assign_length(style.header_height_length);
            break;

        case PropertyType::ItemHeight:
            assign_length(style.item_height_length);
            break;

        case PropertyType::PopupWidth:
            assign_length(style.popup_width_length);
            break;

        case PropertyType::PopupMaxHeight:
            assign_length(style.popup_max_height_length);
            break;

        case PropertyType::Indent:
            assign_length(style.indent_length);
            break;

        case PropertyType::SplitterSize:
            assign_length(style.splitter_size_length);
            break;

        case PropertyType::TabHeight:
            assign_length(style.tab_height_length);
            break;

        case PropertyType::Left:
            assign_length(style.left_length);
            break;

        case PropertyType::Right:
            assign_length(style.right_length);
            break;

        case PropertyType::Top:
            assign_length(style.top_length);
            break;

        case PropertyType::Bottom:
            assign_length(style.bottom_length);
            break;

        case PropertyType::FlexBasis:
            assign_length(style.flex_basis_length);
            break;

        case PropertyType::Padding: {
            std::optional<CSSLength> length;
            assign_length(length);
            if(length.has_value()) {
                style.padding_lengths.left = *length;
                style.padding_lengths.right = *length;
                style.padding_lengths.top = *length;
                style.padding_lengths.bottom = *length;
            }
            break;
        }

        case PropertyType::PaddingLeft:
            assign_length(style.padding_lengths.left);
            break;

        case PropertyType::PaddingRight:
            assign_length(style.padding_lengths.right);
            break;

        case PropertyType::PaddingTop:
            assign_length(style.padding_lengths.top);
            break;

        case PropertyType::PaddingBottom:
            assign_length(style.padding_lengths.bottom);
            break;

        case PropertyType::Margin: {
            std::optional<CSSLength> length;
            assign_length(length);
            if(length.has_value()) {
                style.margin_lengths.left = *length;
                style.margin_lengths.right = *length;
                style.margin_lengths.top = *length;
                style.margin_lengths.bottom = *length;
            }
            break;
        }

        case PropertyType::MarginLeft:
            assign_length(style.margin_lengths.left);
            break;

        case PropertyType::MarginRight:
            assign_length(style.margin_lengths.right);
            break;

        case PropertyType::MarginTop:
            assign_length(style.margin_lengths.top);
            break;

        case PropertyType::MarginBottom:
            assign_length(style.margin_lengths.bottom);
            break;

        case PropertyType::FontName:
            if(std::holds_alternative<std::string>(value)) {
                style.font = std::get<std::string>(value);
            }
            break;

        case PropertyType::DisplayType:
            if(std::holds_alternative<DisplayType>(value)) {
                style.display = std::get<DisplayType>(value);
            }
            break;

        case PropertyType::Opacity:
            if(std::holds_alternative<float>(value)) {
                style.opacity = std::clamp(
                    std::get<float>(value),
                    0.0f,
                    1.0f
                );
            }
            break;
        case PropertyType::Scale:
            if(std::holds_alternative<float>(value)) {
                style.scale = std::clamp(
                    std::get<float>(value),
                    0.01f,
                    100.0f
                );
            }
            break;
        case PropertyType::Position:
            if(std::holds_alternative<PositionType>(value)) {
                style.position = std::get<PositionType>(value);
            }
            break;

        case PropertyType::FlexDirection:
            if(std::holds_alternative<FlexDirection>(value)) {
                style.flex_direction = std::get<FlexDirection>(value);
            }
            break;

        case PropertyType::JustifyContent:
            if(std::holds_alternative<JustifyContent>(value)) {
                style.justify_content = std::get<JustifyContent>(value);
            }
            break;

        case PropertyType::AlignItems:
            if(std::holds_alternative<AlignItems>(value)) {
                style.align_items = std::get<AlignItems>(value);
            }
            break;

        case PropertyType::JustifyItems:
            if(std::holds_alternative<AlignItems>(value)) {
                style.justify_items = std::get<AlignItems>(value);
            }
            break;

        case PropertyType::AlignSelf:
            if(std::holds_alternative<AlignSelf>(value)) {
                style.align_self = std::get<AlignSelf>(value);
            }
            break;

        case PropertyType::FlexGrow:
            if(std::holds_alternative<float>(value)) {
                style.flex_grow = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::FlexShrink:
            if(std::holds_alternative<float>(value)) {
                style.flex_shrink = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::TextAlign:
            if(std::holds_alternative<TextAlign>(value)) {
                style.text_align = std::get<TextAlign>(value);
            }
            break;

        case PropertyType::Overflow:
            if(std::holds_alternative<OverflowType>(value)) {
                style.overflow = std::get<OverflowType>(value);
            }
            break;

        case PropertyType::GridTemplateColumns:
            if(std::holds_alternative<GridTrackList>(value)) {
                style.grid_column_tracks = std::get<GridTrackList>(value);
                style.grid_template_columns = std::max(1,static_cast<int>(style.grid_column_tracks.size()));
            }
            break;

        case PropertyType::GridTemplateRows:
            if(std::holds_alternative<GridTrackList>(value)) {
                style.grid_row_tracks = std::get<GridTrackList>(value);
                style.grid_template_rows = static_cast<int>(style.grid_row_tracks.size());
            }
            break;

        case PropertyType::GridAutoColumns:
            if(std::holds_alternative<GridTrackList>(value) && !std::get<GridTrackList>(value).empty()) {
                style.grid_auto_column = std::get<GridTrackList>(value).front();
            }
            break;

        case PropertyType::GridAutoRows:
            if(std::holds_alternative<GridTrackList>(value) && !std::get<GridTrackList>(value).empty()) {
                style.grid_auto_row = std::get<GridTrackList>(value).front();
            }
            break;

        case PropertyType::GridAutoFlow:
            if(std::holds_alternative<GridAutoFlow>(value)) {
                style.grid_auto_flow = std::get<GridAutoFlow>(value);
            }
            break;

        case PropertyType::GridColumn:
            if(std::holds_alternative<int>(value)) {
                style.grid_column = std::max(
                    0,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridRow:
            if(std::holds_alternative<int>(value)) {
                style.grid_row = std::max(
                    0,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridColumnSpan:
            if(std::holds_alternative<int>(value)) {
                style.grid_column_span = std::max(
                    1,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridRowSpan:
            if(std::holds_alternative<int>(value)) {
                style.grid_row_span = std::max(
                    1,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::InputStepButtons:
            if(std::holds_alternative<bool>(value)) {
                style.input_step_buttons = std::get<bool>(value);
            }
            break;

        case PropertyType::InputStep:
            if(std::holds_alternative<float>(value)) {
                style.input_step = std::get<float>(value);
            }
            break;

        case PropertyType::InputValueBox:
            if(std::holds_alternative<bool>(value)) {
                style.input_value_box = std::get<bool>(value);
            }
            break;

        case PropertyType::InputDragSpeed:
            if(std::holds_alternative<float>(value)) {
                style.input_drag_speed = std::max(
                    0.000001f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::InputSliderShowValue:
            if(std::holds_alternative<bool>(value)) {
                style.input_slider_show_value = std::get<bool>(value);
            }
            break;

        case PropertyType::InputSliderValueWidth:
            if(std::holds_alternative<float>(value)) {
                style.input_slider_value_width = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::InputSliderClickToSet:
            if(std::holds_alternative<bool>(value)) {
                style.input_slider_click_to_set = std::get<bool>(value);
            }
            break;

        case PropertyType::RuntimeMovable:
            if(std::holds_alternative<bool>(value)) style.runtime_movable = std::get<bool>(value);
            break;
        case PropertyType::RuntimeResizable:
            if(std::holds_alternative<bool>(value)) style.runtime_resizable = std::get<bool>(value);
            break;
        case PropertyType::RuntimeConstrain:
            if(std::holds_alternative<bool>(value)) style.runtime_constrain = std::get<bool>(value);
            break;
        case PropertyType::RuntimeLockX:
            if(std::holds_alternative<bool>(value)) style.runtime_lock_x = std::get<bool>(value);
            break;
        case PropertyType::RuntimeLockY:
            if(std::holds_alternative<bool>(value)) style.runtime_lock_y = std::get<bool>(value);
            break;
        case PropertyType::RuntimeSnap:
            if(std::holds_alternative<float>(value)) style.runtime_snap = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ResizeToContent:
            if(std::holds_alternative<bool>(value)) style.resize_to_content = std::get<bool>(value);
            break;
        case PropertyType::ScrollSpeed:
            if(std::holds_alternative<float>(value)) style.scroll_speed = std::max(1.0f,std::get<float>(value));
            break;
        case PropertyType::TooltipDelay:
            if(std::holds_alternative<float>(value)) style.tooltip_delay = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::Resizable:
            if(std::holds_alternative<bool>(value)) style.resizable = std::get<bool>(value);
            break;
        case PropertyType::Closable:
            if(std::holds_alternative<bool>(value)) style.closable = std::get<bool>(value);
            break;
        case PropertyType::Collapsible:
            if(std::holds_alternative<bool>(value)) style.collapsible = std::get<bool>(value);
            break;
        case PropertyType::ObjectFit:
            if(std::holds_alternative<UIImageFit>(value)) style.image_fit = std::get<UIImageFit>(value);
            break;
        case PropertyType::ImageRendering:
            if(std::holds_alternative<UIImageSampling>(value)) style.image_sampling = std::get<UIImageSampling>(value);
            break;
        case PropertyType::ImageTint:
            if(std::holds_alternative<Color>(value)) style.image_tint = std::get<Color>(value);
            break;
        case PropertyType::ImageFlipX:
            if(std::holds_alternative<bool>(value)) style.image_flip_x = std::get<bool>(value);
            break;
        case PropertyType::ImageFlipY:
            if(std::holds_alternative<bool>(value)) style.image_flip_y = std::get<bool>(value);
            break;
        case PropertyType::TabClosable:
            if(std::holds_alternative<bool>(value)) style.tab_closable = std::get<bool>(value);
            break;
    }
}

ComputedStyle ResolveStyleUncached(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style ) {
    style.id = id;

    std::array<int,property_names.size()> best_specificity{};
    best_specificity.fill(-1);
    int best_transition_specificity = -1;
    int best_animation_specificity = -1;

    for(const StyleRule& rule : style_sheet.rules) {
        if(!SelectorMatches(
            rule,
            selector_type,
            id,
            style.classes,
            selector_state
        )) {
            continue;
        }

        const int specificity =
            GetSelectorSpecificity(rule);

        if(rule.transitions_declared && specificity >= best_transition_specificity) {
            best_transition_specificity = specificity;
            style.transitions = rule.transitions;
        }
        if(rule.animations_declared && specificity >= best_animation_specificity) {
            best_animation_specificity = specificity;
            style.animations = rule.animations;
        }

        for(const Declaration& declaration : rule.declarations) {
            const std::size_t property_index =
                static_cast<std::size_t>(
                    declaration.property
                );

            if(property_index >= best_specificity.size()) {
                continue;
            }

            if(specificity < best_specificity[property_index]) {
                continue;
            }

            best_specificity[property_index] = specificity;
            ApplyResolvedDeclaration(style,declaration);
        }
    }

    return style;
}

ComputedStyle ResolveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style ) {
    // Styles stored on widgets contain screen-space values from the previous
    // layout. Restore their logical values before matching CSS again.
    const float inherited_scale = style.applied_inherited_scale;
    const bool relative_reference_includes_local_scale =
        style.applied_relative_reference_includes_local_scale;
    const bool preserve_applied_scale =
        std::abs(style.applied_scale - 1.0f) > 0.00001f ||
        std::abs(inherited_scale - 1.0f) > 0.00001f;
    ResetComputedStyleScale(style);
    style.id = id;

    ResolvedStyleCacheKey key {
        &style_sheet,
        style_sheet.revision,
        HashComputedStyle(style),
        selector_type,
        selector_state
    };

    auto found = CSS_UI_RESOLVED_STYLE_CACHE.find(key);
    if(found != CSS_UI_RESOLVED_STYLE_CACHE.end()) {
        ComputedStyle cached = found->second;
        if(preserve_applied_scale) {
            ApplyComputedStyleScale(
                cached,
                inherited_scale,
                relative_reference_includes_local_scale
            );
        }
        return cached;
    }

    ComputedStyle resolved = ResolveStyleUncached(
        style_sheet,
        selector_type,
        id,
        selector_state,
        std::move(style)
    );

    if(CSS_UI_RESOLVED_STYLE_CACHE.size() >= 4096) {
        CSS_UI_RESOLVED_STYLE_CACHE.clear();
    }

    CSS_UI_RESOLVED_STYLE_CACHE.emplace(
        std::move(key),
        resolved
    );

    if(preserve_applied_scale) {
        ApplyComputedStyleScale(
            resolved,
            inherited_scale,
            relative_reference_includes_local_scale
        );
    }

    return resolved;
}

ComputedStyle ResolveStyleWithPseudoStates(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& active_state_names,
    ComputedStyle style
) {
    const float inherited_scale = style.applied_inherited_scale;
    const bool relative_reference_includes_local_scale =
        style.applied_relative_reference_includes_local_scale;
    const bool preserve_applied_scale =
        std::abs(style.applied_scale-1.0f) > 0.00001f ||
        std::abs(inherited_scale-1.0f) > 0.00001f;
    ResetComputedStyleScale(style);
    style.id = id;

    std::unordered_set<std::string> active_states;
    for(const std::string& state : active_state_names) {
        const std::string normalized = NormalizeUIPseudoStateName(state);
        if(!normalized.empty()) active_states.insert(normalized);
    }

    std::array<int,property_names.size()> best_specificity{};
    best_specificity.fill(-1);
    int best_transition_specificity = -1;
    int best_animation_specificity = -1;

    for(const StyleRule& rule : style_sheet.rules) {
        if(!SelectorMatchesPseudoStates(
            rule,selector_type,id,style.classes,active_states
        )) continue;

        const int specificity = GetSelectorSpecificity(rule);
        if(rule.transitions_declared && specificity >= best_transition_specificity) {
            best_transition_specificity = specificity;
            style.transitions = rule.transitions;
        }
        if(rule.animations_declared && specificity >= best_animation_specificity) {
            best_animation_specificity = specificity;
            style.animations = rule.animations;
        }
        for(const Declaration& declaration : rule.declarations) {
            const std::size_t property_index = static_cast<std::size_t>(declaration.property);
            if(property_index >= best_specificity.size()) continue;
            if(specificity < best_specificity[property_index]) continue;
            best_specificity[property_index] = specificity;
            ApplyResolvedDeclaration(style,declaration);
        }
    }

    if(preserve_applied_scale) {
        ApplyComputedStyleScale(
            style,inherited_scale,relative_reference_includes_local_scale
        );
    }
    return style;
}

