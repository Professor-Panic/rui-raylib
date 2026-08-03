#include "ui_markup.h"
#include "css_parser.h"

#include <cassert>
#include <cstdlib>
#include <cmath>
#include <iostream>

#ifndef RUI_REQUIRE
#define RUI_REQUIRE(...) do { \
    if(!(__VA_ARGS__)) { \
        std::cerr << "Requirement failed: " #__VA_ARGS__ \
                  << " at " << __FILE__ << ':' << __LINE__ << '\n'; \
        std::abort(); \
    } \
} while(false)
#endif

namespace {

bool SameColor(Color first, Color second) {
    return first.r == second.r && first.g == second.g &&
        first.b == second.b && first.a == second.a;
}

const StyleRule* FindRule(
    const StyleSheet& sheet,
    const std::vector<std::string>& states
) {
    for(const StyleRule& rule : sheet.rules) {
        if(rule.pseudo_states == states) return &rule;
    }
    return nullptr;
}

} // namespace

int main() {
    const std::string rcss = R"RCSS(
.item {
    background-color: #101010;
    opacity: 0.5;
    transition: background-color 250ms ease-out, opacity 0.5s linear;
}

.item:selected {
    background-color: #5020A0;
}

.item:selected:hover {
    opacity: 1;
    animation: pulse 800ms ease-in-out 2 alternate both;
}

@keyframes pulse {
    from { border-color: #000000; opacity: 0.25; }
    50% { border-color: #8040FF; opacity: 0.75; }
    to { border-color: #FFFFFF; opacity: 1; }
}
)RCSS";

    StyleSheet sheet = ParseStyleSheet(rcss);
    RUI_REQUIRE(sheet.keyframes.count("pulse") == 1);
    RUI_REQUIRE(sheet.keyframes.at("pulse").steps.size() == 3);

    const StyleRule* selected = FindRule(sheet,{"selected"});
    const StyleRule* selected_hover = FindRule(sheet,{"selected","hover"});
    RUI_REQUIRE(selected != nullptr);
    RUI_REQUIRE(selected_hover != nullptr);
    RUI_REQUIRE(selected_hover->animations_declared);
    RUI_REQUIRE(selected_hover->animations.size() == 1);
    RUI_REQUIRE(selected_hover->animations.front().name == "pulse");
    RUI_REQUIRE(std::abs(selected_hover->animations.front().duration - 0.8f) < 0.0001f);
    RUI_REQUIRE(selected_hover->animations.front().iterations == 2.0f);
    RUI_REQUIRE(selected_hover->animations.front().direction == UIAnimationDirection::Alternate);
    RUI_REQUIRE(selected_hover->animations.front().fill_mode == UIAnimationFillMode::Both);

    ComputedStyle base;
    base.classes.push_back("item");
    ComputedStyle normal = ResolveStyleWithPseudoStates(
        sheet,SelectorType::Button,{}, {},base
    );
    RUI_REQUIRE(SameColor(normal.background_color,Color{16,16,16,255}));
    RUI_REQUIRE(normal.transitions.size() == 2);

    ComputedStyle selected_style = ResolveStyleWithPseudoStates(
        sheet,SelectorType::Button,{}, {"selected"},base
    );
    RUI_REQUIRE(SameColor(selected_style.background_color,Color{80,32,160,255}));

    ComputedStyle compound = ResolveStyleWithPseudoStates(
        sheet,SelectorType::Button,{}, {"selected","hover"},base
    );
    RUI_REQUIRE(std::abs(compound.opacity - 1.0f) < 0.0001f);
    RUI_REQUIRE(compound.animations.size() == 1);

    bool selected_value = false;
    UIPseudoStateSet states;
    states.Define("selected",false,[&](bool& active) {
        active = selected_value;
    });
    states.Update();
    RUI_REQUIRE(!states.Active("selected"));
    selected_value = true;
    states.Update();
    RUI_REQUIRE(states.Active("selected"));
    states.Set("hover",true);
    RUI_REQUIRE(states.ActiveNames() == std::vector<std::string>({"hover","selected"}));

    // The animation manager seeks Tweeny with elapsed milliseconds. Repeated
    // evaluation advances according to GetTime(), not a fixed frame count.
    UIElementBase element;
    ComputedStyle first;
    first.background_color = BLACK;
    first.transitions = {{false,{PropertyType::BackgroundColor},1.0f,0.0f,UIEasing::Linear}};
    ResolveAnimatedStyle(element,sheet,first);

    ComputedStyle second = first;
    second.background_color = WHITE;
    ComputedStyle animated = ResolveAnimatedStyle(element,sheet,second);
    RUI_REQUIRE(SameColor(animated.background_color,BLACK));
    for(int index=0; index<25; ++index) {
        animated = ResolveAnimatedStyle(element,sheet,second);
    }
    RUI_REQUIRE(animated.background_color.r > 0 && animated.background_color.r < 255);

    bool model_selected = false;
    UIXRuntime runtime;
    runtime.Bind("selected",model_selected);
    RUI_REQUIRE(runtime.LoadString(R"RXML(
<UI>
    <Button id="choice" state:selected="{selected}">Choice</Button>
    <Functions>
        <Function name="markInvalid">
            <SetState target="choice" name="invalid" value="true" />
        </Function>
    </Functions>
</UI>
)RXML","pseudo-test.rxml"));

    UIContainer root;
    runtime.Rebuild(root);
    UIButton* choice = FindUI<UIButton>(root,"choice");
    RUI_REQUIRE(choice != nullptr);
    choice->rect.pseudo_states.Update();
    RUI_REQUIRE(!choice->rect.pseudo_states.Active("selected"));
    model_selected = true;
    choice->rect.pseudo_states.Update();
    RUI_REQUIRE(choice->rect.pseudo_states.Active("selected"));

    UIXControl control = runtime.Control(root,"choice");
    RUI_REQUIRE(control.Valid());
    RUI_REQUIRE(control.SetState("invalid",true));
    RUI_REQUIRE(control.HasState("invalid"));
    RUI_REQUIRE(control.ToggleState("invalid"));
    RUI_REQUIRE(!control.HasState("invalid"));
    RUI_REQUIRE(control.ToggleState("invalid"));
    RUI_REQUIRE(control.HasState("invalid"));
    RUI_REQUIRE(runtime.Invoke("markInvalid").IsNull());
    RUI_REQUIRE(control.HasState("invalid"));

    std::cout << "Pseudo-state and animation tests passed\n";
}
