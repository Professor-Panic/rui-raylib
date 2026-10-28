#include "css_emulator.h"
#include "css_parser.h"

bool HasCSSId(const CSS_Id& id) {
    return !std::holds_alternative<std::monostate>(id);
}

bool HasCSSClass(const std::vector<std::string>& classes, const std::string& class_name) {
    return std::find(classes.begin(), classes.end(), class_name) != classes.end();
}

void AddCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    if(!class_name.empty() && !HasCSSClass(classes, class_name)) {
        classes.push_back(class_name);
    }
}

bool RemoveCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    auto iterator = std::remove(classes.begin(), classes.end(), class_name);

    if(iterator == classes.end()) {
        return false;
    }

    classes.erase(iterator, classes.end());
    return true;
}

bool ToggleCSSClass(std::vector<std::string>& classes, const std::string& class_name) {
    if(HasCSSClass(classes, class_name)) {
        RemoveCSSClass(classes, class_name);
        return false;
    }

    AddCSSClass(classes, class_name);
    return true;
}

void ClearCSSClasses(std::vector<std::string>& classes) {
    classes.clear();
}

Color WithOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(
        std::clamp(opacity, 0.0f, 1.0f) * 255.0f
    );

    return color;
}

Color MixColors(Color first, Color second, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);

    return Color{
        static_cast<unsigned char>(Lerp(first.r, second.r, amount)),
        static_cast<unsigned char>(Lerp(first.g, second.g, amount)),
        static_cast<unsigned char>(Lerp(first.b, second.b, amount)),
        static_cast<unsigned char>(Lerp(first.a, second.a, amount))
    };
}

Color LightenColor(Color color, float amount) {
    return MixColors(color, WHITE, amount);
}

Color DarkenColor(Color color, float amount) {
    return MixColors(color, BLACK, amount);
}

Color ApplyStyleOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(
        static_cast<float>(color.a) * std::clamp(opacity,0.0f,1.0f)
    );
    return color;
}

EdgeValues EffectiveBorderWidths(const ComputedStyle& style) {
    EdgeValues widths = style.border_widths;
    const bool no_side_source =
        !style.border_width_lengths.left.has_value() &&
        !style.border_width_lengths.right.has_value() &&
        !style.border_width_lengths.top.has_value() &&
        !style.border_width_lengths.bottom.has_value();
    const bool side_values_are_zero =
        widths.left == 0.0f && widths.right == 0.0f &&
        widths.top == 0.0f && widths.bottom == 0.0f;

    if(no_side_source && side_values_are_zero && style.border_width != 0.0f) {
        widths = EdgeValues(style.border_width);
    }

    widths.left = std::max(0.0f,widths.left);
    widths.right = std::max(0.0f,widths.right);
    widths.top = std::max(0.0f,widths.top);
    widths.bottom = std::max(0.0f,widths.bottom);
    return widths;
}

float MaximumBorderWidth(const ComputedStyle& style) {
    const EdgeValues widths = EffectiveBorderWidths(style);
    return std::max({widths.left,widths.right,widths.top,widths.bottom});
}

namespace {

UIBorderStyle BorderStyleFromCSSValue(const std::string& value) {
    if(value == "none") return UIBorderStyle::None;
    if(value == "dashed") return UIBorderStyle::Dashed;
    if(value == "dotted") return UIBorderStyle::Dotted;
    if(value == "double") return UIBorderStyle::Double;
    return UIBorderStyle::Solid;
}

void DrawPatternedBorderLine(
    Vector2 start,
    Vector2 end,
    float thickness,
    UIBorderStyle border_style,
    Color color
) {
    if(thickness <= 0.0f) return;

    const float dx = end.x-start.x;
    const float dy = end.y-start.y;
    const float length = std::sqrt(dx*dx+dy*dy);
    if(length <= 0.0001f) return;

    const Vector2 direction = {dx/length,dy/length};

    if(border_style == UIBorderStyle::Dotted) {
        const float radius = std::max(0.5f,thickness*0.5f);
        const float step = std::max(2.0f,thickness*2.5f);
        for(float distance = 0.0f; distance <= length; distance += step) {
            DrawCircleV(
                {
                    start.x+direction.x*distance,
                    start.y+direction.y*distance
                },
                radius,
                color
            );
        }
        return;
    }

    const float dash = std::max(4.0f,thickness*3.0f);
    const float gap = std::max(2.0f,thickness*2.0f);
    for(float distance = 0.0f; distance < length; distance += dash+gap) {
        const float dash_end_distance = std::min(length,distance+dash);
        DrawLineEx(
            {
                start.x+direction.x*distance,
                start.y+direction.y*distance
            },
            {
                start.x+direction.x*dash_end_distance,
                start.y+direction.y*dash_end_distance
            },
            thickness,
            color
        );
    }
}

void DrawPatternedBorder(
    Rectangle rectangle,
    const EdgeValues& widths,
    UIBorderStyle border_style,
    Color color
) {
    if(widths.top > 0.0f) {
        const float y = rectangle.y+widths.top*0.5f;
        DrawPatternedBorderLine(
            {rectangle.x,y},{rectangle.x+rectangle.width,y},
            widths.top,border_style,color
        );
    }
    if(widths.right > 0.0f) {
        const float x = rectangle.x+rectangle.width-widths.right*0.5f;
        DrawPatternedBorderLine(
            {x,rectangle.y},{x,rectangle.y+rectangle.height},
            widths.right,border_style,color
        );
    }
    if(widths.bottom > 0.0f) {
        const float y = rectangle.y+rectangle.height-widths.bottom*0.5f;
        DrawPatternedBorderLine(
            {rectangle.x+rectangle.width,y},{rectangle.x,y},
            widths.bottom,border_style,color
        );
    }
    if(widths.left > 0.0f) {
        const float x = rectangle.x+widths.left*0.5f;
        DrawPatternedBorderLine(
            {x,rectangle.y+rectangle.height},{x,rectangle.y},
            widths.left,border_style,color
        );
    }
}

void DrawDoubleBorderSide(
    Rectangle rectangle,
    float width,
    int side,
    Color color
) {
    if(width <= 0.0f) return;

    const float band = std::max(0.25f,width/3.0f);

    switch(side) {
        case 0: // top
            DrawRectangleRec({rectangle.x,rectangle.y,rectangle.width,band},color);
            DrawRectangleRec({
                rectangle.x,rectangle.y+width-band,rectangle.width,band
            },color);
            break;
        case 1: // right
            DrawRectangleRec({
                rectangle.x+rectangle.width-band,rectangle.y,band,rectangle.height
            },color);
            DrawRectangleRec({
                rectangle.x+rectangle.width-width,rectangle.y,band,rectangle.height
            },color);
            break;
        case 2: // bottom
            DrawRectangleRec({
                rectangle.x,rectangle.y+rectangle.height-band,rectangle.width,band
            },color);
            DrawRectangleRec({
                rectangle.x,rectangle.y+rectangle.height-width,rectangle.width,band
            },color);
            break;
        case 3: // left
            DrawRectangleRec({rectangle.x,rectangle.y,band,rectangle.height},color);
            DrawRectangleRec({
                rectangle.x+width-band,rectangle.y,band,rectangle.height
            },color);
            break;
    }
}

}

void DrawStyleBorder(
    Rectangle rectangle,
    const ComputedStyle& style,
    Color color,
    int segments
) {
    if(
        style.border_style == UIBorderStyle::None ||
        color.a == 0 ||
        rectangle.width <= 0.0f ||
        rectangle.height <= 0.0f
    ) {
        return;
    }

    const EdgeValues widths = EffectiveBorderWidths(style);
    constexpr float epsilon = 0.0001f;
    const bool uniform =
        std::abs(widths.left-widths.right) <= epsilon &&
        std::abs(widths.left-widths.top) <= epsilon &&
        std::abs(widths.left-widths.bottom) <= epsilon;

    const float border_radius_pixels =
        ResolveUIBorderRadiusPixels(rectangle,style.border_radius);
    const float border_roundness =
        ResolveUIBorderRoundness(rectangle,style.border_radius);

    if(style.border_style == UIBorderStyle::Dashed ||
       style.border_style == UIBorderStyle::Dotted) {
        DrawPatternedBorder(rectangle,widths,style.border_style,color);
        return;
    }

    if(style.border_style == UIBorderStyle::Double) {
        if(uniform) {
            const float width = widths.left;
            if(width <= 0.0f) return;

            const float band = std::max(0.25f,width/3.0f);
            DrawRectangleRoundedLinesEx(
                rectangle,border_roundness,segments,band,color
            );

            const float inset = std::max(band,width-band);
            Rectangle inner = {
                rectangle.x+inset,
                rectangle.y+inset,
                std::max(0.0f,rectangle.width-inset*2.0f),
                std::max(0.0f,rectangle.height-inset*2.0f)
            };
            if(inner.width > 0.0f && inner.height > 0.0f) {
                const float inner_radius_pixels =
                    std::max(0.0f,border_radius_pixels-inset);
                const float inner_roundness = inner.width > 0.0f && inner.height > 0.0f
                    ? std::clamp(
                        (inner_radius_pixels*2.0f)/std::min(inner.width,inner.height),
                        0.0f,
                        1.0f
                    )
                    : 0.0f;
                DrawRectangleRoundedLinesEx(
                    inner,inner_roundness,segments,band,color
                );
            }
            return;
        }

        DrawDoubleBorderSide(rectangle,std::min(widths.top,rectangle.height),0,color);
        DrawDoubleBorderSide(rectangle,std::min(widths.right,rectangle.width),1,color);
        DrawDoubleBorderSide(rectangle,std::min(widths.bottom,rectangle.height),2,color);
        DrawDoubleBorderSide(rectangle,std::min(widths.left,rectangle.width),3,color);
        return;
    }

    // Solid preserves the pre-border-style renderer exactly.
    if(uniform) {
        if(widths.left > 0.0f) {
            DrawRectangleRoundedLinesEx(
                rectangle,border_roundness,segments,widths.left,color
            );
        }
        return;
    }

    const float left = std::min(widths.left,rectangle.width);
    const float right = std::min(widths.right,rectangle.width);
    const float top = std::min(widths.top,rectangle.height);
    const float bottom = std::min(widths.bottom,rectangle.height);

    if(top > 0.0f) {
        DrawRectangleRec({rectangle.x,rectangle.y,rectangle.width,top},color);
    }
    if(right > 0.0f) {
        DrawRectangleRec({
            rectangle.x+rectangle.width-right,rectangle.y,right,rectangle.height
        },color);
    }
    if(bottom > 0.0f) {
        DrawRectangleRec({
            rectangle.x,rectangle.y+rectangle.height-bottom,rectangle.width,bottom
        },color);
    }
    if(left > 0.0f) {
        DrawRectangleRec({rectangle.x,rectangle.y,left,rectangle.height},color);
    }
}

float GetTextX(Rectangle content, float text_width, TextAlign alignment) {
    if(alignment == TextAlign::Left) return content.x;
    if(alignment == TextAlign::Right) return content.x + content.width - text_width;
    return content.x + (content.width - text_width) * 0.5f;
}

bool HasAllCSSClasses(
    const std::vector<std::string>& element_classes,
    const std::vector<std::string>& rule_classes
) {
    return std::all_of(rule_classes.begin(), rule_classes.end(), [&](const std::string& class_name) {
        return HasCSSClass(element_classes, class_name);
    });
}
bool IsSyntheticCSSPartClass(const std::string& class_name) {
    return class_name.rfind("__css-", 0) == 0;
}

bool HasSyntheticCSSPartClass(
    const std::vector<std::string>& classes
) {
    return std::any_of(
        classes.begin(),
        classes.end(),
        [](const std::string& class_name) {
            return IsSyntheticCSSPartClass(class_name);
        }
    );
}
std::string LegacySelectorStateName(SelectorState state) {
    switch(state) {
        case SelectorState::Hover: return "hover";
        case SelectorState::Active: return "active";
        case SelectorState::Focus: return "focus";
        case SelectorState::Disabled: return "disabled";
        default: return {};
    }
}

bool SelectorIdentityMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    std::size_t sibling_index,
    std::size_t sibling_count
) {
    if(
        rule.selector != SelectorType::Any &&
        rule.selector != selector_type
    ) {
        return false;
    }

    if(!StructuralPseudosMatch(rule.structural,sibling_index,sibling_count)) {
        return false;
    }

    if(HasCSSId(rule.id) && rule.id != id) {
        return false;
    }

    const bool element_is_part =
        HasSyntheticCSSPartClass(classes);

    const bool rule_targets_part =
        HasSyntheticCSSPartClass(rule.classes);

    // A normal input selector must not restyle an input pseudo-element.
    if(element_is_part && !rule_targets_part) {
        return false;
    }

    return HasAllCSSClasses(classes, rule.classes);
}

bool StyleSelectorMatches(
    const StyleSelector& required,
    const StyleSelector& actual
) {
    if(!StructuralPseudosMatch(
        required.structural,actual.sibling_index,actual.sibling_count
    )) return false;

    if(
        required.selector != SelectorType::Any &&
        required.selector != actual.selector
    ) return false;

    if(HasCSSId(required.id) && required.id != actual.id) {
        return false;
    }

    if(!HasAllCSSClasses(actual.classes,required.classes)) {
        return false;
    }

    return std::all_of(
        required.pseudo_states.begin(),
        required.pseudo_states.end(),
        [&](const std::string& required_state) {
            if(std::find(
                actual.pseudo_states.begin(),
                actual.pseudo_states.end(),
                required_state
            ) != actual.pseudo_states.end()) {
                return true;
            }

            const bool already_normalized =
                !required_state.empty() &&
                required_state.front() != ':' &&
                !std::isspace(static_cast<unsigned char>(required_state.front())) &&
                !std::isspace(static_cast<unsigned char>(required_state.back())) &&
                std::none_of(
                    required_state.begin(),required_state.end(),
                    [](unsigned char character) {
                        return std::isupper(character) != 0;
                    }
                );
            if(already_normalized) return false;

            const std::string normalized =
                NormalizeUIPseudoStateName(required_state);
            return std::find(
                actual.pseudo_states.begin(),
                actual.pseudo_states.end(),
                normalized
            ) != actual.pseudo_states.end();
        }
    );
}

bool MediaQueryMatches(const UIMediaQuery& query) {
    const float width = CSS_UI_VIEWPORT_WIDTH > 0.0f
        ? CSS_UI_VIEWPORT_WIDTH
        : static_cast<float>(GetScreenWidth());
    const float height = CSS_UI_VIEWPORT_HEIGHT > 0.0f
        ? CSS_UI_VIEWPORT_HEIGHT
        : static_cast<float>(GetScreenHeight());
    if(query.min_width && width < *query.min_width) return false;
    if(query.max_width && width > *query.max_width) return false;
    if(query.min_height && height < *query.min_height) return false;
    if(query.max_height && height > *query.max_height) return false;
    if(query.orientation == "landscape" && width < height) return false;
    if(query.orientation == "portrait" && height < width) return false;
    return true;
}

bool RuleMediaMatches(const StyleRule& rule) {
    return rule.media_queries.empty() || std::any_of(
        rule.media_queries.begin(),rule.media_queries.end(),MediaQueryMatches
    );
}

bool ComplexSelectorMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    const std::vector<StyleSelector>& ancestors,
    const std::vector<StyleSelectorSiblingChain>&
        ancestor_previous_siblings,
    StyleSelectorSiblingChain previous_siblings,
    std::size_t sibling_index = 0,
    std::size_t sibling_count = 0
) {
    if(!RuleMediaMatches(rule)) return false;
    if(!SelectorIdentityMatches(rule,selector_type,id,classes,sibling_index,sibling_count)) return false;
    if(rule.ancestors.empty()) return true;

    struct Position {
        std::size_t depth = 0;
        StyleSelectorSiblingChain sibling;
        bool on_ancestor_path = true;
    };

    const std::size_t target_depth = ancestors.size();

    auto path_siblings_at =
        [&](std::size_t depth) -> StyleSelectorSiblingChain {
            if(depth == target_depth) return previous_siblings;
            if(depth < ancestor_previous_siblings.size()) {
                return ancestor_previous_siblings[depth];
            }
            return {};
        };

    auto selector_at =
        [&](const Position& position) -> const StyleSelector* {
            if(!position.on_ancestor_path) {
                return position.sibling
                    ? &position.sibling->selector
                    : nullptr;
            }
            if(position.depth < ancestors.size()) {
                return &ancestors[position.depth];
            }
            return nullptr;
        };

    auto preceding_siblings =
        [&](const Position& position) -> StyleSelectorSiblingChain {
            if(position.on_ancestor_path) {
                return path_siblings_at(position.depth);
            }
            return position.sibling
                ? position.sibling->previous
                : StyleSelectorSiblingChain{};
        };

    Position current{target_depth,{},true};

    for(
        std::size_t required_index=rule.ancestors.size();
        required_index>0;
        --required_index
    ) {
        const std::size_t index = required_index-1;
        const StyleSelector& required = rule.ancestors[index];
        const SelectorCombinator combinator =
            index < rule.combinators.size()
                ? rule.combinators[index]
                : SelectorCombinator::Descendant;
        bool matched = false;

        if(combinator == SelectorCombinator::Child) {
            if(current.depth > 0) {
                Position candidate{current.depth-1,{},true};
                const StyleSelector* actual = selector_at(candidate);
                if(
                    actual != nullptr &&
                    StyleSelectorMatches(required,*actual)
                ) {
                    current = std::move(candidate);
                    matched = true;
                }
            }
        }
        else if(combinator == SelectorCombinator::Descendant) {
            for(std::size_t depth=current.depth; depth>0; --depth) {
                Position candidate{depth-1,{},true};
                const StyleSelector* actual = selector_at(candidate);
                if(
                    actual != nullptr &&
                    StyleSelectorMatches(required,*actual)
                ) {
                    current = std::move(candidate);
                    matched = true;
                    break;
                }
            }
        }
        else {
            StyleSelectorSiblingChain siblings =
                preceding_siblings(current);

            if(combinator == SelectorCombinator::AdjacentSibling) {
                if(
                    siblings &&
                    StyleSelectorMatches(required,siblings->selector)
                ) {
                    current = {
                        current.depth,
                        std::move(siblings),
                        false
                    };
                    matched = true;
                }
            }
            else {
                for(
                    StyleSelectorSiblingChain sibling =
                        std::move(siblings);
                    sibling;
                    sibling = sibling->previous
                ) {
                    if(StyleSelectorMatches(required,sibling->selector)) {
                        current = {
                            current.depth,
                            std::move(sibling),
                            false
                        };
                        matched = true;
                        break;
                    }
                }
            }
        }

        if(!matched) return false;
    }

    return true;
}

bool SelectorMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    const std::vector<StyleSelector>& ancestors,
    const std::vector<StyleSelectorSiblingChain>&
        ancestor_previous_siblings,
    StyleSelectorSiblingChain previous_siblings,
    std::size_t sibling_index,
    std::size_t sibling_count
) {
    if(rule.pseudo_element != UIGeneratedPseudoKind::None) return false;
    if(!ComplexSelectorMatches(
        rule,selector_type,id,classes,ancestors,
        ancestor_previous_siblings,previous_siblings,
        sibling_index,sibling_count
    )) return false;
    if(!rule.pseudo_states.empty()) {
        const std::string requested = LegacySelectorStateName(selector_state);
        return rule.pseudo_states.size() == 1 && rule.pseudo_states.front() == requested;
    }
    return rule.state == selector_state;
}

bool SelectorMatchesPseudoStates(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& active_states,
    const std::vector<StyleSelector>& ancestors,
    const std::vector<StyleSelectorSiblingChain>&
        ancestor_previous_siblings,
    StyleSelectorSiblingChain previous_siblings,
    std::size_t sibling_index,
    std::size_t sibling_count
) {
    if(rule.pseudo_element != UIGeneratedPseudoKind::None) return false;
    if(!ComplexSelectorMatches(
        rule,selector_type,id,classes,ancestors,
        ancestor_previous_siblings,previous_siblings,
        sibling_index,sibling_count
    )) return false;
    if(!rule.pseudo_states.empty()) {
        return std::all_of(
            rule.pseudo_states.begin(),rule.pseudo_states.end(),
            [&](const std::string& state) {
                return std::binary_search(
                    active_states.begin(),active_states.end(),state
                );
            }
        );
    }
    if(rule.state == SelectorState::None) return true;
    return std::binary_search(
        active_states.begin(),active_states.end(),
        LegacySelectorStateName(rule.state)
    );
}

int GetSelectorSpecificity(const StyleRule& rule) {
    int specificity = 0;

    if(HasCSSId(rule.id)) {
        specificity += 100;
    }

    specificity += static_cast<int>(rule.classes.size()) * 10;
    specificity += static_cast<int>(
        rule.pseudo_states.empty()
            ? (rule.state == SelectorState::None ? 0 : 1)
            : rule.pseudo_states.size()
    ) * 10;
    specificity += static_cast<int>(rule.structural.size()) * 10;

    if(rule.selector != SelectorType::Any) {
        specificity += 1;
    }
    if(rule.pseudo_element != UIGeneratedPseudoKind::None) {
        specificity += 1;
    }

    for(const StyleSelector& ancestor : rule.ancestors) {
        if(HasCSSId(ancestor.id)) specificity += 100;
        specificity += static_cast<int>(ancestor.classes.size()) * 10;
        specificity += static_cast<int>(ancestor.pseudo_states.size()+ancestor.structural.size()) * 10;
        if(ancestor.selector != SelectorType::Any) specificity += 1;
    }

    return specificity;
}

std::optional<CSSLength> ResolveLengthValue(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    PropertyType property_type
) {
    auto length = ResolveValue<CSSLength>(
        style_sheet,
        selector_type,
        id,
        classes,
        selector_state,
        property_type
    );

    if(length.has_value()) {
        return length;
    }

    auto number = ResolveValue<float>(
        style_sheet,
        selector_type,
        id,
        classes,
        selector_state,
        property_type
    );

    if(number.has_value()) {
        return CSSLength{*number,LengthUnit::Pixels};
    }

    return std::nullopt;
}

namespace {

class CSSLengthExpressionEvaluator {
public:
    CSSLengthExpressionEvaluator(std::string_view source,float percent_reference)
        : source_(source), percent_reference_(percent_reference) {}

    std::optional<float> Evaluate() {
        const float value = ParseExpression();
        SkipWhitespace();
        if(!valid_ || position_ != source_.size()) return std::nullopt;
        return value;
    }

private:
    void SkipWhitespace() {
        while(position_ < source_.size() &&
              std::isspace(static_cast<unsigned char>(source_[position_]))) {
            ++position_;
        }
    }

    bool Consume(char character) {
        SkipWhitespace();
        if(position_ >= source_.size() || source_[position_] != character) return false;
        ++position_;
        return true;
    }

    float ParseExpression() {
        float value = ParseTerm();
        while(valid_) {
            SkipWhitespace();
            if(Consume('+')) value += ParseTerm();
            else if(Consume('-')) value -= ParseTerm();
            else break;
        }
        return value;
    }

    float ParseTerm() {
        float value = ParseUnary();
        while(valid_) {
            SkipWhitespace();
            if(Consume('*')) value *= ParseUnary();
            else if(Consume('/')) {
                const float divisor = ParseUnary();
                value = std::abs(divisor) <= 0.000001f ? 0.0f : value/divisor;
            }
            else break;
        }
        return value;
    }

    float ParseUnary() {
        SkipWhitespace();
        if(Consume('+')) return ParseUnary();
        if(Consume('-')) return -ParseUnary();
        return ParsePrimary();
    }

    float ParsePrimary() {
        SkipWhitespace();
        if(Consume('(')) {
            const float value = ParseExpression();
            if(!Consume(')')) valid_ = false;
            return value;
        }

        if(position_ < source_.size() &&
           (std::isalpha(static_cast<unsigned char>(source_[position_])) ||
            source_[position_] == '_')) {
            const std::size_t start = position_;
            while(position_ < source_.size() &&
                  (std::isalnum(static_cast<unsigned char>(source_[position_])) ||
                   source_[position_] == '-' || source_[position_] == '_')) {
                ++position_;
            }
            std::string name(source_.substr(start,position_-start));
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if(!Consume('(')) {
                valid_ = false;
                return 0.0f;
            }
            std::vector<float> arguments;
            if(!Consume(')')) {
                do {
                    arguments.push_back(ParseExpression());
                } while(Consume(','));
                if(!Consume(')')) valid_ = false;
            }
            if(name == "calc") return arguments.empty() ? 0.0f : arguments.front();
            if(name == "min" && !arguments.empty()) {
                return *std::min_element(arguments.begin(),arguments.end());
            }
            if(name == "max" && !arguments.empty()) {
                return *std::max_element(arguments.begin(),arguments.end());
            }
            if(name == "clamp" && arguments.size() == 3) {
                return std::clamp(arguments[1],arguments[0],arguments[2]);
            }
            valid_ = false;
            return 0.0f;
        }

        const std::size_t number_start = position_;
        bool exponent = false;
        while(position_ < source_.size()) {
            const char character = source_[position_];
            if(std::isdigit(static_cast<unsigned char>(character)) || character == '.') {
                ++position_;
            }
            else if((character == 'e' || character == 'E') && !exponent) {
                exponent = true;
                ++position_;
                if(position_ < source_.size() &&
                   (source_[position_] == '+' || source_[position_] == '-')) ++position_;
            }
            else break;
        }
        if(number_start == position_) {
            valid_ = false;
            return 0.0f;
        }
        float number = 0.0f;
        try {
            number = std::stof(std::string(source_.substr(number_start,position_-number_start)));
        }
        catch(...) {
            valid_ = false;
            return 0.0f;
        }

        const std::size_t unit_start = position_;
        while(position_ < source_.size() &&
              (std::isalpha(static_cast<unsigned char>(source_[position_])) ||
               source_[position_] == '%')) ++position_;
        std::string unit(source_.substr(unit_start,position_-unit_start));
        std::transform(unit.begin(),unit.end(),unit.begin(),[](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if(unit.empty() || unit == "px") return number;
        if(unit == "%") return percent_reference_*number/100.0f;
        if(unit == "vw") {
            const float width = CSS_UI_VIEWPORT_WIDTH > 0.0f
                ? CSS_UI_VIEWPORT_WIDTH : static_cast<float>(GetScreenWidth());
            return width*number/100.0f;
        }
        if(unit == "vh") {
            const float height = CSS_UI_VIEWPORT_HEIGHT > 0.0f
                ? CSS_UI_VIEWPORT_HEIGHT : static_cast<float>(GetScreenHeight());
            return height*number/100.0f;
        }
        valid_ = false;
        return 0.0f;
    }

    std::string_view source_;
    std::size_t position_ = 0;
    float percent_reference_ = 0.0f;
    bool valid_ = true;
};

} // namespace

float ResolveCSSLength(
    const CSSLength& length,
    float percent_reference,
    float font_size,
    float root_font_size,
    const UIStyleVariableMap* variables
) {
    const float viewport_width = CSS_UI_VIEWPORT_WIDTH > 0.0f
        ? CSS_UI_VIEWPORT_WIDTH : static_cast<float>(GetScreenWidth());
    const float viewport_height = CSS_UI_VIEWPORT_HEIGHT > 0.0f
        ? CSS_UI_VIEWPORT_HEIGHT : static_cast<float>(GetScreenHeight());

    if(length.unit == LengthUnit::Expression) {
        UIStyleExpression expression = length.expression_ast;
        if(!expression && !length.expression.empty()) {
            // Compatibility path for programmatically-created CSSLength values.
            expression = ParseUIStyleExpression(length.expression,nullptr);
        }
        UIStyleEvaluationContext context;
        context.percent_reference = percent_reference;
        context.viewport_width = viewport_width;
        context.viewport_height = viewport_height;
        context.font_size = font_size;
        context.root_font_size = root_font_size;
        context.variables = variables;
        auto evaluated = EvaluateUIStyleExpression(expression,context,nullptr);
        return evaluated.has_value() ? evaluated->value : 0.0f;
    }
    switch(length.unit) {
        case LengthUnit::Auto: return 0.0f;
        case LengthUnit::Percent: return percent_reference*length.value/100.0f;
        case LengthUnit::ViewWidth: return viewport_width*length.value/100.0f;
        case LengthUnit::ViewHeight: return viewport_height*length.value/100.0f;
        case LengthUnit::Em: return font_size*length.value;
        case LengthUnit::RootEm: return root_font_size*length.value;
        default: return length.value;
    }
}

float ComputedMetricScale(
    const std::optional<CSSLength>& source,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale
) {
    // Relative lengths already use a containing block that has inherited its
    // parent's scale. Multiplying them by inherited_scale again would double
    // their size. Pixel values and C++ defaults still need the full factor.
    const bool relative =
        source.has_value() &&
        source->unit != LengthUnit::Pixels &&
        source->unit != LengthUnit::Auto;

    if(relative) {
        return relative_reference_includes_local_scale
            ? 1.0f
            : local_scale;
    }

    return inherited_scale * local_scale;
}

void ScaleComputedStyleMetrics(
    ComputedStyle& style,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale,
    bool inverse ) {
    inherited_scale = std::max(0.0001f,inherited_scale);
    local_scale = std::max(0.0001f,local_scale);

    auto multiply = [&](float& value, float factor) {
        value *= inverse ? 1.0f / factor : factor;
    };

    auto dimension = [&] (
        float& value,
        const std::optional<CSSLength>& source = std::nullopt
    ) {
        multiply(
            value,
            ComputedMetricScale(
                source,
                inherited_scale,
                local_scale,
                relative_reference_includes_local_scale
            )
        );
    };

    // Position belongs to the containing coordinate system. A widget's own
    // scale changes its size, not where its top-left anchor is placed.
    auto position = [&] (
        float& value,
        const std::optional<CSSLength>& source
    ) {
        const bool relative =
            source.has_value() &&
            source->unit != LengthUnit::Pixels;
        multiply(value,relative ? 1.0f : inherited_scale);
    };

    dimension(style.border_width,style.border_width_length);
    dimension(style.border_widths.left,style.border_width_lengths.left);
    dimension(style.border_widths.right,style.border_width_lengths.right);
    dimension(style.border_widths.top,style.border_width_lengths.top);
    dimension(style.border_widths.bottom,style.border_width_lengths.bottom);
    style.border_width = MaximumBorderWidth(style);
    dimension(style.width,style.width_length);
    dimension(style.height,style.height_length);
    dimension(style.min_width,style.min_width_length);
    dimension(style.max_width,style.max_width_length);
    dimension(style.min_height,style.min_height_length);
    dimension(style.max_height,style.max_height_length);
    dimension(style.font_size,style.font_size_length);
    dimension(style.gap,style.gap_length);
    dimension(style.row_gap,style.row_gap_length);
    dimension(style.column_gap,style.column_gap_length);
    dimension(style.scroll_speed);
    dimension(style.scrollbar_size,style.scrollbar_size_length);
    dimension(style.header_height,style.header_height_length);
    dimension(style.item_height,style.item_height_length);
    dimension(style.popup_width,style.popup_width_length);
    dimension(style.popup_max_height,style.popup_max_height_length);
    dimension(style.indent,style.indent_length);
    dimension(style.splitter_size,style.splitter_size_length);
    dimension(style.tab_height,style.tab_height_length);
    dimension(style.flex_basis,style.flex_basis_length);

    position(style.left,style.left_length);
    position(style.right,style.right_length);
    position(style.top,style.top_length);
    position(style.bottom,style.bottom_length);

    dimension(style.padding.left,style.padding_lengths.left);
    dimension(style.padding.right,style.padding_lengths.right);
    dimension(style.padding.top,style.padding_lengths.top);
    dimension(style.padding.bottom,style.padding_lengths.bottom);
    dimension(style.margin.left,style.margin_lengths.left);
    dimension(style.margin.right,style.margin_lengths.right);
    dimension(style.margin.top,style.margin_lengths.top);
    dimension(style.margin.bottom,style.margin_lengths.bottom);

    auto scale_track = [&](GridTrackSize& track) {
        if(track.unit == GridTrackUnit::Pixels) {
            multiply(track.value,inherited_scale * local_scale);
        }
    };

    for(GridTrackSize& track : style.grid_column_tracks) {
        scale_track(track);
    }
    for(GridTrackSize& track : style.grid_row_tracks) {
        scale_track(track);
    }
    scale_track(style.grid_auto_column);
    scale_track(style.grid_auto_row);
}

void ResetComputedStyleScale(ComputedStyle& style) {
    ScaleComputedStyleMetrics(
        style,
        style.applied_inherited_scale,
        style.applied_scale,
        style.applied_relative_reference_includes_local_scale,
        true
    );
    style.applied_scale = 1.0f;
    style.applied_inherited_scale = 1.0f;
    style.applied_relative_reference_includes_local_scale = false;
}

void ApplyComputedStyleScale(
    ComputedStyle& style,
    float inherited_scale,
    bool relative_reference_includes_local_scale ) {
    const float local_scale = std::clamp(style.scale,0.01f,100.0f);
    const float safe_inherited_scale = std::max(0.0001f,inherited_scale);

    ScaleComputedStyleMetrics(
        style,
        safe_inherited_scale,
        local_scale,
        relative_reference_includes_local_scale,
        false
    );
    style.applied_scale = local_scale;
    style.applied_inherited_scale = safe_inherited_scale;
    style.applied_relative_reference_includes_local_scale =
        relative_reference_includes_local_scale;
}

float EffectiveComputedStyleScale(const ComputedStyle& style) {
    return style.applied_inherited_scale * style.applied_scale;
}

void ResolveComputedLengths(
    ComputedStyle& style,
    float parent_width,
    float parent_height,
    float parent_font_size ,
    float inherited_scale ,
    bool relative_reference_includes_local_scale ) {
    ResetComputedStyleScale(style);

    // Resolve font-size first so em on other properties uses this element's
    // computed font size; em on font-size itself uses the parent font size.
    if(style.font_size_length) {
        style.font_size = ResolveCSSLength(
            *style.font_size_length,parent_font_size,parent_font_size,CSS_UI_ROOT_FONT_SIZE,
            style.custom_properties.get()
        );
    }
    const auto resolve_length = [&](const CSSLength& length,float reference) {
        return ResolveCSSLength(
            length,reference,style.font_size,CSS_UI_ROOT_FONT_SIZE,style.custom_properties.get()
        );
    };

    const float border_reference = std::min(parent_width,parent_height);
    const bool has_side_border_lengths =
        style.border_width_lengths.left.has_value() ||
        style.border_width_lengths.right.has_value() ||
        style.border_width_lengths.top.has_value() ||
        style.border_width_lengths.bottom.has_value();

    if(has_side_border_lengths) {
        if(style.border_width_lengths.left) {
            style.border_widths.left = resolve_length(
                *style.border_width_lengths.left,border_reference
            );
        }
        if(style.border_width_lengths.right) {
            style.border_widths.right = resolve_length(
                *style.border_width_lengths.right,border_reference
            );
        }
        if(style.border_width_lengths.top) {
            style.border_widths.top = resolve_length(
                *style.border_width_lengths.top,border_reference
            );
        }
        if(style.border_width_lengths.bottom) {
            style.border_widths.bottom = resolve_length(
                *style.border_width_lengths.bottom,border_reference
            );
        }
        style.border_width = MaximumBorderWidth(style);
    }
    else if(style.border_width_length) {
        style.border_width = resolve_length(
            *style.border_width_length,border_reference
        );
        style.border_widths = EdgeValues(style.border_width);
    }

    if(
        style.width_length &&
        style.width_length->unit != LengthUnit::Auto
    ) {
        style.width = resolve_length(
            *style.width_length,
            parent_width
        );
    }

    if(
        style.height_length &&
        style.height_length->unit != LengthUnit::Auto
    ) {
        style.height = resolve_length(
            *style.height_length,
            parent_height
        );
    }

    if(style.min_width_length) {
        style.min_width = resolve_length(*style.min_width_length,parent_width);
    }
    if(style.max_width_length) {
        style.max_width = resolve_length(*style.max_width_length,parent_width);
    }
    if(style.min_height_length) {
        style.min_height = resolve_length(*style.min_height_length,parent_height);
    }
    if(style.max_height_length) {
        style.max_height = resolve_length(*style.max_height_length,parent_height);
    }

    style.max_width = std::max(style.min_width,style.max_width);
    style.max_height = std::max(style.min_height,style.max_height);
    style.width = std::clamp(style.width,style.min_width,style.max_width);
    style.height = std::clamp(style.height,style.min_height,style.max_height);


    if(style.gap_length) {
        style.gap = resolve_length(
            *style.gap_length,
            parent_width
        );
        if(!style.row_gap_length) style.row_gap = style.gap;
        if(!style.column_gap_length) style.column_gap = style.gap;
    }
    if(style.row_gap_length) {
        style.row_gap = resolve_length(*style.row_gap_length,parent_height);
    }
    if(style.column_gap_length) {
        style.column_gap = resolve_length(*style.column_gap_length,parent_width);
    }
    if(style.scrollbar_size_length) {
        style.scrollbar_size = resolve_length(*style.scrollbar_size_length,parent_width);
    }
    if(style.header_height_length) {
        style.header_height = resolve_length(*style.header_height_length,parent_height);
    }
    if(style.item_height_length) {
        style.item_height = resolve_length(*style.item_height_length,parent_height);
    }
    if(style.popup_width_length) {
        style.popup_width = resolve_length(*style.popup_width_length,parent_width);
    }
    if(style.popup_max_height_length) {
        style.popup_max_height = resolve_length(*style.popup_max_height_length,parent_height);
    }
    if(style.indent_length) {
        style.indent = resolve_length(*style.indent_length,parent_width);
    }
    if(style.splitter_size_length) {
        style.splitter_size = resolve_length(*style.splitter_size_length,parent_width);
    }
    if(style.tab_height_length) {
        style.tab_height = resolve_length(*style.tab_height_length,parent_height);
    }

    if(style.left_length) {
        style.left = resolve_length(
            *style.left_length,
            parent_width
        );
    }

    if(style.right_length) {
        style.right = resolve_length(
            *style.right_length,
            parent_width
        );
    }

    if(style.top_length) {
        style.top = resolve_length(
            *style.top_length,
            parent_height
        );
    }

    if(style.bottom_length) {
        style.bottom = resolve_length(
            *style.bottom_length,
            parent_height
        );
    }

    if(style.flex_basis_length) {
        style.flex_basis =
            style.flex_basis_length->unit == LengthUnit::Auto
                ? -1.0f
                : resolve_length(
                    *style.flex_basis_length,
                    parent_width
                );
    }

    if(style.padding_lengths.left) {
        style.padding.left = resolve_length(
            *style.padding_lengths.left,
            parent_width
        );
    }

    if(style.padding_lengths.right) {
        style.padding.right = resolve_length(
            *style.padding_lengths.right,
            parent_width
        );
    }

    if(style.padding_lengths.top) {
        style.padding.top = resolve_length(
            *style.padding_lengths.top,
            parent_height
        );
    }

    if(style.padding_lengths.bottom) {
        style.padding.bottom = resolve_length(
            *style.padding_lengths.bottom,
            parent_height
        );
    }

    if(style.margin_lengths.left) {
        style.margin.left = resolve_length(
            *style.margin_lengths.left,
            parent_width
        );
    }

    if(style.margin_lengths.right) {
        style.margin.right = resolve_length(
            *style.margin_lengths.right,
            parent_width
        );
    }

    if(style.margin_lengths.top) {
        style.margin.top = resolve_length(
            *style.margin_lengths.top,
            parent_height
        );
    }

    if(style.margin_lengths.bottom) {
        style.margin.bottom = resolve_length(
            *style.margin_lengths.bottom,
            parent_height
        );
    }

    ApplyComputedStyleScale(
        style,
        inherited_scale,
        relative_reference_includes_local_scale
    );
}

void ApplyResolvedDeclaration(
    ComputedStyle& style,
    const Declaration& declaration
) {
    const PropertyValueType& value = declaration.value;

    auto assign_length = [&](std::optional<CSSLength>& target) {
        if(std::holds_alternative<CSSLength>(value)) {
            target = std::get<CSSLength>(value);
        }
        else if(std::holds_alternative<float>(value)) {
            target = CSSLength{
                std::get<float>(value),
                LengthUnit::Pixels
            };
        }
    };

    switch(declaration.property) {
        case PropertyType::BackgroundColor:
            if(std::holds_alternative<Color>(value)) {
                style.background_color = std::get<Color>(value);
            }
            break;

        case PropertyType::BorderColor:
            if(std::holds_alternative<Color>(value)) {
                style.border_color = std::get<Color>(value);
            }
            break;

        case PropertyType::BorderStyle:
            if(std::holds_alternative<std::string>(value)) {
                style.border_style = BorderStyleFromCSSValue(
                    std::get<std::string>(value)
                );
            }
            break;

        // The border shorthand is expanded by the parser and should not reach
        // computed-style resolution directly.
        case PropertyType::Border:
            break;

        case PropertyType::TextColor:
            if(std::holds_alternative<Color>(value)) {
                style.text_color = std::get<Color>(value);
            }
            break;

        case PropertyType::BorderRadius:
            if(std::holds_alternative<float>(value)) {
                style.border_radius = std::get<float>(value);
            }
            break;

        case PropertyType::BorderWidth:
            assign_length(style.border_width_length);
            assign_length(style.border_width_lengths.left);
            assign_length(style.border_width_lengths.right);
            assign_length(style.border_width_lengths.top);
            assign_length(style.border_width_lengths.bottom);
            break;

        case PropertyType::BorderTopWidth:
            assign_length(style.border_width_lengths.top);
            break;

        case PropertyType::BorderRightWidth:
            assign_length(style.border_width_lengths.right);
            break;

        case PropertyType::BorderBottomWidth:
            assign_length(style.border_width_lengths.bottom);
            break;

        case PropertyType::BorderLeftWidth:
            assign_length(style.border_width_lengths.left);
            break;

        case PropertyType::Width:
            assign_length(style.width_length);
            break;

        case PropertyType::Height:
            assign_length(style.height_length);
            break;

        case PropertyType::MinWidth:
            assign_length(style.min_width_length);
            break;

        case PropertyType::MaxWidth:
            assign_length(style.max_width_length);
            break;

        case PropertyType::MinHeight:
            assign_length(style.min_height_length);
            break;
        case PropertyType::Perspective:
            assign_length(style.perspective);
            break;
        case PropertyType::MaxHeight:
            assign_length(style.max_height_length);
            break;

        case PropertyType::FontSize:
            assign_length(style.font_size_length);
            break;

        case PropertyType::Gap:
            assign_length(style.gap_length);
            break;

        case PropertyType::RowGap:
            assign_length(style.row_gap_length);
            break;

        case PropertyType::ColumnGap:
            assign_length(style.column_gap_length);
            break;

        case PropertyType::ScrollbarSize:
            assign_length(style.scrollbar_size_length);
            break;

        case PropertyType::HeaderHeight:
            assign_length(style.header_height_length);
            break;

        case PropertyType::ItemHeight:
            assign_length(style.item_height_length);
            break;

        case PropertyType::PopupWidth:
            assign_length(style.popup_width_length);
            break;

        case PropertyType::PopupMaxHeight:
            assign_length(style.popup_max_height_length);
            break;

        case PropertyType::Indent:
            assign_length(style.indent_length);
            break;

        case PropertyType::SplitterSize:
            assign_length(style.splitter_size_length);
            break;

        case PropertyType::TabHeight:
            assign_length(style.tab_height_length);
            break;

        case PropertyType::Left:
            assign_length(style.left_length);
            break;

        case PropertyType::Right:
            assign_length(style.right_length);
            break;

        case PropertyType::Top:
            assign_length(style.top_length);
            break;

        case PropertyType::Bottom:
            assign_length(style.bottom_length);
            break;

        case PropertyType::FlexBasis:
            assign_length(style.flex_basis_length);
            break;

        case PropertyType::TransformOriginX:
            assign_length(style.transform_origin_x_length);
            break;

        case PropertyType::TransformOriginY:
            assign_length(style.transform_origin_y_length);
            break;

        case PropertyType::Padding: {
            std::optional<CSSLength> length;
            assign_length(length);
            if(length.has_value()) {
                style.padding_lengths.left = *length;
                style.padding_lengths.right = *length;
                style.padding_lengths.top = *length;
                style.padding_lengths.bottom = *length;
            }
            break;
        }

        case PropertyType::PaddingLeft:
            assign_length(style.padding_lengths.left);
            break;

        case PropertyType::PaddingRight:
            assign_length(style.padding_lengths.right);
            break;

        case PropertyType::PaddingTop:
            assign_length(style.padding_lengths.top);
            break;

        case PropertyType::PaddingBottom:
            assign_length(style.padding_lengths.bottom);
            break;

        case PropertyType::Margin: {
            std::optional<CSSLength> length;
            assign_length(length);
            if(length.has_value()) {
                style.margin_lengths.left = *length;
                style.margin_lengths.right = *length;
                style.margin_lengths.top = *length;
                style.margin_lengths.bottom = *length;
            }
            break;
        }

        case PropertyType::MarginLeft:
            assign_length(style.margin_lengths.left);
            break;

        case PropertyType::MarginRight:
            assign_length(style.margin_lengths.right);
            break;

        case PropertyType::MarginTop:
            assign_length(style.margin_lengths.top);
            break;

        case PropertyType::MarginBottom:
            assign_length(style.margin_lengths.bottom);
            break;

        case PropertyType::FontName:
            if(std::holds_alternative<std::string>(value)) {
                style.font = std::get<std::string>(value);
            }
            break;

        case PropertyType::DisplayType:
            if(std::holds_alternative<DisplayType>(value)) {
                style.display = std::get<DisplayType>(value);
            }
            break;

        case PropertyType::Opacity:
            if(std::holds_alternative<float>(value)) {
                style.opacity = std::clamp(
                    std::get<float>(value),
                    0.0f,
                    1.0f
                );
            }
            break;
        case PropertyType::Scale:
            if(std::holds_alternative<float>(value)) {
                style.scale = std::clamp(
                    std::get<float>(value),
                    0.01f,
                    100.0f
                );
            }
            break;
        case PropertyType::Position:
            if(std::holds_alternative<PositionType>(value)) {
                style.position = std::get<PositionType>(value);
            }
            break;

        case PropertyType::FlexDirection:
            if(std::holds_alternative<FlexDirection>(value)) {
                style.flex_direction = std::get<FlexDirection>(value);
            }
            break;

        case PropertyType::FlexWrap:
            if(std::holds_alternative<FlexWrap>(value)) {
                style.flex_wrap = std::get<FlexWrap>(value);
            }
            break;

        case PropertyType::JustifyContent:
            if(std::holds_alternative<JustifyContent>(value)) {
                style.justify_content = std::get<JustifyContent>(value);
            }
            break;

        case PropertyType::AlignItems:
            if(std::holds_alternative<AlignItems>(value)) {
                style.align_items = std::get<AlignItems>(value);
            }
            break;

        case PropertyType::JustifyItems:
            if(std::holds_alternative<AlignItems>(value)) {
                style.justify_items = std::get<AlignItems>(value);
            }
            break;

        case PropertyType::AlignSelf:
            if(std::holds_alternative<AlignSelf>(value)) {
                style.align_self = std::get<AlignSelf>(value);
            }
            break;

        case PropertyType::AlignContent:
            if(std::holds_alternative<AlignContent>(value)) {
                style.align_content = std::get<AlignContent>(value);
            }
            break;

        case PropertyType::JustifySelf:
            if(std::holds_alternative<JustifySelf>(value)) {
                style.justify_self = std::get<JustifySelf>(value);
            }
            break;

        case PropertyType::FlexGrow:
            if(std::holds_alternative<float>(value)) {
                style.flex_grow = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::FlexShrink:
            if(std::holds_alternative<float>(value)) {
                style.flex_shrink = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::TextAlign:
            if(std::holds_alternative<TextAlign>(value)) {
                style.text_align = std::get<TextAlign>(value);
            }
            break;

        case PropertyType::Overflow:
            if(std::holds_alternative<OverflowType>(value)) {
                style.overflow = std::get<OverflowType>(value);
            }
            break;

        case PropertyType::GridTemplateColumns:
            if(std::holds_alternative<GridTrackList>(value)) {
                style.grid_column_tracks = std::get<GridTrackList>(value);
                style.grid_template_columns = std::max(1,static_cast<int>(style.grid_column_tracks.size()));
            }
            break;

        case PropertyType::GridTemplateRows:
            if(std::holds_alternative<GridTrackList>(value)) {
                style.grid_row_tracks = std::get<GridTrackList>(value);
                style.grid_template_rows = static_cast<int>(style.grid_row_tracks.size());
            }
            break;

        case PropertyType::GridAutoColumns:
            if(std::holds_alternative<GridTrackList>(value) && !std::get<GridTrackList>(value).empty()) {
                style.grid_auto_column = std::get<GridTrackList>(value).front();
            }
            break;

        case PropertyType::GridAutoRows:
            if(std::holds_alternative<GridTrackList>(value) && !std::get<GridTrackList>(value).empty()) {
                style.grid_auto_row = std::get<GridTrackList>(value).front();
            }
            break;

        case PropertyType::GridAutoFlow:
            if(std::holds_alternative<GridAutoFlow>(value)) {
                style.grid_auto_flow = std::get<GridAutoFlow>(value);
            }
            break;

        case PropertyType::GridColumn:
            if(std::holds_alternative<int>(value)) {
                style.grid_column = std::max(
                    0,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridRow:
            if(std::holds_alternative<int>(value)) {
                style.grid_row = std::max(
                    0,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridColumnSpan:
            if(std::holds_alternative<int>(value)) {
                style.grid_column_span = std::max(
                    1,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::GridRowSpan:
            if(std::holds_alternative<int>(value)) {
                style.grid_row_span = std::max(
                    1,
                    std::get<int>(value)
                );
            }
            break;

        case PropertyType::InputStepButtons:
            if(std::holds_alternative<bool>(value)) {
                style.input_step_buttons = std::get<bool>(value);
            }
            break;

        case PropertyType::InputStep:
            if(std::holds_alternative<float>(value)) {
                style.input_step = std::get<float>(value);
            }
            break;

        case PropertyType::InputValueBox:
            if(std::holds_alternative<bool>(value)) {
                style.input_value_box = std::get<bool>(value);
            }
            break;

        case PropertyType::InputDragSpeed:
            if(std::holds_alternative<float>(value)) {
                style.input_drag_speed = std::max(
                    0.000001f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::InputSliderShowValue:
            if(std::holds_alternative<bool>(value)) {
                style.input_slider_show_value = std::get<bool>(value);
            }
            break;

        case PropertyType::InputSliderValueWidth:
            if(std::holds_alternative<float>(value)) {
                style.input_slider_value_width = std::max(
                    0.0f,
                    std::get<float>(value)
                );
            }
            break;

        case PropertyType::InputSliderClickToSet:
            if(std::holds_alternative<bool>(value)) {
                style.input_slider_click_to_set = std::get<bool>(value);
            }
            break;

        case PropertyType::RuntimeMovable:
            if(std::holds_alternative<bool>(value)) style.runtime_movable = std::get<bool>(value);
            break;
        case PropertyType::RuntimeResizable:
            if(std::holds_alternative<bool>(value)) style.runtime_resizable = std::get<bool>(value);
            break;
        case PropertyType::RuntimeConstrain:
            if(std::holds_alternative<bool>(value)) style.runtime_constrain = std::get<bool>(value);
            break;
        case PropertyType::RuntimeLockX:
            if(std::holds_alternative<bool>(value)) style.runtime_lock_x = std::get<bool>(value);
            break;
        case PropertyType::RuntimeLockY:
            if(std::holds_alternative<bool>(value)) style.runtime_lock_y = std::get<bool>(value);
            break;
        case PropertyType::RuntimeSnap:
            if(std::holds_alternative<float>(value)) style.runtime_snap = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ResizeToContent:
            if(std::holds_alternative<bool>(value)) style.resize_to_content = std::get<bool>(value);
            break;
        case PropertyType::ScrollSpeed:
            if(std::holds_alternative<float>(value)) style.scroll_speed = std::max(1.0f,std::get<float>(value));
            break;
        case PropertyType::TooltipDelay:
            if(std::holds_alternative<float>(value)) style.tooltip_delay = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::Resizable:
            if(std::holds_alternative<bool>(value)) style.resizable = std::get<bool>(value);
            break;
        case PropertyType::Closable:
            if(std::holds_alternative<bool>(value)) style.closable = std::get<bool>(value);
            break;
        case PropertyType::Collapsible:
            if(std::holds_alternative<bool>(value)) style.collapsible = std::get<bool>(value);
            break;
        case PropertyType::ObjectFit:
            if(std::holds_alternative<UIImageFit>(value)) style.image_fit = std::get<UIImageFit>(value);
            break;
        case PropertyType::ImageRendering:
            if(std::holds_alternative<UIImageSampling>(value)) style.image_sampling = std::get<UIImageSampling>(value);
            break;
        case PropertyType::ImageTint:
            if(std::holds_alternative<Color>(value)) style.image_tint = std::get<Color>(value);
            break;
        case PropertyType::ImageFlipX:
            if(std::holds_alternative<bool>(value)) style.image_flip_x = std::get<bool>(value);
            break;
        case PropertyType::ImageFlipY:
            if(std::holds_alternative<bool>(value)) style.image_flip_y = std::get<bool>(value);
            break;
        case PropertyType::ObjectPositionX:
            if(std::holds_alternative<float>(value)) style.image_position.x = std::clamp(std::get<float>(value),0.0f,1.0f);
            break;
        case PropertyType::ObjectPositionY:
            if(std::holds_alternative<float>(value)) style.image_position.y = std::clamp(std::get<float>(value),0.0f,1.0f);
            break;
        case PropertyType::ImageRotation:
            if(std::holds_alternative<float>(value)) style.image_rotation = std::get<float>(value);
            break;
        case PropertyType::ImagePivotX:
            if(std::holds_alternative<float>(value)) style.image_pivot.x = std::clamp(std::get<float>(value),0.0f,1.0f);
            break;
        case PropertyType::ImagePivotY:
            if(std::holds_alternative<float>(value)) style.image_pivot.y = std::clamp(std::get<float>(value),0.0f,1.0f);
            break;
        case PropertyType::ImageFrame:
            if(std::holds_alternative<int>(value)) style.image_frame = std::max(0,std::get<int>(value));
            break;
        case PropertyType::ImageFrameWidth:
            if(std::holds_alternative<float>(value)) style.image_frame_size.x = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageFrameHeight:
            if(std::holds_alternative<float>(value)) style.image_frame_size.y = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageFrameColumns:
            if(std::holds_alternative<int>(value)) style.image_frame_columns = std::max(0,std::get<int>(value));
            break;
        case PropertyType::ImageSliceLeft:
            if(std::holds_alternative<float>(value)) style.image_slice.left = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageSliceRight:
            if(std::holds_alternative<float>(value)) style.image_slice.right = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageSliceTop:
            if(std::holds_alternative<float>(value)) style.image_slice.top = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageSliceBottom:
            if(std::holds_alternative<float>(value)) style.image_slice.bottom = std::max(0.0f,std::get<float>(value));
            break;
        case PropertyType::ImageSliceScale:
            if(std::holds_alternative<float>(value)) style.image_slice_scale = std::max(0.001f,std::get<float>(value));
            break;
        case PropertyType::ImageSliceMode:
            if(std::holds_alternative<UIImageSliceMode>(value)) style.image_slice_mode = std::get<UIImageSliceMode>(value);
            break;
        case PropertyType::ImageTileScale:
            if(std::holds_alternative<float>(value)) style.image_tile_scale = std::max(0.001f,std::get<float>(value));
            break;
        case PropertyType::ImageTileOffsetX:
            if(std::holds_alternative<float>(value)) style.image_tile_offset.x = std::get<float>(value);
            break;
        case PropertyType::ImageTileOffsetY:
            if(std::holds_alternative<float>(value)) style.image_tile_offset.y = std::get<float>(value);
            break;
        case PropertyType::ImageBlendMode:
            if(std::holds_alternative<UIImageBlendMode>(value)) style.image_blend_mode = std::get<UIImageBlendMode>(value);
            break;
        case PropertyType::ImagePixelSnap:
            if(std::holds_alternative<bool>(value)) style.image_pixel_snap = std::get<bool>(value);
            break;
        case PropertyType::TabClosable:
            if(std::holds_alternative<bool>(value)) style.tab_closable = std::get<bool>(value);
            break;
        case PropertyType::Content:
            if(std::holds_alternative<UIGeneratedContentValue>(value)) {
                const auto& content=std::get<UIGeneratedContentValue>(value);
                style.generated_content = content.text;
                style.generated_content_defined = content.enabled;
            }
            break;
        case PropertyType::Filter:
            if(std::holds_alternative<UIFilterListRef>(value)) style.filters = std::get<UIFilterListRef>(value);
            break;
        case PropertyType::BackdropFilter:
            if(std::holds_alternative<UIFilterListRef>(value)) style.backdrop_filters = std::get<UIFilterListRef>(value);
            break;
        case PropertyType::Effect:
            if(std::holds_alternative<UIShaderEffectListRef>(value)) style.shader_effects = std::get<UIShaderEffectListRef>(value);
            break;
        case PropertyType::MixBlendMode:
            if(std::holds_alternative<UIMixBlendMode>(value)) style.mix_blend_mode = std::get<UIMixBlendMode>(value);
            break;
        case PropertyType::Transform:
            if(std::holds_alternative<UITransformListRef>(value)) style.transform_operations = std::get<UITransformListRef>(value);
            break;
        case PropertyType::Background:
        case PropertyType::Font:
        case PropertyType::Inset:
        case PropertyType::Flex:
        case PropertyType::PlaceItems:
            // Parser-only shorthands are expanded before style resolution.
            break;
    }
}


namespace {

template <typename Callback>
void ForEachCandidateStyleRule(
    const StyleSheet& style_sheet,
    SelectorType selector,
    Callback&& callback
) {
    const std::size_t selector_index=static_cast<std::size_t>(selector);
    if(selector_index<style_sheet.rules_by_selector.size()) {
        for(std::size_t rule_index:style_sheet.rules_by_selector[selector_index]) {
            callback(style_sheet.rules[rule_index]);
        }
    }
    else {
        for(const StyleRule& rule:style_sheet.rules) callback(rule);
    }
}

std::shared_ptr<const UIStyleVariableMap> ResolveCascadedCustomProperties(
    const StyleSheet& style_sheet,
    const std::vector<const StyleRule*>& matched,
    const std::shared_ptr<const UIStyleVariableMap>& inherited
) {
    const auto& base = inherited ? inherited : style_sheet.root_custom_properties;

    bool has_local_custom_properties=false;
    for(const StyleRule* rule:matched) {
        if(rule && !rule->custom_properties.empty()) {
            has_local_custom_properties=true;
            break;
        }
    }
    // This is the overwhelmingly common path. Share the inherited/root map
    // instead of cloning every custom property for every element.
    if(!has_local_custom_properties) return base;

    UIStyleVariableMap values;
    if(base) values=*base;
    std::unordered_map<std::string,int> specificity;
    for(const StyleRule* rule:matched) {
        if(!rule) continue;
        for(const auto& custom:rule->custom_properties) {
            auto found=specificity.find(custom.name);
            if(found==specificity.end() || rule->specificity>=found->second) {
                values[custom.name]=custom.raw_value;
                specificity[custom.name]=rule->specificity;
            }
        }
    }
    if(values.empty()) return {};
    return std::make_shared<const UIStyleVariableMap>(std::move(values));
}

std::shared_ptr<const UIStyleVariableMap> ResolveCascadedComponentStyleProperties(
    const std::vector<const StyleRule*>& matched,
    const std::shared_ptr<const UIStyleVariableMap>& base,
    const std::shared_ptr<const UIStyleVariableMap>& variables
) {
    bool has_values=base && !base->empty();
    for(const StyleRule* rule:matched) {
        if(rule && !rule->component_style_properties.empty()) { has_values=true; break; }
    }
    if(!has_values) return {};

    UIStyleVariableMap values;
    if(base) values=*base;
    std::unordered_map<std::string,int> specificity;
    static const UIStyleVariableMap empty_variables;
    const UIStyleVariableMap& vars=variables?*variables:empty_variables;

    for(const StyleRule* rule:matched) {
        if(!rule) continue;
        for(const auto& declaration:rule->component_style_properties) {
            const std::string name=LowerCSS(TrimCSS(declaration.name));
            auto found=specificity.find(name);
            if(found!=specificity.end() && rule->specificity<found->second) continue;
            std::string value=declaration.raw_value;
            if(auto resolved=ResolveCSSVariables(value,vars)) value=*resolved;
            values[name]=std::move(value);
            specificity[name]=rule->specificity;
        }
    }
    if(values.empty()) return {};
    return std::make_shared<const UIStyleVariableMap>(std::move(values));
}

UIShaderUniformListRef ResolveCascadedShaderUniforms(
    const std::vector<const StyleRule*>& matched
) {
    struct Winner { int specificity=-1; std::size_t order=0; UIShaderUniformBinding binding; };
    std::unordered_map<std::string,Winner> winners;
    std::size_t order=0;
    for(const StyleRule* rule:matched) {
        if(!rule) continue;
        for(const auto& uniform:rule->shader_uniforms) {
            auto& winner=winners[uniform.name];
            if(rule->specificity>=winner.specificity) {
                winner.specificity=rule->specificity;
                winner.order=order++;
                winner.binding=uniform;
            }
        }
    }
    if(winners.empty()) return {};
    std::vector<Winner> ordered;
    ordered.reserve(winners.size());
    for(auto& pair:winners) ordered.push_back(std::move(pair.second));
    std::sort(ordered.begin(),ordered.end(),[](const Winner& a,const Winner& b){ return a.order<b.order; });
    auto result=std::make_shared<UIShaderUniformList>();
    result->reserve(ordered.size());
    for(auto& winner:ordered) result->push_back(std::move(winner.binding));
    return result;
}

bool ApplyResolvedOrDeferredDeclaration(
    ComputedStyle& style,
    const Declaration& declaration
) {
    const auto* deferred=std::get_if<UIDeferredCSSValue>(&declaration.value);
    if(!deferred) {
        ApplyResolvedDeclaration(style,declaration);
        return true;
    }
    static const UIStyleVariableMap empty_variables;
    const UIStyleVariableMap& variables=style.custom_properties
        ? *style.custom_properties : empty_variables;
    auto resolved=ResolveCSSVariables(deferred->raw,variables);
    if(!resolved) {
        TraceLog(LOG_WARNING,"Unable to resolve RCSS value '%s'",deferred->raw.c_str());
        return false;
    }
    auto parsed=ParsePropertyValue(declaration.property,*resolved);
    if(!parsed || std::holds_alternative<UIDeferredCSSValue>(*parsed)) {
        TraceLog(LOG_WARNING,"Invalid resolved RCSS value '%s'",resolved->c_str());
        return false;
    }
    ApplyResolvedDeclaration(style,Declaration{declaration.property,std::move(*parsed)});
    return true;
}

} // namespace

ComputedStyle ResolveStyleUncached(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style ) {
    style.id=id;
    style.selector=selector_type;
    // font-family and color inherit from the parent; a matching rule may
    // override below. (Reset to the inherited value before the cascade.)
    if(!style.inherited_font.empty()) style.font=style.inherited_font;
    if(style.inherited_text_color && !style.text_color_explicit) {
        style.text_color=*style.inherited_text_color;
    }

    std::vector<const StyleRule*> matched;
    matched.reserve(12);
    ForEachCandidateStyleRule(style_sheet,selector_type,[&](const StyleRule& rule) {
        if(SelectorMatches(
            rule,selector_type,id,style.classes,selector_state,
            style.selector_ancestors,style.selector_ancestor_previous_siblings,
            style.selector_previous_siblings,
            style.sibling_index,style.sibling_count)) {
            matched.push_back(&rule);
        }
    });

    style.custom_properties=ResolveCascadedCustomProperties(
        style_sheet,matched,style.inherited_custom_properties);
    style.component_style_properties=ResolveCascadedComponentStyleProperties(
        matched,{},style.custom_properties);
    style.shader_uniforms=ResolveCascadedShaderUniforms(matched);

    std::array<int,property_names.size()> best_specificity{};
    best_specificity.fill(-1);
    int best_transition_specificity=-1;
    int best_animation_specificity=-1;
    for(const StyleRule* rule:matched) {
        const int specificity=rule->specificity;
        if(rule->transitions_declared && specificity>=best_transition_specificity) {
            best_transition_specificity=specificity;
            style.transitions=rule->transitions;
        }
        if(rule->animations_declared && specificity>=best_animation_specificity) {
            best_animation_specificity=specificity;
            style.animations=rule->animations;
        }
        for(const Declaration& declaration:rule->declarations) {
            const std::size_t property_index=static_cast<std::size_t>(declaration.property);
            if(property_index>=best_specificity.size() || specificity<best_specificity[property_index]) continue;
            best_specificity[property_index]=specificity;
            ApplyResolvedOrDeferredDeclaration(style,declaration);
        }
    }
    return style;
}

ComputedStyle ResolveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style ) {
    // Styles stored on widgets contain screen-space values from the previous
    // layout. Restore their logical values before matching CSS again.
    const float inherited_scale = style.applied_inherited_scale;
    const bool relative_reference_includes_local_scale =
        style.applied_relative_reference_includes_local_scale;
    const bool preserve_applied_scale =
        std::abs(style.applied_scale - 1.0f) > 0.00001f ||
        std::abs(inherited_scale - 1.0f) > 0.00001f;
    ResetComputedStyleScale(style);
    style.id = id;
    style.selector = selector_type;

    ResolvedStyleCacheKey key;
    key.style_sheet = &style_sheet;
    key.revision = style_sheet.revision;
    key.base_hash = HashComputedStyle(style);
    key.selector = selector_type;
    key.state = selector_state;

    auto found = CSS_UI_RESOLVED_STYLE_CACHE.find(key);
    if(found != CSS_UI_RESOLVED_STYLE_CACHE.end()) {
        ComputedStyle cached = found->second;
        if(preserve_applied_scale) {
            ApplyComputedStyleScale(
                cached,
                inherited_scale,
                relative_reference_includes_local_scale
            );
        }
        return cached;
    }

    ComputedStyle resolved = ResolveStyleUncached(
        style_sheet,
        selector_type,
        id,
        selector_state,
        std::move(style)
    );

    if(CSS_UI_RESOLVED_STYLE_CACHE.size() >= 4096) {
        CSS_UI_RESOLVED_STYLE_CACHE.clear();
    }

    CSS_UI_RESOLVED_STYLE_CACHE.emplace(
        std::move(key),
        resolved
    );

    if(preserve_applied_scale) {
        ApplyComputedStyleScale(
            resolved,
            inherited_scale,
            relative_reference_includes_local_scale
        );
    }

    return resolved;
}

ComputedStyle ResolveStyleWithPseudoStates(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& active_state_names,
    ComputedStyle style
) {
    const float inherited_scale = style.applied_inherited_scale;
    const bool relative_reference_includes_local_scale =
        style.applied_relative_reference_includes_local_scale;
    const bool preserve_applied_scale =
        std::abs(style.applied_scale-1.0f) > 0.00001f ||
        std::abs(inherited_scale-1.0f) > 0.00001f;
    ResetComputedStyleScale(style);
    style.id = id;
    style.selector = selector_type;

    auto normalized_name = [](const std::string& name) {
        if(name.empty() || name.front() == ':' ||
           std::isspace(static_cast<unsigned char>(name.front())) ||
           std::isspace(static_cast<unsigned char>(name.back()))) {
            return false;
        }
        return std::none_of(name.begin(),name.end(),[](unsigned char character) {
            return std::isupper(character) != 0;
        });
    };

    const bool use_input_directly =
        std::is_sorted(active_state_names.begin(),active_state_names.end()) &&
        std::all_of(
            active_state_names.begin(),active_state_names.end(),normalized_name
        );

    std::vector<std::string> normalized_states;
    if(!use_input_directly) {
        normalized_states.reserve(active_state_names.size());
        for(const std::string& state : active_state_names) {
            std::string normalized = NormalizeUIPseudoStateName(state);
            if(!normalized.empty()) normalized_states.push_back(std::move(normalized));
        }
        std::sort(normalized_states.begin(),normalized_states.end());
        normalized_states.erase(
            std::unique(normalized_states.begin(),normalized_states.end()),
            normalized_states.end()
        );
    }
    const std::vector<std::string>& active_states =
        use_input_directly ? active_state_names : normalized_states;

    std::size_t pseudo_hash = active_states.size();
    for(const std::string& state : active_states) {
        HashCombine(pseudo_hash,std::hash<std::string>{}(state));
    }

    ResolvedStyleCacheKey key;
    key.style_sheet = &style_sheet;
    key.revision = style_sheet.revision;
    key.base_hash = HashComputedStyle(style);
    key.pseudo_hash = pseudo_hash;
    key.selector = selector_type;
    key.state = SelectorState::None;
    key.pseudo_states = true;

    auto found = CSS_UI_RESOLVED_STYLE_CACHE.find(key);
    if(found != CSS_UI_RESOLVED_STYLE_CACHE.end()) {
        ComputedStyle cached = found->second;
        if(preserve_applied_scale) {
            ApplyComputedStyleScale(
                cached,inherited_scale,relative_reference_includes_local_scale
            );
        }
        return cached;
    }

    std::vector<const StyleRule*> matched;
    matched.reserve(12);
    ForEachCandidateStyleRule(style_sheet,selector_type,[&](const StyleRule& rule) {
        if(SelectorMatchesPseudoStates(
            rule,selector_type,id,style.classes,active_states,
            style.selector_ancestors,style.selector_ancestor_previous_siblings,
            style.selector_previous_siblings,
            style.sibling_index,style.sibling_count)) {
            matched.push_back(&rule);
        }
    });

    // The normal-state pass has already produced custom_properties. Treat that
    // result as the inherited base for pseudo-state overrides.
    style.custom_properties=ResolveCascadedCustomProperties(
        style_sheet,matched,style.custom_properties);
    style.component_style_properties=ResolveCascadedComponentStyleProperties(
        matched,style.component_style_properties,style.custom_properties);
    if(auto uniforms=ResolveCascadedShaderUniforms(matched)) style.shader_uniforms=std::move(uniforms);

    std::array<int,property_names.size()> best_specificity{};
    best_specificity.fill(-1);
    int best_transition_specificity=-1;
    int best_animation_specificity=-1;
    for(const StyleRule* rule:matched) {
        const int specificity=rule->specificity;
        if(rule->transitions_declared && specificity>=best_transition_specificity) {
            best_transition_specificity=specificity;
            style.transitions=rule->transitions;
        }
        if(rule->animations_declared && specificity>=best_animation_specificity) {
            best_animation_specificity=specificity;
            style.animations=rule->animations;
        }
        for(const Declaration& declaration:rule->declarations) {
            const std::size_t property_index=static_cast<std::size_t>(declaration.property);
            if(property_index>=best_specificity.size() || specificity<best_specificity[property_index]) continue;
            best_specificity[property_index]=specificity;
            ApplyResolvedOrDeferredDeclaration(style,declaration);
        }
    }

    if(CSS_UI_RESOLVED_STYLE_CACHE.size() >= 4096) {
        CSS_UI_RESOLVED_STYLE_CACHE.clear();
    }
    CSS_UI_RESOLVED_STYLE_CACHE.emplace(std::move(key),style);

    if(preserve_applied_scale) {
        ApplyComputedStyleScale(
            style,inherited_scale,relative_reference_includes_local_scale
        );
    }
    return style;
}


namespace {

bool GeneratedPseudoRuleMatches(
    const StyleRule& rule,
    UIGeneratedPseudoKind kind,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& active_states,
    const std::vector<StyleSelector>& ancestors,
    const std::vector<StyleSelectorSiblingChain>& ancestor_previous_siblings,
    StyleSelectorSiblingChain previous_siblings,
    std::size_t sibling_index,
    std::size_t sibling_count
) {
    if(rule.pseudo_element != kind) return false;
    if(!ComplexSelectorMatches(
        rule,selector_type,id,classes,ancestors,
        ancestor_previous_siblings,previous_siblings,
        sibling_index,sibling_count
    )) return false;

    if(!rule.pseudo_states.empty()) {
        return std::all_of(
            rule.pseudo_states.begin(),rule.pseudo_states.end(),
            [&](const std::string& state) {
                return std::binary_search(active_states.begin(),active_states.end(),state);
            }
        );
    }
    if(rule.state == SelectorState::None) return true;
    return std::binary_search(
        active_states.begin(),active_states.end(),LegacySelectorStateName(rule.state)
    );
}

ComputedStyle MakeGeneratedPseudoBase(const ComputedStyle& owner) {
    ComputedStyle style;
    // Pseudo-elements inherit typography and custom properties, not the owner's
    // physical box. The owner opacity is applied to keep generated paint in the
    // same visual group until full group-opacity compositing is required.
    style.background_color = BLANK;
    style.border_color = BLANK;
    style.text_color = owner.text_color;
    style.font = owner.font;
    style.font_size = owner.font_size;
    style.opacity = owner.opacity;
    style.width = 0.0f;
    style.height = 0.0f;
    style.width_length = CSSLength{0.0f,LengthUnit::Auto};
    style.height_length = CSSLength{0.0f,LengthUnit::Auto};
    style.display = DisplayType::Inline;
    style.position = PositionType::Static;
    style.selector = owner.selector;
    style.id = owner.id;
    style.classes = owner.classes;
    style.selector_ancestors = owner.selector_ancestors;
    style.selector_ancestor_previous_siblings = owner.selector_ancestor_previous_siblings;
    style.selector_previous_siblings = owner.selector_previous_siblings;
    style.sibling_index = owner.sibling_index;
    style.sibling_count = owner.sibling_count;
    style.inherited_custom_properties = owner.custom_properties;
    style.custom_properties = owner.custom_properties;
    return style;
}

bool IsAutoGeneratedLength(const std::optional<CSSLength>& value) {
    return !value.has_value() || value->unit == LengthUnit::Auto;
}

Rectangle GeneratedPseudoPaintRect(
    const Rectangle& owner,
    ComputedStyle& style,
    UIGeneratedPseudoKind kind
) {
    Font font = ResolveUIFont(style.font);
    Vector2 text_size = style.generated_content.empty()
        ? Vector2{}
        : MeasureTextCached(font,style.generated_content.c_str(),style.font_size,1.0f);

    const bool auto_width = IsAutoGeneratedLength(style.width_length);
    const bool auto_height = IsAutoGeneratedLength(style.height_length);
    float width = auto_width
        ? text_size.x + style.padding.left + style.padding.right
        : std::max(0.0f,style.width);
    float height = auto_height
        ? text_size.y + style.padding.top + style.padding.bottom
        : std::max(0.0f,style.height);

    float x = owner.x;
    float y = owner.y + (owner.height-height)*0.5f;

    if(style.position == PositionType::Absolute) {
        const bool has_left = style.left_length.has_value();
        const bool has_right = style.right_length.has_value();
        const bool has_top = style.top_length.has_value();
        const bool has_bottom = style.bottom_length.has_value();

        if(auto_width && has_left && has_right) {
            width = std::max(0.0f,owner.width-style.left-style.right);
        }
        if(auto_height && has_top && has_bottom) {
            height = std::max(0.0f,owner.height-style.top-style.bottom);
        }

        if(has_left) x = owner.x + style.left;
        else if(has_right) x = owner.x + owner.width - style.right - width;
        else x = owner.x;

        if(has_top) y = owner.y + style.top;
        else if(has_bottom) y = owner.y + owner.height - style.bottom - height;
        else y = owner.y;
    }
    else if(kind == UIGeneratedPseudoKind::After) {
        x = owner.x + owner.width - width;
    }

    return {x,y,width,height};
}

} // namespace

const ComputedStyle* ResolveGeneratedPseudoStyleRef(
    UIElementBase& owner,
    const ComputedStyle& owner_style,
    SelectorType selector,
    const StyleSheet& style_sheet,
    UIGeneratedPseudoKind kind
) {
    if(!StyleMayUseGeneratedPseudo(style_sheet,selector,owner_style,kind)) return nullptr;

    const auto& active_states = owner.pseudo_states.ActiveNamesView();
    const std::size_t pseudo_hash = owner.pseudo_states.ActiveHash();

    if(!owner.generated_pseudo_state) {
        owner.generated_pseudo_state = std::make_shared<UIGeneratedPseudoElementState>();
    }
    auto& slot = kind == UIGeneratedPseudoKind::Before
        ? owner.generated_pseudo_state->before
        : owner.generated_pseudo_state->after;
    if(!slot) slot = std::make_shared<UIGeneratedPseudoElement>();
    UIGeneratedPseudoElement& generated = *slot;

    const bool cache_hit =
        generated.owner_style_generation == owner.style_generation &&
        generated.sheet_revision == style_sheet.revision &&
        generated.pseudo_hash == pseudo_hash;

    if(!cache_hit) {
        ComputedStyle style = MakeGeneratedPseudoBase(owner_style);
        std::vector<const StyleRule*> matched;
        matched.reserve(6);
        ForEachCandidateStyleRule(style_sheet,selector,[&](const StyleRule& rule) {
            if(GeneratedPseudoRuleMatches(
                rule,kind,selector,owner_style.id,owner_style.classes,active_states,
                owner_style.selector_ancestors,
                owner_style.selector_ancestor_previous_siblings,
                owner_style.selector_previous_siblings,
                owner_style.sibling_index,owner_style.sibling_count
            )) matched.push_back(&rule);
        });

        if(matched.empty()) {
            generated.active = false;
            generated.owner_style_generation = owner.style_generation;
            generated.sheet_revision = style_sheet.revision;
            generated.pseudo_hash = pseudo_hash;
            return nullptr;
        }

        style.custom_properties = ResolveCascadedCustomProperties(
            style_sheet,matched,owner_style.custom_properties
        );
        style.shader_uniforms = ResolveCascadedShaderUniforms(matched);

        std::array<int,property_names.size()> best_specificity{};
        best_specificity.fill(-1);
        int best_transition_specificity=-1;
        int best_animation_specificity=-1;
        for(const StyleRule* rule : matched) {
            const int specificity=rule->specificity;
            if(rule->transitions_declared && specificity>=best_transition_specificity) {
                best_transition_specificity=specificity;
                style.transitions=rule->transitions;
            }
            if(rule->animations_declared && specificity>=best_animation_specificity) {
                best_animation_specificity=specificity;
                style.animations=rule->animations;
            }
            for(const Declaration& declaration : rule->declarations) {
                const std::size_t property_index=static_cast<std::size_t>(declaration.property);
                if(property_index>=best_specificity.size() || specificity<best_specificity[property_index]) continue;
                best_specificity[property_index]=specificity;
                ApplyResolvedOrDeferredDeclaration(style,declaration);
            }
        }

        ResolveComputedLengths(
            style,owner.transform.width,owner.transform.height,owner_style.font_size,
            owner_style.applied_inherited_scale,true
        );
        generated.style = std::move(style);
        generated.paint_rect = GeneratedPseudoPaintRect(owner.transform,generated.style,kind);
        generated.active = generated.style.generated_content_defined &&
            generated.style.display != DisplayType::None;
        generated.owner_style_generation = owner.style_generation;
        generated.sheet_revision = style_sheet.revision;
        generated.pseudo_hash = pseudo_hash;
        generated.element.style_generation++;
        if(generated.element.style_generation==0) generated.element.style_generation=1;
    }

    if(!generated.active) return nullptr;
    generated.element.transform=generated.paint_rect;
    generated.element.layout_transform=generated.paint_rect;
    generated.element.inherited_disabled=owner.inherited_disabled;
    for(const char* state_name : {"hover","active","focus","disabled"}) {
        generated.element.pseudo_states.SetKnown(
            state_name,owner.pseudo_states.Active(state_name)
        );
    }
    const std::string channel = kind == UIGeneratedPseudoKind::Before
        ? "__rui_before" : "__rui_after";
    return &ResolveAnimatedStyleRef(
        generated.element,style_sheet,generated.style,channel,
        generated.owner_style_generation ^ (generated.sheet_revision<<1) ^ generated.pseudo_hash
    );
}

void DrawGeneratedPseudoElement(
    UIElementBase& owner,
    const ComputedStyle& owner_style,
    SelectorType selector,
    const StyleSheet& style_sheet,
    UIGeneratedPseudoKind kind
) {
    const ComputedStyle* resolved = ResolveGeneratedPseudoStyleRef(
        owner,owner_style,selector,style_sheet,kind
    );
    if(!resolved || !owner.generated_pseudo_state) return;
    const auto& slot = kind == UIGeneratedPseudoKind::Before
        ? owner.generated_pseudo_state->before
        : owner.generated_pseudo_state->after;
    if(!slot) return;

    ComputedStyle style = *resolved;
    ResolveComputedLengths(
        style,owner.transform.width,owner.transform.height,owner_style.font_size,
        owner_style.applied_inherited_scale,true
    );
    Rectangle rect = GeneratedPseudoPaintRect(owner.transform,style,kind);
    slot->paint_rect = rect;
    slot->element.transform=rect;
    slot->element.layout_transform=rect;

    auto paint_raw=[&]() {
        const Color background=ApplyStyleOpacity(style.background_color,style.opacity);
        if(background.a>0 && rect.width>0.0f && rect.height>0.0f) {
            DrawRectangleRounded(
                rect,ResolveUIBorderRoundness(rect,style.border_radius),8,background
            );
        }
        DrawStyleBorder(
            rect,style,ApplyStyleOpacity(style.border_color,style.opacity),8
        );

        if(!style.generated_content.empty() && rect.width>0.0f && rect.height>0.0f) {
            Font font=ResolveUIFont(style.font);
            Rectangle content={
                rect.x+style.padding.left,rect.y+style.padding.top,
                std::max(0.0f,rect.width-style.padding.left-style.padding.right),
                std::max(0.0f,rect.height-style.padding.top-style.padding.bottom)
            };
            Vector2 measured=MeasureTextCached(
                font,style.generated_content.c_str(),style.font_size,1.0f
            );
            Vector2 position={
                GetTextX(content,measured.x,style.text_align),
                content.y+(content.height-measured.y)*0.5f
            };
            DrawUIText(
                font,style.generated_content.c_str(),position,style.font_size,1.0f,
                ApplyStyleOpacity(style.text_color,style.opacity)
            );
        }
    };

    if(!DrawUICompositedLayer(slot->element,style,rect,paint_raw)) paint_raw();
}
// -----------------------------------------------------------------------------
// EvaluateTransformMatrix – Builds a 4×4 matrix from CSS transform operations.
// -----------------------------------------------------------------------------
void EvaluateTransformMatrix(
    ComputedStyle& style,
    const Rectangle& bounds,
    float parent_width,
    float parent_height,
    float font_size,
    float root_font_size,
    float viewport_width,
    float viewport_height
) {
    Matrix matrix = MatrixIdentity();

    // --- Apply transform operations (if any) ---
    if (style.transform_operations) {
        // Origin for scale/rotate/skew (resolved against element's own box)
        const CSSLength default_origin{50.0f, LengthUnit::Percent};
        const CSSLength& origin_x_length = style.transform_origin_x_length.has_value()
            ? *style.transform_origin_x_length : default_origin;
        const CSSLength& origin_y_length = style.transform_origin_y_length.has_value()
            ? *style.transform_origin_y_length : default_origin;
        float origin_x = ResolveCSSLength(origin_x_length, bounds.width, font_size, root_font_size, style.custom_properties.get());
        float origin_y = ResolveCSSLength(origin_y_length, bounds.height, font_size, root_font_size, style.custom_properties.get());

        // Helper to evaluate an expression to a float
        auto eval = [&](const UIStyleExpression& expr, float percent_ref) -> float {
            UIStyleEvaluationContext ctx;
            ctx.percent_reference = percent_ref;
            ctx.viewport_width = viewport_width;
            ctx.viewport_height = viewport_height;
            ctx.font_size = font_size;
            ctx.root_font_size = root_font_size;
            ctx.variables = style.custom_properties.get();
            auto result = EvaluateUIStyleExpression(expr, ctx, nullptr);
            return result ? result->value : 0.0f;
        };

        for (const auto& op : style.transform_operations->operations) {
            float value = 0.0f;
            Matrix opMatrix = MatrixIdentity();

            float percentRef = 1.0f;
            switch (op.type) {
                case UITransformOperationType::TranslateX:
                case UITransformOperationType::ScaleX:
                    percentRef = bounds.width;
                    break;
                case UITransformOperationType::TranslateY:
                case UITransformOperationType::ScaleY:
                    percentRef = bounds.height;
                    break;
                case UITransformOperationType::TranslateZ:
                case UITransformOperationType::ScaleZ:
                    percentRef = std::min(bounds.width, bounds.height);
                    break;
                default:
                    percentRef = 1.0f;
                    break;
            }

            value = eval(op.value, percentRef);

            switch (op.type) {
                case UITransformOperationType::TranslateX:
                    opMatrix = MatrixTranslate(value, 0.0f, 0.0f);
                    break;
                case UITransformOperationType::TranslateY:
                    opMatrix = MatrixTranslate(0.0f, value, 0.0f);
                    break;
                case UITransformOperationType::TranslateZ:
                    opMatrix = MatrixTranslate(0.0f, 0.0f, value);
                    break;
                case UITransformOperationType::ScaleX:
                    opMatrix = MatrixScale(value, 1.0f, 1.0f);
                    break;
                case UITransformOperationType::ScaleY:
                    opMatrix = MatrixScale(1.0f, value, 1.0f);
                    break;
                case UITransformOperationType::ScaleZ:
                    opMatrix = MatrixScale(1.0f, 1.0f, value);
                    break;
                case UITransformOperationType::Rotate:
                    opMatrix = MatrixRotateZ(value * DEG2RAD);
                    break;
                case UITransformOperationType::RotateX:
                    opMatrix = MatrixRotateX(value * DEG2RAD);
                    break;
                case UITransformOperationType::RotateY:
                    opMatrix = MatrixRotateY(value * DEG2RAD);
                    break;
                case UITransformOperationType::RotateZ:
                    opMatrix = MatrixRotateZ(value * DEG2RAD);
                    break;
                case UITransformOperationType::Rotate3D: {
                    // Parse axis and angle from source
                    std::vector<float> components;
                    std::string raw = op.source;
                    std::replace(raw.begin(), raw.end(), ',', ' ');
                    std::istringstream iss(raw);
                    float num;
                    while (iss >> num) components.push_back(num);
                    if (components.size() == 4) {
                        Vector3 axis = {components[0], components[1], components[2]};
                        float angle = components[3] * DEG2RAD;
                        opMatrix = MatrixRotate(axis, angle);
                    }
                    break;
                }
                case UITransformOperationType::Matrix3D: {
                    std::vector<float> m;
                    std::string raw = op.source;
                    std::replace(raw.begin(), raw.end(), ',', ' ');
                    std::istringstream iss(raw);
                    float num;
                    while (iss >> num) m.push_back(num);
                    if (m.size() == 16) {
                        opMatrix = {
                            m[0],  m[4],  m[8],  m[12],
                            m[1],  m[5],  m[9],  m[13],
                            m[2],  m[6],  m[10], m[14],
                            m[3],  m[7],  m[11], m[15]
                        };
                    }
                    break;
                }
                case UITransformOperationType::SkewX: {
                    float t = tanf(value * DEG2RAD);
                    opMatrix = MatrixIdentity();
                    opMatrix.m4 = t;  // y contribution to x
                    break;
                }
                case UITransformOperationType::SkewY: {
                    float t = tanf(value * DEG2RAD);
                    opMatrix = MatrixIdentity();
                    opMatrix.m1 = t;  // x contribution to y
                    break;
                }
                default:
                    break;
            }

            // Multiply opMatrix on the left: opMatrix is applied after existing transforms
            matrix = MatrixMultiply(opMatrix, matrix);
        }
    }

    // Perspective is no longer folded in here as a hand-rolled W-perturbation
    // matrix — that approach (composing a raw projective term into the local
    // transform, then relying on it surviving whatever ambient projection was
    // active downstream) was fragile and never rendered correctly. CSS
    // `perspective` is now implemented properly in ui_effects.cpp using
    // raylib's own Camera3D/BeginMode3D, which handles the actual projection
    // and view matrix construction — proven, tested code instead of hand-
    // derived matrix math. style.perspective is read directly from there.

    // --- Pivot around transform-origin ---
    // Origin defaults to 50%/50% (center of the element)
    const CSSLength default_origin{50.0f, LengthUnit::Percent};
    const CSSLength& origin_x_length = style.transform_origin_x_length.has_value()
        ? *style.transform_origin_x_length : default_origin;
    const CSSLength& origin_y_length = style.transform_origin_y_length.has_value()
        ? *style.transform_origin_y_length : default_origin;
    float origin_x = ResolveCSSLength(origin_x_length, bounds.width, font_size, root_font_size, style.custom_properties.get());
    float origin_y = ResolveCSSLength(origin_y_length, bounds.height, font_size, root_font_size, style.custom_properties.get());

    // Translate to origin, apply the composed transform, translate back
    matrix = MatrixMultiply(
        MatrixTranslate(origin_x, origin_y, 0.0f),
        MatrixMultiply(matrix, MatrixTranslate(-origin_x, -origin_y, 0.0f))
    );

    // Store the final transform matrix
    style.transform_matrix = matrix;
}