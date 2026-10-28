#include "css_emulator.h"

std::optional<UIRuntimeTarget> FindRuntimeTarget(
    UIContainer& root,
    const std::string& key
) {
    std::optional<UIRuntimeTarget> result;
    VisitRuntimeElements(root,[&](UIRuntimeTarget& target) {
        if(target.key == key) result = target;
    });
    return result;
}

Rectangle RuntimeParentBounds(const UIRuntimeTarget& target) {
    if(target.parent == nullptr) {
        return {0.0f,0.0f,static_cast<float>(GetScreenWidth()),static_cast<float>(GetScreenHeight())};
    }
    const Rectangle parent = target.parent->rect.transform;
    const EdgeValues padding = target.parent->style.padding;
    return {
        parent.x+padding.left,
        parent.y+padding.top,
        std::max(0.0f,parent.width-padding.left-padding.right),
        std::max(0.0f,parent.height-padding.top-padding.bottom)
    };
}

bool RuntimeCanMove(const UIRuntimeTarget& target) {
    return RuntimeStyleFlag(target.style->runtime_movable,true);
}

bool RuntimeCanResize(const UIRuntimeTarget& target) {
    const bool fallback = target.selector != SelectorType::Label;
    return RuntimeStyleFlag(target.style->runtime_resizable,fallback);
}

bool UpdateRuntimeLayout(
    UIContainer& root,
    UIRuntimeLayer& layer
) {
    const bool toggle_modifier =
        !layer.toggle_requires_control ||
        IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL);
    if(toggle_modifier && IsKeyPressed(layer.toggle_key)) {
        layer.enabled = !layer.enabled;
        layer.interaction = UIRuntimeInteraction::None;
        layer.active_key.clear();
    }

    if(!layer.enabled) return false;

    if(IsKeyPressed(KEY_ESCAPE)) {
        layer.selected_key.clear();
        layer.active_key.clear();
        layer.interaction = UIRuntimeInteraction::None;
    }

    if(IsKeyPressed(KEY_R) && !layer.selected_key.empty()) {
        auto target = FindRuntimeTarget(root,layer.selected_key);
        if(target.has_value()) {
            target->rect->runtime_offset = {0.0f,0.0f};
            target->rect->runtime_size_override = false;
            layer.overrides.erase(target->key);
            layer.dirty = true;
            MarkLayoutDirty(root,true);
            if(layer.auto_save) SaveRuntimeLayout(layer);
            return true;
        }
    }

    const Vector2 mouse = GetUIMousePosition();
    const bool prefer_containers =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        std::optional<UIRuntimeTarget> picked;

        if(!layer.selected_key.empty()) {
            auto selected = FindRuntimeTarget(root,layer.selected_key);
            if(selected.has_value() && RuntimeCanResize(*selected)) {
                const Rectangle rect = selected->rect->transform;
                Rectangle handle = {
                    rect.x+rect.width-layer.resize_handle_size,
                    rect.y+rect.height-layer.resize_handle_size,
                    layer.resize_handle_size,
                    layer.resize_handle_size
                };
                if(CheckCollisionPointRec(mouse,handle)) picked = selected;
            }
        }

        if(!picked.has_value()) {
            VisitRuntimeElements(root,[&](UIRuntimeTarget& target) {
                if(!target.visible) return;
                if(target.style->display == DisplayType::None) return;
                if(prefer_containers && target.selector != SelectorType::Container) return;
                if(!prefer_containers && target.selector == SelectorType::Container) return;
                if(!CheckCollisionPointRec(mouse,target.rect->transform)) return;
                if(!picked.has_value() || target.depth >= picked->depth) picked = target;
            });

            // If the cursor is only over containers, still make the deepest one selectable.
            if(!picked.has_value() && !prefer_containers) {
                VisitRuntimeElements(root,[&](UIRuntimeTarget& target) {
                    if(!target.visible) return;
                    if(target.selector != SelectorType::Container) return;
                    if(target.style->display == DisplayType::None) return;
                    if(!CheckCollisionPointRec(mouse,target.rect->transform)) return;
                    if(!picked.has_value() || target.depth >= picked->depth) picked = target;
                });
            }
        }

        if(picked.has_value()) {
            layer.selected_key = picked->key;
            layer.active_key = picked->key;
            layer.drag_start_mouse = mouse;
            layer.drag_start_offset = picked->rect->runtime_offset;
            layer.drag_start_size = picked->rect->runtime_size_override
                ? picked->rect->runtime_size
                : Vector2{picked->rect->transform.width,picked->rect->transform.height};

            const Rectangle rect = picked->rect->transform;
            Rectangle handle = {
                rect.x+rect.width-layer.resize_handle_size,
                rect.y+rect.height-layer.resize_handle_size,
                layer.resize_handle_size,
                layer.resize_handle_size
            };

            if(RuntimeCanResize(*picked) && CheckCollisionPointRec(mouse,handle)) {
                layer.interaction = UIRuntimeInteraction::Resize;
            }
            else if(RuntimeCanMove(*picked)) {
                layer.interaction = UIRuntimeInteraction::Move;
            }
            else {
                layer.interaction = UIRuntimeInteraction::None;
            }
        }
        else {
            layer.selected_key.clear();
        }
    }

    bool changed = false;
    if(
        layer.interaction != UIRuntimeInteraction::None &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT)
    ) {
        auto target = FindRuntimeTarget(root,layer.active_key);
        if(target.has_value()) {
            const Vector2 delta = Vector2Subtract(mouse,layer.drag_start_mouse);
            const float snap = target->style->runtime_snap.has_value()
                ? *target->style->runtime_snap
                : layer.snap;
            auto snap_value = [&](float value) {
                return snap > 0.0f ? std::round(value/snap)*snap : value;
            };

            if(layer.interaction == UIRuntimeInteraction::Move) {
                Vector2 offset = {
                    snap_value(layer.drag_start_offset.x+delta.x),
                    snap_value(layer.drag_start_offset.y+delta.y)
                };
                if(RuntimeStyleFlag(target->style->runtime_lock_x,false)) offset.x = layer.drag_start_offset.x;
                if(RuntimeStyleFlag(target->style->runtime_lock_y,false)) offset.y = layer.drag_start_offset.y;

                if(RuntimeStyleFlag(target->style->runtime_constrain,layer.constrain_to_parent)) {
                    const Rectangle bounds = RuntimeParentBounds(*target);
                    const Rectangle base = target->rect->layout_transform;
                    offset.x = std::clamp(
                        offset.x,
                        bounds.x-base.x,
                        bounds.x+bounds.width-base.x-target->rect->transform.width
                    );
                    offset.y = std::clamp(
                        offset.y,
                        bounds.y-base.y,
                        bounds.y+bounds.height-base.y-target->rect->transform.height
                    );
                }
                target->rect->runtime_offset = offset;
            }
            else {
                Vector2 size = {
                    snap_value(layer.drag_start_size.x+delta.x),
                    snap_value(layer.drag_start_size.y+delta.y)
                };
                const float minimum_width = std::max(layer.minimum_width,target->style->min_width);
                const float minimum_height = std::max(layer.minimum_height,target->style->min_height);
                size.x = std::clamp(size.x,minimum_width,target->style->max_width);
                size.y = std::clamp(size.y,minimum_height,target->style->max_height);

                if(RuntimeStyleFlag(target->style->runtime_constrain,layer.constrain_to_parent)) {
                    const Rectangle bounds = RuntimeParentBounds(*target);
                    size.x = std::min(size.x,bounds.x+bounds.width-target->rect->transform.x);
                    size.y = std::min(size.y,bounds.y+bounds.height-target->rect->transform.y);
                }
                target->rect->runtime_size = size;
                target->rect->runtime_size_override = true;
            }

            layer.overrides[target->key] = {
                target->rect->runtime_offset,
                target->rect->runtime_size,
                target->rect->runtime_size_override
            };
            layer.dirty = true;
            changed = true;
            MarkLayoutDirty(root,true);
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        layer.interaction = UIRuntimeInteraction::None;
        layer.active_key.clear();
        if(layer.dirty && layer.auto_save) {
            SaveRuntimeLayout(layer);
            layer.dirty = false;
        }
    }

    if(layer.interaction == UIRuntimeInteraction::Move) {
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_POINTING_HAND);
    }
    else if(layer.interaction == UIRuntimeInteraction::Resize) {
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_RESIZE_NWSE);
    }
    return changed;
}

void DrawRuntimeLayoutOverlay(
    UIContainer& root,
    const UIRuntimeLayer& layer
) {
    if(!layer.enabled) return;

    DrawRectangle(8,8,410,25,Color{15,18,24,225});
    DrawText(
        layer.toggle_requires_control
            ? "LAYOUT EDIT: drag | Ctrl: containers | R: reset | Ctrl+F2: exit"
            : "LAYOUT EDIT: drag | Ctrl: containers | R: reset | F2: exit",
        14,14,12,Color{235,240,255,255}
    );

    if(layer.selected_key.empty()) return;
    std::optional<Rectangle> selected;
    VisitRuntimeElements(root,[&](UIRuntimeTarget& target) {
        if(target.visible && target.key == layer.selected_key) {
            selected = target.rect->transform;
        }
    });
    if(!selected.has_value()) return;

    DrawRectangleLinesEx(*selected,2.0f,Color{82,190,255,255});
    Rectangle handle = {
        selected->x+selected->width-layer.resize_handle_size,
        selected->y+selected->height-layer.resize_handle_size,
        layer.resize_handle_size,
        layer.resize_handle_size
    };
    DrawRectangleRec(handle,Color{82,190,255,255});
}

