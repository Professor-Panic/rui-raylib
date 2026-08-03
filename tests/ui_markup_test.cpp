#include "ui_markup.h"

#include <cassert>
#include <cstdlib>
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

struct TestParticle {
    std::string name = "Spark";
    float speed = 4.0f;
    Vector2 velocity = {1.0f,2.0f};
    bool enabled = true;
};

int main() {
    TestParticle particle;
    std::vector<TestParticle> emitters = {
        {"Smoke",2.0f,{0.0f,1.0f},true},
        {"Fire",6.0f,{1.0f,-1.0f},false}
    };
    int selected_tab = 0;
    bool popup_open = false;
    int native_calls = 0;
    Texture2D preview_texture{};
    preview_texture.id = 7;

    UIXRegistry registry;
    registry.Bind("selectedTab",selected_tab);
    registry.Bind("popupOpen",popup_open);
    registry.Bind("previewTexture",preview_texture);
    registry.BindObject("particle",particle,[](auto& object) {
        object
            .Field("name",&TestParticle::name)
            .Field("speed",&TestParticle::speed)
            .Field("velocity",&TestParticle::velocity)
            .Field("enabled",&TestParticle::enabled);
    });
    registry.BindList("emitters",emitters,[](auto& object) {
        object
            .Field("name",&TestParticle::name)
            .Field("speed",&TestParticle::speed)
            .Field("velocity",&TestParticle::velocity)
            .Field("enabled",&TestParticle::enabled);
    });
    registry.Function("nativeSave",UIXNativeFunction(
        [&](const UIXArguments& arguments,UIXEvent&) {
            RUI_REQUIRE(!arguments.empty() && arguments.front().String() == "Spark");
            ++native_calls;
            return UIXValue(true);
        }
    ));
    registry.Function("canvasUpdate",[]() {});
    registry.Function("canvasDraw",[]() {});

    const std::string markup = R"UIX(
<UI id="editor-root" class="editor">
  <Components>
    <Component name="FieldRow">
      <Row class="field-row">
        <Label class="field-label" text="{props.label}" />
        <Slot />
      </Row>
    </Component>
  </Components>

  <Functions>
    <Function name="commit" params="bonus">
      particle.speed += bonus;
      cpp("nativeSave", particle.name);
    </Function>
    <Function name="openSettings">
      <Set target="popupOpen" value="{true}" />
    </Function>
  </Functions>

  <Container id="inspector" class="panel inspector" direction="column">
    <Label id="title">Editing {particle.name}</Label>
    <FieldRow label="Speed">
      <FloatInput id="speed" mode="slider" value="{particle.speed}"
                  min="0" max="20" step="0.25" onChange="commit(1)" />
    </FieldRow>
    <Vector2Input id="velocity" mode="drag" value="{particle.velocity}" />
    <BoolInput id="enabled" value="{particle.enabled}">Enabled</BoolInput>
    <ColorInput id="tint" value="{rgba(255, 120, 30, 255)}" />
    <StringInput id="name" value="{particle.name}" onSubmit="nativeSave(particle.name)" />
    <IntInput id="count" value="3" min="0" max="10" />
    <Selectable id="selectable" selected="false">Selectable</Selectable>
    <ComboBox id="blend" items="{['Alpha', 'Additive']}" selectedIndex="0" />
    <RangeInput id="range" lower="0.2" upper="0.8" min="0" max="1" />
    <Image id="preview-image" texture="{previewTexture}" source="0, 0, 32, 32" />
    <Canvas id="preview-canvas" onUpdate="canvasUpdate" onDraw="canvasDraw" />
    <Button id="settings" if="{particle.enabled}" onClick="openSettings">Settings</Button>
  </Container>

  <Row id="workspace-row">
    <Container id="left" />
    <Splitter id="split" orientation="vertical" />
    <Container id="right" />
  </Row>

  <Collapsible id="advanced" open="true" text="Advanced"><Label>Inside</Label></Collapsible>
  <TreeNode id="tree" open="true" selected="false" text="Emitter"><Label>Child</Label></TreeNode>
  <Menu id="file-menu" text="File"><Button>Save</Button></Menu>
  <Popup id="settings-popup" open="{popupOpen}" title="Settings"><Label>Popup body</Label></Popup>
  <Window id="tool-window" title="Tool"><Label>Window body</Label></Window>
  <TabBar id="tabs" selectedIndex="{selectedTab}">
    <Tab id="general" title="General"><Label>General body</Label></Tab>
    <Tab id="output" title="Output"><Label>Output body</Label></Tab>
  </TabBar>

  <For each="{emitters}" as="emitter" index="i">
    <Label id="emitter-{i}" key="emitter-{i}">{i}: {emitter.name}</Label>
  </For>
</UI>
)UIX";

    UIXRuntime runtime(registry);
    RUI_REQUIRE(runtime.LoadString(markup,"test.ui.xml"));
    RUI_REQUIRE(runtime.Document().components.count("fieldrow") == 1);
    RUI_REQUIRE(runtime.Document().functions.count("commit") == 1);
    RUI_REQUIRE(runtime.Evaluate("particle.velocity.x + particle.velocity.y").Number() == 3.0);
    RUI_REQUIRE(runtime.Execute("particle.velocity.x = 9; particle.name = 'Glow'"));
    RUI_REQUIRE(particle.velocity.x == 9.0f && particle.name == "Glow");
    particle.name = "Spark";

    UIContainer root;
    runtime.Rebuild(root);
    RUI_REQUIRE(!root.elements.empty());
    RUI_REQUIRE(FindUI<UILabel>(root,"title") != nullptr);
    UIInput<float>* speed = FindUI<UIInput<float>>(root,"speed");
    RUI_REQUIRE(speed != nullptr && speed->value == particle.speed);
    speed->value = 8.0f;
    speed->changed = true;
    speed->on_change(*speed);
    RUI_REQUIRE(particle.speed == 9.0f);
    RUI_REQUIRE(native_calls == 1);

    // A rebuild happens between input frames. Pointer capture and drag
    // bookkeeping must survive it so held interactions continue on frame two.
    speed->dragging = true;
    speed->active = true;
    speed->drag_start_value = 3.0f;
    speed->drag_accumulator = 1.25f;

    UIInput<Vector2>* velocity = FindUI<UIInput<Vector2>>(root,"velocity");
    RUI_REQUIRE(velocity != nullptr);
    velocity->dragging = true;
    velocity->active_component = 1;
    velocity->drag_start_value = {2.0f,3.0f};
    velocity->drag_accumulator = 0.75f;

    UIRangeInput<float>* range = FindUI<UIRangeInput<float>>(root,"range");
    RUI_REQUIRE(range != nullptr);
    range->active_handle = 2;

    UICanvas* canvas = FindUI<UICanvas>(root,"preview-canvas");
    RUI_REQUIRE(canvas != nullptr);
    canvas->dragging = true;
    canvas->mouse_pressed_inside[MOUSE_BUTTON_LEFT] = true;

    UISplitter* splitter = FindUI<UISplitter>(root,"split");
    RUI_REQUIRE(splitter != nullptr);
    splitter->dragging = true;
    splitter->drag_origin = {12.0f,34.0f};
    splitter->before_start_size = {200.0f,100.0f};
    splitter->after_start_size = {300.0f,100.0f};

    UIWindow* window = FindUI<UIWindow>(root,"tool-window");
    RUI_REQUIRE(window != nullptr);
    window->dragging = true;
    window->drag_anchor = {40.0f,50.0f};
    window->interaction_start = {10.0f,20.0f,320.0f,240.0f};

    runtime.Rebuild(root);

    speed = FindUI<UIInput<float>>(root,"speed");
    velocity = FindUI<UIInput<Vector2>>(root,"velocity");
    range = FindUI<UIRangeInput<float>>(root,"range");
    canvas = FindUI<UICanvas>(root,"preview-canvas");
    splitter = FindUI<UISplitter>(root,"split");
    window = FindUI<UIWindow>(root,"tool-window");
    RUI_REQUIRE(speed != nullptr && speed->dragging && speed->active);
    RUI_REQUIRE(speed->drag_start_value == 3.0f && speed->drag_accumulator == 1.25f);
    RUI_REQUIRE(velocity != nullptr && velocity->dragging && velocity->active_component == 1);
    RUI_REQUIRE(velocity->drag_start_value.y == 3.0f && velocity->drag_accumulator == 0.75f);
    RUI_REQUIRE(range != nullptr && range->active_handle == 2);
    RUI_REQUIRE(canvas != nullptr && canvas->dragging);
    RUI_REQUIRE(canvas->mouse_pressed_inside[MOUSE_BUTTON_LEFT]);
    RUI_REQUIRE(splitter != nullptr && splitter->dragging);
    RUI_REQUIRE(splitter->drag_origin.x == 12.0f && splitter->before_start_size.x == 200.0f);
    RUI_REQUIRE(window != nullptr && window->dragging);
    RUI_REQUIRE(window->drag_anchor.y == 50.0f && window->interaction_start.width == 320.0f);

    UIButton* settings = FindUI<UIButton>(root,"settings");
    RUI_REQUIRE(settings != nullptr);
    settings->on_click(*settings);
    RUI_REQUIRE(popup_open);

    runtime.Rebuild(root);
    UIPopup* popup = FindUI<UIPopup>(root,"settings-popup");
    RUI_REQUIRE(popup != nullptr && popup->open);
    RUI_REQUIRE(FindUI<UILabel>(root,"emitter-0") != nullptr);
    RUI_REQUIRE(FindUI<UILabel>(root,"emitter-1") != nullptr);
    RUI_REQUIRE(runtime.Diagnostics().empty());

    std::cout << "UI markup tests passed\n";
}
