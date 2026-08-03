#pragma once

// Public umbrella header for the complete Raylib UI framework.
#include "css_emulator.h"
#include "css_parser.h"
#include "ui_markup.h"

#include <string>

namespace rui {

struct FrameOptions {
    float width = -1.0f;
    float height = -1.0f;
    bool force_layout = false;
    bool rebuild_markup = false;
};

class Context {
public:
    UI ui;
    UIXRuntime runtime;
    StyleSheet styles;

    bool auto_rebuild = false;

    bool LoadStyleString(const std::string& source);
    bool LoadStyleFile(const std::string& path);
    bool LoadMarkupString(
        const std::string& source,
        std::string source_name = "<memory>"
    );
    bool LoadMarkupFile(
        const std::string& path,
        const UIXBuildOptions& options = {}
    );

    void SetBuildOptions(UIXBuildOptions options);
    void RequestRebuild();
    void Rebuild();

    // Unified frame update. Visual tweens, layout tweens, dirty layout,
    // widgets, drag/drop, docking, and binding synchronization are updated
    // through this one call.
    void Update(const FrameOptions& options = {});
    void Draw() const;

    UIContainer& Root() { return ui.root; }
    const UIContainer& Root() const { return ui.root; }
    UIUndoStack& UndoStack() { return runtime.UndoStack(); }
    bool Undo() { return runtime.Undo(); }
    bool Redo() { return runtime.Redo(); }

private:
    UIXBuildOptions build_options_;
    bool rebuild_requested_ = false;
};

using Control = UIXControl;
using Runtime = UIXRuntime;
using UndoStack = UIUndoStack;
using DragPayload = UIDragPayload;
using DragDropContext = UIDragDropContext;
using TextBox = UITextBox;
using Table = UITable;
using VirtualList = UIVirtualList;
using DockSpace = UIDockSpace;

} // namespace rui
