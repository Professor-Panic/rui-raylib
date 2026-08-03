#include "css_parser.h"

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
    value = LowerCSS(TrimCSS(value));

    if(value.empty()) {
        return std::nullopt;
    }

    if(value == "auto") {
        return CSSLength{0.0f,LengthUnit::Auto};
    }

    LengthUnit unit = LengthUnit::Pixels;

    if(EndsWithCSS(value,"px")) {
        value.resize(value.size() - 2);
        unit = LengthUnit::Pixels;
    }
    else if(EndsWithCSS(value,"vw")) {
        value.resize(value.size() - 2);
        unit = LengthUnit::ViewWidth;
    }
    else if(EndsWithCSS(value,"vh")) {
        value.resize(value.size() - 2);
        unit = LengthUnit::ViewHeight;
    }
    else if(EndsWithCSS(value,"%")) {
        value.resize(value.size() - 1);
        unit = LengthUnit::Percent;
    }

    auto number = ParseStrictCSSFloat(
        TrimCSS(value)
    );

    if(!number.has_value()) {
        return std::nullopt;
    }

    return CSSLength{
        *number,
        unit
    };
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

    if(property == PropertyType::InputSliderValueWidth) {
        auto number = ParseCSSNumber(value);

        if(number.has_value() && *number >= 0.0f) {
            return PropertyValueType(*number);
        }

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
        if(
            value == "auto" &&
            property != PropertyType::Width &&
            property != PropertyType::Height &&
            property != PropertyType::FlexBasis
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

bool AppendCSSDeclaration(
    std::vector<Declaration>& declarations,
    PropertyType property,
    const std::string& raw
) {
    if(
        property == PropertyType::Padding ||
        property == PropertyType::Margin
    ) {
        auto values = ParseCSSBoxShorthand(raw);

        if(!values.has_value()) {
            return false;
        }

        PropertyType top = property == PropertyType::Padding
            ? PropertyType::PaddingTop
            : PropertyType::MarginTop;
        PropertyType right = property == PropertyType::Padding
            ? PropertyType::PaddingRight
            : PropertyType::MarginRight;
        PropertyType bottom = property == PropertyType::Padding
            ? PropertyType::PaddingBottom
            : PropertyType::MarginBottom;
        PropertyType left = property == PropertyType::Padding
            ? PropertyType::PaddingLeft
            : PropertyType::MarginLeft;

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

StyleRule ParseRuleSelector(
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

        if(part_name == "value-box") {
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

    rule.selector = ParseSelectorType(
        selector.substr(0,type_end)
    );

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
    std::size_t cursor = 0;

    while(cursor <= selectors.size()) {
        std::size_t comma =
            selectors.find(',',cursor);

        std::string selector = TrimCSS(
            selectors.substr(
                cursor,
                comma == std::string::npos
                    ? std::string::npos
                    : comma - cursor
            )
        );

        if(!selector.empty()) {
            result.push_back(selector);
        }

        if(comma == std::string::npos) {
            break;
        }

        cursor = comma + 1;
    }

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
    std::size_t cursor = 0;

    while(cursor < source.size()) {
        std::size_t open = source.find('{',cursor);

        if(open == std::string::npos) {
            break;
        }

        std::size_t close = source.find('}',open + 1);

        if(close == std::string::npos) {
            break;
        }

        std::string selector_text = TrimCSS(
            source.substr(cursor,open - cursor)
        );

        auto selectors = SplitCSSSelectors(selector_text);
        bool is_root_rule = std::any_of(
            selectors.begin(),
            selectors.end(),
            [](const std::string& selector) {
                return IsCSSRootSelector(selector);
            }
        );

        if(is_root_rule) {
            std::string body = source.substr(
                open + 1,
                close - open - 1
            );

            for(
                const auto& declaration :
                ParseRawCSSDeclarations(body)
            ) {
                if(IsCSSCustomProperty(
                    declaration.property_name
                )) {
                    variables[declaration.property_name] =
                        declaration.property_value;
                }
            }
        }

        cursor = close + 1;
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
            const std::string property_name = LowerCSS(TrimCSS(token));
            if(property_name == "all") {
                spec.all = true;
                spec.properties.clear();
            }
            else if(auto property = ParsePropertyType(property_name)) {
                spec.all = false;
                spec.properties = {*property};
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
        const std::string property = LowerCSS(TrimCSS(property_values[index%property_values.size()]));
        if(property != "all") {
            auto parsed = ParsePropertyType(property);
            if(!parsed.has_value()) continue;
            spec.all = false;
            spec.properties = {*parsed};
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

std::vector<Declaration> ParseKeyframeDeclarations(
    const std::string& body,
    const CSSVariableMap& variables
) {
    std::vector<Declaration> declarations;
    for(const auto& raw : ParseRawCSSDeclarations(body)) {
        if(IsCSSCustomProperty(raw.property_name)) continue;
        auto property = ParsePropertyType(raw.property_name);
        if(!property.has_value()) continue;
        auto value = ResolveCSSVariables(raw.property_value,variables);
        if(value.has_value()) AppendCSSDeclaration(declarations,*property,*value);
    }
    return declarations;
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
            const auto declarations = ParseKeyframeDeclarations(
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
                if(offset >= 0.0f) frames.steps.push_back({offset,declarations});
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

} // namespace


StyleSheet ParseStyleSheet(
    const std::string& css
) {
    // A tiny user-agent stylesheet keeps an empty or missing CSS file usable.
    StyleSheet sheet = MakeDefaultUIStyleSheet();
    std::string source = RemoveCSSComments(css);
    CSSVariableMap root_variables =
        CollectCSSRootVariables(source);
    ExtractCSSKeyframes(source,root_variables,sheet);

    std::size_t cursor = 0;

    while(cursor < source.size()) {
        std::size_t open = source.find('{',cursor);

        if(open == std::string::npos) {
            break;
        }

        std::size_t close = source.find('}',open + 1);

        if(close == std::string::npos) {
            TraceLog(
                LOG_WARNING,
                "CSS rule is missing a closing brace"
            );

            break;
        }

        std::string selector_text = TrimCSS(
            source.substr(cursor,open - cursor)
        );

        std::vector<std::string> selectors =
            SplitCSSSelectors(selector_text);

        std::string body = source.substr(
            open + 1,
            close - open - 1
        );

        auto raw_declarations =
            ParseRawCSSDeclarations(body);

        CSSVariableMap rule_variables = root_variables;

        // Variables declared in a rule are available to every
        // declaration in that same rule, regardless of order.
        for(const auto& declaration : raw_declarations) {
            if(IsCSSCustomProperty(
                declaration.property_name
            )) {
                rule_variables[declaration.property_name] =
                    declaration.property_value;
            }
        }

        std::vector<Declaration> declarations;

        for(const auto& declaration : raw_declarations) {
            if(IsCSSCustomProperty(
                declaration.property_name
            ) || IsAnimationMetaProperty(declaration.property_name)) {
                continue;
            }

            auto property = ParsePropertyType(
                declaration.property_name
            );

            if(!property.has_value()) {
                TraceLog(
                    LOG_WARNING,
                    "Unknown CSS property: %s",
                    declaration.property_name.c_str()
                );

                continue;
            }

            auto resolved_value = ResolveCSSVariables(
                declaration.property_value,
                rule_variables
            );

            if(!resolved_value.has_value()) {
                TraceLog(
                    LOG_WARNING,
                    "Unable to resolve variables in value '%s' for property '%s'",
                    declaration.property_value.c_str(),
                    declaration.property_name.c_str()
                );

                continue;
            }

            if(!AppendCSSDeclaration(
                declarations,
                *property,
                *resolved_value
            )) {
                TraceLog(
                    LOG_WARNING,
                    "Invalid value '%s' for property '%s'",
                    resolved_value->c_str(),
                    declaration.property_name.c_str()
                );
            }
        }

        bool transitions_declared = false;
        std::vector<UITransitionSpec> transitions;
        ApplyTransitionLonghands(
            raw_declarations,rule_variables,transitions_declared,transitions
        );
        bool animations_declared = false;
        std::vector<UIAnimationSpec> animations;
        ApplyAnimationLonghands(
            raw_declarations,rule_variables,animations_declared,animations
        );

        for(const auto& selector : selectors) {
            // :root only provides global custom properties.
            if(IsCSSRootSelector(selector)) {
                continue;
            }

            StyleRule rule = ParseRuleSelector(selector);
            rule.declarations = declarations;
            rule.transitions_declared = transitions_declared;
            rule.transitions = transitions;
            rule.animations_declared = animations_declared;
            rule.animations = animations;

            sheet.rules.push_back(std::move(rule));
        }

        cursor = close + 1;
    }

    // Give every parsed stylesheet a fresh cache generation.
    TouchStyleSheet(sheet);
    return sheet;
}
