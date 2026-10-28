#define CSS_UI_INTERNAL_TEMPLATES
#include "css_emulator.h"
#undef CSS_UI_INTERNAL_TEMPLATES

void UpdateInput(UIInput<float>& input) {
    UpdateInput<float>(input);
}

void UpdateInput(UIInput<int>& input) {
    UpdateInput<int>(input);
}

void UpdateRangeInput(UIRangeInput<float>& input) {
    UpdateRangeInput<float>(input);
}

void UpdateRangeInput(UIRangeInput<int>& input) {
    UpdateRangeInput<int>(input);
}

float* GetVectorComponent(Vector2& value, int index) {
    if(index == 0) {
        return &value.x;
    }

    if(index == 1) {
        return &value.y;
    }

    return nullptr;
}

float* GetVectorComponent(Vector3& value, int index) {
    if(index == 0) {
        return &value.x;
    }

    if(index == 1) {
        return &value.y;
    }

    if(index == 2) {
        return &value.z;
    }

    return nullptr;
}

float* GetVectorComponent(Vector4& value, int index) {
    if(index == 0) {
        return &value.x;
    }

    if(index == 1) {
        return &value.y;
    }

    if(index == 2) {
        return &value.z;
    }

    if(index == 3) {
        return &value.w;
    }

    return nullptr;
}

const float* GetVectorComponent(const Vector2& value, int index) {
    return GetVectorComponent(const_cast<Vector2&>(value), index);
}

const float* GetVectorComponent(const Vector3& value, int index) {
    return GetVectorComponent(const_cast<Vector3&>(value), index);
}

const float* GetVectorComponent(const Vector4& value, int index) {
    return GetVectorComponent(const_cast<Vector4&>(value), index);
}

void UpdateInput(UIInput<Vector2>& input) {
    UpdateVectorInput(input);
}

void UpdateInput(UIInput<Vector3>& input) {
    UpdateVectorInput(input);
}

void UpdateInput(UIInput<Vector4>& input) {
    UpdateVectorInput(input);
}

void UpdateInput(UIInput<bool>& input) {
    if(UpdateTriggerInput(input)) {
        return;
    }

    ResetInputFrameState(input);

    Rectangle rect = input.rect.transform;
    input.hovered = CheckCollisionPointRec(GetUIMousePosition(),rect);
    if(input.hovered) {
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_POINTING_HAND);
    }

    if(input.hovered &&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool old_value = input.value;
        input.value = !input.value;
        input.changed = input.value != old_value;
        input.focused = true;
        input.active = true;
    }
    else {
        input.active = false;
    }
}

Vector3 ColorToHSV01(Color color) {
    const float red = static_cast<float>(color.r) / 255.0f;
    const float green = static_cast<float>(color.g) / 255.0f;
    const float blue = static_cast<float>(color.b) / 255.0f;

    const float maximum = std::max(red,std::max(green,blue));
    const float minimum = std::min(red,std::min(green,blue));
    const float delta = maximum-minimum;

    float hue = 0.0f;

    if(delta > 0.000001f) {
        if(maximum == red) {
            hue = std::fmod(
                (green-blue)/delta,
                6.0f
            );
        }
        else if(maximum == green) {
            hue = (blue-red)/delta + 2.0f;
        }
        else {
            hue = (red-green)/delta + 4.0f;
        }

        hue /= 6.0f;

        if(hue < 0.0f) {
            hue += 1.0f;
        }
    }

    const float saturation =maximum <= 0.000001f? 0.0f: delta/maximum;
    return {
        std::clamp(hue,0.0f,1.0f),
        std::clamp(saturation,0.0f,1.0f),
        std::clamp(maximum,0.0f,1.0f)
    };
}
bool TryParseColorHex(const std::string& text, Color& out) {
    std::string hex = text;
    if(hex.empty()) return false;
    if(hex[0] == '#') hex.erase(0,1);
    if(hex.size() < 6 || hex.size() > 8) return false; // allow RRGGBB or RRGGBBAA
    if(hex.size() == 6) hex += "FF"; // default alpha 255
    if(hex.size() != 8) return false;
    auto parse_component = [&](size_t pos) -> int {
        int value = 0;
        for(int i=0;i<2;i++) {
            char c = hex[pos+i];
            value *= 16;
            if(c >= '0' && c <= '9') value += c - '0';
            else if(c >= 'a' && c <= 'f') value += c - 'a' + 10;
            else if(c >= 'A' && c <= 'F') value += c - 'A' + 10;
            else return -1;
        }
        return value;
    };
    int r = parse_component(0), g = parse_component(2), b = parse_component(4), a = parse_component(6);
    if(r < 0 || g < 0 || b < 0 || a < 0) return false;
    out = { (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
    return true;
}
Color ColorFromHSV01(
    float hue,
    float saturation,
    float brightness,
    unsigned char alpha ) {
    hue = hue-std::floor(hue);
    saturation = std::clamp(saturation,0.0f,1.0f);
    brightness = std::clamp(brightness,0.0f,1.0f);

    const float chroma = brightness*saturation;
    const float sector = hue*6.0f;
    const float x = chroma*(
        1.0f-
        std::abs(
            std::fmod(sector,2.0f)-1.0f
        )
    );
    const float match = brightness-chroma;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    if(sector < 1.0f) {
        red = chroma;
        green = x;
    }
    else if(sector < 2.0f) {
        red = x;
        green = chroma;
    }
    else if(sector < 3.0f) {
        green = chroma;
        blue = x;
    }
    else if(sector < 4.0f) {
        green = x;
        blue = chroma;
    }
    else if(sector < 5.0f) {
        red = x;
        blue = chroma;
    }
    else {
        red = chroma;
        blue = x;
    }

    return {
        static_cast<unsigned char>(
            std::round((red+match)*255.0f)
        ),
        static_cast<unsigned char>(
            std::round((green+match)*255.0f)
        ),
        static_cast<unsigned char>(
            std::round((blue+match)*255.0f)
        ),
        alpha
    };
}

UIColorPickerGeometry GetColorPickerGeometry(
    const UIInput<Color>& input
) {
    const float scale = EffectiveComputedStyleScale(input.style);
    const float popup_width = 294.0f * scale;
    const float popup_height = 248.0f * scale;
    const float padding = 12.0f * scale;
    const float hue_width = 20.0f * scale;

    Rectangle popup = {
        input.rect.transform.x,
        input.rect.transform.y+
            input.rect.transform.height+
            6.0f * scale,
        popup_width,
        popup_height
    };

    const Rectangle visible=CurrentUIVisibleLogicalScreenRect();
    const float visible_right=visible.x+visible.width;
    const float visible_bottom=visible.y+visible.height;
    if(popup.x+popup.width > visible_right-4.0f) {
        popup.x=std::max(visible.x+4.0f,visible_right-popup.width-4.0f);
    }

    if(popup.y+popup.height > visible_bottom-4.0f) {
        popup.y =
            input.rect.transform.y-
            popup.height-
            6.0f * scale;
    }

    popup.y=std::max(visible.y+4.0f,popup.y);

    Rectangle saturation_value = {
        popup.x+padding,
        popup.y+padding,
        popup.width-
            padding*3.0f-
            hue_width,
        158.0f * scale
    };

    Rectangle hue = {
        saturation_value.x+
            saturation_value.width+
            padding,
        saturation_value.y,
        hue_width,
        saturation_value.height
    };

    Rectangle alpha = {
        saturation_value.x,
        saturation_value.y+
            saturation_value.height+
            12.0f * scale,
        saturation_value.width+
            padding+
            hue_width,
        18.0f * scale
    };

    Rectangle preview = {
        saturation_value.x,
        alpha.y+alpha.height+12.0f * scale,
        48.0f * scale,
        32.0f * scale
    };

    Rectangle close_button = {
        popup.x+popup.width-34.0f * scale,
        popup.y+popup.height-34.0f * scale,
        24.0f * scale,
        24.0f * scale
    };

    return {
        popup,
        saturation_value,
        hue,
        alpha,
        preview,
        close_button
    };
}

unsigned char* GetColorComponent(Color& value, int index) {
    switch(index) {
        case 0: return &value.r;
        case 1: return &value.g;
        case 2: return &value.b;
        case 3: return &value.a;
        default: return nullptr;
    }
}

const unsigned char* GetColorComponent(
    const Color& value,
    int index
) {
    return GetColorComponent(
        const_cast<Color&>(value),
        index
    );
}

bool CommitColorDragTextEdit(UIInput<Color>& input) {
    if(input.active_component < 0 || input.active_component >= 4) return false;

    std::string& buffer = input.component_buffers[input.active_component];
    if(buffer.empty()) return false;

    unsigned char* component = GetColorComponent(input.value,input.active_component);
    if(component == nullptr) return false;

    const unsigned char old_value = *component;
    try {
        const int next = std::clamp(std::stoi(buffer),0,255);
        *component = static_cast<unsigned char>(next);
    }
    catch(...) {
        return false;
    }

    const bool changed = *component != old_value;
    input.changed = input.changed || changed;
    return changed;
}

void UpdateInput(UIInput<Color>& input) {
    if(UpdateTriggerInput(input)) {
        return;
    }

    ResetInputFrameState(input);

    Rectangle rect = input.rect.transform;
    Vector2 mouse = GetUIMousePosition();

    input.hovered =
        CheckCollisionPointRec(mouse,rect);

    const Color previous = input.value;

    if(input.input_type == InputType::Drag) {
        Rectangle content = {
            rect.x + input.style.padding.left,
            rect.y + input.style.padding.top,
            std::max(
                0.0f,
                rect.width -
                    input.style.padding.left -
                    input.style.padding.right
            ),
            std::max(
                0.0f,
                rect.height -
                    input.style.padding.top -
                    input.style.padding.bottom
            )
        };

        const float component_width =
            content.width / 4.0f;

        int hovered_component = -1;

        if(
            input.hovered &&
            component_width > 0.0f
        ) {
            hovered_component = std::clamp(
                static_cast<int>(
                    (mouse.x - content.x) /
                    component_width
                ),
                0,
                3
            );

            CSS_UI_REQUESTED_CURSOR =
                input.drag_text_editing &&
                input.active_component == hovered_component
                    ? MOUSE_CURSOR_IBEAM
                    : MOUSE_CURSOR_RESIZE_EW;
        }

        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const int previous_component = input.active_component;
            const bool was_drag_text_editing = input.drag_text_editing;
            const bool drag_double_click =
                hovered_component >= 0 &&
                RegisterDragInputClick(input,mouse,hovered_component);

            if(
                was_drag_text_editing &&
                (hovered_component < 0 || hovered_component != previous_component)
            ) {
                CommitColorDragTextEdit(input);
                input.drag_text_editing = false;
                input.drag_text_select_all = false;
            }

            input.focused = hovered_component >= 0;
            input.dragging = false;

            if(input.focused) {
                input.active_component = hovered_component;
                const unsigned char* component =
                    GetColorComponent(input.value,input.active_component);
                if(
                    component != nullptr &&
                    !(
                        was_drag_text_editing &&
                        hovered_component == previous_component
                    )
                ) {
                    input.component_buffers[input.active_component] =
                        TextFormat("%d",static_cast<int>(*component));
                }

                if(drag_double_click) {
                    input.drag_text_editing = true;
                    UIInputSelectAllText(
                        input,
                        input.component_buffers[input.active_component]
                    );
                    input.dragging = false;
                    input.caret_frame_counter = 0;
                }
                else if(!input.drag_text_editing) {
                    input.dragging = true;
                    input.drag_start_value = input.value;
                    input.drag_accumulator = 0.0f;
                }

                if(!drag_double_click && input.drag_text_editing) {
                    Rectangle component_rect = {
                        content.x + component_width*static_cast<float>(input.active_component),
                        content.y,
                        component_width,
                        content.height
                    };
                    const bool extend =
                        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                    UIInputSetCaretFromMouse(
                        input,
                        input.component_buffers[input.active_component],
                        component_rect,
                        input.style,
                        mouse.x,
                        extend
                    );
                    input.text_mouse_selecting = true;
                }
            }
            else {
                input.active_component = -1;
                input.drag_text_editing = false;
                input.drag_text_select_all = false;
                input.last_drag_click_time = -1.0;
            }
        }

        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            input.dragging = false;
            input.text_mouse_selecting = false;
        }

        if(
            input.text_mouse_selecting &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            input.drag_text_editing &&
            input.focused &&
            input.active_component >= 0
        ) {
            Rectangle component_rect = {
                content.x + component_width*static_cast<float>(input.active_component),
                content.y,
                component_width,
                content.height
            };
            UIInputSetCaretFromMouse(
                input,
                input.component_buffers[input.active_component],
                component_rect,
                input.style,
                mouse.x,
                true
            );
        }

        input.active = input.dragging;

        if(
            !input.drag_text_editing &&
            input.dragging &&
            input.active_component >= 0
        ) {
            float modifier = 1.0f;

            if(
                IsKeyDown(KEY_LEFT_SHIFT) ||
                IsKeyDown(KEY_RIGHT_SHIFT)
            ) {
                modifier = 0.1f;
            }
            else if(
                IsKeyDown(KEY_LEFT_CONTROL) ||
                IsKeyDown(KEY_RIGHT_CONTROL)
            ) {
                modifier = 10.0f;
            }

            const float speed =
                std::max(
                    0.01f,
                    input.drag_speed
                );

            input.drag_accumulator +=
                GetUIMouseDelta().x *
                speed *
                modifier;

            unsigned char* component =
                GetColorComponent(
                    input.value,
                    input.active_component
                );

            const unsigned char* start_component =
                GetColorComponent(
                    input.drag_start_value,
                    input.active_component
                );

            if(
                component != nullptr &&
                start_component != nullptr
            ) {
                const int next = std::clamp(
                    static_cast<int>(
                        std::round(
                            static_cast<float>(
                                *start_component
                            ) +
                            input.drag_accumulator
                        )
                    ),
                    0,
                    255
                );

                *component =
                    static_cast<unsigned char>(next);
            }
        }

        if(
            input.hovered &&
            !input.dragging &&
            !input.drag_text_editing &&
            hovered_component >= 0
        ) {
            const float wheel =
                GetMouseWheelMove();

            if(wheel != 0.0f) {
                unsigned char* component =
                    GetColorComponent(
                        input.value,
                        hovered_component
                    );

                if(component != nullptr) {
                    const int direction =
                        wheel > 0.0f ? 1 : -1;

                    *component =
                        static_cast<unsigned char>(
                            std::clamp(
                                static_cast<int>(*component) +
                                    direction,
                                0,
                                255
                            )
                        );
                }
            }
        }

        if(
            input.drag_text_editing &&
            input.focused &&
            input.active_component >= 0
        ) {
            std::string& buffer =
                input.component_buffers[input.active_component];
            input.caret_frame_counter++;
            UIInputHandleTextNavigation(input,buffer);

            int character = GetCharPressed();
            while(character > 0) {
                if(
                    character >= '0' &&
                    character <= '9' &&
                    (UIInputHasTextSelection(input) || buffer.size() < input.max_characters)
                ) {
                    UIInputInsertTextCharacter(input,buffer,static_cast<char>(character));
                }
                character = GetCharPressed();
            }

            if(UIKeyPressedOrRepeat(KEY_BACKSPACE)) {
                UIInputBackspaceText(input,buffer);
            }

            if(UIKeyPressedOrRepeat(KEY_DELETE)) {
                UIInputDeleteText(input,buffer);
            }

            if(IsKeyPressed(KEY_ENTER)) {
                CommitColorDragTextEdit(input);
                input.submitted = true;
                input.focused = false;
                input.drag_text_editing = false;
                input.drag_text_select_all = false;
                input.text_mouse_selecting = false;
                UIInputCollapseTextSelection(input,0);
                input.last_drag_click_time = -1.0;
                input.active_component = -1;
                buffer.clear();
            }
            else if(IsKeyPressed(KEY_ESCAPE)) {
                input.focused = false;
                input.drag_text_editing = false;
                input.drag_text_select_all = false;
                input.text_mouse_selecting = false;
                UIInputCollapseTextSelection(input,0);
                input.last_drag_click_time = -1.0;
                input.active_component = -1;
                buffer.clear();
            }
        }

        input.changed =
            input.changed ||
            input.value.r != previous.r ||
            input.value.g != previous.g ||
            input.value.b != previous.b ||
            input.value.a != previous.a;

        return;
    }

    if(input.input_type != InputType::ColorPicker) {
        input.active = false;
        input.dragging = false;
        return;
    }

    if(input.hovered) {
        CSS_UI_REQUESTED_CURSOR =
            MOUSE_CURSOR_POINTING_HAND;
    }

    const UIColorPickerGeometry geometry =
        GetColorPickerGeometry(input);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(input.hovered) {
            input.picker_open =
                !input.picker_open;

            input.focused =
                input.picker_open;

            if(input.picker_open) {
                Vector3 hsv =
                    ColorToHSV01(input.value);

                input.picker_hue = hsv.x;
                input.picker_saturation = hsv.y;
                input.picker_brightness = hsv.z;
                input.picker_initialized = true;
            }
        }
        else if(input.picker_open) {
            if(
                CheckCollisionPointRec(
                    mouse,
                    geometry.close_button
                )
            ) {
                input.picker_open = false;
                input.focused = false;
                input.picker_drag_region = 0;
                input.dragging = false;
            }
            else if(
                CheckCollisionPointRec(
                    mouse,
                    geometry.saturation_value
                )
            ) {
                input.picker_drag_region = 1;
            }
            else if(
                CheckCollisionPointRec(
                    mouse,
                    geometry.hue
                )
            ) {
                input.picker_drag_region = 2;
            }
            else if(
                CheckCollisionPointRec(
                    mouse,
                    geometry.alpha
                )
            ) {
                input.picker_drag_region = 3;
            }
            else if(
                !CheckCollisionPointRec(
                    mouse,
                    geometry.popup
                )
            ) {
                input.picker_open = false;
                input.focused = false;
                input.picker_drag_region = 0;
            }
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.picker_drag_region = 0;
        input.dragging = false;
    }

    if(
        input.picker_open &&
        IsKeyPressed(KEY_ESCAPE)
    ) {
        input.picker_open = false;
        input.focused = false;
        input.picker_drag_region = 0;
    }

    if(!input.picker_open) {
        // Popup closed – reset embedded hex editor state.
        if(input.hex_input) {
            input.hex_input->focused = false;
            input.hex_input->edit_buffer.clear();
            input.hex_input->text_mouse_selecting = false;
            UIInputCollapseTextSelection(*input.hex_input, 0);
        }
        input.active = false;
        return;
    }

    // ----- Embedded hex color editor -----
    {
        if(!input.hex_input) {
            input.hex_input = std::make_unique<UIInput<std::string>>();
            input.hex_input->style = input.style;
            input.hex_input->max_characters = 9;      // "#RRGGBBAA"
            input.hex_input->placeholder = "#RRGGBBAA";
            input.hex_input->show_value_box = false;
            input.hex_input->style.text_align = TextAlign::Left;
        }

        // Position: where the static hex text was previously drawn.
        const float hex_x = geometry.preview.x + geometry.preview.width + 12.0f;

        // Size the box to fit "#RRGGBBAA" in the resolved font/size,
        // plus padding — rather than stretching to fill the popup.
        const Font hex_font = ResolveUIFont(input.hex_input->style.font);
        const float hex_font_size = input.hex_input->style.font_size;   // verify this matches your ComputedStyle field name
        const float hex_char_spacing = 1.0f;                            // match spacing used elsewhere by DrawTextEx

        const Vector2 hex_text_size = MeasureTextEx(
            hex_font,
            "#RRGGBBAA",
            hex_font_size,
            hex_char_spacing
        );

        const float hex_padding = 16.0f;   // 8px each side — tune to taste

        const float hex_available_width = std::max(
            0.0f,
            geometry.popup.x + geometry.popup.width - hex_x - 12.0f
        );

        const float hex_width = std::min(
            hex_text_size.x + hex_padding,
            hex_available_width
        );

        Rectangle hex_rect = {
            hex_x,
            geometry.preview.y,
            hex_width,
            geometry.preview.height
        };

        input.hex_input->rect.transform = hex_rect;

        // Sync display value when not actively editing.
        if(!input.hex_input->focused) {
            input.hex_input->value = FormatColorHex(input.value);
            input.hex_input->edit_buffer.clear();
        }

        input.hex_input_was_focused = input.hex_input->focused;

        // Process the embedded input (mouse, keyboard, caret, selection).
        UpdateInput(*input.hex_input);

        const bool focus_lost = input.hex_input_was_focused && !input.hex_input->focused;
        const bool should_commit = input.hex_input->submitted || focus_lost;

        if(should_commit) {
            Color parsed;
            if(TryParseColorHex(input.hex_input->value, parsed)) {
                input.value = parsed;
                input.changed = true;

                // Keep the picker's HSV state in sync.
                Vector3 hsv = ColorToHSV01(parsed);
                input.picker_hue = hsv.x;
                input.picker_saturation = hsv.y;
                input.picker_brightness = hsv.z;
            }
            input.hex_input->submitted = false;
        }

        // If the field lost focus or we just committed, resync display.
        if(!input.hex_input->focused) {
            input.hex_input->value = FormatColorHex(input.value);
        }
    }

    if(
        CheckCollisionPointRec(
            mouse,
            geometry.saturation_value
        ) ||
        CheckCollisionPointRec(
            mouse,
            geometry.hue
        ) ||
        CheckCollisionPointRec(
            mouse,
            geometry.alpha
        )
    ) {
        CSS_UI_REQUESTED_CURSOR =
            MOUSE_CURSOR_CROSSHAIR;
    }

    input.dragging =
        input.picker_drag_region != 0 &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    input.active = input.dragging;

    if(input.dragging) {
        if(input.picker_drag_region == 1) {
            input.picker_saturation = std::clamp(
                (
                    mouse.x -
                    geometry.saturation_value.x
                ) /
                std::max(
                    1.0f,
                    geometry.saturation_value.width
                ),
                0.0f,
                1.0f
            );

            input.picker_brightness =
                1.0f -
                std::clamp(
                    (
                        mouse.y -
                        geometry.saturation_value.y
                    ) /
                        std::max(
                            1.0f,
                            geometry.saturation_value.height
                        ),
                    0.0f,
                    1.0f
                );
        }
        else if(input.picker_drag_region == 2) {
            input.picker_hue = std::clamp(
                (
                    mouse.y -
                    geometry.hue.y
                ) /
                    std::max(
                        1.0f,
                        geometry.hue.height
                    ),
                0.0f,
                1.0f
            );
        }
        else if(input.picker_drag_region == 3) {
            input.value.a =
                static_cast<unsigned char>(
                    std::round(
                        std::clamp(
                            (
                                mouse.x -
                                geometry.alpha.x
                            ) /
                                std::max(
                                    1.0f,
                                    geometry.alpha.width
                                ),
                            0.0f,
                            1.0f
                        ) *
                        255.0f
                    )
                );
        }

        const unsigned char alpha =
            input.value.a;

        input.value = ColorFromHSV01(
            input.picker_hue,
            input.picker_saturation,
            input.picker_brightness,
            alpha
        );
    }

    input.changed =
        input.value.r != previous.r ||
        input.value.g != previous.g ||
        input.value.b != previous.b ||
        input.value.a != previous.a;
}
void UpdateInput(UIInput<std::string>& input) {
    if(UpdateTriggerInput(input)) {
        return;
    }

    ResetInputFrameState(input);

    Rectangle rect = input.rect.transform;
    Vector2 mouse = GetUIMousePosition();

    input.hovered =CheckCollisionPointRec(mouse,rect);

    if(input.hovered) {
        CSS_UI_REQUESTED_CURSOR=(MOUSE_CURSOR_IBEAM);
    }

    // Focus can already have been granted before this function runs (pointer
    // focus on press, Tab navigation), in which case the false->true edge is
    // never seen here and the edit buffer would stay empty. A focused field
    // whose buffer was never filled counts as "just focused".
    bool just_focused = false;
    if(input.focused && input.edit_buffer.empty() && !input.value.empty()) {
        input.edit_buffer = input.value;
        UIInputSelectAllText(input,input.edit_buffer);
        input.caret_frame_counter = 0;
        just_focused = true;
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool was_focused = input.focused && !just_focused;
        input.focused = input.hovered;
        if(input.focused && !was_focused) {
            // First click: select everything. The caret is placed by the next click.
            input.edit_buffer = input.value;
            UIInputSelectAllText(input,input.edit_buffer);
            input.caret_frame_counter = 0;
            just_focused = true;
        }

        if(input.focused && !just_focused) {
            Rectangle text_rect = input.rect.transform;
            text_rect.x += input.style.padding.left;
            text_rect.y += input.style.padding.top;
            text_rect.width = std::max(
                0.0f,
                text_rect.width-input.style.padding.left-input.style.padding.right
            );
            text_rect.height = std::max(
                0.0f,
                text_rect.height-input.style.padding.top-input.style.padding.bottom
            );
            const bool extend = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            UIInputSetCaretFromMouse(
                input,input.edit_buffer,text_rect,input.style,mouse.x,extend
            );
            input.text_mouse_selecting = true;
        }

        if(was_focused && !input.focused) {
            input.value = input.edit_buffer;
            input.edit_buffer.clear();
            input.text_mouse_selecting = false;
            UIInputCollapseTextSelection(input,0);
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.text_mouse_selecting = false;
    }

    if(input.text_mouse_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && input.focused) {
        Rectangle text_rect = input.rect.transform;
        text_rect.x += input.style.padding.left;
        text_rect.y += input.style.padding.top;
        text_rect.width = std::max(
            0.0f,
            text_rect.width-input.style.padding.left-input.style.padding.right
        );
        text_rect.height = std::max(
            0.0f,
            text_rect.height-input.style.padding.top-input.style.padding.bottom
        );
        UIInputSetCaretFromMouse(
            input,input.edit_buffer,text_rect,input.style,mouse.x,true
        );
    }

    input.active = input.focused;

    if(!input.focused) {
        // Focus can be taken away elsewhere (pointer focus, Tab). Drop the stale
        // buffer so the next focus re-reads the current value.
        input.edit_buffer.clear();
        return;
    }

    input.caret_frame_counter++;
    UIInputHandleTextNavigation(input,input.edit_buffer);

    int character = GetCharPressed();

    while(character > 0) {
        if(
            character >= 32 &&
            character <= 125 &&
            (UIInputHasTextSelection(input) || input.edit_buffer.size() < input.max_characters)
        ) {
            UIInputInsertTextCharacter(input,input.edit_buffer,static_cast<char>(character));
            input.value = input.edit_buffer;
            input.changed = true;
        }

        character = GetCharPressed();
    }

    if(UIKeyPressedOrRepeat(KEY_BACKSPACE)) {
        const std::string before = input.edit_buffer;
        UIInputBackspaceText(input,input.edit_buffer);
        if(input.edit_buffer != before) {
            input.value = input.edit_buffer;
            input.changed = true;
        }
    }

    if(UIKeyPressedOrRepeat(KEY_DELETE)) {
        const std::string before = input.edit_buffer;
        UIInputDeleteText(input,input.edit_buffer);
        if(input.edit_buffer != before) {
            input.value = input.edit_buffer;
            input.changed = true;
        }
    }
    if(IsKeyPressed(KEY_TAB)) {
        input.focused = false;
        input.text_mouse_selecting = false;
        input.edit_buffer.clear();
        UIInputCollapseTextSelection(input,0);
    }
    if(IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) {
        if(IsKeyPressed(KEY_C)) {
            UIInputCopyTextSelection(input,input.edit_buffer);
        }
        if(IsKeyPressed(KEY_V)) {
            UIInputPasteText(input,input.edit_buffer);
        }
    }

    if(IsKeyPressed(KEY_ENTER)) {
        input.value = input.edit_buffer;
        input.submitted = true;
        input.focused = false;
        input.text_mouse_selecting = false;
        input.edit_buffer.clear();
        UIInputCollapseTextSelection(input,0);
    }

    if(IsKeyPressed(KEY_ESCAPE)) {
        input.focused = false;
        input.text_mouse_selecting = false;
        input.edit_buffer.clear();
        UIInputCollapseTextSelection(input,0);
    }
}

void UpdateSelectable(UISelectable& selectable) {
    selectable.clicked = false;
    if(selectable.disabled) {
        selectable.hovered = false;
        selectable.active = false;
        selectable.pressed_inside = false;
        return;
    }

    selectable.hovered = CheckCollisionPointRec(
        GetUIMousePosition(),
        selectable.rect.transform
    );
    if(selectable.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selectable.pressed_inside = true;
        selectable.focused = true;
    }
    selectable.active = selectable.pressed_inside &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        selectable.clicked = selectable.pressed_inside && selectable.hovered;
        selectable.pressed_inside = false;
        selectable.active = false;
        if(selectable.clicked && selectable.toggle_on_click) {
            selectable.selected = !selectable.selected;
        }
    }
    if(
        selectable.focused &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    ) {
        selectable.clicked = true;
        if(selectable.toggle_on_click) selectable.selected = !selectable.selected;
    }
}

Rectangle ComboPopupRectangle(const UIComboBox& combo) {
    const float visible_height = std::min(
        combo.style.popup_max_height,
        combo.style.item_height * static_cast<float>(combo.items.size())
    );
    return {
        combo.rect.transform.x,
        combo.rect.transform.y + combo.rect.transform.height,
        combo.style.popup_width > 0.0f
            ? combo.style.popup_width
            : combo.rect.transform.width,
        std::max(combo.style.item_height,visible_height)
    };
}

void UpdateComboBox(UIComboBox& combo) {
    combo.changed = false;
    combo.hovered_index = -1;

    if(combo.disabled) {
        combo.hovered = false;
        combo.active = false;
        combo.pressed_inside = false;
        combo.open = false;
        return;
    }

    if(combo.items.empty()) {
        combo.selected_index = -1;
    }
    else {
        combo.selected_index = std::clamp(
            combo.selected_index,
            -1,
            static_cast<int>(combo.items.size())-1
        );
    }

    const Vector2 mouse = GetUIMousePosition();
    combo.hovered = CheckCollisionPointRec(mouse,combo.rect.transform);
    const bool was_open = combo.open;
    bool opened_this_frame = false;

    // Mouse activation is press-armed/release-committed. Consume both halves of
    // the click so opening/closing a ComboBox cannot also activate a sibling,
    // parent header, dock splitter, or control behind the overlay.
    if(combo.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        combo.pressed_inside = true;
        combo.active = true;
        combo.focused = true;
        CSS_UI_POINTER_INPUT_CONSUMED = true;
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(combo.pressed_inside) {
            CSS_UI_POINTER_INPUT_CONSUMED = true;
            if(combo.hovered) {
                if(combo.open) {
                    combo.open = false;
                }
                else {
                    combo.open = true;
                    opened_this_frame = true;
                }
            }
        }
        combo.pressed_inside = false;
        combo.active = false;
    }

    // Keyboard ownership is handled before the closed fast-path. The old order
    // returned early here, which meant Enter/Space could close an open combo but
    // could not reliably open a closed one.
    if(combo.focused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        combo.open = !combo.open;
        opened_this_frame = combo.open;
        combo.pressed_inside = false;
        combo.active = false;
    }

    if(combo.open && IsKeyPressed(KEY_ESCAPE)) {
        combo.open = false;
        combo.pressed_inside = false;
        combo.active = false;
        return;
    }

    if(combo.focused && !combo.items.empty()) {
        int next = combo.selected_index;
        bool navigated = false;
        if(IsKeyPressed(KEY_DOWN)) {
            next = combo.selected_index < 0
                ? 0
                : std::min(combo.selected_index+1,static_cast<int>(combo.items.size())-1);
            navigated = true;
        }
        if(IsKeyPressed(KEY_UP)) {
            next = combo.selected_index < 0
                ? 0
                : std::max(0,combo.selected_index-1);
            navigated = true;
        }
        if(navigated && next != combo.selected_index) {
            combo.selected_index = next;
            combo.changed = true;
        }
    }

    if(!combo.open) return;

    Rectangle popup = ComboPopupRectangle(combo);
    const float content_height = combo.style.item_height *
        static_cast<float>(combo.items.size());
    const float max_scroll = std::max(0.0f,content_height-popup.height);
    combo.scroll_offset = std::clamp(combo.scroll_offset,0.0f,max_scroll);

    // The click that opened the popup belongs to the owner control. Never reuse
    // it as a popup/outside click in the same update. This is especially
    // important for ComboBoxes inside rebuilt event cards/collapsibles.
    if(opened_this_frame) return;

    if(CheckCollisionPointRec(mouse,popup)) {
        const float popup_wheel = GetMouseWheelMove();
        if(
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) ||
            popup_wheel != 0.0f
        ) {
            CSS_UI_POINTER_INPUT_CONSUMED = true;
        }

        combo.scroll_offset = std::clamp(
            combo.scroll_offset - popup_wheel*combo.style.scroll_speed,
            0.0f,
            max_scroll
        );

        const float local_y = mouse.y-popup.y+combo.scroll_offset;
        const int index = static_cast<int>(local_y/combo.style.item_height);
        if(index >= 0 && index < static_cast<int>(combo.items.size())) {
            combo.hovered_index = index;
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                CSS_UI_POINTER_INPUT_CONSUMED = true;
                if(combo.selected_index != index) {
                    combo.selected_index = index;
                    combo.changed = true;
                }
                combo.open = false;
                combo.pressed_inside = false;
                combo.active = false;
            }
        }
    }
    else if(
        was_open &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !combo.hovered
    ) {
        // Outside click dismisses the overlay and is fully consumed. It must not
        // become a click on whatever sits behind the popup.
        CSS_UI_POINTER_INPUT_CONSUMED = true;
        combo.open = false;
        combo.pressed_inside = false;
        combo.active = false;
    }
}

void UpdateCanvas(UICanvas& canvas) {
    const Vector2 mouse = GetUIMousePosition();
    const Vector2 mouse_delta = GetUIMouseDelta();
    canvas.hovered = CheckCollisionPointRec(mouse,canvas.rect.transform);
    canvas.local_mouse = {
        mouse.x-canvas.rect.transform.x,
        mouse.y-canvas.rect.transform.y
    };
    canvas.local_mouse_delta = mouse_delta;
    canvas.mouse_double_clicked.fill(false);
    for(int button=0; button<static_cast<int>(canvas.mouse_down.size()); ++button) {
        canvas.mouse_down[static_cast<std::size_t>(button)] = IsMouseButtonDown(button);
        canvas.mouse_pressed[static_cast<std::size_t>(button)] =
            canvas.hovered && IsMouseButtonPressed(button);
        canvas.mouse_released[static_cast<std::size_t>(button)] =
            IsMouseButtonReleased(button);

        if(canvas.mouse_pressed[static_cast<std::size_t>(button)]) {
            canvas.mouse_pressed_inside[static_cast<std::size_t>(button)] = true;
            const double now = GetTime();
            const Vector2 previous = canvas.last_click_position[static_cast<std::size_t>(button)];
            const float distance = Vector2Distance(previous,mouse);
            canvas.mouse_double_clicked[static_cast<std::size_t>(button)] =
                canvas.last_click_time[static_cast<std::size_t>(button)] > 0.0 &&
                now-canvas.last_click_time[static_cast<std::size_t>(button)] <= canvas.double_click_seconds &&
                distance <= canvas.double_click_distance *
                    EffectiveComputedStyleScale(canvas.style);
            canvas.last_click_time[static_cast<std::size_t>(button)] = now;
            canvas.last_click_position[static_cast<std::size_t>(button)] = mouse;
        }
    }
    canvas.pressed = canvas.mouse_pressed[MOUSE_BUTTON_LEFT];
    canvas.released = canvas.mouse_released[MOUSE_BUTTON_LEFT];
    canvas.clicked =
        canvas.released &&
        canvas.hovered &&
        canvas.mouse_pressed_inside[MOUSE_BUTTON_LEFT];
    for(std::size_t button=0; button<canvas.mouse_released.size(); ++button) {
        if(canvas.mouse_released[button]) canvas.mouse_pressed_inside[button] = false;
    }
    if(canvas.pressed) {
        canvas.dragging = true;
        canvas.drag_delta = {};
        if(canvas.focusable) canvas.focused = true;
    }
    if(canvas.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        canvas.drag_delta = mouse_delta;
    }
    if(canvas.released) canvas.dragging = false;
    canvas.wheel_delta = canvas.hovered ? GetMouseWheelMoveV() : Vector2{};
    canvas.wheel = canvas.wheel_delta.y;
    canvas.key_pressed = canvas.focused ? GetKeyPressed() : 0;
    canvas.character_pressed = canvas.focused ? GetCharPressed() : 0;
    if(canvas.disabled) {
        canvas.pressed = false;
        canvas.clicked = false;
        canvas.dragging = false;
        canvas.wheel = 0.0f;
        canvas.wheel_delta = {};
        canvas.mouse_down.fill(false);
        canvas.mouse_pressed.fill(false);
        canvas.mouse_released.fill(false);
        canvas.mouse_double_clicked.fill(false);
        canvas.key_pressed = 0;
        canvas.character_pressed = 0;
    }
    if(canvas.on_update) canvas.on_update(canvas);
}

void UpdateImage(UIImage& image) {
    image.clicked = false;
    image.pressed = false;
    image.released = false;
    if(!image.interactive || image.disabled || image.rect.inherited_disabled) {
        image.hovered = false;
        return;
    }
    image.hovered = CheckCollisionPointRec(GetUIMousePosition(),image.rect.transform);
    image.pressed = image.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    image.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(image.pressed) image.focused = true;
    image.clicked = image.released && image.hovered && image.focused;
}

UIElementLayoutRef GetUIElementLayoutRef(UIElement& element) {
    return std::visit([](auto& widget) -> UIElementLayoutRef {
        using T = std::decay_t<decltype(widget)>;
        if constexpr(IsUICompositePointerV<T> || IsUICustomElementPointerV<T>) {
            if(!widget) return {};
            return {&widget->rect,&widget->style};
        }
        else {
            return {&widget.rect,&widget.style};
        }
    },element);
}

void UpdateSplitter(
    UIContainer& parent,
    std::size_t index,
    UISplitter& splitter
) {
    splitter.changed = false;
    splitter.delta = 0.0f;
    const bool disabled = splitter.disabled || splitter.rect.inherited_disabled;
    splitter.hovered = !disabled && CheckCollisionPointRec(
        GetUIMousePosition(),splitter.rect.transform
    );
    if(index == 0 || index+1 >= parent.elements.size()) {
        splitter.dragging = false;
        return;
    }
    UIElementLayoutRef before = GetUIElementLayoutRef(parent.elements[index-1]);
    UIElementLayoutRef after = GetUIElementLayoutRef(parent.elements[index+1]);
    if(before.rect == nullptr || after.rect == nullptr) {
        splitter.dragging = false;
        return;
    }
    UISplitterOrientation orientation = splitter.orientation;
    if(orientation == UISplitterOrientation::Auto) {
        orientation = parent.direction == LayoutDirection::Row
            ? UISplitterOrientation::Vertical
            : UISplitterOrientation::Horizontal;
    }
    if(splitter.hovered || splitter.dragging) {
        CSS_UI_REQUESTED_CURSOR=(
            orientation == UISplitterOrientation::Vertical
                ? MOUSE_CURSOR_RESIZE_EW
                : MOUSE_CURSOR_RESIZE_NS
        );
    }
    if(splitter.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        splitter.dragging = true;
        splitter.drag_origin = GetUIMousePosition();
        splitter.before_start_size = {
            before.rect->transform.width,before.rect->transform.height
        };
        splitter.after_start_size = {
            after.rect->transform.width,after.rect->transform.height
        };
    }

    if(splitter.dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetUIMousePosition();
        float delta = orientation == UISplitterOrientation::Vertical
            ? mouse.x-splitter.drag_origin.x
            : mouse.y-splitter.drag_origin.y;
        const float before_start = orientation == UISplitterOrientation::Vertical
            ? splitter.before_start_size.x
            : splitter.before_start_size.y;
        const float after_start = orientation == UISplitterOrientation::Vertical
            ? splitter.after_start_size.x
            : splitter.after_start_size.y;

        const float scale = EffectiveComputedStyleScale(splitter.style);
        const float minimum_before = splitter.minimum_before * scale;
        const float minimum_after = splitter.minimum_after * scale;

        // before.style->min/max_width and after.style->min/max_width are already
        // screen-space (ResolveComputedLengths + ApplyComputedStyleScale ran on
        // them), so no rescale needed here — they're directly comparable to
        // before_start/after_start.
        const float before_min = orientation == UISplitterOrientation::Vertical
            ? before.style->min_width : before.style->min_height;
        const float before_max = orientation == UISplitterOrientation::Vertical
            ? before.style->max_width : before.style->max_height;
        const float after_min = orientation == UISplitterOrientation::Vertical
            ? after.style->min_width : after.style->min_height;
        const float after_max = orientation == UISplitterOrientation::Vertical
            ? after.style->max_width : after.style->max_height;

        // delta must satisfy the splitter's own minimums AND each panel's
        // actual CSS-resolved min/max — whichever is tighter wins, so this
        // can never disagree with what ApplyRuntimeSizeToElement clamps to later.
        const float lower_bound = std::max({
            minimum_before-before_start,
            before_min-before_start,
            after_start-after_max
        });
        const float upper_bound = std::min({
            after_start-minimum_after,
            after_start-after_min,
            before_max-before_start
        });

        if(lower_bound > upper_bound) {
            delta = 0.0f;
        }
        else {
            delta = std::clamp(delta,lower_bound,upper_bound);
        }

        splitter.delta = delta;
        splitter.changed = true;
        before.rect->runtime_size_override = true;
        after.rect->runtime_size_override = true;
        before.rect->runtime_size = splitter.before_start_size;
        after.rect->runtime_size = splitter.after_start_size;
        if(orientation == UISplitterOrientation::Vertical) {
            before.rect->runtime_size.x = before_start+delta;
            after.rect->runtime_size.x = after_start-delta;
        }
        else {
            before.rect->runtime_size.y = before_start+delta;
            after.rect->runtime_size.y = after_start-delta;
        }
        MarkLayoutDirty(parent,false);
        CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) splitter.dragging = false;
}

void UpdateButton(UIButton& button) {   
    button.clicked = false;
    button.pressed = false;
    button.released = false;
    button.hover_entered = false;
    button.hover_exited = false;

    if(button.disabled) {
        button.hovered = false;
        button.active = false;
        button.pressed_inside = false;
        button.focused = false;
        return;
    }

    Rectangle hit_rect = button.rect.transform;

    bool was_hovered = button.hovered;

    button.hovered = CheckCollisionPointRec(GetUIMousePosition(),hit_rect);
    button.hover_entered =!was_hovered && button.hovered;
    button.hover_exited =was_hovered && !button.hovered;
    if(button.hovered){
        CSS_UI_REQUESTED_CURSOR=MOUSE_CURSOR_POINTING_HAND;
    }
    if(button.hovered &&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        button.pressed = true;
        button.pressed_inside = true;
        button.focused = true;
    }

    button.active =button.pressed_inside &&IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        button.released =button.pressed_inside;
        button.clicked =button.pressed_inside &&button.hovered;
        button.pressed_inside = false;
        button.active = false;
    }

    if(button.focused &&(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        button.clicked = true;
        button.pressed = true;
        button.released = true;
    }
}