#include "css_emulator.h"

namespace {
UIComboBox* FindOpenComboBox(UIContainer& container) {
    UIComboBox* found = nullptr;

    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if(found != nullptr) return;

            if constexpr(std::is_same_v<T,UIComboBox>) {
                if(widget.open && widget.style.display != DisplayType::None) {
                    found = &widget;
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                if(widget && widget->style.display != DisplayType::None) {
                    found = FindOpenComboBox(*widget);
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                if(widget && widget->open) found = FindOpenComboBox(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                if(widget && widget->open) found = FindOpenComboBox(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                if(widget && widget->open) found = FindOpenComboBox(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                if(widget && widget->open) found = FindOpenComboBox(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                if(widget && widget->open && !widget->collapsed) {
                    found = FindOpenComboBox(widget->content);
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(widget) {
                    if(UITab* tab = widget->SelectedTab()) {
                        found = FindOpenComboBox(tab->content);
                    }
                }
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(widget) {
                    if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                        for(UIDockPanel& panel : dock->panels) {
                            if(!panel.open) continue;
                            found = FindOpenComboBox(panel.content);
                            if(found != nullptr) break;
                        }
                    }
                    else if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                        found = FindOpenComboBox(viewport->content);
                    }
                    else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                        found = FindOpenComboBox(item->content);
                    }
                }
            }
        },element);

        if(found != nullptr) break;
    }

    return found;
}

UIComboBox* g_open_combo = nullptr;
bool g_combo_captures_pointer = false;

bool ComboOwnsPointerThisFrame(UIComboBox* combo) {
    // An open ComboBox is a real overlay. Keep pointer ownership for its
    // complete lifetime so hover/focus/drag state cannot leak into controls
    // underneath it between mouse events.
    return combo != nullptr && combo->open;
}

// Non-modal Popup (context menus, tooltip-style popups, and anything opened
// via OpenPopupAt without OpenModal) is drawn as a screen-space overlay on
// top of the rest of the tree, exactly like a ComboBox dropdown or an open
// color picker. Unlike those, however, it was previously updated purely
// in-place during the normal document-order walk, so it never claimed
// pointer ownership for the frame: whatever the popup happened to be drawn
// over kept receiving clicks/drags as if the popup were not there. This
// walker mirrors FindOpenComboBox/FindOpenModal so the same "owns pointer
// for its complete open lifetime" rule can be applied to it too.
UIPopup* FindOpenNonModalPopup(UIContainer& container) {
    UIPopup* found = nullptr;
    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if(found != nullptr) return;
            if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                if(widget && widget->open && !widget->modal) found = widget.get();
                else if(widget && widget->open) found = FindOpenNonModalPopup(widget->content);
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return;
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) {
                        if(panel.open && (found = FindOpenNonModalPopup(panel.content)) != nullptr) break;
                    }
                }
                else if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                    found = FindOpenNonModalPopup(viewport->content);
                }
                else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    found = FindOpenNonModalPopup(item->content);
                }
            }
            else if constexpr(IsUICompositePointerV<T>) {
                if(!widget) return;
                if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(UITab* tab = widget->SelectedTab()) {
                        found = FindOpenNonModalPopup(tab->content);
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    if(widget->style.display != DisplayType::None) {
                        found = FindOpenNonModalPopup(*widget);
                    }
                }
                else {
                    bool visible = true;
                    if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) visible = widget->open;
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) visible = widget->open && !widget->collapsed;
                    if(visible) found = FindOpenNonModalPopup(widget->content);
                }
            }
        },element);
        if(found != nullptr) break;
    }
    return found;
}

UIPopup* g_open_popup = nullptr;

bool PopupOwnsPointerThisFrame(UIPopup* popup) {
    return popup != nullptr && popup->open;
}

// Same story as the popup case above, but for the ColorPicker overlay owned
// by a UIInput<Color>: UpdateInput() only ever hit-tests the picker's own
// popup/close/saturation/hue/alpha rectangles against the mouse. It never
// told the rest of the frame that a big chunk of the screen is now covered
// by that popup, so any widget whose layout rect happened to sit underneath
// it stayed fully clickable/draggable while the picker was open.
UIInput<Color>* FindOpenColorPicker(UIContainer& container) {
    UIInput<Color>* found = nullptr;
    for(UIElement& element : container.elements) {
        std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if(found != nullptr) return;
            if constexpr(std::is_same_v<T,UIInput<Color>>) {
                if(
                    widget.input_type == InputType::ColorPicker &&
                    widget.picker_open &&
                    widget.style.display != DisplayType::None
                ) {
                    found = &widget;
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                if(widget && widget->style.display != DisplayType::None) {
                    found = FindOpenColorPicker(*widget);
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                if(widget && widget->open) found = FindOpenColorPicker(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                if(widget && widget->open) found = FindOpenColorPicker(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                if(widget && widget->open) found = FindOpenColorPicker(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                if(widget && widget->open) found = FindOpenColorPicker(widget->content);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                if(widget && widget->open && !widget->collapsed) {
                    found = FindOpenColorPicker(widget->content);
                }
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(widget) {
                    if(UITab* tab = widget->SelectedTab()) {
                        found = FindOpenColorPicker(tab->content);
                    }
                }
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(widget) {
                    if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                        for(UIDockPanel& panel : dock->panels) {
                            if(!panel.open) continue;
                            found = FindOpenColorPicker(panel.content);
                            if(found != nullptr) break;
                        }
                    }
                    else if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                        found = FindOpenColorPicker(viewport->content);
                    }
                    else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                        found = FindOpenColorPicker(item->content);
                    }
                }
            }
        },element);
        if(found != nullptr) break;
    }
    return found;
}

UIInput<Color>* g_open_color_picker = nullptr;

bool ColorPickerOwnsPointerThisFrame(UIInput<Color>* picker) {
    // Same rationale as ComboOwnsPointerThisFrame: the picker popup renders
    // as a screen-space overlay on top of the rest of the tree, so it must
    // own the pointer for its complete open lifetime, or clicks pass
    // straight through it onto whatever it happens to be drawn over.
    return picker != nullptr && picker->picker_open;
}
}

namespace {
struct UIInteractionClipState {
    Rectangle rectangle = {};
    bool active = false;
};

thread_local std::vector<UIInteractionClipState> g_focus_interaction_clips;
thread_local std::vector<UIInteractionClipState> g_update_interaction_clips;
thread_local bool g_collecting_pointer_focus = false;

Rectangle IntersectInteractionRectangles(Rectangle first,Rectangle second) {
    const float left=std::max(first.x,second.x);
    const float top=std::max(first.y,second.y);
    const float right=std::min(first.x+first.width,second.x+second.width);
    const float bottom=std::min(first.y+first.height,second.y+second.height);
    return {
        left,top,
        std::max(0.0f,right-left),
        std::max(0.0f,bottom-top)
    };
}

bool InteractionRectanglesOverlap(Rectangle first,Rectangle second) {
    return
        first.width>0.0f && first.height>0.0f &&
        second.width>0.0f && second.height>0.0f &&
        first.x<second.x+second.width &&
        first.x+first.width>second.x &&
        first.y<second.y+second.height &&
        first.y+first.height>second.y;
}

UIInteractionClipState ResolveInteractionClip(
    const UIContainer& container,
    const std::vector<UIInteractionClipState>& stack
) {
    UIInteractionClipState result;
    if(!stack.empty()) result=stack.back();

    if(container.style.overflow!=OverflowType::Visible) {
        if(result.active) {
            result.rectangle=IntersectInteractionRectangles(
                result.rectangle,container.rect.transform
            );
        }
        else {
            result.rectangle=container.rect.transform;
            result.active=true;
        }
    }
    return result;
}

bool InteractionPointerInside(const UIInteractionClipState& clip) {
    return
        !clip.active ||
        (
            clip.rectangle.width>0.0f &&
            clip.rectangle.height>0.0f &&
            CheckCollisionPointRec(GetUIMousePosition(),clip.rectangle)
        );
}

class ScopedUIInteractionClip {
public:
    ScopedUIInteractionClip(
        std::vector<UIInteractionClipState>& stack,
        const UIContainer& container,
        bool block_pointer
    )
        : stack_(stack),
          state_(ResolveInteractionClip(container,stack))
    {
        stack_.push_back(state_);
        if(block_pointer && !InteractionPointerInside(state_)) {
            forced_pointer_block_=true;
            pointer_consumed_before_=CSS_UI_POINTER_INPUT_CONSUMED;
            CSS_UI_POINTER_INPUT_CONSUMED=true;
        }
    }

    ~ScopedUIInteractionClip() {
        if(forced_pointer_block_) {
            CSS_UI_POINTER_INPUT_CONSUMED=pointer_consumed_before_;
        }
        stack_.pop_back();
    }

    const UIInteractionClipState& State() const { return state_; }
    bool BlocksPointer() const { return forced_pointer_block_; }
    bool HadPointerCaptureBefore() const { return pointer_consumed_before_; }

private:
    std::vector<UIInteractionClipState>& stack_;
    UIInteractionClipState state_;
    bool forced_pointer_block_=false;
    bool pointer_consumed_before_=false;
};
}

void CollectFocusableElements(
    UIContainer& container,
    std::vector<UIFocusEntry>& focus,
    bool inherited_disabled ) {
    if(container.style.display == DisplayType::None) return;

    const std::size_t first_focus_entry=focus.size();
    ScopedUIInteractionClip interaction_clip(
        g_focus_interaction_clips,container,false
    );

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
                else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                    if(item->visible && item->interactive) {
                        CollectFocusableElements(item->content,focus,group_disabled || item->disabled);
                    }
                }
                // UIViewport intentionally does not recurse here. It owns a
                // transformed coordinate space and performs focus routing from
                // UIViewport::Update after pushing its world-space input context.
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

    // Pointer focus must use the visible part of each element, not its full
    // unclipped layout rectangle. Keyboard focus keeps the existing behavior.
    const UIInteractionClipState& clip=interaction_clip.State();
    if(clip.active && g_collecting_pointer_focus) {
        const Vector2 mouse=GetUIMousePosition();
        auto begin=focus.begin()+
            static_cast<std::vector<UIFocusEntry>::difference_type>(
                first_focus_entry
            );
        focus.erase(
            std::remove_if(
                begin,focus.end(),
                [&](const UIFocusEntry& entry) {
                    if(entry.rect==nullptr) return true;
                    const Rectangle visible=IntersectInteractionRectangles(
                        entry.rect->transform,clip.rectangle
                    );
                    return
                        !InteractionRectanglesOverlap(
                            entry.rect->transform,clip.rectangle
                        ) ||
                        !CheckCollisionPointRec(mouse,visible);
                }
            ),
            focus.end()
        );
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
    const bool previous_pointer_focus_collection=g_collecting_pointer_focus;
    g_collecting_pointer_focus=true;
    CollectFocusableElements(root,focus);
    g_collecting_pointer_focus=previous_pointer_focus_collection;
    UIFocusEntry* target = nullptr;
    const Vector2 mouse = GetUIMousePosition();
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
    hovered = !disabled && CheckCollisionPointRec(GetUIMousePosition(),header);
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
    const bool interaction_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;
    const Vector2 mouse = GetUIMousePosition();
    for(std::size_t index=0; index<tabs.tabs.size(); ++index) {
        UITab& tab = tabs.tabs[index];
        tab.close_requested = false;
        const Rectangle item = TabItemRectangle(tabs,index);
        if(interaction_blocked || tab.disabled || !CheckCollisionPointRec(mouse,item)) continue;
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
    const bool interaction_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;
    if(!interaction_blocked && popup.close_on_escape && IsKeyPressed(KEY_ESCAPE)) {
        ClosePopup(popup);
        if(popup.closed && popup.on_close) popup.on_close(popup);
        return;
    }
    if(
        !interaction_blocked &&
        popup.close_on_outside_click &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetUIMousePosition(),popup.rect.transform)
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
    const Vector2 mouse = GetUIMousePosition();
    const bool interaction_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;

    if(!interaction_blocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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

    if(!interaction_blocked && (window.dragging || window.resizing) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 delta = Vector2Subtract(mouse,window.drag_anchor);
        if(window.dragging) {
            Rectangle before = window.rect.transform;
            window.rect.runtime_offset = Vector2Add(window.rect.runtime_offset,delta);
            const Rectangle visible=CurrentUIVisibleLogicalScreenRect();
            window.rect.transform.x = std::clamp(
                window.interaction_start.x+delta.x,
                visible.x,
                std::max(visible.x,visible.x+visible.width-window.rect.transform.width)
            );
            window.rect.transform.y = std::clamp(
                window.interaction_start.y+delta.y,
                visible.y,
                std::max(visible.y,visible.y+visible.height-header.height)
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

    const Vector2 mouse = GetUIMousePosition();
    const bool pointer_inside_clip=
        g_update_interaction_clips.empty() ||
        InteractionPointerInside(g_update_interaction_clips.back());
    const bool hovered =
        pointer_inside_clip &&
        CheckCollisionPointRec(mouse,ui.rect.transform);
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
        CSS_UI_POINTER_INPUT_CONSUMED = false;
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_DEFAULT);

        g_open_combo = FindOpenComboBox(ui);
        g_combo_captures_pointer = ComboOwnsPointerThisFrame(g_open_combo);

        g_open_color_picker = FindOpenColorPicker(ui);
        const bool picker_captures_pointer = ColorPickerOwnsPointerThisFrame(g_open_color_picker);

        g_open_popup = FindOpenNonModalPopup(ui);
        const bool popup_captures_pointer = PopupOwnsPointerThisFrame(g_open_popup);

        if(g_combo_captures_pointer || picker_captures_pointer || popup_captures_pointer) {
            CSS_UI_POINTER_INPUT_CONSUMED = true;
            CSS_UI_SCROLL_INPUT_CONSUMED = true;
        }

        const bool overlay_captures_pointer =
            g_combo_captures_pointer || picker_captures_pointer || popup_captures_pointer;

        if(UIPopup* modal = FindOpenModal(ui)) {
            if(!overlay_captures_pointer) UpdatePointerFocus(modal->content);
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
        if(!overlay_captures_pointer) UpdatePointerFocus(ui);
        if(IsKeyPressed(KEY_TAB)) AdvanceKeyboardFocus(ui);
    }

    // Match the draw scissor hierarchy during interaction. When the pointer is
    // outside this container's effective overflow clip, block new pointer hits
    // only for this subtree, then restore the previous capture state so later
    // siblings can still receive the same frame's input.
    ScopedUIInteractionClip interaction_clip(
        g_update_interaction_clips,ui,true
    );

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
                        widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                        UpdateButton(widget);
                        widget.disabled = own_disabled;
                        if(widget.clicked && widget.on_click) widget.on_click(widget);
                    }
                }
                else if constexpr(IsUIInputV<T>) {
                    // Pointer capture blocks interaction; it must not masquerade
                    // as :disabled or RCSS can fade/reset the visible input style.
                    widget.rect.inherited_disabled = group_disabled || widget.disabled;

                    // Binding is data synchronization, not pointer interaction. Keep
                    // the retained value current even while the widget is disabled or
                    // another overlay owns the pointer. Pulling never emits OnChange.
                    if(widget.bound_value != nullptr) {
                        widget.value = *widget.bound_value;
                    }

                    const bool continue_clipped_capture=
                        interaction_clip.BlocksPointer() &&
                        !interaction_clip.HadPointerCaptureBefore() &&
                        (widget.active || widget.dragging);

                    // The color picker's own popup is the thing that may have
                    // just claimed CSS_UI_POINTER_INPUT_CONSUMED for the frame
                    // (see FindOpenColorPicker/ColorPickerOwnsPointerThisFrame);
                    // it must still be able to update itself (drag hue/sat/alpha,
                    // close on outside click, etc.) rather than being locked out
                    // by its own overlay.
                    bool owns_open_picker = false;
                    if constexpr(std::is_same_v<T,UIInput<Color>>) {
                        owns_open_picker = (&widget == g_open_color_picker);
                    }

                    if(
                        widget.style.display != DisplayType::None &&
                        !group_disabled &&
                        !widget.disabled &&
                        (!CSS_UI_POINTER_INPUT_CONSUMED || continue_clipped_capture || owns_open_picker)
                    ) {
                        UpdateInput(widget);

                        if(widget.bound_value != nullptr && widget.changed) {
                            *widget.bound_value = widget.value;
                        }
                        if(widget.changed && widget.on_change) widget.on_change(widget);
                        if(widget.submitted && widget.on_submit) widget.on_submit(widget);
                    }
                }
                else if constexpr(std::is_same_v<T,UISelectable>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    UpdateSelectable(widget);
                    widget.disabled = own_disabled;
                    if(widget.clicked && widget.on_click) widget.on_click(widget);
                }
                else if constexpr(std::is_same_v<T,UIComboBox>) {
                    const bool own_disabled = widget.disabled;
                    const bool owns_popup = &widget == g_open_combo;
                    widget.disabled = own_disabled || group_disabled ||
                        (CSS_UI_POINTER_INPUT_CONSUMED && !owns_popup);
                    UpdateComboBox(widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(IsUIRangeInputV<T>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    UpdateRangeInput(widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(std::is_same_v<T,UICanvas>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    UpdateCanvas(widget);
                    widget.disabled = own_disabled;
                }
                else if constexpr(std::is_same_v<T,UIImage>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    UpdateImage(widget);
                    widget.disabled = own_disabled;
                    if(widget.clicked && widget.on_click) widget.on_click(widget);
                }
                else if constexpr(std::is_same_v<T,UISplitter>) {
                    const bool own_disabled = widget.disabled;
                    widget.disabled = own_disabled || group_disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    UpdateSplitter(ui,element_index,widget);
                    widget.disabled = own_disabled;
                    if(widget.changed && widget.on_change) widget.on_change(widget);
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(widget && widget->style.display != DisplayType::None) {
                        // DockSpace owns child UI containers. Pointer capture by an
                        // overlay (notably an open ComboBox) must block dock chrome
                        // interaction without disabling the panel subtree that owns
                        // the overlay, otherwise the ComboBox can never receive the
                        // click that selects/closes it.
                        if(
                            dynamic_cast<UIDockSpace*>(widget.get()) != nullptr ||
                            dynamic_cast<UIViewport*>(widget.get()) != nullptr ||
                            dynamic_cast<UIViewportItem*>(widget.get()) != nullptr
                        ) {
                            // Composite custom elements own retained child trees.
                            // Keep those trees updating under overlay capture so the
                            // overlay owner can receive the click that closes/selects
                            // it. Their own chrome/actions remain gated by the global
                            // capture flags.
                            widget->Update(group_disabled);
                        }
                        else if(!CSS_UI_POINTER_INPUT_CONSUMED) {
                            // The custom-element API has only a disabled flag.
                            // Skip interaction while an overlay owns the pointer
                            // instead of forcing a false disabled visual state.
                            widget->Update(group_disabled);
                        }
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    if(widget) {
                        UpdateElements(*widget,false,group_disabled);
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                    if(!widget) return;
                    const bool disabled = group_disabled || widget->disabled;
                    const bool header_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        header_blocked,
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
                    const bool disabled = group_disabled || widget->disabled;
                    const bool header_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        header_blocked,
                        widget->hovered,
                        widget->clicked
                    )) {
                        const Rectangle header = CompositeHeaderRectangle(widget->rect,widget->style);
                        if(GetUIMousePosition().x < header.x+widget->style.header_height) {
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
                    const bool disabled = group_disabled || widget->disabled;
                    const bool header_blocked = disabled || CSS_UI_POINTER_INPUT_CONSUMED;
                    if(UpdateCompositeHeader(
                        CompositeHeaderRectangle(widget->rect,widget->style),
                        header_blocked,
                        widget->hovered,
                        widget->clicked
                    )) widget->open = !widget->open;
                    if(widget->clicked) CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                    if(widget->clicked && widget->on_click) widget->on_click(*widget);
                    if(widget->open) {
                        UpdateElements(widget->content,false,disabled);
                        if(
                            !CSS_UI_POINTER_INPUT_CONSUMED &&
                            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                            !CheckCollisionPointRec(GetUIMousePosition(),widget->rect.transform) &&
                            !CheckCollisionPointRec(GetUIMousePosition(),widget->content.rect.transform)
                        ) {
                            widget->open = false;
                            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                        }
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                    if(widget) {
                        // If this is the popup that FindOpenNonModalPopup found this
                        // frame, the reset block already marked pointer input as
                        // globally consumed so nothing *else* can be interacted
                        // with underneath it. Lift that for the popup's own update
                        // call so its own content (and outside-click-to-close
                        // detection) still works, then restore it afterward so
                        // later siblings stay blocked.
                        const bool owns_popup = widget.get() == g_open_popup;
                        const bool saved_consumed = CSS_UI_POINTER_INPUT_CONSUMED;
                        if(owns_popup) CSS_UI_POINTER_INPUT_CONSUMED = false;
                        UpdatePopup(*widget,group_disabled);
                        if(owns_popup) CSS_UI_POINTER_INPUT_CONSUMED = saved_consumed;
                    }
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

                if(!CSS_UI_POINTER_INPUT_CONSUMED) {
                    if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
                        if(widget) UpdateElementDragDrop(widget->rect);
                    }
                    else {
                        UpdateElementDragDrop(widget.rect);
                    }
                }
            },
            element
        );
    }

    const bool continue_scrollbar_capture=
        interaction_clip.BlocksPointer() &&
        !interaction_clip.HadPointerCaptureBefore() &&
        (ui.dragging_horizontal_scrollbar || ui.dragging_vertical_scrollbar);
    if(!CSS_UI_POINTER_INPUT_CONSUMED || continue_scrollbar_capture) {
        UpdateContainerScroll(ui,group_disabled);
    }
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
    bool force_layout,
    bool detect_layout_mutations
) {
    const bool previous_mutation_detection =
        CSS_UI_DETECT_LAYOUT_MUTATIONS;
    CSS_UI_DETECT_LAYOUT_MUTATIONS = detect_layout_mutations;
    if(width < 0.0f) width = static_cast<float>(GetScreenWidth());
    if(height < 0.0f) height = static_cast<float>(GetScreenHeight());
    if(
        std::abs(CSS_UI_VIEWPORT_WIDTH-width) > 0.0001f ||
        std::abs(CSS_UI_VIEWPORT_HEIGHT-height) > 0.0001f
    ) {
        CSS_UI_VIEWPORT_WIDTH = width;
        CSS_UI_VIEWPORT_HEIGHT = height;
        // Media-query activation and viewport-based expressions can change at
        // a breakpoint even when the stylesheet itself did not change.
        ClearResolvedStyleCache();
        MarkLayoutDirty(root,true);
    }
    root.rect.transform.width=width;
    root.rect.transform.height=height;
    LayoutContainer(root,style_sheet,width,height,force_layout);
    UpdateElements(root,true,false);
    CSS_UI_DETECT_LAYOUT_MUTATIONS = previous_mutation_detection;
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
    const bool hovered = CheckCollisionPointRec(GetUIMousePosition(),rect.transform);
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
    rect.pseudo_states.SetKnown("open",expanded);
    rect.pseudo_states.SetKnown("expanded",expanded);
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
    tabs.rect.pseudo_states.SetKnown("open",true);
    tabs.rect.pseudo_states.SetKnown("selected",false);
    ComputedStyle tab_bar_style = ResolveInteractiveStyle(
        style_sheet,SelectorType::TabBar,tabs.style,false,false,false,
        tabs.disabled || tabs.rect.inherited_disabled,&tabs.rect,"body"
    );
    DrawStyledTextBox(tabs.rect.transform,"",tab_bar_style,TextAlign::Left);
    for(std::size_t index=0; index<tabs.tabs.size(); ++index) {
        const UITab& tab = tabs.tabs[index];
        const bool selected = static_cast<int>(index) == tabs.selected_index;
        tabs.rect.pseudo_states.SetKnown("selected",selected);
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
            DrawUIText(
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
    if(StyleSheetUsesPart(style_sheet,CSS_UI_SCROLLBAR_PART)) {
        AddCSSClass(bar_style.classes,CSS_UI_SCROLLBAR_PART);
        bar_style = ResolveStyleWithPseudoStates(
            style_sheet,
            SelectorType::Container,
            ui.style.id,
            ui.rect.pseudo_states.ActiveNamesView(),
            bar_style
        );
        bar_style = ResolveAnimatedStyle(
            ui.rect,style_sheet,std::move(bar_style),CSS_UI_SCROLLBAR_PART
        );
    }
    ComputedStyle thumb_style = ui.style;
    if(StyleSheetUsesPart(style_sheet,CSS_UI_SCROLLBAR_THUMB_PART)) {
        AddCSSClass(thumb_style.classes,CSS_UI_SCROLLBAR_THUMB_PART);
        thumb_style = ResolveStyleWithPseudoStates(
            style_sheet,
            SelectorType::Container,
            ui.style.id,
            ui.rect.pseudo_states.ActiveNamesView(),
            thumb_style
        );
        thumb_style = ResolveAnimatedStyle(
            ui.rect,style_sheet,std::move(thumb_style),CSS_UI_SCROLLBAR_THUMB_PART
        );
    }
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
        BeginUIScissorMode(rectangle);

        context.scissor_active = true;
        context.scissor = rectangle;
    }
}

void DrawContainerBox(UIContainer& ui, const StyleSheet& style_sheet) {
    if(ui.style.display == DisplayType::None) return;
    const bool hovered = CheckCollisionPointRec(GetUIMousePosition(),ui.rect.transform);
    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,SelectorType::Container,ui.style,
        false,hovered,hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        ui.disabled || ui.rect.inherited_disabled,ui.rect
    );

    const Color background = ApplyStyleOpacity(
        style.background_color,
        style.opacity
    );

    if(background.a > 0) {
        DrawRectangleRounded(
            ui.rect.transform,
            ResolveUIBorderRoundness(ui.rect.transform,style.border_radius),
            8,
            background
        );
    }

    const Color border = ApplyStyleOpacity(
        style.border_color,
        style.opacity
    );

    DrawStyleBorder(ui.rect.transform,style,border,8);
    DrawGeneratedPseudoElement(
        ui.rect,style,SelectorType::Container,style_sheet,UIGeneratedPseudoKind::Before
    );
}


Rectangle UnionPaintRect(Rectangle a,Rectangle b) {
    if(a.width<=0.0f || a.height<=0.0f) return b;
    if(b.width<=0.0f || b.height<=0.0f) return a;
    const float left=std::min(a.x,b.x);
    const float top=std::min(a.y,b.y);
    const float right=std::max(a.x+a.width,b.x+b.width);
    const float bottom=std::max(a.y+a.height,b.y+b.height);
    return {left,top,right-left,bottom-top};
}

Rectangle ContainerSubtreePaintBounds(const UIContainer& ui) {
    Rectangle bounds=ui.rect.transform;
    // This walk happens only for a container that actually requires
    // compositing; ordinary containers never pay for subtree-bound discovery.
    for(const auto& element:ui.elements) {
        std::visit([&](const auto& widget) {
            using T=std::decay_t<decltype(widget)>;
            if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                if(widget) bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(*widget));
            }
            else if constexpr(IsUICompositePointerV<T>) {
                if(!widget) return;
                bounds=UnionPaintRect(bounds,widget->rect.transform);
                if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(const UITab* tab=widget->SelectedTab()) {
                        bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(tab->content));
                    }
                }
                else if constexpr(!std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(widget->content));
                }
            }
            else if constexpr(IsUICustomElementPointerV<T>) {
                if(widget) bounds=UnionPaintRect(bounds,widget->rect.transform);
            }
            else {
                bounds=UnionPaintRect(bounds,widget.rect.transform);
            }
        },element);
    }
    return bounds;
}

bool TryDrawElementComposite(
    UIDrawContext& context,
    UIElementBase& owner,
    const ComputedStyle& style,
    Rectangle bounds,
    Rectangle clip,
    bool clip_active,
    const std::function<void()>& draw
) {
    if(!RequiresUICompositing(style)) return false;
    SetUIDrawScissor(context,false);
    const bool composited=DrawUICompositedLayer(owner,style,bounds,draw);
    SetUIDrawScissor(context,clip_active,clip);
    return composited;
}

void DrawElementsClippedInternal(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    Rectangle parent_clip,
    bool parent_clip_active,
    UIDrawContext& context,
    bool skip_current_composite
) {
    if(ui.style.display == DisplayType::None) {
        return;
    }

    // An element whose style requires compositing (transform/filter/etc.) can
    // end up visually repositioned or resized well outside its own untransformed
    // layout rect — that's the whole point of e.g. `transform: translate()`.
    // Testing overlap against the PRE-transform rect here would cull it before
    // its transform ever gets a chance to move it back into view, so such
    // elements skip this early cull and let the compositing path (which knows
    // the real, transformed bounds) make that call instead.
    const bool may_require_compositing =
        !skip_current_composite &&
        StyleMayUseCompositing(style_sheet,SelectorType::Container,ui.style);

    if(
        !may_require_compositing &&
        parent_clip_active &&
        (
            parent_clip.width <= 0.0f ||
            parent_clip.height <= 0.0f ||
            !RectanglesOverlap(ui.rect.transform,parent_clip)
        )
    ) {
        return;
    }

    if(!skip_current_composite && StyleMayUseCompositing(style_sheet,SelectorType::Container,ui.style)) {
        const bool hovered=CheckCollisionPointRec(GetUIMousePosition(),ui.rect.transform);
        const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
            style_sheet,SelectorType::Container,ui.style,
            false,hovered,hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            ui.disabled || ui.rect.inherited_disabled,ui.rect
        );
        if(RequiresUICompositing(effect_style)) {
            const Rectangle subtree_bounds=ContainerSubtreePaintBounds(ui);
            if(TryDrawElementComposite(
                context,ui.rect,effect_style,subtree_bounds,
                parent_clip,parent_clip_active,
                [&]() {
                    // Deliberately NOT parent_clip/parent_clip_active here.
                    // This capture renders the subtree, at its normal untransformed
                    // size, into an offscreen texture that the transform (scale/
                    // rotate/translate/skew) is applied to afterward as a whole.
                    // Scissoring the capture itself against the ambient ancestor
                    // clip clips content based on where the element sits BEFORE
                    // its own transform runs — which is exactly what was cutting
                    // off scaled/rotated/translated content that only ends up
                    // inside the visible area *after* the transform is applied.
                    // The ancestor clip still applies normally to every element
                    // that isn't itself being composited, via the recursive calls
                    // below this one.
                    DrawElementsClippedInternal(
                        ui,style_sheet,Rectangle{0.0f,0.0f,0.0f,0.0f},false,context,true
                    );
                }
            )) return;
        }
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

                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Button,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Button,widget.style,
                                widget.focused,widget.hovered,widget.active,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawButton(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawButton(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UIImage>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Image,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Image,widget.style,
                                widget.focused,widget.hovered,widget.pressed,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawImage(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawImage(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UISplitter>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Splitter,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Splitter,widget.style,
                                false,widget.hovered,widget.dragging,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawSplitter(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawSplitter(widget,style_sheet);
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

                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Label,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Label,widget.style,
                                false,false,false,widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawLabel(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawLabel(widget,style_sheet);
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

                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Input,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Input,widget.style,
                                widget.focused,widget.hovered,widget.active,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawInput(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawInput(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UISelectable>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        bool composited=false;
                        if(
                            StyleMayUseCompositing(style_sheet,SelectorType::Selectable,widget.style) ||
                            (widget.selected && style_sheet.compositing_classes.find("selected")!=style_sheet.compositing_classes.end())
                        ) {
                            ComputedStyle effect_base=widget.style;
                            if(widget.selected) AddCSSClass(effect_base.classes,"selected");
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Selectable,effect_base,
                                widget.focused,widget.hovered,widget.active,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawSelectable(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawSelectable(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UIComboBox>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::ComboBox,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::ComboBox,widget.style,
                                widget.focused,widget.hovered,widget.active,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawComboBoxBase(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawComboBoxBase(widget,style_sheet);
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
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::RangeInput,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::RangeInput,widget.style,
                                widget.focused,widget.hovered,widget.active_handle!=0,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawRangeInput(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawRangeInput(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(std::is_same_v<T,UICanvas>) {
                    if(
                        widget.style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget.rect.transform,child_clip))
                    ) {
                        SetUIDrawScissor(context,child_clip_active,child_clip);
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,SelectorType::Canvas,widget.style)) {
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,SelectorType::Canvas,widget.style,
                                widget.focused,widget.hovered,widget.dragging,
                                widget.disabled || widget.rect.inherited_disabled,widget.rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget.rect,effect_style,widget.rect.transform,
                                child_clip,child_clip_active,[&](){ DrawCanvas(widget,style_sheet); }
                            );
                        }
                        if(!composited) DrawCanvas(widget,style_sheet);
                        ConsiderTooltip(context,widget.rect,widget.style);
                    }
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(
                        widget &&
                        widget->style.display != DisplayType::None &&
                        (!child_clip_active || RectanglesOverlap(widget->rect.transform,child_clip))
                    ) {
                        // Custom components must participate in the same overflow clipping
                        // contract as retained containers. Previously their DrawContent()
                        // path inherited only the parent clip, so custom painting could
                        // escape the component even when `overflow: hidden/auto/scroll`.
                        Rectangle widget_clip=child_clip;
                        bool widget_clip_active=child_clip_active;
                        if(widget->style.overflow != OverflowType::Visible) {
                            widget_clip=widget_clip_active
                                ? IntersectRectangles(widget_clip,widget->rect.transform)
                                : widget->rect.transform;
                            widget_clip_active=true;
                        }

                        if(
                            widget_clip_active &&
                            (widget_clip.width <= 0.0f || widget_clip.height <= 0.0f)
                        ) {
                            return;
                        }

                        SetUIDrawScissor(context,widget_clip_active,widget_clip);
                        bool composited=false;
                        if(StyleMayUseCompositing(style_sheet,widget->Selector(),widget->style)) {
                            const bool hovered=CheckCollisionPointRec(GetUIMousePosition(),widget->rect.transform);
                            const ComputedStyle& effect_style=ResolveInteractiveStyleRef(
                                style_sheet,widget->Selector(),widget->style,
                                false,hovered,hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                widget->rect.inherited_disabled,widget->rect
                            );
                            composited=TryDrawElementComposite(
                                context,widget->rect,effect_style,widget->rect.transform,
                                widget_clip,widget_clip_active,[&](){ widget->Draw(style_sheet); }
                            );
                        }
                        if(!composited) widget->Draw(style_sheet);
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
                    widget->rect.pseudo_states.SetKnown("open",widget->open);
                    const ComputedStyle& owner_style=ResolveInteractiveStyleRef(
                        style_sheet,SelectorType::Collapsible,widget->style,
                        false,widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,widget->rect
                    );
                    auto draw_composite=[&]() {
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::Collapsible,style_sheet,UIGeneratedPseudoKind::Before
                        );
                        DrawCompositeHeader(
                            widget->rect,widget->style,SelectorType::Collapsible,
                            widget->style.id,widget->text,style_sheet,
                            widget->hovered,widget->clicked,
                            widget->disabled || widget->rect.inherited_disabled,
                            widget->open
                        );
                        if(widget->open) {
                            DrawElementsClippedInternal(
                                widget->content,style_sheet,child_clip,child_clip_active,context
                            );
                        }
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::Collapsible,style_sheet,UIGeneratedPseudoKind::After
                        );
                    };
                    Rectangle bounds=widget->rect.transform;
                    if(widget->open) bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(widget->content));
                    if(!TryDrawElementComposite(
                        context,widget->rect,owner_style,bounds,child_clip,child_clip_active,draw_composite
                    )) draw_composite();
                    ConsiderTooltip(context,widget->rect,widget->style);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    ComputedStyle style = widget->style;
                    if(widget->selected) AddCSSClass(style.classes,"selected");
                    widget->rect.pseudo_states.SetKnown("selected",widget->selected);
                    widget->rect.pseudo_states.SetKnown("open",widget->open);
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    const ComputedStyle& owner_style=ResolveInteractiveStyleRef(
                        style_sheet,SelectorType::TreeNode,style,
                        false,widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,widget->rect
                    );
                    auto draw_composite=[&]() {
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::TreeNode,style_sheet,UIGeneratedPseudoKind::Before
                        );
                        DrawCompositeHeader(
                            widget->rect,style,SelectorType::TreeNode,
                            widget->style.id,widget->text,style_sheet,
                            widget->hovered,widget->clicked,
                            widget->disabled || widget->rect.inherited_disabled,
                            widget->open
                        );
                        if(widget->open) {
                            DrawElementsClippedInternal(
                                widget->content,style_sheet,child_clip,child_clip_active,context
                            );
                        }
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::TreeNode,style_sheet,UIGeneratedPseudoKind::After
                        );
                    };
                    Rectangle bounds=widget->rect.transform;
                    if(widget->open) bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(widget->content));
                    if(!TryDrawElementComposite(
                        context,widget->rect,owner_style,bounds,child_clip,child_clip_active,draw_composite
                    )) draw_composite();
                    ConsiderTooltip(context,widget->rect,widget->style);
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                    if(!widget || widget->style.display == DisplayType::None) return;
                    SetUIDrawScissor(context,child_clip_active,child_clip);
                    widget->rect.pseudo_states.SetKnown("open",widget->open);
                    const ComputedStyle& owner_style=ResolveInteractiveStyleRef(
                        style_sheet,SelectorType::Menu,widget->style,
                        false,widget->hovered,widget->clicked,
                        widget->disabled || widget->rect.inherited_disabled,widget->rect
                    );
                    auto draw_menu=[&]() {
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::Menu,style_sheet,UIGeneratedPseudoKind::Before
                        );
                        DrawCompositeHeader(
                            widget->rect,widget->style,SelectorType::Menu,
                            widget->style.id,widget->text,style_sheet,
                            widget->hovered,widget->clicked,
                            widget->disabled || widget->rect.inherited_disabled,
                            widget->open,false
                        );
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::Menu,style_sheet,UIGeneratedPseudoKind::After
                        );
                    };
                    if(!TryDrawElementComposite(
                        context,widget->rect,owner_style,widget->rect.transform,
                        child_clip,child_clip_active,draw_menu
                    )) draw_menu();
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
                    const ComputedStyle& owner_style=ResolveInteractiveStyleRef(
                        style_sheet,SelectorType::TabBar,widget->style,
                        false,false,false,widget->disabled || widget->rect.inherited_disabled,widget->rect
                    );
                    UITab* selected=widget->SelectedTab();
                    auto draw_tabs=[&]() {
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::TabBar,style_sheet,UIGeneratedPseudoKind::Before
                        );
                        DrawTabBar(*widget,style_sheet);
                        if(selected) {
                            DrawElementsClippedInternal(
                                selected->content,style_sheet,child_clip,child_clip_active,context
                            );
                        }
                        DrawGeneratedPseudoElement(
                            widget->rect,owner_style,SelectorType::TabBar,style_sheet,UIGeneratedPseudoKind::After
                        );
                    };
                    Rectangle bounds=widget->rect.transform;
                    if(selected) bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(selected->content));
                    if(!TryDrawElementComposite(
                        context,widget->rect,owner_style,bounds,child_clip,child_clip_active,draw_tabs
                    )) draw_tabs();
                    ConsiderTooltip(context,widget->rect,widget->style);
                }
            },
            element
        );
    }

    SetUIDrawScissor(context,child_clip_active,child_clip);
    DrawContainerScrollbars(ui,style_sheet);

    const bool container_hovered = CheckCollisionPointRec(GetUIMousePosition(),ui.rect.transform);
    const ComputedStyle& container_style = ResolveInteractiveStyleRef(
        style_sheet,SelectorType::Container,ui.style,
        false,container_hovered,container_hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        ui.disabled || ui.rect.inherited_disabled,ui.rect
    );
    DrawGeneratedPseudoElement(
        ui.rect,container_style,SelectorType::Container,style_sheet,UIGeneratedPseudoKind::After
    );
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

        window->rect.pseudo_states.SetKnown("open",window->open);
        window->rect.pseudo_states.SetKnown("collapsed",window->collapsed);
        const bool hovered=CheckCollisionPointRec(GetUIMousePosition(),window->rect.transform);
        const ComputedStyle& window_style = ResolveInteractiveStyleRef(
            style_sheet,SelectorType::Window,window->style,
            false,hovered,window->dragging,
            window->disabled || window->rect.inherited_disabled,
            window->rect
        );

        auto draw_window=[&]() {
            DrawStyledBox(window->rect.transform,window_style);
            DrawGeneratedPseudoElement(
                window->rect,window_style,SelectorType::Window,
                style_sheet,UIGeneratedPseudoKind::Before
            );
            DrawCompositeHeader(
                window->rect,window->style,SelectorType::Window,
                window->style.id,window->title,style_sheet,
                false,window->dragging,
                window->disabled || window->rect.inherited_disabled,
                !window->collapsed,false
            );
            const Rectangle header = CompositeHeaderRectangle(window->rect,window_style);
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
            if(window_style.closable.value_or(true)) {
                DrawRectangleRounded(
                    close_button,0.2f,4,
                    CheckCollisionPointRec(GetUIMousePosition(),close_button)
                        ? DarkenColor(window_style.background_color,0.12f)
                        : BLANK
                );
                DrawText(
                    "x",
                    static_cast<int>(close_button.x+close_button.width*0.5f-4.0f),
                    static_cast<int>(close_button.y+3.0f),
                    static_cast<int>(std::max(12.0f,close_button.height-6.0f)),
                    window_style.text_color
                );
            }
            if(window_style.collapsible.value_or(true)) {
                DrawRectangleRounded(
                    collapse_button,0.2f,4,
                    CheckCollisionPointRec(GetUIMousePosition(),collapse_button)
                        ? DarkenColor(window_style.background_color,0.12f)
                        : BLANK
                );
                DrawText(
                    window->collapsed ? "+" : "-",
                    static_cast<int>(collapse_button.x+collapse_button.width*0.5f-4.0f),
                    static_cast<int>(collapse_button.y+2.0f),
                    static_cast<int>(std::max(12.0f,collapse_button.height-5.0f)),
                    window_style.text_color
                );
            }
            if(!window->collapsed) {
                DrawElementsClippedInternal(
                    window->content,style_sheet,screen_clip,true,context
                );
                if(window_style.resizable.value_or(true)) {
                    SetUIDrawScissor(context,false);
                    DrawRectangle(
                        static_cast<int>(window->rect.transform.x+window->rect.transform.width-10.0f),
                        static_cast<int>(window->rect.transform.y+window->rect.transform.height-10.0f),
                        8,8,window_style.border_color
                    );
                }
            }
            DrawGeneratedPseudoElement(
                window->rect,window_style,SelectorType::Window,
                style_sheet,UIGeneratedPseudoKind::After
            );
        };

        Rectangle bounds=window->rect.transform;
        if(!window->collapsed) {
            bounds=UnionPaintRect(bounds,ContainerSubtreePaintBounds(window->content));
        }
        if(!TryDrawElementComposite(
            context,window->rect,window_style,bounds,
            screen_clip,true,draw_window
        )) draw_window();
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
            const Rectangle visible=CurrentUIVisibleLogicalScreenRect();
            DrawRectangleRec(visible,Color{0,0,0,135});
        }

        popup->rect.pseudo_states.SetKnown("open",popup->open);
        const bool hovered=CheckCollisionPointRec(GetUIMousePosition(),popup->rect.transform);
        const ComputedStyle& popup_style = ResolveInteractiveStyleRef(
            style_sheet,SelectorType::Popup,popup->style,
            false,hovered,false,popup->rect.inherited_disabled,popup->rect
        );
        auto draw_popup=[&]() {
            DrawStyledBox(popup->rect.transform,popup_style);
            DrawGeneratedPseudoElement(
                popup->rect,popup_style,SelectorType::Popup,
                style_sheet,UIGeneratedPseudoKind::Before
            );
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
            DrawGeneratedPseudoElement(
                popup->rect,popup_style,SelectorType::Popup,
                style_sheet,UIGeneratedPseudoKind::After
            );
        };
        Rectangle bounds=UnionPaintRect(
            popup->rect.transform,ContainerSubtreePaintBounds(popup->content)
        );
        if(!TryDrawElementComposite(
            context,popup->rect,popup_style,bounds,
            screen_clip,true,draw_popup
        )) draw_popup();
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
    style.background_color = Color{255,255,225,255};
    style.border_color = Color{118,118,118,255};
    style.text_color = Color{0,0,0,255};
    style.border_width = 1.0f;
    style.border_radius = 0.0f;
    style.font_size = 13.3333f;
    style.padding = EdgeValues(3.0f);
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
        GetUIMousePosition().x+14.0f,
        GetUIMousePosition().y+18.0f,
        measured.x+style.padding.left+style.padding.right,
        measured.y+style.padding.top+style.padding.bottom
    };
    const Rectangle visible=CurrentUIVisibleLogicalScreenRect();
    rectangle.x = std::clamp(
        rectangle.x,visible.x,
        std::max(visible.x,visible.x+visible.width-rectangle.width)
    );
    rectangle.y = std::clamp(
        rectangle.y,visible.y,
        std::max(visible.y,visible.y+visible.height-rectangle.height)
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

    Rectangle screen_clip = CurrentUIVisibleLogicalScreenRect();
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

    Rectangle screen_clip = CurrentUIVisibleLogicalScreenRect();

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
