#define CSS_UI_INTERNAL_TEMPLATES
#include "css_emulator.h"
#undef CSS_UI_INTERNAL_TEMPLATES

void DrawRangeInput(
    UIRangeInput<float>& input,
    const StyleSheet& style_sheet
) {
    DrawRangeInput<float>(input,style_sheet);
}

void DrawRangeInput(
    UIRangeInput<int>& input,
    const StyleSheet& style_sheet
) {
    DrawRangeInput<int>(input,style_sheet);
}

void DrawInput(
    UIInput<float>& input,
    const StyleSheet& style_sheet
) {
    DrawInput<float>(input,style_sheet);
}

void DrawInput(
    UIInput<int>& input,
    const StyleSheet& style_sheet
) {
    DrawInput<int>(input,style_sheet);
}

void DrawLabel(UILabel& label,const StyleSheet& style_sheet) {
    ComputedStyle draw_style = ResolveInteractiveStyle(
        style_sheet,SelectorType::Label,label.style,
        false,false,false,label.rect.inherited_disabled,&label.rect
    );

    Rectangle outer_rect =
        label.rect.transform;

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
        draw_style.border_radius,
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        outer_rect,
        draw_style.border_radius,
        8,
        draw_style.border_width,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity)
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

    DrawTextEx(
        font,
        label.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );
}

void DrawButton(
    UIButton& button,
    const StyleSheet& style_sheet
) {
    ComputedStyle draw_style = ResolveInteractiveStyle(
        style_sheet,SelectorType::Button,button.style,
        button.focused,button.hovered,button.active,
        button.disabled || button.rect.inherited_disabled,&button.rect
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
        draw_style.border_radius,
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        outer_rect,
        draw_style.border_radius,
        8,
        draw_style.border_width,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity)
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

    DrawTextEx(
        font,
        button.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
    );
}

ComputedStyle ResolveInteractiveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector,
    ComputedStyle base,
    bool focused,
    bool hovered,
    bool active,
    bool disabled,
    UIElementBase* element,
    const std::string& animation_channel
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

    element->pseudo_states.Set("focus",focused);
    element->pseudo_states.Set("hover",hovered);
    element->pseudo_states.Set("active",active);
    element->pseudo_states.Set("disabled",disabled);
    // Every pseudo-state consists of its current boolean and an optional
    // function that may alter that boolean. Functions run after engine values.
    element->pseudo_states.Update();
    ComputedStyle result = ResolveStyleWithPseudoStates(
        style_sheet,selector,base.id,element->pseudo_states.ActiveNames(),base
    );
    return ResolveAnimatedStyle(*element,style_sheet,std::move(result),animation_channel);
}

void DrawStyledTextBox(
    Rectangle rectangle,
    const std::string& text,
    const ComputedStyle& style,
    TextAlign alignment
) {
    DrawRectangleRounded(
        rectangle,
        style.border_radius,
        8,
        ApplyStyleOpacity(style.background_color,style.opacity)
    );
    if(style.border_width > 0.0f) {
        DrawRectangleRoundedLinesEx(
            rectangle,
            style.border_radius,
            8,
            style.border_width,
            ApplyStyleOpacity(style.border_color,style.opacity)
        );
    }
    Rectangle content = {
        rectangle.x+style.padding.left,
        rectangle.y+style.padding.top,
        std::max(0.0f,rectangle.width-style.padding.left-style.padding.right),
        std::max(0.0f,rectangle.height-style.padding.top-style.padding.bottom)
    };
    Font font = ResolveUIFont(style.font);
    Vector2 text_size = MeasureTextCached(font,text.c_str(),style.font_size,1.0f);
    DrawTextEx(
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

void DrawSelectable(
    UISelectable& selectable,
    const StyleSheet& style_sheet
) {
    selectable.rect.pseudo_states.Set("selected",selectable.selected);
    ComputedStyle base = selectable.style;
    if(selectable.selected) AddCSSClass(base.classes,"selected");
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::Selectable,
        base,
        selectable.focused,
        selectable.hovered,
        selectable.active,
        selectable.disabled || selectable.rect.inherited_disabled,
        &selectable.rect
    );
    DrawStyledTextBox(selectable.rect.transform,selectable.text,style,style.text_align);
}

void DrawComboBoxBase(
    UIComboBox& combo,
    const StyleSheet& style_sheet
) {
    combo.rect.pseudo_states.Set("open",combo.open);
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::ComboBox,
        combo.style,
        combo.focused,
        combo.hovered,
        combo.active,
        combo.disabled || combo.rect.inherited_disabled,
        &combo.rect
    );
    std::string text = combo.placeholder;
    if(combo.selected_index >= 0 && combo.selected_index < static_cast<int>(combo.items.size())) {
        text = combo.items[static_cast<std::size_t>(combo.selected_index)];
    }
    DrawStyledTextBox(combo.rect.transform,text,style,TextAlign::Left);
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
}

void DrawComboBoxPopup(
    UIComboBox& combo,
    const StyleSheet& style_sheet
) {
    if(!combo.open) return;
    Rectangle popup = ComboPopupRectangle(combo);
    ComputedStyle popup_style = combo.style;
    AddCSSClass(popup_style.classes,CSS_UI_POPUP_PART);
    popup_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::ComboBox,
        combo.style.id,
        combo.rect.pseudo_states.ActiveNames(),
        popup_style
    );
    popup_style = ResolveAnimatedStyle(
        combo.rect,style_sheet,std::move(popup_style),CSS_UI_POPUP_PART
    );
    DrawStyledTextBox(popup,"",popup_style,TextAlign::Left);
    BeginScissorMode(
        static_cast<int>(popup.x),
        static_cast<int>(popup.y),
        static_cast<int>(popup.width),
        static_cast<int>(popup.height)
    );
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
        AddCSSClass(item_style.classes,CSS_UI_ITEM_PART);
        if(index == combo.selected_index) AddCSSClass(item_style.classes,"selected");
        std::vector<std::string> item_states = combo.rect.pseudo_states.ActiveNames();
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
        DrawStyledTextBox(item,combo.items[static_cast<std::size_t>(index)],item_style,TextAlign::Left);
    }
    EndScissorMode();
}

void DrawCanvas(
    UICanvas& canvas,
    const StyleSheet& style_sheet
) {
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::Canvas,
        canvas.style,
        canvas.focused,
        canvas.hovered,
        canvas.dragging,
        canvas.disabled || canvas.rect.inherited_disabled,
        &canvas.rect
    );
    DrawStyledTextBox(canvas.rect.transform,"",style,TextAlign::Left);
    if(canvas.on_draw) canvas.on_draw(canvas,canvas.rect.transform);
}

Color MultiplyUIColor(Color first, Color second) {
    return {
        static_cast<unsigned char>((static_cast<unsigned int>(first.r)*second.r)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.g)*second.g)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.b)*second.b)/255u),
        static_cast<unsigned char>((static_cast<unsigned int>(first.a)*second.a)/255u)
    };
}

void DrawImage(
    UIImage& image,
    const StyleSheet& style_sheet
) {
    ComputedStyle base = image.style;
    base.image_fit = image.fit;
    base.image_sampling = image.sampling;
    base.image_flip_x = image.flip_x;
    base.image_flip_y = image.flip_y;
    ComputedStyle style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::Image,
        base,
        image.focused,
        image.hovered,
        image.pressed,
        image.disabled || image.rect.inherited_disabled,
        &image.rect
    );
    DrawStyledTextBox(image.rect.transform,"",style,TextAlign::Left);
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
    Rectangle content = {
        image.rect.transform.x+style.padding.left,
        image.rect.transform.y+style.padding.top,
        std::max(0.0f,image.rect.transform.width-style.padding.left-style.padding.right),
        std::max(0.0f,image.rect.transform.height-style.padding.top-style.padding.bottom)
    };
    if(content.width <= 0.0f || content.height <= 0.0f || source.width == 0.0f || source.height == 0.0f) return;

    const float source_width = std::fabs(source.width);
    const float source_height = std::fabs(source.height);
    Rectangle destination = content;
    if(resolved_fit == UIImageFit::Contain || resolved_fit == UIImageFit::Center) {
        float scale = resolved_fit == UIImageFit::Center
            ? std::min(1.0f,std::min(content.width/source_width,content.height/source_height))
            : std::min(content.width/source_width,content.height/source_height);
        destination.width = source_width*scale;
        destination.height = source_height*scale;
        destination.x += (content.width-destination.width)*0.5f;
        destination.y += (content.height-destination.height)*0.5f;
    }
    else if(resolved_fit == UIImageFit::Cover) {
        const float source_aspect = source_width/source_height;
        const float destination_aspect = content.width/content.height;
        if(source_aspect > destination_aspect) {
            const float cropped = source_height*destination_aspect;
            source.x += (source_width-cropped)*0.5f;
            source.width = cropped;
        }
        else {
            const float cropped = source_width/destination_aspect;
            source.y += (source_height-cropped)*0.5f;
            source.height = cropped;
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

    Color tint = MultiplyUIColor(
        MultiplyUIColor(image.tint,style.image_tint),
        Color{255,255,255,static_cast<unsigned char>(std::clamp(style.opacity,0.0f,1.0f)*255.0f)}
    );

    auto flipped_source = [&](Rectangle value) {
        if(resolved_flip_x) {
            value.x += value.width;
            value.width = -value.width;
        }
        if(resolved_flip_y) {
            value.y += value.height;
            value.height = -value.height;
        }
        return value;
    };

    if(resolved_fit != UIImageFit::Tile) {
        DrawTexturePro(image.texture,flipped_source(source),destination,{0.0f,0.0f},0.0f,tint);
        return;
    }

    for(float y=content.y; y<content.y+content.height; y+=source_height) {
        for(float x=content.x; x<content.x+content.width; x+=source_width) {
            const float width = std::min(source_width,content.x+content.width-x);
            const float height = std::min(source_height,content.y+content.height-y);
            Rectangle tile_source = source;
            tile_source.width = width;
            tile_source.height = height;
            DrawTexturePro(
                image.texture,
                flipped_source(tile_source),
                {x,y,width,height},
                {0.0f,0.0f},0.0f,tint
            );
        }
    }
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
    AddCSSClass(style.classes,CSS_UI_SPLITTER_HANDLE_PART);
    style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Splitter,
        splitter.style.id,
        splitter.rect.pseudo_states.ActiveNames(),
        style
    );
    style = ResolveAnimatedStyle(
        splitter.rect,style_sheet,std::move(style),CSS_UI_SPLITTER_HANDLE_PART
    );
    DrawStyledTextBox(splitter.rect.transform,"",style,TextAlign::Center);
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

    DrawRectangleRounded(
        text_area,
        std::min(draw_style.border_radius,0.18f),
        6,
        ApplyStyleOpacity(background,draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        text_area,
        std::min(draw_style.border_radius,0.18f),
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

    ComputedStyle draw_style =
        ResolveInputDrawStyle(input,style_sheet);

    Rectangle outer_rect = input.rect.transform;

    DrawRectangleRounded(
        outer_rect,
        draw_style.border_radius,
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawRectangleRoundedLinesEx(
        outer_rect,
        draw_style.border_radius,
        8,
        draw_style.border_width,
        ApplyStyleOpacity(
            draw_style.border_color,
            draw_style.opacity
        )
    );

    Rectangle value_rect = GetInputValueRect(input);
    Rectangle text_area = outer_rect;

    if(input.show_value_box) {
        bool value_hovered = CheckCollisionPointRec(
            GetMousePosition(),
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

    DrawTextEx(
        font,
        value_text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        text_color
    );

    if(
        input.focused &&
        ((input.caret_frame_counter/20)%2)==0 &&
        input.edit_buffer.size() <
            input.max_characters
    ) {
        DrawTextEx(
            font,
            "_",
            Vector2{
                text_position.x+text_size.x+2.0f,
                text_position.y
            },
            draw_style.font_size,
            1.0f,
            ApplyStyleOpacity(
                draw_style.text_color,
                draw_style.opacity
            )
        );
    }
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

    input.rect.pseudo_states.Set("checked",input.value);
    ComputedStyle draw_style = ResolveInteractiveStyle(
        style_sheet,
        SelectorType::Input,
        input.style,
        input.focused,
        input.hovered,
        input.active,
        input.disabled || input.rect.inherited_disabled,
        &input.rect
    );

    Rectangle outer_rect = input.rect.transform;

    DrawRectangleRounded(
        outer_rect,
        draw_style.border_radius,
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawRectangleRoundedLinesEx(
        outer_rect,
        draw_style.border_radius,
        8,
        draw_style.border_width,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity)
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

    DrawTextEx(
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
        draw_style.border_radius,
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawRectangleRoundedLinesEx(
        outer_rect,
        draw_style.border_radius,
        8,
        draw_style.border_width,
        ApplyStyleOpacity(
            draw_style.border_color,
            draw_style.opacity
        )
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

        const char* labels[component_count] = {
            "R",
            "G",
            "B",
            "A"
        };

        const float component_width =
            content.width /
            static_cast<float>(component_count);

        Font font =
            ResolveUIFont(draw_style.font);

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

            const std::string value_text =
                TextFormat(
                    "%s %d",
                    labels[index],
                    component != nullptr
                        ? static_cast<int>(*component)
                        : 0
                );

            Vector2 text_size =
                MeasureTextCached(
                    font,
                    value_text.c_str(),
                    draw_style.font_size,
                    1.0f
                );

            DrawTextEx(
                font,
                value_text.c_str(),
                {
                    std::round(
                        component_rect.x +
                        (
                            component_rect.width -
                            text_size.x
                        ) * 0.5f
                    ),
                    std::round(
                        component_rect.y +
                        (
                            component_rect.height -
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

    DrawTextEx(
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

void DrawColorPickerPopup(
    UIInput<Color>& input,
    const StyleSheet& style_sheet
) {
    if(
        input.input_type != InputType::ColorPicker ||
        !input.picker_open ||
        input.style.display == DisplayType::None
    ) {
        return;
    }

    ComputedStyle draw_style =
        ResolveInputDrawStyle(input,style_sheet);

    const UIColorPickerGeometry geometry =
        GetColorPickerGeometry(input);

    DrawRectangleRounded(
        geometry.popup,
        0.04f,
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawRectangleRoundedLinesEx(
        geometry.popup,
        0.04f,
        8,
        std::max(1.0f,draw_style.border_width),
        ApplyStyleOpacity(
            draw_style.border_color,
            draw_style.opacity
        )
    );

    // Saturation runs left-to-right. Value runs top-to-bottom.
    const Color hue_color = ColorFromHSV01(
        input.picker_hue,
        1.0f,
        1.0f
    );

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
    constexpr std::array<Color,7> hue_colors = {
        Color{255,0,0,255},
        Color{255,255,0,255},
        Color{0,255,0,255},
        Color{0,255,255,255},
        Color{0,0,255,255},
        Color{255,0,255,255},
        Color{255,0,0,255}
    };

    const float hue_segment_height =
        geometry.hue.height/6.0f;

    for(int segment = 0; segment < 6; ++segment) {
        const int segment_y = static_cast<int>(
            std::round(
                geometry.hue.y+
                hue_segment_height*
                static_cast<float>(segment)
            )
        );

        const int next_y = static_cast<int>(
            std::round(
                geometry.hue.y+
                hue_segment_height*
                static_cast<float>(segment+1)
            )
        );

        DrawRectangleGradientV(
            static_cast<int>(std::round(geometry.hue.x)),
            segment_y,
            static_cast<int>(std::round(geometry.hue.width)),
            std::max(1,next_y-segment_y),
            hue_colors[segment],
            hue_colors[segment+1]
        );
    }

    DrawCheckerboard(geometry.alpha,6.0f);

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

    const Vector2 sv_cursor = {
        geometry.saturation_value.x+
            geometry.saturation_value.width*
            input.picker_saturation,
        geometry.saturation_value.y+
            geometry.saturation_value.height*
            (1.0f-input.picker_brightness)
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

    const float hue_y =
        geometry.hue.y+
        geometry.hue.height*
        input.picker_hue;

    DrawRectangleLinesEx(
        {
            geometry.hue.x-2.0f,
            hue_y-2.0f,
            geometry.hue.width+4.0f,
            4.0f
        },
        1.0f,
        WHITE
    );

    const float alpha_x =
        geometry.alpha.x+
        geometry.alpha.width*
        (
            static_cast<float>(input.value.a)/
            255.0f
        );

    DrawRectangleLinesEx(
        {
            alpha_x-2.0f,
            geometry.alpha.y-2.0f,
            4.0f,
            geometry.alpha.height+4.0f
        },
        1.0f,
        WHITE
    );

    DrawCheckerboard(geometry.preview,6.0f);
    DrawRectangleRec(
        geometry.preview,
        input.value
    );

    DrawRectangleLinesEx(
        geometry.preview,
        1.0f,
        ApplyStyleOpacity(
            draw_style.border_color,
            draw_style.opacity
        )
    );

    const std::string hex =
        FormatColorHex(input.value);

    DrawTextEx(
        ResolveUIFont(draw_style.font),
        hex.c_str(),
        {
            geometry.preview.x+
                geometry.preview.width+
                12.0f,
            geometry.preview.y+
                (
                    geometry.preview.height-
                    draw_style.font_size
                )*0.5f
        },
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(
            draw_style.text_color,
            draw_style.opacity
        )
    );

    const bool close_hovered = CheckCollisionPointRec(
        GetMousePosition(),
        geometry.close_button
    );

    DrawRectangleRounded(
        geometry.close_button,
        0.2f,
        4,
        close_hovered
            ? DarkenColor(draw_style.border_color,0.1f)
            : DarkenColor(draw_style.background_color,0.12f)
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
    Vector2 close_size = MeasureTextCached(
        close_font,
        close_text,
        14.0f,
        1.0f
    );

    DrawTextEx(
        close_font,
        close_text,
        {
            std::round(
                geometry.close_button.x+
                (geometry.close_button.width-close_size.x)*0.5f
            ),
            std::round(
                geometry.close_button.y+
                (geometry.close_button.height-close_size.y)*0.5f
            )
        },
        14.0f,
        1.0f,
        draw_style.text_color
    );
}

