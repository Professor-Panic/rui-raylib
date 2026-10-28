#include "css_emulator.h"

std::string UIElementStableKey(const UIElement& element) {
    return std::visit([](const auto& widget) -> std::string {
        using T = std::decay_t<decltype(widget)>;
        const UIElementBase* rect = nullptr;
        const ComputedStyle* style = nullptr;
        if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
            if(!widget) return {};
            rect = &widget->rect;
            style = &widget->style;
        }
        else {
            rect = &widget.rect;
            style = &widget.style;
        }
        std::string key = CSSRuntimeId(style->id);
        if(key.empty()) key = rect->key;
        return key;
    },element);
}

void PreserveUIElementBase(
    UIElementBase& destination,
    const UIElementBase& source,
    const UIReconcileOptions& options
) {
    if(options.preserve_runtime_layout) {
        destination.runtime_offset = source.runtime_offset;
        destination.runtime_size = source.runtime_size;
        destination.runtime_size_override = source.runtime_size_override;
    }
    destination.tooltip_hover_started = source.tooltip_hover_started;
    destination.animation_state = source.animation_state;
    destination.layout_animation_state = source.layout_animation_state;
    destination.last_layout_pseudo_hash = source.last_layout_pseudo_hash;
    for(const std::string& state : source.pseudo_states.ActiveNamesView()) {
        destination.pseudo_states.Set(state,true);
    }
}

void PreserveUIElementState(
    UIElement& destination,
    UIElement& source,
    const UIReconcileOptions& options
) {
    std::visit([&](auto& next, auto& previous) {
        using Next = std::decay_t<decltype(next)>;
        using Previous = std::decay_t<decltype(previous)>;
        if constexpr(!std::is_same_v<Next,Previous>) {
            return;
        }
        else if constexpr(IsUICustomElementPointerV<Next>) {
            if(!next || !previous) return;
            PreserveUIElementBase(next->rect,previous->rect,options);
            if(next->TypeName() == previous->TypeName()) {
                next->ReconcileFrom(*previous,options);
            }
        }
        else if constexpr(IsUICompositePointerV<Next>) {
            if(!next || !previous) return;
            PreserveUIElementBase(next->rect,previous->rect,options);
            if constexpr(std::is_same_v<Next,std::shared_ptr<UIContainer>>) {
                ReconcileUI(*previous,std::move(*next),options);
                *next = std::move(*previous);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UICollapsible>>) {
                if(options.preserve_open_state) next->open = previous->open;
                ReconcileUI(previous->content,std::move(next->content),options);
                next->content = std::move(previous->content);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UITreeNode>>) {
                if(options.preserve_open_state) next->open = previous->open;
                if(options.preserve_selection) next->selected = previous->selected;
                ReconcileUI(previous->content,std::move(next->content),options);
                next->content = std::move(previous->content);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UIMenu>>) {
                if(options.preserve_open_state) next->open = previous->open;
                ReconcileUI(previous->content,std::move(next->content),options);
                next->content = std::move(previous->content);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UIPopup>>) {
                if(options.preserve_open_state) next->open = previous->open;
                ReconcileUI(previous->content,std::move(next->content),options);
                next->content = std::move(previous->content);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UIWindow>>) {
                if(options.preserve_open_state) {
                    next->open = previous->open;
                    next->collapsed = previous->collapsed;
                }
                next->dragging = previous->dragging;
                next->resizing = previous->resizing;
                next->drag_anchor = previous->drag_anchor;
                next->interaction_start = previous->interaction_start;
                ReconcileUI(previous->content,std::move(next->content),options);
                next->content = std::move(previous->content);
            }
            else if constexpr(std::is_same_v<Next,std::shared_ptr<UITabBar>>) {
                if(options.preserve_selection) next->selected_index = previous->selected_index;
                for(UITab& next_tab : next->tabs) {
                    auto found = std::find_if(
                        previous->tabs.begin(),previous->tabs.end(),
                        [&](const UITab& old_tab) {
                            if(!next_tab.id.empty()) return next_tab.id == old_tab.id;
                            return next_tab.title == old_tab.title;
                        }
                    );
                    if(found != previous->tabs.end()) {
                        ReconcileUI(found->content,std::move(next_tab.content),options);
                        next_tab.content = std::move(found->content);
                    }
                }
            }
        }
        else {
            PreserveUIElementBase(next.rect,previous.rect,options);
            if constexpr(std::is_same_v<Next,UIButton>) {
                next.focused = previous.focused;
                next.pressed_inside = previous.pressed_inside;
            }
            else if constexpr(IsUIInputV<Next>) {
                next.focused = previous.focused;
                next.active = previous.active;
                next.dragging = previous.dragging;
                next.drag_start_value = previous.drag_start_value;
                next.drag_accumulator = previous.drag_accumulator;
                next.pressed_inside = previous.pressed_inside;
                next.edit_buffer = previous.edit_buffer;
                next.component_buffers = previous.component_buffers;
                next.active_component = previous.active_component;
                next.caret_frame_counter = previous.caret_frame_counter;
                next.drag_text_editing = previous.drag_text_editing;
                next.drag_text_select_all = previous.drag_text_select_all;
                next.text_caret = previous.text_caret;
                next.text_anchor = previous.text_anchor;
                next.text_selection_start = previous.text_selection_start;
                next.text_selection_end = previous.text_selection_end;
                next.text_mouse_selecting = previous.text_mouse_selecting;
                next.last_drag_click_time = previous.last_drag_click_time;
                next.last_drag_click_position = previous.last_drag_click_position;
                next.last_drag_click_component = previous.last_drag_click_component;
                next.picker_open = previous.picker_open;
                next.picker_initialized = previous.picker_initialized;
                next.picker_hue = previous.picker_hue;
                next.picker_saturation = previous.picker_saturation;
                next.picker_brightness = previous.picker_brightness;
                next.picker_drag_region = previous.picker_drag_region;
                if(options.preserve_values) next.value = previous.value;
            }
            else if constexpr(std::is_same_v<Next,UISelectable>) {
                next.focused = previous.focused;
                next.active = previous.active;
                next.pressed_inside = previous.pressed_inside;
                if(options.preserve_selection) next.selected = previous.selected;
            }
            else if constexpr(std::is_same_v<Next,UIComboBox>) {
                next.focused = previous.focused;
                next.active = previous.active;
                next.pressed_inside = previous.pressed_inside;
                next.open = previous.open;
                next.scroll_offset = previous.scroll_offset;
                if(options.preserve_values) next.selected_index = previous.selected_index;
            }
            else if constexpr(IsUIRangeInputV<Next>) {
                next.focused = previous.focused;
                next.active_handle = previous.active_handle;
                if(options.preserve_values) {
                    next.lower = previous.lower;
                    next.upper = previous.upper;
                }
            }
            else if constexpr(std::is_same_v<Next,UICanvas>) {
                next.focused = previous.focused;
                next.dragging = previous.dragging;
                next.drag_delta = previous.drag_delta;
                next.mouse_pressed_inside = previous.mouse_pressed_inside;
                next.last_click_time = previous.last_click_time;
                next.last_click_position = previous.last_click_position;
            }
            else if constexpr(std::is_same_v<Next,UIImage>) {
                next.focused = previous.focused;
                next.sampling_texture_id = previous.sampling_texture_id;
                next.applied_sampling = previous.applied_sampling;
            }
            else if constexpr(std::is_same_v<Next,UISplitter>) {
                next.dragging = previous.dragging;
                next.drag_origin = previous.drag_origin;
                next.before_start_size = previous.before_start_size;
                next.after_start_size = previous.after_start_size;
                next.delta = previous.delta;
            }
        }
    },destination,source);
}

void ReconcileUI(
    UIContainer& destination,
    UIContainer rebuilt,
    const UIReconcileOptions& options
) {
    PreserveUIElementBase(rebuilt.rect,destination.rect,options);
    rebuilt.scroll = destination.scroll;
    rebuilt.dragging_horizontal_scrollbar =
        destination.dragging_horizontal_scrollbar;
    rebuilt.dragging_vertical_scrollbar =
        destination.dragging_vertical_scrollbar;
    rebuilt.scrollbar_drag_anchor = destination.scrollbar_drag_anchor;
    rebuilt.scrollbar_scroll_start = destination.scrollbar_scroll_start;

    std::vector<bool> consumed(destination.elements.size(),false);
    for(std::size_t next_index=0; next_index<rebuilt.elements.size(); ++next_index) {
        UIElement& next = rebuilt.elements[next_index];
        const std::string key = UIElementStableKey(next);
        std::size_t match = destination.elements.size();

        if(!key.empty()) {
            for(std::size_t old_index=0; old_index<destination.elements.size(); ++old_index) {
                if(!consumed[old_index] && UIElementStableKey(destination.elements[old_index]) == key) {
                    match = old_index;
                    break;
                }
            }
        }
        else if(
            options.match_unkeyed_by_index &&
            next_index < destination.elements.size() &&
            !consumed[next_index] &&
            next.index() == destination.elements[next_index].index()
        ) {
            match = next_index;
        }

        if(match < destination.elements.size()) {
            consumed[match] = true;
            PreserveUIElementState(next,destination.elements[match],options);
        }
    }

    destination = std::move(rebuilt);
    MarkLayoutDirty(destination,true);
}

UIElement* FindUIElement(UIContainer& container, const std::string& key) {
    const std::string css_key = !key.empty() && key.front() == '#' ? key : "#"+key;
    for(UIElement& element : container.elements) {
        const std::string candidate = UIElementStableKey(element);
        if(candidate == key || candidate == css_key) return &element;
        UIElement* nested = std::visit([&](auto& widget) -> UIElement* {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return nullptr;
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(UIElement* found = FindUIElement(panel.content,key)) return found;
                    }
                }
                else if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    return FindUIElement(viewport->content,key);
                }
                else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    return FindUIElement(item->content,key);
                }
                else if(auto* list = dynamic_cast<UIVirtualList*>(widget.get())) {
                    return list->UsesTemplate()
                        ? FindUIElement(list->content,key)
                        : nullptr;
                }
                if(UIContainer* custom_children = widget->ChildContainer()) {
                    return FindUIElement(*custom_children,key);
                }
                return nullptr;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget ? FindUIElement(*widget,key) : nullptr;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return nullptr;
                for(UITab& tab : widget->tabs) {
                    if(UIElement* found = FindUIElement(tab.content,key)) return found;
                }
                return nullptr;
            }
            else if constexpr(IsUICompositePointerV<T>) {
                return widget ? FindUIElement(widget->content,key) : nullptr;
            }
            else {
                return nullptr;
            }
        },element);
        if(nested != nullptr) return nested;
    }
    return nullptr;
}
