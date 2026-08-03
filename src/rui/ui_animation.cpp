#include "css_emulator.h"
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
    ComputedStyle target;
    ComputedStyle output;
    std::map<PropertyType,ActiveTransition> transitions;
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
}

} // namespace

ComputedStyle ResolveAnimatedStyle(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    ComputedStyle target_style,
    const std::string& channel_name
) {
    UIElementAnimationState& state=EnsureAnimationState(element);
    const std::string channel_key=channel_name.empty()?std::string("__main"):channel_name;
    AnimationChannelState& channel=state.channels[channel_key];
    const double now=GetTime();

    if(!channel.initialized) {
        channel.initialized=true;
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
    channel.output=output;
    return output;
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
