#include "css_parser.h"
#include "component_registry.h"

std::string TrimCSS(const std::string& text) {
    std::size_t first = 0;
    std::size_t last = text.size();

    while(
        first < last &&
        std::isspace(
            static_cast<unsigned char>(text[first])
        )
    ) {
        first++;
    }

    while(
        last > first &&
        std::isspace(
            static_cast<unsigned char>(text[last - 1])
        )
    ) {
        last--;
    }

    return text.substr(first,last - first);
}

std::string LowerCSS(std::string text) {
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return text;
}

bool EndsWithCSS(
    const std::string& text,
    const std::string& suffix
) {
    return
        text.size() >= suffix.size() &&
        text.compare(
            text.size() - suffix.size(),
            suffix.size(),
            suffix
        ) == 0;
}

std::optional<float> ParseStrictCSSFloat(
    const std::string& text
) {
    try {
        std::size_t parsed_characters = 0;

        float value = std::stof(
            text,
            &parsed_characters
        );

        if(parsed_characters != text.size()) {
            return std::nullopt;
        }

        return value;
    }
    catch(...) {
        return std::nullopt;
    }
}

std::optional<float> ParseCSSNumber(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(EndsWithCSS(value,"px")) {
        value.resize(value.size() - 2);
    }

    return ParseStrictCSSFloat(value);
}

std::optional<CSSLength> ParseCSSLength(
    std::string value
) {
    value = TrimCSS(value);
    const std::string lower = LowerCSS(value);

    if(lower.empty()) return std::nullopt;
    if(lower == "auto") return CSSLength{0.0f,LengthUnit::Auto};

    // Parse expressions once. Variables intentionally remain in the AST so a
    // cascaded custom property can change the evaluated result at runtime.
    const bool expression =
        lower.find("var(") != std::string::npos ||
        lower.rfind("calc(",0) == 0 ||
        lower.rfind("min(",0) == 0 ||
        lower.rfind("max(",0) == 0 ||
        lower.rfind("clamp(",0) == 0;
    if(expression) {
        std::string error;
        UIStyleExpression parsed = ParseUIStyleExpression(value,&error);
        if(!parsed) {
            TraceLog(LOG_WARNING,"Invalid RCSS expression '%s': %s",value.c_str(),error.c_str());
            return std::nullopt;
        }
        CSSLength result;
        result.unit = LengthUnit::Expression;
        result.expression = value;
        result.expression_ast = std::move(parsed);
        return result;
    }

    std::string number_text = lower;
    LengthUnit unit = LengthUnit::Pixels;
    if(EndsWithCSS(number_text,"rem")) {
        number_text.resize(number_text.size()-3);
        unit = LengthUnit::RootEm;
    }
    else if(EndsWithCSS(number_text,"px")) {
        number_text.resize(number_text.size()-2);
        unit = LengthUnit::Pixels;
    }
    else if(EndsWithCSS(number_text,"vw")) {
        number_text.resize(number_text.size()-2);
        unit = LengthUnit::ViewWidth;
    }
    else if(EndsWithCSS(number_text,"vh")) {
        number_text.resize(number_text.size()-2);
        unit = LengthUnit::ViewHeight;
    }
    else if(EndsWithCSS(number_text,"em")) {
        number_text.resize(number_text.size()-2);
        unit = LengthUnit::Em;
    }
    else if(EndsWithCSS(number_text,"%")) {
        number_text.resize(number_text.size()-1);
        unit = LengthUnit::Percent;
    }

    auto number = ParseStrictCSSFloat(TrimCSS(number_text));
    if(!number.has_value()) return std::nullopt;
    return CSSLength{*number,unit};
}

std::optional<CSSLength> ParseCSSFontSizeLength(
    std::string value
) {
    auto length = ParseCSSLength(std::move(value));
    if(!length.has_value() || length->unit == LengthUnit::Auto) return std::nullopt;
    return length;
}

std::optional<float> ParseCSSOpacity(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(EndsWithCSS(value,"%")) {
        value.resize(value.size() - 1);

        auto percentage =
            ParseStrictCSSFloat(TrimCSS(value));

        if(!percentage.has_value()) {
            return std::nullopt;
        }

        return std::clamp(
            *percentage / 100.0f,
            0.0f,
            1.0f
        );
    }

    auto number = ParseStrictCSSFloat(value);

    if(!number.has_value()) {
        return std::nullopt;
    }

    return std::clamp(
        *number,
        0.0f,
        1.0f
    );
}

std::optional<bool> ParseCSSBoolean(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(
        value == "true" ||
        value == "yes" ||
        value == "on" ||
        value == "1"
    ) {
        return true;
    }

    if(
        value == "false" ||
        value == "no" ||
        value == "off" ||
        value == "0"
    ) {
        return false;
    }

    return std::nullopt;
}

std::vector<std::string> SplitCSSArguments(
    const std::string& arguments
) {
    std::vector<std::string> result;
    std::size_t cursor = 0;

    while(cursor <= arguments.size()) {
        std::size_t comma =
            arguments.find(',',cursor);

        result.push_back(
            TrimCSS(
                arguments.substr(
                    cursor,
                    comma == std::string::npos
                        ? std::string::npos
                        : comma - cursor
                )
            )
        );

        if(comma == std::string::npos) {
            break;
        }

        cursor = comma + 1;
    }

    return result;
}

std::vector<std::string> SplitCSSWhitespaceValues(
    const std::string& value
) {
    std::vector<std::string> result;
    std::string token;
    int parenthesis_depth = 0;

    for(char character : value) {
        if(character == '(') {
            parenthesis_depth++;
            token.push_back(character);
            continue;
        }

        if(character == ')') {
            parenthesis_depth = std::max(0,parenthesis_depth - 1);
            token.push_back(character);
            continue;
        }

        if(
            std::isspace(static_cast<unsigned char>(character)) &&
            parenthesis_depth == 0
        ) {
            if(!token.empty()) {
                result.push_back(TrimCSS(token));
                token.clear();
            }

            continue;
        }

        token.push_back(character);
    }

    if(!token.empty()) {
        result.push_back(TrimCSS(token));
    }

    return result;
}

bool IsCSSBorderStyleValue(const std::string& raw) {
    const std::string value = LowerCSS(TrimCSS(raw));
    return
        value == "none" ||
        value == "solid" ||
        value == "dashed" ||
        value == "dotted" ||
        value == "double";
}

std::optional<std::array<CSSLength,4>> ParseCSSBoxShorthand(
    const std::string& value
) {
    auto tokens = SplitCSSWhitespaceValues(value);

    if(tokens.empty() || tokens.size() > 4) {
        return std::nullopt;
    }

    std::vector<CSSLength> lengths;
    lengths.reserve(tokens.size());

    for(const auto& token : tokens) {
        auto length = ParseCSSLength(token);

        if(!length.has_value()) {
            return std::nullopt;
        }

        lengths.push_back(*length);
    }

    if(lengths.size() == 1) {
        return std::array<CSSLength,4>{
            lengths[0],
            lengths[0],
            lengths[0],
            lengths[0]
        };
    }

    if(lengths.size() == 2) {
        return std::array<CSSLength,4>{
            lengths[0],
            lengths[1],
            lengths[0],
            lengths[1]
        };
    }

    if(lengths.size() == 3) {
        return std::array<CSSLength,4>{
            lengths[0],
            lengths[1],
            lengths[2],
            lengths[1]
        };
    }

    return std::array<CSSLength,4>{
        lengths[0],
        lengths[1],
        lengths[2],
        lengths[3]
    };
}

std::optional<int> ParsePositiveCSSInteger(
    const std::string& value
) {
    auto number = ParseStrictCSSFloat(TrimCSS(value));

    if(!number.has_value()) {
        return std::nullopt;
    }

    int integer = static_cast<int>(std::round(*number));

    if(integer < 1) {
        return std::nullopt;
    }

    return integer;
}

std::optional<Color> ParseCSSColor(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(value == "white") return WHITE;
    if(value == "black") return BLACK;
    if(value == "red") return RED;
    if(value == "green") return GREEN;
    if(value == "blue") return BLUE;
    if(value == "yellow") return YELLOW;
    if(value == "orange") return ORANGE;
    if(value == "purple") return PURPLE;
    if(value == "pink") return PINK;
    if(value == "gray" || value == "grey") return GRAY;
    if(value == "darkgray" || value == "darkgrey") return DARKGRAY;
    if(value == "lightgray" || value == "lightgrey") return LIGHTGRAY;
    if(value == "transparent") return BLANK;

    if(
        (value.size() == 7 || value.size() == 9) &&
        value[0] == '#'
    ) {
        try {
            unsigned long parsed =
                std::stoul(
                    value.substr(1),
                    nullptr,
                    16
                );

            if(value.size() == 7) {
                return Color{
                    static_cast<unsigned char>(
                        (parsed >> 16) & 0xff
                    ),
                    static_cast<unsigned char>(
                        (parsed >> 8) & 0xff
                    ),
                    static_cast<unsigned char>(
                        parsed & 0xff
                    ),
                    255
                };
            }

            return Color{
                static_cast<unsigned char>(
                    (parsed >> 24) & 0xff
                ),
                static_cast<unsigned char>(
                    (parsed >> 16) & 0xff
                ),
                static_cast<unsigned char>(
                    (parsed >> 8) & 0xff
                ),
                static_cast<unsigned char>(
                    parsed & 0xff
                )
            };
        }
        catch(...) {
            return std::nullopt;
        }
    }

    bool is_rgb =
        value.size() > 5 &&
        value.substr(0,4) == "rgb(" &&
        value.back() == ')';

    bool is_rgba =
        value.size() > 6 &&
        value.substr(0,5) == "rgba(" &&
        value.back() == ')';

    if(is_rgb || is_rgba) {
        std::size_t opening =
            value.find('(');

        auto arguments = SplitCSSArguments(
            value.substr(
                opening + 1,
                value.size() - opening - 2
            )
        );

        std::size_t expected =
            is_rgba ? 4 : 3;

        if(arguments.size() != expected) {
            return std::nullopt;
        }

        auto red =
            ParseStrictCSSFloat(arguments[0]);

        auto green =
            ParseStrictCSSFloat(arguments[1]);

        auto blue =
            ParseStrictCSSFloat(arguments[2]);

        if(
            !red.has_value() ||
            !green.has_value() ||
            !blue.has_value()
        ) {
            return std::nullopt;
        }

        float alpha = 255.0f;

        if(is_rgba) {
            auto parsed_alpha =
                ParseStrictCSSFloat(arguments[3]);

            if(!parsed_alpha.has_value()) {
                return std::nullopt;
            }

            alpha =
                *parsed_alpha <= 1.0f
                    ? *parsed_alpha * 255.0f
                    : *parsed_alpha;
        }

        return Color{
            static_cast<unsigned char>(
                std::clamp(*red,0.0f,255.0f)
            ),
            static_cast<unsigned char>(
                std::clamp(*green,0.0f,255.0f)
            ),
            static_cast<unsigned char>(
                std::clamp(*blue,0.0f,255.0f)
            ),
            static_cast<unsigned char>(
                std::clamp(alpha,0.0f,255.0f)
            )
        };
    }

    return std::nullopt;
}

std::optional<GridTrackSize> ParseGridTrackSize(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));
    if(value == "auto") return GridTrackSize{1.0f,GridTrackUnit::Auto};

    GridTrackUnit unit = GridTrackUnit::Pixels;
    if(EndsWithCSS(value,"fr")) {
        value.resize(value.size()-2);
        unit = GridTrackUnit::Fraction;
    }
    else if(EndsWithCSS(value,"px")) {
        value.resize(value.size()-2);
        unit = GridTrackUnit::Pixels;
    }
    else if(EndsWithCSS(value,"%")) {
        value.resize(value.size()-1);
        unit = GridTrackUnit::Percent;
    }

    auto number = ParseStrictCSSFloat(TrimCSS(value));
    if(!number.has_value() || *number < 0.0f) return std::nullopt;
    if(unit == GridTrackUnit::Fraction && *number <= 0.0f) return std::nullopt;
    return GridTrackSize{*number,unit};
}

std::optional<GridTrackList> ParseGridTrackList(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));
    if(value.empty()) return std::nullopt;

    // Keep the old shorthand: `grid-template-columns: 3` means three equal tracks.
    if(auto count = ParsePositiveCSSInteger(value)) {
        return GridTrackList(
            static_cast<std::size_t>(*count),
            GridTrackSize{1.0f,GridTrackUnit::Fraction}
        );
    }

    GridTrackList result;
    for(const std::string& token : SplitCSSWhitespaceValues(value)) {
        if(token.rfind("repeat(",0) == 0 && token.back() == ')') {
            std::string arguments = token.substr(7,token.size()-8);
            std::size_t comma = arguments.find(',');
            if(comma == std::string::npos) return std::nullopt;

            auto count = ParsePositiveCSSInteger(TrimCSS(arguments.substr(0,comma)));
            auto repeated = ParseGridTrackList(TrimCSS(arguments.substr(comma+1)));
            if(!count.has_value() || !repeated.has_value()) return std::nullopt;

            for(int repetition=0; repetition<*count; ++repetition) {
                result.insert(result.end(),repeated->begin(),repeated->end());
            }
        }
        else {
            auto track = ParseGridTrackSize(token);
            if(!track.has_value()) return std::nullopt;
            result.push_back(*track);
        }
    }

    if(result.empty()) return std::nullopt;
    return result;
}

std::optional<PropertyType> ParsePropertyType(
    const std::string& raw
) {
    std::string value =
        LowerCSS(TrimCSS(raw));

    if(
        value == "show-step-buttons" ||
        value == "input-step-buttons"
    ) {
        return PropertyType::InputStepButtons;
    }

    if(
        value == "step-size" ||
        value == "input-step"
    ) {
        return PropertyType::InputStep;
    }

    if(
        value == "show-value-box" ||
        value == "input-value-box"
    ) {
        return PropertyType::InputValueBox;
    }

    // Legacy CSS Grid name. Modern CSS spells this as `gap`.
    if(value == "grid-gap") {
        return PropertyType::Gap;
    }

    for(
        std::size_t index = 0;
        index < property_names.size();
        index++
    ) {
        if(value == property_names[index]) {
            return static_cast<PropertyType>(index);
        }
    }

    return std::nullopt;
}

bool IsLengthProperty(
    PropertyType property
) {
    switch(property) {
        case PropertyType::BorderWidth:
        case PropertyType::BorderTopWidth:
        case PropertyType::BorderRightWidth:
        case PropertyType::BorderBottomWidth:
        case PropertyType::BorderLeftWidth:
        case PropertyType::Width:
        case PropertyType::Height:
        case PropertyType::MinWidth:
        case PropertyType::MaxWidth:
        case PropertyType::MinHeight:
        case PropertyType::MaxHeight:
        case PropertyType::Padding:
        case PropertyType::PaddingLeft:
        case PropertyType::PaddingRight:
        case PropertyType::PaddingTop:
        case PropertyType::PaddingBottom:
        case PropertyType::Margin:
        case PropertyType::MarginLeft:
        case PropertyType::MarginRight:
        case PropertyType::MarginTop:
        case PropertyType::MarginBottom:
        case PropertyType::FontSize:
        case PropertyType::Gap:
        case PropertyType::RowGap:
        case PropertyType::ColumnGap:
        case PropertyType::Left:
        case PropertyType::Right:
        case PropertyType::Top:
        case PropertyType::Bottom:
        case PropertyType::FlexBasis:
        case PropertyType::ScrollbarSize:
        case PropertyType::HeaderHeight:
        case PropertyType::ItemHeight:
        case PropertyType::PopupWidth:
        case PropertyType::PopupMaxHeight:
        case PropertyType::Indent:
        case PropertyType::SplitterSize:
        case PropertyType::TabHeight:
        case PropertyType::TransformOriginX:
        case PropertyType::TransformOriginY:
            return true;

        default:
            return false;
    }
}

std::optional<PropertyValueType> ParsePropertyValue(
    PropertyType property,
    const std::string& raw
) {
    std::string value =
        LowerCSS(TrimCSS(raw));

    // Non-length var() values are resolved after selector/custom-property
    // cascade. Lengths keep var() in their parsed expression AST instead.
    if(
        value.find("var(") != std::string::npos &&
        !IsLengthProperty(property) &&
        property != PropertyType::FontSize
    ) {
        return PropertyValueType(UIDeferredCSSValue{TrimCSS(raw)});
    }

    if(property == PropertyType::BorderRadius) {
        // Historically RUI exposed raylib's normalized 0..1 roundness
        // directly. Keep unitless values compatible, but distinguish explicit
        // CSS pixels so `1px` does not collide with legacy roundness `1`.
        if(EndsWithCSS(value,"px")) {
            value.resize(value.size()-2);
            const auto pixels = ParseStrictCSSFloat(TrimCSS(value));
            if(pixels.has_value() && *pixels >= 0.0f) {
                return PropertyValueType(-*pixels);
            }
            return std::nullopt;
        }

        const auto roundness = ParseStrictCSSFloat(value);
        if(roundness.has_value() && *roundness >= 0.0f) {
            return PropertyValueType(*roundness);
        }
        return std::nullopt;
    }

    if(property == PropertyType::Scale) {
        float multiplier = 1.0f;

        if(EndsWithCSS(value,"%")) {
            value.resize(value.size() - 1);
            multiplier = 0.01f;
        }

        auto number = ParseStrictCSSFloat(TrimCSS(value));

        if(number.has_value() && *number > 0.0f) {
            return PropertyValueType(
                std::clamp(*number * multiplier,0.01f,100.0f)
            );
        }

        return std::nullopt;
    }
    if(
        property == PropertyType::InputStepButtons ||
        property == PropertyType::InputValueBox ||
        property == PropertyType::InputSliderShowValue ||
        property == PropertyType::InputSliderClickToSet ||
        property == PropertyType::RuntimeMovable ||
        property == PropertyType::RuntimeResizable ||
        property == PropertyType::RuntimeConstrain ||
        property == PropertyType::RuntimeLockX ||
        property == PropertyType::RuntimeLockY ||
        property == PropertyType::ResizeToContent ||
        property == PropertyType::Resizable ||
        property == PropertyType::Closable ||
        property == PropertyType::Collapsible ||
        property == PropertyType::ImageFlipX ||
        property == PropertyType::ImageFlipY ||
        property == PropertyType::ImagePixelSnap ||
        property == PropertyType::TabClosable
    ) {
        auto boolean = ParseCSSBoolean(value);

        if(boolean.has_value()) {
            return PropertyValueType(*boolean);
        }

        return std::nullopt;
    }

    if(
        property == PropertyType::InputStep ||
        property == PropertyType::InputDragSpeed ||
        property == PropertyType::RuntimeSnap ||
        property == PropertyType::ScrollSpeed ||
        property == PropertyType::TooltipDelay
    ) {
        auto number = ParseCSSNumber(value);

        if(number.has_value() && *number > 0.0f) {
            return PropertyValueType(*number);
        }

        return std::nullopt;
    }

    if(property == PropertyType::ImageRotation ||
        property == PropertyType::ImageTileOffsetX ||
        property == PropertyType::ImageTileOffsetY
    ) {
        const auto number = ParseCSSNumber(value);
        if(number.has_value()) return PropertyValueType(*number);
        return std::nullopt;
    }

    if(property == PropertyType::InputSliderValueWidth) {
        auto number = ParseCSSNumber(value);

        if(number.has_value() && *number >= 0.0f) {
            return PropertyValueType(*number);
        }

        return std::nullopt;
    }

    if(
        property == PropertyType::ObjectPositionX ||
        property == PropertyType::ObjectPositionY ||
        property == PropertyType::ImagePivotX ||
        property == PropertyType::ImagePivotY
    ) {
        float multiplier = 1.0f;
        if(EndsWithCSS(value,"%")) {
            value.resize(value.size()-1);
            multiplier = 0.01f;
        }
        const auto number = ParseStrictCSSFloat(TrimCSS(value));
        if(!number.has_value()) return std::nullopt;
        return PropertyValueType(std::clamp(*number*multiplier,0.0f,1.0f));
    }

    if(
        property == PropertyType::ImageFrameWidth ||
        property == PropertyType::ImageFrameHeight ||
        property == PropertyType::ImageSliceLeft ||
        property == PropertyType::ImageSliceRight ||
        property == PropertyType::ImageSliceTop ||
        property == PropertyType::ImageSliceBottom
    ) {
        const auto number = ParseCSSNumber(value);
        if(number.has_value() && *number >= 0.0f) return PropertyValueType(*number);
        return std::nullopt;
    }

    if(
        property == PropertyType::ImageSliceScale ||
        property == PropertyType::ImageTileScale
    ) {
        const auto number = ParseCSSNumber(value);
        if(number.has_value() && *number > 0.0f) return PropertyValueType(*number);
        return std::nullopt;
    }

    if(
        property == PropertyType::ImageFrame ||
        property == PropertyType::ImageFrameColumns
    ) {
        const auto number = ParseCSSNumber(value);
        if(number.has_value() && *number >= 0.0f) {
            return PropertyValueType(static_cast<int>(std::floor(*number)));
        }
        return std::nullopt;
    }

    if(property == PropertyType::FontSize) {
        auto size = ParseCSSFontSizeLength(raw);
        if(size.has_value()) return PropertyValueType(*size);
        return std::nullopt;
    }

    if(property == PropertyType::FontName) {
        std::string font_name = TrimCSS(raw);

        if(font_name.size() >= 2) {
            char first = font_name.front();
            char last = font_name.back();
            if((first == '"' && last == '"') ||
               (first == '\'' && last == '\'')) {
                font_name = font_name.substr(1,font_name.size()-2);
            }
        }

        font_name = TrimCSS(font_name);
        if(font_name.empty()) return std::nullopt;
        return PropertyValueType(font_name);
    }

    if(
        property == PropertyType::BackgroundColor ||
        property == PropertyType::BorderColor ||
        property == PropertyType::TextColor ||
        property == PropertyType::ImageTint
    ) {
        auto color = ParseCSSColor(value);

        if(color.has_value()) {
            return PropertyValueType(*color);
        }

        return std::nullopt;
    }

    if(property == PropertyType::BorderStyle) {
        if(IsCSSBorderStyleValue(value)) {
            return PropertyValueType(value);
        }
        return std::nullopt;
    }

    if(property == PropertyType::Content) {
        std::string content = TrimCSS(raw);
        const std::string lowered = LowerCSS(content);
        if(lowered == "none" || lowered == "normal") {
            return PropertyValueType(UIGeneratedContentValue{{},false});
        }
        if(content.size() >= 2 &&
           ((content.front()=='"' && content.back()=='"') ||
            (content.front()=='\'' && content.back()=='\''))) {
            content = content.substr(1,content.size()-2);
        }
        return PropertyValueType(UIGeneratedContentValue{std::move(content),true});
    }

    if(property == PropertyType::MixBlendMode) {
        if(value=="normal") return PropertyValueType(UIMixBlendMode::Normal);
        if(value=="alpha") return PropertyValueType(UIMixBlendMode::Alpha);
        if(value=="add" || value=="additive") return PropertyValueType(UIMixBlendMode::Additive);
        if(value=="multiply") return PropertyValueType(UIMixBlendMode::Multiply);
        if(value=="add-colors" || value=="add-colours") return PropertyValueType(UIMixBlendMode::AddColors);
        if(value=="subtract" || value=="subtract-colors" || value=="subtract-colours") return PropertyValueType(UIMixBlendMode::SubtractColors);
        if(value=="premultiplied" || value=="alpha-premultiply") return PropertyValueType(UIMixBlendMode::AlphaPremultiply);
        return std::nullopt;
    }

    if(property == PropertyType::DisplayType) {
        if(value == "inline") {
            return PropertyValueType(
                DisplayType::Inline
            );
        }

        if(value == "block") {
            return PropertyValueType(
                DisplayType::Block
            );
        }

        if(value == "flex") {
            return PropertyValueType(
                DisplayType::Flex
            );
        }

        if(value == "grid") {
            return PropertyValueType(
                DisplayType::Grid
            );
        }

        if(value == "none") {
            return PropertyValueType(
                DisplayType::None
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::ObjectFit) {
        if(value == "stretch" || value == "fill") return PropertyValueType(UIImageFit::Stretch);
        if(value == "contain") return PropertyValueType(UIImageFit::Contain);
        if(value == "cover") return PropertyValueType(UIImageFit::Cover);
        if(value == "center" || value == "none") return PropertyValueType(UIImageFit::Center);
        if(value == "tile" || value == "repeat") return PropertyValueType(UIImageFit::Tile);
        if(value == "repeat-x" || value == "tile-x") return PropertyValueType(UIImageFit::TileX);
        if(value == "repeat-y" || value == "tile-y") return PropertyValueType(UIImageFit::TileY);
        return std::nullopt;
    }

    if(property == PropertyType::ImageRendering) {
        if(value == "pixelated" || value == "crisp-edges" || value == "nearest") {
            return PropertyValueType(UIImageSampling::Nearest);
        }
        if(value == "auto" || value == "smooth" || value == "bilinear") {
            return PropertyValueType(UIImageSampling::Bilinear);
        }
        return std::nullopt;
    }

    if(property == PropertyType::ImageSliceMode) {
        if(value == "stretch") return PropertyValueType(UIImageSliceMode::Stretch);
        if(value == "tile" || value == "repeat") return PropertyValueType(UIImageSliceMode::Tile);
        return std::nullopt;
    }

    if(property == PropertyType::ImageBlendMode) {
        if(value == "alpha" || value == "normal") return PropertyValueType(UIImageBlendMode::Alpha);
        if(value == "add" || value == "additive") return PropertyValueType(UIImageBlendMode::Additive);
        if(value == "multiply" || value == "multiplied") return PropertyValueType(UIImageBlendMode::Multiplied);
        if(value == "add-colors" || value == "add-colours") return PropertyValueType(UIImageBlendMode::AddColors);
        if(value == "subtract" || value == "subtract-colors" || value == "subtract-colours") return PropertyValueType(UIImageBlendMode::SubtractColors);
        if(value == "premultiplied" || value == "alpha-premultiply") return PropertyValueType(UIImageBlendMode::AlphaPremultiply);
        return std::nullopt;
    }

    if(property == PropertyType::Position) {
        if(value == "static") {
            return PropertyValueType(
                PositionType::Static
            );
        }

        if(value == "relative") {
            return PropertyValueType(
                PositionType::Relative
            );
        }

        if(value == "absolute") {
            return PropertyValueType(
                PositionType::Absolute
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::FlexDirection) {
        if(value == "row") {
            return PropertyValueType(
                FlexDirection::Row
            );
        }

        if(value == "column") {
            return PropertyValueType(
                FlexDirection::Column
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::FlexWrap) {
        if(value == "nowrap") return PropertyValueType(FlexWrap::NoWrap);
        if(value == "wrap") return PropertyValueType(FlexWrap::Wrap);
        if(value == "wrap-reverse") return PropertyValueType(FlexWrap::WrapReverse);
        return std::nullopt;
    }

    if(property == PropertyType::JustifyContent) {
        if(value == "flex-start" || value == "start") {
            return PropertyValueType(
                JustifyContent::FlexStart
            );
        }

        if(value == "center") {
            return PropertyValueType(
                JustifyContent::Center
            );
        }

        if(value == "flex-end" || value == "end") {
            return PropertyValueType(
                JustifyContent::FlexEnd
            );
        }

        if(value == "space-between") {
            return PropertyValueType(
                JustifyContent::SpaceBetween
            );
        }

        if(value == "space-around") {
            return PropertyValueType(
                JustifyContent::SpaceAround
            );
        }

        if(value == "space-evenly") {
            return PropertyValueType(
                JustifyContent::SpaceEvenly
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::AlignItems || property == PropertyType::JustifyItems) {
        if(value == "stretch") {
            return PropertyValueType(
                AlignItems::Stretch
            );
        }

        if(value == "flex-start" || value == "start") {
            return PropertyValueType(
                AlignItems::FlexStart
            );
        }

        if(value == "center") {
            return PropertyValueType(
                AlignItems::Center
            );
        }

        if(value == "flex-end" || value == "end") {
            return PropertyValueType(
                AlignItems::FlexEnd
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::AlignSelf) {
        if(value == "auto") {
            return PropertyValueType(
                AlignSelf::Auto
            );
        }

        if(value == "stretch") {
            return PropertyValueType(
                AlignSelf::Stretch
            );
        }

        if(value == "flex-start" || value == "start") {
            return PropertyValueType(
                AlignSelf::FlexStart
            );
        }

        if(value == "center") {
            return PropertyValueType(
                AlignSelf::Center
            );
        }

        if(value == "flex-end" || value == "end") {
            return PropertyValueType(
                AlignSelf::FlexEnd
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::AlignContent) {
        if(value == "normal" || value == "stretch") {
            return PropertyValueType(AlignContent::Stretch);
        }
        if(value == "flex-start" || value == "start") {
            return PropertyValueType(AlignContent::FlexStart);
        }
        if(value == "center") return PropertyValueType(AlignContent::Center);
        if(value == "flex-end" || value == "end") {
            return PropertyValueType(AlignContent::FlexEnd);
        }
        if(value == "space-between") return PropertyValueType(AlignContent::SpaceBetween);
        if(value == "space-around") return PropertyValueType(AlignContent::SpaceAround);
        if(value == "space-evenly") return PropertyValueType(AlignContent::SpaceEvenly);
        return std::nullopt;
    }

    if(property == PropertyType::JustifySelf) {
        if(value == "auto") return PropertyValueType(JustifySelf::Auto);
        if(value == "normal" || value == "stretch") {
            return PropertyValueType(JustifySelf::Stretch);
        }
        if(value == "flex-start" || value == "start") {
            return PropertyValueType(JustifySelf::FlexStart);
        }
        if(value == "center") return PropertyValueType(JustifySelf::Center);
        if(value == "flex-end" || value == "end") {
            return PropertyValueType(JustifySelf::FlexEnd);
        }
        return std::nullopt;
    }

    if(property == PropertyType::TextAlign) {
        if(value == "left" || value == "start") {
            return PropertyValueType(
                TextAlign::Left
            );
        }

        if(value == "center") {
            return PropertyValueType(
                TextAlign::Center
            );
        }

        if(value == "right" || value == "end") {
            return PropertyValueType(
                TextAlign::Right
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::Overflow) {
        if(value == "visible") {
            return PropertyValueType(
                OverflowType::Visible
            );
        }

        if(value == "hidden" || value == "clip") {
            return PropertyValueType(
                OverflowType::Hidden
            );
        }

        if(value == "auto") {
            return PropertyValueType(
                OverflowType::Auto
            );
        }

        if(value == "scroll") {
            return PropertyValueType(
                OverflowType::Scroll
            );
        }

        return std::nullopt;
    }

    if(property == PropertyType::Opacity) {
        auto opacity = ParseCSSOpacity(value);

        if(opacity.has_value()) {
            return PropertyValueType(*opacity);
        }

        return std::nullopt;
    }

    if(
        property == PropertyType::GridTemplateColumns ||
        property == PropertyType::GridTemplateRows ||
        property == PropertyType::GridAutoColumns ||
        property == PropertyType::GridAutoRows
    ) {
        auto tracks = ParseGridTrackList(value);

        if(tracks.has_value()) {
            return PropertyValueType(*tracks);
        }

        return std::nullopt;
    }

    if(property == PropertyType::GridAutoFlow) {
        if(value == "row") return PropertyValueType(GridAutoFlow::Row);
        if(value == "column") return PropertyValueType(GridAutoFlow::Column);
        return std::nullopt;
    }

    if(
        property == PropertyType::GridColumn ||
        property == PropertyType::GridRow ||
        property == PropertyType::GridColumnSpan ||
        property == PropertyType::GridRowSpan
    ) {
        auto number = ParseStrictCSSFloat(value);

        if(number.has_value()) {
            return PropertyValueType(
                static_cast<int>(
                    std::round(*number)
                )
            );
        }

        return std::nullopt;
    }

    if(IsLengthProperty(property)) {
        const bool is_margin_property =
            property == PropertyType::Margin ||
            property == PropertyType::MarginLeft ||
            property == PropertyType::MarginRight ||
            property == PropertyType::MarginTop ||
            property == PropertyType::MarginBottom;

        if(
            value == "auto" &&
            property != PropertyType::Width &&
            property != PropertyType::Height &&
            property != PropertyType::FlexBasis &&
            !is_margin_property
        ) {
            return std::nullopt;
        }

        auto length = ParseCSSLength(value);

        if(length.has_value()) {
            return PropertyValueType(*length);
        }

        return std::nullopt;
    }

    auto number = ParseCSSNumber(value);

    if(number.has_value()) {
        return PropertyValueType(*number);
    }

    return std::nullopt;
}

bool AppendGridPlacementDeclarations(
    std::vector<Declaration>& declarations,
    PropertyType property,
    const std::string& raw
) {
    std::string value = LowerCSS(TrimCSS(raw));
    PropertyType span_property =
        property == PropertyType::GridColumn
            ? PropertyType::GridColumnSpan
            : PropertyType::GridRowSpan;

    std::size_t slash = value.find('/');
    std::string start_text = TrimCSS(
        value.substr(0,slash)
    );
    std::string end_text = slash == std::string::npos
        ? std::string{}
        : TrimCSS(value.substr(slash + 1));

    auto parse_span = [](const std::string& text) -> std::optional<int> {
        std::string token = LowerCSS(TrimCSS(text));

        if(token.rfind("span",0) != 0) {
            return std::nullopt;
        }

        return ParsePositiveCSSInteger(
            TrimCSS(token.substr(4))
        );
    };

    if(auto span = parse_span(start_text)) {
        if(slash != std::string::npos) {
            return false;
        }

        declarations.emplace_back(
            span_property,
            PropertyValueType(*span)
        );
        return true;
    }

    auto start = ParsePositiveCSSInteger(start_text);

    if(!start.has_value()) {
        return false;
    }

    declarations.emplace_back(
        property,
        PropertyValueType(*start)
    );

    if(end_text.empty()) {
        return true;
    }

    if(auto span = parse_span(end_text)) {
        declarations.emplace_back(
            span_property,
            PropertyValueType(*span)
        );
        return true;
    }

    auto end = ParsePositiveCSSInteger(end_text);

    if(!end.has_value() || *end <= *start) {
        return false;
    }

    declarations.emplace_back(
        span_property,
        PropertyValueType(*end - *start)
    );
    return true;
}

namespace {

bool AppendBoxSideDeclarations(
    std::vector<Declaration>& declarations,
    const std::array<CSSLength,4>& values,
    PropertyType top,
    PropertyType right,
    PropertyType bottom,
    PropertyType left
) {
    declarations.emplace_back(top,PropertyValueType(values[0]));
    declarations.emplace_back(right,PropertyValueType(values[1]));
    declarations.emplace_back(bottom,PropertyValueType(values[2]));
    declarations.emplace_back(left,PropertyValueType(values[3]));
    return true;
}

bool AppendGapShorthand(
    std::vector<Declaration>& declarations,
    const std::string& raw
) {
    const auto tokens = SplitCSSWhitespaceValues(raw);
    if(tokens.empty() || tokens.size() > 2) return false;

    auto parse_gap = [](const std::string& token) -> std::optional<CSSLength> {
        if(LowerCSS(TrimCSS(token)) == "normal") {
            return CSSLength{0.0f,LengthUnit::Pixels};
        }
        return ParseCSSLength(token);
    };

    auto row = parse_gap(tokens[0]);
    if(!row.has_value() || row->unit == LengthUnit::Auto) return false;

    if(tokens.size() == 1) {
        // Preserve the original Gap property so transition: gap and the
        // existing computed-style fallback continue to behave as before.
        declarations.emplace_back(PropertyType::Gap,PropertyValueType(*row));
        return true;
    }

    auto column = parse_gap(tokens[1]);
    if(!column.has_value() || column->unit == LengthUnit::Auto) return false;
    declarations.emplace_back(PropertyType::RowGap,PropertyValueType(*row));
    declarations.emplace_back(PropertyType::ColumnGap,PropertyValueType(*column));
    return true;
}

bool AppendFlexShorthand(
    std::vector<Declaration>& declarations,
    const std::string& raw
) {
    const std::string value = LowerCSS(TrimCSS(raw));
    float grow = 0.0f;
    float shrink = 1.0f;
    CSSLength basis{0.0f,LengthUnit::Percent};

    if(value == "none") {
        grow = 0.0f;
        shrink = 0.0f;
        basis = CSSLength{0.0f,LengthUnit::Auto};
    }
    else if(value == "auto") {
        grow = 1.0f;
        shrink = 1.0f;
        basis = CSSLength{0.0f,LengthUnit::Auto};
    }
    else if(value == "initial") {
        grow = 0.0f;
        shrink = 1.0f;
        basis = CSSLength{0.0f,LengthUnit::Auto};
    }
    else {
        const auto tokens = SplitCSSWhitespaceValues(raw);
        if(tokens.empty() || tokens.size() > 3) return false;

        const auto first_number = ParseStrictCSSFloat(TrimCSS(tokens[0]));
        if(tokens.size() == 1) {
            if(first_number.has_value()) {
                if(*first_number < 0.0f) return false;
                grow = *first_number;
                shrink = 1.0f;
                basis = CSSLength{0.0f,LengthUnit::Percent};
            }
            else {
                auto parsed_basis = ParseCSSLength(tokens[0]);
                if(!parsed_basis.has_value()) return false;
                grow = 1.0f;
                shrink = 1.0f;
                basis = *parsed_basis;
            }
        }
        else if(tokens.size() == 2) {
            if(!first_number.has_value() || *first_number < 0.0f) return false;
            grow = *first_number;
            const auto second_number = ParseStrictCSSFloat(TrimCSS(tokens[1]));
            if(second_number.has_value()) {
                if(*second_number < 0.0f) return false;
                shrink = *second_number;
                basis = CSSLength{0.0f,LengthUnit::Percent};
            }
            else {
                auto parsed_basis = ParseCSSLength(tokens[1]);
                if(!parsed_basis.has_value()) return false;
                shrink = 1.0f;
                basis = *parsed_basis;
            }
        }
        else {
            const auto second_number = ParseStrictCSSFloat(TrimCSS(tokens[1]));
            auto parsed_basis = ParseCSSLength(tokens[2]);
            if(!first_number.has_value() || !second_number.has_value() ||
               !parsed_basis.has_value() || *first_number < 0.0f ||
               *second_number < 0.0f) {
                return false;
            }
            grow = *first_number;
            shrink = *second_number;
            basis = *parsed_basis;
        }
    }

    declarations.emplace_back(PropertyType::FlexGrow,PropertyValueType(grow));
    declarations.emplace_back(PropertyType::FlexShrink,PropertyValueType(shrink));
    declarations.emplace_back(PropertyType::FlexBasis,PropertyValueType(basis));
    return true;
}

bool AppendFlexFlowShorthand(
    std::vector<Declaration>& declarations,
    const std::string& raw
) {
    const auto tokens = SplitCSSWhitespaceValues(raw);
    if(tokens.empty() || tokens.size() > 2) return false;

    // CSS flex-flow defaults omitted components to `row nowrap`. RUI keeps its
    // historical default column direction unless the shorthand itself is used.
    FlexDirection direction = FlexDirection::Row;
    FlexWrap wrap = FlexWrap::NoWrap;
    bool direction_seen = false;
    bool wrap_seen = false;

    for(const std::string& token : tokens) {
        if(auto parsed = ParsePropertyValue(PropertyType::FlexDirection,token)) {
            if(direction_seen || !std::holds_alternative<FlexDirection>(*parsed)) return false;
            direction = std::get<FlexDirection>(*parsed);
            direction_seen = true;
            continue;
        }
        if(auto parsed = ParsePropertyValue(PropertyType::FlexWrap,token)) {
            if(wrap_seen || !std::holds_alternative<FlexWrap>(*parsed)) return false;
            wrap = std::get<FlexWrap>(*parsed);
            wrap_seen = true;
            continue;
        }
        return false;
    }

    declarations.emplace_back(PropertyType::FlexDirection,PropertyValueType(direction));
    declarations.emplace_back(PropertyType::FlexWrap,PropertyValueType(wrap));
    return true;
}

bool AppendPlaceItemsShorthand(
    std::vector<Declaration>& declarations,
    const std::string& raw
) {
    const auto tokens = SplitCSSWhitespaceValues(raw);
    if(tokens.empty() || tokens.size() > 2) return false;

    auto align = ParsePropertyValue(PropertyType::AlignItems,tokens[0]);
    auto justify = ParsePropertyValue(
        PropertyType::JustifyItems,
        tokens.size() == 1 ? tokens[0] : tokens[1]
    );
    if(!align.has_value() || !justify.has_value()) return false;

    declarations.emplace_back(PropertyType::AlignItems,*align);
    declarations.emplace_back(PropertyType::JustifyItems,*justify);
    return true;
}

std::vector<std::pair<std::string,std::string>> ParseCSSFunctionSequence(
    const std::string& raw
) {
    std::vector<std::pair<std::string,std::string>> result;
    std::size_t cursor=0;
    while(cursor<raw.size()) {
        while(cursor<raw.size() && std::isspace(static_cast<unsigned char>(raw[cursor]))) ++cursor;
        if(cursor>=raw.size()) break;
        const std::size_t name_begin=cursor;
        while(cursor<raw.size() &&
              (std::isalnum(static_cast<unsigned char>(raw[cursor])) || raw[cursor]=='-' || raw[cursor]=='_')) ++cursor;
        if(cursor==name_begin) return {};
        std::string name=LowerCSS(raw.substr(name_begin,cursor-name_begin));
        while(cursor<raw.size() && std::isspace(static_cast<unsigned char>(raw[cursor]))) ++cursor;
        if(cursor>=raw.size() || raw[cursor]!='(') return {};
        const std::size_t opening=cursor;
        const std::size_t closing=FindMatchingCSSParenthesis(raw,opening);
        if(closing==std::string::npos) return {};
        result.emplace_back(std::move(name),TrimCSS(raw.substr(opening+1,closing-opening-1)));
        cursor=closing+1;
    }
    return result;
}

UIFilterListRef ParseUIFilterListValue(const std::string& raw) {
    const std::string trimmed=TrimCSS(raw);
    if(LowerCSS(trimmed)=="none") return std::make_shared<UIFilterList>();
    auto functions=ParseCSSFunctionSequence(trimmed);
    if(functions.empty()) return {};
    auto result=std::make_shared<UIFilterList>();
    result->operations.reserve(functions.size());
    for(const auto& function:functions) {
        UIFilterOperation operation;
        operation.source=function.second;
        if(function.first=="blur") operation.type=UIFilterType::Blur;
        else if(function.first=="brightness") operation.type=UIFilterType::Brightness;
        else if(function.first=="contrast") operation.type=UIFilterType::Contrast;
        else if(function.first=="saturate" || function.first=="saturation") operation.type=UIFilterType::Saturate;
        else if(function.first=="grayscale" || function.first=="greyscale") operation.type=UIFilterType::Grayscale;
        else if(function.first=="opacity") operation.type=UIFilterType::Opacity;
        else if(function.first=="hue-rotate") operation.type=UIFilterType::HueRotate;
        else {
            TraceLog(LOG_WARNING,"Unknown RCSS filter function '%s'",function.first.c_str());
            return {};
        }
        std::string expression_text=function.second;
        // CSS percentages on scalar color filters are ratios. Convert to a
        // unitless calc expression while preserving var() if present.
        if(operation.type!=UIFilterType::Blur && operation.type!=UIFilterType::HueRotate) {
            const std::string lower=LowerCSS(TrimCSS(expression_text));
            if(EndsWithCSS(lower,"%") && lower.find("var(")==std::string::npos) {
                auto number=ParseStrictCSSFloat(TrimCSS(lower.substr(0,lower.size()-1)));
                if(number) expression_text=std::to_string(*number/100.0f);
            }
        }
        std::string error;
        operation.value=ParseUIStyleExpression(expression_text,&error);
        if(!operation.value) {
            TraceLog(LOG_WARNING,"Invalid RCSS filter value '%s': %s",function.second.c_str(),error.c_str());
            return {};
        }
        result->operations.push_back(std::move(operation));
    }
    return result;
}

UIShaderEffectListRef ParseUIShaderEffectListValue(const std::string& raw) {
    const std::string trimmed=TrimCSS(raw);
    if(LowerCSS(trimmed)=="none") return std::make_shared<UIShaderEffectList>();
    auto functions=ParseCSSFunctionSequence(trimmed);
    if(functions.empty()) return {};
    auto result=std::make_shared<UIShaderEffectList>();
    result->effects.reserve(functions.size());
    for(const auto& function:functions) {
        if(function.first!="shader") {
            TraceLog(LOG_WARNING,"Unknown RCSS effect function '%s'",function.first.c_str());
            return {};
        }
        std::string path=TrimCSS(function.second);
        if(path.size()>=2 &&
           ((path.front()=='"'&&path.back()=='"') || (path.front()=='\''&&path.back()=='\''))) {
            path=path.substr(1,path.size()-2);
        }
        if(path.empty()) return {};
        result->effects.push_back(UIShaderEffect{std::move(path)});
    }
    return result;
}
std::vector<std::string> SplitTransformArguments(
    const std::string& args
) {
    const std::string trimmed = TrimCSS(args);

    if(trimmed.empty()) {
        return {};
    }

    // Prefer comma-separated syntax when commas are present.
    if(trimmed.find(',') != std::string::npos) {
        return SplitCSSArguments(trimmed);
    }

    // Otherwise accept CSS-style whitespace-separated syntax.
    return SplitCSSWhitespaceValues(trimmed);
}
UITransformListRef ParseUITransformListValue(const std::string& raw) {
    const std::string trimmed = TrimCSS(raw);

    if(LowerCSS(trimmed) == "none") {
        return std::make_shared<UITransformList>();
    }

    auto functions = ParseCSSFunctionSequence(trimmed);
    if(functions.empty()) {
        return {};
    }

    auto result = std::make_shared<UITransformList>();

    // Transform functions can use either comma-separated or
    // whitespace-separated arguments.
    auto split_transform_arguments =
        [](const std::string& value) -> std::vector<std::string> {
            const std::string trimmed_value = TrimCSS(value);

            if(trimmed_value.empty()) {
                return {};
            }

            if(trimmed_value.find(',') != std::string::npos) {
                return SplitCSSArguments(trimmed_value);
            }

            return SplitCSSWhitespaceValues(trimmed_value);
        };

    for(const auto& function : functions) {
        auto add =
            [&](UITransformOperationType type,
                const std::string& source,
                bool parse_as_expression = true) -> bool {

            UIStyleExpression expression;

            if(parse_as_expression) {
                std::string error;
                expression = ParseUIStyleExpression(source,&error);

                if(!expression) {
                    TraceLog(
                        LOG_WARNING,
                        "Invalid transform value '%s': %s",
                        source.c_str(),
                        error.c_str()
                    );
                    return false;
                }
            }

            result->operations.push_back(
                UITransformOperation{
                    type,
                    std::move(expression),
                    source
                }
            );

            return true;
        };

        const std::string& name = function.first;
        const std::string& args = function.second;

        // ---- Translation ----

        if(name == "translatex") {
            if(!add(UITransformOperationType::TranslateX,args)) {
                return {};
            }
        }
        else if(name == "translatey") {
            if(!add(UITransformOperationType::TranslateY,args)) {
                return {};
            }
        }
        else if(name == "translate") {
            auto values = split_transform_arguments(args);

            if(values.empty() || values.size() > 3) {
                return {};
            }

            if(!add(
                UITransformOperationType::TranslateX,
                values[0]
            )) {
                return {};
            }

            if(values.size() >= 2) {
                if(!add(
                    UITransformOperationType::TranslateY,
                    values[1]
                )) {
                    return {};
                }
            }
            else {
                if(!add(
                    UITransformOperationType::TranslateY,
                    "0px"
                )) {
                    return {};
                }
            }

            if(values.size() == 3) {
                if(!add(
                    UITransformOperationType::TranslateZ,
                    values[2]
                )) {
                    return {};
                }
            }
        }
        else if(name == "translatez") {
            if(!add(UITransformOperationType::TranslateZ,args)) {
                return {};
            }
        }

        // ---- Scale ----

        else if(name == "scalex") {
            if(!add(UITransformOperationType::ScaleX,args)) {
                return {};
            }
        }
        else if(name == "scaley") {
            if(!add(UITransformOperationType::ScaleY,args)) {
                return {};
            }
        }
        else if(name == "scale") {
            auto values = split_transform_arguments(args);

            if(values.empty() || values.size() > 3) {
                return {};
            }

            if(!add(
                UITransformOperationType::ScaleX,
                values[0]
            )) {
                return {};
            }

            if(values.size() >= 2) {
                if(!add(
                    UITransformOperationType::ScaleY,
                    values[1]
                )) {
                    return {};
                }
            }
            else {
                if(!add(
                    UITransformOperationType::ScaleY,
                    values[0]
                )) {
                    return {};
                }
            }

            if(values.size() == 3) {
                if(!add(
                    UITransformOperationType::ScaleZ,
                    values[2]
                )) {
                    return {};
                }
            }
        }
        else if(name == "scalez") {
            if(!add(UITransformOperationType::ScaleZ,args)) {
                return {};
            }
        }

        // ---- Rotation ----

        else if(name == "rotate") {
            if(!add(UITransformOperationType::Rotate,args)) {
                return {};
            }
        }
        else if(name == "rotatex") {
            if(!add(UITransformOperationType::RotateX,args)) {
                return {};
            }
        }
        else if(name == "rotatey") {
            if(!add(UITransformOperationType::RotateY,args)) {
                return {};
            }
        }
        else if(name == "rotatez") {
            if(!add(UITransformOperationType::RotateZ,args)) {
                return {};
            }
        }

        // ---- Rotate3D ----

        else if(name == "rotate3d") {
            auto values = split_transform_arguments(args);

            if(values.size() != 4) {
                TraceLog(
                    LOG_WARNING,
                    "rotate3d() requires 4 values, got %zu",
                    values.size()
                );
                return {};
            }

            // Keep the complete argument string because the evaluator
            // consumes the axis and angle together.
            if(!add(
                UITransformOperationType::Rotate3D,
                args,
                false
            )) {
                return {};
            }
        }

        // ---- Matrix3D ----

        else if(name == "matrix3d") {
            auto values = split_transform_arguments(args);

            if(values.size() != 16) {
                TraceLog(
                    LOG_WARNING,
                    "matrix3d() requires exactly 16 values, got %zu",
                    values.size()
                );
                return {};
            }

            // Keep the complete argument string for matrix evaluation.
            if(!add(
                UITransformOperationType::Matrix3D,
                args,
                false
            )) {
                return {};
            }
        }

        // ---- Skew ----

        else if(name == "skewx") {
            if(!add(UITransformOperationType::SkewX,args)) {
                return {};
            }
        }
        else if(name == "skewy") {
            if(!add(UITransformOperationType::SkewY,args)) {
                return {};
            }
        }
        else if(name == "skew") {
            auto values = split_transform_arguments(args);

            if(values.empty() || values.size() > 2) {
                return {};
            }

            if(!add(
                UITransformOperationType::SkewX,
                values[0]
            )) {
                return {};
            }

            if(values.size() == 2) {
                if(!add(
                    UITransformOperationType::SkewY,
                    values[1]
                )) {
                    return {};
                }
            }
        }

        else {
            TraceLog(
                LOG_WARNING,
                "Unknown RCSS transform function '%s'",
                name.c_str()
            );
            return {};
        }
    }

    return result;
}
bool AppendFontShorthand(
    std::vector<Declaration>& declarations,
    const std::string& raw
) {
    const std::string value = TrimCSS(raw);
    if(value.empty()) return false;

    // RUI currently owns font-size and font-family, but not browser font-style,
    // font-weight, font-variant, line-height or font-stretch. Keep the shorthand
    // intentionally exact to the subset RUI can render:
    //     font: 1.1em Inter, Arial, default;
    std::size_t split = 0;
    while(split < value.size() &&
          !std::isspace(static_cast<unsigned char>(value[split]))) {
        ++split;
    }
    if(split == value.size()) return false;

    const std::string size_text = value.substr(0,split);
    const std::string family_text = TrimCSS(value.substr(split));
    if(family_text.empty()) return false;

    auto size = ParseCSSFontSizeLength(size_text);
    auto family = ParsePropertyValue(PropertyType::FontName,family_text);
    if(!size.has_value() || !family.has_value()) return false;

    declarations.emplace_back(PropertyType::FontSize,PropertyValueType(*size));
    declarations.emplace_back(PropertyType::FontName,*family);
    return true;
}

} // namespace

bool AppendCSSDeclaration(
    std::vector<Declaration>& declarations,
    PropertyType property,
    const std::string& raw
) {
    if(property == PropertyType::Filter || property == PropertyType::BackdropFilter) {
        auto filters=ParseUIFilterListValue(raw);
        if(!filters) return false;
        declarations.emplace_back(property,PropertyValueType(filters));
        return true;
    }

    if(property == PropertyType::Effect) {
        auto effects=ParseUIShaderEffectListValue(raw);
        if(!effects) return false;
        declarations.emplace_back(property,PropertyValueType(effects));
        return true;
    }

    if(property == PropertyType::Transform) {
        auto transform=ParseUITransformListValue(raw);
        if(!transform) return false;
        declarations.emplace_back(property,PropertyValueType(transform));
        return true;
    }
    if(property == PropertyType::Perspective) {
        std::string value = LowerCSS(TrimCSS(raw));

        // Keywords
        if(value == "none") {
            declarations.emplace_back(property, PropertyValueType(PerspectiveType::None));
            return true;
        }
        if(value == "inherit") {
            declarations.emplace_back(property, PropertyValueType(PerspectiveType::Inherit));
            return true;
        }
        if(value == "initial") {
            declarations.emplace_back(property, PropertyValueType(PerspectiveType::Initial));
            return true;
        }

        // Defer var() – same pattern as other properties
        if(value.find("var(") != std::string::npos) {
            declarations.emplace_back(property, PropertyValueType(UIDeferredCSSValue{value}));
            return true;
        }

        // Static length
        auto length = ParseCSSLength(value);
        if(!length.has_value() || length->unit == LengthUnit::Auto) {
            return false;
        }
        // Optional: reject negative values (CSS spec: perspective length must be > 0)
        if(length->value <= 0.0f) {
            return false;
        }
        declarations.emplace_back(property, PropertyValueType(*length));
        return true;
    }
    if(property == PropertyType::TransformOrigin) {
        const auto tokens = SplitCSSWhitespaceValues(LowerCSS(TrimCSS(raw)));
        if(tokens.empty() || tokens.size() > 2) return false;
        auto resolveKeywordOrLength = [](const std::string& token, bool horizontal) -> std::optional<CSSLength> {
            if(token == "center") return CSSLength{50.0f,LengthUnit::Percent};
            if(horizontal && token == "left") return CSSLength{0.0f,LengthUnit::Percent};
            if(horizontal && token == "right") return CSSLength{100.0f,LengthUnit::Percent};
            if(!horizontal && token == "top") return CSSLength{0.0f,LengthUnit::Percent};
            if(!horizontal && token == "bottom") return CSSLength{100.0f,LengthUnit::Percent};
            return ParseCSSLength(token);
        };
        auto x = resolveKeywordOrLength(tokens[0],true);
        if(!x.has_value()) return false;
        auto y = tokens.size() == 2
            ? resolveKeywordOrLength(tokens[1],false)
            : std::optional<CSSLength>(CSSLength{50.0f,LengthUnit::Percent});
        if(!y.has_value()) return false;
        declarations.emplace_back(PropertyType::TransformOriginX,PropertyValueType(*x));
        declarations.emplace_back(PropertyType::TransformOriginY,PropertyValueType(*y));
        return true;
    }
    if(property == PropertyType::Background) {
        const std::string value = LowerCSS(TrimCSS(raw));
        if(value == "none") {
            declarations.emplace_back(
                PropertyType::BackgroundColor,PropertyValueType(BLANK)
            );
            return true;
        }
        if(value.find("var(") != std::string::npos) {
            declarations.emplace_back(
                PropertyType::BackgroundColor,
                PropertyValueType(UIDeferredCSSValue{TrimCSS(raw)})
            );
            return true;
        }
        auto color = ParseCSSColor(raw);
        if(!color.has_value()) return false;
        declarations.emplace_back(
            PropertyType::BackgroundColor,PropertyValueType(*color)
        );
        return true;
    }

    if(property == PropertyType::Font) {
        return AppendFontShorthand(declarations,raw);
    }

    if(property == PropertyType::Inset) {
        auto values = ParseCSSBoxShorthand(raw);
        if(!values.has_value()) return false;
        // RUI does not yet represent top/right/bottom/left:auto distinctly
        // from an omitted edge, so reject auto instead of silently treating it
        // as zero and changing absolute-position sizing semantics.
        if(std::any_of(values->begin(),values->end(),[](const CSSLength& length) {
            return length.unit == LengthUnit::Auto;
        })) return false;
        return AppendBoxSideDeclarations(
            declarations,*values,
            PropertyType::Top,PropertyType::Right,
            PropertyType::Bottom,PropertyType::Left
        );
    }

    if(property == PropertyType::Flex) {
        return AppendFlexShorthand(declarations,raw);
    }

    if(property == PropertyType::FlexFlow) {
        return AppendFlexFlowShorthand(declarations,raw);
    }

    if(property == PropertyType::PlaceItems) {
        return AppendPlaceItemsShorthand(declarations,raw);
    }

    if(property == PropertyType::Gap) {
        return AppendGapShorthand(declarations,raw);
    }

    if(property == PropertyType::Border) {
        const auto tokens = SplitCSSWhitespaceValues(raw);
        if(tokens.empty()) return false;

        CSSLength width{1.0f,LengthUnit::Pixels};
        std::string border_style = "solid";
        std::optional<Color> color;
        std::optional<std::string> deferred_color;
        bool width_seen = false;
        bool style_seen = false;
        bool color_seen = false;

        for(const std::string& token : tokens) {
            const std::string normalized = LowerCSS(TrimCSS(token));

            // A var(...) token is treated as the colour component; it is
            // resolved after the custom-property cascade like border-color.
            // (Use border-width for a variable width.)
            if(normalized.find("var(") != std::string::npos) {
                if(color_seen) return false;
                deferred_color = TrimCSS(token);
                color_seen = true;
                continue;
            }

            if(IsCSSBorderStyleValue(normalized)) {
                if(style_seen) return false;
                border_style = normalized;
                style_seen = true;
                continue;
            }

            if(auto parsed_width = ParseCSSLength(token); parsed_width.has_value()) {
                if(width_seen || parsed_width->unit == LengthUnit::Auto) return false;
                width = *parsed_width;
                width_seen = true;
                continue;
            }

            if(auto parsed_color = ParseCSSColor(token); parsed_color.has_value()) {
                if(color_seen) return false;
                color = *parsed_color;
                color_seen = true;
                continue;
            }

            return false;
        }

        if(border_style == "none") {
            width = CSSLength{0.0f,LengthUnit::Pixels};
        }

        // Expand the shorthand into normal declarations so the existing
        // cascade, pseudo-state and per-side-width code keeps doing the work.
        declarations.emplace_back(
            PropertyType::BorderTopWidth,PropertyValueType(width)
        );
        declarations.emplace_back(
            PropertyType::BorderRightWidth,PropertyValueType(width)
        );
        declarations.emplace_back(
            PropertyType::BorderBottomWidth,PropertyValueType(width)
        );
        declarations.emplace_back(
            PropertyType::BorderLeftWidth,PropertyValueType(width)
        );
        declarations.emplace_back(
            PropertyType::BorderStyle,
            PropertyValueType(border_style)
        );
        if(color.has_value()) {
            declarations.emplace_back(
                PropertyType::BorderColor,
                PropertyValueType(*color)
            );
        }
        else if(deferred_color.has_value()) {
            declarations.emplace_back(
                PropertyType::BorderColor,
                PropertyValueType(UIDeferredCSSValue{*deferred_color})
            );
        }
        return true;
    }

    if(
        property == PropertyType::Padding ||
        property == PropertyType::Margin ||
        property == PropertyType::BorderWidth
    ) {
        auto values = ParseCSSBoxShorthand(raw);

        if(!values.has_value()) {
            return false;
        }

        // CSS auto box lengths are meaningful for margins. Padding and border
        // widths must remain concrete lengths.
        if(
            property != PropertyType::Margin &&
            std::any_of(
                values->begin(),
                values->end(),
                [](const CSSLength& length) {
                    return length.unit == LengthUnit::Auto;
                }
            )
        ) {
            return false;
        }

        PropertyType top = property == PropertyType::Padding
            ? PropertyType::PaddingTop
            : property == PropertyType::Margin
                ? PropertyType::MarginTop
                : PropertyType::BorderTopWidth;
        PropertyType right = property == PropertyType::Padding
            ? PropertyType::PaddingRight
            : property == PropertyType::Margin
                ? PropertyType::MarginRight
                : PropertyType::BorderRightWidth;
        PropertyType bottom = property == PropertyType::Padding
            ? PropertyType::PaddingBottom
            : property == PropertyType::Margin
                ? PropertyType::MarginBottom
                : PropertyType::BorderBottomWidth;
        PropertyType left = property == PropertyType::Padding
            ? PropertyType::PaddingLeft
            : property == PropertyType::Margin
                ? PropertyType::MarginLeft
                : PropertyType::BorderLeftWidth;

        declarations.emplace_back(top,PropertyValueType((*values)[0]));
        declarations.emplace_back(right,PropertyValueType((*values)[1]));
        declarations.emplace_back(bottom,PropertyValueType((*values)[2]));
        declarations.emplace_back(left,PropertyValueType((*values)[3]));
        return true;
    }

    if(
        property == PropertyType::GridColumn ||
        property == PropertyType::GridRow
    ) {
        return AppendGridPlacementDeclarations(
            declarations,
            property,
            raw
        );
    }

    auto parsed_value = ParsePropertyValue(property,raw);

    if(!parsed_value.has_value()) {
        return false;
    }

    declarations.emplace_back(property,*parsed_value);
    return true;
}

SelectorType ParseSelectorType(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(value == "*") return SelectorType::Any;
    if(value == "body") return SelectorType::Body;
    if(value == "button") return SelectorType::Button;
    if(value == "label") return SelectorType::Label;
    if(value == "input") return SelectorType::Input;
    if(value == "container") return SelectorType::Container;
    if(value == "selectable") return SelectorType::Selectable;
    if(value == "combo" || value == "combobox") return SelectorType::ComboBox;
    if(value == "range" || value == "range-input") return SelectorType::RangeInput;
    if(value == "canvas") return SelectorType::Canvas;
    if(value == "image" || value == "ui-image") return SelectorType::Image;
    if(value == "splitter") return SelectorType::Splitter;
    if(value == "tabs" || value == "tab-bar") return SelectorType::TabBar;
    if(value == "collapsible") return SelectorType::Collapsible;
    if(value == "tree-node" || value == "tree") return SelectorType::TreeNode;
    if(value == "menu") return SelectorType::Menu;
    if(value == "popup" || value == "modal") return SelectorType::Popup;
    if(value == "window") return SelectorType::Window;
    if(value == "textbox" || value == "text-box" || value == "rich-text") return SelectorType::TextBox;
    if(value == "table" || value == "data-grid" || value == "datagrid") return SelectorType::Table;
    if(value == "virtual-list" || value == "virtuallist") return SelectorType::VirtualList;
    if(value == "dock-space" || value == "dockspace" || value == "docking") return SelectorType::DockSpace;
    if(value == "viewport") return SelectorType::Viewport;
    if(value == "viewport-item" || value == "viewportitem") return SelectorType::ViewportItem;
    if(value == "tooltip") return SelectorType::Tooltip;

    return SelectorType::Any;
}

SelectorState ParseSelectorState(
    std::string value
) {
    value = LowerCSS(TrimCSS(value));

    if(value == "hover") return SelectorState::Hover;
    if(value == "active") return SelectorState::Active;
    if(value == "focus") return SelectorState::Focus;
    if(value == "disabled") return SelectorState::Disabled;

    return SelectorState::None;
}

struct ParsedCSSSelectorChain {
    std::vector<std::string> compounds;
    std::vector<SelectorCombinator> combinators;
};

ParsedCSSSelectorChain ParseCSSSelectorChain(
    const std::string& selector
) {
    ParsedCSSSelectorChain result;
    std::string current;
    char quote = '\0';
    bool escaped = false;
    int parenthesis_depth = 0;
    std::optional<SelectorCombinator> pending_combinator;

    auto flush = [&]() {
        std::string compound = TrimCSS(current);
        if(!compound.empty()) {
            if(!result.compounds.empty()) {
                result.combinators.push_back(
                    pending_combinator.value_or(SelectorCombinator::Descendant)
                );
            }
            result.compounds.push_back(std::move(compound));
            pending_combinator.reset();
        }
        current.clear();
    };

    for(char character : selector) {
        if(quote != '\0') {
            current.push_back(character);
            if(escaped) {
                escaped = false;
            }
            else if(character == '\\') {
                escaped = true;
            }
            else if(character == quote) {
                quote = '\0';
            }
            continue;
        }

        if(character == '\'' || character == '"') {
            quote = character;
            current.push_back(character);
        }
        else if(character == '(') {
            parenthesis_depth++;
            current.push_back(character);
        }
        else if(character == ')') {
            parenthesis_depth = std::max(0,parenthesis_depth - 1);
            current.push_back(character);
        }
        else if(
            parenthesis_depth == 0 &&
            std::isspace(static_cast<unsigned char>(character))
        ) {
            if(!current.empty()) flush();
            if(!result.compounds.empty() && !pending_combinator.has_value()) {
                pending_combinator = SelectorCombinator::Descendant;
            }
        }
        else if(parenthesis_depth == 0 &&
                (character == '>' || character == '+' || character == '~')) {
            if(!current.empty()) flush();
            if(character == '>') pending_combinator = SelectorCombinator::Child;
            else if(character == '+') pending_combinator = SelectorCombinator::AdjacentSibling;
            else pending_combinator = SelectorCombinator::GeneralSibling;
        }
        else {
            current.push_back(character);
        }
    }

    flush();
    if(result.combinators.size()+1 != result.compounds.size()) {
        result.combinators.resize(
            result.compounds.empty() ? 0 : result.compounds.size()-1,
            SelectorCombinator::Descendant
        );
    }
    return result;
}

std::vector<std::string> SplitCSSDescendantSelector(
    const std::string& selector
) {
    return ParseCSSSelectorChain(selector).compounds;
}

static bool ParseNthExpression(std::string text,UIStructuralPseudo& out) {
    text = LowerCSS(TrimCSS(text));
    text.erase(std::remove_if(text.begin(),text.end(),[](unsigned char c){return std::isspace(c)!=0;}),text.end());
    if(text.empty()) return false;
    if(text == "odd") { out.a = 2; out.b = 1; return true; }
    if(text == "even") { out.a = 2; out.b = 0; return true; }
    const std::size_t n = text.find('n');
    auto to_int = [](const std::string& s,int& value) {
        if(s.empty()) return false;
        std::size_t used = 0;
        try { value = std::stoi(s,&used); } catch(...) { return false; }
        return used == s.size();
    };
    if(n == std::string::npos) {
        out.a = 0;
        return to_int(text,out.b);
    }
    const std::string a_text = text.substr(0,n);
    const std::string b_text = text.substr(n+1);
    if(a_text.empty() || a_text == "+") out.a = 1;
    else if(a_text == "-") out.a = -1;
    else if(!to_int(a_text,out.a)) return false;
    if(b_text.empty()) out.b = 0;
    else if(!to_int(b_text,out.b)) return false;
    return true;
}

// Returns true (and appends to `rule.structural`) if `state` is a structural
// pseudo-class.
static bool TryParseStructuralPseudo(const std::string& state,StyleRule& rule) {
    const std::string text = LowerCSS(TrimCSS(state));
    auto add = [&](bool from_end,int a,int b) {
        rule.structural.push_back(UIStructuralPseudo{from_end,a,b});
    };
    if(text == "first-child") { add(false,0,1); return true; }
    if(text == "last-child") { add(true,0,1); return true; }
    if(text == "only-child") { add(false,0,1); add(true,0,1); return true; }
    for(const char* name : {"nth-child","nth-last-child"}) {
        const std::string prefix = std::string(name)+"(";
        if(text.rfind(prefix,0) == 0 && text.back() == ')') {
            UIStructuralPseudo parsed;
            parsed.from_end = std::string(name) == "nth-last-child";
            if(!ParseNthExpression(text.substr(prefix.size(),text.size()-prefix.size()-1),parsed)) {
                TraceLog(LOG_WARNING,"Invalid :%s expression: %s",name,state.c_str());
                // Never matches.
                rule.structural.push_back(UIStructuralPseudo{false,0,0});
                return true;
            }
            rule.structural.push_back(parsed);
            return true;
        }
    }
    return false;
}

static StyleRule ParseCompoundRuleSelector(
    std::string selector
) {
    StyleRule rule;
    selector = TrimCSS(selector);

    std::optional<std::string> part_class;

    std::size_t pseudo_element =
        selector.find("::");

    if(pseudo_element != std::string::npos) {
        std::size_t part_end =
            selector.find(':',pseudo_element + 2);

        std::string part_name = LowerCSS(
            TrimCSS(
                selector.substr(
                    pseudo_element + 2,
                    part_end == std::string::npos
                        ? std::string::npos
                        : part_end - pseudo_element - 2
                )
            )
        );

        if(part_name == "before") {
            rule.pseudo_element = UIGeneratedPseudoKind::Before;
        }
        else if(part_name == "after") {
            rule.pseudo_element = UIGeneratedPseudoKind::After;
        }
        else if(
            part_name == "label" ||
            part_name == "input-label" ||
            part_name == "scalar-label" ||
            part_name == "field-label"
        ) {
            part_class = CSS_INPUT_LABEL_PART;
        }
        else if(part_name == "value-box") {
            part_class = CSS_INPUT_VALUE_BOX_PART;
        }
        else if(
            part_name == "decrement-button" ||
            part_name == "minus-button"
        ) {
            part_class =
                CSS_INPUT_DECREMENT_BUTTON_PART;
        }
        else if(
            part_name == "increment-button" ||
            part_name == "plus-button"
        ) {
            part_class =
                CSS_INPUT_INCREMENT_BUTTON_PART;
        }
        else if(
            part_name == "slider-track" ||
            part_name == "track"
        ) {
            part_class =
                CSS_INPUT_SLIDER_TRACK_PART;
        }
        else if(
            part_name == "slider-fill" ||
            part_name == "fill"
        ) {
            part_class =
                CSS_INPUT_SLIDER_FILL_PART;
        }
        else if(
            part_name == "slider-handle" ||
            part_name == "thumb" ||
            part_name == "handle"
        ) {
            part_class =
                CSS_INPUT_SLIDER_HANDLE_PART;
        }
        else if(
            part_name == "component-label" ||
            part_name == "vector-label" ||
            part_name == "channel-label"
        ) {
            part_class = CSS_INPUT_COMPONENT_LABEL_PART;
        }
        else if(
            part_name == "component-0-label" ||
            part_name == "x-label" ||
            part_name == "r-label" ||
            part_name == "first-label"
        ) {
            part_class = CSS_INPUT_COMPONENT_0_LABEL_PART;
        }
        else if(
            part_name == "component-1-label" ||
            part_name == "y-label" ||
            part_name == "g-label" ||
            part_name == "second-label"
        ) {
            part_class = CSS_INPUT_COMPONENT_1_LABEL_PART;
        }
        else if(
            part_name == "component-2-label" ||
            part_name == "z-label" ||
            part_name == "b-label" ||
            part_name == "third-label"
        ) {
            part_class = CSS_INPUT_COMPONENT_2_LABEL_PART;
        }
        else if(
            part_name == "component-3-label" ||
            part_name == "w-label" ||
            part_name == "a-label" ||
            part_name == "fourth-label"
        ) {
            part_class = CSS_INPUT_COMPONENT_3_LABEL_PART;
        }
        else if(part_name == "header" || part_name == "title-bar") {
            part_class = CSS_UI_HEADER_PART;
        }
        else if(part_name == "popup" || part_name == "dropdown") {
            part_class = CSS_UI_POPUP_PART;
        }
        else if(part_name == "item" || part_name == "option") {
            part_class = CSS_UI_ITEM_PART;
        }
        else if(part_name == "scrollbar") {
            part_class = CSS_UI_SCROLLBAR_PART;
        }
        else if(part_name == "scrollbar-thumb") {
            part_class = CSS_UI_SCROLLBAR_THUMB_PART;
        }
        else if(part_name == "range-track") {
            part_class = CSS_UI_RANGE_TRACK_PART;
        }
        else if(part_name == "range-fill") {
            part_class = CSS_UI_RANGE_FILL_PART;
        }
        else if(part_name == "range-handle") {
            part_class = CSS_UI_RANGE_HANDLE_PART;
        }
        else if(part_name == "tab") {
            part_class = CSS_UI_TAB_PART;
        }
        else if(part_name == "tab-content" || part_name == "content") {
            part_class = CSS_UI_TAB_CONTENT_PART;
        }
        else if(part_name == "splitter-handle" || part_name == "grip") {
            part_class = CSS_UI_SPLITTER_HANDLE_PART;
        }
        else {
            TraceLog(
                LOG_WARNING,
                "Unknown CSS pseudo-element: %s",
                part_name.c_str()
            );

            part_class =
                "__css-invalid-input-pseudo-element";
        }

        selector.erase(
            pseudo_element,
            part_end == std::string::npos
                ? std::string::npos
                : part_end - pseudo_element
        );
    }

    std::size_t colon = selector.find(':');

    if(colon != std::string::npos) {
        std::string pseudo_text = selector.substr(colon + 1);
        selector.resize(colon);

        std::size_t state_cursor = 0;
        while(state_cursor <= pseudo_text.size()) {
            std::size_t next = pseudo_text.find(':',state_cursor);
            if(TryParseStructuralPseudo(
                pseudo_text.substr(
                    state_cursor,
                    next == std::string::npos ? std::string::npos : next-state_cursor
                ),rule)) {
                if(next == std::string::npos) break;
                state_cursor = next+1;
                continue;
            }
            std::string state_name = NormalizeUIPseudoStateName(
                pseudo_text.substr(
                    state_cursor,
                    next == std::string::npos
                        ? std::string::npos
                        : next-state_cursor
                )
            );
            if(!state_name.empty()) {
                rule.pseudo_states.push_back(state_name);
                if(rule.state == SelectorState::None) {
                    rule.state = ParseSelectorState(state_name);
                }
            }
            if(next == std::string::npos) break;
            state_cursor = next+1;
        }
    }

    std::size_t type_end =
        selector.find_first_of("#.");

    const std::string raw_type = TrimCSS(selector.substr(0,type_end));
    rule.selector = ParseSelectorType(raw_type);
    // Unknown type names are registry-backed component selectors in RUI 0.8.
    // Encode them as an internal class so the existing fast selector matcher
    // needs no per-component enum entries or global registry state.
    if(
        rule.selector == SelectorType::Any &&
        !raw_type.empty() && raw_type != "*"
    ) {
        rule.classes.push_back(UIComponentSyntheticClass(raw_type));
    }

    std::size_t index = type_end;

    while(
        index != std::string::npos &&
        index < selector.size()
    ) {
        char marker = selector[index];

        std::size_t next =
            selector.find_first_of(
                "#.",
                index + 1
            );

        std::string token =
            TrimCSS(
                selector.substr(
                    index + 1,
                    next == std::string::npos
                        ? std::string::npos
                        : next - index - 1
                )
            );

        if(marker == '#' && !token.empty()) {
            rule.id = token;
        }
        else if(marker == '.' && !token.empty()) {
            rule.classes.push_back(token);
        }

        index = next;
    }

    if(part_class.has_value()) {
        rule.classes.push_back(*part_class);
    }

    return rule;
}

StyleRule ParseRuleSelector(
    std::string selector
) {
    ParsedCSSSelectorChain chain =
        ParseCSSSelectorChain(selector);
    std::vector<std::string>& compounds = chain.compounds;

    if(compounds.empty()) {
        return {};
    }

    StyleRule rule =
        ParseCompoundRuleSelector(compounds.back());

    rule.ancestors.reserve(compounds.size() - 1);
    rule.combinators = std::move(chain.combinators);

    for(std::size_t index = 0; index + 1 < compounds.size(); index++) {
        StyleRule parsed =
            ParseCompoundRuleSelector(compounds[index]);
        if(parsed.pseudo_element != UIGeneratedPseudoKind::None) {
            TraceLog(LOG_WARNING,"Generated pseudo-elements may only appear on the final selector compound");
        }

        StyleSelector ancestor;
        ancestor.selector = parsed.selector;
        ancestor.id = std::move(parsed.id);
        ancestor.classes = std::move(parsed.classes);
        ancestor.pseudo_states = std::move(parsed.pseudo_states);
        ancestor.structural = std::move(parsed.structural);
        rule.ancestors.push_back(std::move(ancestor));
    }

    return rule;
}

std::string RemoveCSSComments(
    const std::string& css
) {
    std::string result;
    std::size_t cursor = 0;

    while(cursor < css.size()) {
        std::size_t opening =
            css.find("/*",cursor);

        if(opening == std::string::npos) {
            result.append(css.substr(cursor));
            break;
        }

        result.append(
            css.substr(
                cursor,
                opening - cursor
            )
        );

        std::size_t closing =
            css.find("*/",opening + 2);

        if(closing == std::string::npos) {
            break;
        }

        cursor = closing + 2;
    }

    return result;
}

std::vector<std::string> SplitCSSSelectors(
    const std::string& selectors
) {
    std::vector<std::string> result;
    std::string current;
    char quote = '\0';
    bool escaped = false;
    int parenthesis_depth = 0;

    for(char character : selectors) {
        if(quote != '\0') {
            current.push_back(character);
            if(escaped) escaped = false;
            else if(character == '\\') escaped = true;
            else if(character == quote) quote = '\0';
            continue;
        }
        if(character == '\'' || character == '"') {
            quote = character;
            current.push_back(character);
        }
        else if(character == '(') {
            ++parenthesis_depth;
            current.push_back(character);
        }
        else if(character == ')') {
            parenthesis_depth = std::max(0,parenthesis_depth-1);
            current.push_back(character);
        }
        else if(character == ',' && parenthesis_depth == 0) {
            std::string selector = TrimCSS(current);
            if(!selector.empty()) result.push_back(std::move(selector));
            current.clear();
        }
        else current.push_back(character);
    }

    std::string selector = TrimCSS(current);
    if(!selector.empty()) result.push_back(std::move(selector));

    return result;
}

bool IsCSSCustomProperty(
    const std::string& property_name
) {
    std::string name = TrimCSS(property_name);

    return
        name.size() > 2 &&
        name.rfind("--",0) == 0;
}

bool IsCSSRootSelector(
    const std::string& selector
) {
    return LowerCSS(TrimCSS(selector)) == ":root";
}

std::size_t FindMatchingCSSParenthesis(
    const std::string& text,
    std::size_t opening
) {
    if(
        opening >= text.size() ||
        text[opening] != '('
    ) {
        return std::string::npos;
    }

    int depth = 0;

    for(
        std::size_t index = opening;
        index < text.size();
        index++
    ) {
        if(text[index] == '(') {
            depth++;
        }
        else if(text[index] == ')') {
            depth--;

            if(depth == 0) {
                return index;
            }
        }
    }

    return std::string::npos;
}

std::size_t FindTopLevelCSSComma(
    const std::string& text
) {
    int depth = 0;

    for(
        std::size_t index = 0;
        index < text.size();
        index++
    ) {
        if(text[index] == '(') {
            depth++;
        }
        else if(text[index] == ')') {
            depth = std::max(0,depth - 1);
        }
        else if(text[index] == ',' && depth == 0) {
            return index;
        }
    }

    return std::string::npos;
}

std::optional<std::string> ResolveCSSVariablesInternal(
    const std::string& raw,
    const CSSVariableMap& variables,
    std::unordered_set<std::string>& resolving,
    int recursion_depth
) {
    constexpr int maximum_recursion_depth = 64;

    if(recursion_depth > maximum_recursion_depth) {
        return std::nullopt;
    }

    std::string result = raw;
    std::size_t search_from = 0;
    int replacements = 0;

    while(search_from < result.size()) {
        std::string lowered = LowerCSS(result);
        std::size_t variable_start =
            lowered.find("var(",search_from);

        if(variable_start == std::string::npos) {
            break;
        }

        std::size_t opening = variable_start + 3;
        std::size_t closing =
            FindMatchingCSSParenthesis(result,opening);

        if(closing == std::string::npos) {
            return std::nullopt;
        }

        std::string arguments = result.substr(
            opening + 1,
            closing - opening - 1
        );

        std::size_t comma =
            FindTopLevelCSSComma(arguments);

        std::string variable_name = TrimCSS(
            arguments.substr(0,comma)
        );

        std::optional<std::string> fallback;

        if(comma != std::string::npos) {
            fallback = TrimCSS(
                arguments.substr(comma + 1)
            );
        }

        if(!IsCSSCustomProperty(variable_name)) {
            return std::nullopt;
        }

        std::optional<std::string> replacement;
        auto found = variables.find(variable_name);

        if(
            found != variables.end() &&
            resolving.find(variable_name) == resolving.end()
        ) {
            resolving.insert(variable_name);

            replacement = ResolveCSSVariablesInternal(
                found->second,
                variables,
                resolving,
                recursion_depth + 1
            );

            resolving.erase(variable_name);
        }

        if(!replacement.has_value() && fallback.has_value()) {
            replacement = ResolveCSSVariablesInternal(
                *fallback,
                variables,
                resolving,
                recursion_depth + 1
            );
        }

        if(!replacement.has_value()) {
            return std::nullopt;
        }

        result.replace(
            variable_start,
            closing - variable_start + 1,
            *replacement
        );

        search_from = variable_start + replacement->size();
        replacements++;

        if(replacements > 256) {
            return std::nullopt;
        }
    }

    return TrimCSS(result);
}

std::optional<std::string> ResolveCSSVariables(
    const std::string& raw,
    const CSSVariableMap& variables
) {
    std::unordered_set<std::string> resolving;

    return ResolveCSSVariablesInternal(
        raw,
        variables,
        resolving,
        0
    );
}

std::vector<RawCSSDeclaration> ParseRawCSSDeclarations(
    const std::string& body
) {
    std::vector<RawCSSDeclaration> declarations;
    std::size_t cursor = 0;

    while(cursor < body.size()) {
        std::size_t semicolon = body.find(';',cursor);

        std::string declaration = TrimCSS(
            body.substr(
                cursor,
                semicolon == std::string::npos
                    ? std::string::npos
                    : semicolon - cursor
            )
        );

        if(!declaration.empty()) {
            std::size_t colon = declaration.find(':');

            if(colon == std::string::npos) {
                TraceLog(
                    LOG_WARNING,
                    "Invalid CSS declaration: %s",
                    declaration.c_str()
                );
            }
            else {
                declarations.push_back(
                    RawCSSDeclaration{
                        TrimCSS(declaration.substr(0,colon)),
                        TrimCSS(declaration.substr(colon + 1))
                    }
                );
            }
        }

        if(semicolon == std::string::npos) {
            break;
        }

        cursor = semicolon + 1;
    }

    return declarations;
}

CSSVariableMap CollectCSSRootVariables(
    const std::string& source
) {
    CSSVariableMap variables;
    std::size_t statement_start = 0;
    std::size_t cursor = 0;
    char quote = '\0';
    bool escaped = false;

    // Walk only top-level blocks.  The previous implementation searched for the
    // next literal '}', which stopped at the first nested rule and could skip a
    // later :root block entirely.  Theme overrides are intentionally appended
    // after the authored stylesheet, so later :root declarations must win.
    while(cursor < source.size()) {
        const char c = source[cursor];
        if(quote != '\0') {
            if(escaped) escaped = false;
            else if(c == '\\') escaped = true;
            else if(c == quote) quote = '\0';
            ++cursor;
            continue;
        }
        if(c == '\'' || c == '"') {
            quote = c;
            ++cursor;
            continue;
        }
        if(c != '{') {
            ++cursor;
            continue;
        }

        const std::string selector_text = TrimCSS(RemoveCSSComments(
            source.substr(statement_start,cursor-statement_start)
        ));

        std::size_t close = cursor+1;
        int depth = 1;
        char body_quote = '\0';
        bool body_escaped = false;
        for(; close < source.size() && depth > 0; ++close) {
            const char body_c = source[close];
            if(body_quote != '\0') {
                if(body_escaped) body_escaped = false;
                else if(body_c == '\\') body_escaped = true;
                else if(body_c == body_quote) body_quote = '\0';
                continue;
            }
            if(body_c == '\'' || body_c == '"') body_quote = body_c;
            else if(body_c == '{') ++depth;
            else if(body_c == '}') --depth;
        }
        if(depth != 0) break;

        const std::size_t closing_brace = close-1;
        const auto selectors = SplitCSSSelectors(selector_text);
        const bool is_root_rule = std::any_of(
            selectors.begin(),selectors.end(),
            [](const std::string& selector) { return IsCSSRootSelector(selector); }
        );
        if(is_root_rule) {
            const std::string body = source.substr(
                cursor+1,closing_brace-cursor-1
            );
            for(const RawCSSDeclaration& declaration : ParseRawCSSDeclarations(body)) {
                if(IsCSSCustomProperty(declaration.property_name)) {
                    // CSS source order: a later :root declaration with the same
                    // name replaces the earlier value.
                    variables[declaration.property_name] = declaration.property_value;
                }
            }
        }

        cursor = close;
        statement_start = cursor;
    }

    return variables;
}


namespace {

std::vector<std::string> SplitTopLevelCSSList(const std::string& text) {
    std::vector<std::string> result;
    int parentheses = 0;
    std::size_t start = 0;
    for(std::size_t index=0; index<=text.size(); ++index) {
        const char character = index < text.size() ? text[index] : ',';
        if(character == '(') ++parentheses;
        else if(character == ')') parentheses = std::max(0,parentheses-1);
        else if(character == ',' && parentheses == 0) {
            std::string value = TrimCSS(text.substr(start,index-start));
            if(!value.empty()) result.push_back(std::move(value));
            start = index+1;
        }
    }
    return result;
}

std::optional<float> ParseCSSTimeSeconds(std::string value) {
    value = LowerCSS(TrimCSS(std::move(value)));
    float multiplier = 1.0f;
    if(EndsWithCSS(value,"ms")) {
        value.resize(value.size()-2);
        multiplier = 0.001f;
    }
    else if(EndsWithCSS(value,"s")) {
        value.resize(value.size()-1);
    }
    else {
        return std::nullopt;
    }
    auto number = ParseStrictCSSFloat(TrimCSS(value));
    return number.has_value()
        ? std::optional<float>(std::max(0.0f,*number*multiplier))
        : std::nullopt;
}

std::optional<UIEasing> ParseUIEasing(std::string value) {
    value = LowerCSS(TrimCSS(std::move(value)));
    if(value == "linear") return UIEasing::Linear;
    if(value == "ease") return UIEasing::Ease;
    if(value == "ease-in") return UIEasing::EaseIn;
    if(value == "ease-out") return UIEasing::EaseOut;
    if(value == "ease-in-out") return UIEasing::EaseInOut;
    if(value == "quadratic-in" || value == "quad-in") return UIEasing::QuadraticIn;
    if(value == "quadratic-out" || value == "quad-out") return UIEasing::QuadraticOut;
    if(value == "quadratic-in-out" || value == "quad-in-out") return UIEasing::QuadraticInOut;
    if(value == "cubic-in") return UIEasing::CubicIn;
    if(value == "cubic-out") return UIEasing::CubicOut;
    if(value == "cubic-in-out") return UIEasing::CubicInOut;
    if(value == "back-in") return UIEasing::BackIn;
    if(value == "back-out") return UIEasing::BackOut;
    if(value == "back-in-out") return UIEasing::BackInOut;
    if(value == "bounce-in") return UIEasing::BounceIn;
    if(value == "bounce-out") return UIEasing::BounceOut;
    if(value == "bounce-in-out") return UIEasing::BounceInOut;
    if(value == "elastic-in") return UIEasing::ElasticIn;
    if(value == "elastic-out") return UIEasing::ElasticOut;
    if(value == "elastic-in-out") return UIEasing::ElasticInOut;
    return std::nullopt;
}

std::optional<UIAnimationDirection> ParseUIAnimationDirection(std::string value) {
    value = LowerCSS(TrimCSS(std::move(value)));
    if(value == "normal") return UIAnimationDirection::Normal;
    if(value == "reverse") return UIAnimationDirection::Reverse;
    if(value == "alternate") return UIAnimationDirection::Alternate;
    if(value == "alternate-reverse") return UIAnimationDirection::AlternateReverse;
    return std::nullopt;
}

std::optional<UIAnimationFillMode> ParseUIAnimationFillMode(std::string value) {
    value = LowerCSS(TrimCSS(std::move(value)));
    if(value == "none") return UIAnimationFillMode::None;
    if(value == "forwards") return UIAnimationFillMode::Forwards;
    if(value == "backwards") return UIAnimationFillMode::Backwards;
    if(value == "both") return UIAnimationFillMode::Both;
    return std::nullopt;
}

std::vector<std::string> SplitCSSWordsPreservingFunctions(const std::string& text) {
    std::vector<std::string> result;
    std::string current;
    int parentheses = 0;
    for(char character : text) {
        if(character == '(') ++parentheses;
        else if(character == ')') parentheses = std::max(0,parentheses-1);
        if(std::isspace(static_cast<unsigned char>(character)) && parentheses == 0) {
            if(!current.empty()) {
                result.push_back(std::move(current));
                current.clear();
            }
        }
        else current.push_back(character);
    }
    if(!current.empty()) result.push_back(std::move(current));
    return result;
}

std::vector<UITransitionSpec> ParseTransitionShorthand(const std::string& raw) {
    std::vector<UITransitionSpec> result;
    if(LowerCSS(TrimCSS(raw)) == "none") return result;
    for(const std::string& item : SplitTopLevelCSSList(raw)) {
        UITransitionSpec spec;
        bool duration_set = false;
        for(const std::string& token : SplitCSSWordsPreservingFunctions(item)) {
            if(auto time = ParseCSSTimeSeconds(token)) {
                if(!duration_set) {
                    spec.duration = *time;
                    duration_set = true;
                }
                else spec.delay = *time;
                continue;
            }
            if(auto easing = ParseUIEasing(token)) {
                spec.easing = *easing;
                continue;
            }
            const std::string raw_property_name = TrimCSS(token);
            const std::string property_name = LowerCSS(raw_property_name);
            if(property_name == "all") {
                spec.all = true;
                spec.properties.clear();
                spec.dynamic_properties.clear();
            }
            else if(auto property = ParsePropertyType(property_name)) {
                spec.all = false;
                spec.properties = {*property};
                spec.dynamic_properties.clear();
            }
            else if(!raw_property_name.empty()) {
                // Registry-driven component style properties do not have fixed
                // PropertyType slots. Preserve their spelling and let the
                // component descriptor validate/apply them at runtime.
                spec.all = false;
                spec.properties.clear();
                spec.dynamic_properties = {raw_property_name};
            }
        }
        if(spec.duration > 0.0f) result.push_back(std::move(spec));
    }
    return result;
}

std::vector<UIAnimationSpec> ParseAnimationShorthand(const std::string& raw) {
    std::vector<UIAnimationSpec> result;
    if(LowerCSS(TrimCSS(raw)) == "none") return result;
    for(const std::string& item : SplitTopLevelCSSList(raw)) {
        UIAnimationSpec spec;
        bool duration_set = false;
        for(const std::string& token : SplitCSSWordsPreservingFunctions(item)) {
            const std::string lower = LowerCSS(TrimCSS(token));
            if(auto time = ParseCSSTimeSeconds(lower)) {
                if(!duration_set) {
                    spec.duration = *time;
                    duration_set = true;
                }
                else spec.delay = *time;
                continue;
            }
            if(auto easing = ParseUIEasing(lower)) { spec.easing = *easing; continue; }
            if(auto direction = ParseUIAnimationDirection(lower)) { spec.direction = *direction; continue; }
            if(auto fill = ParseUIAnimationFillMode(lower)) { spec.fill_mode = *fill; continue; }
            if(lower == "infinite") { spec.infinite = true; continue; }
            if(lower == "paused") { spec.paused = true; continue; }
            if(lower == "running") { spec.paused = false; continue; }
            if(auto count = ParseStrictCSSFloat(lower)) {
                spec.iterations = std::max(0.0f,*count);
                continue;
            }
            if(spec.name.empty()) spec.name = lower;
        }
        if(!spec.name.empty() && spec.duration > 0.0f) result.push_back(std::move(spec));
    }
    return result;
}

void ApplyTransitionLonghands(
    const std::vector<RawCSSDeclaration>& raw,
    const CSSVariableMap& variables,
    bool& declared,
    std::vector<UITransitionSpec>& transitions
) {
    std::optional<std::string> properties;
    std::optional<std::string> durations;
    std::optional<std::string> delays;
    std::optional<std::string> easings;
    for(const auto& declaration : raw) {
        const std::string name = LowerCSS(TrimCSS(declaration.property_name));
        auto value = ResolveCSSVariables(declaration.property_value,variables);
        if(!value.has_value()) continue;
        if(name == "transition") {
            declared = true;
            transitions = ParseTransitionShorthand(*value);
        }
        else if(name == "transition-property") { declared = true; properties = *value; }
        else if(name == "transition-duration") { declared = true; durations = *value; }
        else if(name == "transition-delay") { declared = true; delays = *value; }
        else if(name == "transition-timing-function") { declared = true; easings = *value; }
    }
    if(!properties && !durations && !delays && !easings) return;
    auto property_values = SplitTopLevelCSSList(properties.value_or("all"));
    auto duration_values = SplitTopLevelCSSList(durations.value_or("0s"));
    auto delay_values = SplitTopLevelCSSList(delays.value_or("0s"));
    auto easing_values = SplitTopLevelCSSList(easings.value_or("ease"));
    const std::size_t count = std::max({
        property_values.size(),duration_values.size(),delay_values.size(),easing_values.size()
    });
    transitions.clear();
    for(std::size_t index=0; index<count; ++index) {
        UITransitionSpec spec;
        const std::string raw_property = TrimCSS(property_values[index%property_values.size()]);
        const std::string property = LowerCSS(raw_property);
        if(property != "all") {
            if(auto parsed = ParsePropertyType(property)) {
                spec.all = false;
                spec.properties = {*parsed};
            }
            else if(!raw_property.empty()) {
                spec.all = false;
                spec.dynamic_properties = {raw_property};
            }
            else continue;
        }
        spec.duration = ParseCSSTimeSeconds(duration_values[index%duration_values.size()]).value_or(0.0f);
        spec.delay = ParseCSSTimeSeconds(delay_values[index%delay_values.size()]).value_or(0.0f);
        spec.easing = ParseUIEasing(easing_values[index%easing_values.size()]).value_or(UIEasing::Ease);
        if(spec.duration > 0.0f) transitions.push_back(std::move(spec));
    }
}

void ApplyAnimationLonghands(
    const std::vector<RawCSSDeclaration>& raw,
    const CSSVariableMap& variables,
    bool& declared,
    std::vector<UIAnimationSpec>& animations
) {
    std::optional<std::string> names,durations,delays,easings,iterations,directions,fills,states;
    for(const auto& declaration : raw) {
        const std::string name = LowerCSS(TrimCSS(declaration.property_name));
        auto value = ResolveCSSVariables(declaration.property_value,variables);
        if(!value.has_value()) continue;
        if(name == "animation") { declared = true; animations = ParseAnimationShorthand(*value); }
        else if(name == "animation-name") { declared = true; names = *value; }
        else if(name == "animation-duration") { declared = true; durations = *value; }
        else if(name == "animation-delay") { declared = true; delays = *value; }
        else if(name == "animation-timing-function") { declared = true; easings = *value; }
        else if(name == "animation-iteration-count") { declared = true; iterations = *value; }
        else if(name == "animation-direction") { declared = true; directions = *value; }
        else if(name == "animation-fill-mode") { declared = true; fills = *value; }
        else if(name == "animation-play-state") { declared = true; states = *value; }
    }
    if(!names && !durations && !delays && !easings && !iterations && !directions && !fills && !states) return;
    auto name_values = SplitTopLevelCSSList(names.value_or("none"));
    auto duration_values = SplitTopLevelCSSList(durations.value_or("0s"));
    auto delay_values = SplitTopLevelCSSList(delays.value_or("0s"));
    auto easing_values = SplitTopLevelCSSList(easings.value_or("ease"));
    auto iteration_values = SplitTopLevelCSSList(iterations.value_or("1"));
    auto direction_values = SplitTopLevelCSSList(directions.value_or("normal"));
    auto fill_values = SplitTopLevelCSSList(fills.value_or("none"));
    auto state_values = SplitTopLevelCSSList(states.value_or("running"));
    const std::size_t count = name_values.size();
    animations.clear();
    for(std::size_t index=0; index<count; ++index) {
        UIAnimationSpec spec;
        spec.name = LowerCSS(TrimCSS(name_values[index]));
        if(spec.name == "none" || spec.name.empty()) continue;
        spec.duration = ParseCSSTimeSeconds(duration_values[index%duration_values.size()]).value_or(0.0f);
        spec.delay = ParseCSSTimeSeconds(delay_values[index%delay_values.size()]).value_or(0.0f);
        spec.easing = ParseUIEasing(easing_values[index%easing_values.size()]).value_or(UIEasing::Ease);
        const std::string count_text = LowerCSS(TrimCSS(iteration_values[index%iteration_values.size()]));
        if(count_text == "infinite") spec.infinite = true;
        else spec.iterations = std::max(0.0f,ParseStrictCSSFloat(count_text).value_or(1.0f));
        spec.direction = ParseUIAnimationDirection(direction_values[index%direction_values.size()]).value_or(UIAnimationDirection::Normal);
        spec.fill_mode = ParseUIAnimationFillMode(fill_values[index%fill_values.size()]).value_or(UIAnimationFillMode::None);
        spec.paused = LowerCSS(TrimCSS(state_values[index%state_values.size()])) == "paused";
        if(spec.duration > 0.0f) animations.push_back(std::move(spec));
    }
}

std::size_t FindMatchingCSSBrace(const std::string& source, std::size_t opening) {
    if(opening >= source.size() || source[opening] != '{') return std::string::npos;
    int depth = 0;
    for(std::size_t index=opening; index<source.size(); ++index) {
        if(source[index] == '{') ++depth;
        else if(source[index] == '}' && --depth == 0) return index;
    }
    return std::string::npos;
}

struct ParsedKeyframeDeclarations {
    std::vector<Declaration> declarations;
    std::vector<UIComponentStyleDeclaration> component_styles;
};

ParsedKeyframeDeclarations ParseKeyframeDeclarations(
    const std::string& body,
    const CSSVariableMap& variables
) {
    ParsedKeyframeDeclarations result;
    for(const auto& raw : ParseRawCSSDeclarations(body)) {
        if(IsCSSCustomProperty(raw.property_name)) continue;
        const std::string lowered=LowerCSS(TrimCSS(raw.property_name));
        if(lowered.rfind("shader-",0)==0 || lowered.rfind("animation",0)==0 || lowered.rfind("transition",0)==0) continue;
        auto property = ParsePropertyType(raw.property_name);
        if(property.has_value()) {
            auto value = ResolveCSSVariables(raw.property_value,variables);
            if(value.has_value()) AppendCSSDeclaration(result.declarations,*property,*value);
        }
        else {
            result.component_styles.push_back({TrimCSS(raw.property_name),TrimCSS(raw.property_value)});
        }
    }
    return result;
}

void ExtractCSSKeyframes(
    std::string& source,
    const CSSVariableMap& variables,
    StyleSheet& sheet
) {
    std::size_t search = 0;
    while(search < source.size()) {
        const std::string lower = LowerCSS(source);
        std::size_t marker = lower.find("@keyframes",search);
        if(marker == std::string::npos) break;
        std::size_t name_start = marker+10;
        while(name_start < source.size() && std::isspace(static_cast<unsigned char>(source[name_start]))) ++name_start;
        std::size_t open = source.find('{',name_start);
        if(open == std::string::npos) break;
        const std::string name = LowerCSS(TrimCSS(source.substr(name_start,open-name_start)));
        const std::size_t close = FindMatchingCSSBrace(source,open);
        if(close == std::string::npos) {
            TraceLog(LOG_WARNING,"@keyframes '%s' is missing a closing brace",name.c_str());
            break;
        }
        UIKeyframes frames;
        frames.name = name;
        std::size_t cursor = open+1;
        while(cursor < close) {
            std::size_t step_open = source.find('{',cursor);
            if(step_open == std::string::npos || step_open >= close) break;
            std::size_t step_close = FindMatchingCSSBrace(source,step_open);
            if(step_close == std::string::npos || step_close > close) break;
            const std::string offsets = source.substr(cursor,step_open-cursor);
            const auto parsed_step = ParseKeyframeDeclarations(
                source.substr(step_open+1,step_close-step_open-1),variables
            );
            for(const std::string& raw_offset : SplitCSSSelectors(offsets)) {
                const std::string offset_text = LowerCSS(TrimCSS(raw_offset));
                float offset = -1.0f;
                if(offset_text == "from") offset = 0.0f;
                else if(offset_text == "to") offset = 1.0f;
                else if(EndsWithCSS(offset_text,"%")) {
                    auto parsed = ParseStrictCSSFloat(offset_text.substr(0,offset_text.size()-1));
                    if(parsed.has_value()) offset = std::clamp(*parsed/100.0f,0.0f,1.0f);
                }
                if(offset >= 0.0f) {
                    UIKeyframeStep step;
                    step.offset=offset;
                    step.declarations=parsed_step.declarations;
                    step.component_style_properties=parsed_step.component_styles;
                    frames.steps.push_back(std::move(step));
                }
            }
            cursor = step_close+1;
        }
        std::sort(frames.steps.begin(),frames.steps.end(),[](const UIKeyframeStep& a,const UIKeyframeStep& b) {
            return a.offset < b.offset;
        });
        if(!frames.name.empty() && !frames.steps.empty()) sheet.keyframes[frames.name] = std::move(frames);
        std::fill(source.begin()+static_cast<std::ptrdiff_t>(marker),source.begin()+static_cast<std::ptrdiff_t>(close+1),' ');
        search = close+1;
    }
}

bool IsAnimationMetaProperty(const std::string& raw_name) {
    const std::string name = LowerCSS(TrimCSS(raw_name));
    return name == "transition" || name.rfind("transition-",0) == 0 ||
           name == "animation" || name.rfind("animation-",0) == 0;
}

std::vector<UIMediaQuery> ParseUIMediaQueries(const std::string& raw) {
    std::vector<UIMediaQuery> queries;
    for(std::string group : SplitCSSSelectors(raw)) {
        group = LowerCSS(TrimCSS(std::move(group)));
        UIMediaQuery query;
        bool valid = true;
        std::size_t cursor = 0;
        bool found_feature = false;
        while((cursor = group.find('(',cursor)) != std::string::npos) {
            const std::size_t close = FindMatchingCSSParenthesis(group,cursor);
            if(close == std::string::npos) {
                valid = false;
                break;
            }
            found_feature = true;
            const std::string feature = TrimCSS(
                group.substr(cursor+1,close-cursor-1)
            );
            const std::size_t colon = feature.find(':');
            std::string name = TrimCSS(feature.substr(0,colon));
            std::string value = colon == std::string::npos
                ? std::string{} : TrimCSS(feature.substr(colon+1));

            // Modern media range syntax used by nested RCSS:
            // (width < 800px), (height >= 400px). Inclusive/exclusive
            // differences below a pixel are not meaningful to this UI layout.
            std::string range_operator;
            if(colon == std::string::npos) {
                for(const char* candidate : {"<=",">=","<",">"}) {
                    const std::size_t position=feature.find(candidate);
                    if(position==std::string::npos) continue;
                    name=TrimCSS(feature.substr(0,position));
                    value=TrimCSS(feature.substr(position+std::strlen(candidate)));
                    range_operator=candidate;
                    break;
                }
            }
            if(!range_operator.empty()) {
                const auto number=ParseCSSNumber(value);
                if(!number.has_value()) valid=false;
                else if(name=="width") {
                    if(range_operator[0]=='<') query.max_width=*number;
                    else query.min_width=*number;
                }
                else if(name=="height") {
                    if(range_operator[0]=='<') query.max_height=*number;
                    else query.min_height=*number;
                }
                else valid=false;
            }
            else if(name == "orientation") {
                if(value == "landscape" || value == "portrait") {
                    query.orientation = value;
                }
                else valid = false;
            }
            else {
                const auto number = ParseCSSNumber(value);
                if(!number.has_value()) valid = false;
                else if(name == "min-width") query.min_width = *number;
                else if(name == "max-width") query.max_width = *number;
                else if(name == "width") {
                    query.min_width = *number;
                    query.max_width = *number;
                }
                else if(name == "min-height") query.min_height = *number;
                else if(name == "max-height") query.max_height = *number;
                else if(name == "height") {
                    query.min_height = *number;
                    query.max_height = *number;
                }
                else valid = false;
            }
            cursor = close+1;
        }
        if(!found_feature && group != "all" && group != "screen" &&
           group != "only screen") valid = false;
        if(valid) queries.push_back(std::move(query));
        else TraceLog(LOG_WARNING,"Unsupported @media query: %s",group.c_str());
    }
    return queries;
}

std::vector<UIMediaQuery> CombineMediaQueries(
    const std::vector<UIMediaQuery>& parent,
    const std::vector<UIMediaQuery>& child
) {
    if(parent.empty()) return child;
    if(child.empty()) return parent;
    std::vector<UIMediaQuery> combined;
    for(const UIMediaQuery& first : parent) {
        for(const UIMediaQuery& second : child) {
            UIMediaQuery query;
            if(first.min_width || second.min_width) query.min_width = std::max(
                first.min_width.value_or(-std::numeric_limits<float>::infinity()),
                second.min_width.value_or(-std::numeric_limits<float>::infinity())
            );
            if(first.max_width || second.max_width) query.max_width = std::min(
                first.max_width.value_or(std::numeric_limits<float>::infinity()),
                second.max_width.value_or(std::numeric_limits<float>::infinity())
            );
            if(first.min_height || second.min_height) query.min_height = std::max(
                first.min_height.value_or(-std::numeric_limits<float>::infinity()),
                second.min_height.value_or(-std::numeric_limits<float>::infinity())
            );
            if(first.max_height || second.max_height) query.max_height = std::min(
                first.max_height.value_or(std::numeric_limits<float>::infinity()),
                second.max_height.value_or(std::numeric_limits<float>::infinity())
            );
            query.orientation = second.orientation.empty()
                ? first.orientation : second.orientation;
            if(!first.orientation.empty() && !second.orientation.empty() &&
               first.orientation != second.orientation) continue;
            combined.push_back(std::move(query));
        }
    }
    return combined;
}

struct CSSNestedBlock {
    std::string header;
    std::string body;
};

struct CSSBlockContent {
    std::vector<RawCSSDeclaration> declarations;
    std::vector<CSSNestedBlock> nested;
};

std::optional<RawCSSDeclaration> ParseSingleRawDeclaration(std::string text) {
    text=TrimCSS(std::move(text));
    if(text.empty()) return std::nullopt;
    int parentheses=0;
    char quote='\0';
    bool escaped=false;
    for(std::size_t index=0;index<text.size();++index) {
        const char c=text[index];
        if(quote!='\0') {
            if(escaped) escaped=false;
            else if(c=='\\') escaped=true;
            else if(c==quote) quote='\0';
            continue;
        }
        if(c=='\'' || c=='"') { quote=c; continue; }
        if(c=='(') { ++parentheses; continue; }
        if(c==')') { parentheses=std::max(0,parentheses-1); continue; }
        if(c==':' && parentheses==0) {
            return RawCSSDeclaration{
                TrimCSS(text.substr(0,index)),
                TrimCSS(text.substr(index+1))
            };
        }
    }
    TraceLog(LOG_WARNING,"Invalid CSS declaration: %s",text.c_str());
    return std::nullopt;
}

CSSBlockContent ParseCSSBlockContent(const std::string& source) {
    CSSBlockContent result;
    std::size_t statement_start=0;
    std::size_t cursor=0;
    int parentheses=0;
    char quote='\0';
    bool escaped=false;

    while(cursor<source.size()) {
        const char c=source[cursor];
        if(quote!='\0') {
            if(escaped) escaped=false;
            else if(c=='\\') escaped=true;
            else if(c==quote) quote='\0';
            ++cursor;
            continue;
        }
        if(c=='\'' || c=='"') { quote=c; ++cursor; continue; }
        if(c=='(') { ++parentheses; ++cursor; continue; }
        if(c==')') { parentheses=std::max(0,parentheses-1); ++cursor; continue; }

        if(c==';' && parentheses==0) {
            if(auto declaration=ParseSingleRawDeclaration(
                source.substr(statement_start,cursor-statement_start))) {
                result.declarations.push_back(std::move(*declaration));
            }
            statement_start=cursor+1;
            ++cursor;
            continue;
        }
        if(c=='{' && parentheses==0) {
            const std::string header=TrimCSS(
                source.substr(statement_start,cursor-statement_start));
            const std::size_t close=FindMatchingCSSBrace(source,cursor);
            if(close==std::string::npos) {
                TraceLog(LOG_WARNING,"CSS nested rule '%s' is missing a closing brace",header.c_str());
                break;
            }
            if(!header.empty()) {
                result.nested.push_back(CSSNestedBlock{
                    header,
                    source.substr(cursor+1,close-cursor-1)
                });
            }
            cursor=close+1;
            statement_start=cursor;
            continue;
        }
        ++cursor;
    }

    const std::string tail=TrimCSS(source.substr(statement_start));
    if(!tail.empty()) {
        if(auto declaration=ParseSingleRawDeclaration(tail)) {
            result.declarations.push_back(std::move(*declaration));
        }
    }
    return result;
}

std::string ReplaceCSSNestingAmpersands(
    const std::string& child,
    const std::string& parent
) {
    std::string result;
    result.reserve(child.size()+parent.size());
    bool replaced=false;
    char quote='\0';
    bool escaped=false;
    for(char c:child) {
        if(quote!='\0') {
            result.push_back(c);
            if(escaped) escaped=false;
            else if(c=='\\') escaped=true;
            else if(c==quote) quote='\0';
            continue;
        }
        if(c=='\'' || c=='"') { quote=c; result.push_back(c); continue; }
        if(c=='&') { result+=parent; replaced=true; }
        else result.push_back(c);
    }
    if(!replaced) {
        const std::string trimmed=TrimCSS(child);
        if(trimmed.empty()) return parent;
        if(trimmed.front()=='>' || trimmed.front()=='+' || trimmed.front()=='~') {
            return parent+" "+trimmed;
        }
        return parent+" "+trimmed;
    }
    return TrimCSS(result);
}

std::vector<std::string> ExpandNestedSelectorGroups(
    const std::vector<std::string>& parents,
    const std::vector<std::string>& children
) {
    if(parents.empty()) return children;
    std::vector<std::string> result;
    result.reserve(parents.size()*children.size());
    for(const std::string& parent:parents) {
        for(const std::string& child:children) {
            result.push_back(ReplaceCSSNestingAmpersands(child,parent));
        }
    }
    return result;
}

void EmitCompiledCSSRule(
    const std::vector<std::string>& selectors,
    const CSSBlockContent& content,
    const CSSVariableMap& root_variables,
    StyleSheet& sheet,
    const std::vector<UIMediaQuery>& media
) {
    if(selectors.empty()) return;

    CSSVariableMap parse_variables=root_variables;
    std::vector<UICustomPropertyDeclaration> custom_properties;
    std::vector<UIShaderUniformBinding> shader_uniforms;
    for(const auto& raw:content.declarations) {
        if(IsCSSCustomProperty(raw.property_name)) {
            custom_properties.push_back({raw.property_name,raw.property_value});
            parse_variables[raw.property_name]=raw.property_value;
            continue;
        }
        const std::string lowered=LowerCSS(TrimCSS(raw.property_name));
        if(lowered.rfind("shader-",0)==0 && raw.property_name.size()>7) {
            shader_uniforms.push_back({TrimCSS(raw.property_name.substr(7)),raw.property_value});
        }
    }

    std::vector<Declaration> declarations;
    std::vector<UIComponentStyleDeclaration> component_style_properties;
    for(const auto& raw:content.declarations) {
        if(IsCSSCustomProperty(raw.property_name) || IsAnimationMetaProperty(raw.property_name)) continue;
        const std::string lowered=LowerCSS(TrimCSS(raw.property_name));
        if(lowered.rfind("shader-",0)==0) continue;
        const auto property=ParsePropertyType(raw.property_name);
        if(!property.has_value()) {
            // Preserve unknown names as descriptor-driven component style
            // declarations. A registered UIComponent decides whether the
            // property is valid; ordinary elements simply ignore it.
            component_style_properties.push_back({TrimCSS(raw.property_name),TrimCSS(raw.property_value)});
            continue;
        }
        if(!AppendCSSDeclaration(declarations,*property,raw.property_value)) {
            TraceLog(LOG_WARNING,"Invalid value '%s' for property '%s'",
                raw.property_value.c_str(),raw.property_name.c_str());
        }
    }

    bool transitions_declared=false;
    std::vector<UITransitionSpec> transitions;
    ApplyTransitionLonghands(content.declarations,parse_variables,transitions_declared,transitions);
    bool animations_declared=false;
    std::vector<UIAnimationSpec> animations;
    ApplyAnimationLonghands(content.declarations,parse_variables,animations_declared,animations);

    for(const std::string& selector:selectors) {
        if(IsCSSRootSelector(selector)) continue;
        StyleRule rule=ParseRuleSelector(selector);
        rule.declarations=declarations;
        rule.custom_properties=custom_properties;
        rule.component_style_properties=component_style_properties;
        rule.shader_uniforms=shader_uniforms;
        rule.transitions_declared=transitions_declared;
        rule.transitions=transitions;
        rule.animations_declared=animations_declared;
        rule.animations=animations;
        rule.media_queries=media;
        sheet.rules.push_back(std::move(rule));
    }
}

void CompileCSSContent(
    const std::string& source,
    const std::vector<std::string>& parent_selectors,
    const CSSVariableMap& root_variables,
    StyleSheet& sheet,
    const std::vector<UIMediaQuery>& media
);

void CompileNestedCSSBlock(
    const CSSNestedBlock& block,
    const std::vector<std::string>& parent_selectors,
    const CSSVariableMap& root_variables,
    StyleSheet& sheet,
    const std::vector<UIMediaQuery>& media
) {
    const std::string lowered=LowerCSS(TrimCSS(block.header));
    if(lowered.rfind("@media",0)==0) {
        const auto child_media=ParseUIMediaQueries(block.header.substr(6));
        if(!child_media.empty()) {
            CompileCSSContent(block.body,parent_selectors,root_variables,sheet,
                CombineMediaQueries(media,child_media));
        }
        return;
    }
    if(!lowered.empty() && lowered.front()=='@') {
        TraceLog(LOG_WARNING,"Unsupported nested CSS at-rule: %s",block.header.c_str());
        return;
    }

    const auto children=SplitCSSSelectors(block.header);
    const auto selectors=ExpandNestedSelectorGroups(parent_selectors,children);
    const CSSBlockContent content=ParseCSSBlockContent(block.body);
    EmitCompiledCSSRule(selectors,content,root_variables,sheet,media);
    for(const CSSNestedBlock& nested:content.nested) {
        CompileNestedCSSBlock(nested,selectors,root_variables,sheet,media);
    }
}

void CompileCSSContent(
    const std::string& source,
    const std::vector<std::string>& parent_selectors,
    const CSSVariableMap& root_variables,
    StyleSheet& sheet,
    const std::vector<UIMediaQuery>& media
) {
    const CSSBlockContent content=ParseCSSBlockContent(source);
    // Declarations directly inside a nested @media belong to its parent selector.
    if(!parent_selectors.empty() && !content.declarations.empty()) {
        EmitCompiledCSSRule(parent_selectors,content,root_variables,sheet,media);
    }
    for(const CSSNestedBlock& block:content.nested) {
        CompileNestedCSSBlock(block,parent_selectors,root_variables,sheet,media);
    }
}

void AppendCSSRules(
    const std::string& source,
    const CSSVariableMap& root_variables,
    StyleSheet& sheet,
    const std::vector<UIMediaQuery>& inherited_media = {}
) {
    CompileCSSContent(source,{},root_variables,sheet,inherited_media);
}

} // namespace


StyleSheet ParseStyleSheet(
    const std::string& css
) {
    // A tiny user-agent stylesheet keeps an empty or missing CSS file usable.
    StyleSheet sheet = MakeDefaultUIStyleSheet();
    std::string source = RemoveCSSComments(css);
    CSSVariableMap root_variables =
        CollectCSSRootVariables(source);
    if(!root_variables.empty()) {
        sheet.root_custom_properties =
            std::make_shared<const UIStyleVariableMap>(root_variables);
    }
    ExtractCSSKeyframes(source,root_variables,sheet);

    AppendCSSRules(source,root_variables,sheet);

    // Give every parsed stylesheet a fresh cache generation.
    TouchStyleSheet(sheet);
    return sheet;
}
