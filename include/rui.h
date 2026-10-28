#pragma once

#include "css_emulator.h"
#include "css_parser.h"
#include "ui_markup.h"
#include "component_registry.h"

#include <string>

namespace rui {

inline constexpr int VersionMajor = 0;
inline constexpr int VersionMinor = 9;
inline constexpr int VersionPatch = 0;
inline constexpr const char* VersionString = "0.9.1";
inline constexpr bool FeatureFrozen = false;

// Explicitly release RCSS effect RenderTextures/shaders while the raylib
// graphics context is still alive. Usually needed only for controlled shutdown
// or renderer reloads.
inline void ClearEffectResources() {
    ::ClearUIEffectResources();
}

struct FrameOptions {
    float width = -1.0f;
    float height = -1.0f;
    bool force_layout = false;
    bool rebuild_markup = false;
    // Expensive compatibility mode for code that mutates public layout fields
    // without calling InvalidateLayout(). Keep false for retained-mode speed.
    bool detect_layout_mutations = false;
};

class Context {
public:
    UI ui;
    UIXRuntime runtime;
    StyleSheet styles;

    bool auto_rebuild = false;

    bool LoadStyleString(const std::string& source);
    bool LoadStyleFile(const std::string& path);
    // Compose independent RCSS files without rebuilding the source manually.
    // Append has higher cascade priority than the existing file source;
    // prepend has lower priority. Each file expands its own @import directives.
    bool AppendStyleFile(const std::string& path);
    bool PrependStyleFile(const std::string& path);

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

    void Update(const FrameOptions& options = {});
    void Draw() const;

    // Global interaction queries. Read these after Update() for the current
    // frame. HasFocus() is a convenience alias for HasKeyboardFocus().
    bool HasFocus() const;
    bool HasKeyboardFocus() const;
    bool HasPointerCapture() const;
    bool WantsKeyboard() const;
    bool WantsMouse() const;

    UIElementBase* FocusedElement();
    const UIElementBase* FocusedElement() const;
    std::string FocusedId() const;

    // The `key` of the focused widget (its `id` if one was given), or "".
    std::string FocusedKey() const;

    // The value of the focused widget's userData="{...}" attribute, or a null
    // UIXValue if nothing is focused or the widget has no userData.
    UIXValue FocusedUserData() const;

    // Typed convenience: FocusedUserData<entt::entity>(), <int>(), <std::string>().
    // Returns nullopt if nothing is focused, there is no userData, or it does
    // not convert to T.
    template <typename T>
    std::optional<T> FocusedUserData() const {
        const UIXValue value = FocusedUserData();
        if(value.IsNull()) return std::nullopt;
        T converted{};
        if(!value.AssignTo(converted)) return std::nullopt;
        return converted;
    }

    void InvalidateLayout() {
        ui.InvalidateLayout();
    }

    UIContainer& Root() {
        return ui.root;
    }

    const UIContainer& Root() const {
        return ui.root;
    }

    UIUndoStack& UndoStack() {
        return runtime.UndoStack();
    }

    bool Undo() {
        return runtime.Undo();
    }

    bool Redo() {
        return runtime.Redo();
    }

private:
    void RefreshStyles();
    // Fires onBlur/onFocus when the focused widget changed since last frame.
    void DispatchFocusEvents();
    // Runs the focused widget's onKey bindings (once per frame).
    void DispatchKeyEvents();

    std::string focus_key_;
    std::string style_source_;
    UIXBuildOptions build_options_;
    bool rebuild_requested_ = false;
};

// Core
using Container = ::UIContainer;
using Element = ::UIElement;
using ComputedStyle = ::ComputedStyle;

// Basic widgets
using Button = ::UIButton;
using Label = ::UILabel;
using Image = ::UIImage;
using ImageFit = ::UIImageFit;
using ImageSampling = ::UIImageSampling;
using ImageSliceMode = ::UIImageSliceMode;
using ImageBlendMode = ::UIImageBlendMode;
using Canvas = ::UICanvas;
using Viewport = ::UIViewport;
using ViewportItem = ::UIViewportItem;
using Selectable = ::UISelectable;
using ComboBox = ::UIComboBox;

// Inputs
template <typename T>
using Input = ::UIInput<T>;

template <typename T>
using RangeInput = ::UIRangeInput<T>;

using FloatInput = ::UIInput<float>;
using IntInput = ::UIInput<int>;
using BoolInput = ::UIInput<bool>;
using StringInput = ::UIInput<std::string>;

using Vector2Input = ::UIInput<Vector2>;
using Vector3Input = ::UIInput<Vector3>;
using Vector4Input = ::UIInput<Vector4>;
using ColorInput = ::UIInput<Color>;

// Structural widgets
using Collapsible = ::UICollapsible;
using TreeNode = ::UITreeNode;
using TabBar = ::UITabBar;
using Menu = ::UIMenu;
using Popup = ::UIPopup;
using Window = ::UIWindow;
using Splitter = ::UISplitter;
using Event =::UIXEvent;

// Advanced widgets
using TextBox = ::UITextBox;
using Table = ::UITable;
using VirtualList = ::UIVirtualList;
using DockSpace = ::UIDockSpace;

// Component metadata / registration (0.9)
using UIComponent = ::UIComponent;
using ComponentBase = ::UIComponent;
using ComponentRegistry = ::UIComponentRegistry;
using ComponentDescriptor = ::UIComponentDescriptor;
using PropertyDescriptor = ::UIPropertyDescriptor;
using EventDescriptor = ::UIEventDescriptor;
using PropertyValue = ::UIPropertyValue;
using PropertyType = ::UIPropertyType;
using PropertyFlag = ::UIPropertyFlag;
using ComponentCapability = ::UIComponentCapability;
using ChildPolicy = ::UIChildPolicy;
using ChildPolicyKind = ::UIChildPolicyKind;
using RegistrationConflictPolicy = ::UIRegistrationConflictPolicy;
using ComponentKind = ::UIComponentKind;
using PropertyValidationPolicy = ::UIPropertyValidationPolicy;
using PropertyConverterRegistry = ::UIPropertyConverterRegistry;
using SerializedComponent = ::UISerializedComponent;
using ConfigurationValues = ::UIConfigurationValues;
template <typename T> using ComponentBuilder = ::UIComponentBuilder<T>;

template <typename Widget, typename T>
inline PropertyDescriptor Property(
    std::string name,
    T Widget::* member,
    T default_value = T{},
    PropertyFlag flags = PropertyFlag::Bindable|PropertyFlag::Serializable|PropertyFlag::Inspectable,
    std::string documentation = {}
) {
    return ::MakeUIProperty<Widget,T>(
        std::move(name),member,std::move(default_value),flags,std::move(documentation)
    );
}

template <typename Custom>
inline ComponentDescriptor Component(std::string name) {
    return ::MakeUICustomComponentDescriptor<Custom>(std::move(name));
}

template <typename Custom>
inline ComponentBuilder<Custom> DescribeComponent(std::string name) {
    return ::DescribeUIComponent<Custom>(std::move(name));
}

template <typename T>
inline PropertyDescriptor ConfigurationProperty(
    std::string name,
    T default_value = T{},
    PropertyFlag flags = PropertyFlag::Inspectable,
    std::string documentation = {}
) {
    return ::MakeUIConfigurationProperty<T>(
        std::move(name),std::move(default_value),flags,std::move(documentation)
    );
}

inline std::shared_ptr<ComponentRegistry> DefaultComponentRegistry() {
    return ::MakeDefaultComponentRegistry();
}
inline std::string GenerateSchema(const ComponentRegistry& registry) {
    return ::GenerateRuiSchemaJson(registry);
}
inline std::string GenerateProjectSchema(const ComponentRegistry& registry) {
    return ::GenerateRuiProjectSchemaJson(registry);
}
inline std::string GenerateComponentReference(const ComponentRegistry& registry) {
    return ::GenerateRuiComponentMarkdown(registry);
}
inline std::optional<SerializedComponent> SerializeComponent(
    const ComponentRegistry& registry,
    const ComponentDescriptor& descriptor,
    const Element& element,
    std::vector<std::string>* diagnostics = nullptr
) {
    return ::SerializeRegisteredComponent(registry,descriptor,element,diagnostics);
}
inline bool DeserializeComponent(
    const ComponentRegistry& registry,
    const ComponentDescriptor& descriptor,
    Element& element,
    const SerializedComponent& serialized,
    std::vector<std::string>* diagnostics = nullptr
) {
    return ::DeserializeRegisteredComponent(registry,descriptor,element,serialized,diagnostics);
}

// Runtime
using Control = ::UIXControl;
using Runtime = ::UIXRuntime;
using BuildOptions = ::UIXBuildOptions;

// Undo and drag/drop
using UndoStack = ::UIUndoStack;
using DragPayload = ::UIDragPayload;
using DragDropContext = ::UIDragDropContext;

} // namespace rui