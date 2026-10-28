#include "css_emulator.h"

namespace {
thread_local std::vector<UIInputContext> RUI_INPUT_CONTEXT_STACK;
thread_local std::vector<UITransformContext> RUI_TRANSFORM_CONTEXT_STACK;
}

Vector2 GetUIMousePosition() {
    return RUI_INPUT_CONTEXT_STACK.empty()
        ? ::GetMousePosition()
        : RUI_INPUT_CONTEXT_STACK.back().mouse_position;
}

Vector2 GetUIMouseDelta() {
    return RUI_INPUT_CONTEXT_STACK.empty()
        ? ::GetMouseDelta()
        : RUI_INPUT_CONTEXT_STACK.back().mouse_delta;
}

void PushUIInputContext(const UIInputContext& context) {
    RUI_INPUT_CONTEXT_STACK.push_back(context);
}

void PopUIInputContext() {
    if(!RUI_INPUT_CONTEXT_STACK.empty()) RUI_INPUT_CONTEXT_STACK.pop_back();
}

UITransformContext GetCurrentUITransform() {
    return RUI_TRANSFORM_CONTEXT_STACK.empty()
        ? UITransformContext{}
        : RUI_TRANSFORM_CONTEXT_STACK.back();
}

void PushUITransform(Vector2 translation,float scale) {
    const UITransformContext parent=GetCurrentUITransform();
    UITransformContext next;
    next.scale=parent.scale*std::max(0.0001f,scale);
    next.offset={
        parent.offset.x+translation.x*parent.scale,
        parent.offset.y+translation.y*parent.scale
    };
    RUI_TRANSFORM_CONTEXT_STACK.push_back(next);
}

void PushUITransformAbsolute(const UITransformContext& transform) {
    RUI_TRANSFORM_CONTEXT_STACK.push_back(transform);
}

void PopUITransform() {
    if(!RUI_TRANSFORM_CONTEXT_STACK.empty()) RUI_TRANSFORM_CONTEXT_STACK.pop_back();
}

Vector2 UITransformPointToScreen(Vector2 point) {
    const UITransformContext transform=GetCurrentUITransform();
    return {
        transform.offset.x+point.x*transform.scale,
        transform.offset.y+point.y*transform.scale
    };
}

Vector2 UIScreenPointToLogical(Vector2 point) {
    const UITransformContext transform=GetCurrentUITransform();
    const float scale=std::max(0.0001f,transform.scale);
    return {
        (point.x-transform.offset.x)/scale,
        (point.y-transform.offset.y)/scale
    };
}

Rectangle UITransformRectangleToScreen(Rectangle rectangle) {
    const Vector2 position=UITransformPointToScreen({rectangle.x,rectangle.y});
    const UITransformContext transform=GetCurrentUITransform();
    return {
        position.x,position.y,
        rectangle.width*transform.scale,
        rectangle.height*transform.scale
    };
}

Rectangle UIScreenRectangleToLogical(Rectangle rectangle) {
    const Vector2 position=UIScreenPointToLogical({rectangle.x,rectangle.y});
    const UITransformContext transform=GetCurrentUITransform();
    const float scale=std::max(0.0001f,transform.scale);
    return {
        position.x,position.y,
        rectangle.width/scale,
        rectangle.height/scale
    };
}

Rectangle CurrentUIVisibleLogicalScreenRect() {
    return UIScreenRectangleToLogical({
        0.0f,0.0f,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight())
    });
}

void BeginUIScissorMode(Rectangle logical_rectangle) {
    Rectangle rectangle=UITransformRectangleToScreen(logical_rectangle);
    rectangle.x-=CSS_UI_DRAW_TARGET_ORIGIN.x;
    rectangle.y-=CSS_UI_DRAW_TARGET_ORIGIN.y;
    BeginScissorMode(
        static_cast<int>(std::floor(rectangle.x)),
        static_cast<int>(std::floor(rectangle.y)),
        std::max(0,static_cast<int>(std::ceil(rectangle.width))),
        std::max(0,static_cast<int>(std::ceil(rectangle.height)))
    );
}


std::string NormalizeUIPseudoStateName(std::string name) {
    name.erase(
        name.begin(),
        std::find_if(name.begin(),name.end(),[](unsigned char c) {
            return !std::isspace(c) && c != ':';
        })
    );
    while(!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
        name.pop_back();
    }
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

void UIPseudoStateSet::InvalidateActiveCache() {
    active_cache_dirty_ = true;
}

void UIPseudoStateSet::RebuildActiveCache() const {
    if(!active_cache_dirty_) return;

    active_names_cache_.clear();
    active_names_cache_.reserve(states_.size());
    for(const auto& entry : states_) {
        if(entry.second.active) active_names_cache_.push_back(entry.first);
    }
    std::sort(active_names_cache_.begin(),active_names_cache_.end());

    active_hash_cache_ = 0;
    for(const std::string& name : active_names_cache_) {
        HashCombine(active_hash_cache_,std::hash<std::string>{}(name));
    }
    active_cache_dirty_ = false;
}

UIPseudoState& UIPseudoStateSet::Define(
    std::string name,
    bool initial_value,
    std::function<void(bool&)> update
) {
    name = NormalizeUIPseudoStateName(std::move(name));
    UIPseudoState& state = states_[name];
    state.active = initial_value;
    state.update = std::move(update);
    state.lifetime = {};
    state.lifetime_tracked = false;
    InvalidateActiveCache();
    return state;
}

UIPseudoState& UIPseudoStateSet::DefineTracked(
    std::string name,
    bool initial_value,
    std::weak_ptr<void> lifetime,
    std::function<void(bool&)> update
) {
    name = NormalizeUIPseudoStateName(std::move(name));
    UIPseudoState& state = states_[name];
    state.active = initial_value;
    state.update = std::move(update);
    state.lifetime = std::move(lifetime);
    state.lifetime_tracked = true;
    InvalidateActiveCache();
    return state;
}

bool UIPseudoStateSet::Set(const std::string& name, bool value) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    if(normalized.empty()) return false;
    auto [iterator,inserted] = states_.try_emplace(normalized);
    UIPseudoState& state = iterator->second;
    const bool changed = state.active != value;
    state.active = value;
    if(changed || inserted) InvalidateActiveCache();
    return changed;
}

bool UIPseudoStateSet::SetKnown(
    std::string_view normalized_name,
    bool value
) {
    if(normalized_name.empty()) return false;

    for(auto& entry : states_) {
        if(entry.first != normalized_name) continue;
        const bool changed = entry.second.active != value;
        entry.second.active = value;
        if(changed) InvalidateActiveCache();
        return changed;
    }

    UIPseudoState state;
    state.active = value;
    states_.emplace(std::string(normalized_name),std::move(state));
    InvalidateActiveCache();
    return true;
}

bool UIPseudoStateSet::Toggle(const std::string& name) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    if(normalized.empty()) return false;
    UIPseudoState& state = states_[normalized];
    state.active = !state.active;
    InvalidateActiveCache();
    return state.active;
}

bool UIPseudoStateSet::Remove(const std::string& name) {
    const bool removed = states_.erase(NormalizeUIPseudoStateName(name)) != 0;
    if(removed) InvalidateActiveCache();
    return removed;
}

bool UIPseudoStateSet::Active(const std::string& name) const {
    const auto found = states_.find(NormalizeUIPseudoStateName(name));
    return found != states_.end() && found->second.active;
}

bool UIPseudoStateSet::Contains(const std::string& name) const {
    return states_.find(NormalizeUIPseudoStateName(name)) != states_.end();
}

void UIPseudoStateSet::Clear() {
    if(states_.empty()) return;
    states_.clear();
    InvalidateActiveCache();
}

void UIPseudoStateSet::Update() {
    bool changed = false;
    for(auto iterator = states_.begin(); iterator != states_.end();) {
        UIPseudoState& state = iterator->second;
        if(state.lifetime_tracked && state.lifetime.expired()) {
            iterator = states_.erase(iterator);
            changed = true;
            continue;
        }
        const bool previous = state.active;
        if(state.update) state.update(state.active);
        changed = changed || previous != state.active;
        ++iterator;
    }
    if(changed) InvalidateActiveCache();
}

void UIPseudoStateSet::SyncBuiltIn(
    const std::string& name,
    bool engine_value
) {
    const std::string normalized = NormalizeUIPseudoStateName(name);
    auto [iterator,inserted] = states_.try_emplace(normalized);
    UIPseudoState& state = iterator->second;
    const bool previous = state.active;
    state.active = engine_value;
    if(state.lifetime_tracked && state.lifetime.expired()) {
        states_.erase(iterator);
        states_[normalized].active = engine_value;
        InvalidateActiveCache();
        return;
    }
    if(state.update) state.update(state.active);
    if(inserted || previous != state.active) InvalidateActiveCache();
}

std::vector<std::string> UIPseudoStateSet::ActiveNames() const {
    RebuildActiveCache();
    return active_names_cache_;
}

const std::vector<std::string>& UIPseudoStateSet::ActiveNamesView() const {
    RebuildActiveCache();
    return active_names_cache_;
}

std::size_t UIPseudoStateSet::ActiveHash() const {
    RebuildActiveCache();
    return active_hash_cache_;
}


std::size_t NextStyleSheetRevision() {
    static std::size_t revision = 1;
    return revision++;
}

UIDirtyFlags UIPropertyDirtyFlags(PropertyType property) {
    UIDirtyFlags flags=UIDirtyFlags::Style|UIDirtyFlags::Paint;
    if(UIPropertyAffectsLayout(property)) flags|=UIDirtyFlags::Layout;
    switch(property) {
        case PropertyType::Filter:
        case PropertyType::BackdropFilter:
        case PropertyType::Effect:
        case PropertyType::MixBlendMode:
        case PropertyType::Transform:
            flags|=UIDirtyFlags::Composite;
            flags|=UIDirtyFlags::EffectParameters;
            break;
        case PropertyType::Content:
            flags|=UIDirtyFlags::PseudoElements;
            break;
        default:
            break;
    }
    return flags;
}

bool UIPropertyAffectsLayout(PropertyType property) {
    switch(property) {
        case PropertyType::BackgroundColor:
        case PropertyType::BorderColor:
        case PropertyType::BorderStyle:
        case PropertyType::Border:
        case PropertyType::BorderRadius:
        case PropertyType::TextColor:
        case PropertyType::TextAlign:
        case PropertyType::Opacity:
        case PropertyType::Content:
        case PropertyType::Filter:
        case PropertyType::BackdropFilter:
        case PropertyType::Effect:
        case PropertyType::MixBlendMode:
        case PropertyType::Transform:
        case PropertyType::InputStepButtons:
        case PropertyType::InputStep:
        case PropertyType::InputValueBox:
        case PropertyType::InputDragSpeed:
        case PropertyType::InputSliderShowValue:
        case PropertyType::InputSliderValueWidth:
        case PropertyType::InputSliderClickToSet:
        case PropertyType::RuntimeMovable:
        case PropertyType::RuntimeResizable:
        case PropertyType::RuntimeConstrain:
        case PropertyType::RuntimeLockX:
        case PropertyType::RuntimeLockY:
        case PropertyType::RuntimeSnap:
        case PropertyType::ScrollSpeed:
        case PropertyType::TooltipDelay:
        case PropertyType::Resizable:
        case PropertyType::Closable:
        case PropertyType::Collapsible:
        case PropertyType::ObjectFit:
        case PropertyType::ImageRendering:
        case PropertyType::ImageTint:
        case PropertyType::ImageFlipX:
        case PropertyType::ImageFlipY:
        case PropertyType::TabClosable:
        case PropertyType::ObjectPositionX:
        case PropertyType::ObjectPositionY:
        case PropertyType::ImageRotation:
        case PropertyType::ImagePivotX:
        case PropertyType::ImagePivotY:
        case PropertyType::ImageFrame:
        case PropertyType::ImageFrameWidth:
        case PropertyType::ImageFrameHeight:
        case PropertyType::ImageFrameColumns:
        case PropertyType::ImageSliceLeft:
        case PropertyType::ImageSliceRight:
        case PropertyType::ImageSliceTop:
        case PropertyType::ImageSliceBottom:
        case PropertyType::ImageSliceScale:
        case PropertyType::ImageSliceMode:
        case PropertyType::ImageTileScale:
        case PropertyType::ImageTileOffsetX:
        case PropertyType::ImageTileOffsetY:
        case PropertyType::ImageBlendMode:
        case PropertyType::ImagePixelSnap:
            return false;
        default:
            return true;
    }
}


std::uint32_t UIInternalPartMask(std::string_view part_class) {
    static constexpr std::array<std::string_view,24> parts = {
        CSS_INPUT_LABEL_PART,
        CSS_INPUT_VALUE_BOX_PART,
        CSS_INPUT_DECREMENT_BUTTON_PART,
        CSS_INPUT_INCREMENT_BUTTON_PART,
        CSS_INPUT_SLIDER_TRACK_PART,
        CSS_INPUT_SLIDER_FILL_PART,
        CSS_INPUT_SLIDER_HANDLE_PART,
        CSS_INPUT_COMPONENT_LABEL_PART,
        CSS_INPUT_COMPONENT_0_LABEL_PART,
        CSS_INPUT_COMPONENT_1_LABEL_PART,
        CSS_INPUT_COMPONENT_2_LABEL_PART,
        CSS_INPUT_COMPONENT_3_LABEL_PART,
        CSS_UI_HEADER_PART,
        CSS_UI_POPUP_PART,
        CSS_UI_ITEM_PART,
        CSS_UI_SCROLLBAR_PART,
        CSS_UI_SCROLLBAR_THUMB_PART,
        CSS_UI_RANGE_TRACK_PART,
        CSS_UI_RANGE_FILL_PART,
        CSS_UI_RANGE_HANDLE_PART,
        CSS_UI_TAB_PART,
        CSS_UI_TAB_CONTENT_PART,
        CSS_UI_SPLITTER_HANDLE_PART,
        ""
    };
    for(std::size_t index=0; index+1<parts.size(); ++index) {
        if(parts[index] == part_class) {
            return static_cast<std::uint32_t>(1u << index);
        }
    }
    return 0;
}

bool StyleSheetUsesPart(
    const StyleSheet& style_sheet,
    std::string_view part_class
) {
    const std::uint32_t mask = UIInternalPartMask(part_class);
    return mask != 0 && (style_sheet.used_internal_parts & mask) != 0;
}

bool StyleMayUseTargetPseudo(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& style
) {
    const std::size_t selector_index = static_cast<std::size_t>(selector);
    if(
        selector_index < 64 &&
        (style_sheet.target_pseudo_selector_mask & (1ULL << selector_index)) != 0
    ) {
        return true;
    }
    if(style_sheet.uses_unqualified_any_target_pseudo) return true;
    if(
        !std::holds_alternative<std::monostate>(style.id) &&
        style_sheet.target_pseudo_ids.find(HashCSSId(style.id)) !=
            style_sheet.target_pseudo_ids.end()
    ) {
        return true;
    }
    for(const std::string& class_name : style.classes) {
        if(
            style_sheet.target_pseudo_classes.find(class_name) !=
            style_sheet.target_pseudo_classes.end()
        ) {
            return true;
        }
    }
    return false;
}

bool StyleMayUseGeneratedPseudo(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& style,
    UIGeneratedPseudoKind kind
) {
    if(kind == UIGeneratedPseudoKind::None) return false;
    if(kind == UIGeneratedPseudoKind::Before && !style_sheet.uses_before_pseudo) return false;
    if(kind == UIGeneratedPseudoKind::After && !style_sheet.uses_after_pseudo) return false;

    const std::uint64_t mask = kind == UIGeneratedPseudoKind::Before
        ? style_sheet.before_pseudo_selector_mask
        : style_sheet.after_pseudo_selector_mask;
    const bool unqualified = kind == UIGeneratedPseudoKind::Before
        ? style_sheet.uses_unqualified_before_pseudo
        : style_sheet.uses_unqualified_after_pseudo;
    const auto& classes = kind == UIGeneratedPseudoKind::Before
        ? style_sheet.before_pseudo_classes
        : style_sheet.after_pseudo_classes;
    const auto& ids = kind == UIGeneratedPseudoKind::Before
        ? style_sheet.before_pseudo_ids
        : style_sheet.after_pseudo_ids;

    const std::size_t selector_index = static_cast<std::size_t>(selector);
    if(selector_index < 64 && (mask & (1ULL << selector_index)) != 0) return true;
    if(unqualified) return true;
    if(!std::holds_alternative<std::monostate>(style.id) &&
       ids.find(HashCSSId(style.id)) != ids.end()) return true;
    for(const std::string& class_name : style.classes) {
        if(classes.find(class_name) != classes.end()) return true;
    }
    return false;
}

bool StyleMayUseCompositing(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& style
) {
    if(!style_sheet.uses_compositing) return false;
    const std::size_t selector_index=static_cast<std::size_t>(selector);
    if(selector_index<64 &&
       (style_sheet.compositing_selector_mask&(1ULL<<selector_index))!=0) return true;
    if(style_sheet.uses_unqualified_compositing) return true;
    if(!std::holds_alternative<std::monostate>(style.id) &&
       style_sheet.compositing_ids.find(HashCSSId(style.id))!=style_sheet.compositing_ids.end()) {
        return true;
    }
    for(const std::string& class_name:style.classes) {
        if(style_sheet.compositing_classes.find(class_name)!=style_sheet.compositing_classes.end()) {
            return true;
        }
    }
    return false;
}

void TouchStyleSheet(StyleSheet& style_sheet) {
    style_sheet.uses_sibling_combinators = false;
    style_sheet.uses_structural_selectors = false;
    style_sheet.uses_layout_pseudo_states = false;
    style_sheet.uses_layout_animations = false;
    style_sheet.used_internal_parts = 0;
    style_sheet.target_pseudo_selector_mask = 0;
    style_sheet.uses_unqualified_any_target_pseudo = false;
    style_sheet.target_pseudo_classes.clear();
    style_sheet.target_pseudo_ids.clear();
    style_sheet.uses_before_pseudo = false;
    style_sheet.uses_after_pseudo = false;
    style_sheet.before_pseudo_selector_mask = 0;
    style_sheet.after_pseudo_selector_mask = 0;
    style_sheet.uses_unqualified_before_pseudo = false;
    style_sheet.uses_unqualified_after_pseudo = false;
    style_sheet.before_pseudo_classes.clear();
    style_sheet.after_pseudo_classes.clear();
    style_sheet.before_pseudo_ids.clear();
    style_sheet.after_pseudo_ids.clear();
    style_sheet.uses_compositing = false;
    style_sheet.uses_backdrop_filters = false;
    style_sheet.compositing_selector_mask = 0;
    style_sheet.uses_unqualified_compositing = false;
    style_sheet.compositing_classes.clear();
    style_sheet.compositing_ids.clear();
    for(auto& bucket : style_sheet.rules_by_selector) bucket.clear();
    style_sheet.wildcard_rules.clear();

    auto selector_uses_pseudo = [](const StyleRule& rule) {
        if(rule.state != SelectorState::None || !rule.pseudo_states.empty()) {
            return true;
        }
        return std::any_of(
            rule.ancestors.begin(),rule.ancestors.end(),
            [](const StyleSelector& selector) {
                return !selector.pseudo_states.empty();
            }
        );
    };

    for(std::size_t rule_index=0; rule_index<style_sheet.rules.size(); ++rule_index) {
        StyleRule& rule = style_sheet.rules[rule_index];
        rule.specificity = GetSelectorSpecificity(rule);
        if(!rule.structural.empty() || std::any_of(
            rule.ancestors.begin(),rule.ancestors.end(),
            [](const StyleSelector& selector) { return !selector.structural.empty(); }
        )) {
            style_sheet.uses_structural_selectors = true;
        }

        const std::size_t selector_index = static_cast<std::size_t>(rule.selector);
        if(rule.selector == SelectorType::Any) {
            style_sheet.wildcard_rules.push_back(rule_index);
            for(auto& bucket : style_sheet.rules_by_selector) bucket.push_back(rule_index);
        }
        else if(selector_index < style_sheet.rules_by_selector.size()) {
            style_sheet.rules_by_selector[selector_index].push_back(rule_index);
        }

        style_sheet.uses_before_pseudo = style_sheet.uses_before_pseudo ||
            rule.pseudo_element == UIGeneratedPseudoKind::Before;
        style_sheet.uses_after_pseudo = style_sheet.uses_after_pseudo ||
            rule.pseudo_element == UIGeneratedPseudoKind::After;
        if(rule.pseudo_element != UIGeneratedPseudoKind::None) {
            std::uint64_t& mask = rule.pseudo_element == UIGeneratedPseudoKind::Before
                ? style_sheet.before_pseudo_selector_mask : style_sheet.after_pseudo_selector_mask;
            bool& any = rule.pseudo_element == UIGeneratedPseudoKind::Before
                ? style_sheet.uses_unqualified_before_pseudo : style_sheet.uses_unqualified_after_pseudo;
            auto& classes = rule.pseudo_element == UIGeneratedPseudoKind::Before
                ? style_sheet.before_pseudo_classes : style_sheet.after_pseudo_classes;
            auto& ids = rule.pseudo_element == UIGeneratedPseudoKind::Before
                ? style_sheet.before_pseudo_ids : style_sheet.after_pseudo_ids;
            const std::size_t pseudo_selector_index = static_cast<std::size_t>(rule.selector);
            if(rule.selector != SelectorType::Any && pseudo_selector_index < 64) mask |= 1ULL<<pseudo_selector_index;
            else if(std::holds_alternative<std::monostate>(rule.id) && rule.classes.empty()) any = true;
            if(!std::holds_alternative<std::monostate>(rule.id)) ids.insert(HashCSSId(rule.id));
            for(const std::string& class_name:rule.classes) classes.insert(class_name);
        }
        bool rule_uses_compositing=false;
        for(const Declaration& declaration : rule.declarations) {
            const bool declaration_composites =
                declaration.property == PropertyType::Filter ||
                declaration.property == PropertyType::BackdropFilter ||
                declaration.property == PropertyType::Effect ||
                declaration.property == PropertyType::MixBlendMode ||
                declaration.property == PropertyType::Transform;
            // Generated pseudo-elements composite independently; do not make
            // their owner enter the subtree capture path.
            if(rule.pseudo_element == UIGeneratedPseudoKind::None && declaration_composites) {
                rule_uses_compositing=true;
                style_sheet.uses_compositing=true;
            }
            style_sheet.uses_backdrop_filters = style_sheet.uses_backdrop_filters ||
                declaration.property == PropertyType::BackdropFilter;
        }
        if(rule_uses_compositing) {
            const std::size_t target_index=static_cast<std::size_t>(rule.selector);
            const bool has_id=!std::holds_alternative<std::monostate>(rule.id);
            const bool has_classes=!rule.classes.empty();
            // Only a bare type selector needs the type-wide bit. Qualified
            // selectors are represented by their class/id set so `.glass`
            // does not force every Button through the effect preflight.
            if(rule.selector != SelectorType::Any && !has_id && !has_classes && target_index<64) {
                style_sheet.compositing_selector_mask |= 1ULL<<target_index;
            }
            else if(rule.selector == SelectorType::Any && !has_id && !has_classes) {
                style_sheet.uses_unqualified_compositing=true;
            }
            if(!std::holds_alternative<std::monostate>(rule.id)) {
                style_sheet.compositing_ids.insert(HashCSSId(rule.id));
            }
            for(const std::string& class_name:rule.classes) {
                style_sheet.compositing_classes.insert(class_name);
            }
        }

        auto register_part = [&](const std::string& class_name) {
            style_sheet.used_internal_parts |= UIInternalPartMask(class_name);
        };
        for(const std::string& class_name : rule.classes) register_part(class_name);
        for(const StyleSelector& ancestor : rule.ancestors) {
            for(const std::string& class_name : ancestor.classes) register_part(class_name);
        }

        const bool target_uses_pseudo =
            rule.state != SelectorState::None || !rule.pseudo_states.empty();
        if(target_uses_pseudo) {
            if(rule.selector != SelectorType::Any) {
                const std::size_t selector_index =
                    static_cast<std::size_t>(rule.selector);
                if(selector_index < 64) {
                    style_sheet.target_pseudo_selector_mask |=
                        (1ULL << selector_index);
                }
            }
            else if(
                std::holds_alternative<std::monostate>(rule.id) &&
                rule.classes.empty()
            ) {
                style_sheet.uses_unqualified_any_target_pseudo = true;
            }
            if(!std::holds_alternative<std::monostate>(rule.id)) {
                style_sheet.target_pseudo_ids.insert(HashCSSId(rule.id));
            }
            for(const std::string& class_name : rule.classes) {
                style_sheet.target_pseudo_classes.insert(class_name);
            }
        }

        style_sheet.uses_sibling_combinators =
            style_sheet.uses_sibling_combinators ||
            std::any_of(
                rule.combinators.begin(),rule.combinators.end(),
                [](SelectorCombinator combinator) {
                    return
                        combinator == SelectorCombinator::AdjacentSibling ||
                        combinator == SelectorCombinator::GeneralSibling;
                }
            );

        const bool layout_declaration =
            rule.pseudo_element == UIGeneratedPseudoKind::None &&
            std::any_of(
                rule.declarations.begin(),rule.declarations.end(),
                [](const Declaration& declaration) {
                    return UIPropertyAffectsLayout(declaration.property);
                }
            );

        const bool layout_keyframes = std::any_of(
            rule.animations.begin(),rule.animations.end(),
            [&](const UIAnimationSpec& animation) {
                return UIAnimationTouchesLayout(style_sheet,animation.name);
            }
        );

        const bool layout_transition = std::any_of(
            rule.transitions.begin(),rule.transitions.end(),
            [](const UITransitionSpec& transition) {
                return
                    transition.duration > 0.0f &&
                    (
                        transition.all ||
                        std::any_of(
                            transition.properties.begin(),
                            transition.properties.end(),
                            IsUILayoutProperty
                        )
                    );
            }
        );

        style_sheet.uses_layout_animations =
            style_sheet.uses_layout_animations ||
            layout_keyframes || layout_transition;

        if(selector_uses_pseudo(rule) &&
           (layout_declaration || layout_keyframes)) {
            style_sheet.uses_layout_pseudo_states = true;
        }
    }

    style_sheet.revision = NextStyleSheetRevision();
    ClearResolvedStyleCache();
}

// Default ("user agent") stylesheet. Values follow what a desktop browser does
// by default: black text on a white canvas, 16px root font, form controls at
// 13.333px with a 1px #767676 border on #efefef / white, block containers
// stacking their children with no gap, and no decoration on plain boxes.
StyleSheet MakeDefaultUIStyleSheet() {
    constexpr float kControlFont = 13.3333f;
    const Color black{0,0,0,255};
    const Color white{255,255,255,255};
    const Color control_border{118,118,118,255};   // #767676
    const Color button_face{239,239,239,255};      // #efefef
    const auto px = [](float value) { return CSSLength{value,LengthUnit::Pixels}; };
    const CSSLength auto_length{0.0f,LengthUnit::Auto};

    StyleSheet sheet;
    StyleRule body;
    body.selector = SelectorType::Body;
    body.declarations = {
        {PropertyType::Width,CSSLength{100.0f,LengthUnit::ViewWidth}},
        {PropertyType::Height,CSSLength{100.0f,LengthUnit::ViewHeight}},
        {PropertyType::DisplayType,DisplayType::Flex},
        {PropertyType::FlexDirection,FlexDirection::Column},
        {PropertyType::AlignItems,AlignItems::FlexStart},
        {PropertyType::Gap,px(0.0f)},
        {PropertyType::Padding,px(8.0f)},          // body { margin: 8px }
        {PropertyType::BackgroundColor,BLANK},
        {PropertyType::TextColor,white},
        {PropertyType::FontSize,px(16.0f)}
    };

    // div-like block box: fills the available width, height from content.
    StyleRule container;
    container.selector = SelectorType::Container;
    container.declarations = {
        {PropertyType::Width,CSSLength{100.0f,LengthUnit::Percent}},
        {PropertyType::Height,auto_length},
        {PropertyType::DisplayType,DisplayType::Flex},
        {PropertyType::FlexDirection,FlexDirection::Column},
        {PropertyType::AlignItems,AlignItems::Stretch},
        {PropertyType::Gap,px(0.0f)}
    };

    // <button>: padding 1px 6px, 1px solid #767676, #efefef, centered text.
    StyleRule button;
    button.selector = SelectorType::Button;
    button.declarations = {
        {PropertyType::Width,auto_length},
        {PropertyType::Height,auto_length},
        {PropertyType::DisplayType,DisplayType::Block},
        {PropertyType::BackgroundColor,button_face},
        {PropertyType::BorderColor,control_border},
        {PropertyType::BorderWidth,px(1.0f)},
        {PropertyType::TextColor,black},
        {PropertyType::FontSize,px(kControlFont)},
        {PropertyType::Padding,px(2.0f)},
        {PropertyType::TextAlign,TextAlign::Center}
    };

    // Plain text: inherits colour, font and size from its parent.
    StyleRule label;
    label.selector = SelectorType::Label;
    label.declarations = {
        {PropertyType::DisplayType,DisplayType::Inline}
    };

    // <input>: white field, 1px #767676 border, ~20 characters wide.
    StyleRule input = button;
    input.selector = SelectorType::Input;
    input.declarations = {
        {PropertyType::Width,px(173.0f)},
        {PropertyType::Height,px(21.0f)},
        {PropertyType::DisplayType,DisplayType::Block},
        {PropertyType::BackgroundColor,white},
        {PropertyType::BorderColor,control_border},
        {PropertyType::BorderWidth,px(1.0f)},
        {PropertyType::TextColor,black},
        {PropertyType::FontSize,px(kControlFont)},
        {PropertyType::Padding,px(2.0f)},
        {PropertyType::TextAlign,TextAlign::Left}
    };

    // No browser equivalent: a selectable row behaves like plain text.
    StyleRule selectable;
    selectable.selector = SelectorType::Selectable;
    selectable.declarations = {
        {PropertyType::Width,auto_length},
        {PropertyType::Height,auto_length},
        {PropertyType::DisplayType,DisplayType::Block},
        {PropertyType::BackgroundColor,Color{0,0,0,0}},
        {PropertyType::BorderWidth,px(0.0f)},
        {PropertyType::Padding,px(2.0f)},
        {PropertyType::TextAlign,TextAlign::Left}
    };

    // <select>
    StyleRule combo = input;
    combo.selector = SelectorType::ComboBox;
    combo.declarations.push_back({PropertyType::ItemHeight,px(20.0f)});
    combo.declarations.push_back({PropertyType::PopupWidth,px(173.0f)});

    // <input type=range>
    StyleRule range = input;
    range.selector = SelectorType::RangeInput;
    range.declarations[0] = {PropertyType::Width,px(129.0f)};
    range.declarations[1] = {PropertyType::Height,px(21.0f)};
    range.declarations[3] = {PropertyType::BackgroundColor,Color{0,0,0,0}};
    range.declarations[5] = {PropertyType::BorderWidth,px(0.0f)};

    // <canvas>: 300x150, transparent, undecorated.
    StyleRule canvas;
    canvas.selector = SelectorType::Canvas;
    canvas.declarations = {
        {PropertyType::Width,px(300.0f)},
        {PropertyType::Height,px(150.0f)},
        {PropertyType::DisplayType,DisplayType::Inline},
        {PropertyType::BackgroundColor,Color{0,0,0,0}},
        {PropertyType::BorderWidth,px(0.0f)}
    };

    // <img>: object-fit: fill, undecorated.
    StyleRule image = canvas;
    image.selector = SelectorType::Image;
    image.declarations.push_back({PropertyType::ObjectFit,UIImageFit::Stretch});

    // <hr>-like divider.
    StyleRule splitter;
    splitter.selector = SelectorType::Splitter;
    splitter.declarations = {
        {PropertyType::Width,px(6.0f)},
        {PropertyType::Height,CSSLength{100.0f,LengthUnit::Percent}},
        {PropertyType::BackgroundColor,Color{229,229,229,255}},
        {PropertyType::SplitterSize,px(6.0f)}
    };

    StyleRule tabs = container;
    tabs.selector = SelectorType::TabBar;
    tabs.declarations.push_back({PropertyType::TabHeight,px(28.0f)});

    // <details> / <summary>: undecorated block with a header row.
    auto composite_rule = [&](SelectorType selector) {
        StyleRule rule = container;
        rule.selector = selector;
        rule.declarations.push_back({PropertyType::HeaderHeight,px(24.0f)});
        return rule;
    };
    // <dialog>-like floating surfaces: white, thin border, black text.
    auto surface_rule = [&](SelectorType selector) {
        StyleRule rule = composite_rule(selector);
        rule.declarations.push_back({PropertyType::BackgroundColor,white});
        rule.declarations.push_back({PropertyType::BorderColor,control_border});
        rule.declarations.push_back({PropertyType::BorderWidth,px(1.0f)});
        rule.declarations.push_back({PropertyType::TextColor,black});
        return rule;
    };

    StyleRule collapsible = composite_rule(SelectorType::Collapsible);
    StyleRule tree = composite_rule(SelectorType::TreeNode);
    StyleRule menu = button;
    menu.selector = SelectorType::Menu;
    menu.declarations.push_back({PropertyType::HeaderHeight,px(24.0f)});
    menu.declarations.push_back({PropertyType::PopupWidth,px(220.0f)});
    StyleRule popup = surface_rule(SelectorType::Popup);
    StyleRule window = surface_rule(SelectorType::Window);

    // Native title-attribute tooltip look.
    StyleRule tooltip;
    tooltip.selector = SelectorType::Tooltip;
    tooltip.declarations = {
        {PropertyType::BackgroundColor,Color{255,255,225,255}},
        {PropertyType::BorderColor,control_border},
        {PropertyType::BorderWidth,px(1.0f)},
        {PropertyType::TextColor,black},
        {PropertyType::FontSize,px(kControlFont)},
        {PropertyType::Padding,px(3.0f)}
    };

    sheet.rules = {
        std::move(body),
        std::move(container),
        std::move(button),
        std::move(label),
        std::move(input),
        std::move(selectable),
        std::move(combo),
        std::move(range),
        std::move(canvas),
        std::move(image),
        std::move(splitter),
        std::move(tabs),
        std::move(collapsible),
        std::move(tree),
        std::move(menu),
        std::move(popup),
        std::move(window),
        std::move(tooltip)
    };
    TouchStyleSheet(sheet);
    return sheet;
}

void SetUIFontRegistry(const std::unordered_map<std::string,Font>& fonts) {
    CSS_UI_ACTIVE_FONTS = &fonts;
}

Font ResolveUIFont(const std::string& name) {
    auto trim = [](std::string value) {
        std::size_t first = 0;
        std::size_t last = value.size();
        while(first < last && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
        while(last > first && std::isspace(static_cast<unsigned char>(value[last-1]))) --last;
        value = value.substr(first,last-first);
        if(value.size() >= 2 &&
           ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1,value.size()-2);
        }
        return value;
    };

    std::size_t cursor = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;
    for(std::size_t index=0; index<=name.size(); ++index) {
        const char character = index < name.size() ? name[index] : ',';
        if(character == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if(character == '"' && !in_single_quote) in_double_quote = !in_double_quote;

        if(character != ',' || in_single_quote || in_double_quote) continue;

        std::string candidate = trim(name.substr(cursor,index-cursor));
        cursor = index+1;
        if(candidate.empty()) continue;

        if(CSS_UI_ACTIVE_FONTS != nullptr) {
            auto found = CSS_UI_ACTIVE_FONTS->find(candidate);
            if(found != CSS_UI_ACTIVE_FONTS->end() && found->second.texture.id != 0) {
                return found->second;
            }
        }

        std::string generic = candidate;
        std::transform(generic.begin(),generic.end(),generic.begin(),[](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if(generic == "default" || generic == "system-ui" ||
           generic == "sans-serif" || generic == "serif" ||
           generic == "monospace") {
            return GetFontDefault();
        }
    }

    return GetFontDefault();
}

std::vector<int> DefaultUICodepoints() {
    std::vector<int> cps;
    auto range = [&](int a,int b){ for(int c=a;c<=b;++c) cps.push_back(c); };
    range(32,126);          // Basic Latin
    range(160,255);         // Latin-1 Supplement (accents)
    range(256,383);         // Latin Extended-A
    range(0x2010,0x2027);   // dashes, quotes, bullets, ellipsis
    range(0x2030,0x203A);
    cps.push_back(0x20AC);  // euro
    cps.push_back(0x2122);  // trademark
    range(0x2190,0x2199);   // arrows
    range(0x2212,0x2212);   // minus
    range(0x25A0,0x25CF);   // geometric shapes
    range(0x2713,0x2717);   // check / cross marks
    cps.push_back(0xFFFD);
    return cps;
}

bool AddUIFont(
    std::unordered_map<std::string,Font>& fonts,
    const std::string& name,
    const std::string& path,
    int base_size,
    const std::vector<int>& codepoints ) {
    if(name.empty() || path.empty()) return false;

    std::vector<int> cps = codepoints.empty() ? DefaultUICodepoints() : codepoints;
    Font loaded = LoadFontEx(path.c_str(),base_size,cps.data(),static_cast<int>(cps.size()));
    if(loaded.texture.id == 0) return false;

    auto existing = fonts.find(name);
    if(existing != fonts.end() && existing->second.texture.id != 0) {
        UnloadFont(existing->second);
    }

    fonts[name] = loaded;
    SetTextureFilter(fonts[name].texture,TEXTURE_FILTER_BILINEAR);
    SetUIFontRegistry(fonts);
    ClearTextMeasureCache();
    return true;
}

UIFontVerticalExtent MeasureFontVerticalExtent(Font font,float font_size) {
    UIFontVerticalExtent fallback;
    fallback.top = 0.0f;
    fallback.bottom = font_size;
    if(font.texture.id == 0 || font.glyphs == nullptr || font.glyphCount <= 0 || font.baseSize <= 0) {
        return fallback;
    }

    struct Key {
        unsigned int texture;
        const void* glyphs;
        int base_size;
        bool operator==(const Key& o) const {
            return texture == o.texture && glyphs == o.glyphs && base_size == o.base_size;
        }
    };
    struct Cached { Key key; float top; float bottom; };
    static std::vector<Cached> cache;
    const Key key{font.texture.id,static_cast<const void*>(font.glyphs),font.baseSize};
    float top_unit = 0.0f;
    float bottom_unit = 0.0f;
    bool found = false;
    for(const Cached& entry : cache) {
        if(entry.key == key) { top_unit = entry.top; bottom_unit = entry.bottom; found = true; break; }
    }
    if(!found) {
        // Ink extents are measured from real glyph bitmaps: ascender/cap top
        // from "Hlbdf", descender bottom from "gjpqy" (offsetY is relative to
        // the draw origin, image.height is the bitmap height in base-size px).
        float top = 1e9f;
        float bottom = -1e9f;
        const auto probe = [&](const char* letters,bool use_top) {
            for(const char* p = letters; *p; ++p) {
                const int index = GetGlyphIndex(font,static_cast<int>(*p));
                if(index < 0 || index >= font.glyphCount) continue;
                const GlyphInfo& glyph = font.glyphs[index];
                if(glyph.image.height <= 0) continue;
                const float glyph_top = static_cast<float>(glyph.offsetY);
                const float glyph_bottom = glyph_top+static_cast<float>(glyph.image.height);
                if(use_top) top = std::min(top,glyph_top);
                else bottom = std::max(bottom,glyph_bottom);
            }
        };
        probe("Hlbdf",true);
        probe("gjpqy",false);
        if(top > bottom || top > 1e8f || bottom < -1e8f) {
            top_unit = 0.0f;
            bottom_unit = 1.0f;
        }
        else {
            top_unit = top/static_cast<float>(font.baseSize);
            bottom_unit = bottom/static_cast<float>(font.baseSize);
        }
        cache.push_back({key,top_unit,bottom_unit});
    }
    UIFontVerticalExtent result;
    result.top = top_unit*font_size;
    result.bottom = bottom_unit*font_size;
    return result;
}

void UnloadUIFonts(std::unordered_map<std::string,Font>& fonts) {
    for(auto& [name,font] : fonts) {
        (void)name;
        if(font.texture.id != 0) UnloadFont(font);
    }
    fonts.clear();
    if(CSS_UI_ACTIVE_FONTS == &fonts) CSS_UI_ACTIVE_FONTS = nullptr;
    ClearTextMeasureCache();
}

void HashCombine(std::size_t& seed, std::size_t value) {
    seed ^= value + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
        (seed << 6) + (seed >> 2);
}

std::size_t HashFloat(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits,&value,sizeof(value));
    return std::hash<std::uint32_t>{}(bits);
}

std::size_t HashColor(Color color) {
    std::size_t seed = color.r;
    HashCombine(seed,color.g);
    HashCombine(seed,color.b);
    HashCombine(seed,color.a);
    return seed;
}

std::size_t HashCSSLength(const CSSLength& length) {
    std::size_t seed = HashFloat(length.value);
    HashCombine(seed,static_cast<std::size_t>(length.unit));
    if(length.expression_ast) {
        HashCombine(seed,reinterpret_cast<std::size_t>(length.expression_ast.get()));
    }
    else {
        HashCombine(seed,std::hash<std::string>{}(length.expression));
    }
    return seed;
}

std::size_t HashCSSId(const CSS_Id& id) {
    std::size_t seed = id.index();

    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,int>) {
                HashCombine(seed,std::hash<int>{}(value));
            }
            else if constexpr(std::is_same_v<T,std::string>) {
                HashCombine(seed,std::hash<std::string>{}(value));
            }
        },
        id
    );

    return seed;
}

std::size_t HashStyleSelector(const StyleSelector& selector) {
    std::size_t seed = static_cast<std::size_t>(selector.selector);
    HashCombine(seed,HashCSSId(selector.id));
    HashCombine(seed,selector.classes.size());
    for(const std::string& class_name : selector.classes) {
        HashCombine(seed,std::hash<std::string>{}(class_name));
    }
    HashCombine(seed,selector.pseudo_states.size());
    for(const std::string& state : selector.pseudo_states) {
        HashCombine(seed,std::hash<std::string>{}(state));
    }
    HashCombine(seed,selector.sibling_index);
    HashCombine(seed,selector.sibling_count);
    return seed;
}

bool StructuralPseudosMatch(
    const std::vector<UIStructuralPseudo>& pseudos,
    std::size_t index,
    std::size_t count
) {
    if(pseudos.empty()) return true;
    if(index == 0 || count == 0 || index > count) return false;
    for(const UIStructuralPseudo& pseudo : pseudos) {
        const long position = pseudo.from_end
            ? static_cast<long>(count-index+1)
            : static_cast<long>(index);
        const long difference = position-static_cast<long>(pseudo.b);
        bool ok;
        if(pseudo.a == 0) ok = difference == 0;
        else ok = difference % pseudo.a == 0 && difference / pseudo.a >= 0;
        if(!ok) return false;
    }
    return true;
}

StyleSelectorSiblingChain AppendStyleSelectorSibling(
    StyleSelectorSiblingChain previous,
    StyleSelector selector
) {
    auto node = std::make_shared<StyleSelectorSiblingNode>();
    node->selector = std::move(selector);
    node->previous = std::move(previous);
    node->count = node->previous ? node->previous->count+1 : 1;
    node->cumulative_hash =
        node->previous ? node->previous->cumulative_hash : 0;
    HashCombine(node->cumulative_hash,HashStyleSelector(node->selector));
    return node;
}

std::size_t HashComputedStyle(const ComputedStyle& style) {
    std::size_t seed = 0;

    HashCombine(seed,HashColor(style.background_color));
    HashCombine(seed,HashColor(style.border_color));
    HashCombine(seed,HashColor(style.text_color));
    HashCombine(seed,HashFloat(style.border_width));
    HashCombine(seed,HashFloat(style.border_widths.left));
    HashCombine(seed,HashFloat(style.border_widths.right));
    HashCombine(seed,HashFloat(style.border_widths.top));
    HashCombine(seed,HashFloat(style.border_widths.bottom));
    HashCombine(seed,static_cast<std::size_t>(style.border_style));
    HashCombine(seed,HashFloat(style.border_radius));
    HashCombine(seed,HashFloat(style.scale));
    HashCombine(seed,HashFloat(style.width));
    HashCombine(seed,HashFloat(style.height));
    HashCombine(seed,HashFloat(style.min_width));
    HashCombine(seed,HashFloat(style.max_width));
    HashCombine(seed,HashFloat(style.min_height));
    HashCombine(seed,HashFloat(style.max_height));
    HashCombine(seed,HashFloat(style.font_size));
    HashCombine(seed,HashFloat(style.gap));
    HashCombine(seed,HashFloat(style.row_gap));
    HashCombine(seed,HashFloat(style.column_gap));
    HashCombine(seed,HashFloat(style.opacity));
    HashCombine(seed,HashFloat(style.scroll_speed));
    HashCombine(seed,HashFloat(style.scrollbar_size));
    HashCombine(seed,HashFloat(style.header_height));
    HashCombine(seed,HashFloat(style.item_height));
    HashCombine(seed,HashFloat(style.popup_width));
    HashCombine(seed,HashFloat(style.popup_max_height));
    HashCombine(seed,HashFloat(style.tooltip_delay));
    HashCombine(seed,HashFloat(style.indent));
    HashCombine(seed,HashFloat(style.splitter_size));
    HashCombine(seed,HashFloat(style.tab_height));
    HashCombine(seed,HashColor(style.image_tint));
    HashCombine(seed,static_cast<std::size_t>(style.image_fit));
    HashCombine(seed,static_cast<std::size_t>(style.image_sampling));
    HashCombine(seed,style.image_flip_x ? 1u : 0u);
    HashCombine(seed,style.image_flip_y ? 1u : 0u);
    HashCombine(seed,HashFloat(style.image_position.x));
    HashCombine(seed,HashFloat(style.image_position.y));
    HashCombine(seed,HashFloat(style.image_rotation));
    HashCombine(seed,HashFloat(style.image_pivot.x));
    HashCombine(seed,HashFloat(style.image_pivot.y));
    HashCombine(seed,static_cast<std::size_t>(style.image_frame));
    HashCombine(seed,HashFloat(style.image_frame_size.x));
    HashCombine(seed,HashFloat(style.image_frame_size.y));
    HashCombine(seed,static_cast<std::size_t>(style.image_frame_columns));
    HashCombine(seed,HashFloat(style.image_slice.left));
    HashCombine(seed,HashFloat(style.image_slice.right));
    HashCombine(seed,HashFloat(style.image_slice.top));
    HashCombine(seed,HashFloat(style.image_slice.bottom));
    HashCombine(seed,HashFloat(style.image_slice_scale));
    HashCombine(seed,static_cast<std::size_t>(style.image_slice_mode));
    HashCombine(seed,HashFloat(style.image_tile_scale));
    HashCombine(seed,HashFloat(style.image_tile_offset.x));
    HashCombine(seed,HashFloat(style.image_tile_offset.y));
    HashCombine(seed,static_cast<std::size_t>(style.image_blend_mode));
    HashCombine(seed,style.image_pixel_snap ? 1u : 0u);
    HashCombine(seed,std::hash<std::string>{}(style.generated_content));
    HashCombine(seed,style.generated_content_defined ? 1u : 0u);
    HashCombine(seed,reinterpret_cast<std::size_t>(style.filters.get()));
    HashCombine(seed,reinterpret_cast<std::size_t>(style.backdrop_filters.get()));
    HashCombine(seed,reinterpret_cast<std::size_t>(style.shader_effects.get()));
    HashCombine(seed,reinterpret_cast<std::size_t>(style.shader_uniforms.get()));
    HashCombine(seed,static_cast<std::size_t>(style.mix_blend_mode));
    HashCombine(seed,reinterpret_cast<std::size_t>(style.transform_operations.get()));
    // Custom-property maps are immutable and structurally shared. Pointer
    // identity is therefore a sufficient cache key and avoids O(variable-count)
    // hashing for every style-cache lookup.
    HashCombine(
        seed,reinterpret_cast<std::size_t>(style.inherited_custom_properties.get())
    );
    HashCombine(seed,std::hash<std::string>{}(style.inherited_font));
    HashCombine(seed,style.inherited_text_color ? HashColor(*style.inherited_text_color) : 0);
    HashCombine(seed,style.sibling_index);
    HashCombine(seed,style.sibling_count);
    HashCombine(
        seed,reinterpret_cast<std::size_t>(style.component_style_properties.get())
    );

    auto hash_length = [](const CSSLength& value) {
        return HashCSSLength(value);
    };

    HashOptional(seed,style.border_width_length,hash_length);
    HashOptional(seed,style.border_width_lengths.left,hash_length);
    HashOptional(seed,style.border_width_lengths.right,hash_length);
    HashOptional(seed,style.border_width_lengths.top,hash_length);
    HashOptional(seed,style.border_width_lengths.bottom,hash_length);
    HashOptional(seed,style.width_length,hash_length);
    HashOptional(seed,style.height_length,hash_length);
    HashOptional(seed,style.min_width_length,hash_length);
    HashOptional(seed,style.max_width_length,hash_length);
    HashOptional(seed,style.min_height_length,hash_length);
    HashOptional(seed,style.max_height_length,hash_length);
    HashOptional(seed,style.font_size_length,hash_length);
    HashOptional(seed,style.gap_length,hash_length);
    HashOptional(seed,style.row_gap_length,hash_length);
    HashOptional(seed,style.column_gap_length,hash_length);
    HashOptional(seed,style.scrollbar_size_length,hash_length);
    HashOptional(seed,style.header_height_length,hash_length);
    HashOptional(seed,style.item_height_length,hash_length);
    HashOptional(seed,style.popup_width_length,hash_length);
    HashOptional(seed,style.popup_max_height_length,hash_length);
    HashOptional(seed,style.indent_length,hash_length);
    HashOptional(seed,style.splitter_size_length,hash_length);
    HashOptional(seed,style.tab_height_length,hash_length);

    HashCombine(seed,static_cast<std::size_t>(style.position));
    HashCombine(seed,HashFloat(style.left));
    HashCombine(seed,HashFloat(style.right));
    HashCombine(seed,HashFloat(style.top));
    HashCombine(seed,HashFloat(style.bottom));

    HashOptional(seed,style.left_length,hash_length);
    HashOptional(seed,style.right_length,hash_length);
    HashOptional(seed,style.top_length,hash_length);
    HashOptional(seed,style.bottom_length,hash_length);

    HashCombine(seed,std::hash<std::string>{}(style.font));
    HashCombine(seed,static_cast<std::size_t>(style.flex_direction));
    HashCombine(seed,static_cast<std::size_t>(style.flex_wrap));
    HashCombine(seed,static_cast<std::size_t>(style.justify_content));
    HashCombine(seed,static_cast<std::size_t>(style.align_items));
    HashCombine(seed,static_cast<std::size_t>(style.justify_items));
    HashCombine(seed,static_cast<std::size_t>(style.align_self));
    HashCombine(seed,style.align_content.has_value() ? 1u : 0u);
    if(style.align_content.has_value()) {
        HashCombine(seed,static_cast<std::size_t>(*style.align_content));
    }
    HashCombine(seed,static_cast<std::size_t>(style.justify_self));
    HashCombine(seed,HashFloat(style.flex_grow));
    HashCombine(seed,HashFloat(style.flex_shrink));
    HashCombine(seed,HashFloat(style.flex_basis));
    HashOptional(seed,style.flex_basis_length,hash_length);

    HashCombine(seed,static_cast<std::size_t>(style.text_align));
    HashCombine(seed,static_cast<std::size_t>(style.overflow));
    HashCombine(seed,std::hash<int>{}(style.grid_template_columns));
    HashCombine(seed,std::hash<int>{}(style.grid_template_rows));
    auto hash_track = [&](const GridTrackSize& track) {
        HashCombine(seed,HashFloat(track.value));
        HashCombine(seed,static_cast<std::size_t>(track.unit));
    };
    for(const auto& track : style.grid_column_tracks) hash_track(track);
    for(const auto& track : style.grid_row_tracks) hash_track(track);
    hash_track(style.grid_auto_column);
    hash_track(style.grid_auto_row);
    HashCombine(seed,static_cast<std::size_t>(style.grid_auto_flow));
    HashCombine(seed,std::hash<int>{}(style.grid_column));
    HashCombine(seed,std::hash<int>{}(style.grid_row));
    HashCombine(seed,std::hash<int>{}(style.grid_column_span));
    HashCombine(seed,std::hash<int>{}(style.grid_row_span));

    auto hash_bool = [](bool value) {
        return std::hash<bool>{}(value);
    };
    auto hash_number = [](float value) {
        return HashFloat(value);
    };

    HashOptional(seed,style.input_step_buttons,hash_bool);
    HashOptional(seed,style.input_step,hash_number);
    HashOptional(seed,style.input_value_box,hash_bool);
    HashOptional(seed,style.input_drag_speed,hash_number);
    HashOptional(seed,style.input_slider_show_value,hash_bool);
    HashOptional(seed,style.input_slider_value_width,hash_number);
    HashOptional(seed,style.input_slider_click_to_set,hash_bool);
    HashOptional(seed,style.runtime_movable,hash_bool);
    HashOptional(seed,style.runtime_resizable,hash_bool);
    HashOptional(seed,style.runtime_constrain,hash_bool);
    HashOptional(seed,style.runtime_lock_x,hash_bool);
    HashOptional(seed,style.runtime_lock_y,hash_bool);
    HashOptional(seed,style.runtime_snap,hash_number);
    HashOptional(seed,style.resize_to_content,hash_bool);
    HashOptional(seed,style.resizable,hash_bool);
    HashOptional(seed,style.closable,hash_bool);
    HashOptional(seed,style.collapsible,hash_bool);
    HashOptional(seed,style.tab_closable,hash_bool);

    HashCombine(seed,HashCSSId(style.id));

    for(const std::string& class_name : style.classes) {
        HashCombine(seed,std::hash<std::string>{}(class_name));
    }

    HashCombine(seed,static_cast<std::size_t>(style.selector));

    HashCombine(seed,style.selector_ancestors.size());
    for(const StyleSelector& ancestor : style.selector_ancestors) {
        HashCombine(seed,HashStyleSelector(ancestor));
    }
    HashCombine(seed,style.selector_ancestor_previous_siblings.size());
    for(const StyleSelectorSiblingChain& siblings :
        style.selector_ancestor_previous_siblings) {
        HashCombine(seed,siblings ? siblings->count : 0);
        HashCombine(seed,siblings ? siblings->cumulative_hash : 0);
    }
    HashCombine(
        seed,
        style.selector_previous_siblings
            ? style.selector_previous_siblings->count
            : 0
    );
    HashCombine(
        seed,
        style.selector_previous_siblings
            ? style.selector_previous_siblings->cumulative_hash
            : 0
    );

    HashCombine(seed,static_cast<std::size_t>(style.display));

    auto hash_edges = [&](const EdgeValues& edges) {
        HashCombine(seed,HashFloat(edges.left));
        HashCombine(seed,HashFloat(edges.right));
        HashCombine(seed,HashFloat(edges.top));
        HashCombine(seed,HashFloat(edges.bottom));
    };

    hash_edges(style.padding);
    hash_edges(style.margin);

    HashOptional(seed,style.padding_lengths.left,hash_length);
    HashOptional(seed,style.padding_lengths.right,hash_length);
    HashOptional(seed,style.padding_lengths.top,hash_length);
    HashOptional(seed,style.padding_lengths.bottom,hash_length);
    HashOptional(seed,style.margin_lengths.left,hash_length);
    HashOptional(seed,style.margin_lengths.right,hash_length);
    HashOptional(seed,style.margin_lengths.top,hash_length);
    HashOptional(seed,style.margin_lengths.bottom,hash_length);

    return seed;
}

void ClearResolvedStyleCache() {
    CSS_UI_RESOLVED_STYLE_CACHE.clear();
}

std::size_t HashTextMeasureKey(
    Font font,
    std::string_view text,
    float font_size,
    float spacing
) {
    std::size_t seed = std::hash<unsigned int>{}(font.texture.id);
    HashCombine(seed,std::hash<int>{}(font.baseSize));
    HashCombine(seed,HashFloat(font_size));
    HashCombine(seed,HashFloat(spacing));
    HashCombine(seed,text.size());
    HashCombine(seed,std::hash<std::string_view>{}(text));
    return seed;
}

void ClearTextMeasureCache() {
    CSS_UI_TEXT_MEASURE_CACHE.clear();
}

Vector2 MeasureTextCached(
    Font font,
    const char* text,
    float font_size,
    float spacing
) {
    std::string_view view = text != nullptr
        ? std::string_view(text)
        : std::string_view{};

    const std::size_t key =
        HashTextMeasureKey(font,view,font_size,spacing);

    auto range = CSS_UI_TEXT_MEASURE_CACHE.equal_range(key);

    for(auto iterator = range.first; iterator != range.second; ++iterator) {
        const TextMeasureCacheEntry& entry = iterator->second;

        if(
            entry.texture_id == font.texture.id &&
            entry.base_size == font.baseSize &&
            entry.font_size == font_size &&
            entry.spacing == spacing &&
            entry.text == view
        ) {
            return entry.measured;
        }
    }

    Vector2 measured = MeasureTextEx(
        font,
        view.empty() ? "" : view.data(),
        font_size,
        spacing
    );

    if(CSS_UI_TEXT_MEASURE_CACHE.size() >= 4096) {
        CSS_UI_TEXT_MEASURE_CACHE.clear();
    }

    CSS_UI_TEXT_MEASURE_CACHE.emplace(
        key,
        TextMeasureCacheEntry{
            font.texture.id,
            font.baseSize,
            font_size,
            spacing,
            std::string(view),
            measured
        }
    );

    return measured;
}

UIContainer& UIContainer::Add(UIContainer child) {
    auto value = std::make_shared<UIContainer>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UICollapsible& UIContainer::Add(UICollapsible child) {
    auto value = std::make_shared<UICollapsible>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UITreeNode& UIContainer::Add(UITreeNode child) {
    auto value = std::make_shared<UITreeNode>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIMenu& UIContainer::Add(UIMenu child) {
    auto value = std::make_shared<UIMenu>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIPopup& UIContainer::Add(UIPopup child) {
    auto value = std::make_shared<UIPopup>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UIWindow& UIContainer::Add(UIWindow child) {
    auto value = std::make_shared<UIWindow>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

UITabBar& UIContainer::Add(UITabBar child) {
    auto value = std::make_shared<UITabBar>(std::move(child));
    elements.emplace_back(value);
    layout_dirty = true;
    return *value;
}

void OpenPopupAt(UIPopup& popup, Vector2 position) {
    popup.open = true;
    popup.opened = true;
    popup.closed = false;
    popup.style.position = PositionType::Absolute;
    popup.style.left = position.x;
    popup.style.top = position.y;
    popup.style.left_length = CSSLength{position.x,LengthUnit::Pixels};
    popup.style.top_length = CSSLength{position.y,LengthUnit::Pixels};
    popup.rect.runtime_offset = {};
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

void ClosePopup(UIPopup& popup) {
    popup.closed = popup.open;
    popup.open = false;
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

void OpenContextMenu(UIPopup& popup) {
    popup.modal = false;
    popup.close_on_outside_click = true;
    OpenPopupAt(popup,GetUIMousePosition());
}

void OpenModal(UIPopup& popup) {
    popup.modal = true;
    popup.open = true;
    popup.opened = true;
    popup.closed = false;
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
}

std::size_t HashRectangle(Rectangle rectangle) {
    std::size_t seed = HashFloat(rectangle.x);
    HashCombine(seed,HashFloat(rectangle.y));
    HashCombine(seed,HashFloat(rectangle.width));
    HashCombine(seed,HashFloat(rectangle.height));
    return seed;
}

std::string CSSRuntimeId(const CSS_Id& id) {
    if(const auto* text = std::get_if<std::string>(&id)) return "#" + *text;
    if(const auto* number = std::get_if<int>(&id)) return "#" + std::to_string(*number);
    return {};
}

const char* RuntimeSelectorName(SelectorType selector) {
    switch(selector) {
        case SelectorType::Button: return "button";
        case SelectorType::Label: return "label";
        case SelectorType::Input: return "input";
        case SelectorType::Container: return "container";
        case SelectorType::Selectable: return "selectable";
        case SelectorType::ComboBox: return "combo";
        case SelectorType::RangeInput: return "range";
        case SelectorType::Canvas: return "canvas";
        case SelectorType::Image: return "image";
        case SelectorType::Splitter: return "splitter";
        case SelectorType::TabBar: return "tabs";
        case SelectorType::Collapsible: return "collapsible";
        case SelectorType::TreeNode: return "tree-node";
        case SelectorType::Menu: return "menu";
        case SelectorType::Popup: return "popup";
        case SelectorType::Window: return "window";
        case SelectorType::TextBox: return "textbox";
        case SelectorType::Table: return "table";
        case SelectorType::VirtualList: return "virtual-list";
        case SelectorType::DockSpace: return "dock-space";
        case SelectorType::Viewport: return "viewport";
        case SelectorType::ViewportItem: return "viewport-item";
        default: return "element";
    }
}
