#include "css_emulator.h"

void CollectFocusableElements(
    UIContainer& container,
    std::vector<UIFocusEntry>& focus,
    bool inherited_disabled ) {
    if(container.style.display == DisplayType::None) return;
    const bool group_disabled = inherited_disabled || container.disabled;

    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(std::is_same_v<T,UIButton>) {
                if(!group_disabled && !widget.disabled && widget.style.display != DisplayType::None) {
                    focus.push_back({&widget.focused,&widget.rect});
                }
            }
            else if constexpr(IsUIInputV<T>) {
                if(!group_disabled && !widget.disabled && widget.style.display != DisplayType::None) {
                    focus.push_back({&widget.focused,&widget.rect});
                }
            }
            else if constexpr(std::is_same_v<T,UISelectable>) {
                if(!group_disabled && !widget.disabled) focus.push_back({&widget.focused,&widget.rect});
            }
            else if constexpr(std::is_same_v<T,UIComboBox>) {
                if(!group_disabled && !widget.disabled) focus.push_back({&widget.focused,&widget.rect});
            }
            else if constexpr(IsUIRangeInputV<T>) {
                if(!group_disabled && !widget.disabled) focus.push_back({&widget.focused,&widget.rect});
            }
            else if constexpr(std::is_same_v<T,UICanvas>) {
                if(!group_disabled && !widget.disabled && widget.focusable) focus.push_back({&widget.focused,&widget.rect});
            }
            else if constexpr(std::is_same_v<T,UIImage>) {
                if(!group_disabled && !widget.disabled && widget.interactive) {
                    focus.push_back({&widget.focused,&widget.rect});
                }
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget || group_disabled || widget->style.display == DisplayType::None) return;
                if(widget->Focusable() && widget->FocusFlag()) {
                    focus.push_back({widget->FocusFlag(),&widget->rect});
                }
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(panel.open) CollectFocusableElements(panel.content,focus,group_disabled);
                    }
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                if(widget) CollectFocusableElements(*widget,focus,group_disabled);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                if(widget && widget->open) CollectFocusableElements(widget->content,focus,group_disabled || widget->disabled);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                if(widget && widget->open) CollectFocusableElements(widget->content,focus,group_disabled || widget->disabled);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                if(widget && widget->open) CollectFocusableElements(widget->content,focus,group_disabled || widget->disabled);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                if(widget && widget->open) CollectFocusableElements(widget->content,focus,group_disabled);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                if(widget && widget->open && !widget->collapsed) {
                    CollectFocusableElements(widget->content,focus,group_disabled || widget->disabled);
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(widget && !widget->disabled) {
                    if(UITab* tab = widget->SelectedTab()) {
                        CollectFocusableElements(tab->content,focus,group_disabled || tab->disabled);
                    }
                }
            }
        },element);
    }
}

void AdvanceKeyboardFocus(UIContainer& root) {
    std::vector<UIFocusEntry> focus;
    CollectFocusableElements(root,focus);
    if(focus.empty()) return;

    std::size_t current = focus.size();
    for(std::size_t index=0; index<focus.size(); ++index) {
        if(*focus[index].focused) {
            current = index;
            break;
        }
    }
    for(UIFocusEntry& entry : focus) *entry.focused = false;

    const bool reverse =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    std::size_t next = 0;
    if(current == focus.size()) {
        next = reverse ? focus.size()-1 : 0;
    }
    else if(reverse) {
        next = current == 0 ? focus.size()-1 : current-1;
    }
    else {
        next = (current+1)%focus.size();
    }
    *focus[next].focused = true;
}

void UpdatePointerFocus(UIContainer& root) {
    if(!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    std::vector<UIFocusEntry> focus;
    CollectFocusableElements(root,focus);
    UIFocusEntry* target = nullptr;
    const Vector2 mouse = GetMousePosition();
    for(UIFocusEntry& entry : focus) {
        if(entry.rect != nullptr && CheckCollisionPointRec(mouse,entry.rect->transform)) {
            target = &entry;
        }
    }
    for(UIFocusEntry& entry : focus) *entry.focused = false;
    if(target != nullptr) *target->focused = true;
}

Rectangle CompositeHeaderRectangle(
    const UIElementBase& rect,
    const ComputedStyle& style
) {
    return {
        rect.transform.x,
        rect.transform.y,
        rect.transform.width,
        std::min(rect.transform.height,std::max(1.0f,style.header_height))
    };
}

bool UpdateCompositeHeader(
    Rectangle header,
    bool disabled,
    bool& hovered,
    bool& clicked
) {
    clicked = false;
    hovered = !disabled && CheckCollisionPointRec(GetMousePosition(),header);
    if(hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        clicked = true;
        return true;
    }
    return false;
}

Rectangle TabHeaderRectangle(const UITabBar& tabs) {
    return {
        tabs.rect.transform.x,
        tabs.rect.transform.y,
        tabs.rect.transform.width,
        std::min(tabs.rect.transform.height,std::max(1.0f,tabs.style.tab_height))
    };
}

Rectangle TabItemRectangle(
    const UITabBar& tabs,
    std::size_t index
) {
    Rectangle header = TabHeaderRectangle(tabs);
    const float count = std::max(1.0f,static_cast<float>(tabs.tabs.size()));
    const float width = header.width/count;
    return {
        header.x+width*static_cast<float>(index),
        header.y,
        width,
        header.height
    };
}

void UpdateTabBar(UITabBar& tabs, bool inherited_disabled) {
    tabs.changed = false;
    tabs.hovered_index = -1;
    if(tabs.tabs.empty()) {
        tabs.selected_index = -1;
        return;
    }
    tabs.selected_index = std::clamp(
        tabs.selected_index,0,static_cast<int>(tabs.tabs.size())-1
    );
    const bool disabled = inherited_disabled || tabs.disabled;
    const Vector2 mouse = GetMousePosition();
    for(std::size_t index=0; index<tabs.tabs.size(); ++index) {
        UITab& tab = tabs.tabs[index];
        tab.close_requested = false;
        const Rectangle item = TabItemRectangle(tabs,index);
        if(disabled || tab.disabled || !CheckCollisionPointRec(mouse,item)) continue;
        tabs.hovered_index = static_cast<int>(index);
        if(!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) continue;

        const bool closable = tab.closable || tabs.style.tab_closable.value_or(false);
        const Rectangle close = {
            item.x+item.width-item.height,
            item.y,
            item.height,
            item.height
        };
        if(closable && CheckCollisionPointRec(mouse,close)) {
            tab.close_requested = true;
            if(tabs.on_close) tabs.on_close(tabs,static_cast<int>(index));
        }
        else if(tabs.selected_index != static_cast<int>(index)) {
            tabs.selected_index = static_cast<int>(index);
            tabs.changed = true;
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
        }
    }

    if(UITab* selected = tabs.SelectedTab()) {
        UpdateElements(selected->content,false,disabled || selected->disabled);
    }
}

void TranslateContainer(UIContainer& container, Vector2 delta) {
    container.rect.transform.x += delta.x;
    container.rect.transform.y += delta.y;
    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return;
                widget->rect.transform.x += delta.x;
                widget->rect.transform.y += delta.y;
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        TranslateContainer(panel.content,delta);
                        if(panel.floating) {
                            panel.floating_rectangle.x += delta.x;
                            panel.floating_rectangle.y += delta.y;
                        }
                    }
                }
            }
            else if constexpr(IsUICompositePointerV<T>) {
                if(!widget) return;
                widget->rect.transform.x += delta.x;
                widget->rect.transform.y += delta.y;
                if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    for(UITab& tab : widget->tabs) TranslateContainer(tab.content,delta);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    TranslateContainer(*widget,delta);
                    widget->rect.transform.x -= delta.x;
                    widget->rect.transform.y -= delta.y;
                }
                else {
                    TranslateContainer(widget->content,delta);
                }
            }
            else {
                widget.rect.transform.x += delta.x;
                widget.rect.transform.y += delta.y;
            }
        },element);
    }
}

UIPopup* FindOpenModal(UIContainer& container) {
    UIPopup* found = nullptr;
    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if(found != nullptr) return;
            if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                if(widget && widget->open && widget->modal) found = widget.get();
                else if(widget && widget->open) found = FindOpenModal(widget->content);
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return;
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(panel.open && (found = FindOpenModal(panel.content)) != nullptr) break;
                    }
                }
            }
            else if constexpr(IsUICompositePointerV<T>) {
                if(!widget) return;
                if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(UITab* tab = widget->SelectedTab()) {
                        found = FindOpenModal(tab->content);
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    if(widget->style.display != DisplayType::None) {
                        found = FindOpenModal(*widget);
                    }
                }
                else {
                    bool visible = true;
                    if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) visible = widget->open && !widget->collapsed;
                    if(visible) found = FindOpenModal(widget->content);
                }
            }
        },element);
        if(found != nullptr) break;
    }
    return found;
}

void UpdatePopup(UIPopup& popup, bool disabled) {
    const bool opened = popup.opened;
    popup.opened = false;
    popup.closed = false;
    if(opened && popup.on_open) popup.on_open(popup);
    if(!popup.open) return;
    if(popup.close_on_escape && IsKeyPressed(KEY_ESCAPE)) {
        ClosePopup(popup);
        if(popup.closed && popup.on_close) popup.on_close(popup);
        return;
    }
    if(
        popup.close_on_outside_click &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetMousePosition(),popup.rect.transform)
    ) {
        ClosePopup(popup);
        if(popup.closed && popup.on_close) popup.on_close(popup);
        return;
    }
    UpdateElements(popup.content,false,disabled);
}

void UpdateWindow(UIWindow& window, bool disabled) {
    window.close_requested = false;
    if(!window.open) return;
    const Rectangle header = CompositeHeaderRectangle(window.rect,window.style);
    const float button_size = std::max(16.0f,header.height-8.0f);
    const Rectangle close_button = {
        header.x+header.width-button_size-4.0f,
        header.y+4.0f,
        button_size,
        button_size
    };
    const Rectangle collapse_button = {
        close_button.x-button_size-4.0f,
        close_button.y,
        button_size,
        button_size
    };
    const bool can_close = window.style.closable.value_or(true);
    const bool can_collapse = window.style.collapsible.value_or(true);
    const bool can_resize = window.style.resizable.value_or(true);
    const Vector2 mouse = GetMousePosition();

    if(!disabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(can_close && CheckCollisionPointRec(mouse,close_button)) {
            window.close_requested = true;
            window.open = false;
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
            if(window.on_close) window.on_close(window);
            return;
        }
        if(can_collapse && CheckCollisionPointRec(mouse,collapse_button)) {
            window.collapsed = !window.collapsed;
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
            if(window.on_collapse) window.on_collapse(window);
            return;
        }

        Rectangle resize_handle = {
            window.rect.transform.x+window.rect.transform.width-12.0f,
            window.rect.transform.y+window.rect.transform.height-12.0f,
            12.0f,
            12.0f
        };
        if(can_resize && !window.collapsed && CheckCollisionPointRec(mouse,resize_handle)) {
            window.resizing = true;
        }
        else if(CheckCollisionPointRec(mouse,header)) {
            window.dragging = true;
        }
        window.drag_anchor = mouse;
        window.interaction_start = window.rect.transform;
    }

    if((window.dragging || window.resizing) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 delta = Vector2Subtract(mouse,window.drag_anchor);
        if(window.dragging) {
            Rectangle before = window.rect.transform;
            window.rect.runtime_offset = Vector2Add(window.rect.runtime_offset,delta);
            window.rect.transform.x = std::clamp(
                window.interaction_start.x+delta.x,
                0.0f,
                std::max(0.0f,static_cast<float>(GetScreenWidth())-window.rect.transform.width)
            );
            window.rect.transform.y = std::clamp(
                window.interaction_start.y+delta.y,
                0.0f,
                std::max(0.0f,static_cast<float>(GetScreenHeight())-header.height)
            );
            TranslateContainer(window.content,{
                window.rect.transform.x-before.x,
                window.rect.transform.y-before.y
            });
            window.drag_anchor = mouse;
            window.interaction_start = window.rect.transform;
        }
        else {
            window.rect.runtime_size_override = true;
            window.rect.runtime_size = {
                std::clamp(window.interaction_start.width+delta.x,window.style.min_width,window.style.max_width),
                std::clamp(window.interaction_start.height+delta.y,window.style.min_height,window.style.max_height)
            };
            window.rect.transform.width = window.rect.runtime_size.x;
            window.rect.transform.height = window.rect.runtime_size.y;
            window.content.layout_dirty = true;
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
        }
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        window.dragging = false;
        window.resizing = false;
    }
    if(!window.collapsed) UpdateElements(window.content,false,disabled || window.disabled);
}

void UpdateContainerScroll(UIContainer& ui, bool disabled) {
    const bool scrollable =
        ui.style.overflow == OverflowType::Auto ||
        ui.style.overflow == OverflowType::Scroll;
    if(disabled || !scrollable) {
        ui.dragging_horizontal_scrollbar = false;
        ui.dragging_vertical_scrollbar = false;
        return;
    }

    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse,ui.rect.transform);
    const float thickness = std::max(4.0f,ui.style.scrollbar_size);
    bool changed = false;

    auto vertical_thumb = [&]() {
        Rectangle bar = {
            ui.rect.transform.x+ui.rect.transform.width-thickness,
            ui.rect.transform.y,
            thickness,
            ui.rect.transform.height
        };
        const float ratio = ui.content_size.y > 0.0f
            ? std::clamp(ui.rect.transform.height/ui.content_size.y,0.08f,1.0f)
            : 1.0f;
        const float size = bar.height*ratio;
        const float t = ui.maximum_scroll.y > 0.0f
            ? ui.scroll.y/ui.maximum_scroll.y
            : 0.0f;
        return Rectangle{bar.x,bar.y+t*(bar.height-size),bar.width,size};
    };
    auto horizontal_thumb = [&]() {
        Rectangle bar = {
            ui.rect.transform.x,
            ui.rect.transform.y+ui.rect.transform.height-thickness,
            ui.rect.transform.width,
            thickness
        };
        const float ratio = ui.content_size.x > 0.0f
            ? std::clamp(ui.rect.transform.width/ui.content_size.x,0.08f,1.0f)
            : 1.0f;
        const float size = bar.width*ratio;
        const float t = ui.maximum_scroll.x > 0.0f
            ? ui.scroll.x/ui.maximum_scroll.x
            : 0.0f;
        return Rectangle{bar.x+t*(bar.width-size),bar.y,size,bar.height};
    };

    if(
        !CSS_UI_SCROLL_INPUT_CONSUMED &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    ) {
        if(ui.maximum_scroll.y > 0.0f && CheckCollisionPointRec(mouse,vertical_thumb())) {
            ui.dragging_vertical_scrollbar = true;
            ui.scrollbar_drag_anchor = mouse.y;
            ui.scrollbar_scroll_start = ui.scroll.y;
            CSS_UI_SCROLL_INPUT_CONSUMED = true;
        }
        else if(ui.maximum_scroll.x > 0.0f && CheckCollisionPointRec(mouse,horizontal_thumb())) {
            ui.dragging_horizontal_scrollbar = true;
            ui.scrollbar_drag_anchor = mouse.x;
            ui.scrollbar_scroll_start = ui.scroll.x;
            CSS_UI_SCROLL_INPUT_CONSUMED = true;
        }
    }

    if(ui.dragging_vertical_scrollbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Rectangle thumb = vertical_thumb();
        const float travel = std::max(1.0f,ui.rect.transform.height-thumb.height);
        ui.scroll.y = std::clamp(
            ui.scrollbar_scroll_start+
                (mouse.y-ui.scrollbar_drag_anchor)*ui.maximum_scroll.y/travel,
            0.0f,
            ui.maximum_scroll.y
        );
        changed = true;
    }
    if(ui.dragging_horizontal_scrollbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Rectangle thumb = horizontal_thumb();
        const float travel = std::max(1.0f,ui.rect.transform.width-thumb.width);
        ui.scroll.x = std::clamp(
            ui.scrollbar_scroll_start+
                (mouse.x-ui.scrollbar_drag_anchor)*ui.maximum_scroll.x/travel,
            0.0f,
            ui.maximum_scroll.x
        );
        changed = true;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        ui.dragging_horizontal_scrollbar = false;
        ui.dragging_vertical_scrollbar = false;
    }

    if(hovered && !CSS_UI_SCROLL_INPUT_CONSUMED) {
        const Vector2 wheel = GetMouseWheelMoveV();
        if(wheel.x != 0.0f || wheel.y != 0.0f) {
            CSS_UI_SCROLL_INPUT_CONSUMED = true;
            const bool prefer_horizontal =
                wheel.x != 0.0f ||
                IsKeyDown(KEY_LEFT_SHIFT) ||
                IsKeyDown(KEY_RIGHT_SHIFT) ||
                (ui.maximum_scroll.y <= 0.0f && ui.maximum_scroll.x > 0.0f);
            if(prefer_horizontal) {
                const float amount = wheel.x != 0.0f ? wheel.x : wheel.y;
                ui.scroll.x = std::clamp(
                    ui.scroll.x-amount*ui.style.scroll_speed,
                    0.0f,
                    ui.maximum_scroll.x
                );
            }
            else {
                ui.scroll.y = std::clamp(
                    ui.scroll.y-wheel.y*ui.style.scroll_speed,
                    0.0f,
                    ui.maximum_scroll.y
                );
            }
            changed = true;
        }
    }

    if(changed) {
        MarkLayoutDirty(ui,false);
        CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
    }
}

void UpdateElements(
    UIContainer& ui,
    bool reset_cursor,
    bool inherited_disabled
) {
    if(ui.style.display == DisplayType::None) {
        return;
    }

    if(reset_cursor) {
        GetUIDragDropContext().BeginFrame();
        CSS_UI_SCROLL_INPUT_CONSUMED = false;
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_DEFAULT);
        if(UIPopup* modal = FindOpenModal(ui)) {
            UpdatePointerFocus(modal->content);
            if(IsKeyPressed(KEY_TAB)) AdvanceKeyboardFocus(modal->content);
            UpdatePopup(*modal,false);
            if(CSS_UI_LAYOUT_CHANGE_REQUESTED && CSS_UI_ACTIVE_STYLE_SHEET != nullptr) {
                CSS_UI_LAYOUT_CHANGE_REQUESTED = false;
                LayoutContainer(
                    ui,*CSS_UI_ACTIVE_STYLE_SHEET,
                    static_cast<float>(GetScreenWidth()),
                    static_cast<float>(GetScreenHeight()),true
                );
            }
            GetUIDragDropContext().EndFrame();
            return;
        }
        UpdatePointerFocus(ui);
        if(IsKeyPressed(KEY_TAB)) AdvanceKeyboardFocus(ui);
    }
    const bool group_disabled = inherited_disabled || ui.disabled;
    ui.rect.inherited_disabled = inherited_disabled;

    for(std::size_t element_index=0; element_index<ui.elements.size(); ++element_index) {
        UIElement& element = ui.elements[element_index];
        std::visit(
            [&](auto& widget) {
                using T =
                    std::decay_t<decltype(widget)>;

                if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
                    if(widget) widget->rect.inherited_disabled = group_disabled;
                }
                else {
                    widget.rect.inherited_disabled = group_disabled;
                }

                if constexpr(std::is_same_v<T, UIButton>) {
                    if(widget.style.display != DisplayType::None) {
                        const bool own_disabled = widget.disabled;
                        widget.disabled = own_disabled || group_disabled;
                        UpdateButton(widget);
                        widget.disabled = own_disabled;
                        if(widget.clicked && widget.on_click) widget.on_click(widget);
                    }
                }
                else if constexpr(IsUIInputV<T>) {
                    widget.rect.inherited_disabled = group_disabled || widget.disabled;
                    if(widget.style.display != DisplayType::None && !group_disabled && !widget.disabled) {
                        UpdateInput(widget);
                        if(widget.changed && widget.on_change) widget.on_change(widget);
                        if(widget.submitted && widget.on_submit) widget.on_submit(widget);
                    }
                }
                else if constexpr(std::is_same_v<T,UISelectable>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateSelectable(widget);
                    widget.disabled = own_disabled;
                    if(widget.clicked && widget.on_click) widget.on_click(widget);
                }
                else if constexpr(std::is_same_v<T,UIComboBox>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateComboBox(widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(IsUIRangeInputV<T>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateRangeInput(widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(std::is_same_v<T,UICanvas>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateCanvas(widget);
                    widget.disabled = own_disabled;
                }
                else if constexpr(std::is_same_v<T,UIImage>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateImage(widget);
                    widget.disabled = own_disabled;
                    if(widget.clicked && widget.on_click) widget.on_click(widget);
                }
                else if constexpr(std::is_same_v<T,UISplitter>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled;
                    UpdateSplitter(ui,element_index,widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(widget && widget->style.display != DisplayType::None) {
                        widget->Update(group_disabled);
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    if(widget) {
                        UpdateElements(*widget,false,group_disabled);
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                    if(!widget) return;
                    bool disabled = group_disabled || widget->disabled;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        disabled,
                        widget->hovered,
                        widget->clicked
                    ) && widget->style.collapsible.value_or(true)) {
                        widget->open = !widget->open;
                        CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                    }
                    if(widget->clicked && widget->on_click) widget->on_click(*widget);
                    if(widget->open) UpdateElements(widget->content,false,disabled);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                    if(!widget) return;
                    bool disabled = group_disabled || widget->disabled;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        disabled,
                        widget->hovered,
                        widget->clicked
                    )) {
                        const Rectangle header = CompositeHeaderRectangle(widget->rect,widget->style);
                        if(GetMousePosition().x < header.x+widget->style.header_height) {
                            widget->open = !widget->open;
                            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                        }
                        else {
                            widget->selected = true;
                        }
                    }
                    if(widget->clicked && widget->on_click) widget->on_click(*widget);
                    if(widget->open) UpdateElements(widget->content,false,disabled);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                    if(!widget) return;
                    bool disabled = group_disabled || widget->disabled;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        disabled,
                        widget->hovered,
                        widget->clicked
                    )) widget->open = !widget->open;
                    if(widget->clicked) CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                    if(widget->clicked && widget->on_click) widget->on_click(*widget);
                    if(widget->open) {
                        UpdateElements(widget->content,false,disabled);
                        if(
                            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                            !CheckCollisionPointRec(GetMousePosition(),widget->rect.transform) &&
                            !CheckCollisionPointRec(GetMousePosition(),widget->content.rect.transform)
                        ) {
                            widget->open = false;
                            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                        }
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                    if(widget) UpdatePopup(*widget,group_disabled);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                    if(widget) UpdateWindow(*widget,group_disabled);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(widget) {
                        UpdateTabBar(*widget,group_disabled);
                        if(widget->changed && widget->on_change) widget->on_change(*widget);
                    }
                }

                if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
                    if(widget) UpdateElementDragDrop(widget->rect);
                }
                else {
                    UpdateElementDragDrop(widget.rect);
                }
            },
            element
        );
    }

    UpdateContainerScroll(ui,group_disabled);
    if(reset_cursor) {
        GetUIDragDropContext().EndFrame();
        SetMouseCursor(CSS_UI_REQUESTED_CURSOR);
    }

    if(
        reset_cursor &&
        CSS_UI_LAYOUT_CHANGE_REQUESTED &&
        CSS_UI_ACTIVE_STYLE_SHEET != nullptr
    ) {
        CSS_UI_LAYOUT_CHANGE_REQUESTED = false;
        SetMouseCursor(CSS_UI_REQUESTED_CURSOR);
        LayoutContainer(
            ui,*CSS_UI_ACTIVE_STYLE_SHEET,
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight()),true
        );
    }
}

void UpdateUIFrame(
    UIContainer& root,
    const StyleSheet& style_sheet,
    float width,
    float height,
    bool force_layout
) {
    if(width < 0.0f) width = static_cast<float>(GetScreenWidth());
    if(height < 0.0f) height = static_cast<float>(GetScreenHeight());
    LayoutContainer(root,style_sheet,width,height,force_layout);
    UpdateElements(root,true,false);
}

Rectangle IntersectRectangles(Rectangle first, Rectangle second) {
    float left = std::max(first.x,second.x);
    float top = std::max(first.y,second.y);
    float right = std::min(first.x + first.width,second.x + second.width);
    float bottom = std::min(first.y + first.height,second.y + second.height);
    return {left,top,std::max(0.0f,right-left),std::max(0.0f,bottom-top)};
}

bool RectanglesOverlap(Rectangle first, Rectangle second) {
    return
        first.x < second.x + second.width &&
        first.x + first.width > second.x &&
        first.y < second.y + second.height &&
        first.y + first.height > second.y;
}

bool SameScissorRectangle(Rectangle first, Rectangle second) {
    return
        static_cast<int>(first.x) == static_cast<int>(second.x) &&
        static_cast<int>(first.y) == static_cast<int>(second.y) &&
        static_cast<int>(first.width) == static_cast<int>(second.width) &&
        static_cast<int>(first.height) == static_cast<int>(second.height);
}

void ConsiderTooltip(
    UIDrawContext& context,
    UIElementBase& rect,
    const ComputedStyle& style
) {
    if(rect.tooltip.empty()) return;
    const bool hovered = CheckCollisionPointRec(GetMousePosition(),rect.transform);
    if(!hovered) {
        rect.tooltip_hover_started = -1.0;
        return;
    }
    if(rect.tooltip_hover_started < 0.0) rect.tooltip_hover_started = GetTime();
    context.tooltip_owner = &rect;
    context.tooltip_source_style = style;
}

void DrawCompositeHeader(
    UIElementBase& rect,
    ComputedStyle style,
    SelectorType selector,
    const CSS_Id& id,
    const std::string& text,
    const StyleSheet& style_sheet,
    bool hovered,
    bool active,
    bool disabled,
    bool expanded,
    bool show_arrow ) {
    rect.pseudo_states.Set("open",expanded);
    rect.pseudo_states.Set("expanded",expanded);
    AddCSSClass(style.classes,CSS_UI_HEADER_PART);
    style = ResolveInteractiveStyle(
        style_sheet,
        selector,
        style,
        false,
        hovered,
        active,
        disabled,
        &rect,
        "header"
    );
    Rectangle header = CompositeHeaderRectangle(rect,style);
    if(show_arrow) style.padding.left += 24.0f;
    DrawStyledTextBox(header,text,style,TextAlign::Left);
    if(show_arrow) {
        const float x = header.x+10.0f;
        const float y = header.y+header.height*0.5f;
        if(expanded) {
            DrawTriangle({x-4.0f,y-3.0f},{x,y+4.0f},{x+4.0f,y-3.0f},style.text_color);
        }
        else {
            DrawTriangle({x-3.0f,y-4.0f},{x-3.0f,y+4.0f},{x+4.0f,y},style.text_color);
        }
    }
    (void)id;
}
void DrawTabBar(
    UITabBar& tabs,
    const StyleSheet& style_sheet
) {
    tabs.rect.pseudo_states.Set("open",true);
    tabs.rect.pseudo_states.Set("selected",false);
    ComputedStyle tab_bar_style = ResolveInteractiveStyle(
        style_sheet,SelectorType::TabBar,tabs.style,false,false,false,
        tabs.disabled || tabs.rect.inherited_disabled,&tabs.rect,"body"
    );
    DrawStyledTextBox(tabs.rect.transform,"",tab_bar_style,TextAlign::Left);
    for(std::size_t index=0; index<tabs.tabs.size(); ++index) {
        const UITab& tab = tabs.tabs[index];
        const bool selected = static_cast<int>(index) == tabs.selected_index;
        tabs.rect.pseudo_states.Set("selected",selected);
        ComputedStyle style = tabs.style;
        AddCSSClass(style.classes,CSS_UI_TAB_PART);
        if(selected) AddCSSClass(style.classes,"selected");
        style = ResolveInteractiveStyle(
            style_sheet,
            SelectorType::TabBar,
            style,
            false,
            static_cast<int>(index) == tabs.hovered_index,
            selected,
            tabs.disabled || tab.disabled || tabs.rect.inherited_disabled,
            &tabs.rect,
            "tab:"+std::to_string(index)
        );
        const Rectangle item = TabItemRectangle(tabs,index);
        DrawStyledTextBox(item,tab.title,style,TextAlign::Center);
        if(tab.closable || tabs.style.tab_closable.value_or(false)) {
            DrawTextEx(
                ResolveUIFont(style.font),"x",
                {item.x+item.width-item.height*0.55f,item.y+(item.height-style.font_size)*0.5f},
                style.font_size,1.0f,style.text_color
            );
        }
    }
}

void DrawContainerScrollbars(
    UIContainer& ui,
    const StyleSheet& style_sheet
) {
    if(
        !ui.show_scrollbars ||
        (ui.style.overflow != OverflowType::Auto && ui.style.overflow != OverflowType::Scroll)
    ) return;

    ComputedStyle bar_style = ui.style;
    AddCSSClass(bar_style.classes,CSS_UI_SCROLLBAR_PART);
    bar_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Container,
        ui.style.id,
        ui.rect.pseudo_states.ActiveNames(),
        bar_style
    );
    bar_style = ResolveAnimatedStyle(
        ui.rect,style_sheet,std::move(bar_style),CSS_UI_SCROLLBAR_PART
    );
    ComputedStyle thumb_style = ui.style;
    AddCSSClass(thumb_style.classes,CSS_UI_SCROLLBAR_THUMB_PART);
    thumb_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Container,
        ui.style.id,
        ui.rect.pseudo_states.ActiveNames(),
        thumb_style
    );
    thumb_style = ResolveAnimatedStyle(
        ui.rect,style_sheet,std::move(thumb_style),CSS_UI_SCROLLBAR_THUMB_PART
    );
    const float thickness = std::max(4.0f,ui.style.scrollbar_size);
    const Color bar_color = bar_style.background_color.a
        ? bar_style.background_color
        : Color{20,22,28,180};
    const Color thumb_color = thumb_style.background_color.a
        ? thumb_style.background_color
        : Color{105,112,130,220};

    if(ui.maximum_scroll.y > 0.0f || ui.style.overflow == OverflowType::Scroll) {
        Rectangle bar = {
            ui.rect.transform.x+ui.rect.transform.width-thickness,
            ui.rect.transform.y,
            thickness,
            ui.rect.transform.height
        };
        DrawRectangleRec(bar,bar_color);
        const float ratio = ui.content_size.y > 0.0f
            ? std::clamp(ui.rect.transform.height/ui.content_size.y,0.08f,1.0f)
            : 1.0f;
        const float thumb_height = bar.height*ratio;
        const float t = ui.maximum_scroll.y > 0.0f
            ? ui.scroll.y/ui.maximum_scroll.y
            : 0.0f;
        DrawRectangleRounded(
            {bar.x,bar.y+t*(bar.height-thumb_height),bar.width,thumb_height},
            0.5f,
            4,
            thumb_color
        );
    }
    if(ui.maximum_scroll.x > 0.0f || ui.style.overflow == OverflowType::Scroll) {
        Rectangle bar = {
            ui.rect.transform.x,
            ui.rect.transform.y+ui.rect.transform.height-thickness,
            ui.rect.transform.width,
            thickness
        };
        DrawRectangleRec(bar,bar_color);
        const float ratio = ui.content_size.x > 0.0f
            ? std::clamp(ui.rect.transform.width/ui.content_size.x,0.08f,1.0f)
            : 1.0f;
        const float thumb_width = bar.width*ratio;
        const float t = ui.maximum_scroll.x > 0.0f
            ? ui.scroll.x/ui.maximum_scroll.x
            : 0.0f;
        DrawRectangleRounded(
            {bar.x+t*(bar.width-thumb_width),bar.y,thumb_width,bar.height},
            0.5f,
            4,
            thumb_color
        );
    }
}

void SetUIDrawScissor(
    UIDrawContext& context,
    bool enabled,
    Rectangle rectangle ) {
    if(
        context.scissor_active == enabled &&
        (
            !enabled ||
            SameScissorRectangle(context.scissor,rectangle)
        )
    ) {
        return;
    }

    if(context.scissor_active) {
        EndScissorMode();
        context.scissor_active = false;
    }

    if(
        enabled &&
        rectangle.width > 0.0f &&
        rectangle.height > 0.0f
    ) {
        BeginScissorMode(
            static_cast<int>(rectangle.x),
            static_cast<int>(rectangle.y),
            static_cast<int>(rectangle.width),
            static_cast<int>(rectangle.height)
        );

        context.scissor_active = true;
        context.scissor = rectangle;
    }
}

void DrawContainerBox(UIContainer& ui, const StyleSheet& style_sheet) {
    if(ui.style.display == DisplayType::None) return;
    const bool hovered = CheckCollisionPointRec(GetMousePosition(),ui.rect.transform);
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,SelectorType::Container,ui.style,
        false,hovered,hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        ui.disabled || ui.rect.inherited_disabled,&ui.rect
    );

    const Color background = ApplyStyleOpacity(
        style.background_color,
        style.opacity
    );

    if(background.a > 0) {
        DrawRectangleRounded(
            ui.rect.transform,
            style.border_radius,
            8,
            background
        );
    }

    const Color border = ApplyStyleOpacity(
        style.border_color,
        style.opacity
    );

    if(style.border_width > 0.0f && border.a > 0) {
        DrawRectangleRoundedLinesEx(
            ui.rect.transform,
            style.border_radius,
            8,
            style.border_width,
            border
        );
    }
}

void DrawElementsClippedInternal(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    Rectangle parent_clip,
    bool parent_clip_active,
    UIDrawContext& context
) {
    if(ui.style.display == DisplayType::None) {
        return;
    }

    if(
        parent_clip_active &&
        (
            parent_clip.width <= 0.0f ||
            parent_clip.height <= 0.0f ||
            !RectanglesOverlap(ui.rect.transform,parent_clip)
        )
    ) {
        return;
    }

    // The container itself is clipped by its parent, not by its own overflow.
    SetUIDrawScissor(
        context,
        parent_clip_active,
        parent_clip
    );

    DrawContainerBox(ui,style_sheet);

    Rectangle child_clip = parent_clip;
    bool child_clip_active = parent_clip_active;

    if(ui.style.overflow != OverflowType::Visible) {
        child_clip = child_clip_active
            ? IntersectRectangles(
                child_clip,
                ui.rect.transform
            )
            : ui.rect.transform;

        child_clip_active = true;
    }

    if(
        child_clip_active &&
        (
            child_clip.width <= 0.0f ||
            child_clip.height <= 0.0f
        )
    ) {
        return;
    }

    for(auto& element : ui.elements) {
        std::visit(
            [&](auto& widget) {
                using T = std::decay_t<decltype(widget)>;

                if constexpr(std::is_same_v<T,UIInput<Color>>) {
                    if(
                        widget.input_type == InputType::ColorPicker &&
                        widget.picker_open &&
                        widget.style.display != DisplayType::None
                    ) {
                        context.active_color_picker = &widget;
                    }
                }

                if constexpr(std::is_same_v<T,UIButton>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (
                            !child_clip_active ||
                            RectanglesOverlap(
                                widget.rect.transform,
                                child_clip
                            )
                        )
                    ) {
                        SetUIDrawScissor(
                            context,
                            child_clip_active,
                            child_clip
                        );

                        DrawButton(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UIImage>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawImage(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UISplitter>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawSplitter(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UILabel>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (
                            !child_clip_active ||
                            RectanglesOverlap(
                                widget.rect.transform,
                                child_clip
                            )
                        )
                    ) {
                        SetUIDrawScissor(
                            context,
                            child_clip_active,
                            child_clip
                        );

                        DrawLabel(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(IsUIInputV<T>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (
                            !child_clip_active ||
                            RectanglesOverlap(
                                widget.rect.transform,
                                child_clip
                            )
                        )
                    ) {
                        SetUIDrawScissor(
                            context,
                            child_clip_active,
                            child_clip
                        );

                        DrawInput(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UISelectable>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawSelectable(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UIComboBox>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawComboBoxBase(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                        if(widget.open) context.open_combos.push_back(&widget);
                    }
                }
                else if constexpr(IsUIRangeInputV<T>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawRangeInput(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UICanvas>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        DrawCanvas(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(
                        widget &&
                        widget->style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget->rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        widget->Draw(style_sheet);
                        ConsiderTooltip(context,widget->rect,widget->style);
                    }
                }
                else if constexpr(
                    std::is_same_v<
                        T,
                        std::shared_ptr<UIContainer>
                    >
                ) {
                    if(widget) {
                        DrawElementsClippedInternal(
                            *widget,
                            style_sheet,
                            child_clip,
                            child_clip_active,
                            context
                        );
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    DrawCompositeHeader(
                        widget->rect,widget->style,SelectorType::Collapsible,
                        widget->style.id,widget->text,style_sheet,
                        widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,
                        widget->open
                    );
                    ConsiderTooltip(context,widget->rect,widget->style);
                    if(widget->open) {
                        DrawElementsClippedInternal(
                            widget->content,style_sheet,child_clip,child_clip_active,context
                        );
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    ComputedStyle style = widget->style;
                    if(widget->selected) AddCSSClass(style.classes,"selected");
                    widget->rect.pseudo_states.Set("selected",widget->selected);
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    DrawCompositeHeader(
                        widget->rect,style,SelectorType::TreeNode,
                        widget->style.id,widget->text,style_sheet,
                        widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,
                        widget->open
                    );
                    ConsiderTooltip(context,widget->rect,widget->style);
                    if(widget->open) {
                        DrawElementsClippedInternal(
                            widget->content,style_sheet,child_clip,child_clip_active,context
                        );
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    DrawCompositeHeader(
                        widget->rect,widget->style,SelectorType::Menu,
                        widget->style.id,widget->text,style_sheet,
                        widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,
                        widget->open,false
                    );
                    ConsiderTooltip(context,widget->rect,widget->style);
                    if(widget->open) context.open_menus.push_back(widget.get());
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                    if(widget && widget->open) context.open_popups.push_back(widget.get());
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                    if(widget && widget->open) context.open_windows.push_back(widget.get());
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    DrawTabBar(*widget,style_sheet);
                    ConsiderTooltip(context,widget->rect,widget->style);
                    if(UITab* selected = widget->SelectedTab()) {
                        DrawElementsClippedInternal(
                            selected->content,
                            style_sheet,
                            child_clip,
                            child_clip_active,
                            context
                        );
                    }
                }
            },
            element
        );
    }

    SetUIDrawScissor(context,child_clip_active,child_clip);
    DrawContainerScrollbars(ui,style_sheet);
}

void DrawQueuedOverlays(
    UIDrawContext& context,
    const StyleSheet& style_sheet,
    Rectangle screen_clip
) {
    SetUIDrawScissor(context,false);

    for(std::size_t index=0; index<context.open_windows.size(); ++index) {
        UIWindow* window = context.open_windows[index];
        if(window == nullptr || !window->open) continue;
        window->rect.pseudo_states.Set("open",window->open);
        window->rect.pseudo_states.Set("collapsed",window->collapsed);
        ComputedStyle window_style = ResolveInteractiveStyle(
            style_sheet,SelectorType::Window,window->style,false,false,window->dragging,
            window->disabled || window->rect.inherited_disabled,&window->rect,"body"
        );
        DrawStyledTextBox(window->rect.transform,"",window_style,TextAlign::Left);
        DrawCompositeHeader(
            window->rect,window->style,SelectorType::Window,
            window->style.id,window->title,style_sheet,
            false,window->dragging,
            window->disabled || window->rect.inherited_disabled,
            !window->collapsed,false
        );
        const Rectangle header = CompositeHeaderRectangle(window->rect,window->style);
        const float button_size = std::max(16.0f,header.height-8.0f);
        const Rectangle close_button = {
            header.x+header.width-button_size-4.0f,
            header.y+4.0f,
            button_size,
            button_size
        };
        const Rectangle collapse_button = {
            close_button.x-button_size-4.0f,
            close_button.y,
            button_size,
            button_size
        };
        if(window->style.closable.value_or(true)) {
            DrawRectangleRounded(
                close_button,0.2f,4,
                CheckCollisionPointRec(GetMousePosition(),close_button)
                    ? DarkenColor(window->style.background_color,0.12f)
                    : BLANK
            );
            DrawText(
                "x",
                static_cast<int>(close_button.x+close_button.width*0.5f-4.0f),
                static_cast<int>(close_button.y+3.0f),
                static_cast<int>(std::max(12.0f,close_button.height-6.0f)),
                window->style.text_color
            );
        }
        if(window->style.collapsible.value_or(true)) {
            DrawRectangleRounded(
                collapse_button,0.2f,4,
                CheckCollisionPointRec(GetMousePosition(),collapse_button)
                    ? DarkenColor(window->style.background_color,0.12f)
                    : BLANK
            );
            DrawText(
                window->collapsed ? "+" : "-",
                static_cast<int>(collapse_button.x+collapse_button.width*0.5f-4.0f),
                static_cast<int>(collapse_button.y+2.0f),
                static_cast<int>(std::max(12.0f,collapse_button.height-5.0f)),
                window->style.text_color
            );
        }
        if(!window->collapsed) {
            DrawElementsClippedInternal(
                window->content,style_sheet,screen_clip,true,context
            );
            if(window->style.resizable.value_or(true)) {
                SetUIDrawScissor(context,false);
                DrawRectangle(
                    static_cast<int>(window->rect.transform.x+window->rect.transform.width-10.0f),
                    static_cast<int>(window->rect.transform.y+window->rect.transform.height-10.0f),
                    8,8,window->style.border_color
                );
            }
        }
    }

    for(std::size_t index=0; index<context.open_menus.size(); ++index) {
        UIMenu* menu = context.open_menus[index];
        if(menu == nullptr || !menu->open) continue;
        DrawElementsClippedInternal(
            menu->content,style_sheet,screen_clip,true,context
        );
    }

    SetUIDrawScissor(context,false);
    for(UIComboBox* combo : context.open_combos) {
        if(combo != nullptr) DrawComboBoxPopup(*combo,style_sheet);
    }

    const std::size_t menus_before_popups = context.open_menus.size();
    const std::size_t combos_before_popups = context.open_combos.size();

    for(std::size_t index=0; index<context.open_popups.size(); ++index) {
        UIPopup* popup = context.open_popups[index];
        if(popup == nullptr || !popup->open) continue;
        SetUIDrawScissor(context,false);
        if(popup->modal) {
            DrawRectangle(
                0,0,GetScreenWidth(),GetScreenHeight(),
                Color{0,0,0,135}
            );
        }
        popup->rect.pseudo_states.Set("open",popup->open);
        ComputedStyle popup_style = ResolveInteractiveStyle(
            style_sheet,SelectorType::Popup,popup->style,false,false,false,
            popup->rect.inherited_disabled,&popup->rect,"body"
        );
        DrawStyledTextBox(popup->rect.transform,"",popup_style,TextAlign::Left);
        if(!popup->title.empty()) {
            DrawCompositeHeader(
                popup->rect,popup->style,SelectorType::Popup,
                popup->style.id,popup->title,style_sheet,
                false,false,false,true,false
            );
        }
        DrawElementsClippedInternal(
            popup->content,style_sheet,screen_clip,true,context
        );
    }

    for(std::size_t index=menus_before_popups; index<context.open_menus.size(); ++index) {
        UIMenu* menu = context.open_menus[index];
        if(menu == nullptr || !menu->open) continue;
        DrawElementsClippedInternal(
            menu->content,style_sheet,screen_clip,true,context
        );
    }

    SetUIDrawScissor(context,false);
    for(std::size_t index=combos_before_popups; index<context.open_combos.size(); ++index) {
        UIComboBox* combo = context.open_combos[index];
        if(combo != nullptr) DrawComboBoxPopup(*combo,style_sheet);
    }
    SetUIDrawScissor(context,false);
}

void DrawTooltip(
    UIDrawContext& context,
    const StyleSheet& style_sheet
) {
    UIElementBase* owner = context.tooltip_owner;
    if(
        owner == nullptr ||
        owner->tooltip.empty() ||
        owner->tooltip_hover_started < 0.0 ||
        GetTime()-owner->tooltip_hover_started < context.tooltip_source_style.tooltip_delay
    ) return;

    ComputedStyle style;
    style.background_color = Color{24,26,32,248};
    style.border_color = Color{92,98,112,255};
    style.text_color = Color{245,246,250,255};
    style.border_width = 1.0f;
    style.border_radius = 0.15f;
    style.font_size = 15.0f;
    style.padding = EdgeValues(7.0f);
    style = ResolveStyle(
        style_sheet,
        SelectorType::Tooltip,
        std::monostate{},
        SelectorState::None,
        style
    );
    Font font = ResolveUIFont(style.font);
    Vector2 measured = MeasureTextCached(
        font,owner->tooltip.c_str(),style.font_size,1.0f
    );
    Rectangle rectangle = {
        GetMousePosition().x+14.0f,
        GetMousePosition().y+18.0f,
        measured.x+style.padding.left+style.padding.right,
        measured.y+style.padding.top+style.padding.bottom
    };
    rectangle.x = std::clamp(
        rectangle.x,0.0f,
        std::max(0.0f,static_cast<float>(GetScreenWidth())-rectangle.width)
    );
    rectangle.y = std::clamp(
        rectangle.y,0.0f,
        std::max(0.0f,static_cast<float>(GetScreenHeight())-rectangle.height)
    );
    DrawStyledTextBox(rectangle,owner->tooltip,style,TextAlign::Left);
}

void DrawElementsClipped(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    Rectangle clip
) {
    UIDrawContext context;
    DrawElementsClippedInternal(
        ui,
        style_sheet,
        clip,
        true,
        context
    );

    Rectangle screen_clip = {
        0.0f,0.0f,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight())
    };
    DrawQueuedOverlays(context,style_sheet,screen_clip);

    if(context.active_color_picker != nullptr) {
        DrawColorPickerPopup(
            *context.active_color_picker,
            style_sheet
        );
    }
    DrawTooltip(context,style_sheet);
}

void DrawElementsClipped(
    UIContainer& ui,
    const std::unordered_map<std::string,Font>& fonts,
    const StyleSheet& style_sheet,
    Rectangle clip
) {
    SetUIFontRegistry(fonts);
    DrawElementsClipped(ui,style_sheet,clip);
}

void DrawInputPopups(
    UIContainer& ui,
    const StyleSheet& style_sheet
) {
    if(ui.style.display == DisplayType::None) {
        return;
    }

    for(auto& element : ui.elements) {
        std::visit(
            [&](auto& widget) {
                using T =
                    std::decay_t<decltype(widget)>;

                if constexpr(std::is_same_v<T,UIInput<Color>>) {
                    DrawColorPickerPopup(
                        widget,
                        style_sheet
                    );
                }
                else if constexpr(
                    std::is_same_v<
                        T,
                        std::shared_ptr<UIContainer>
                    >
                ) {
                    if(widget) {
                        DrawInputPopups(
                            *widget,
                            style_sheet
                        );
                    }
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(!widget) return;
                    if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                        for(UIDockPanel& panel : dock->panels) {
                            if(panel.open) DrawInputPopups(panel.content,style_sheet);
                        }
                    }
                }
                else if constexpr(IsUICompositePointerV<T>) {
                    if(!widget) return;
                    if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                        if(UITab* tab = widget->SelectedTab()) {
                            DrawInputPopups(tab->content,style_sheet);
                        }
                    }
                    else if constexpr(!std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                        DrawInputPopups(widget->content,style_sheet);
                    }
                }
            },
            element
        );
    }
}

void DrawElements(
    UIContainer& ui,
    const std::unordered_map<std::string,Font>& fonts,
    const StyleSheet& style_sheet
) {
    SetUIFontRegistry(fonts);

    Rectangle screen_clip = {
        0.0f,
        0.0f,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight())
    };

    UIDrawContext context;
    DrawElementsClippedInternal(
        ui,
        style_sheet,
        screen_clip,
        true,
        context
    );

    DrawQueuedOverlays(context,style_sheet,screen_clip);

    if(context.active_color_picker != nullptr) {
        DrawColorPickerPopup(
            *context.active_color_picker,
            style_sheet
        );
    }
    DrawTooltip(context,style_sheet);
}
