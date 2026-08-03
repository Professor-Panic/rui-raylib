#include "Rui.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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

void TestPseudoStatePredicate() {
    bool model = false;
    UIPseudoStateSet states;
    states.Define("selected",false,[&](bool& active) { active = model; });
    states.Update();
    RUI_REQUIRE(!states.Active("selected"));
    model = true;
    states.Update();
    RUI_REQUIRE(states.Active("selected"));
}

void TestTrackedPseudoStateLifetime() {
    UIPseudoStateSet states;
    int calls = 0;
    UILifetimeToken lifetime;
    states.DefineTracked("safe",false,lifetime.Token(),[&](bool& active) {
        ++calls;
        active = true;
    });
    states.Update();
    RUI_REQUIRE(calls == 1 && states.Active("safe"));
    lifetime.Invalidate();
    states.Update();
    RUI_REQUIRE(!states.Contains("safe"));
    RUI_REQUIRE(calls == 1);

    UIContainer retained_root;
    {
        bool selected = true;
        UIXRuntime runtime;
        runtime.Bind("selected",selected);
        RUI_REQUIRE(runtime.LoadString(
            "<UI><Button id=\"safe-choice\" state:selected=\"{selected}\">Choice</Button></UI>",
            "lifetime.rxml"
        ));
        runtime.Rebuild(retained_root);
    }
    UIButton* retained = FindUI<UIButton>(retained_root,"safe-choice");
    RUI_REQUIRE(retained != nullptr);
    retained->rect.pseudo_states.Update();
    RUI_REQUIRE(!retained->rect.pseudo_states.Contains("selected"));
}

void TestUndoRedoAndMerge() {
    UIUndoStack stack;
    int value = 0;
    value = 1;
    stack.PushValue(value,0,1,"Edit value","value");
    value = 2;
    stack.PushValue(value,1,2,"Edit value","value");
    RUI_REQUIRE(stack.UndoCount() == 1);
    RUI_REQUIRE(stack.Undo());
    RUI_REQUIRE(value == 0);
    RUI_REQUIRE(stack.Redo());
    RUI_REQUIRE(value == 2);
}

void TestUndoTransaction() {
    UIUndoStack stack;
    int first = 0;
    int second = 0;
    stack.BeginTransaction("Change pair");
    first = 4;
    stack.PushValue(first,0,4,"First");
    second = 9;
    stack.PushValue(second,0,9,"Second");
    stack.EndTransaction();
    RUI_REQUIRE(stack.UndoCount() == 1);
    RUI_REQUIRE(stack.Undo());
    RUI_REQUIRE(first == 0 && second == 0);
    RUI_REQUIRE(stack.Redo());
    RUI_REQUIRE(first == 4 && second == 9);
}

void TestVirtualizedRange() {
    UIVisibleRange range = ComputeUIVisibleRange(10000,20.0f,1000.0f,200.0f,2);
    RUI_REQUIRE(!range.empty);
    RUI_REQUIRE(range.first == 48);
    RUI_REQUIRE(range.last == 63);
    RUI_REQUIRE(range.last-range.first < 100);
}

void TestWrappedAnimatedText() {
    UITextBox box;
    box.style.font_size = 16.0f;
    box.text = "The quick brown fox jumps over the lazy dog and wraps dynamically.";
    box.RebuildWrap(110.0f);
    RUI_REQUIRE(box.wrapped_lines.size() > 1);
    box.Animate(UITextAnimationMode::Typewriter,0.25f,false);
    const std::size_t beginning = box.VisibleCharacterCount(GetTime());
    for(int index=0; index<40; ++index) GetTime();
    const std::size_t later = box.VisibleCharacterCount(GetTime());
    RUI_REQUIRE(later >= beginning);
    RUI_REQUIRE(later <= box.text.size());
}

void TestTableSorting() {
    UITable table;
    table.AddColumn({"name","Name",120.0f});
    table.AddRow({"b",{"Beta"},false});
    table.AddRow({"a",{"Alpha"},false});
    table.SortBy(0,true);
    RUI_REQUIRE(table.rows.front().cells.front() == "Alpha");
    table.SortBy(0,false);
    RUI_REQUIRE(table.rows.front().cells.front() == "Beta");
}

void TestGeneralDragDropOverride() {
    UIDragDropContext context;
    context.BeginFrame();
    UIDragPayload payload;
    payload.type = "texture";
    payload.text = "spark.png";
    RUI_REQUIRE(context.BeginDrag("asset",payload,{10.0f,10.0f}));
    bool previewed = false;
    UIDropTargetBinding target;
    target.accepts = [](const UIDragPayload& incoming) {
        return incoming.type == "texture" && incoming.text.size() >= 4 && incoming.text.compare(incoming.text.size()-4,4,".png") == 0;
    };
    target.on_preview = [&](const UIDragPayload&) { previewed = true; };
    context.ConsiderTarget("slot",{0.0f,0.0f,50.0f,50.0f},target);
    RUI_REQUIRE(previewed);
    RUI_REQUIRE(context.Preview().accepted);
    RUI_REQUIRE(context.Preview().target_key == "slot");
    context.Cancel();
    RUI_REQUIRE(!context.Dragging());
}

void TestDockingPersistence() {
    UIDockSpace dock;
    UIDockPanel inspector; inspector.id = "inspector"; inspector.title = "Inspector";
    UIDockPanel preview; preview.id = "preview"; preview.title = "Preview";
    dock.AddPanel(std::move(inspector));
    dock.AddPanel(std::move(preview));
    RUI_REQUIRE(dock.root != nullptr);
    RUI_REQUIRE(dock.DockPanel("preview",dock.root->id,UIDockDropZone::Right));
    RUI_REQUIRE(dock.root->type == UIDockNodeType::Split);
    const std::string saved = dock.SerializeLayout();

    UIDockSpace restored;
    UIDockPanel restored_inspector; restored_inspector.id = "inspector";
    UIDockPanel restored_preview; restored_preview.id = "preview";
    restored.AddPanel(std::move(restored_inspector));
    restored.AddPanel(std::move(restored_preview));
    RUI_REQUIRE(restored.DeserializeLayout(saved));
    RUI_REQUIRE(restored.root != nullptr);
    RUI_REQUIRE(restored.root->type == UIDockNodeType::Split);
}

void TestRxmlControlsAndLayoutTween() {
    bool selected = true;
    UIXRuntime runtime;
    runtime.Bind("selected",selected);
    RUI_REQUIRE(runtime.LoadString(R"RXML(
<UI>
    <TextBox id="story" value="Animated wrapped text" editable="true"
             wrap="true" dynamicHeight="true" textAnimation="typewriter"
             state:selected="{selected}" />
    <VirtualList id="items" items="{['A','B','C','D']}" itemHeight="24" />
    <Table id="grid">
        <Column id="name" title="Name" width="100" />
        <Row key="one"><Cell>One</Cell></Row>
        <Row key="two"><Cell>Two</Cell></Row>
    </Table>
    <Button id="drag-source" dragType="asset/test" dragValue="{'payload'}">Drag</Button>
    <Container id="drop-target" dropTypes="asset/test" onDrop="handleDrop" />
    <DockSpace id="workspace">
        <DockPanel id="left" title="Left"><Label>Panel content</Label></DockPanel>
        <DockPanel id="right" title="Right"><Label>Second panel</Label></DockPanel>
    </DockSpace>
</UI>
)RXML","advanced-controls.rxml"));
    UIContainer root;
    runtime.Rebuild(root);
    UITextBox* story = FindUI<UITextBox>(root,"story");
    UIVirtualList* list = FindUI<UIVirtualList>(root,"items");
    UITable* table = FindUI<UITable>(root,"grid");
    UIDockSpace* dock = FindUI<UIDockSpace>(root,"workspace");
    UIButton* drag_source = FindUI<UIButton>(root,"drag-source");
    UIContainer* drop_target = FindUI<UIContainer>(root,"drop-target");
    RUI_REQUIRE(story && list && table && dock && drag_source && drop_target);
    RUI_REQUIRE(drag_source->rect.drag_source != nullptr);
    RUI_REQUIRE(drop_target->rect.drop_target != nullptr);
    story->rect.pseudo_states.Update();
    RUI_REQUIRE(story->rect.pseudo_states.Active("selected"));
    RUI_REQUIRE(list->items.size() == 4);
    RUI_REQUIRE(table->columns.size() == 1 && table->rows.size() == 2);
    RUI_REQUIRE(dock->panels.size() == 2);

    UIElementBase element;
    ComputedStyle first;
    first.width = 100.0f;
    first.width_length = CSSLength{100.0f,LengthUnit::Pixels};
    first.transitions = {{false,{PropertyType::Width},1.0f,0.0f,UIEasing::Linear}};
    StyleSheet sheet;
    ResolveLayoutAnimatedStyle(element,sheet,first);
    ComputedStyle second = first;
    second.width = 200.0f;
    second.width_length = CSSLength{200.0f,LengthUnit::Pixels};
    const ComputedStyle animated = ResolveLayoutAnimatedStyle(element,sheet,second);
    RUI_REQUIRE(IsUILayoutAnimationActive(element));
    RUI_REQUIRE(animated.width >= 0.0f && animated.width < 200.0f);
}

using Test = void(*)();
const std::vector<std::pair<const char*,Test>> Tests = {
    {"pseudo predicate",TestPseudoStatePredicate},
    {"tracked pseudo lifetime",TestTrackedPseudoStateLifetime},
    {"undo redo merge",TestUndoRedoAndMerge},
    {"undo transaction",TestUndoTransaction},
    {"virtualized range",TestVirtualizedRange},
    {"wrapped animated text",TestWrappedAnimatedText},
    {"table sorting",TestTableSorting},
    {"general drag drop",TestGeneralDragDropOverride},
    {"docking persistence",TestDockingPersistence},
    {"RXML controls and layout tween",TestRxmlControlsAndLayoutTween}
};

} // namespace

int main(int argc,char** argv) {
    if(argc == 2) {
        const int index = std::stoi(argv[1]);
        RUI_REQUIRE(index >= 0 && index < static_cast<int>(Tests.size()));
        Tests[static_cast<std::size_t>(index)].second();
        std::cout << Tests[static_cast<std::size_t>(index)].first << " passed\n";
        return 0;
    }
    for(const auto& test : Tests) test.second();
    std::cout << Tests.size() << " RUI behavior tests passed\n";
}
