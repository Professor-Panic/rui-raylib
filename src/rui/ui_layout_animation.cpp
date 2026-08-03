#include "css_emulator.h"
#include "tweeny-4.0.0.h"

#include <map>

namespace {

using ProgressTween = tweeny::tween<float>;

ProgressTween MakeLayoutTween(UIEasing easing,float duration_seconds) {
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
        default: builder.via(tweeny::easing::quadraticInOut); break;
    }
    return std::move(builder).build();
}

const std::vector<PropertyType>& LayoutProperties() {
    static const std::vector<PropertyType> properties = {
        PropertyType::Width,PropertyType::Height,
        PropertyType::MinWidth,PropertyType::MaxWidth,
        PropertyType::MinHeight,PropertyType::MaxHeight,
        PropertyType::Padding,PropertyType::PaddingLeft,PropertyType::PaddingRight,
        PropertyType::PaddingTop,PropertyType::PaddingBottom,
        PropertyType::Margin,PropertyType::MarginLeft,PropertyType::MarginRight,
        PropertyType::MarginTop,PropertyType::MarginBottom,
        PropertyType::FontSize,PropertyType::BorderWidth,
        PropertyType::Gap,PropertyType::RowGap,PropertyType::ColumnGap,
        PropertyType::Left,PropertyType::Right,PropertyType::Top,PropertyType::Bottom,
        PropertyType::FlexGrow,PropertyType::FlexShrink,PropertyType::FlexBasis,
        PropertyType::Scale,PropertyType::HeaderHeight,PropertyType::ItemHeight,
        PropertyType::PopupWidth,PropertyType::PopupMaxHeight,PropertyType::Indent,
        PropertyType::SplitterSize,PropertyType::TabHeight,PropertyType::ScrollbarSize
    };
    return properties;
}

bool IsLayoutProperty(PropertyType property) {
    const auto& properties = LayoutProperties();
    return std::find(properties.begin(),properties.end(),property) != properties.end();
}

std::optional<PropertyValueType> ValueOf(const ComputedStyle& style,PropertyType property) {
    switch(property) {
        case PropertyType::Width: return style.width;
        case PropertyType::Height: return style.height;
        case PropertyType::MinWidth: return style.min_width;
        case PropertyType::MaxWidth: return style.max_width;
        case PropertyType::MinHeight: return style.min_height;
        case PropertyType::MaxHeight: return style.max_height;
        case PropertyType::FontSize: return style.font_size;
        case PropertyType::BorderWidth: return style.border_width;
        case PropertyType::Gap: return style.gap;
        case PropertyType::RowGap: return style.row_gap;
        case PropertyType::ColumnGap: return style.column_gap;
        case PropertyType::Left: return style.left;
        case PropertyType::Right: return style.right;
        case PropertyType::Top: return style.top;
        case PropertyType::Bottom: return style.bottom;
        case PropertyType::FlexGrow: return style.flex_grow;
        case PropertyType::FlexShrink: return style.flex_shrink;
        case PropertyType::FlexBasis: return style.flex_basis;
        case PropertyType::Scale: return style.scale;
        case PropertyType::HeaderHeight: return style.header_height;
        case PropertyType::ItemHeight: return style.item_height;
        case PropertyType::PopupWidth: return style.popup_width;
        case PropertyType::PopupMaxHeight: return style.popup_max_height;
        case PropertyType::Indent: return style.indent;
        case PropertyType::SplitterSize: return style.splitter_size;
        case PropertyType::TabHeight: return style.tab_height;
        case PropertyType::ScrollbarSize: return style.scrollbar_size;
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

bool Same(const PropertyValueType& first,const PropertyValueType& second) {
    if(first.index() != second.index()) return false;
    if(const float* value = std::get_if<float>(&first)) {
        return std::abs(*value-std::get<float>(second)) < 0.0001f;
    }
    if(const Vector4* value = std::get_if<Vector4>(&first)) {
        const Vector4 other = std::get<Vector4>(second);
        return std::abs(value->x-other.x)<0.0001f &&
            std::abs(value->y-other.y)<0.0001f &&
            std::abs(value->z-other.z)<0.0001f &&
            std::abs(value->w-other.w)<0.0001f;
    }
    return false;
}

PropertyValueType Interpolate(const PropertyValueType& first,const PropertyValueType& second,float amount) {
    amount = std::clamp(amount,0.0f,1.0f);
    if(first.index() != second.index()) return amount < 1.0f ? first : second;
    if(const float* value = std::get_if<float>(&first)) {
        return Lerp(*value,std::get<float>(second),amount);
    }
    if(const Vector4* value = std::get_if<Vector4>(&first)) {
        const Vector4 other = std::get<Vector4>(second);
        return Vector4{
            Lerp(value->x,other.x,amount),Lerp(value->y,other.y,amount),
            Lerp(value->z,other.z,amount),Lerp(value->w,other.w,amount)
        };
    }
    return amount < 1.0f ? first : second;
}

void Apply(ComputedStyle& style,PropertyType property,const PropertyValueType& value) {
    if(property == PropertyType::Padding && std::holds_alternative<Vector4>(value)) {
        const Vector4 edges = std::get<Vector4>(value);
        style.padding.left = edges.x;
        style.padding.right = edges.y;
        style.padding.top = edges.z;
        style.padding.bottom = edges.w;
        return;
    }
    if(property == PropertyType::Margin && std::holds_alternative<Vector4>(value)) {
        const Vector4 edges = std::get<Vector4>(value);
        style.margin.left = edges.x;
        style.margin.right = edges.y;
        style.margin.top = edges.z;
        style.margin.bottom = edges.w;
        return;
    }
    const float* scalar = std::get_if<float>(&value);
    if(!scalar) return;
    switch(property) {
        case PropertyType::Width: style.width = *scalar; break;
        case PropertyType::Height: style.height = *scalar; break;
        case PropertyType::MinWidth: style.min_width = *scalar; break;
        case PropertyType::MaxWidth: style.max_width = *scalar; break;
        case PropertyType::MinHeight: style.min_height = *scalar; break;
        case PropertyType::MaxHeight: style.max_height = *scalar; break;
        case PropertyType::FontSize: style.font_size = *scalar; break;
        case PropertyType::BorderWidth: style.border_width = *scalar; break;
        case PropertyType::Gap: style.gap = *scalar; break;
        case PropertyType::RowGap: style.row_gap = *scalar; break;
        case PropertyType::ColumnGap: style.column_gap = *scalar; break;
        case PropertyType::Left: style.left = *scalar; break;
        case PropertyType::Right: style.right = *scalar; break;
        case PropertyType::Top: style.top = *scalar; break;
        case PropertyType::Bottom: style.bottom = *scalar; break;
        case PropertyType::FlexGrow: style.flex_grow = *scalar; break;
        case PropertyType::FlexShrink: style.flex_shrink = *scalar; break;
        case PropertyType::FlexBasis: style.flex_basis = *scalar; break;
        case PropertyType::Scale: style.scale = *scalar; break;
        case PropertyType::HeaderHeight: style.header_height = *scalar; break;
        case PropertyType::ItemHeight: style.item_height = *scalar; break;
        case PropertyType::PopupWidth: style.popup_width = *scalar; break;
        case PropertyType::PopupMaxHeight: style.popup_max_height = *scalar; break;
        case PropertyType::Indent: style.indent = *scalar; break;
        case PropertyType::SplitterSize: style.splitter_size = *scalar; break;
        case PropertyType::TabHeight: style.tab_height = *scalar; break;
        case PropertyType::ScrollbarSize: style.scrollbar_size = *scalar; break;
        case PropertyType::PaddingLeft: style.padding.left = *scalar; break;
        case PropertyType::PaddingRight: style.padding.right = *scalar; break;
        case PropertyType::PaddingTop: style.padding.top = *scalar; break;
        case PropertyType::PaddingBottom: style.padding.bottom = *scalar; break;
        case PropertyType::MarginLeft: style.margin.left = *scalar; break;
        case PropertyType::MarginRight: style.margin.right = *scalar; break;
        case PropertyType::MarginTop: style.margin.top = *scalar; break;
        case PropertyType::MarginBottom: style.margin.bottom = *scalar; break;
        default: break;
    }
}

const UITransitionSpec* TransitionFor(const ComputedStyle& style,PropertyType property) {
    const UITransitionSpec* all = nullptr;
    for(const UITransitionSpec& transition : style.transitions) {
        if(transition.duration <= 0.0f) continue;
        if(transition.all) all = &transition;
        else if(std::find(transition.properties.begin(),transition.properties.end(),property) != transition.properties.end()) {
            return &transition;
        }
    }
    return all;
}

struct LayoutTransition {
    PropertyType property = PropertyType::Width;
    PropertyValueType from = 0.0f;
    PropertyValueType to = 0.0f;
    double started = 0.0;
    float delay = 0.0f;
    float duration = 0.0f;
    ProgressTween tween;

    LayoutTransition(
        PropertyType property_value,
        PropertyValueType from_value,
        PropertyValueType to_value,
        double start,
        const UITransitionSpec& transition
    ) : property(property_value),from(std::move(from_value)),to(std::move(to_value)),
        started(start),delay(transition.delay),duration(transition.duration),
        tween(MakeLayoutTween(transition.easing,transition.duration)) {}
};

struct LayoutPlayer {
    UIAnimationSpec spec;
    double started = 0.0;
    double paused_at = 0.0;
    double accumulated_pause = 0.0;
    bool paused = false;
    bool stopped = false;
    bool reversed = false;
    ProgressTween tween;

    LayoutPlayer(UIAnimationSpec value,double now)
        : spec(std::move(value)),started(now),paused(spec.paused),
          tween(MakeLayoutTween(spec.easing,std::max(0.001f,spec.duration))) {
        if(paused) paused_at = now;
    }
};

struct LayoutCommand {
    UIAnimationCommand command = UIAnimationCommand::Play;
    std::string name;
    bool restart = true;
};

bool SpecsEqual(const std::vector<UIAnimationSpec>& first,const std::vector<UIAnimationSpec>& second) {
    if(first.size() != second.size()) return false;
    for(std::size_t index=0; index<first.size(); ++index) {
        const auto& a = first[index];
        const auto& b = second[index];
        if(a.name!=b.name || a.duration!=b.duration || a.delay!=b.delay ||
           a.easing!=b.easing || a.iterations!=b.iterations || a.infinite!=b.infinite ||
           a.direction!=b.direction || a.fill_mode!=b.fill_mode || a.paused!=b.paused) return false;
    }
    return true;
}

float PlayerProgress(LayoutPlayer& player,double now,bool& before,bool& finished) {
    const double effective_now = player.paused ? player.paused_at : now;
    const double elapsed = effective_now-player.started-player.accumulated_pause-player.spec.delay;
    const double duration = std::max(0.001,static_cast<double>(player.spec.duration));
    before = elapsed < 0.0;
    finished = false;
    if(before) return player.reversed ? 1.0f : 0.0f;
    const double iterations = player.spec.infinite
        ? std::numeric_limits<double>::infinity()
        : std::max(0.0,static_cast<double>(player.spec.iterations));
    if(!player.spec.infinite && elapsed >= duration*iterations) {
        finished = true;
        long long last = std::max<long long>(0,static_cast<long long>(std::ceil(iterations))-1);
        bool reverse = player.spec.direction == UIAnimationDirection::Reverse ||
            ((player.spec.direction == UIAnimationDirection::Alternate ||
              player.spec.direction == UIAnimationDirection::AlternateReverse) && (last&1));
        if(player.spec.direction == UIAnimationDirection::AlternateReverse) reverse = !reverse;
        if(player.reversed) reverse = !reverse;
        return reverse ? 0.0f : 1.0f;
    }
    const double iteration_value = elapsed/duration;
    const long long iteration = static_cast<long long>(std::floor(iteration_value));
    float local = static_cast<float>(iteration_value-static_cast<double>(iteration));
    bool reverse = player.spec.direction == UIAnimationDirection::Reverse;
    if(player.spec.direction == UIAnimationDirection::Alternate) reverse = (iteration&1) != 0;
    if(player.spec.direction == UIAnimationDirection::AlternateReverse) reverse = (iteration&1) == 0;
    if(player.reversed) reverse = !reverse;
    if(reverse) local = 1.0f-local;
    return player.tween.seek(static_cast<uint32_t>(std::round(local*duration*1000.0)));
}

} // namespace

struct UILayoutAnimationState {
    bool initialized = false;
    ComputedStyle target;
    ComputedStyle output;
    std::map<PropertyType,LayoutTransition> transitions;
    std::vector<UIAnimationSpec> automatic_specs;
    std::map<std::string,LayoutPlayer> players;
    std::vector<LayoutCommand> pending;
};

namespace {

UILayoutAnimationState& Ensure(UIElementBase& element) {
    if(!element.layout_animation_state) {
        element.layout_animation_state = std::make_shared<UILayoutAnimationState>();
    }
    return *element.layout_animation_state;
}

void SyncPlayers(UILayoutAnimationState& state,const std::vector<UIAnimationSpec>& specs,double now) {
    if(SpecsEqual(state.automatic_specs,specs)) return;
    state.automatic_specs = specs;
    std::set<std::string> retained;
    for(UIAnimationSpec spec : specs) {
        spec.name = NormalizeUIPseudoStateName(spec.name);
        if(spec.name.empty()) continue;
        retained.insert(spec.name);
        auto found = state.players.find(spec.name);
        if(found == state.players.end()) state.players.emplace(spec.name,LayoutPlayer(spec,now));
        else found->second.spec = spec;
    }
    for(auto iterator=state.players.begin(); iterator!=state.players.end();) {
        if(!retained.count(iterator->first) && !iterator->second.stopped) iterator = state.players.erase(iterator);
        else ++iterator;
    }
}

UIAnimationSpec FindSpec(const UILayoutAnimationState& state,const std::string& name) {
    for(const UIAnimationSpec& spec : state.target.animations) {
        if(NormalizeUIPseudoStateName(spec.name) == name) return spec;
    }
    UIAnimationSpec fallback;
    fallback.name = name;
    fallback.duration = 1.0f;
    fallback.fill_mode = UIAnimationFillMode::Forwards;
    return fallback;
}

void ApplyCommands(UILayoutAnimationState& state,double now) {
    for(const LayoutCommand& command : state.pending) {
        const std::string name = NormalizeUIPseudoStateName(command.name);
        auto found = state.players.find(name);
        if(command.command == UIAnimationCommand::Play || command.command == UIAnimationCommand::Restart) {
            if(found == state.players.end() || command.restart || command.command == UIAnimationCommand::Restart) {
                UIAnimationSpec spec = FindSpec(state,name);
                spec.paused = false;
                state.players.erase(name);
                state.players.emplace(name,LayoutPlayer(spec,now));
            }
            continue;
        }
        if(found == state.players.end()) continue;
        LayoutPlayer& player = found->second;
        if(command.command == UIAnimationCommand::Pause && !player.paused) {
            player.paused = true;
            player.paused_at = now;
        }
        else if(command.command == UIAnimationCommand::Resume && player.paused) {
            player.accumulated_pause += now-player.paused_at;
            player.paused = false;
        }
        else if(command.command == UIAnimationCommand::Stop) player.stopped = true;
        else if(command.command == UIAnimationCommand::Reverse) player.reversed = !player.reversed;
    }
    state.pending.clear();
}

void ApplyKeyframes(
    ComputedStyle& output,
    LayoutPlayer& player,
    const StyleSheet& sheet,
    const ComputedStyle& underlying,
    double now
) {
    const auto definition = sheet.keyframes.find(NormalizeUIPseudoStateName(player.spec.name));
    if(definition == sheet.keyframes.end() || definition->second.steps.empty() || player.stopped) return;
    bool before = false;
    bool finished = false;
    const float progress = PlayerProgress(player,now,before,finished);
    if(before && player.spec.fill_mode != UIAnimationFillMode::Backwards && player.spec.fill_mode != UIAnimationFillMode::Both) return;
    if(finished && player.spec.fill_mode != UIAnimationFillMode::Forwards && player.spec.fill_mode != UIAnimationFillMode::Both) return;
    const auto& steps = definition->second.steps;
    const UIKeyframeStep* lower = &steps.front();
    const UIKeyframeStep* upper = &steps.back();
    for(std::size_t index=1; index<steps.size(); ++index) {
        if(progress <= steps[index].offset) {
            lower = &steps[index-1];
            upper = &steps[index];
            break;
        }
    }
    const float range = std::max(0.00001f,upper->offset-lower->offset);
    const float local = std::clamp((progress-lower->offset)/range,0.0f,1.0f);
    ComputedStyle from = underlying;
    ComputedStyle to = underlying;
    std::set<PropertyType> properties;
    for(const Declaration& declaration : lower->declarations) {
        if(!IsLayoutProperty(declaration.property)) continue;
        ApplyResolvedDeclaration(from,declaration);
        properties.insert(declaration.property);
    }
    for(const Declaration& declaration : upper->declarations) {
        if(!IsLayoutProperty(declaration.property)) continue;
        ApplyResolvedDeclaration(to,declaration);
        properties.insert(declaration.property);
    }
    for(PropertyType property : properties) {
        const auto first = ValueOf(from,property);
        const auto second = ValueOf(to,property);
        if(first && second) Apply(output,property,Interpolate(*first,*second,local));
    }
}

bool HasActiveInElement(const UIElementBase& element) {
    return IsUILayoutAnimationActive(element);
}

bool HasActiveInContainer(const UIContainer& container) {
    if(HasActiveInElement(container.rect)) return true;
    for(const UIElement& element : container.elements) {
        bool active = std::visit([&](const auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return false;
                if(HasActiveInElement(widget->rect)) return true;
                if(const auto* dock = dynamic_cast<const UIDockSpace*>(widget.get())) {
                    for(const UIDockPanel& panel : dock->panels) {
                        if(HasActiveInContainer(panel.content)) return true;
                    }
                }
                return false;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && HasActiveInContainer(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget || !HasActiveInElement(widget->rect)) {
                    if(!widget) return false;
                }
                for(const UITab& tab : widget->tabs) if(HasActiveInContainer(tab.content)) return true;
                return widget && HasActiveInElement(widget->rect);
            }
            else if constexpr(IsUICompositePointerV<T>) {
                return widget && (HasActiveInElement(widget->rect) || HasActiveInContainer(widget->content));
            }
            else return HasActiveInElement(widget.rect);
        },element);
        if(active) return true;
    }
    return false;
}

bool RefreshContainerStates(UIContainer& container) {
    bool changed = false;
    container.rect.pseudo_states.Update();
    std::size_t hash = container.rect.pseudo_states.ActiveHash();
    if(hash != container.rect.last_layout_pseudo_hash) {
        container.rect.last_layout_pseudo_hash = hash;
        changed = true;
    }
    for(UIElement& element : container.elements) {
        changed = std::visit([&](auto& widget) {
            using T = std::decay_t<decltype(widget)>;
            if constexpr(IsUICustomElementPointerV<T>) {
                if(!widget) return false;
                widget->rect.pseudo_states.Update();
                const std::size_t next = widget->rect.pseudo_states.ActiveHash();
                bool local = next != widget->rect.last_layout_pseudo_hash;
                widget->rect.last_layout_pseudo_hash = next;
                if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                    for(UIDockPanel& panel : dock->panels) local = RefreshContainerStates(panel.content) || local;
                }
                return local;
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                return widget && RefreshContainerStates(*widget);
            }
            else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                if(!widget) return false;
                widget->rect.pseudo_states.Update();
                std::size_t next = widget->rect.pseudo_states.ActiveHash();
                bool local = next != widget->rect.last_layout_pseudo_hash;
                widget->rect.last_layout_pseudo_hash = next;
                for(UITab& tab : widget->tabs) local = RefreshContainerStates(tab.content) || local;
                return local;
            }
            else if constexpr(IsUICompositePointerV<T>) {
                if(!widget) return false;
                widget->rect.pseudo_states.Update();
                std::size_t next = widget->rect.pseudo_states.ActiveHash();
                bool local = next != widget->rect.last_layout_pseudo_hash;
                widget->rect.last_layout_pseudo_hash = next;
                return RefreshContainerStates(widget->content) || local;
            }
            else {
                widget.rect.pseudo_states.Update();
                std::size_t next = widget.rect.pseudo_states.ActiveHash();
                bool local = next != widget.rect.last_layout_pseudo_hash;
                widget.rect.last_layout_pseudo_hash = next;
                return local;
            }
        },element) || changed;
    }
    return changed;
}

} // namespace

ComputedStyle ResolveLayoutAnimatedStyle(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    ComputedStyle target_style
) {
    UILayoutAnimationState& state = Ensure(element);
    const double now = GetTime();
    if(!state.initialized) {
        state.initialized = true;
        state.target = target_style;
        state.output = target_style;
        SyncPlayers(state,target_style.animations,now);
        ApplyCommands(state,now);
    }
    else {
        ComputedStyle current = state.target;
        for(auto iterator=state.transitions.begin(); iterator!=state.transitions.end();) {
            LayoutTransition& transition = iterator->second;
            const double elapsed = now-transition.started-transition.delay;
            float progress = 0.0f;
            if(elapsed > 0.0) {
                progress = transition.tween.seek(static_cast<uint32_t>(
                    std::min<double>(transition.duration*1000.0,elapsed*1000.0)
                ));
            }
            Apply(current,transition.property,Interpolate(transition.from,transition.to,progress));
            if(elapsed >= transition.duration) iterator = state.transitions.erase(iterator);
            else ++iterator;
        }
        state.output = current;
        for(PropertyType property : LayoutProperties()) {
            const auto previous = ValueOf(state.target,property);
            const auto next = ValueOf(target_style,property);
            if(!previous || !next || Same(*previous,*next)) continue;
            const UITransitionSpec* transition = TransitionFor(target_style,property);
            if(!transition) {
                state.transitions.erase(property);
                continue;
            }
            auto visible = ValueOf(state.output,property);
            if(!visible) visible = previous;
            state.transitions.erase(property);
            state.transitions.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(property),
                std::forward_as_tuple(property,*visible,*next,now,*transition)
            );
        }
        state.target = target_style;
        SyncPlayers(state,target_style.animations,now);
        ApplyCommands(state,now);
    }

    ComputedStyle output = target_style;
    for(auto iterator=state.transitions.begin(); iterator!=state.transitions.end();) {
        LayoutTransition& transition = iterator->second;
        const double elapsed = now-transition.started-transition.delay;
        float progress = 0.0f;
        if(elapsed > 0.0) {
            progress = transition.tween.seek(static_cast<uint32_t>(
                std::min<double>(transition.duration*1000.0,elapsed*1000.0)
            ));
        }
        Apply(output,transition.property,Interpolate(transition.from,transition.to,progress));
        if(elapsed >= transition.duration) iterator = state.transitions.erase(iterator);
        else ++iterator;
    }
    for(auto iterator=state.players.begin(); iterator!=state.players.end();) {
        ApplyKeyframes(output,iterator->second,style_sheet,target_style,now);
        bool before = false;
        bool finished = false;
        PlayerProgress(iterator->second,now,before,finished);
        const bool retain = iterator->second.spec.infinite || !finished ||
            iterator->second.spec.fill_mode == UIAnimationFillMode::Forwards ||
            iterator->second.spec.fill_mode == UIAnimationFillMode::Both;
        if(iterator->second.stopped || !retain) iterator = state.players.erase(iterator);
        else ++iterator;
    }
    state.output = output;
    return output;
}

bool IsUILayoutAnimationActive(const UIElementBase& element) {
    if(!element.layout_animation_state) return false;
    const UILayoutAnimationState& state = *element.layout_animation_state;
    if(!state.transitions.empty()) return true;
    for(const auto& entry : state.players) {
        if(!entry.second.stopped && (!entry.second.paused || entry.second.spec.infinite)) return true;
    }
    return false;
}

bool HasActiveUILayoutAnimations(const UIContainer& root) {
    return HasActiveInContainer(root);
}

bool RefreshUIPseudoStatesForLayout(UIContainer& root) {
    return RefreshContainerStates(root);
}

void QueueUILayoutAnimationCommand(
    UIElementBase& element,
    const std::string& name,
    UIAnimationCommand command,
    bool restart
) {
    if(name.empty()) return;
    Ensure(element).pending.push_back({command,NormalizeUIPseudoStateName(name),restart});
}
