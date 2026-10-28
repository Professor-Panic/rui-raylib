#include "css_emulator.h"

bool RuntimeStyleFlag(
    const std::optional<bool>& value,
    bool fallback
) {
    return value.has_value() ? *value : fallback;
}

static bool ConstrainHeightToParent(const ComputedStyle& style);

void ApplyRuntimeSizeToElement(
    UIElementBase& rect,
    const ComputedStyle& style,
    Rectangle parent_bounds
) {
    rect.transform.width = std::clamp(rect.transform.width,style.min_width,style.max_width);
    rect.transform.height = std::clamp(rect.transform.height,style.min_height,style.max_height);
    rect.layout_transform.width = rect.transform.width;
    rect.layout_transform.height = rect.transform.height;

    if(rect.runtime_size_override) {
        rect.transform.width = std::clamp(rect.runtime_size.x,style.min_width,style.max_width);
        rect.transform.height = std::clamp(rect.runtime_size.y,style.min_height,style.max_height);
    }

    if(RuntimeStyleFlag(style.runtime_constrain,true)) {
        rect.transform.width = std::min(rect.transform.width,parent_bounds.width);
        if(ConstrainHeightToParent(style)) {
            rect.transform.height = std::min(rect.transform.height,parent_bounds.height);
        }
    }
}

void ApplyRuntimePositionToElement(
    UIElementBase& rect,
    const ComputedStyle& style,
    Rectangle parent_bounds
) {
    rect.layout_transform.x = rect.transform.x;
    rect.layout_transform.y = rect.transform.y;
    rect.transform.x += rect.runtime_offset.x;
    rect.transform.y += rect.runtime_offset.y;

    if(RuntimeStyleFlag(style.runtime_constrain,true)) {
        const float maximum_x = std::max(parent_bounds.x,parent_bounds.x+parent_bounds.width-rect.transform.width);
        const float maximum_y = std::max(parent_bounds.y,parent_bounds.y+parent_bounds.height-rect.transform.height);
        rect.transform.x = std::clamp(rect.transform.x,parent_bounds.x,maximum_x);
        if(ConstrainHeightToParent(style)) {
            rect.transform.y = std::clamp(rect.transform.y,parent_bounds.y,maximum_y);
        }
    }
}

std::vector<float> ResolveGridTrackSizes(
    const GridTrackList& source_tracks,
    const GridTrackSize& automatic_track,
    int required_count,
    float available_space,
    float gap
) {
    GridTrackList tracks = source_tracks;
    while(static_cast<int>(tracks.size()) < std::max(1,required_count)) {
        tracks.push_back(automatic_track);
    }
    if(tracks.empty()) tracks.push_back(automatic_track);

    const float track_space = std::max(
        0.0f,
        available_space-gap*static_cast<float>(tracks.size()-1)
    );
    float fixed = 0.0f;
    float fraction_total = 0.0f;

    for(const auto& track : tracks) {
        switch(track.unit) {
            case GridTrackUnit::Pixels: fixed += track.value; break;
            case GridTrackUnit::Percent: fixed += track_space*track.value/100.0f; break;
            case GridTrackUnit::Fraction: fraction_total += std::max(0.0f,track.value); break;
            case GridTrackUnit::Auto: fraction_total += 1.0f; break;
        }
    }

    const float remaining = std::max(0.0f,track_space-fixed);
    std::vector<float> sizes;
    sizes.reserve(tracks.size());
    for(const auto& track : tracks) {
        float size = 0.0f;
        switch(track.unit) {
            case GridTrackUnit::Pixels: size = track.value; break;
            case GridTrackUnit::Percent: size = track_space*track.value/100.0f; break;
            case GridTrackUnit::Fraction:
                size = fraction_total > 0.0f ? remaining*track.value/fraction_total : 0.0f;
                break;
            case GridTrackUnit::Auto:
                size = fraction_total > 0.0f ? remaining/fraction_total : 0.0f;
                break;
        }
        sizes.push_back(std::max(0.0f,size));
    }
    return sizes;
}

float GridTrackOffset(
    const std::vector<float>& tracks,
    int index,
    float gap
) {
    float offset = gap*static_cast<float>(index);
    for(int track=0; track<index && track<static_cast<int>(tracks.size()); ++track) {
        offset += tracks[static_cast<std::size_t>(track)];
    }
    return offset;
}

float GridTrackSpan(
    const std::vector<float>& tracks,
    int start,
    int span,
    float gap
) {
    float size = gap*static_cast<float>(std::max(0,span-1));
    for(int track=start; track<start+span && track<static_cast<int>(tracks.size()); ++track) {
        size += tracks[static_cast<std::size_t>(track)];
    }
    return size;
}


void LayoutContainerPass(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    float containing_width,
    float containing_height,
    bool force_layout
);

namespace {

thread_local bool CSS_UI_AUTO_SIZE_CHANGED = false;
thread_local int CSS_UI_LAYOUT_DEPTH = 0;

bool IsAutoCSSLength(const std::optional<CSSLength>& length) {
    return
        length.has_value() &&
        length->unit == LengthUnit::Auto;
}

bool ShouldAutoWidth(const ComputedStyle& style) {
    if(
        style.resize_to_content.has_value() &&
        !*style.resize_to_content
    ) {
        return false;
    }

    return
        IsAutoCSSLength(style.width_length) ||
        style.resize_to_content.value_or(false);
}

bool ShouldAutoHeight(const ComputedStyle& style) {
    if(
        style.resize_to_content.has_value() &&
        !*style.resize_to_content
    ) {
        return false;
    }

    return
        IsAutoCSSLength(style.height_length) ||
        style.resize_to_content.value_or(false);
}

StyleSelector MakeSelectorSnapshot(
    const ComputedStyle& style,
    const UIElementBase& rect
) {
    StyleSelector selector;
    selector.selector = style.selector;
    selector.id = style.id;
    selector.classes = style.classes;
    selector.pseudo_states = rect.pseudo_states.ActiveNamesView();
    selector.sibling_index = style.sibling_index;
    selector.sibling_count = style.sibling_count;
    return selector;
}

void InheritSelectorAncestors(
    ComputedStyle& child_style,
    const UIContainer& parent,
    StyleSelectorSiblingChain previous_siblings,
    bool track_sibling_context,
    std::size_t sibling_index = 0,
    std::size_t sibling_count = 0
) {
    child_style.sibling_index = sibling_index;
    child_style.sibling_count = sibling_count;
    child_style.inherited_custom_properties = parent.style.custom_properties;
    child_style.inherited_font = parent.style.font;
    child_style.inherited_text_color = parent.style.text_color;
    child_style.selector_ancestors =
        parent.style.selector_ancestors;
    child_style.selector_ancestors.push_back(
        MakeSelectorSnapshot(parent.style,parent.rect)
    );

    if(track_sibling_context) {
        child_style.selector_ancestor_previous_siblings =
            parent.style.selector_ancestor_previous_siblings;
        child_style.selector_ancestor_previous_siblings.push_back(
            parent.style.selector_previous_siblings
        );
        child_style.selector_previous_siblings =
            std::move(previous_siblings);
    }
    else {
        child_style.selector_ancestor_previous_siblings.clear();
        child_style.selector_previous_siblings.reset();
    }
}

void InheritCompositeContentAncestors(
    ComputedStyle& content_style,
    const ComputedStyle& owner_style,
    const UIElementBase& owner_rect,
    bool track_sibling_context
) {
    content_style.inherited_custom_properties = owner_style.custom_properties;
    content_style.inherited_font = owner_style.font;
    content_style.inherited_text_color = owner_style.text_color;
    content_style.selector_ancestors =
        owner_style.selector_ancestors;
    content_style.selector_ancestors.push_back(
        MakeSelectorSnapshot(owner_style,owner_rect)
    );

    if(track_sibling_context) {
        content_style.selector_ancestor_previous_siblings =
            owner_style.selector_ancestor_previous_siblings;
        content_style.selector_ancestor_previous_siblings.push_back(
            owner_style.selector_previous_siblings
        );
    }
    else {
        content_style.selector_ancestor_previous_siblings.clear();
    }
    content_style.selector_previous_siblings.reset();
}

void ResolveElementLayoutStyle(
    ComputedStyle& style,
    UIElementBase& rect,
    const StyleSheet& style_sheet,
    SelectorType selector,
    float parent_width,
    float parent_height,
    float parent_font_size,
    float inherited_scale
) {
    style = ResolveStyle(
        style_sheet,selector,style.id,SelectorState::None,style
    );
    style = ResolveStyleWithPseudoStates(
        style_sheet,selector,style.id,rect.pseudo_states.ActiveNamesView(),style
    );
    ResolveComputedLengths(
        style,parent_width,parent_height,parent_font_size,inherited_scale
    );
    style = ResolveLayoutAnimatedStyle(rect,style_sheet,std::move(style));
    ++rect.style_generation;
}

Vector2 MeasureContainerContentSize(const UIContainer& ui) {
    float right =
        ui.rect.transform.x +
        ui.style.padding.left;

    float bottom =
        ui.rect.transform.y +
        ui.style.padding.top;

    bool found_child = false;

    for(const UIElement& element : ui.elements) {
        std::visit(
            [&](const auto& widget) {
                using T = std::decay_t<decltype(widget)>;

                const UIElementBase* rect = nullptr;
                const ComputedStyle* style = nullptr;

                if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
                    if(!widget) {
                        return;
                    }

                    rect = &widget->rect;
                    style = &widget->style;
                }
                else {
                    rect = &widget.rect;
                    style = &widget.style;
                }

                if(
                    rect == nullptr ||
                    style == nullptr ||
                    style->display == DisplayType::None
                ) {
                    return;
                }

                found_child = true;

                right = std::max(
                    right,
                    rect->transform.x +
                        ui.scroll.x +
                        rect->transform.width +
                        style->margin.right
                );

                bottom = std::max(
                    bottom,
                    rect->transform.y +
                        ui.scroll.y +
                        rect->transform.height +
                        style->margin.bottom
                );
            },
            element
        );
    }

    if(!found_child) {
        return {
            ui.style.padding.left +
                ui.style.padding.right,
            ui.style.padding.top +
                ui.style.padding.bottom
        };
    }

    return {
        std::max(
            0.0f,
            right -
                ui.rect.transform.x +
                ui.style.padding.right
        ),
        std::max(
            0.0f,
            bottom -
                ui.rect.transform.y +
                ui.style.padding.bottom
        )
    };
}

bool ResizeRectangleToContent(
    UIElementBase& rect,
    const ComputedStyle& style,
    Vector2 desired,
    float maximum_width,
    float maximum_height
) {
    bool changed = false;

    if(
        ShouldAutoWidth(style) &&
        !rect.runtime_size_override
    ) {
        float next = std::clamp(
            desired.x,
            style.min_width,
            style.max_width
        );

        if(RuntimeStyleFlag(style.runtime_constrain,true)) {
            next = std::min(next,maximum_width);
        }

        if(std::abs(next - rect.transform.width) > 0.01f) {
            rect.transform.width = next;
            rect.layout_transform.width = next;
            changed = true;
        }
    }

    if(
        ShouldAutoHeight(style) &&
        !rect.runtime_size_override
    ) {
        float next = std::clamp(
            desired.y,
            style.min_height,
            style.max_height
        );

        if(ConstrainHeightToParent(style)) {
            next = std::min(next,maximum_height);
        }

        if(std::abs(next - rect.transform.height) > 0.01f) {
            rect.transform.height = next;
            rect.layout_transform.height = next;
            changed = true;
        }
    }

    if(changed) {
        CSS_UI_AUTO_SIZE_CHANGED = true;
    }

    return changed;
}


float MeasureCompositeHeaderWidth(
    const std::string& text,
    const ComputedStyle& style,
    bool show_arrow = true,
    bool show_close = false
) {
    Font font = ResolveUIFont(style.font);

    const Vector2 measured = MeasureTextCached(
        font,
        text.c_str(),
        style.font_size,
        1.0f
    );

    float width =
        measured.x +
        style.padding.left +
        style.padding.right +
        12.0f * EffectiveComputedStyleScale(style);

    if(show_arrow) {
        width += 18.0f * EffectiveComputedStyleScale(style);
    }

    if(show_close) {
        width += style.header_height;
    }

    return width;
}

} // namespace

// Auto-height elements (wrapped text, content-sized boxes) are not squeezed to
// the space left in the parent unless `runtime-constrain: true` is explicit;
// they overflow instead, as in CSS (min-height:auto).
static bool ConstrainHeightToParent(const ComputedStyle& style) {
    if(style.runtime_constrain.has_value()) return *style.runtime_constrain;
    return !ShouldAutoHeight(style);
}

void LayoutContainerPass(UIContainer& ui,const StyleSheet& style_sheet,float containing_width ,
                    float containing_height ,bool force_layout ) {
    CSS_UI_ACTIVE_STYLE_SHEET = &style_sheet;
    CSS_UI_REQUESTED_CURSOR=(CSS_UI_REQUESTED_CURSOR);
    struct LayoutChild {
        UIElementBase* rect = nullptr;
        Rectangle* transform = nullptr;
        ComputedStyle* style = nullptr;
        UIContainer* container = nullptr;
        std::function<void()> layout_nested;
    };

    if(containing_width < 0.0f) {
        containing_width = static_cast<float>(GetScreenWidth());
    }

    if(containing_height < 0.0f) {
        containing_height = static_cast<float>(GetScreenHeight());
    }

    const bool detect_layout_mutations =
        CSS_UI_DETECT_LAYOUT_MUTATIONS;

    const std::size_t current_signature =
        !force_layout && detect_layout_mutations
            ? ComputeContainerLayoutSignature(ui)
            : ui.last_layout_signature;

    const bool containing_size_changed =
        containing_width != ui.last_containing_width ||
        containing_height != ui.last_containing_height;

    const bool stylesheet_changed =
        ui.last_layout_revision != style_sheet.revision;

    if(
        !force_layout &&
        !ui.layout_dirty &&
        !containing_size_changed &&
        !stylesheet_changed &&
        (
            !detect_layout_mutations ||
            current_signature == ui.last_layout_signature
        )
    ) {
        return;
    }

    auto commit_layout_state = [&]() {
        ui.content_size =
            MeasureContainerContentSize(ui);

        ResizeRectangleToContent(
            ui.rect,
            ui.style,
            ui.content_size,
            containing_width,
            containing_height
        );

        const bool scrollable =
            ui.style.overflow == OverflowType::Auto ||
            ui.style.overflow == OverflowType::Scroll;

        if(scrollable) {
            ui.maximum_scroll = {
                std::max(
                    0.0f,
                    ui.content_size.x -
                        ui.rect.transform.width
                ),
                std::max(
                    0.0f,
                    ui.content_size.y -
                        ui.rect.transform.height
                )
            };
            ui.scroll.x = std::clamp(
                ui.scroll.x,
                0.0f,
                ui.maximum_scroll.x
            );

            ui.scroll.y = std::clamp(
                ui.scroll.y,
                0.0f,
                ui.maximum_scroll.y
            );
        }
        else {
            ui.scroll = {};
            ui.maximum_scroll = {};
        }

        ui.layout_dirty = false;
        ui.last_containing_width = containing_width;
        ui.last_containing_height = containing_height;
        ui.last_layout_revision = style_sheet.revision;
        ui.last_layout_signature = detect_layout_mutations
            ? ComputeContainerLayoutSignature(ui)
            : 0;
    };

    ResolveComputedLengths(
        ui.style,
        containing_width,
        containing_height,
        CSS_UI_ROOT_FONT_SIZE,
        ui.inherited_scale,
        true
    );

    // Auto-sized containers lay out against the space offered by their
    // parent instead of their stale rectangle from the previous pass.
    // This prevents flex shrink from forcing measured content back into
    // the default 200x100 rectangle before content measurement occurs.
    const float layout_outer_width =
        ShouldAutoWidth(ui.style)
            ? containing_width
            : ui.rect.transform.width;

    const float layout_outer_height =
        ShouldAutoHeight(ui.style)
            ? containing_height
            : ui.rect.transform.height;

    Rectangle inner = {
        ui.rect.transform.x + ui.style.padding.left,
        ui.rect.transform.y + ui.style.padding.top,
        std::max(
            0.0f,
            layout_outer_width -
                ui.style.padding.left -
                ui.style.padding.right
        ),
        std::max(
            0.0f,
            layout_outer_height -
                ui.style.padding.top -
                ui.style.padding.bottom
        )
    };

    ui.scroll.x = std::clamp(ui.scroll.x,0.0f,ui.maximum_scroll.x);
    ui.scroll.y = std::clamp(ui.scroll.y,0.0f,ui.maximum_scroll.y);

    auto apply_scroll = [&](LayoutChild& child) {
        if(
            ui.style.overflow == OverflowType::Auto ||
            ui.style.overflow == OverflowType::Scroll
        ) {
            child.transform->x -= ui.scroll.x;
            child.transform->y -= ui.scroll.y;
        }
    };

    std::vector<LayoutChild> flow_children;
    std::vector<LayoutChild> absolute_children;
    flow_children.reserve(ui.elements.size());
    absolute_children.reserve(ui.elements.size());

    const bool track_sibling_context =
        style_sheet.uses_sibling_combinators;
    StyleSelectorSiblingChain previous_siblings;
    const bool track_structure = style_sheet.uses_structural_selectors;
    const std::size_t sibling_total = ui.elements.size();
    std::size_t sibling_position = 0;

    for(auto& element : ui.elements) {
        LayoutChild child;
        ++sibling_position;

        std::visit(
            [&](auto& widget) {
                using T = std::decay_t<decltype(widget)>;

                if constexpr(
                    IsUICompositePointerV<T> ||
                    IsUICustomElementPointerV<T>
                ) {
                    if(!widget) return;
                    InheritSelectorAncestors(
                        widget->style,ui,previous_siblings,
                        track_sibling_context,
                        track_structure ? sibling_position : 0,
                        track_structure ? sibling_total : 0
                    );
                }
                else {
                    InheritSelectorAncestors(
                        widget.style,ui,previous_siblings,
                        track_sibling_context,
                        track_structure ? sibling_position : 0,
                        track_structure ? sibling_total : 0
                    );
                }

                if constexpr(std::is_same_v<T, UIButton>) {
                    ResolveElementLayoutStyle(
                        widget.style,widget.rect,style_sheet,
                        SelectorType::Button,inner.width,inner.height,
                        ui.style.font_size,EffectiveComputedStyleScale(ui.style)
                    );

                    Font button_font =
                        ResolveUIFont(widget.style.font);

                    Vector2 button_text_size =
                        MeasureTextCached(
                            button_font,
                            widget.text.c_str(),
                            widget.style.font_size,
                            1.0f
                        );

                    widget.rect.transform.width =
                        ShouldAutoWidth(widget.style)
                            ? button_text_size.x +
                                widget.style.padding.left +
                                widget.style.padding.right +
                                16.0f
                            : widget.style.width;

                    widget.rect.transform.height =
                        ShouldAutoHeight(widget.style)
                            ? button_text_size.y +
                                widget.style.padding.top +
                                widget.style.padding.bottom +
                                8.0f
                            : widget.style.height;

                    ApplyRuntimeSizeToElement(widget.rect,widget.style,inner);

                    child = {
                        &widget.rect,
                        &widget.rect.transform,
                        &widget.style,
                        nullptr,
                        {}
                    };
                }
                else if constexpr(std::is_same_v<T, UILabel>) {
                    ResolveElementLayoutStyle(
                        widget.style,widget.rect,style_sheet,
                        SelectorType::Label,inner.width,inner.height,
                        ui.style.font_size,EffectiveComputedStyleScale(ui.style)
                    );

                    Font font =
                        ResolveUIFont(widget.style.font);

                    widget.text_size = MeasureTextCached(
                        font,
                        widget.text.c_str(),
                        widget.style.font_size,
                        1.0f
                    );

                    const float intrinsic_width =
                        widget.text_size.x +
                        widget.style.padding.left +
                        widget.style.padding.right;

                    const float intrinsic_height =
                        widget.text_size.y +
                        widget.style.padding.top +
                        widget.style.padding.bottom;

                    widget.rect.transform.width =
                        widget.style.display == DisplayType::Inline ||
                        ShouldAutoWidth(widget.style)
                            ? intrinsic_width
                            : widget.style.width;

                    widget.rect.transform.height =
                        widget.style.display == DisplayType::Inline ||
                        ShouldAutoHeight(widget.style)
                            ? intrinsic_height
                            : widget.style.height;

                    ApplyRuntimeSizeToElement(widget.rect,widget.style,inner);

                    // A label with no explicit height keeps its measured text
                    // height. Without this, ApplyRuntimeSizeToElement squashed
                    // it to the parent's height (ShouldAutoHeight is only true
                    // for an explicit `height: auto`), so tall text never made
                    // its scroll container scrollable.
                    if(
                        !widget.style.height_length.has_value() &&
                        !widget.rect.runtime_size_override
                    ) {
                        const float label_height = std::clamp(
                            intrinsic_height,
                            widget.style.min_height,
                            widget.style.max_height
                        );
                        widget.rect.transform.height = label_height;
                        widget.rect.layout_transform.height = label_height;
                    }

                    child = {
                        &widget.rect,
                        &widget.rect.transform,
                        &widget.style,
                        nullptr,
                        {}
                    };
                }
                else if constexpr(IsUIInputV<T>) {
                    ResolveElementLayoutStyle(
                        widget.style,widget.rect,style_sheet,
                        SelectorType::Input,inner.width,inner.height,
                        ui.style.font_size,EffectiveComputedStyleScale(ui.style)
                    );

                    if(widget.style.input_step_buttons.has_value()) {
                        widget.show_step_buttons =
                            *widget.style.input_step_buttons;
                    }

                    if(widget.style.input_value_box.has_value()) {
                        widget.show_value_box =
                            *widget.style.input_value_box;
                    }

                    if(widget.style.input_drag_speed.has_value()) {
                        widget.drag_speed =
                            *widget.style.input_drag_speed;
                    }

                    if(widget.style.input_slider_show_value.has_value()) {
                        widget.slider_show_value =
                            *widget.style.input_slider_show_value;
                    }

                    if(widget.style.input_slider_value_width.has_value()) {
                        widget.slider_value_width =
                            *widget.style.input_slider_value_width *
                            EffectiveComputedStyleScale(widget.style);
                    }

                    if(widget.style.input_slider_click_to_set.has_value()) {
                        widget.slider_click_to_set =
                            *widget.style.input_slider_click_to_set;
                    }
                    using InputValue =typename T::ValueType;
                    if constexpr(std::is_arithmetic_v<InputValue> &&!std::is_same_v<InputValue,bool>) {
                        if(widget.style.input_step.has_value()) {
                            widget.step = static_cast<InputValue>(
                                *widget.style.input_step
                            );
                        }
                    }

                    if(!ShouldAutoWidth(widget.style)) {
                        widget.rect.transform.width =
                            widget.style.width;
                    }

                    if(!ShouldAutoHeight(widget.style)) {
                        widget.rect.transform.height = widget.style.height;
                    }
                    else {
                        Font input_font = ResolveUIFont(widget.style.font);
                        const float line_height = MeasureTextCached(
                            input_font, "Ag", widget.style.font_size, 1.0f
                        ).y;

                        widget.rect.transform.height =
                            line_height +
                            widget.style.padding.top +
                            widget.style.padding.bottom +
                            8.0f;                       // same slack the Button branch uses
                    }

                    ApplyRuntimeSizeToElement(widget.rect,widget.style,inner);

                    child = {
                        &widget.rect,
                        &widget.rect.transform,
                        &widget.style,
                        nullptr,
                        {}
                    };
                }
                else if constexpr(
                    std::is_same_v<T,UISelectable> ||
                    std::is_same_v<T,UIComboBox> ||
                    IsUIRangeInputV<T> ||
                    std::is_same_v<T,UICanvas> ||
                    std::is_same_v<T,UIImage> ||
                    std::is_same_v<T,UISplitter>
                ) {
                    SelectorType selector = SelectorType::Selectable;
                    if constexpr(std::is_same_v<T,UIComboBox>) selector = SelectorType::ComboBox;
                    else if constexpr(IsUIRangeInputV<T>) selector = SelectorType::RangeInput;
                    else if constexpr(std::is_same_v<T,UICanvas>) selector = SelectorType::Canvas;
                    else if constexpr(std::is_same_v<T,UIImage>) selector = SelectorType::Image;
                    else if constexpr(std::is_same_v<T,UISplitter>) selector = SelectorType::Splitter;

                    ResolveElementLayoutStyle(
                        widget.style,widget.rect,style_sheet,selector,
                        inner.width,inner.height,ui.style.font_size,
                        EffectiveComputedStyleScale(ui.style)
                    );
                    if(!ShouldAutoWidth(widget.style)) {
                        widget.rect.transform.width =
                            widget.style.width;
                    }

                    if(!ShouldAutoHeight(widget.style)) {
                        widget.rect.transform.height =
                            widget.style.height;
                    }

                    if constexpr(std::is_same_v<T,UIImage>) {
                        const Rectangle source =
                            widget.use_source
                                ? widget.source
                                : Rectangle{
                                    0.0f,
                                    0.0f,
                                    static_cast<float>(widget.texture.width),
                                    static_cast<float>(widget.texture.height)
                                };

                        const float frame_width = widget.style.image_frame_size.x > 0.0f
                            ? widget.style.image_frame_size.x
                            : widget.frame_size.x;
                        const float frame_height = widget.style.image_frame_size.y > 0.0f
                            ? widget.style.image_frame_size.y
                            : widget.frame_size.y;
                        const float intrinsic_width = frame_width > 0.0f
                            ? frame_width
                            : std::abs(source.width);
                        const float intrinsic_height = frame_height > 0.0f
                            ? frame_height
                            : std::abs(source.height);

                        if(ShouldAutoWidth(widget.style)) {
                            widget.rect.transform.width =
                                intrinsic_width +
                                widget.style.padding.left +
                                widget.style.padding.right;
                        }

                        if(ShouldAutoHeight(widget.style)) {
                            widget.rect.transform.height =
                                intrinsic_height +
                                widget.style.padding.top +
                                widget.style.padding.bottom;
                        }
                    }
                    if constexpr(std::is_same_v<T,UISplitter>) {
                        const UISplitterOrientation orientation =
                            widget.orientation == UISplitterOrientation::Auto
                                ? (ui.direction == LayoutDirection::Row
                                    ? UISplitterOrientation::Vertical
                                    : UISplitterOrientation::Horizontal)
                                : widget.orientation;
                        if(orientation == UISplitterOrientation::Vertical) {
                            widget.rect.transform.width = widget.style.splitter_size;
                        }
                        else {
                            widget.rect.transform.height = widget.style.splitter_size;
                        }
                    }
                    ApplyRuntimeSizeToElement(widget.rect,widget.style,inner);
                    child = {
                        &widget.rect,
                        &widget.rect.transform,
                        &widget.style,
                        nullptr,
                        {}
                    };
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(!widget) return;
                    ResolveElementLayoutStyle(
                        widget->style,widget->rect,style_sheet,widget->Selector(),
                        inner.width,inner.height,ui.style.font_size,
                        EffectiveComputedStyleScale(ui.style)
                    );
                    const Vector2 measured = widget->Measure(
                        style_sheet,inner.width,inner.height
                    );
                    widget->rect.transform.width = ShouldAutoWidth(widget->style)
                        ? measured.x : widget->style.width;
                    widget->rect.transform.height = ShouldAutoHeight(widget->style)
                        ? measured.y : widget->style.height;
                    ApplyRuntimeSizeToElement(widget->rect,widget->style,inner);
                    child.rect = &widget->rect;
                    child.transform = &widget->rect.transform;
                    child.style = &widget->style;
                    child.layout_nested = [widget,&style_sheet]() {
                        widget->Layout(style_sheet,widget->rect.transform);
                    };
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(!widget) return;
                    ResolveElementLayoutStyle(
                        widget->style,widget->rect,style_sheet,
                        SelectorType::TabBar,inner.width,inner.height,
                        ui.style.font_size,EffectiveComputedStyleScale(ui.style)
                    );
                    widget->selected_index = widget->tabs.empty()
                        ? -1
                        : std::clamp(widget->selected_index,0,static_cast<int>(widget->tabs.size())-1);
                    if(!ShouldAutoWidth(widget->style)) {
                        widget->rect.transform.width =
                            widget->style.width;
                    }

                    if(!ShouldAutoHeight(widget->style)) {
                        widget->rect.transform.height =
                            widget->style.height;
                    }
                    ApplyRuntimeSizeToElement(widget->rect,widget->style,inner);
                    child.rect = &widget->rect;
                    child.transform = &widget->rect.transform;
                    child.style = &widget->style;
                    child.layout_nested = [widget,&style_sheet,inner]() {
                        UITab* selected = widget->SelectedTab();
                        if(selected == nullptr) return;
                        Rectangle body = {
                            widget->rect.transform.x,
                            widget->rect.transform.y + widget->style.tab_height,
                            ShouldAutoWidth(widget->style)
                                ? inner.width
                                : widget->rect.transform.width,
                            std::max(
                                0.0f,
                                (
                                    ShouldAutoHeight(widget->style)
                                        ? inner.height
                                        : widget->rect.transform.height
                                ) - widget->style.tab_height
                            )
                        };
                        ComputedStyle content_style = selected->content.style;
                        InheritCompositeContentAncestors(
                            content_style,
                            widget->style,
                            widget->rect,
                            style_sheet.uses_sibling_combinators
                        );
                        AddCSSClass(content_style.classes,CSS_UI_TAB_CONTENT_PART);
                        selected->content.style = ResolveStyle(
                            style_sheet,
                            SelectorType::TabBar,
                            widget->style.id,
                            SelectorState::None,
                            content_style
                        );
                        selected->content.inherited_scale =
                            EffectiveComputedStyleScale(widget->style);
                        selected->content.rect.transform = body;
                        LayoutContainerPass(
                            selected->content,
                            style_sheet,
                            body.width,
                            body.height,
                            true
                        );

                        float header_width = 0.0f;

                        for(const UITab& tab : widget->tabs) {
                            header_width +=
                                MeasureCompositeHeaderWidth(
                                    tab.title,
                                    widget->style,
                                    false,
                                    tab.closable ||
                                        widget->style.tab_closable.value_or(false)
                                );
                        }

                        ResizeRectangleToContent(
                            widget->rect,
                            widget->style,
                            {
                                std::max(
                                    header_width,
                                    selected->content.content_size.x
                                ),
                                widget->style.tab_height +
                                    selected->content.content_size.y
                            },
                            inner.width,
                            inner.height
                        );
                    };
                }
                else if constexpr(
                    std::is_same_v<
                        T,
                        std::shared_ptr<UIContainer>
                    >
                ) {
                    if(!widget) {
                        return;
                    }

                    ResolveElementLayoutStyle(
                        widget->style,widget->rect,style_sheet,
                        SelectorType::Container,inner.width,inner.height,
                        ui.style.font_size,EffectiveComputedStyleScale(ui.style)
                    );

                    widget->inherited_scale =
                        EffectiveComputedStyleScale(ui.style);

                    if(!ShouldAutoWidth(widget->style)) {
                        widget->rect.transform.width =
                            widget->style.width;
                    }

                    if(!ShouldAutoHeight(widget->style)) {
                        widget->rect.transform.height =
                            widget->style.height;
                    }

                    widget->direction =
                        widget->style.flex_direction;

                    ApplyRuntimeSizeToElement(widget->rect,widget->style,inner);

                    child = {
                        &widget->rect,
                        &widget->rect.transform,
                        &widget->style,
                        widget.get(),
                        {}
                    };
                }
                else if constexpr(
                    std::is_same_v<T,std::shared_ptr<UICollapsible>> ||
                    std::is_same_v<T,std::shared_ptr<UITreeNode>> ||
                    std::is_same_v<T,std::shared_ptr<UIMenu>> ||
                    std::is_same_v<T,std::shared_ptr<UIPopup>> ||
                    std::is_same_v<T,std::shared_ptr<UIWindow>>
                ) {
                    if(!widget) return;

                    SelectorType selector = SelectorType::Collapsible;
                    if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) selector = SelectorType::TreeNode;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) selector = SelectorType::Menu;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) selector = SelectorType::Popup;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) selector = SelectorType::Window;

                    if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                        if(!widget->open) return;
                        if(widget->style.position == PositionType::Static) {
                            widget->style.position = PositionType::Absolute;
                        }
                    }
                    if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                        if(!widget->open) return;
                        if(widget->style.position == PositionType::Static) {
                            widget->style.position = PositionType::Absolute;
                        }
                    }

                    ResolveElementLayoutStyle(
                        widget->style,widget->rect,style_sheet,selector,
                        inner.width,inner.height,ui.style.font_size,
                        EffectiveComputedStyleScale(ui.style)
                    );
                    if(!ShouldAutoWidth(widget->style)) {
                        widget->rect.transform.width =
                            widget->style.width;
                    }

                    if(!ShouldAutoHeight(widget->style)) {
                        widget->rect.transform.height =
                            widget->style.height;
                    }

                    if constexpr(
                        std::is_same_v<T,std::shared_ptr<UICollapsible>> ||
                        std::is_same_v<T,std::shared_ptr<UITreeNode>>
                    ) {
                        if(!widget->open) widget->rect.transform.height = widget->style.header_height;
                    }
                    if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                        widget->rect.transform.height = widget->style.header_height;
                    }
                    if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                        if(widget->collapsed) widget->rect.transform.height = widget->style.header_height;
                    }

                    ApplyRuntimeSizeToElement(widget->rect,widget->style,inner);
                    child.rect = &widget->rect;
                    child.transform = &widget->rect.transform;
                    child.style = &widget->style;
                    child.layout_nested = [widget,&style_sheet,inner]() {
                        const float header =
                            std::max(
                                1.0f,
                                widget->style.header_height
                            );

                        Rectangle body = {
                            widget->rect.transform.x,
                            widget->rect.transform.y + header,
                            ShouldAutoWidth(widget->style)
                                ? inner.width
                                : widget->rect.transform.width,
                            std::max(
                                0.0f,
                                (
                                    ShouldAutoHeight(widget->style)
                                        ? inner.height
                                        : widget->rect.transform.height
                                ) - header
                            )
                        };

                        bool show_content = true;
                        SelectorType content_selector =
                            SelectorType::Container;

                        if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UICollapsible>
                            >
                        ) {
                            show_content = widget->open;
                            content_selector =
                                SelectorType::Collapsible;
                        }
                        else if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UITreeNode>
                            >
                        ) {
                            show_content = widget->open;
                            content_selector =
                                SelectorType::TreeNode;
                            body.x += widget->style.indent;
                            body.width = std::max(
                                0.0f,
                                body.width -
                                    widget->style.indent
                            );
                        }
                        else if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UIMenu>
                            >
                        ) {
                            show_content = widget->open;
                            content_selector =
                                SelectorType::Menu;
                            body.x =
                                widget->rect.transform.x;
                            body.y =
                                widget->rect.transform.y +
                                widget->rect.transform.height;
                            body.width =
                                widget->style.popup_width;
                            body.height =
                                widget->style.popup_max_height;
                        }
                        else if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UIPopup>
                            >
                        ) {
                            show_content = widget->open;
                            content_selector =
                                SelectorType::Popup;
                        }
                        else if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UIWindow>
                            >
                        ) {
                            show_content =
                                widget->open &&
                                !widget->collapsed;
                            content_selector =
                                SelectorType::Window;
                        }

                        Vector2 content_size = {};

                        if(show_content) {
                            ComputedStyle content_style =
                                widget->content.style;

                            InheritCompositeContentAncestors(
                                content_style,
                                widget->style,
                                widget->rect,
                                style_sheet.uses_sibling_combinators
                            );

                            AddCSSClass(
                                content_style.classes,
                                CSS_UI_POPUP_PART
                            );

                            widget->content.style = ResolveStyle(
                                style_sheet,
                                content_selector,
                                widget->style.id,
                                SelectorState::None,
                                content_style
                            );

                            widget->content.style = ResolveStyle(
                                style_sheet,
                                SelectorType::Container,
                                widget->content.style.id,
                                SelectorState::None,
                                widget->content.style
                            );

                            widget->content.inherited_scale =
                                EffectiveComputedStyleScale(
                                    widget->style
                                );

                            widget->content.rect.transform =
                                body;

                            LayoutContainerPass(
                                widget->content,
                                style_sheet,
                                body.width,
                                body.height,
                                true
                            );

                            content_size =
                                widget->content.content_size;
                        }

                        std::string header_text;

                        if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UICollapsible>
                            > ||
                            std::is_same_v<
                                T,
                                std::shared_ptr<UITreeNode>
                            > ||
                            std::is_same_v<
                                T,
                                std::shared_ptr<UIMenu>
                            >
                        ) {
                            header_text = widget->text;
                        }
                        else {
                            header_text = widget->title;
                        }

                        const bool show_arrow =
                            !std::is_same_v<
                                T,
                                std::shared_ptr<UIPopup>
                            >;

                        const bool show_close =
                            std::is_same_v<
                                T,
                                std::shared_ptr<UIWindow>
                            > &&
                            widget->style.closable.value_or(false);

                        float content_width =
                            content_size.x;

                        if constexpr(
                            std::is_same_v<
                                T,
                                std::shared_ptr<UITreeNode>
                            >
                        ) {
                            content_width +=
                                widget->style.indent;
                        }

                        float desired_height = header;

                        if constexpr(
                            !std::is_same_v<
                                T,
                                std::shared_ptr<UIMenu>
                            >
                        ) {
                            if(show_content) {
                                desired_height +=
                                    content_size.y;
                            }
                        }

                        ResizeRectangleToContent(
                            widget->rect,
                            widget->style,
                            {
                                std::max(
                                    MeasureCompositeHeaderWidth(
                                        header_text,
                                        widget->style,
                                        show_arrow,
                                        show_close
                                    ),
                                    content_width
                                ),
                                desired_height
                            },
                            inner.width,
                            inner.height
                        );
                    };
                }
            },
            element
        );

        if(
            track_sibling_context &&
            child.rect != nullptr &&
            child.transform != nullptr &&
            child.style != nullptr
        ) {
            previous_siblings = AppendStyleSelectorSibling(
                std::move(previous_siblings),
                MakeSelectorSnapshot(*child.style,*child.rect)
            );
        }

        if(
            child.transform == nullptr ||
            child.style == nullptr ||
            child.style->display == DisplayType::None
        ) {
            continue;
        }

        if(child.style->position == PositionType::Absolute) {
            absolute_children.push_back(child);
        }
        else {
            flow_children.push_back(child);
        }
    }

    auto apply_relative_position = [](LayoutChild& child) {
        if(child.style->position != PositionType::Relative) {
            return;
        }

        child.transform->x +=
            child.style->left -
            child.style->right;

        child.transform->y +=
            child.style->top -
            child.style->bottom;
    };

    auto layout_absolute_children = [&]() {
        for(auto& child : absolute_children) {
            // Optional lengths distinguish an explicit zero from an omitted edge.
            const bool has_left =
                child.style->left_length.has_value() || child.style->left != 0.0f;
            const bool has_right =
                child.style->right_length.has_value() || child.style->right != 0.0f;
            const bool has_top =
                child.style->top_length.has_value() || child.style->top != 0.0f;
            const bool has_bottom =
                child.style->bottom_length.has_value() || child.style->bottom != 0.0f;

            if(has_left && has_right) {
                child.transform->x = inner.x+child.style->left;
                child.transform->width = std::max(
                    0.0f,inner.width-child.style->left-child.style->right
                );
            }
            else if(has_left) {
                child.transform->x = inner.x+child.style->left;
            }
            else if(has_right) {
                child.transform->x =
                    inner.x+inner.width-child.style->right-child.transform->width;
            }
            else {
                child.transform->x = inner.x;
            }

            if(has_top && has_bottom) {
                child.transform->y = inner.y+child.style->top;
                child.transform->height = std::max(
                    0.0f,inner.height-child.style->top-child.style->bottom
                );
            }
            else if(has_top) {
                child.transform->y = inner.y+child.style->top;
            }
            else if(has_bottom) {
                child.transform->y =
                    inner.y+inner.height-child.style->bottom-child.transform->height;
            }
            else {
                child.transform->y = inner.y;
            }

            ApplyRuntimeSizeToElement(*child.rect,*child.style,inner);
            ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
            if (child.style->transform_operations||child.style->perspective.has_value()) {
                EvaluateTransformMatrix(*child.style, *child.transform, inner.width, inner.height, child.style->font_size, CSS_UI_ROOT_FONT_SIZE, CSS_UI_VIEWPORT_WIDTH, CSS_UI_VIEWPORT_HEIGHT);
            }
            apply_scroll(child);

            if(child.container != nullptr) {
                LayoutContainerPass(
                    *child.container,
                    style_sheet,
                    // By this point this pass has already resolved child.transform
                    // (explicit size, flex-basis+grow/shrink, stretch, grid track, or
                    // absolute left/right/top/bottom) - that resolved size is always
                    // the correct space to recurse into, never the parent's raw inner
                    // box. Falling back to inner.width/height here whenever the child
                    // merely lacks an explicit width/height (ShouldAutoWidth/Height)
                    // fed the *parent's entire remaining space* into children that had
                    // already been sized to a much smaller share of it (e.g. a flex:1
                    // panel that only got part of its parent's height), which is what
                    // let auto-height flex/grid items balloon past their allotted box.
                    child.transform->width,
                    child.transform->height,
                    true
                );
            }
            if(child.layout_nested) child.layout_nested();
        }
    };

    if(ui.style.display == DisplayType::Grid) {
        int columns = std::max(
            1,
            static_cast<int>(ui.style.grid_column_tracks.size())
        );
        int rows = std::max(1,static_cast<int>(ui.style.grid_row_tracks.size()));

        if(ui.style.grid_auto_flow == GridAutoFlow::Row) {
            rows = std::max(rows,static_cast<int>(std::ceil(
                static_cast<float>(flow_children.size())/static_cast<float>(columns)
            )));
        }
        else {
            columns = std::max(columns,static_cast<int>(std::ceil(
                static_cast<float>(flow_children.size())/static_cast<float>(rows)
            )));
        }

        const float horizontal_gap = ui.style.column_gap;
        const float vertical_gap = ui.style.row_gap;
        const std::vector<float> column_sizes = ResolveGridTrackSizes(
            ui.style.grid_column_tracks,
            ui.style.grid_auto_column,
            columns,
            inner.width,
            horizontal_gap
        );
        std::vector<float> row_sizes = ResolveGridTrackSizes(
            ui.style.grid_row_tracks,
            ui.style.grid_auto_row,
            rows,
            inner.height,
            vertical_gap
        );

        float grid_cross_start = 0.0f;
        float grid_row_spacing = vertical_gap;
        if(ui.style.align_content.has_value() && !row_sizes.empty()) {
            float used_cross = vertical_gap * static_cast<float>(row_sizes.size()-1);
            for(float size : row_sizes) used_cross += size;
            float free_cross = std::max(0.0f,inner.height-used_cross);

            switch(*ui.style.align_content) {
                case AlignContent::Stretch:
                    // Fixed grid tracks do not become flexible merely because
                    // align-content is stretch. Auto/fr tracks already consume
                    // available space in ResolveGridTrackSizes().
                    break;
                case AlignContent::Center:
                    grid_cross_start = free_cross*0.5f;
                    break;
                case AlignContent::FlexEnd:
                    grid_cross_start = free_cross;
                    break;
                case AlignContent::SpaceBetween:
                    if(row_sizes.size()>1) {
                        grid_row_spacing += free_cross/static_cast<float>(row_sizes.size()-1);
                    }
                    break;
                case AlignContent::SpaceAround:
                    if(!row_sizes.empty()) {
                        const float extra = free_cross/static_cast<float>(row_sizes.size());
                        grid_row_spacing += extra;
                        grid_cross_start = extra*0.5f;
                    }
                    break;
                case AlignContent::SpaceEvenly:
                    if(!row_sizes.empty()) {
                        const float extra = free_cross/static_cast<float>(row_sizes.size()+1);
                        grid_row_spacing += extra;
                        grid_cross_start = extra;
                    }
                    break;
                case AlignContent::FlexStart:
                default:
                    break;
            }
        }

        for(std::size_t index = 0;
            index < flow_children.size();
            index++
        ) {
            auto& child = flow_children[index];

            int column = child.style->grid_column > 0
                ? child.style->grid_column - 1
                : (
                    ui.style.grid_auto_flow == GridAutoFlow::Row
                        ? static_cast<int>(index) % columns
                        : static_cast<int>(index) / rows
                );

            int row = child.style->grid_row > 0
                ? child.style->grid_row - 1
                : (
                    ui.style.grid_auto_flow == GridAutoFlow::Row
                        ? static_cast<int>(index) / columns
                        : static_cast<int>(index) % rows
                );

            column = std::clamp(
                column,
                0,
                columns - 1
            );

            row = std::clamp(
                row,
                0,
                rows - 1
            );

            int column_span = std::clamp(
                child.style->grid_column_span,
                1,
                columns - column
            );

            int row_span = std::clamp(
                child.style->grid_row_span,
                1,
                rows - row
            );

            Rectangle cell = {
                inner.x + GridTrackOffset(column_sizes,column,horizontal_gap),
                inner.y + grid_cross_start + GridTrackOffset(row_sizes,row,grid_row_spacing),
                GridTrackSpan(column_sizes,column,column_span,horizontal_gap),
                GridTrackSpan(row_sizes,row,row_span,grid_row_spacing)
            };

            const bool auto_left =
                IsAutoCSSLength(child.style->margin_lengths.left);
            const bool auto_right =
                IsAutoCSSLength(child.style->margin_lengths.right);
            const bool auto_top =
                IsAutoCSSLength(child.style->margin_lengths.top);
            const bool auto_bottom =
                IsAutoCSSLength(child.style->margin_lengths.bottom);

            const float fixed_left = auto_left ? 0.0f : child.style->margin.left;
            const float fixed_right = auto_right ? 0.0f : child.style->margin.right;
            const float fixed_top = auto_top ? 0.0f : child.style->margin.top;
            const float fixed_bottom = auto_bottom ? 0.0f : child.style->margin.bottom;

            float content_width = std::max(
                0.0f,
                cell.width - fixed_left - fixed_right
            );

            float content_height = std::max(
                0.0f,
                cell.height - fixed_top - fixed_bottom
            );

            AlignSelf alignment =
                child.style->align_self;

            AlignItems vertical_alignment =
                ui.style.align_items;

            if(alignment != AlignSelf::Auto) {
                vertical_alignment = static_cast<AlignItems>(
                    static_cast<int>(alignment) - 1
                );
            }

            AlignItems horizontal_alignment = ui.style.justify_items;
            if(child.style->justify_self != JustifySelf::Auto) {
                horizontal_alignment = static_cast<AlignItems>(
                    static_cast<int>(child.style->justify_self)-1
                );
            }
            const bool horizontal_auto_margin = auto_left || auto_right;
            const bool vertical_auto_margin = auto_top || auto_bottom;

            if(
                horizontal_alignment == AlignItems::Stretch &&
                !horizontal_auto_margin
            ) {
                child.transform->width = content_width;
            }

            if(
                vertical_alignment == AlignItems::Stretch &&
                !vertical_auto_margin
            ) {
                child.transform->height = content_height;
            }

            if(horizontal_auto_margin) {
                const float free_horizontal = std::max(
                    0.0f,
                    cell.width - fixed_left - fixed_right - child.transform->width
                );
                const int auto_count =
                    static_cast<int>(auto_left) + static_cast<int>(auto_right);
                const float auto_share = auto_count > 0
                    ? free_horizontal / static_cast<float>(auto_count)
                    : 0.0f;
                child.transform->x =
                    cell.x + fixed_left + (auto_left ? auto_share : 0.0f);
            }
            else {
                child.transform->x = cell.x + fixed_left;
                if(horizontal_alignment == AlignItems::Center) {
                    child.transform->x +=
                        (content_width-child.transform->width)*0.5f;
                }
                else if(horizontal_alignment == AlignItems::FlexEnd) {
                    child.transform->x =
                        cell.x+cell.width-fixed_right-child.transform->width;
                }
            }

            if(vertical_auto_margin) {
                const float free_vertical = std::max(
                    0.0f,
                    cell.height - fixed_top - fixed_bottom - child.transform->height
                );
                const int auto_count =
                    static_cast<int>(auto_top) + static_cast<int>(auto_bottom);
                const float auto_share = auto_count > 0
                    ? free_vertical / static_cast<float>(auto_count)
                    : 0.0f;
                child.transform->y =
                    cell.y + fixed_top + (auto_top ? auto_share : 0.0f);
            }
            else {
                child.transform->y = cell.y + fixed_top;
                if(vertical_alignment == AlignItems::Center) {
                    child.transform->y +=
                        (content_height-child.transform->height)*0.5f;
                }
                else if(vertical_alignment == AlignItems::FlexEnd) {
                    child.transform->y =
                        cell.y+cell.height-fixed_bottom-child.transform->height;
                }
            }

            ApplyRuntimeSizeToElement(*child.rect,*child.style,cell);

            apply_relative_position(child);
            ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
            if (child.style->transform_operations) {
                EvaluateTransformMatrix(*child.style, *child.transform, inner.width, inner.height, child.style->font_size, CSS_UI_ROOT_FONT_SIZE, CSS_UI_VIEWPORT_WIDTH, CSS_UI_VIEWPORT_HEIGHT);
            }
            apply_scroll(child);

            if(child.container != nullptr) {
                LayoutContainerPass(
                    *child.container,
                    style_sheet,
                    // See the comment on the first recursive LayoutContainerPass
                    // call in this file: always recurse with the size this pass
                    // already resolved for this child, not the parent's raw inner box.
                    child.transform->width,
                    child.transform->height,
                    true
                );
            }
            if(child.layout_nested) child.layout_nested();
        }

        layout_absolute_children();
        commit_layout_state();
        return;
    }

    FlexDirection direction =
        ui.style.display == DisplayType::Flex
            ? ui.style.flex_direction
            : ui.direction;

    const float available_main =
        direction == FlexDirection::Row
            ? inner.width
            : inner.height;
    const float available_cross =
        direction == FlexDirection::Row
            ? inner.height
            : inner.width;

    const bool cross_axis_is_auto =
        direction == FlexDirection::Row
            ? ShouldAutoHeight(ui.style)
            : ShouldAutoWidth(ui.style);

    // In flex layout CSS gap is axis-aware: row-gap separates rows and
    // column-gap separates columns. For a column flex container those axes
    // swap relative to main/cross.
    const float main_gap =
        direction == FlexDirection::Row
            ? ui.style.column_gap
            : ui.style.row_gap;
    const float cross_gap =
        direction == FlexDirection::Row
            ? ui.style.row_gap
            : ui.style.column_gap;

    const bool wrapping =
        ui.style.display == DisplayType::Flex &&
        ui.style.flex_wrap != FlexWrap::NoWrap;
    const bool wrap_reverse =
        ui.style.display == DisplayType::Flex &&
        ui.style.flex_wrap == FlexWrap::WrapReverse;

    const auto main_before_is_auto = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? IsAutoCSSLength(child.style->margin_lengths.left)
            : IsAutoCSSLength(child.style->margin_lengths.top);
    };

    const auto main_after_is_auto = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? IsAutoCSSLength(child.style->margin_lengths.right)
            : IsAutoCSSLength(child.style->margin_lengths.bottom);
    };

    const auto main_before_margin = [&](const LayoutChild& child) {
        if(main_before_is_auto(child)) return 0.0f;
        return direction == FlexDirection::Row
            ? child.style->margin.left
            : child.style->margin.top;
    };

    const auto main_after_margin = [&](const LayoutChild& child) {
        if(main_after_is_auto(child)) return 0.0f;
        return direction == FlexDirection::Row
            ? child.style->margin.right
            : child.style->margin.bottom;
    };

    const auto cross_before_is_auto = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? IsAutoCSSLength(child.style->margin_lengths.top)
            : IsAutoCSSLength(child.style->margin_lengths.left);
    };

    const auto cross_after_is_auto = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? IsAutoCSSLength(child.style->margin_lengths.bottom)
            : IsAutoCSSLength(child.style->margin_lengths.right);
    };

    const auto cross_before_margin = [&](const LayoutChild& child) {
        if(cross_before_is_auto(child)) return 0.0f;
        return direction == FlexDirection::Row
            ? child.style->margin.top
            : child.style->margin.left;
    };

    const auto cross_after_margin = [&](const LayoutChild& child) {
        if(cross_after_is_auto(child)) return 0.0f;
        return direction == FlexDirection::Row
            ? child.style->margin.bottom
            : child.style->margin.right;
    };

    const auto child_main_size = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? child.transform->width
            : child.transform->height;
    };

    const auto child_cross_size = [&](const LayoutChild& child) {
        return direction == FlexDirection::Row
            ? child.transform->height
            : child.transform->width;
    };

    // Resolve basis once before line breaking. No per-line temporary child
    // copies are needed; FlexLine stores only index ranges and scalar totals.
    for(auto& child : flow_children) {
        if(child.style->flex_basis_length.has_value()) {
            child.style->flex_basis = ResolveCSSLength(
                *child.style->flex_basis_length,
                available_main,
                child.style->font_size,
                CSS_UI_ROOT_FONT_SIZE,
                child.style->custom_properties.get()
            );
        }

        if(child.style->flex_basis >= 0.0f) {
            if(direction == FlexDirection::Row) {
                child.transform->width = child.style->flex_basis;
            }
            else {
                child.transform->height = child.style->flex_basis;
            }
        }
    }

    struct FlexLine {
        std::size_t begin = 0;
        std::size_t end = 0;
        float base_main = 0.0f;
        float cross_size = 0.0f;
        float cross_offset = 0.0f;
        float total_grow = 0.0f;
        float total_shrink = 0.0f;
        std::size_t auto_main_margins = 0;
    };

    // In a column, auto-height children keep their content height instead of
    // being shrunk below it (CSS min-height:auto), unless runtime-constrain is
    // explicitly enabled.
    const auto shrink_weight = [&](const LayoutChild& child) {
        if(
            direction != FlexDirection::Row &&
            ShouldAutoHeight(*child.style) &&
            !child.style->runtime_constrain.value_or(false)
        ) return 0.0f;
        return child.style->flex_shrink;
    };

    const auto append_to_line = [&](FlexLine& target, std::size_t index) {
        LayoutChild& child = flow_children[index];
        const float outer_main =
            child_main_size(child) +
            main_before_margin(child) +
            main_after_margin(child);
        const float outer_cross =
            child_cross_size(child) +
            cross_before_margin(child) +
            cross_after_margin(child);

        if(target.end > target.begin) target.base_main += main_gap;
        target.base_main += outer_main;
        target.cross_size = std::max(target.cross_size,outer_cross);
        target.total_grow += child.style->flex_grow;
        target.total_shrink +=
            shrink_weight(child) *
            std::max(1.0f,child_main_size(child));
        target.auto_main_margins +=
            static_cast<std::size_t>(main_before_is_auto(child)) +
            static_cast<std::size_t>(main_after_is_auto(child));
        target.end = index+1;
    };

    const auto layout_line = [&](FlexLine& current) {
        float free_space = available_main-current.base_main;

        if(free_space > 0.0f && current.total_grow > 0.0f) {
            for(std::size_t index=current.begin; index<current.end; ++index) {
                LayoutChild& child = flow_children[index];
                const float addition =
                    free_space * child.style->flex_grow/current.total_grow;
                if(direction == FlexDirection::Row) {
                    child.transform->width += addition;
                }
                else {
                    child.transform->height += addition;
                }
            }
            free_space = 0.0f;
        }
        else if(free_space < 0.0f && current.total_shrink > 0.0f) {
            for(std::size_t index=current.begin; index<current.end; ++index) {
                LayoutChild& child = flow_children[index];
                const float size = child_main_size(child);
                const float reduction =
                    (-free_space) *
                    (shrink_weight(child)*std::max(1.0f,size)) /
                    current.total_shrink;
                if(direction == FlexDirection::Row) {
                    child.transform->width =
                        std::max(0.0f,child.transform->width-reduction);
                }
                else {
                    child.transform->height =
                        std::max(0.0f,child.transform->height-reduction);
                }
            }
            free_space = 0.0f;
        }

        float auto_main_margin = 0.0f;
        if(free_space > 0.0f && current.auto_main_margins > 0) {
            auto_main_margin =
                free_space/static_cast<float>(current.auto_main_margins);
            free_space = 0.0f;
        }

        const float distributable = std::max(0.0f,free_space);
        float start_offset = 0.0f;
        float spacing = main_gap;
        const std::size_t count = current.end-current.begin;

        switch(ui.style.justify_content) {
            case JustifyContent::Center:
                start_offset = distributable*0.5f;
                break;
            case JustifyContent::FlexEnd:
                start_offset = distributable;
                break;
            case JustifyContent::SpaceBetween:
                if(count>1) {
                    spacing += distributable/static_cast<float>(count-1);
                }
                break;
            case JustifyContent::SpaceAround:
                if(count>0) {
                    const float extra = distributable/static_cast<float>(count);
                    spacing += extra;
                    start_offset = extra*0.5f;
                }
                break;
            case JustifyContent::SpaceEvenly:
                if(count>0) {
                    const float extra = distributable/static_cast<float>(count+1);
                    spacing += extra;
                    start_offset = extra;
                }
                break;
            case JustifyContent::FlexStart:
            default:
                break;
        }

        const float main_origin =
            direction == FlexDirection::Row ? inner.x : inner.y;
        float cursor = main_origin + start_offset;

        float physical_cross_offset = current.cross_offset;
        if(wrap_reverse) {
            physical_cross_offset =
                available_cross-current.cross_offset-current.cross_size;
        }
        const float line_cross_origin =
            (direction == FlexDirection::Row ? inner.y : inner.x) +
            physical_cross_offset;

        for(std::size_t index=current.begin; index<current.end; ++index) {
            LayoutChild& child = flow_children[index];

            AlignItems effective = ui.style.align_items;
            if(child.style->align_self != AlignSelf::Auto) {
                effective = static_cast<AlignItems>(
                    static_cast<int>(child.style->align_self)-1
                );
            }

            const bool cross_auto_before = cross_before_is_auto(child);
            const bool cross_auto_after = cross_after_is_auto(child);
            const bool cross_auto_margin = cross_auto_before || cross_auto_after;
            const float fixed_cross_before = cross_before_margin(child);
            const float fixed_cross_after = cross_after_margin(child);
            const float cross_available = std::max(
                0.0f,
                current.cross_size-fixed_cross_before-fixed_cross_after
            );

            // wrap-reverse reverses flex cross-start/cross-end. Physical
            // margin-top/right/bottom/left remain physical margins.
            if(wrap_reverse && !cross_auto_margin) {
                if(effective == AlignItems::FlexStart) {
                    effective = AlignItems::FlexEnd;
                }
                else if(effective == AlignItems::FlexEnd) {
                    effective = AlignItems::FlexStart;
                }
            }

            const bool auto_main_before = main_before_is_auto(child);
            const bool auto_main_after = main_after_is_auto(child);
            const float used_main_before = auto_main_before
                ? auto_main_margin
                : main_before_margin(child);
            const float used_main_after = auto_main_after
                ? auto_main_margin
                : main_after_margin(child);

            cursor += used_main_before;

            if(direction == FlexDirection::Row) {
                child.transform->x = cursor;

                // cross_available ultimately comes from this container's own box,
                // which for an auto-height row is itself still unresolved at this
                // point (its real height is what we're about to measure FROM these
                // children). Stretching children into it first creates a circular
                // auto-height-stretch feedback where the row inflates to whatever
                // space its parent happened to offer, instead of sizing to the
                // children's own natural height like a normal browser would.
                if(
                    effective == AlignItems::Stretch && !cross_auto_margin &&
                    !ShouldAutoHeight(ui.style)
                ) {
                    child.transform->height = cross_available;
                }

                if(cross_auto_margin) {
                    const float cross_free = std::max(
                        0.0f,
                        cross_available-child.transform->height
                    );
                    const int auto_count =
                        static_cast<int>(cross_auto_before) +
                        static_cast<int>(cross_auto_after);
                    const float auto_share = auto_count>0
                        ? cross_free/static_cast<float>(auto_count)
                        : 0.0f;
                    child.transform->y =
                        line_cross_origin + fixed_cross_before +
                        (cross_auto_before ? auto_share : 0.0f);
                }
                else if(effective == AlignItems::Center) {
                    child.transform->y =
                        line_cross_origin + fixed_cross_before +
                        (cross_available-child.transform->height)*0.5f;
                }
                else if(effective == AlignItems::FlexEnd) {
                    child.transform->y =
                        line_cross_origin + current.cross_size -
                        fixed_cross_after-child.transform->height;
                }
                else {
                    child.transform->y =
                        line_cross_origin + fixed_cross_before;
                }

                cursor +=
                    child.transform->width + used_main_after + spacing;
            }
            else {
                child.transform->y = cursor;

                // Same reasoning as the Row branch above, mirrored onto the other
                // axis: don't stretch a column's children into its own not-yet-
                // resolved auto width.
                if(
                    effective == AlignItems::Stretch && !cross_auto_margin &&
                    !ShouldAutoWidth(ui.style)
                ) {
                    child.transform->width = cross_available;
                }

                if(cross_auto_margin) {
                    const float cross_free = std::max(
                        0.0f,
                        cross_available-child.transform->width
                    );
                    const int auto_count =
                        static_cast<int>(cross_auto_before) +
                        static_cast<int>(cross_auto_after);
                    const float auto_share = auto_count>0
                        ? cross_free/static_cast<float>(auto_count)
                        : 0.0f;
                    child.transform->x =
                        line_cross_origin + fixed_cross_before +
                        (cross_auto_before ? auto_share : 0.0f);
                }
                else if(effective == AlignItems::Center) {
                    child.transform->x =
                        line_cross_origin + fixed_cross_before +
                        (cross_available-child.transform->width)*0.5f;
                }
                else if(effective == AlignItems::FlexEnd) {
                    child.transform->x =
                        line_cross_origin + current.cross_size -
                        fixed_cross_after-child.transform->width;
                }
                else {
                    child.transform->x =
                        line_cross_origin + fixed_cross_before;
                }

                cursor +=
                    child.transform->height + used_main_after + spacing;
            }

            apply_relative_position(child);
            ApplyRuntimeSizeToElement(*child.rect,*child.style,inner);
            ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
            if (child.style->transform_operations) {
                EvaluateTransformMatrix(*child.style, *child.transform, inner.width, inner.height, child.style->font_size,CSS_UI_ROOT_FONT_SIZE, CSS_UI_VIEWPORT_WIDTH, CSS_UI_VIEWPORT_HEIGHT);
            }
            apply_scroll(child);

            if(child.container != nullptr) {
                LayoutContainerPass(
                    *child.container,
                    style_sheet,
                    // See the comment on the first recursive LayoutContainerPass
                    // call in this file: always recurse with the size this pass
                    // already resolved for this child, not the parent's raw inner box.
                    child.transform->width,
                    child.transform->height,
                    true
                );
            }
            if(child.layout_nested) child.layout_nested();
        }
    };

    if(flow_children.empty()) {
        layout_absolute_children();
        commit_layout_state();
        return;
    }

    if(!wrapping) {
        // Preserve the common no-wrap path without allocating a line vector.
        // This keeps existing flex/container layout as cheap as before 0.7.1.
        FlexLine single_line;
        single_line.begin = 0;
        for(std::size_t index=0; index<flow_children.size(); ++index) {
            append_to_line(single_line,index);
        }
        // An auto-sized cross axis (auto-height row / auto-width column) has no
        // definite size to align inside yet: `available_cross` is merely the
        // space the parent offered. Align within the line's own content size
        // (already accumulated by append_to_line) instead, or align-self /
        // align-items: center|flex-end push children to the bottom of that
        // offered space and inflate the container to match.
        if(!cross_axis_is_auto) single_line.cross_size = available_cross;
        layout_line(single_line);
    }
    else {
        std::vector<FlexLine> lines;
        // Worst case is one item per line. One reserve avoids growth/copies on
        // wrapped toolbars and palettes with many children.
        lines.reserve(flow_children.size());

        FlexLine line;
        line.begin = 0;
        for(std::size_t index=0; index<flow_children.size(); ++index) {
            LayoutChild& child = flow_children[index];
            const float outer_main =
                child_main_size(child) +
                main_before_margin(child) +
                main_after_margin(child);
            const bool has_items = line.end>line.begin;
            const float candidate_main =
                line.base_main +
                (has_items ? main_gap : 0.0f) +
                outer_main;

            if(
                has_items &&
                candidate_main > available_main + 0.01f
            ) {
                lines.push_back(line);
                line = FlexLine{};
                line.begin = index;
            }
            append_to_line(line,index);
        }
        if(line.end>line.begin) lines.push_back(line);

        if(lines.size()==1) {
            if(!cross_axis_is_auto) lines[0].cross_size = available_cross;
        }
        else {
            float used_cross =
                cross_gap*static_cast<float>(lines.size()-1);
            for(const FlexLine& current : lines) used_cross += current.cross_size;

            float free_cross = std::max(0.0f,available_cross-used_cross);
            const AlignContent alignment =
                ui.style.align_content.value_or(AlignContent::Stretch);
            float cross_start = 0.0f;
            float line_spacing = cross_gap;

            switch(alignment) {
                case AlignContent::Stretch:
                    if(free_cross>0.0f) {
                        const float addition =
                            free_cross/static_cast<float>(lines.size());
                        for(FlexLine& current : lines) current.cross_size += addition;
                    }
                    break;
                case AlignContent::Center:
                    cross_start = free_cross*0.5f;
                    break;
                case AlignContent::FlexEnd:
                    cross_start = free_cross;
                    break;
                case AlignContent::SpaceBetween:
                    if(lines.size()>1) {
                        line_spacing +=
                            free_cross/static_cast<float>(lines.size()-1);
                    }
                    break;
                case AlignContent::SpaceAround: {
                    const float extra =
                        free_cross/static_cast<float>(lines.size());
                    line_spacing += extra;
                    cross_start = extra*0.5f;
                    break;
                }
                case AlignContent::SpaceEvenly: {
                    const float extra =
                        free_cross/static_cast<float>(lines.size()+1);
                    line_spacing += extra;
                    cross_start = extra;
                    break;
                }
                case AlignContent::FlexStart:
                default:
                    break;
            }

            float cross_cursor = cross_start;
            for(FlexLine& current : lines) {
                current.cross_offset = cross_cursor;
                cross_cursor += current.cross_size+line_spacing;
            }
        }

        for(FlexLine& current : lines) layout_line(current);
    }

    layout_absolute_children();
    commit_layout_state();
}

void LayoutContainer(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    float containing_width,
    float containing_height,
    bool force_layout
) {
    if(CSS_UI_LAYOUT_DEPTH == 0) {
        const std::size_t previous_root_pseudo_hash =
            ui.rect.last_layout_pseudo_hash;

        UILayoutFrameActivity activity;
        if(
            style_sheet.uses_layout_pseudo_states ||
            style_sheet.uses_layout_animations
        ) {
            activity = RefreshUILayoutFrameActivity(
                ui,
                style_sheet.uses_layout_pseudo_states,
                style_sheet.uses_layout_animations
            );
        }

        const bool root_pseudo_changed =
            previous_root_pseudo_hash != ui.rect.last_layout_pseudo_hash;

        force_layout = force_layout ||
            activity.pseudo_changed || activity.animation_active;
        if(activity.pseudo_changed) MarkLayoutDirty(ui,true);

        const float resolved_width = containing_width < 0.0f
            ? static_cast<float>(GetScreenWidth())
            : containing_width;
        const float resolved_height = containing_height < 0.0f
            ? static_cast<float>(GetScreenHeight())
            : containing_height;

        const bool root_style_dirty =
            force_layout ||
            ui.layout_dirty ||
            root_pseudo_changed ||
            ui.last_layout_revision != style_sheet.revision ||
            ui.last_containing_width != resolved_width ||
            ui.last_containing_height != resolved_height;

        if(root_style_dirty) {
            ui.style.selector = SelectorType::Body;
            ui.style.selector_ancestors.clear();
            ui.style.selector_ancestor_previous_siblings.clear();
            ui.style.selector_previous_siblings.reset();
            ui.style = ResolveStyleWithPseudoStates(
                style_sheet,SelectorType::Body,ui.style.id,
                ui.rect.pseudo_states.ActiveNamesView(),ui.style
            );
            ResolveComputedLengths(
                ui.style,
                resolved_width,
                resolved_height,
                CSS_UI_ROOT_FONT_SIZE,ui.inherited_scale,true
            );
            ui.style = ResolveLayoutAnimatedStyle(
                ui.rect,style_sheet,std::move(ui.style)
            );
            CSS_UI_ROOT_FONT_SIZE = ui.style.font_size;
            ++ui.rect.style_generation;
        }
    }

    if(CSS_UI_LAYOUT_DEPTH > 0) {
        LayoutContainerPass(
            ui,
            style_sheet,
            containing_width,
            containing_height,
            force_layout
        );
        return;
    }

    CSS_UI_LAYOUT_DEPTH++;

    constexpr int maximum_auto_size_passes = 4;

    for(int pass = 0; pass < maximum_auto_size_passes; pass++) {
        CSS_UI_AUTO_SIZE_CHANGED = false;

        LayoutContainerPass(
            ui,
            style_sheet,
            containing_width,
            containing_height,
            force_layout || pass > 0
        );

        if(!CSS_UI_AUTO_SIZE_CHANGED) {
            break;
        }
    }

    CSS_UI_LAYOUT_DEPTH--;
}