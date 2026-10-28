#define CSS_UI_INTERNAL_TEMPLATES
#include "css_emulator.h"
#undef CSS_UI_INTERNAL_TEMPLATES

void DrawRangeInput(UIRangeInput<float>& input,const StyleSheet& style_sheet) {
    DrawRangeInput<float>(input,style_sheet);
}

void DrawRangeInput(UIRangeInput<int>& input,const StyleSheet& style_sheet) {
    DrawRangeInput<int>(input,style_sheet);
}

void DrawInput(UIInput<float>& input,const StyleSheet& style_sheet) {
    DrawInput<float>(input,style_sheet);
}

void DrawInput(UIInput<int>& input,const StyleSheet& style_sheet) {
    DrawInput<int>(input,style_sheet);
}

void DrawLabel(UILabel& label,const StyleSheet& style_sheet) {
    const ComputedStyle& draw_style = ResolveInteractiveStyleRef(style_sheet,SelectorType::Label,label.style,
        false,false,false,label.rect.inherited_disabled,label.rect
    );
    Rectangle outer_rect =label.rect.transform;

    Rectangle content = {
        outer_rect.x + draw_style.padding.left,
        outer_rect.y + draw_style.padding.top,

        outer_rect.width -
            draw_style.padding.left -
            draw_style.padding.right,

        outer_rect.height -
            draw_style.padding.top -
            draw_style.padding.bottom
    };

    content.width =std::max(0.0f, content.width);

    content.height =std::max(0.0f, content.height);

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        label.rect,draw_style,SelectorType::Label,style_sheet,UIGeneratedPseudoKind::Before
    );

    Font font = ResolveUIFont(draw_style.font);
    Vector2 text_size = MeasureTextCached(
        font,
        label.text.c_str(),
        draw_style.font_size,
        1.0f
    );

    Vector2 text_position = {
        GetTextX(content,text_size.x,draw_style.text_align),

        content.y +
            (content.height - text_size.y) * 0.5f
    };

    DrawUIText(
        font,
        label.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );
    DrawGeneratedPseudoElement(
        label.rect,draw_style,SelectorType::Label,style_sheet,UIGeneratedPseudoKind::After
    );
}

void DrawButton(
    UIButton& button,
    const StyleSheet& style_sheet
) {
    const ComputedStyle& draw_style = ResolveInteractiveStyleRef(
        style_sheet,SelectorType::Button,button.style,
        button.focused,button.hovered,button.active,
        button.disabled || button.rect.inherited_disabled,button.rect
    );
    Rectangle outer_rect =
        button.rect.transform;

    Rectangle content = {
        outer_rect.x + draw_style.padding.left,
        outer_rect.y + draw_style.padding.top,

        outer_rect.width -
            draw_style.padding.left -
            draw_style.padding.right,

        outer_rect.height -
            draw_style.padding.top -
            draw_style.padding.bottom
    };

    content.width =
        std::max(0.0f, content.width);

    content.height =
        std::max(0.0f, content.height);

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        button.rect,draw_style,SelectorType::Button,style_sheet,UIGeneratedPseudoKind::Before
    );

    Font font = ResolveUIFont(draw_style.font);
    Vector2 text_size = MeasureTextCached(
        font,
        button.text.c_str(),
        draw_style.font_size,
        1.0f
    );

    Vector2 text_position = {
        GetTextX(content,text_size.x,draw_style.text_align),

        content.y +
            (content.height - text_size.y) * 0.5f
    };

    DrawUIText(
        font,
        button.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );
    DrawGeneratedPseudoElement(
        button.rect,draw_style,SelectorType::Button,style_sheet,UIGeneratedPseudoKind::After
    );
}

const ComputedStyle& ResolveInteractiveStyleRef(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& base,
    bool focused,
    bool hovered,
    bool active,
    bool disabled,
    UIElementBase& element,
    std::size_t overlay_hash
) {
    const bool needs_pseudo =
        StyleMayUseTargetPseudo(style_sheet,selector,base);
    std::size_t pseudo_hash = 0;
    const std::vector<std::string>* active_states = nullptr;

    if(needs_pseudo) {
        element.pseudo_states.SetKnown("focus",focused);
        element.pseudo_states.SetKnown("hover",hovered);
        element.pseudo_states.SetKnown("active",active);
        element.pseudo_states.SetKnown("disabled",disabled);
        element.pseudo_states.Update();
        active_states = &element.pseudo_states.ActiveNamesView();
        pseudo_hash = element.pseudo_states.ActiveHash();
    }

    const bool cache_hit =
        element.cached_interactive_style &&
        element.cached_interactive_style_generation == element.style_generation &&
        element.cached_interactive_sheet_revision == style_sheet.revision &&
        element.cached_interactive_pseudo_hash == pseudo_hash &&
        element.cached_interactive_overlay_hash == overlay_hash &&
        element.cached_interactive_selector == selector;

    if(!cache_hit) {
        const bool previously_composited =
            element.cached_interactive_style &&
            RequiresUICompositing(*element.cached_interactive_style);
        const std::size_t previous_pseudo_hash=
            element.cached_interactive_pseudo_hash;

        ComputedStyle resolved;
        if(needs_pseudo) {
            ComputedStyle working = base;
            resolved = ResolveStyleWithPseudoStates(
                style_sheet,selector,working.id,*active_states,std::move(working)
            );
        }
        else {
            resolved = base;
        }

        if(!element.cached_interactive_style) {
            element.cached_interactive_style =
                std::make_shared<ComputedStyle>(std::move(resolved));
        }
        else {
            *element.cached_interactive_style = std::move(resolved);
        }
        element.cached_interactive_style_generation = element.style_generation;
        element.cached_interactive_sheet_revision = style_sheet.revision;
        element.cached_interactive_pseudo_hash = pseudo_hash;
        element.cached_interactive_overlay_hash = overlay_hash;
        element.cached_interactive_selector = selector;
        element.dirty_flags |= UIDirtyFlags::Style|UIDirtyFlags::Paint;
        if(previous_pseudo_hash != pseudo_hash) {
            element.dirty_flags |= UIDirtyFlags::PseudoElements;
        }
        if(previously_composited || RequiresUICompositing(*element.cached_interactive_style)) {
            element.dirty_flags |= UIDirtyFlags::Composite;
            element.dirty_flags |= UIDirtyFlags::EffectParameters;
        }
        ++element.cached_interactive_style_revision;
        if(element.cached_interactive_style_revision == 0) {
            element.cached_interactive_style_revision = 1;
        }
    }

    return ResolveAnimatedStyleRef(
        element,
        style_sheet,
        *element.cached_interactive_style,
        {},
        element.cached_interactive_style_revision
    );
}

ComputedStyle ResolveInteractiveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& base,
    bool focused,
    bool hovered,
    bool active,
    bool disabled,
    UIElementBase* element,
    const std::string& animation_channel,
    std::size_t overlay_hash
) {
    if(element == nullptr) {
        ComputedStyle result = ResolveStyle(
            style_sheet,selector,base.id,SelectorState::None,base
        );
        if(focused) result = ResolveStyle(style_sheet,selector,base.id,SelectorState::Focus,result);
        if(hovered) result = ResolveStyle(style_sheet,selector,base.id,SelectorState::Hover,result);
        if(active) result = ResolveStyle(style_sheet,selector,base.id,SelectorState::Active,result);
        if(disabled) result = ResolveStyle(style_sheet,selector,base.id,SelectorState::Disabled,result);
        return result;
    }

    if(animation_channel.empty()) {
        return ResolveInteractiveStyleRef(
            style_sheet,selector,base,focused,hovered,active,disabled,
            *element,overlay_hash
        );
    }

    const bool needs_pseudo =
        StyleMayUseTargetPseudo(style_sheet,selector,base);
    ComputedStyle result = base;
    if(needs_pseudo) {
        element->pseudo_states.SetKnown("focus",focused);
        element->pseudo_states.SetKnown("hover",hovered);
        element->pseudo_states.SetKnown("active",active);
        element->pseudo_states.SetKnown("disabled",disabled);
        element->pseudo_states.Update();
        result = ResolveStyleWithPseudoStates(
            style_sheet,selector,result.id,
            element->pseudo_states.ActiveNamesView(),std::move(result)
        );
    }
    return ResolveAnimatedStyle(
        *element,style_sheet,std::move(result),animation_channel
    );
}

void DrawStyledBox(
    Rectangle rectangle,
    const ComputedStyle& style
) {
    DrawRectangleRounded(
        rectangle,
        ResolveUIBorderRoundness(rectangle,style.border_radius),
        8,
        ApplyStyleOpacity(style.background_color,style.opacity)
    );
    DrawStyleBorder(
        rectangle,style,
        ApplyStyleOpacity(style.border_color,style.opacity),8
    );
}

void DrawStyledTextContent(
    Rectangle rectangle,
    const std::string& text,
    const ComputedStyle& style,
    TextAlign alignment
) {
    if(text.empty()) return;
    Rectangle content = {
        rectangle.x+style.padding.left,
        rectangle.y+style.padding.top,
        std::max(0.0f,rectangle.width-style.padding.left-style.padding.right),
        std::max(0.0f,rectangle.height-style.padding.top-style.padding.bottom)
    };
    Font font = ResolveUIFont(style.font);
    Vector2 text_size = MeasureTextCached(font,text.c_str(),style.font_size,1.0f);
    DrawUIText(
        font,
        text.c_str(),
        {
            GetTextX(content,text_size.x,alignment),
            content.y+(content.height-text_size.y)*0.5f
        },
        style.font_size,
        1.0f,
        ApplyStyleOpacity(style.text_color,style.opacity)
    );
}

void DrawStyledTextBox(
    Rectangle rectangle,
    const std::string& text,
    const ComputedStyle& style,
    TextAlign alignment
) {
    DrawStyledBox(rectangle,style);
    DrawStyledTextContent(rectangle,text,style,alignment);
}

void DrawSelectable(
    UISelectable& selectable,
    const StyleSheet& style_sheet
) {
    selectable.rect.pseudo_states.SetKnown("selected",selectable.selected);
    ComputedStyle base = selectable.style;
    if(selectable.selected) AddCSSClass(base.classes,"selected");
    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::Selectable,
        base,
        selectable.focused,
        selectable.hovered,
        selectable.active,
        selectable.disabled || selectable.rect.inherited_disabled,
        selectable.rect
    );
    DrawStyledBox(selectable.rect.transform,style);
    DrawGeneratedPseudoElement(
        selectable.rect,style,SelectorType::Selectable,style_sheet,UIGeneratedPseudoKind::Before
    );
    DrawStyledTextContent(selectable.rect.transform,selectable.text,style,style.text_align);
    DrawGeneratedPseudoElement(
        selectable.rect,style,SelectorType::Selectable,style_sheet,UIGeneratedPseudoKind::After
    );
}

void DrawComboBoxBase(
    UIComboBox& combo,
    const StyleSheet& style_sheet
) {
    combo.rect.pseudo_states.SetKnown("open",combo.open);
    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::ComboBox,
        combo.style,
        combo.focused,
        combo.hovered,
        combo.active,
        combo.disabled || combo.rect.inherited_disabled,
        combo.rect
    );
    DrawStyledBox(combo.rect.transform,style);
    DrawGeneratedPseudoElement(
        combo.rect,style,SelectorType::ComboBox,style_sheet,UIGeneratedPseudoKind::Before
    );
    std::string text = combo.placeholder;
    if(combo.selected_index >= 0 && combo.selected_index < static_cast<int>(combo.items.size())) {
        text = combo.items[static_cast<std::size_t>(combo.selected_index)];
    }
    DrawStyledTextContent(combo.rect.transform,text,style,TextAlign::Left);
    const float visual_scale = EffectiveComputedStyleScale(style);
    const float center_x = combo.rect.transform.x+combo.rect.transform.width-16.0f*visual_scale;
    const float center_y = combo.rect.transform.y+combo.rect.transform.height*0.5f;
    if(combo.open) {
        DrawTriangle(
            {center_x+5.0f*visual_scale,center_y+3.0f*visual_scale},
            {center_x-5.0f*visual_scale,center_y+3.0f*visual_scale},
            {center_x,center_y-3.0f*visual_scale},
            style.text_color
        );
    }
    else {
        DrawTriangle(
            {center_x-5.0f*visual_scale,center_y-3.0f*visual_scale},
            {center_x+5.0f*visual_scale,center_y-3.0f*visual_scale},
            {center_x,center_y+3.0f*visual_scale},
            style.text_color
        );
    }
    DrawGeneratedPseudoElement(
        combo.rect,style,SelectorType::ComboBox,style_sheet,UIGeneratedPseudoKind::After
    );
}

void DrawComboBoxPopup(
    UIComboBox& combo,
    const StyleSheet& style_sheet
) {
    if(!combo.open) return;
    Rectangle popup = ComboPopupRectangle(combo);
    ComputedStyle popup_style = combo.style;
    if(StyleSheetUsesPart(style_sheet,CSS_UI_POPUP_PART)) {
        AddCSSClass(popup_style.classes,CSS_UI_POPUP_PART);
        popup_style = ResolveStyleWithPseudoStates(
            style_sheet,
            SelectorType::ComboBox,
            combo.style.id,
            combo.rect.pseudo_states.ActiveNamesView(),
            popup_style
        );
        popup_style = ResolveAnimatedStyle(
            combo.rect,style_sheet,std::move(popup_style),CSS_UI_POPUP_PART
        );
    }
    DrawStyledTextBox(popup,"",popup_style,TextAlign::Left);
    BeginUIScissorMode(popup);
    const int first = std::max(0,static_cast<int>(combo.scroll_offset/combo.style.item_height));
    const int last = std::min(
        static_cast<int>(combo.items.size()),
        first+static_cast<int>(std::ceil(popup.height/combo.style.item_height))+1
    );
    for(int index=first; index<last; ++index) {
        Rectangle item = {
            popup.x,
            popup.y+static_cast<float>(index)*combo.style.item_height-combo.scroll_offset,
            popup.width,
            combo.style.item_height
        };
        ComputedStyle item_style = combo.style;
        if(StyleSheetUsesPart(style_sheet,CSS_UI_ITEM_PART)) {
            AddCSSClass(item_style.classes,CSS_UI_ITEM_PART);
            AddCSSClass(item_style.classes,"option-"+std::to_string(index));
            if(static_cast<std::size_t>(index) < combo.item_classes.size()) {
                for(const std::string& item_class : combo.item_classes[static_cast<std::size_t>(index)]) {
                    AddCSSClass(item_style.classes,item_class);
                }
            }
            if(index == combo.selected_index) AddCSSClass(item_style.classes,"selected");
            std::vector<std::string> item_states = combo.rect.pseudo_states.ActiveNamesView();
            auto set_item_state = [&](const char* name, bool enabled) {
                const std::string normalized = NormalizeUIPseudoStateName(name);
                auto found = std::find(item_states.begin(),item_states.end(),normalized);
                if(enabled && found == item_states.end()) item_states.push_back(normalized);
                else if(!enabled && found != item_states.end()) item_states.erase(found);
            };
            set_item_state("selected",index == combo.selected_index);
            set_item_state("hover",index == combo.hovered_index);
            set_item_state("active",false);
            set_item_state("disabled",combo.disabled || combo.rect.inherited_disabled);
            item_style = ResolveStyleWithPseudoStates(
                style_sheet,
                SelectorType::ComboBox,
                combo.style.id,
                item_states,
                item_style
            );
            item_style = ResolveAnimatedStyle(
                combo.rect,
                style_sheet,
                std::move(item_style),
                std::string(CSS_UI_ITEM_PART)+":"+std::to_string(index)
            );
        }
        DrawStyledTextBox(item,combo.items[static_cast<std::size_t>(index)],item_style,TextAlign::Left);
    }
    EndScissorMode();
}

void DrawCanvas(
    UICanvas& canvas,
    const StyleSheet& style_sheet
) {
    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::Canvas,
        canvas.style,
        canvas.focused,
        canvas.hovered,
        canvas.dragging,
        canvas.disabled || canvas.rect.inherited_disabled,
        canvas.rect
    );
    DrawStyledBox(canvas.rect.transform,style);
    DrawGeneratedPseudoElement(
        canvas.rect,style,SelectorType::Canvas,style_sheet,UIGeneratedPseudoKind::Before
    );
    if(canvas.on_draw) canvas.on_draw(canvas,canvas.rect.transform);
    DrawGeneratedPseudoElement(
        canvas.rect,style,SelectorType::Canvas,style_sheet,UIGeneratedPseudoKind::After
    );
}

Color MultiplyUIColor(Color first, Color second) {
    return {
        static_cast<unsigned char>((static_cast<unsigned int>(first.r)*second.r)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.g)*second.g)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.b)*second.b)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.a)*second.a)/255u)
    };
}

namespace {

Rectangle FlipImageSource(Rectangle value, bool flip_x, bool flip_y) {
    if(flip_x) {
        value.x += value.width;
        value.width = -value.width;
    }
    if(flip_y) {
        value.y += value.height;
        value.height = -value.height;
    }
    return value;
}

Rectangle PixelSnapRectangle(Rectangle value) {
    value.x = std::round(value.x);
    value.y = std::round(value.y);
    value.width = std::round(value.width);
    value.height = std::round(value.height);
    return value;
}

void DrawImageQuad(
    Texture2D texture,
    Rectangle source,
    Rectangle destination,
    Vector2 pivot,
    float rotation,
    Color tint,
    bool flip_x,
    bool flip_y,
    bool pixel_snap
) {
    if(destination.width <= 0.0f || destination.height <= 0.0f) return;
    if(pixel_snap) destination = PixelSnapRectangle(destination);
    pivot.x = std::clamp(pivot.x,0.0f,1.0f);
    pivot.y = std::clamp(pivot.y,0.0f,1.0f);
    const Vector2 origin = {
        destination.width*pivot.x,
        destination.height*pivot.y
    };
    destination.x += origin.x;
    destination.y += origin.y;
    DrawTexturePro(
        texture,
        FlipImageSource(source,flip_x,flip_y),
        destination,
        origin,
        rotation,
        tint
    );
}

Rectangle IntersectImageRect(Rectangle first, Rectangle second) {
    const float left = std::max(first.x,second.x);
    const float top = std::max(first.y,second.y);
    const float right = std::min(first.x+first.width,second.x+second.width);
    const float bottom = std::min(first.y+first.height,second.y+second.height);
    return {
        left,top,
        std::max(0.0f,right-left),
        std::max(0.0f,bottom-top)
    };
}

void DrawRepeatedImageRegion(
    Texture2D texture,
    Rectangle source,
    Rectangle destination,
    bool repeat_x,
    bool repeat_y,
    float tile_scale,
    Vector2 tile_offset,
    Vector2 alignment,
    Color tint,
    bool flip_x,
    bool flip_y,
    bool pixel_snap
) {
    const float source_width = std::fabs(source.width);
    const float source_height = std::fabs(source.height);
    tile_scale = std::max(0.001f,tile_scale);
    const float tile_width = source_width*tile_scale;
    const float tile_height = source_height*tile_scale;
    if(tile_width <= 0.0f || tile_height <= 0.0f ||
       destination.width <= 0.0f || destination.height <= 0.0f) return;

    alignment.x = std::clamp(alignment.x,0.0f,1.0f);
    alignment.y = std::clamp(alignment.y,0.0f,1.0f);

    auto repeated_start = [](float edge, float offset, float tile_size) {
        float wrapped = std::fmod(offset,tile_size);
        if(wrapped > 0.0f) wrapped -= tile_size;
        return edge+wrapped;
    };

    const float start_x = repeat_x
        ? repeated_start(destination.x,tile_offset.x,tile_width)
        : destination.x+(destination.width-tile_width)*alignment.x+tile_offset.x;
    const float start_y = repeat_y
        ? repeated_start(destination.y,tile_offset.y,tile_height)
        : destination.y+(destination.height-tile_height)*alignment.y+tile_offset.y;
    const float end_x = repeat_x ? destination.x+destination.width+tile_width : start_x+tile_width;
    const float end_y = repeat_y ? destination.y+destination.height+tile_height : start_y+tile_height;

    for(float y=start_y; y<end_y; y+=tile_height) {
        for(float x=start_x; x<end_x; x+=tile_width) {
            const Rectangle tile = {x,y,tile_width,tile_height};
            Rectangle clipped = IntersectImageRect(tile,destination);
            if(clipped.width <= 0.0f || clipped.height <= 0.0f) continue;

            const float x0 = (clipped.x-tile.x)/tile.width;
            const float y0 = (clipped.y-tile.y)/tile.height;
            const float x1 = (clipped.x+clipped.width-tile.x)/tile.width;
            const float y1 = (clipped.y+clipped.height-tile.y)/tile.height;
            Rectangle tile_source = {
                source.x+source.width*x0,
                source.y+source.height*y0,
                source.width*(x1-x0),
                source.height*(y1-y0)
            };
            DrawImageQuad(
                texture,tile_source,clipped,{0.0f,0.0f},0.0f,tint,
                flip_x,flip_y,pixel_snap
            );
        }
    }
}

bool HasImageSlice(const EdgeValues& slice) {
    return slice.left > 0.0f || slice.right > 0.0f ||
           slice.top > 0.0f || slice.bottom > 0.0f;
}

void DrawNineSliceImage(
    Texture2D texture,
    Rectangle source,
    Rectangle destination,
    EdgeValues slice,
    float slice_scale,
    UIImageSliceMode mode,
    float tile_scale,
    Vector2 tile_offset,
    Color tint,
    bool flip_x,
    bool flip_y,
    bool pixel_snap
) {
    const float source_width = std::fabs(source.width);
    const float source_height = std::fabs(source.height);
    if(source_width <= 0.0f || source_height <= 0.0f ||
       destination.width <= 0.0f || destination.height <= 0.0f) return;

    slice.left = std::clamp(slice.left,0.0f,source_width);
    slice.right = std::clamp(slice.right,0.0f,source_width-slice.left);
    slice.top = std::clamp(slice.top,0.0f,source_height);
    slice.bottom = std::clamp(slice.bottom,0.0f,source_height-slice.top);
    slice_scale = std::max(0.001f,slice_scale);

    float destination_left = std::min(slice.left*slice_scale,destination.width);
    float destination_right = std::min(
        slice.right*slice_scale,
        std::max(0.0f,destination.width-destination_left)
    );
    float destination_top = std::min(slice.top*slice_scale,destination.height);
    float destination_bottom = std::min(
        slice.bottom*slice_scale,
        std::max(0.0f,destination.height-destination_top)
    );

    const float source_x[4] = {
        0.0f,slice.left,source_width-slice.right,source_width
    };
    const float source_y[4] = {
        0.0f,slice.top,source_height-slice.bottom,source_height
    };
    const float destination_x[4] = {
        destination.x,
        destination.x+destination_left,
        destination.x+destination.width-destination_right,
        destination.x+destination.width
    };
    const float destination_y[4] = {
        destination.y,
        destination.y+destination_top,
        destination.y+destination.height-destination_bottom,
        destination.y+destination.height
    };
    const float source_direction_x = source.width < 0.0f ? -1.0f : 1.0f;
    const float source_direction_y = source.height < 0.0f ? -1.0f : 1.0f;

    for(int row=0; row<3; ++row) {
        for(int column=0; column<3; ++column) {
            const float source_region_width = source_x[column+1]-source_x[column];
            const float source_region_height = source_y[row+1]-source_y[row];
            const Rectangle source_region = {
                source.x+source_x[column]*source_direction_x,
                source.y+source_y[row]*source_direction_y,
                source_region_width*source_direction_x,
                source_region_height*source_direction_y
            };
            const Rectangle destination_region = {
                destination_x[column],destination_y[row],
                destination_x[column+1]-destination_x[column],
                destination_y[row+1]-destination_y[row]
            };
            if(source_region_width <= 0.0f || source_region_height <= 0.0f ||
               destination_region.width <= 0.0f || destination_region.height <= 0.0f) continue;

            const bool center_column = column == 1;
            const bool center_row = row == 1;
            if(mode == UIImageSliceMode::Tile && (center_column || center_row)) {
                DrawRepeatedImageRegion(
                    texture,source_region,destination_region,
                    center_column,center_row,
                    tile_scale*slice_scale,tile_offset,{0.0f,0.0f},tint,
                    flip_x,flip_y,pixel_snap
                );
            }
            else {
                DrawImageQuad(
                    texture,source_region,destination_region,{0.0f,0.0f},0.0f,tint,
                    flip_x,flip_y,pixel_snap
                );
            }
        }
    }
}

int RaylibImageBlendMode(UIImageBlendMode mode) {
    switch(mode) {
        case UIImageBlendMode::Additive: return BLEND_ADDITIVE;
        case UIImageBlendMode::Multiplied: return BLEND_MULTIPLIED;
        case UIImageBlendMode::AddColors: return BLEND_ADD_COLORS;
        case UIImageBlendMode::SubtractColors: return BLEND_SUBTRACT_COLORS;
        case UIImageBlendMode::AlphaPremultiply: return BLEND_ALPHA_PREMULTIPLY;
        default: return BLEND_ALPHA;
    }
}

} // namespace

void DrawImage(
    UIImage& image,
    const StyleSheet& style_sheet
) {
    ComputedStyle base = image.style;
    base.image_fit = image.fit;
    base.image_sampling = image.sampling;
    base.image_flip_x = image.flip_x;
    base.image_flip_y = image.flip_y;
    base.image_position = image.position;
    base.image_rotation = image.rotation;
    base.image_pivot = image.pivot;
    base.image_frame = image.frame;
    base.image_frame_size = image.frame_size;
    base.image_frame_columns = image.frame_columns;
    base.image_slice = image.slice;
    base.image_slice_scale = image.slice_scale;
    base.image_slice_mode = image.slice_mode;
    base.image_tile_scale = image.tile_scale;
    base.image_tile_offset = image.tile_offset;
    base.image_blend_mode = image.blend_mode;
    base.image_pixel_snap = image.pixel_snap;

    // Draw-only image fields can change without a layout pass. Include a small
    // overlay fingerprint in the interactive-style cache key so retained image
    // updates stay correct without hashing the complete ComputedStyle.
    std::size_t image_overlay_hash = 0;
    auto hash_image_value = [&](std::size_t value) {
        HashCombine(image_overlay_hash,value);
    };
    auto hash_image_float = [&](float value) {
        hash_image_value(std::hash<float>{}(value));
    };
    hash_image_value(static_cast<std::size_t>(image.fit));
    hash_image_value(static_cast<std::size_t>(image.sampling));
    hash_image_value(static_cast<std::size_t>(image.flip_x));
    hash_image_value(static_cast<std::size_t>(image.flip_y));
    hash_image_float(image.position.x);
    hash_image_float(image.position.y);
    hash_image_float(image.rotation);
    hash_image_float(image.pivot.x);
    hash_image_float(image.pivot.y);
    hash_image_value(static_cast<std::size_t>(image.frame));
    hash_image_float(image.frame_size.x);
    hash_image_float(image.frame_size.y);
    hash_image_value(static_cast<std::size_t>(image.frame_columns));
    hash_image_float(image.slice.left);
    hash_image_float(image.slice.right);
    hash_image_float(image.slice.top);
    hash_image_float(image.slice.bottom);
    hash_image_float(image.slice_scale);
    hash_image_value(static_cast<std::size_t>(image.slice_mode));
    hash_image_float(image.tile_scale);
    hash_image_float(image.tile_offset.x);
    hash_image_float(image.tile_offset.y);
    hash_image_value(static_cast<std::size_t>(image.blend_mode));
    hash_image_value(static_cast<std::size_t>(image.pixel_snap));

    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::Image,
        base,
        image.focused,
        image.hovered,
        image.pressed,
        image.disabled || image.rect.inherited_disabled,
        image.rect,
        image_overlay_hash
    );
    DrawStyledBox(image.rect.transform,style);
    DrawGeneratedPseudoElement(
        image.rect,style,SelectorType::Image,style_sheet,UIGeneratedPseudoKind::Before
    );
    struct ImagePseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~ImagePseudoAfterScope() {
            DrawGeneratedPseudoElement(rect,style,SelectorType::Image,sheet,UIGeneratedPseudoKind::After);
        }
    } image_pseudo_after{image.rect,style,style_sheet};
    if(image.texture.id == 0 || image.texture.width <= 0 || image.texture.height <= 0) return;

    const UIImageFit resolved_fit = style.image_fit;
    const bool resolved_flip_x = style.image_flip_x;
    const bool resolved_flip_y = style.image_flip_y;
    const UIImageSampling resolved_sampling = style.image_sampling;
    if(
        image.sampling_texture_id != image.texture.id ||
        image.applied_sampling != resolved_sampling
    ) {
        SetTextureFilter(
            image.texture,
            resolved_sampling == UIImageSampling::Nearest
                ? TEXTURE_FILTER_POINT
                : TEXTURE_FILTER_BILINEAR
        );
        image.sampling_texture_id = image.texture.id;
        image.applied_sampling = resolved_sampling;
    }

    Rectangle source = image.use_source
        ? image.source
        : Rectangle{
            0.0f,0.0f,
            static_cast<float>(image.texture.width),
            static_cast<float>(image.texture.height)
        };
    const float base_source_width = std::fabs(source.width);
    const float base_source_height = std::fabs(source.height);
    if(style.image_frame_size.x > 0.0f && style.image_frame_size.y > 0.0f &&
       base_source_width > 0.0f && base_source_height > 0.0f) {
        const float frame_width = std::min(style.image_frame_size.x,base_source_width);
        const float frame_height = std::min(style.image_frame_size.y,base_source_height);
        const int automatic_columns = std::max(1,static_cast<int>(std::floor(base_source_width/frame_width)));
        const int columns = style.image_frame_columns > 0
            ? style.image_frame_columns
            : automatic_columns;
        const int frame = std::max(0,style.image_frame);
        const int column = frame%columns;
        const int row = frame/columns;
        const float direction_x = source.width < 0.0f ? -1.0f : 1.0f;
        const float direction_y = source.height < 0.0f ? -1.0f : 1.0f;
        source.x += static_cast<float>(column)*frame_width*direction_x;
        source.y += static_cast<float>(row)*frame_height*direction_y;
        source.width = frame_width*direction_x;
        source.height = frame_height*direction_y;
    }

    Rectangle content = {
        image.rect.transform.x+style.padding.left,
        image.rect.transform.y+style.padding.top,
        std::max(0.0f,image.rect.transform.width-style.padding.left-style.padding.right),
        std::max(0.0f,image.rect.transform.height-style.padding.top-style.padding.bottom)
    };
    if(content.width <= 0.0f || content.height <= 0.0f ||
       source.width == 0.0f || source.height == 0.0f) return;

    Vector2 alignment = {
        std::clamp(style.image_position.x,0.0f,1.0f),
        std::clamp(style.image_position.y,0.0f,1.0f)
    };
    Color tint = MultiplyUIColor(
        MultiplyUIColor(image.tint,style.image_tint),
        Color{255,255,255,static_cast<unsigned char>(std::clamp(style.opacity,0.0f,1.0f)*255.0f)}
    );

    const bool custom_blend = style.image_blend_mode != UIImageBlendMode::Alpha;
    if(custom_blend) BeginBlendMode(RaylibImageBlendMode(style.image_blend_mode));

    if(HasImageSlice(style.image_slice)) {
        DrawNineSliceImage(
            image.texture,source,content,style.image_slice,
            style.image_slice_scale,style.image_slice_mode,
            style.image_tile_scale,style.image_tile_offset,tint,
            resolved_flip_x,resolved_flip_y,style.image_pixel_snap
        );
        if(custom_blend) EndBlendMode();
        return;
    }

    const float source_width = std::fabs(source.width);
    const float source_height = std::fabs(source.height);
    if(resolved_fit == UIImageFit::Tile ||
       resolved_fit == UIImageFit::TileX ||
       resolved_fit == UIImageFit::TileY) {
        DrawRepeatedImageRegion(
            image.texture,source,content,
            resolved_fit != UIImageFit::TileY,
            resolved_fit != UIImageFit::TileX,
            style.image_tile_scale,style.image_tile_offset,alignment,tint,
            resolved_flip_x,resolved_flip_y,style.image_pixel_snap
        );
        if(custom_blend) EndBlendMode();
        return;
    }

    Rectangle destination = content;
    if(resolved_fit == UIImageFit::Contain || resolved_fit == UIImageFit::Center) {
        const float scale = resolved_fit == UIImageFit::Center
            ? std::min(1.0f,std::min(content.width/source_width,content.height/source_height))
            : std::min(content.width/source_width,content.height/source_height);
        destination.width = source_width*scale;
        destination.height = source_height*scale;
        destination.x += (content.width-destination.width)*alignment.x;
        destination.y += (content.height-destination.height)*alignment.y;
    }
    else if(resolved_fit == UIImageFit::Cover) {
        const float source_aspect = source_width/source_height;
        const float destination_aspect = content.width/content.height;
        const float direction_x = source.width < 0.0f ? -1.0f : 1.0f;
        const float direction_y = source.height < 0.0f ? -1.0f : 1.0f;
        if(source_aspect > destination_aspect) {
            const float cropped = source_height*destination_aspect;
            source.x += (source_width-cropped)*alignment.x*direction_x;
            source.width = cropped*direction_x;
        }
        else {
            const float cropped = source_width/destination_aspect;
            source.y += (source_height-cropped)*alignment.y*direction_y;
            source.height = cropped*direction_y;
        }
    }

    if(image.zoom != 1.0f || image.pan.x != 0.0f || image.pan.y != 0.0f) {
        const Vector2 center = {
            destination.x+destination.width*0.5f,
            destination.y+destination.height*0.5f
        };
        const float zoom = std::max(0.001f,image.zoom);
        destination.width *= zoom;
        destination.height *= zoom;
        destination.x = center.x-destination.width*0.5f+image.pan.x;
        destination.y = center.y-destination.height*0.5f+image.pan.y;
    }

    DrawImageQuad(
        image.texture,source,destination,style.image_pivot,
        style.image_rotation,tint,resolved_flip_x,resolved_flip_y,
        style.image_pixel_snap
    );
    if(custom_blend) EndBlendMode();
}

void DrawSplitter(
    UISplitter& splitter,
    const StyleSheet& style_sheet
) {
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::Splitter,
        splitter.style,
        false,
        splitter.hovered,
        splitter.dragging,
        splitter.disabled || splitter.rect.inherited_disabled,
        &splitter.rect
    );
    if(StyleSheetUsesPart(style_sheet,CSS_UI_SPLITTER_HANDLE_PART)) {
        AddCSSClass(style.classes,CSS_UI_SPLITTER_HANDLE_PART);
        style = ResolveStyleWithPseudoStates(
            style_sheet,
            SelectorType::Splitter,
            splitter.style.id,
            splitter.rect.pseudo_states.ActiveNamesView(),
            style
        );
        style = ResolveAnimatedStyle(
            splitter.rect,style_sheet,std::move(style),CSS_UI_SPLITTER_HANDLE_PART
        );
    }
    DrawStyledBox(splitter.rect.transform,style);
    DrawGeneratedPseudoElement(
        splitter.rect,style,SelectorType::Splitter,style_sheet,UIGeneratedPseudoKind::Before
    );
    DrawGeneratedPseudoElement(
        splitter.rect,style,SelectorType::Splitter,style_sheet,UIGeneratedPseudoKind::After
    );
}

void DrawInputTextArea(
    Rectangle text_area,
    const ComputedStyle& draw_style,
    bool focused
) {
    Color background = focused
        ? LightenColor(draw_style.background_color,0.05f)
        : DarkenColor(draw_style.background_color,0.14f);

    Color border = focused
        ? draw_style.text_color
        : draw_style.border_color;

    const float text_area_roundness = std::min(
        ResolveUIBorderRoundness(text_area,draw_style.border_radius),
        0.18f
    );

    DrawRectangleRounded(
        text_area,
        text_area_roundness,
        6,
        ApplyStyleOpacity(background,draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        text_area,
        text_area_roundness,
        6,
        std::max(
            EffectiveComputedStyleScale(draw_style),
            draw_style.border_width
        ),
        ApplyStyleOpacity(border,draw_style.opacity)
    );
}

void DrawInput(
    UIInput<std::string>& input,
    const StyleSheet& style_sheet
) {
    if(input.input_type == InputType::Trigger) {
        DrawTriggerInput(input,style_sheet);
        return;
    }

    const ComputedStyle& draw_style =
        ResolveInputDrawStyle(input,style_sheet);
    struct StringInputPseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~StringInputPseudoAfterScope() {
            DrawGeneratedPseudoElement(rect,style,SelectorType::Input,sheet,UIGeneratedPseudoKind::After);
        }
    } string_input_pseudo_after{input.rect,draw_style,style_sheet};

    Rectangle outer_rect = input.rect.transform;

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        input.rect,draw_style,SelectorType::Input,style_sheet,UIGeneratedPseudoKind::Before
    );

    Rectangle value_rect = GetInputValueRect(input);
    Rectangle text_area = outer_rect;

    if(input.show_value_box) {
        bool value_hovered = CheckCollisionPointRec(
            GetUIMousePosition(),
            value_rect
        );

        bool value_active =
            value_hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        ComputedStyle value_box_style =
            ResolveInputPartDrawStyle(
                input,
                style_sheet,
                CSS_INPUT_VALUE_BOX_PART,
                value_hovered,
                value_active,
                input.focused,
                draw_style
            );

        text_area = GetInputTextAreaRect(
            input,
            value_box_style
        );

        DrawInputTextArea(
            text_area,
            value_box_style,
            input.focused
        );
    }

    Rectangle content = {
        text_area.x + draw_style.padding.left,
        text_area.y + draw_style.padding.top,
        std::max(
            0.0f,
            text_area.width -
                draw_style.padding.left -
                draw_style.padding.right
        ),
        std::max(
            0.0f,
            text_area.height -
                draw_style.padding.top -
                draw_style.padding.bottom
        )
    };

    std::string value_text =
        input.focused
            ? input.edit_buffer
            : input.value;

    bool showing_placeholder =
        value_text.empty() &&
        !input.placeholder.empty() &&
        !input.focused;

    if(showing_placeholder) {
        value_text = input.placeholder;
    }

    Font font = ResolveUIFont(draw_style.font);
    Vector2 text_size = MeasureTextCached(
        font,
        value_text.c_str(),
        draw_style.font_size,
        1.0f
    );

    float text_x = GetTextX(
        content,
        text_size.x,
        draw_style.text_align
    );

    if(
        input.focused &&
        text_size.x > content.width
    ) {
        text_x =
            content.x+
            content.width-
            text_size.x-
            2.0f;
    }

    Vector2 text_position = {
        text_x,
        content.y+
            (content.height-text_size.y)*0.5f
    };

    Color text_color = ApplyStyleOpacity(
        draw_style.text_color,
        showing_placeholder
            ? draw_style.opacity*0.55f
            : draw_style.opacity
    );

    DrawUIInputTextSelectionAndCaret(
        input,
        font,
        value_text,
        text_position,
        draw_style.font_size,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity),
        input.focused && !showing_placeholder
    );

    DrawUIText(
        font,
        value_text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        text_color
    );

}

void DrawInput(
    UIInput<Vector2>& input,
    const StyleSheet& style_sheet
) {
    DrawVectorInput(input, style_sheet);
}

void DrawInput(
    UIInput<Vector3>& input,
    const StyleSheet& style_sheet
) {
    DrawVectorInput(input, style_sheet);
}

void DrawInput(
    UIInput<Vector4>& input,
    const StyleSheet& style_sheet
) {
    DrawVectorInput(input, style_sheet);
}

void DrawInput(
    UIInput<bool>& input,
    const StyleSheet& style_sheet
) {
    if(input.input_type == InputType::Trigger) {
        DrawTriggerInput(input,style_sheet);
        return;
    }

    input.rect.pseudo_states.SetKnown("checked",input.value);
    const ComputedStyle& draw_style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::Input,
        input.style,
        input.focused,
        input.hovered,
        input.active,
        input.disabled || input.rect.inherited_disabled,
        input.rect
    );
    struct BoolInputPseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~BoolInputPseudoAfterScope() {
            DrawGeneratedPseudoElement(rect,style,SelectorType::Input,sheet,UIGeneratedPseudoKind::After);
        }
    } bool_input_pseudo_after{input.rect,draw_style,style_sheet};

    Rectangle outer_rect = input.rect.transform;

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        input.rect,draw_style,SelectorType::Input,style_sheet,UIGeneratedPseudoKind::Before
    );

    float box_size =
        std::min(
            outer_rect.width,
            outer_rect.height
        ) * 0.5f;

    Rectangle check_box = {
        outer_rect.x + draw_style.padding.left,
        outer_rect.y +
            (
                outer_rect.height -
                box_size
            ) * 0.5f,
        box_size,
        box_size
    };

    DrawRectangleLinesEx(
        check_box,
        draw_style.border_width,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );

    if(input.value) {
        DrawRectangle(
            static_cast<int>(check_box.x + 4.0f),
            static_cast<int>(check_box.y + 4.0f),
            static_cast<int>(
                std::max(
                    0.0f,
                    check_box.width - 8.0f
                )
            ),
            static_cast<int>(
                std::max(
                    0.0f,
                    check_box.height - 8.0f
                )
            ),
            ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
        );
    }

    Font font = ResolveUIFont(draw_style.font);
    Vector2 text_size = MeasureTextCached(
        font,
        input.text.c_str(),
        draw_style.font_size,
        1.0f
    );

    Vector2 text_position = {
        check_box.x +
            check_box.width +
            draw_style.padding.left,
        outer_rect.y +
            (
                outer_rect.height -
                text_size.y
            ) * 0.5f
    };

    DrawUIText(
        font,
        input.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );
}

void DrawHorizontalDragArrows(
    Rectangle rect,
    Color color,
    float visual_scale
) {
    const float scale =
        std::max(0.25f,visual_scale);

    const float center_y =
        rect.y +
        rect.height * 0.5f;

    const float inset =
        std::min(
            rect.width * 0.18f,
            12.0f * scale
        );

    const float tip =
        std::min(
            rect.width * 0.08f,
            5.0f * scale
        );

    const float half_height =
        std::min(
            rect.height * 0.18f,
            5.0f * scale
        );

    DrawTriangle(
        {
            rect.x + inset - tip,
            center_y
        },
        {
            rect.x + inset + tip,
            center_y - half_height
        },
        {
            rect.x + inset + tip,
            center_y + half_height
        },
        color
    );

    DrawTriangle(
        {
            rect.x + rect.width - inset + tip,
            center_y
        },
        {
            rect.x + rect.width - inset - tip,
            center_y + half_height
        },
        {
            rect.x + rect.width - inset - tip,
            center_y - half_height
        },
        color
    );
}

void DrawCheckerboard(
    Rectangle rect,
    float cell_size ) {
    for(float y = 0.0f; y < rect.height; y += cell_size) {
        for(float x = 0.0f; x < rect.width; x += cell_size) {
            const int column =
                static_cast<int>(x/cell_size);
            const int row =
                static_cast<int>(y/cell_size);

            DrawRectangleRec(
                {
                    rect.x+x,
                    rect.y+y,
                    std::min(cell_size,rect.width-x),
                    std::min(cell_size,rect.height-y)
                },
                (
                    (column+row)%2==0
                )
                    ? Color{210,210,210,255}
                    : Color{150,150,150,255}
            );
        }
    }
}

std::string FormatColorHex(Color color) {
    return TextFormat(
        "#%02X%02X%02X%02X",
        static_cast<int>(color.r),
        static_cast<int>(color.g),
        static_cast<int>(color.b),
        static_cast<int>(color.a)
    );
}

void DrawInput(
    UIInput<Color>& input,
    const StyleSheet& style_sheet
) {
    if(input.input_type == InputType::Trigger) {
        DrawTriggerInput(input,style_sheet);
        return;
    }

    ComputedStyle draw_style =
        ResolveInputDrawStyle(
            input,
            style_sheet
        );

    Rectangle outer_rect =
        input.rect.transform;

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );

    Rectangle content = {
        outer_rect.x +
            draw_style.padding.left,
        outer_rect.y +
            draw_style.padding.top,
        std::max(
            0.0f,
            outer_rect.width -
                draw_style.padding.left -
                draw_style.padding.right
        ),
        std::max(
            0.0f,
            outer_rect.height -
                draw_style.padding.top -
                draw_style.padding.bottom
        )
    };

    if(input.input_type == InputType::Drag) {
        constexpr int component_count = 4;

        const float component_width =
            content.width /
            static_cast<float>(component_count);

        for(
            int index = 0;
            index < component_count;
            index++
        ) {
            Rectangle component_rect = {
                content.x +
                    component_width *
                    static_cast<float>(index),
                content.y,
                component_width,
                content.height
            };

            if(index > 0) {
                DrawLineV(
                    {
                        component_rect.x,
                        component_rect.y
                    },
                    {
                        component_rect.x,
                        component_rect.y +
                            component_rect.height
                    },
                    ApplyStyleOpacity(
                        draw_style.border_color,
                        draw_style.opacity
                    )
                );
            }

            const unsigned char* component =
                GetColorComponent(
                    input.value,
                    index
                );

            const bool text_editing =
                input.drag_text_editing &&
                input.focused &&
                input.active_component == index;
            const std::string value_text = text_editing
                ? input.component_buffers[index]
                : TextFormat(
                    "%d",
                    component != nullptr
                        ? static_cast<int>(*component)
                        : 0
                );

            const bool component_hovered =
                input.hovered &&
                CheckCollisionPointRec(GetUIMousePosition(),component_rect);
            const bool component_active =
                input.active_component == index &&
                (input.active || input.dragging || input.focused);

            DrawInputComponentText(
                input,
                style_sheet,
                index,
                component_rect,
                value_text,
                draw_style,
                component_hovered,
                component_active
            );

            if(
                input.focused &&
                input.active_component == index
            ) {
                DrawRectangleLinesEx(
                    component_rect,
                    std::max(
                        1.0f,
                        draw_style.border_width
                    ),
                    ApplyStyleOpacity(
                        draw_style.text_color,
                        draw_style.opacity * 0.75f
                    )
                );
            }

            if(!text_editing) {
                DrawHorizontalDragArrows(
                    component_rect,
                    ApplyStyleOpacity(
                        draw_style.text_color,
                        draw_style.opacity * 0.5f
                    ),
                    EffectiveComputedStyleScale(
                        draw_style
                    )
                );
            }
        }

        return;
    }

    const float swatch_width =
        std::min(
            58.0f,
            content.width * 0.35f
        );

    Rectangle swatch = {
        content.x,
        content.y,
        swatch_width,
        content.height
    };

    DrawCheckerboard(
        swatch,
        6.0f
    );

    DrawRectangleRec(
        swatch,
        ApplyStyleOpacity(
            input.value,
            draw_style.opacity
        )
    );

    DrawRectangleLinesEx(
        swatch,
        1.0f,
        ApplyStyleOpacity(
            draw_style.border_color,
            draw_style.opacity
        )
    );

    const std::string value_text =
        FormatColorHex(input.value);

    Font font =
        ResolveUIFont(draw_style.font);

    Vector2 text_size =
        MeasureTextCached(
            font,
            value_text.c_str(),
            draw_style.font_size,
            1.0f
        );

    Rectangle text_rect = {
        swatch.x +
            swatch.width +
            10.0f,
        content.y,
        std::max(
            0.0f,
            content.width -
                swatch.width -
                10.0f
        ),
        content.height
    };

    DrawUIText(
        font,
        value_text.c_str(),
        {
            std::round(
                text_rect.x +
                (
                    text_rect.width -
                    text_size.x
                ) * 0.5f
            ),
            std::round(
                text_rect.y +
                (
                    text_rect.height -
                    text_size.y
                ) * 0.5f
            )
        },
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(
            draw_style.text_color,
            draw_style.opacity
        )
    );
}

void DrawColorPickerPopup(UIInput<Color>& input, const StyleSheet& style_sheet) {
    if (input.input_type != InputType::ColorPicker ||
        !input.picker_open ||
        input.style.display == DisplayType::None) {
        return;
    }

    const ComputedStyle& draw_style = ResolveInputDrawStyle(input, style_sheet);
    const UIColorPickerGeometry geometry = GetColorPickerGeometry(input);

    // Popup background and border
    DrawRectangleRounded(
        geometry.popup,
        0.04f,
        8,
        ApplyStyleOpacity(draw_style.background_color, draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        geometry.popup,
        0.04f,
        8,
        std::max(1.0f, draw_style.border_width),
        ApplyStyleOpacity(draw_style.border_color, draw_style.opacity)
    );

    // Saturation runs left-to-right. Value runs top-to-bottom.
    const Color hue_color = ColorFromHSV01(input.picker_hue, 1.0f, 1.0f);

    DrawRectangleGradientH(
        static_cast<int>(std::round(geometry.saturation_value.x)),
        static_cast<int>(std::round(geometry.saturation_value.y)),
        static_cast<int>(std::round(geometry.saturation_value.width)),
        static_cast<int>(std::round(geometry.saturation_value.height)),
        WHITE,
        hue_color
    );

    DrawRectangleGradientV(
        static_cast<int>(std::round(geometry.saturation_value.x)),
        static_cast<int>(std::round(geometry.saturation_value.y)),
        static_cast<int>(std::round(geometry.saturation_value.width)),
        static_cast<int>(std::round(geometry.saturation_value.height)),
        BLANK,
        BLACK
    );

    // Six gradient segments cover the complete hue wheel.
    constexpr std::array<Color, 7> hue_colors = {
        Color{255, 0, 0, 255},
        Color{255, 255, 0, 255},
        Color{0, 255, 0, 255},
        Color{0, 255, 255, 255},
        Color{0, 0, 255, 255},
        Color{255, 0, 255, 255},
        Color{255, 0, 0, 255}
    };

    const float hue_segment_height = geometry.hue.height / 6.0f;

    for (int segment = 0; segment < 6; ++segment) {
        const int segment_y = static_cast<int>(
            std::round(geometry.hue.y + hue_segment_height * static_cast<float>(segment))
        );

        const int next_y = static_cast<int>(
            std::round(geometry.hue.y + hue_segment_height * static_cast<float>(segment + 1))
        );

        DrawRectangleGradientV(
            static_cast<int>(std::round(geometry.hue.x)),
            segment_y,
            static_cast<int>(std::round(geometry.hue.width)),
            std::max(1, next_y - segment_y),
            hue_colors[segment],
            hue_colors[segment + 1]
        );
    }

    // Alpha gradient
    DrawCheckerboard(geometry.alpha, 6.0f);

    Color transparent_color = input.value;
    transparent_color.a = 0;

    Color opaque_color = input.value;
    opaque_color.a = 255;

    DrawRectangleGradientH(
        static_cast<int>(std::round(geometry.alpha.x)),
        static_cast<int>(std::round(geometry.alpha.y)),
        static_cast<int>(std::round(geometry.alpha.width)),
        static_cast<int>(std::round(geometry.alpha.height)),
        transparent_color,
        opaque_color
    );

    // Saturation/value cursor
    const Vector2 sv_cursor = {
        geometry.saturation_value.x + geometry.saturation_value.width * input.picker_saturation,
        geometry.saturation_value.y + geometry.saturation_value.height * (1.0f - input.picker_brightness)
    };

    DrawCircleLines(
        static_cast<int>(std::round(sv_cursor.x)),
        static_cast<int>(std::round(sv_cursor.y)),
        6.0f,
        BLACK
    );

    DrawCircleLines(
        static_cast<int>(std::round(sv_cursor.x)),
        static_cast<int>(std::round(sv_cursor.y)),
        5.0f,
        WHITE
    );

    // Hue cursor
    const float hue_y = geometry.hue.y + geometry.hue.height * input.picker_hue;

    DrawRectangleLinesEx(
        {
            geometry.hue.x - 2.0f,
            hue_y - 2.0f,
            geometry.hue.width + 4.0f,
            4.0f
        },
        1.0f,
        WHITE
    );

    // Alpha cursor
    const float alpha_x = geometry.alpha.x + geometry.alpha.width * (static_cast<float>(input.value.a) / 255.0f);

    DrawRectangleLinesEx(
        {
            alpha_x - 2.0f,
            geometry.alpha.y - 2.0f,
            4.0f,
            geometry.alpha.height + 4.0f
        },
        1.0f,
        WHITE
    );

    // Preview swatch
    DrawCheckerboard(geometry.preview, 6.0f);
    DrawRectangleRec(geometry.preview, input.value);

    DrawRectangleLinesEx(
        geometry.preview,
        1.0f,
        ApplyStyleOpacity(draw_style.border_color, draw_style.opacity)
    );

    // Draw the editable hex input instead of static text.
    if (input.hex_input && input.hex_input->rect.transform.width > 0.0f) {
        DrawInput(*input.hex_input, style_sheet);
    }

    // Close button
    const bool close_hovered = CheckCollisionPointRec(
        GetUIMousePosition(),
        geometry.close_button
    );

    DrawRectangleRounded(
        geometry.close_button,
        0.2f,
        4,
        close_hovered
            ? DarkenColor(draw_style.border_color, 0.1f)
            : DarkenColor(draw_style.background_color, 0.12f)
    );

    DrawRectangleRoundedLinesEx(
        geometry.close_button,
        0.2f,
        4,
        1.0f,
        draw_style.border_color
    );

    const char* close_text = "X";
    Font close_font = ResolveUIFont(draw_style.font);
    Vector2 close_size = MeasureTextCached(close_font, close_text, 14.0f, 1.0f);

    DrawUIText(
        close_font,
        close_text,
        {
            std::round(geometry.close_button.x + (geometry.close_button.width - close_size.x) * 0.5f),
            std::round(geometry.close_button.y + (geometry.close_button.height - close_size.y) * 0.5f)
        },
        14.0f,
        1.0f,
        draw_style.text_color
    );
}