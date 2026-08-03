#include "css_emulator.h"

bool RuntimeStyleFlag(
    const std::optional<bool>& value,
    bool fallback
) {
    return value.has_value() ? *value : fallback;
}

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
        rect.transform.height = std::min(rect.transform.height,parent_bounds.height);
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
        rect.transform.y = std::clamp(rect.transform.y,parent_bounds.y,maximum_y);
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
        style_sheet,selector,style.id,rect.pseudo_states.ActiveNames(),style
    );
    ResolveComputedLengths(
        style,parent_width,parent_height,parent_font_size,inherited_scale
    );
    style = ResolveLayoutAnimatedStyle(rect,style_sheet,std::move(style));
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

        if(RuntimeStyleFlag(style.runtime_constrain,true)) {
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

    const std::size_t current_signature =
        force_layout
            ? 0
            : ComputeContainerLayoutSignature(ui);

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
        current_signature == ui.last_layout_signature
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
        ui.last_layout_signature =
            ComputeContainerLayoutSignature(ui);
    };

    ResolveComputedLengths(
        ui.style,
        containing_width,
        containing_height,
        20.0f,
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

    for(auto& element : ui.elements) {
        LayoutChild child;

        std::visit(
            [&](auto& widget) {
                using T = std::decay_t<decltype(widget)>;

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
                        widget.rect.transform.height =
                            widget.style.height;
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

                        if(ShouldAutoWidth(widget.style)) {
                            widget.rect.transform.width =
                                std::abs(source.width) +
                                widget.style.padding.left +
                                widget.style.padding.right;
                        }

                        if(ShouldAutoHeight(widget.style)) {
                            widget.rect.transform.height =
                                std::abs(source.height) +
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
            child.transform->x =
                inner.x + child.style->left;

            child.transform->y =
                inner.y + child.style->top;

            if(
                child.style->right != 0.0f &&
                child.style->left == 0.0f
            ) {
                child.transform->x =
                    inner.x +
                    inner.width -
                    child.style->right -
                    child.transform->width;
            }

            if(
                child.style->bottom != 0.0f &&
                child.style->top == 0.0f
            ) {
                child.transform->y =
                    inner.y +
                    inner.height -
                    child.style->bottom -
                    child.transform->height;
            }

            ApplyRuntimeSizeToElement(*child.rect,*child.style,inner);
            ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
            apply_scroll(child);

            if(child.container != nullptr) {
                LayoutContainerPass(
                    *child.container,
                    style_sheet,
                    ShouldAutoWidth(*child.style)
                        ? inner.width
                        : child.transform->width,
                    ShouldAutoHeight(*child.style)
                        ? inner.height
                        : child.transform->height,
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
        const std::vector<float> row_sizes = ResolveGridTrackSizes(
            ui.style.grid_row_tracks,
            ui.style.grid_auto_row,
            rows,
            inner.height,
            vertical_gap
        );

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
                inner.y + GridTrackOffset(row_sizes,row,vertical_gap),
                GridTrackSpan(column_sizes,column,column_span,horizontal_gap),
                GridTrackSpan(row_sizes,row,row_span,vertical_gap)
            };

            float content_width = std::max(
                0.0f,
                cell.width -
                    child.style->margin.left -
                    child.style->margin.right
            );

            float content_height = std::max(
                0.0f,
                cell.height -
                    child.style->margin.top -
                    child.style->margin.bottom
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

            const AlignItems horizontal_alignment = ui.style.justify_items;
            child.transform->x = cell.x + child.style->margin.left;
            child.transform->y = cell.y + child.style->margin.top;

            if(horizontal_alignment == AlignItems::Stretch) {
                child.transform->width = content_width;
            }
            else if(horizontal_alignment == AlignItems::Center) {
                child.transform->x += (content_width-child.transform->width)*0.5f;
            }
            else if(horizontal_alignment == AlignItems::FlexEnd) {
                child.transform->x = cell.x+cell.width-child.style->margin.right-child.transform->width;
            }

            if(vertical_alignment == AlignItems::Stretch) {
                child.transform->height = content_height;
            }
            else if(vertical_alignment == AlignItems::Center) {
                child.transform->y += (content_height-child.transform->height)*0.5f;
            }
            else if(vertical_alignment == AlignItems::FlexEnd) {
                child.transform->y = cell.y+cell.height-child.style->margin.bottom-child.transform->height;
            }

            ApplyRuntimeSizeToElement(*child.rect,*child.style,cell);

            apply_relative_position(child);
            ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
            apply_scroll(child);

            if(child.container != nullptr) {
                LayoutContainerPass(
                    *child.container,
                    style_sheet,
                    ShouldAutoWidth(*child.style)
                        ? inner.width
                        : child.transform->width,
                    ShouldAutoHeight(*child.style)
                        ? inner.height
                        : child.transform->height,
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

    float available_main =
        direction == FlexDirection::Row
            ? inner.width
            : inner.height;

    if(ui.style.gap_length.has_value()) {
        ui.style.gap = ResolveCSSLength(
            *ui.style.gap_length,
            available_main
        );
    }

    float total_main = 0.0f;
    float total_grow = 0.0f;
    float total_shrink = 0.0f;

    for(auto& child : flow_children) {
        if(child.style->flex_basis_length.has_value()) {
            child.style->flex_basis = ResolveCSSLength(
                *child.style->flex_basis_length,
                available_main
            );
        }

        if(child.style->flex_basis >= 0.0f) {
            if(direction == FlexDirection::Row) {
                child.transform->width =
                    child.style->flex_basis;
            }
            else {
                child.transform->height =
                    child.style->flex_basis;
            }
        }

        float size =
            direction == FlexDirection::Row
                ? child.transform->width
                : child.transform->height;

        float before =
            direction == FlexDirection::Row
                ? child.style->margin.left
                : child.style->margin.top;

        float after =
            direction == FlexDirection::Row
                ? child.style->margin.right
                : child.style->margin.bottom;

        total_main += size + before + after;
        total_grow += child.style->flex_grow;

        total_shrink +=
            child.style->flex_shrink *
            std::max(1.0f,size);
    }

    if(flow_children.size() > 1) {
        total_main +=
            ui.style.gap *
            static_cast<float>(
                flow_children.size() - 1
            );
    }

    float free_space =
        available_main - total_main;

    if(
        free_space > 0.0f &&
        total_grow > 0.0f
    ) {
        for(auto& child : flow_children) {
            float addition =
                free_space *
                child.style->flex_grow /
                total_grow;

            if(direction == FlexDirection::Row) {
                child.transform->width += addition;
            }
            else {
                child.transform->height += addition;
            }
        }

        free_space = 0.0f;
    }
    else if(
        free_space < 0.0f &&
        total_shrink > 0.0f
    ) {
        for(auto& child : flow_children) {
            float size =
                direction == FlexDirection::Row
                    ? child.transform->width
                    : child.transform->height;

            float reduction =
                (-free_space) *
                (
                    child.style->flex_shrink *
                    std::max(1.0f,size)
                ) /
                total_shrink;

            if(direction == FlexDirection::Row) {
                child.transform->width = std::max(
                    0.0f,
                    child.transform->width -
                        reduction
                );
            }
            else {
                child.transform->height = std::max(
                    0.0f,
                    child.transform->height -
                        reduction
                );
            }
        }

        free_space = 0.0f;
    }

    float start_offset = 0.0f;
    float spacing = ui.style.gap;

    std::size_t count =
        flow_children.size();

    switch(ui.style.justify_content) {
        case JustifyContent::Center:
            start_offset =
                free_space * 0.5f;
            break;

        case JustifyContent::FlexEnd:
            start_offset =
                free_space;
            break;

        case JustifyContent::SpaceBetween:
            if(count > 1) {
                spacing +=
                    free_space /
                    static_cast<float>(count - 1);
            }
            break;

        case JustifyContent::SpaceAround:
            if(count > 0) {
                float extra =
                    free_space /
                    static_cast<float>(count);

                spacing += extra;
                start_offset = extra * 0.5f;
            }
            break;

        case JustifyContent::SpaceEvenly:
            if(count > 0) {
                float extra =
                    free_space /
                    static_cast<float>(count + 1);

                spacing += extra;
                start_offset = extra;
            }
            break;

        default:
            break;
    }

    float cursor =
        (
            direction == FlexDirection::Row
                ? inner.x
                : inner.y
        ) +
        std::max(0.0f,start_offset);

    for(auto& child : flow_children) {
        AlignSelf alignment =
            child.style->align_self;

        AlignItems effective =
            ui.style.align_items;

        if(alignment != AlignSelf::Auto) {
            effective = static_cast<AlignItems>(
                static_cast<int>(alignment) - 1
            );
        }

        if(direction == FlexDirection::Row) {
            cursor +=
                child.style->margin.left;

            child.transform->x =
                cursor;

            float cross_available = std::max(
                0.0f,
                inner.height -
                    child.style->margin.top -
                    child.style->margin.bottom
            );

            if(effective == AlignItems::Stretch) {
                child.transform->height =
                    cross_available;
            }

            if(effective == AlignItems::Center) {
                child.transform->y =
                    inner.y +
                    child.style->margin.top +
                    (
                        cross_available -
                        child.transform->height
                    ) * 0.5f;
            }
            else if(effective == AlignItems::FlexEnd) {
                child.transform->y =
                    inner.y +
                    inner.height -
                    child.style->margin.bottom -
                    child.transform->height;
            }
            else {
                child.transform->y =
                    inner.y +
                    child.style->margin.top;
            }

            cursor +=
                child.transform->width +
                child.style->margin.right +
                spacing;
        }
        else {
            cursor +=
                child.style->margin.top;

            child.transform->y =
                cursor;

            float cross_available = std::max(
                0.0f,
                inner.width -
                    child.style->margin.left -
                    child.style->margin.right
            );

            if(effective == AlignItems::Stretch) {
                child.transform->width =
                    cross_available;
            }

            if(effective == AlignItems::Center) {
                child.transform->x =
                    inner.x +
                    child.style->margin.left +
                    (
                        cross_available -
                        child.transform->width
                    ) * 0.5f;
            }
            else if(effective == AlignItems::FlexEnd) {
                child.transform->x =
                    inner.x +
                    inner.width -
                    child.style->margin.right -
                    child.transform->width;
            }
            else {
                child.transform->x =
                    inner.x +
                    child.style->margin.left;
            }

            cursor +=
                child.transform->height +
                child.style->margin.bottom +
                spacing;
        }

        apply_relative_position(child);
        ApplyRuntimeSizeToElement(*child.rect,*child.style,inner);
        ApplyRuntimePositionToElement(*child.rect,*child.style,inner);
        apply_scroll(child);

        if(child.container != nullptr) {
            LayoutContainerPass(
                *child.container,
                style_sheet,
                ShouldAutoWidth(*child.style)
                    ? inner.width
                    : child.transform->width,
                ShouldAutoHeight(*child.style)
                    ? inner.height
                    : child.transform->height,
                true
            );
        }
        if(child.layout_nested) child.layout_nested();
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
        const bool pseudo_changed = RefreshUIPseudoStatesForLayout(ui);
        const bool layout_animation_active = HasActiveUILayoutAnimations(ui);
        force_layout = force_layout || pseudo_changed || layout_animation_active;
        if(pseudo_changed) MarkLayoutDirty(ui,true);

        ui.style = ResolveStyleWithPseudoStates(
            style_sheet,SelectorType::Body,ui.style.id,
            ui.rect.pseudo_states.ActiveNames(),ui.style
        );
        ResolveComputedLengths(
            ui.style,
            containing_width < 0.0f ? static_cast<float>(GetScreenWidth()) : containing_width,
            containing_height < 0.0f ? static_cast<float>(GetScreenHeight()) : containing_height,
            20.0f,ui.inherited_scale,true
        );
        ui.style = ResolveLayoutAnimatedStyle(ui.rect,style_sheet,std::move(ui.style));
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

