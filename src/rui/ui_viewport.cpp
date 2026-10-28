#include "css_emulator.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace {

float SafeViewportZoom(float zoom) {
    return std::max(0.0001f,zoom);
}

Vector2 ParentLogicalToScreen(const UIViewport& viewport,Vector2 point) {
    return {
        viewport.parent_transform.offset.x+point.x*viewport.parent_transform.scale,
        viewport.parent_transform.offset.y+point.y*viewport.parent_transform.scale
    };
}

Vector2 ScreenToParentLogical(const UIViewport& viewport,Vector2 point) {
    const float scale=std::max(0.0001f,viewport.parent_transform.scale);
    return {
        (point.x-viewport.parent_transform.offset.x)/scale,
        (point.y-viewport.parent_transform.offset.y)/scale
    };
}

UIViewportDrawEvent MakeViewportDrawEvent(const UIViewport& viewport) {
    UIViewportDrawEvent event;
    const Vector2 screen_origin=ParentLogicalToScreen(
        viewport,{viewport.rect.transform.x,viewport.rect.transform.y}
    );
    event.screen_bounds={
        screen_origin.x,screen_origin.y,
        viewport.rect.transform.width*viewport.parent_transform.scale,
        viewport.rect.transform.height*viewport.parent_transform.scale
    };
    event.visible_world_bounds=viewport.VisibleWorldRect();
    event.local_mouse=viewport.local_mouse;
    event.world_mouse=viewport.world_mouse;
    event.pan=viewport.camera.pan;
    event.zoom=viewport.camera.zoom;
    return event;
}

Rectangle ViewportContentClip(const UIViewport& viewport) {
    return viewport.clip
        ? viewport.VisibleWorldRect()
        : CurrentUIVisibleLogicalScreenRect();
}

void BeginViewportTransform(const UIViewport& viewport) {
    const Vector2 translation={
        viewport.rect.transform.x+viewport.camera.pan.x,
        viewport.rect.transform.y+viewport.camera.pan.y
    };
    const float zoom=SafeViewportZoom(viewport.camera.zoom);

    // Use rlgl's matrix stack rather than BeginMode2D. BeginMode2D resets the
    // model-view matrix, which prevents nested Viewports from composing.
    rlPushMatrix();
    rlTranslatef(translation.x,translation.y,0.0f);
    rlScalef(zoom,zoom,1.0f);
    PushUITransform(translation,zoom);
}

void EndViewportTransform() {
    PopUITransform();
    rlPopMatrix();
}


bool ViewportItemIdEquals(const CSS_Id& value,const std::string& id) {
    if(const auto* text=std::get_if<std::string>(&value)) return *text==id;
    if(const auto* number=std::get_if<int>(&value)) return std::to_string(*number)==id;
    return false;
}

Rectangle UnionRect(Rectangle a,Rectangle b) {
    const float left=std::min(a.x,b.x);
    const float top=std::min(a.y,b.y);
    const float right=std::max(a.x+a.width,b.x+b.width);
    const float bottom=std::max(a.y+a.height,b.y+b.height);
    return {left,top,std::max(0.0f,right-left),std::max(0.0f,bottom-top)};
}

} // namespace

void UIViewportItem::SetPosition(Vector2 value) {
    if(value.x==position.x && value.y==position.y) return;
    const Vector2 delta={value.x-position.x,value.y-position.y};
    position=value;
    if(!laid_out) return;

    rect.transform.x+=delta.x;
    rect.transform.y+=delta.y;
    TranslateContainer(content,delta);
}

Vector2 UIViewportItem::Measure(const StyleSheet& style_sheet,float available_width,float available_height) {
    content.rect.transform={0.0f,0.0f,std::max(1.0f,available_width),std::max(1.0f,available_height)};
    content.inherited_scale=1.0f;
    MarkLayoutDirty(content,true);
    LayoutContainer(content,style_sheet,available_width,available_height,true);
    Vector2 measured=content.content_size;
    if(measured.x<=0.0f) measured.x=std::max(1.0f,style.width);
    if(measured.y<=0.0f) measured.y=std::max(1.0f,style.height);
    return measured;
}

void UIViewportItem::Layout(const StyleSheet& style_sheet,Rectangle bounds) {
    layout_origin={bounds.x,bounds.y};
    rect.transform={
        layout_origin.x+position.x,
        layout_origin.y+position.y,
        bounds.width,
        bounds.height
    };
    laid_out=true;
    content.rect.transform=rect.transform;
    content.inherited_scale=1.0f;
    MarkLayoutDirty(content,false);
    LayoutContainer(content,style_sheet,rect.transform.width,rect.transform.height,true);
}

void UIViewportItem::Update(bool inherited_disabled) {
    if(sync_bindings) sync_bindings(*this);
    if(!visible) return;
    rect.inherited_disabled=inherited_disabled;
    if(!interactive) return;
    UpdateElements(content,false,disabled || inherited_disabled);
}

void UIViewportItem::Draw(const StyleSheet& style_sheet) {
    if(!visible) return;
    const bool hovered=interactive && CheckCollisionPointRec(GetUIMousePosition(),rect.transform);
    rect.pseudo_states.SetKnown("hover",hovered);
    rect.pseudo_states.SetKnown("disabled",disabled || rect.inherited_disabled);
    rect.pseudo_states.Update();
    ComputedStyle draw_style=ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::ViewportItem,style.id,
        rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style=ResolveAnimatedStyle(rect,style_sheet,std::move(draw_style),"viewport-item");
    DrawStyledBox(rect.transform,draw_style);
    DrawElementsClipped(content,style_sheet,CurrentUIVisibleLogicalScreenRect());
}

UIElementBase* UIViewportItem::FindNestedElement(const std::string& key) {
    return FindUIElementByKey(content,key);
}

const UIElementBase* UIViewportItem::FindNestedElement(const std::string& key) const {
    return FindUIElementByKey(content,key);
}

void UIViewportItem::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old=dynamic_cast<UIViewportItem*>(&previous);
    if(!old) return;
    if(options.preserve_runtime_layout) position=old->position;
    ReconcileUI(old->content,std::move(content),options);
    content=std::move(old->content);
}

Vector2 UIViewport::Measure(const StyleSheet&,float available_width,float available_height) {
    // The parent layout resolver applies explicit/min/max sizing.  For auto
    // dimensions a Viewport naturally fills the available slot like DockSpace.
    return {std::max(1.0f,available_width),std::max(1.0f,available_height)};
}

void UIViewport::Layout(const StyleSheet& style_sheet,Rectangle bounds) {
    rect.transform=bounds;
    // Content is laid out in stable world units. Camera zoom never changes the
    // containing size and therefore never triggers a flex/grid reflow.
    content.rect.transform={0.0f,0.0f,bounds.width,bounds.height};
    content.inherited_scale=1.0f;
    MarkLayoutDirty(content,false);
    LayoutContainer(content,style_sheet,bounds.width,bounds.height,true);
}

Vector2 UIViewport::WorldToLocal(Vector2 world) const {
    const float zoom=SafeViewportZoom(camera.zoom);
    return {camera.pan.x+world.x*zoom,camera.pan.y+world.y*zoom};
}

Vector2 UIViewport::LocalToWorld(Vector2 local) const {
    const float zoom=SafeViewportZoom(camera.zoom);
    return {(local.x-camera.pan.x)/zoom,(local.y-camera.pan.y)/zoom};
}

Vector2 UIViewport::WorldToScreen(Vector2 world) const {
    const Vector2 local=WorldToLocal(world);
    return ParentLogicalToScreen(
        *this,{rect.transform.x+local.x,rect.transform.y+local.y}
    );
}

Vector2 UIViewport::ScreenToWorld(Vector2 screen) const {
    const Vector2 parent=ScreenToParentLogical(*this,screen);
    return LocalToWorld({parent.x-rect.transform.x,parent.y-rect.transform.y});
}

Rectangle UIViewport::WorldToScreen(Rectangle world) const {
    const Vector2 position=WorldToScreen(Vector2{world.x,world.y});
    const float scale=parent_transform.scale*SafeViewportZoom(camera.zoom);
    return {position.x,position.y,world.width*scale,world.height*scale};
}

Rectangle UIViewport::ScreenToWorld(Rectangle screen) const {
    const Vector2 position=ScreenToWorld(Vector2{screen.x,screen.y});
    const float scale=std::max(0.0001f,parent_transform.scale*SafeViewportZoom(camera.zoom));
    return {position.x,position.y,screen.width/scale,screen.height/scale};
}

Rectangle UIViewport::VisibleWorldRect() const {
    const float zoom=SafeViewportZoom(camera.zoom);
    return {
        -camera.pan.x/zoom,
        -camera.pan.y/zoom,
        rect.transform.width/zoom,
        rect.transform.height/zoom
    };
}

void UIViewport::SetPan(Vector2 pan) {
    camera.pan=pan;
}

void UIViewport::PanBy(Vector2 delta) {
    camera.pan.x+=delta.x;
    camera.pan.y+=delta.y;
}

void UIViewport::SetZoom(float zoom) {
    camera.zoom=std::clamp(
        zoom,
        std::max(0.0001f,camera.min_zoom),
        std::max(camera.min_zoom,camera.max_zoom)
    );
}

void UIViewport::ZoomAt(Vector2 parent_position,float new_zoom) {
    const float old_zoom=SafeViewportZoom(camera.zoom);
    const Vector2 local={
        parent_position.x-rect.transform.x,
        parent_position.y-rect.transform.y
    };
    const Vector2 anchor={
        (local.x-camera.pan.x)/old_zoom,
        (local.y-camera.pan.y)/old_zoom
    };
    SetZoom(new_zoom);
    camera.pan={
        local.x-anchor.x*camera.zoom,
        local.y-anchor.y*camera.zoom
    };
}

void UIViewport::CenterOn(Vector2 world_position) {
    camera.pan={
        rect.transform.width*0.5f-world_position.x*camera.zoom,
        rect.transform.height*0.5f-world_position.y*camera.zoom
    };
}

void UIViewport::Frame(Rectangle world_bounds,float padding) {
    if(world_bounds.width<=0.0f || world_bounds.height<=0.0f) return;
    const float available_width=std::max(1.0f,rect.transform.width-padding*2.0f);
    const float available_height=std::max(1.0f,rect.transform.height-padding*2.0f);
    SetZoom(std::min(available_width/world_bounds.width,available_height/world_bounds.height));
    CenterOn({
        world_bounds.x+world_bounds.width*0.5f,
        world_bounds.y+world_bounds.height*0.5f
    });
}

void UIViewport::FrameAll(float padding) {
    bool found=false;
    Rectangle bounds{};
    for(UIElement& element:content.elements) {
        std::visit([&](auto& widget) {
            using T=std::decay_t<decltype(widget)>;
            if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return;
                if(auto* item=dynamic_cast<UIViewportItem*>(widget.get())) {
                    if(!item->visible) return;
                    if(!found) { bounds=item->rect.transform; found=true; }
                    else bounds=UnionRect(bounds,item->rect.transform);
                }
            }
        },element);
    }
    if(found) Frame(bounds,padding);
}

void UIViewport::ResetView() {
    camera.pan={0.0f,0.0f};
    SetZoom(1.0f);
}

void UIViewport::Update(bool inherited_disabled) {
    parent_transform=GetCurrentUITransform();
    if(sync_bindings) sync_bindings(*this);
    const bool blocked=disabled || inherited_disabled;
    rect.inherited_disabled=inherited_disabled;

    const Vector2 parent_mouse=GetUIMousePosition();
    const Vector2 parent_delta=GetUIMouseDelta();
    local_mouse={parent_mouse.x-rect.transform.x,parent_mouse.y-rect.transform.y};
    local_mouse_delta=parent_delta;
    world_mouse=LocalToWorld(local_mouse);
    world_mouse_delta={
        parent_delta.x/SafeViewportZoom(camera.zoom),
        parent_delta.y/SafeViewportZoom(camera.zoom)
    };
    wheel=GetMouseWheelMove();
    hovered=!blocked && CheckCollisionPointRec(parent_mouse,rect.transform);

    UIInputContext child_input;
    child_input.mouse_position=world_mouse;
    child_input.mouse_delta=world_mouse_delta;
    const Vector2 child_translation={
        rect.transform.x+camera.pan.x,
        rect.transform.y+camera.pan.y
    };
    PushUITransform(child_translation,SafeViewportZoom(camera.zoom));
    PushUIInputContext(child_input);
    // Root focus routing cannot compare screen-space mouse coordinates against
    // world-space child rectangles. Route focus again inside this transformed
    // input context so ordinary inputs/buttons keep their normal semantics.
    if(!blocked && !CSS_UI_POINTER_INPUT_CONSUMED && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        UpdatePointerFocus(content);
    }
    if(!blocked && focused && IsKeyPressed(KEY_TAB)) {
        AdvanceKeyboardFocus(content);
    }
    UpdateElements(content,false,blocked);
    PopUIInputContext();
    PopUITransform();

    bool z_order_dirty=false;
    for(UIElement& element:content.elements) {
        if(auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element)) {
            if(*custom) {
                if(auto* item=dynamic_cast<UIViewportItem*>(custom->get())) {
                    z_order_dirty=z_order_dirty || item->z_order_changed;
                }
            }
        }
    }
    if(z_order_dirty) SortItemsByZ();

    UIViewportCamera previous=camera;

    if(!blocked && pan_enabled) {
        if(hovered && IsMouseButtonPressed(pan_button) && !CSS_UI_POINTER_INPUT_CONSUMED) {
            panning=true;
            focused=focusable;
        }
        if(panning && IsMouseButtonDown(pan_button)) {
            PanBy(parent_delta);
        }
        if(IsMouseButtonReleased(pan_button)) panning=false;
    }
    else panning=false;

    if(
        !blocked && hovered && wheel_zoom && wheel!=0.0f &&
        !CSS_UI_SCROLL_INPUT_CONSUMED
    ) {
        const float factor=std::pow(1.0f+std::max(0.001f,zoom_speed),wheel);
        ZoomAt(parent_mouse,camera.zoom*factor);
        CSS_UI_SCROLL_INPUT_CONSUMED=true;
        focused=focusable;
    }

    local_mouse={parent_mouse.x-rect.transform.x,parent_mouse.y-rect.transform.y};
    world_mouse=LocalToWorld(local_mouse);

    rect.pseudo_states.SetKnown("hover",hovered);
    rect.pseudo_states.SetKnown("focus",focused);
    rect.pseudo_states.SetKnown("panning",panning);
    rect.pseudo_states.SetKnown("disabled",blocked);
    rect.pseudo_states.Update();

    const bool view_changed=
        previous.pan.x!=camera.pan.x || previous.pan.y!=camera.pan.y ||
        previous.zoom!=camera.zoom;
    if(view_changed && on_view_changed) on_view_changed(*this,previous);
    if(on_update) on_update(*this);
}

void UIViewport::Draw(const StyleSheet& style_sheet) {
    parent_transform=GetCurrentUITransform();
    rect.pseudo_states.SetKnown("hover",hovered);
    rect.pseudo_states.SetKnown("focus",focused);
    rect.pseudo_states.SetKnown("panning",panning);
    rect.pseudo_states.SetKnown("disabled",disabled || rect.inherited_disabled);
    rect.pseudo_states.Update();

    ComputedStyle draw_style=ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::Viewport,style.id,
        rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style=ResolveAnimatedStyle(rect,style_sheet,std::move(draw_style),"viewport");
    DrawStyledBox(rect.transform,draw_style);

    const UIViewportDrawEvent event=MakeViewportDrawEvent(*this);
    BeginViewportTransform(*this);
    if(on_background_draw) {
        if(clip) BeginUIScissorMode(ViewportContentClip(*this));
        on_background_draw(*this,event);
        if(clip) EndScissorMode();
    }
    DrawElementsClipped(content,style_sheet,ViewportContentClip(*this));
    if(on_foreground_draw) {
        if(clip) BeginUIScissorMode(ViewportContentClip(*this));
        on_foreground_draw(*this,event);
        if(clip) EndScissorMode();
    }
    EndViewportTransform();
}

UIElementBase* UIViewport::FindNestedElement(const std::string& key) {
    return FindUIElementByKey(content,key);
}

const UIElementBase* UIViewport::FindNestedElement(const std::string& key) const {
    return FindUIElementByKey(content,key);
}

void UIViewport::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old=dynamic_cast<UIViewport*>(&previous);
    if(!old) return;
    camera=old->camera;
    focused=old->focused;
    panning=old->panning;
    ReconcileUI(old->content,std::move(content),options);
    content=std::move(old->content);
}

UIViewportItem& UIViewport::AddItem(std::string id,Vector2 position) {
    auto item=std::make_shared<UIViewportItem>();
    item->position=position;
    item->style.position=PositionType::Absolute;
    if(!id.empty()) item->style.id=std::move(id);
    UIViewportItem& result=*item;
    content.elements.emplace_back(std::static_pointer_cast<IUICustomElement>(std::move(item)));
    content.layout_dirty=true;
    return result;
}

UIViewportItem* UIViewport::FindItem(const std::string& id) {
    for(UIElement& element:content.elements) {
        if(auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element)) {
            if(*custom) {
                if(auto* item=dynamic_cast<UIViewportItem*>(custom->get())) {
                    if(ViewportItemIdEquals(item->style.id,id)) return item;
                }
            }
        }
    }
    return nullptr;
}

const UIViewportItem* UIViewport::FindItem(const std::string& id) const {
    for(const UIElement& element:content.elements) {
        if(const auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element)) {
            if(*custom) {
                if(const auto* item=dynamic_cast<const UIViewportItem*>(custom->get())) {
                    if(ViewportItemIdEquals(item->style.id,id)) return item;
                }
            }
        }
    }
    return nullptr;
}

bool UIViewport::RemoveItem(const std::string& id) {
    const auto previous=content.elements.size();
    content.elements.erase(
        std::remove_if(content.elements.begin(),content.elements.end(),[&](const UIElement& element) {
            const auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element);
            if(!custom || !*custom) return false;
            const auto* item=dynamic_cast<const UIViewportItem*>(custom->get());
            return item && ViewportItemIdEquals(item->style.id,id);
        }),
        content.elements.end()
    );
    if(content.elements.size()==previous) return false;
    content.layout_dirty=true;
    return true;
}

void UIViewport::SortItemsByZ() {
    std::stable_sort(content.elements.begin(),content.elements.end(),[](const UIElement& a,const UIElement& b) {
        auto z=[](const UIElement& element) {
            const auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element);
            if(!custom || !*custom) return 0;
            const auto* item=dynamic_cast<const UIViewportItem*>(custom->get());
            return item ? item->z_index : 0;
        };
        return z(a)<z(b);
    });
    for(UIElement& element:content.elements) {
        if(auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element)) {
            if(*custom) {
                if(auto* item=dynamic_cast<UIViewportItem*>(custom->get())) {
                    item->z_order_changed=false;
                }
            }
        }
    }
}

bool UIViewport::BringToFront(const std::string& id) {
    UIViewportItem* target=FindItem(id);
    if(!target) return false;
    int maximum=target->z_index;
    for(const UIElement& element:content.elements) {
        if(const auto* custom=std::get_if<std::shared_ptr<IUICustomElement>>(&element)) {
            if(*custom) {
                if(const auto* item=dynamic_cast<const UIViewportItem*>(custom->get())) {
                    maximum=std::max(maximum,item->z_index);
                }
            }
        }
    }
    target->SetZIndex(maximum+1);
    SortItemsByZ();
    return true;
}
