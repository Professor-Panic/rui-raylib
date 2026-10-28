#include "css_emulator.h"
#include "css_parser.h"
#include "tweeny-4.0.0.h"

#include <map>
#include <set>

namespace {

using ProgressTween = tweeny::tween<float>;

ProgressTween MakeProgressTween(UIEasing easing, float duration_seconds) {
    const uint32_t milliseconds = static_cast<uint32_t>(
        std::max(1.0f,std::round(duration_seconds*1000.0f))
    );
    auto builder = tweeny::from(0.0f).to(1.0f).during(milliseconds);
    switch(easing) {
        case UIEasing::Linear: builder.via(tweeny::easing::linear); break;
        case UIEasing::EaseIn:
        case UIEasing::QuadraticIn: builder.via(tweeny::easing::quadraticIn); break;
        case UIEasing::EaseOut:
        case UIEasing::QuadraticOut: builder.via(tweeny::easing::quadraticOut); break;
        case UIEasing::Ease:
        case UIEasing::EaseInOut:
        case UIEasing::QuadraticInOut: builder.via(tweeny::easing::quadraticInOut); break;
        case UIEasing::CubicIn: builder.via(tweeny::easing::cubicIn); break;
        case UIEasing::CubicOut: builder.via(tweeny::easing::cubicOut); break;
        case UIEasing::CubicInOut: builder.via(tweeny::easing::cubicInOut); break;
        case UIEasing::BackIn: builder.via(tweeny::easing::backIn); break;
        case UIEasing::BackOut: builder.via(tweeny::easing::backOut); break;
        case UIEasing::BackInOut: builder.via(tweeny::easing::backInOut); break;
        case UIEasing::BounceIn: builder.via(tweeny::easing::bounceIn); break;
        case UIEasing::BounceOut: builder.via(tweeny::easing::bounceOut); break;
        case UIEasing::BounceInOut: builder.via(tweeny::easing::bounceInOut); break;
        case UIEasing::ElasticIn: builder.via(tweeny::easing::elasticIn); break;
        case UIEasing::ElasticOut: builder.via(tweeny::easing::elasticOut); break;
        case UIEasing::ElasticInOut: builder.via(tweeny::easing::elasticInOut); break;
    }
    return std::move(builder).build();
}

bool EqualColor(Color a, Color b) {
    return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}

bool EqualAnimationSpec(const UIAnimationSpec& a, const UIAnimationSpec& b) {
    return a.name==b.name && a.duration==b.duration && a.delay==b.delay &&
        a.easing==b.easing && a.iterations==b.iterations &&
        a.infinite==b.infinite && a.direction==b.direction &&
        a.fill_mode==b.fill_mode && a.paused==b.paused;
}

std::optional<PropertyValueType> StylePropertyValue(
    const ComputedStyle& style,
    PropertyType property
) {
    switch(property) {
        case PropertyType::BackgroundColor: return style.background_color;
        case PropertyType::BorderColor: return style.border_color;
        case PropertyType::TextColor: return style.text_color;
        case PropertyType::ImageTint: return style.image_tint;
        case PropertyType::BorderWidth: return style.border_width;
        case PropertyType::BorderTopWidth: return style.border_widths.top;
        case PropertyType::BorderRightWidth: return style.border_widths.right;
        case PropertyType::BorderBottomWidth: return style.border_widths.bottom;
        case PropertyType::BorderLeftWidth: return style.border_widths.left;
        case PropertyType::BorderRadius: return style.border_radius;
        case PropertyType::Width: return style.width;
        case PropertyType::Height: return style.height;
        case PropertyType::MinWidth: return style.min_width;
        case PropertyType::MaxWidth: return style.max_width;
        case PropertyType::MinHeight: return style.min_height;
        case PropertyType::MaxHeight: return style.max_height;
        case PropertyType::FontSize: return style.font_size;
        case PropertyType::Gap: return style.gap;
        case PropertyType::RowGap: return style.row_gap;
        case PropertyType::ColumnGap: return style.column_gap;
        case PropertyType::Opacity: return style.opacity;
        case PropertyType::Left: return style.left;
        case PropertyType::Right: return style.right;
        case PropertyType::Top: return style.top;
        case PropertyType::Bottom: return style.bottom;
        case PropertyType::FlexGrow: return style.flex_grow;
        case PropertyType::FlexShrink: return style.flex_shrink;
        case PropertyType::FlexBasis: return style.flex_basis;
        case PropertyType::ScrollSpeed: return style.scroll_speed;
        case PropertyType::ScrollbarSize: return style.scrollbar_size;
        case PropertyType::Scale: return style.scale;
        case PropertyType::HeaderHeight: return style.header_height;
        case PropertyType::ItemHeight: return style.item_height;
        case PropertyType::PopupWidth: return style.popup_width;
        case PropertyType::PopupMaxHeight: return style.popup_max_height;
        case PropertyType::TooltipDelay: return style.tooltip_delay;
        case PropertyType::Indent: return style.indent;
        case PropertyType::SplitterSize: return style.splitter_size;
        case PropertyType::TabHeight: return style.tab_height;
        case PropertyType::Padding:
            return Vector4{style.padding.left,style.padding.right,style.padding.top,style.padding.bottom};
        case PropertyType::PaddingLeft: return style.padding.left;
        case PropertyType::PaddingRight: return style.padding.right;
        case PropertyType::PaddingTop: return style.padding.top;
        case PropertyType::PaddingBottom: return style.padding.bottom;
        case PropertyType::Margin:
            return Vector4{style.margin.left,style.margin.right,style.margin.top,style.margin.bottom};
        case PropertyType::MarginLeft: return style.margin.left;
        case PropertyType::MarginRight: return style.margin.right;
        case PropertyType::MarginTop: return style.margin.top;
        case PropertyType::MarginBottom: return style.margin.bottom;
        default: return std::nullopt;
    }
}

bool PropertyValuesEqual(const PropertyValueType& a, const PropertyValueType& b) {
    if(a.index()!=b.index()) return false;
    if(const auto* value=std::get_if<float>(&a)) return std::abs(*value-std::get<float>(b))<0.0001f;
    if(const auto* value=std::get_if<int>(&a)) return *value==std::get<int>(b);
    if(const auto* value=std::get_if<bool>(&a)) return *value==std::get<bool>(b);
    if(const auto* value=std::get_if<std::string>(&a)) return *value==std::get<std::string>(b);
    if(const auto* value=std::get_if<Color>(&a)) return EqualColor(*value,std::get<Color>(b));
    if(const auto* value=std::get_if<Vector2>(&a)) {
        Vector2 other=std::get<Vector2>(b); return value->x==other.x&&value->y==other.y;
    }
    if(const auto* value=std::get_if<Vector3>(&a)) {
        Vector3 other=std::get<Vector3>(b); return value->x==other.x&&value->y==other.y&&value->z==other.z;
    }
    if(const auto* value=std::get_if<Vector4>(&a)) {
        Vector4 other=std::get<Vector4>(b); return value->x==other.x&&value->y==other.y&&value->z==other.z&&value->w==other.w;
    }
    if(const auto* value=std::get_if<CSSLength>(&a)) {
        const CSSLength other=std::get<CSSLength>(b);
        return std::abs(value->value-other.value)<0.0001f&&value->unit==other.unit;
    }
    if(const auto* value=std::get_if<GridTrackList>(&a)) {
        const GridTrackList& other=std::get<GridTrackList>(b);
        if(value->size()!=other.size()) return false;
        for(std::size_t index=0;index<value->size();++index) {
            if(std::abs((*value)[index].value-other[index].value)>=0.0001f||
               (*value)[index].unit!=other[index].unit) return false;
        }
        return true;
    }
#define UI_COMPARE_ENUM(Type) if(const auto* value=std::get_if<Type>(&a)) return *value==std::get<Type>(b)
    UI_COMPARE_ENUM(DisplayType);
    UI_COMPARE_ENUM(PositionType);
    UI_COMPARE_ENUM(FlexDirection);
    UI_COMPARE_ENUM(JustifyContent);
    UI_COMPARE_ENUM(AlignItems);
    UI_COMPARE_ENUM(AlignSelf);
    UI_COMPARE_ENUM(TextAlign);
    UI_COMPARE_ENUM(OverflowType);
    UI_COMPARE_ENUM(GridAutoFlow);
    UI_COMPARE_ENUM(UIImageFit);
    UI_COMPARE_ENUM(UIImageSampling);
    UI_COMPARE_ENUM(UIImageSliceMode);
    UI_COMPARE_ENUM(UIImageBlendMode);
#undef UI_COMPARE_ENUM
    return false;
}

PropertyValueType InterpolatePropertyValue(
    const PropertyValueType& from,
    const PropertyValueType& to,
    float amount
) {
    amount=std::clamp(amount,0.0f,1.0f);
    if(from.index()!=to.index()) return amount<1.0f?from:to;
    if(const auto* value=std::get_if<float>(&from)) return Lerp(*value,std::get<float>(to),amount);
    if(const auto* value=std::get_if<int>(&from)) {
        return static_cast<int>(std::round(Lerp(static_cast<float>(*value),static_cast<float>(std::get<int>(to)),amount)));
    }
    if(const auto* value=std::get_if<Color>(&from)) return MixColors(*value,std::get<Color>(to),amount);
    if(const auto* value=std::get_if<Vector2>(&from)) {
        Vector2 other=std::get<Vector2>(to); return Vector2{Lerp(value->x,other.x,amount),Lerp(value->y,other.y,amount)};
    }
    if(const auto* value=std::get_if<Vector3>(&from)) {
        Vector3 other=std::get<Vector3>(to); return Vector3{Lerp(value->x,other.x,amount),Lerp(value->y,other.y,amount),Lerp(value->z,other.z,amount)};
    }
    if(const auto* value=std::get_if<Vector4>(&from)) {
        Vector4 other=std::get<Vector4>(to); return Vector4{Lerp(value->x,other.x,amount),Lerp(value->y,other.y,amount),Lerp(value->z,other.z,amount),Lerp(value->w,other.w,amount)};
    }
    return amount<1.0f?from:to;
}

void ApplyAnimatedProperty(
    ComputedStyle& style,
    PropertyType property,
    const PropertyValueType& value
) {
    if(property==PropertyType::Padding && std::holds_alternative<Vector4>(value)) {
        const Vector4 v=std::get<Vector4>(value);
        style.padding.left=v.x;
        style.padding.right=v.y;
        style.padding.top=v.z;
        style.padding.bottom=v.w;
        return;
    }
    if(property==PropertyType::Margin && std::holds_alternative<Vector4>(value)) {
        const Vector4 v=std::get<Vector4>(value);
        style.margin.left=v.x;
        style.margin.right=v.y;
        style.margin.top=v.z;
        style.margin.bottom=v.w;
        return;
    }
    ApplyResolvedDeclaration(style,Declaration{property,value});
}

const std::vector<PropertyType>& AnimatableProperties() {
    // These properties affect rendering without requiring the retained layout
    // tree to be rebuilt. Layout-affecting animation can be added later behind
    // the same manager without changing RCSS syntax.
    static const std::vector<PropertyType> properties = {
        PropertyType::BackgroundColor,
        PropertyType::BorderColor,
        PropertyType::TextColor,
        PropertyType::ImageTint,
        PropertyType::BorderRadius,
        PropertyType::Opacity
    };
    return properties;
}

bool IsAnimatableProperty(PropertyType property) {
    const auto& properties=AnimatableProperties();
    return std::find(properties.begin(),properties.end(),property)!=properties.end();
}

std::size_t HashVisualAnimationTarget(const ComputedStyle& style) {
    std::size_t seed = HashColor(style.background_color);
    HashCombine(seed,HashColor(style.border_color));
    HashCombine(seed,HashColor(style.text_color));
    HashCombine(seed,HashColor(style.image_tint));
    HashCombine(seed,HashFloat(style.border_radius));
    HashCombine(seed,HashFloat(style.opacity));
    if(style.custom_properties) {
        std::size_t custom_hash=0;
        for(const auto& entry:*style.custom_properties) {
            std::size_t item=std::hash<std::string>{}(entry.first);
            HashCombine(item,std::hash<std::string>{}(entry.second));
            custom_hash ^= item + 0x9e3779b9 + (item<<6) + (item>>2);
        }
        HashCombine(seed,custom_hash);
    }
    if(style.component_style_properties) {
        std::size_t component_hash=0;
        for(const auto& entry:*style.component_style_properties) {
            std::size_t item=std::hash<std::string>{}(entry.first);
            HashCombine(item,std::hash<std::string>{}(entry.second));
            component_hash ^= item + 0x9e3779b9 + (item<<6) + (item>>2);
        }
        HashCombine(seed,component_hash);
    }
    if(style.shader_uniforms) {
        for(const auto& binding:*style.shader_uniforms) {
            HashCombine(seed,std::hash<std::string>{}(binding.name));
            HashCombine(seed,std::hash<std::string>{}(binding.raw_value));
        }
    }

    HashCombine(seed,style.transitions.size());
    for(const UITransitionSpec& transition : style.transitions) {
        HashCombine(seed,std::hash<bool>{}(transition.all));
        HashCombine(seed,HashFloat(transition.duration));
        HashCombine(seed,HashFloat(transition.delay));
        HashCombine(seed,static_cast<std::size_t>(transition.easing));
        HashCombine(seed,transition.properties.size());
        for(PropertyType property : transition.properties) {
            HashCombine(seed,static_cast<std::size_t>(property));
        }
        HashCombine(seed,transition.dynamic_properties.size());
        for(const std::string& property : transition.dynamic_properties) {
            HashCombine(seed,std::hash<std::string>{}(property));
        }
    }

    HashCombine(seed,style.animations.size());
    for(const UIAnimationSpec& animation : style.animations) {
        HashCombine(seed,std::hash<std::string>{}(animation.name));
        HashCombine(seed,HashFloat(animation.duration));
        HashCombine(seed,HashFloat(animation.delay));
        HashCombine(seed,static_cast<std::size_t>(animation.easing));
        HashCombine(seed,HashFloat(animation.iterations));
        HashCombine(seed,std::hash<bool>{}(animation.infinite));
        HashCombine(seed,static_cast<std::size_t>(animation.direction));
        HashCombine(seed,static_cast<std::size_t>(animation.fill_mode));
        HashCombine(seed,std::hash<bool>{}(animation.paused));
    }

    return seed;
}

const UITransitionSpec* TransitionForProperty(
    const ComputedStyle& style,
    PropertyType property
) {
    const UITransitionSpec* all_match=nullptr;
    for(const UITransitionSpec& transition:style.transitions) {
        if(transition.duration<=0.0f) continue;
        if(transition.all) all_match=&transition;
        else if(std::find(transition.properties.begin(),transition.properties.end(),property)!=transition.properties.end()) {
            return &transition;
        }
    }
    return all_match;
}


struct DynamicScalar {
    float value=0.0f;
    std::string suffix;
};

using DynamicAnimationValue = std::variant<
    DynamicScalar,
    Color,
    std::vector<float>,
    std::string
>;

std::optional<DynamicScalar> ParseDynamicScalar(std::string raw) {
    raw=TrimCSS(raw);
    if(raw.empty()) return std::nullopt;
    std::size_t parsed=0;
    float value=0.0f;
    try { value=std::stof(raw,&parsed); }
    catch(...) { return std::nullopt; }
    if(parsed==0) return std::nullopt;
    DynamicScalar result;
    result.value=value;
    result.suffix=TrimCSS(raw.substr(parsed));
    return result;
}

DynamicAnimationValue ParseDynamicAnimationValue(std::string raw) {
    raw=TrimCSS(raw);
    if(auto color=ParseCSSColor(raw)) return *color;

    // Vector-like descriptor properties use the same whitespace/comma syntax
    // as ConvertUIPropertyText(). Keep them typed during interpolation.
    {
        std::string normalized=raw;
        std::replace(normalized.begin(),normalized.end(),',',' ');
        std::istringstream input(normalized);
        std::vector<float> values;
        float value=0.0f;
        while(input>>value) values.push_back(value);
        if(input.eof() && values.size()>=2 && values.size()<=4) return values;
    }

    if(auto scalar=ParseDynamicScalar(raw)) return *scalar;
    return raw;
}

std::string FormatDynamicAnimationValue(const DynamicAnimationValue& value) {
    if(const auto* scalar=std::get_if<DynamicScalar>(&value)) {
        std::ostringstream stream;
        stream<<scalar->value<<scalar->suffix;
        return stream.str();
    }
    if(const auto* color=std::get_if<Color>(&value)) {
        std::ostringstream stream;
        stream<<'#'<<std::hex<<std::setfill('0')
              <<std::setw(2)<<static_cast<int>(color->r)
              <<std::setw(2)<<static_cast<int>(color->g)
              <<std::setw(2)<<static_cast<int>(color->b)
              <<std::setw(2)<<static_cast<int>(color->a);
        return stream.str();
    }
    if(const auto* vector=std::get_if<std::vector<float>>(&value)) {
        std::ostringstream stream;
        for(std::size_t index=0;index<vector->size();++index) {
            if(index) stream<<' ';
            stream<<(*vector)[index];
        }
        return stream.str();
    }
    return std::get<std::string>(value);
}

DynamicAnimationValue InterpolateDynamicAnimationValue(
    const DynamicAnimationValue& from,
    const DynamicAnimationValue& to,
    float progress
) {
    progress=std::clamp(progress,0.0f,1.0f);
    if(from.index()!=to.index()) return progress<1.0f?from:to;

    if(const auto* a=std::get_if<DynamicScalar>(&from)) {
        const auto& b=std::get<DynamicScalar>(to);
        if(a->suffix!=b.suffix) return progress<1.0f?from:to;
        return DynamicScalar{Lerp(a->value,b.value,progress),b.suffix};
    }
    if(const auto* a=std::get_if<Color>(&from)) {
        return MixColors(*a,std::get<Color>(to),progress);
    }
    if(const auto* a=std::get_if<std::vector<float>>(&from)) {
        const auto& b=std::get<std::vector<float>>(to);
        if(a->size()!=b.size()) return progress<1.0f?from:to;
        std::vector<float> result(a->size());
        for(std::size_t index=0;index<a->size();++index) {
            result[index]=Lerp((*a)[index],b[index],progress);
        }
        return result;
    }
    return progress<1.0f?from:to;
}

std::optional<std::string> DynamicStyleValue(
    const ComputedStyle& style,
    const std::string& property
) {
    if(property.rfind("--",0)==0) {
        if(!style.custom_properties) return std::nullopt;
        auto found=style.custom_properties->find(property);
        if(found!=style.custom_properties->end()) return found->second;
        return std::nullopt;
    }
    const std::string prefix="shader-";
    if(property.size()>prefix.size() &&
       LowerCSS(property.substr(0,prefix.size()))==prefix) {
        if(!style.shader_uniforms) return std::nullopt;
        const std::string name=property.substr(prefix.size());
        for(const auto& binding:*style.shader_uniforms) {
            if(binding.name==name) return binding.raw_value;
        }
        const std::string lower_name=LowerCSS(name);
        for(const auto& binding:*style.shader_uniforms) {
            if(LowerCSS(binding.name)==lower_name) return binding.raw_value;
        }
        return std::nullopt;
    }

    if(style.component_style_properties) {
        const std::string requested=LowerCSS(TrimCSS(property));
        auto found=style.component_style_properties->find(requested);
        if(found!=style.component_style_properties->end()) return found->second;
    }
    return std::nullopt;
}

void ApplyDynamicStyleValue(
    ComputedStyle& style,
    const std::string& property,
    std::string value
) {
    if(property.rfind("--",0)==0) {
        auto values=style.custom_properties
            ? std::make_shared<UIStyleVariableMap>(*style.custom_properties)
            : std::make_shared<UIStyleVariableMap>();
        (*values)[property]=std::move(value);
        style.custom_properties=std::move(values);
        return;
    }
    const std::string prefix="shader-";
    if(property.size()>prefix.size() && LowerCSS(property.substr(0,prefix.size()))==prefix) {
        const std::string requested=property.substr(prefix.size());
        auto list=style.shader_uniforms
            ? std::make_shared<UIShaderUniformList>(*style.shader_uniforms)
            : std::make_shared<UIShaderUniformList>();
        auto found=std::find_if(list->begin(),list->end(),[&](const UIShaderUniformBinding& binding) {
            return binding.name==requested || LowerCSS(binding.name)==LowerCSS(requested);
        });
        if(found==list->end()) list->push_back({requested,std::move(value)});
        else found->raw_value=std::move(value);
        style.shader_uniforms=std::move(list);
        return;
    }

    auto values=style.component_style_properties
        ? std::make_shared<UIStyleVariableMap>(*style.component_style_properties)
        : std::make_shared<UIStyleVariableMap>();
    (*values)[LowerCSS(TrimCSS(property))]=std::move(value);
    style.component_style_properties=std::move(values);
}

const UITransitionSpec* TransitionForDynamicProperty(
    const ComputedStyle& style,
    const std::string& property
) {
    const UITransitionSpec* all_match=nullptr;
    const std::string lower=LowerCSS(property);
    for(const UITransitionSpec& transition:style.transitions) {
        if(transition.duration<=0.0f) continue;
        if(transition.all) all_match=&transition;
        for(const std::string& candidate:transition.dynamic_properties) {
            if(LowerCSS(candidate)==lower) return &transition;
        }
    }
    return all_match;
}

std::vector<std::string> DynamicTransitionNames(const ComputedStyle& style) {
    std::vector<std::string> result;
    bool include_all=false;
    for(const UITransitionSpec& transition:style.transitions) {
        include_all=include_all||transition.all;
        for(const std::string& name:transition.dynamic_properties) {
            if(std::find(result.begin(),result.end(),name)==result.end()) result.push_back(name);
        }
    }
    if(include_all && style.component_style_properties) {
        for(const auto& pair:*style.component_style_properties) {
            if(std::find(result.begin(),result.end(),pair.first)==result.end()) result.push_back(pair.first);
        }
    }
    return result;
}

struct ActiveDynamicTransition {
    std::string property;
    DynamicAnimationValue from;
    DynamicAnimationValue to;
    double started=0.0;
    float delay=0.0f;
    float duration=0.0f;
    ProgressTween tween;

    ActiveDynamicTransition(
        std::string name,DynamicAnimationValue a,DynamicAnimationValue b,double now,
        const UITransitionSpec& spec
    ) : property(std::move(name)),from(std::move(a)),to(std::move(b)),
        started(now),delay(spec.delay),duration(spec.duration),
        tween(MakeProgressTween(spec.easing,spec.duration)) {}
};

std::string InterpolateDynamicValue(
    const ActiveDynamicTransition& transition,
    float progress
) {
    return FormatDynamicAnimationValue(
        InterpolateDynamicAnimationValue(transition.from,transition.to,progress)
    );
}

struct ActiveTransition {
    PropertyType property=PropertyType::Opacity;
    PropertyValueType from=0.0f;
    PropertyValueType to=0.0f;
    double started=0.0;
    float delay=0.0f;
    float duration=0.0f;
    ProgressTween tween;

    ActiveTransition(
        PropertyType property_value,
        PropertyValueType from_value,
        PropertyValueType to_value,
        double start_time,
        const UITransitionSpec& spec
    ) : property(property_value),from(std::move(from_value)),to(std::move(to_value)),
        started(start_time),delay(spec.delay),duration(spec.duration),
        tween(MakeProgressTween(spec.easing,spec.duration)) {}
};

struct ActiveKeyframeAnimation {
    UIAnimationSpec spec;
    double started=0.0;
    double paused_at=0.0;
    double accumulated_pause=0.0;
    bool paused=false;
    bool stopped=false;
    bool reverse_override=false;
    bool completed=false;
    ProgressTween tween;

    ActiveKeyframeAnimation(UIAnimationSpec value,double now)
        : spec(std::move(value)),started(now),paused(spec.paused),
          tween(MakeProgressTween(spec.easing,std::max(0.001f,spec.duration))) {
        if(paused) paused_at=now;
    }
};

struct AnimationChannelState {
    bool initialized=false;
    std::size_t target_hash=0;
    std::size_t target_revision=0;
    ComputedStyle target;
    ComputedStyle output;
    std::map<PropertyType,ActiveTransition> transitions;
    std::map<std::string,ActiveDynamicTransition> dynamic_transitions;
    std::vector<UIAnimationSpec> automatic_specs;
    std::map<std::string,ActiveKeyframeAnimation> players;
};

enum class PendingControlType { Play,Pause,Resume,Stop,Restart,Reverse };
struct PendingControl { PendingControlType type; std::string name; bool restart=true; };

} // namespace

struct UIElementAnimationState {
    std::unordered_map<std::string,AnimationChannelState> channels;
    std::vector<PendingControl> pending;
};

namespace {

UIElementAnimationState& EnsureAnimationState(UIElementBase& element) {
    if(!element.animation_state) element.animation_state=std::make_shared<UIElementAnimationState>();
    return *element.animation_state;
}

const std::string& MainAnimationChannelKey() {
    static const std::string key = "__main";
    return key;
}

bool AnimationChannelRequiresTick(const AnimationChannelState& channel) {
    if(!channel.transitions.empty() || !channel.dynamic_transitions.empty()) return true;
    for(const auto& entry : channel.players) {
        const ActiveKeyframeAnimation& player = entry.second;
        if(player.stopped) return true;
        if(!player.completed && !player.paused) return true;
    }
    return false;
}

void StartPlayer(AnimationChannelState& channel,UIAnimationSpec spec,double now,bool restart) {
    spec.name=NormalizeUIPseudoStateName(spec.name);
    if(spec.name.empty()) return;
    auto found=channel.players.find(spec.name);
    if(found!=channel.players.end()&&!restart&&!found->second.stopped) return;
    channel.players.erase(spec.name);
    channel.players.emplace(spec.name,ActiveKeyframeAnimation(std::move(spec),now));
}

UIAnimationSpec FindPlayerSpec(
    const AnimationChannelState& channel,
    const ComputedStyle& target,
    const std::string& name
) {
    for(const UIAnimationSpec& spec:target.animations) if(spec.name==name) return spec;
    for(const UIAnimationSpec& spec:channel.automatic_specs) if(spec.name==name) return spec;
    UIAnimationSpec fallback; fallback.name=name; fallback.duration=1.0f;
    fallback.fill_mode=UIAnimationFillMode::Forwards;
    return fallback;
}

void ApplyPendingControls(
    UIElementAnimationState& state,
    AnimationChannelState& channel,
    const ComputedStyle& target,
    double now
) {
    for(const PendingControl& control:state.pending) {
        const std::string name=NormalizeUIPseudoStateName(control.name);
        auto found=channel.players.find(name);
        if(control.type==PendingControlType::Play||control.type==PendingControlType::Restart) {
            UIAnimationSpec spec=FindPlayerSpec(channel,target,name);
            // An RCSS declaration may keep an animation paused so it can be
            // triggered explicitly. Play/Restart always starts it running.
            spec.paused=false;
            StartPlayer(
                channel,
                std::move(spec),
                now,
                control.type==PendingControlType::Restart ? true : control.restart
            );
            continue;
        }
        if(found==channel.players.end()) continue;
        ActiveKeyframeAnimation& player=found->second;
        if(control.type==PendingControlType::Pause&&!player.paused) {
            player.paused=true; player.paused_at=now;
        }
        else if(control.type==PendingControlType::Resume&&player.paused) {
            player.accumulated_pause+=now-player.paused_at; player.paused=false;
        }
        else if(control.type==PendingControlType::Stop) player.stopped=true;
        else if(control.type==PendingControlType::Reverse) player.reverse_override=!player.reverse_override;
    }
    state.pending.clear();
}

void SyncAutomaticPlayers(
    AnimationChannelState& channel,
    const std::vector<UIAnimationSpec>& specs,
    double now
) {
    bool same=channel.automatic_specs.size()==specs.size();
    if(same) for(std::size_t i=0;i<specs.size();++i) if(!EqualAnimationSpec(channel.automatic_specs[i],specs[i])) { same=false; break; }
    if(same) return;
    std::set<std::string> old_names;
    for(const UIAnimationSpec& spec:channel.automatic_specs) old_names.insert(spec.name);
    channel.automatic_specs=specs;
    std::set<std::string> new_names;
    for(const UIAnimationSpec& spec:specs) {
        new_names.insert(spec.name);
        StartPlayer(channel,spec,now,true);
    }
    for(const std::string& old:old_names) if(new_names.find(old)==new_names.end()) channel.players.erase(old);
}

float DirectedPlayerProgress(
    ActiveKeyframeAnimation& player,
    long long iteration,
    float local,
    double duration
) {
    bool reverse=false;
    switch(player.spec.direction) {
        case UIAnimationDirection::Reverse: reverse=true; break;
        case UIAnimationDirection::Alternate: reverse=(iteration%2)!=0; break;
        case UIAnimationDirection::AlternateReverse: reverse=(iteration%2)==0; break;
        default: break;
    }
    if(player.reverse_override) reverse=!reverse;
    local=std::clamp(local,0.0f,1.0f);
    if(reverse) local=1.0f-local;
    const uint32_t milliseconds=static_cast<uint32_t>(
        std::round(local*duration*1000.0)
    );
    return player.tween.seek(milliseconds);
}

float PlayerProgress(ActiveKeyframeAnimation& player,double now,bool& before,bool& finished) {
    const double effective_now=player.paused?player.paused_at:now;
    const double elapsed=effective_now-player.started-player.accumulated_pause-player.spec.delay;
    const double duration=std::max(0.001,static_cast<double>(player.spec.duration));
    before=elapsed<0.0;
    finished=false;
    player.completed=false;
    if(before) {
        return DirectedPlayerProgress(player,0,0.0f,duration);
    }

    const double total_iterations=player.spec.infinite
        ? std::numeric_limits<double>::infinity()
        : std::max(0.0,static_cast<double>(player.spec.iterations));

    if(!player.spec.infinite&&elapsed>=duration*total_iterations) {
        finished=true;
        player.completed=true;
        if(total_iterations<=0.0) {
            return DirectedPlayerProgress(player,0,0.0f,duration);
        }

        double whole=0.0;
        const double fractional=std::modf(total_iterations,&whole);
        long long iteration=0;
        float local=1.0f;
        if(fractional>0.000001) {
            iteration=static_cast<long long>(whole);
            local=static_cast<float>(fractional);
        }
        else {
            iteration=std::max<long long>(0,static_cast<long long>(whole)-1);
        }
        return DirectedPlayerProgress(player,iteration,local,duration);
    }

    const double iteration_value=elapsed/duration;
    const long long iteration=static_cast<long long>(std::floor(iteration_value));
    const float local=static_cast<float>(
        iteration_value-static_cast<double>(iteration)
    );
    return DirectedPlayerProgress(player,iteration,local,duration);
}

void ApplyKeyframePlayer(
    ComputedStyle& output,
    ActiveKeyframeAnimation& player,
    const StyleSheet& sheet,
    const ComputedStyle& underlying,
    double now
) {
    auto definition=sheet.keyframes.find(NormalizeUIPseudoStateName(player.spec.name));
    if(definition==sheet.keyframes.end()||definition->second.steps.empty()||player.stopped) return;
    bool before=false,finished=false;
    float progress=PlayerProgress(player,now,before,finished);
    if(before&&player.spec.fill_mode!=UIAnimationFillMode::Backwards&&player.spec.fill_mode!=UIAnimationFillMode::Both) return;
    if(finished&&player.spec.fill_mode!=UIAnimationFillMode::Forwards&&player.spec.fill_mode!=UIAnimationFillMode::Both) return;
    const auto& steps=definition->second.steps;
    const UIKeyframeStep* lower=&steps.front();
    const UIKeyframeStep* upper=&steps.back();
    for(std::size_t index=1;index<steps.size();++index) {
        if(progress<=steps[index].offset) { lower=&steps[index-1]; upper=&steps[index]; break; }
    }
    const float range=std::max(0.00001f,upper->offset-lower->offset);
    const float local=std::clamp((progress-lower->offset)/range,0.0f,1.0f);
    ComputedStyle from=underlying,to=underlying;
    std::set<PropertyType> animated;
    for(const Declaration& declaration:lower->declarations) {
        if(!IsAnimatableProperty(declaration.property)) continue;
        ApplyResolvedDeclaration(from,declaration);
        animated.insert(declaration.property);
    }
    for(const Declaration& declaration:upper->declarations) {
        if(!IsAnimatableProperty(declaration.property)) continue;
        ApplyResolvedDeclaration(to,declaration);
        animated.insert(declaration.property);
    }
    for(PropertyType property:animated) {
        auto a=StylePropertyValue(from,property),b=StylePropertyValue(to,property);
        if(a&&b) ApplyAnimatedProperty(output,property,InterpolatePropertyValue(*a,*b,local));
    }
    std::set<std::string> dynamic_names;
    for(const auto& declaration:lower->component_style_properties) {
        dynamic_names.insert(LowerCSS(TrimCSS(declaration.name)));
    }
    for(const auto& declaration:upper->component_style_properties) {
        dynamic_names.insert(LowerCSS(TrimCSS(declaration.name)));
    }
    auto step_value=[&](const UIKeyframeStep& step,const std::string& name)->std::optional<std::string> {
        for(const auto& declaration:step.component_style_properties) {
            if(LowerCSS(TrimCSS(declaration.name))!=name) continue;
            if(underlying.custom_properties) {
                if(auto resolved=ResolveCSSVariables(declaration.raw_value,*underlying.custom_properties)) {
                    return *resolved;
                }
            }
            return declaration.raw_value;
        }
        return DynamicStyleValue(underlying,name);
    };
    for(const std::string& name:dynamic_names) {
        auto a=step_value(*lower,name);
        auto b=step_value(*upper,name);
        if(!a || !b) continue;
        const DynamicAnimationValue from_value=ParseDynamicAnimationValue(*a);
        const DynamicAnimationValue to_value=ParseDynamicAnimationValue(*b);
        ApplyDynamicStyleValue(
            output,name,
            FormatDynamicAnimationValue(
                InterpolateDynamicAnimationValue(from_value,to_value,local)
            )
        );
    }
}

} // namespace

const ComputedStyle& ResolveAnimatedStyleRef(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    const ComputedStyle& target_style,
    const std::string& channel_name,
    std::size_t target_revision
) {
    // Static elements return their retained target directly. No channel map,
    // full-style copy, or animation hash is needed.
    if(
        !element.animation_state &&
        target_style.transitions.empty() &&
        target_style.animations.empty()
    ) {
        return target_style;
    }

    UIElementAnimationState& state=EnsureAnimationState(element);
    const std::string& channel_key = channel_name.empty()
        ? MainAnimationChannelKey()
        : channel_name;
    AnimationChannelState& channel=state.channels[channel_key];
    const bool revision_keyed = target_revision != 0;
    const std::size_t target_hash = revision_keyed
        ? 0
        : HashVisualAnimationTarget(target_style);
    const bool same_target = revision_keyed
        ? channel.target_revision == target_revision
        : channel.target_hash == target_hash;

    if(
        channel.initialized &&
        state.pending.empty() &&
        same_target &&
        !AnimationChannelRequiresTick(channel)
    ) {
        return channel.output;
    }

    const double now=GetTime();

    if(!channel.initialized) {
        channel.initialized=true;
        channel.target_hash=target_hash;
        channel.target_revision=target_revision;
        channel.target=target_style;
        channel.output=target_style;
        SyncAutomaticPlayers(channel,target_style.animations,now);
        ApplyPendingControls(state,channel,target_style,now);
    }
    else {
        // Evaluate existing transitions before using their current visual values as
        // the source for an interrupted transition.
        ComputedStyle current=channel.target;
        for(auto iterator=channel.transitions.begin();iterator!=channel.transitions.end();) {
            ActiveTransition& transition=iterator->second;
            const double elapsed=now-transition.started-transition.delay;
            float progress=0.0f;
            if(elapsed>0.0) {
                progress=transition.tween.seek(static_cast<uint32_t>(
                    std::min<double>(transition.duration*1000.0,elapsed*1000.0)
                ));
            }
            ApplyAnimatedProperty(current,transition.property,InterpolatePropertyValue(transition.from,transition.to,progress));
            if(elapsed>=transition.duration) iterator=channel.transitions.erase(iterator);
            else ++iterator;
        }
        for(auto iterator=channel.dynamic_transitions.begin();iterator!=channel.dynamic_transitions.end();) {
            ActiveDynamicTransition& transition=iterator->second;
            const double elapsed=now-transition.started-transition.delay;
            float progress=0.0f;
            if(elapsed>0.0) {
                progress=transition.tween.seek(static_cast<uint32_t>(
                    std::min<double>(transition.duration*1000.0,elapsed*1000.0)
                ));
            }
            ApplyDynamicStyleValue(current,transition.property,InterpolateDynamicValue(transition,progress));
            if(elapsed>=transition.duration) iterator=channel.dynamic_transitions.erase(iterator);
            else ++iterator;
        }
        channel.output=current;

        for(PropertyType property:AnimatableProperties()) {
            auto previous=StylePropertyValue(channel.target,property);
            auto next=StylePropertyValue(target_style,property);
            if(!previous||!next||PropertyValuesEqual(*previous,*next)) continue;
            const UITransitionSpec* spec=TransitionForProperty(target_style,property);
            if(spec==nullptr) { channel.transitions.erase(property); continue; }
            auto visible=StylePropertyValue(channel.output,property);
            if(!visible) visible=previous;
            channel.transitions.erase(property);
            channel.transitions.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(property),
                std::forward_as_tuple(property,*visible,*next,now,*spec)
            );
        }
        for(const std::string& property:DynamicTransitionNames(target_style)) {
            auto previous=DynamicStyleValue(channel.target,property);
            auto next=DynamicStyleValue(target_style,property);
            if(!previous || !next || *previous==*next) continue;
            const UITransitionSpec* spec=TransitionForDynamicProperty(target_style,property);
            if(!spec) { channel.dynamic_transitions.erase(property); continue; }
            DynamicAnimationValue from=ParseDynamicAnimationValue(*previous);
            DynamicAnimationValue to=ParseDynamicAnimationValue(*next);
            if(auto visible=DynamicStyleValue(channel.output,property)) {
                from=ParseDynamicAnimationValue(*visible);
            }
            channel.dynamic_transitions.erase(property);
            channel.dynamic_transitions.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(property),
                std::forward_as_tuple(property,std::move(from),std::move(to),now,*spec)
            );
        }
        channel.target=target_style;
        SyncAutomaticPlayers(channel,target_style.animations,now);
        ApplyPendingControls(state,channel,target_style,now);
    }

    ComputedStyle output=target_style;
    for(auto iterator=channel.transitions.begin();iterator!=channel.transitions.end();) {
        ActiveTransition& transition=iterator->second;
        const double elapsed=now-transition.started-transition.delay;
        float progress=0.0f;
        if(elapsed>0.0) {
            progress=transition.tween.seek(static_cast<uint32_t>(
                std::min<double>(transition.duration*1000.0,elapsed*1000.0)
            ));
        }
        ApplyAnimatedProperty(output,transition.property,InterpolatePropertyValue(transition.from,transition.to,progress));
        if(elapsed>=transition.duration) iterator=channel.transitions.erase(iterator);
        else ++iterator;
    }

    for(auto iterator=channel.dynamic_transitions.begin();iterator!=channel.dynamic_transitions.end();) {
        ActiveDynamicTransition& transition=iterator->second;
        const double elapsed=now-transition.started-transition.delay;
        float progress=0.0f;
        if(elapsed>0.0) {
            progress=transition.tween.seek(static_cast<uint32_t>(
                std::min<double>(transition.duration*1000.0,elapsed*1000.0)
            ));
        }
        ApplyDynamicStyleValue(output,transition.property,InterpolateDynamicValue(transition,progress));
        if(elapsed>=transition.duration) iterator=channel.dynamic_transitions.erase(iterator);
        else ++iterator;
    }

    for(auto iterator=channel.players.begin();iterator!=channel.players.end();) {
        ApplyKeyframePlayer(output,iterator->second,style_sheet,target_style,now);
        bool before=false,finished=false;
        PlayerProgress(iterator->second,now,before,finished);
        const bool retain=iterator->second.spec.infinite||!finished||
            iterator->second.spec.fill_mode==UIAnimationFillMode::Forwards||
            iterator->second.spec.fill_mode==UIAnimationFillMode::Both;
        if(iterator->second.stopped||!retain) iterator=channel.players.erase(iterator);
        else ++iterator;
    }
    channel.target_hash=target_hash;
    channel.target_revision=target_revision;
    channel.output=std::move(output);
    return channel.output;
}

ComputedStyle ResolveAnimatedStyle(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    ComputedStyle target_style,
    const std::string& channel_name
) {
    return ResolveAnimatedStyleRef(
        element,style_sheet,target_style,channel_name,0
    );
}

namespace {
bool QueueAnimationControl(UIElementBase& element,const std::string& name,PendingControlType type,bool restart=true) {
    if(name.empty()) return false;
    EnsureAnimationState(element).pending.push_back({type,NormalizeUIPseudoStateName(name),restart});
    return true;
}
}

bool PlayUIAnimation(UIElementBase& element,const std::string& name,bool restart) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Play,restart);
    return QueueAnimationControl(element,name,PendingControlType::Play,restart);
}
bool PauseUIAnimation(UIElementBase& element,const std::string& name) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Pause);
    return QueueAnimationControl(element,name,PendingControlType::Pause);
}
bool ResumeUIAnimation(UIElementBase& element,const std::string& name) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Resume);
    return QueueAnimationControl(element,name,PendingControlType::Resume);
}
bool StopUIAnimation(UIElementBase& element,const std::string& name) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Stop);
    return QueueAnimationControl(element,name,PendingControlType::Stop);
}
bool RestartUIAnimation(UIElementBase& element,const std::string& name) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Restart,true);
    return QueueAnimationControl(element,name,PendingControlType::Restart);
}
bool ReverseUIAnimation(UIElementBase& element,const std::string& name) {
    QueueUILayoutAnimationCommand(element,name,UIAnimationCommand::Reverse);
    return QueueAnimationControl(element,name,PendingControlType::Reverse);
}
bool IsUIAnimationPlaying(const UIElementBase& element,const std::string& name) {
    if(!element.animation_state) return false;
    const std::string normalized=NormalizeUIPseudoStateName(name);
    for(const auto& channel:element.animation_state->channels) {
        auto found=channel.second.players.find(normalized);
        if(found!=channel.second.players.end()&&!found->second.stopped&&
           !found->second.completed) return true;
    }
    return false;
}

UIElementBase* FindUIElementByKey(UIContainer& root,const std::string& key) {
    auto found=FindRuntimeTarget(root,key);
    if(!found.has_value()&&!key.empty()&&key.front()!='#') {
        found=FindRuntimeTarget(root,"#"+key);
    }
    return found.has_value()?found->rect:nullptr;
}
const UIElementBase* FindUIElementByKey(const UIContainer& root,const std::string& key) {
    return FindUIElementByKey(const_cast<UIContainer&>(root),key);
}
