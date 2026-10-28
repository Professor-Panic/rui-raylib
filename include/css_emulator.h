#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cfloat>
#include <deque>
#include <functional>
#include <initializer_list>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

//--------------------------------------------------------------------
// CSS declarations
//--------------------------------------------------------------------

enum class SelectorType {
    Any,
    Body,
    Button,
    Label,
    Input,
    Container,
    Selectable,
    ComboBox,
    RangeInput,
    Canvas,
    Image,
    Splitter,
    TabBar,
    Collapsible,
    TreeNode,
    Menu,
    Popup,
    Window,
    TextBox,
    Table,
    VirtualList,
    DockSpace,
    Viewport,
    ViewportItem,
    Tooltip
};

enum class SelectorState {
    None,
    Hover,
    Active,
    Focus,
    Disabled
};

// Named pseudo-states are open-ended. The enum remains for source compatibility
// with the original built-in state API.
std::string NormalizeUIPseudoStateName(std::string name);

// Lifetime token used by callback-bearing UI features. A tracked callback is
// ignored and removed after its owner expires, preventing stale references from
// being invoked by retained widgets or reconciled markup trees.
class UILifetimeToken {
public:
    UILifetimeToken() : token_(std::make_shared<int>(0)) {}
    UILifetimeToken(const UILifetimeToken&) = delete;
    UILifetimeToken& operator=(const UILifetimeToken&) = delete;
    UILifetimeToken(UILifetimeToken&&) noexcept = default;
    UILifetimeToken& operator=(UILifetimeToken&&) noexcept = default;

    std::weak_ptr<void> Token() const { return token_; }
    void Invalidate() { token_ = std::make_shared<int>(0); }

private:
    std::shared_ptr<void> token_;
};

struct UIPseudoState {
    bool active = false;
    std::function<void(bool&)> update;
    std::weak_ptr<void> lifetime;
    bool lifetime_tracked = false;
};

class UIPseudoStateSet {
public:
    UIPseudoState& Define(
        std::string name,
        bool initial_value = false,
        std::function<void(bool&)> update = {}
    );
    UIPseudoState& DefineTracked(
        std::string name,
        bool initial_value,
        std::weak_ptr<void> lifetime,
        std::function<void(bool&)> update
    );
    bool Set(const std::string& name, bool value);
    // Fast path for normalized engine-owned names such as hover/open/active.
    bool SetKnown(std::string_view normalized_name, bool value);
    bool Toggle(const std::string& name);
    bool Remove(const std::string& name);
    bool Active(const std::string& name) const;
    bool Contains(const std::string& name) const;
    void Clear();
    void Update();
    void SyncBuiltIn(const std::string& name, bool engine_value);
    std::vector<std::string> ActiveNames() const;
    const std::vector<std::string>& ActiveNamesView() const;
    std::size_t ActiveHash() const;

private:
    void InvalidateActiveCache();
    void RebuildActiveCache() const;

    std::unordered_map<std::string,UIPseudoState> states_;
    mutable bool active_cache_dirty_ = true;
    mutable std::vector<std::string> active_names_cache_;
    mutable std::size_t active_hash_cache_ = 0;
};

enum class UIEasing {
    Linear, Ease, EaseIn, EaseOut, EaseInOut,
    QuadraticIn, QuadraticOut, QuadraticInOut,
    CubicIn, CubicOut, CubicInOut,
    BackIn, BackOut, BackInOut,
    BounceIn, BounceOut, BounceInOut,
    ElasticIn, ElasticOut, ElasticInOut
};

enum class UIAnimationDirection {
    Normal, Reverse, Alternate, AlternateReverse
};

enum class UIAnimationFillMode {
    None, Forwards, Backwards, Both
};

enum class DisplayType {
    Inline,
    Block,
    Flex,
    Grid,
    None
};

enum class PositionType {
    Static,
    Relative,
    Absolute
};

enum class FlexDirection {
    Row,
    Column
};

enum class FlexWrap {
    NoWrap,
    Wrap,
    WrapReverse
};

enum class JustifyContent {
    FlexStart,
    Center,
    FlexEnd,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class AlignItems {
    Stretch,
    FlexStart,
    Center,
    FlexEnd
};

enum class AlignSelf {
    Auto,
    Stretch,
    FlexStart,
    Center,
    FlexEnd
};

enum class AlignContent {
    Stretch,
    FlexStart,
    Center,
    FlexEnd,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class JustifySelf {
    Auto,
    Stretch,
    FlexStart,
    Center,
    FlexEnd
};
enum class PerspectiveType{
    None,
    Inherit,
    Initial
};
enum class GridAutoFlow {
    Row,
    Column
};

enum class UIImageFit {
    Stretch,
    Contain,
    Cover,
    Center,
    Tile,
    TileX,
    TileY
};

enum class UIImageSampling {
    Nearest,
    Bilinear
};

enum class UIImageSliceMode {
    Stretch,
    Tile
};

enum class UIImageBlendMode {
    Alpha,
    Additive,
    Multiplied,
    AddColors,
    SubtractColors,
    AlphaPremultiply
};

enum class UISplitterOrientation {
    Auto,
    Vertical,
    Horizontal
};

enum class TextAlign {
    Left,
    Center,
    Right
};

enum class OverflowType {
    Visible,
    Hidden,
    Auto,
    Scroll
};

// RCSS border stroke mode. Solid is the compatibility default.
enum class UIBorderStyle {
    None,
    Solid,
    Dashed,
    Dotted,
    Double
};

// Typed RCSS expression/value infrastructure. Literal lengths stay allocation-free;
// only calc()/min()/max()/clamp()/var() values allocate an immutable AST.
enum class UIStyleValueDimension {
    Number,
    Length,
    Angle,
    Time
};

enum class UIStyleUnit {
    Number,
    Pixels,
    Percent,
    ViewWidth,
    ViewHeight,
    Em,
    RootEm,
    Degrees,
    Seconds,
    Milliseconds
};

enum class UIStyleExpressionKind {
    Literal,
    Variable,
    Add,
    Subtract,
    Multiply,
    Divide,
    Negate,
    Minimum,
    Maximum,
    Clamp
};

struct UIStyleExpressionNode;
using UIStyleExpression = std::shared_ptr<const UIStyleExpressionNode>;
using UIStyleVariableMap = std::unordered_map<std::string,std::string>;

struct UIStyleExpressionNode {
    UIStyleExpressionKind kind = UIStyleExpressionKind::Literal;
    float literal = 0.0f;
    UIStyleUnit unit = UIStyleUnit::Number;
    std::string variable;
    std::string fallback;
    std::vector<UIStyleExpression> arguments;
};

struct UIStyleEvaluationContext {
    float percent_reference = 0.0f;
    float viewport_width = 0.0f;
    float viewport_height = 0.0f;
    float font_size = 16.0f;
    float root_font_size = 16.0f;
    const UIStyleVariableMap* variables = nullptr;
};

struct UIEvaluatedStyleValue {
    float value = 0.0f;
    UIStyleValueDimension dimension = UIStyleValueDimension::Number;
};

UIStyleExpression ParseUIStyleExpression(
    const std::string& source,
    std::string* error = nullptr
);

std::optional<UIEvaluatedStyleValue> EvaluateUIStyleExpression(
    const UIStyleExpression& expression,
    const UIStyleEvaluationContext& context,
    std::string* error = nullptr
);

enum class LengthUnit {
    Auto,
    Pixels,
    Percent,
    ViewWidth,
    ViewHeight,
    Em,
    RootEm,
    Expression
};
enum class InputStepButtonLayout {
    Front,      // Both buttons after the value area:  [-][+]
    Back,       // Both buttons before the value area: [-][+] value
    Opposite    // Decrement before, increment after:  [-] value [+]
};

struct CSSLength {
    float value = 0.0f;
    LengthUnit unit = LengthUnit::Pixels;
    // Kept for diagnostics/source compatibility. expression_ast is parsed once
    // at stylesheet compilation and evaluated cheaply during layout.
    std::string expression;
    UIStyleExpression expression_ast;
};

struct UIDeferredCSSValue {
    std::string raw;
};

struct UIGeneratedContentValue {
    std::string text;
    bool enabled = true;
};

enum class UIGeneratedPseudoKind {
    None,
    Before,
    After
};

enum class UIFilterType {
    Blur,
    Brightness,
    Contrast,
    Saturate,
    Grayscale,
    Opacity,
    HueRotate
};

struct UIFilterOperation {
    UIFilterType type = UIFilterType::Opacity;
    UIStyleExpression value;
    std::string source;
};

struct UIFilterList {
    std::vector<UIFilterOperation> operations;
};
using UIFilterListRef = std::shared_ptr<const UIFilterList>;

struct UIShaderEffect {
    std::string path;
};
struct UIShaderEffectList {
    std::vector<UIShaderEffect> effects;
};
using UIShaderEffectListRef = std::shared_ptr<const UIShaderEffectList>;

struct UIShaderUniformBinding {
    std::string name;
    std::string raw_value;
};
using UIShaderUniformList = std::vector<UIShaderUniformBinding>;
using UIShaderUniformListRef = std::shared_ptr<const UIShaderUniformList>;

enum class UIMixBlendMode {
    Normal,
    Alpha,
    Additive,
    Multiply,
    AddColors,
    SubtractColors,
    AlphaPremultiply
};

enum class UITransformOperationType {
    // 2D operations (existing)
    TranslateX,
    TranslateY,
    ScaleX,
    ScaleY,
    Rotate,          // 2D rotation around Z-axis (angle in degrees)
    // 3D operations (new)
    TranslateZ,
    ScaleZ,
    RotateX,
    RotateY,
    RotateZ,         // alternative to Rotate for clarity
    Rotate3D,     // axis-angle
    Matrix3D,     // raw 4x4 matrix
    SkewX,
    SkewY
};

struct UITransformOperation {
    UITransformOperationType type = UITransformOperationType::TranslateX;
    UIStyleExpression value;
    std::string source;
};
struct UITransformList {
    std::vector<UITransformOperation> operations;
};
using UITransformListRef = std::shared_ptr<const UITransformList>;

struct UICustomPropertyDeclaration {
    std::string name;
    std::string raw_value;
};

// Unresolved descriptor-driven component style property. Unlike --variables,
// these do not inherit. The parser preserves them without needing registry
// access; registered UIComponent descriptors validate/apply them at draw time.
struct UIComponentStyleDeclaration {
    std::string name;
    std::string raw_value;
};

enum class GridTrackUnit {
    Pixels,
    Percent,
    Fraction,
    Auto
};

struct GridTrackSize {
    float value = 1.0f;
    GridTrackUnit unit = GridTrackUnit::Fraction;
};

using GridTrackList = std::vector<GridTrackSize>;

enum class PropertyType {
    BackgroundColor,
    BorderColor,
    BorderWidth,
    BorderTopWidth,
    BorderRightWidth,
    BorderBottomWidth,
    BorderLeftWidth,
    BorderStyle,
    Border,
    BorderRadius,
    Width,
    Height,
    MinWidth,
    MaxWidth,
    MinHeight,
    MaxHeight,
    Padding,
    PaddingLeft,
    PaddingRight,
    PaddingTop,
    PaddingBottom,
    Margin,
    MarginLeft,
    MarginRight,
    MarginTop,
    MarginBottom,
    TextColor,
    FontName,
    FontSize,
    DisplayType,
    Gap,
    RowGap,
    ColumnGap,
    Opacity,
    Position,
    Left,
    Right,
    Top,
    Bottom,
    FlexDirection,
    JustifyContent,
    AlignItems,
    JustifyItems,
    AlignSelf,
    FlexGrow,
    FlexShrink,
    FlexBasis,
    TextAlign,
    Overflow,
    GridTemplateColumns,
    GridTemplateRows,
    GridAutoColumns,
    GridAutoRows,
    GridAutoFlow,
    GridColumn,
    GridRow,
    GridColumnSpan,
    GridRowSpan,
    InputStepButtons,
    InputStep,
    InputValueBox,
    InputDragSpeed,
    InputSliderShowValue,
    InputSliderValueWidth,
    InputSliderClickToSet,
    RuntimeMovable,
    RuntimeResizable,
    RuntimeConstrain,
    RuntimeLockX,
    RuntimeLockY,
    RuntimeSnap,
    ResizeToContent,
    ScrollSpeed,
    ScrollbarSize,
    Scale,
    HeaderHeight,
    ItemHeight,
    PopupWidth,
    PopupMaxHeight,
    TooltipDelay,
    Indent,
    Resizable,
    Closable,
    Collapsible
    ,ObjectFit
    ,ImageRendering
    ,ImageTint
    ,ImageFlipX
    ,ImageFlipY
    ,SplitterSize
    ,TabHeight
    ,TabClosable
    ,ObjectPositionX
    ,ObjectPositionY
    ,ImageRotation
    ,ImagePivotX
    ,ImagePivotY
    ,ImageFrame
    ,ImageFrameWidth
    ,ImageFrameHeight
    ,ImageFrameColumns
    ,ImageSliceLeft
    ,ImageSliceRight
    ,ImageSliceTop
    ,ImageSliceBottom
    ,ImageSliceScale
    ,ImageSliceMode
    ,ImageTileScale
    ,ImageTileOffsetX
    ,ImageTileOffsetY
    ,ImageBlendMode
    ,ImagePixelSnap
    ,Content
    ,Filter
    ,BackdropFilter
    ,Effect
    ,MixBlendMode
    ,Transform
    ,Perspective
    // Parser-only shorthand properties. They expand into existing longhands
    // and are never stored in a ComputedStyle.
    ,Background
    ,Font
    ,Inset
    ,Flex
    ,PlaceItems
    // 0.7.1 flex/grid alignment additions. Appended to preserve the numeric
    // values of all existing PropertyType entries.
    ,FlexWrap
    ,FlexFlow
    ,AlignContent
    ,JustifySelf
    // Appended, same reasoning as the 0.7.1 block above: keeps every existing
    // PropertyType's numeric value unchanged. TransformOriginX/Y are the real,
    // stored-in-ComputedStyle longhands; TransformOrigin is a parser-only
    // shorthand that expands into both, same pattern as Inset/Flex/PlaceItems.
    ,TransformOriginX
    ,TransformOriginY
    ,TransformOrigin
};

struct UITransitionSpec {
    bool all = true;
    std::vector<PropertyType> properties;
    // Dynamic RCSS values such as --strength and shader-strength do not have
    // fixed PropertyType enum slots. They remain named so the animation layer
    // can interpolate them without the shader renderer owning animation logic.
    std::vector<std::string> dynamic_properties;
    float duration = 0.0f;
    float delay = 0.0f;
    UIEasing easing = UIEasing::Ease;
};

struct UIAnimationSpec {
    std::string name;
    float duration = 0.0f;
    float delay = 0.0f;
    UIEasing easing = UIEasing::Ease;
    float iterations = 1.0f;
    bool infinite = false;
    UIAnimationDirection direction = UIAnimationDirection::Normal;
    UIAnimationFillMode fill_mode = UIAnimationFillMode::None;
    bool paused = false;
};

using CSS_Id = std::variant<
    std::monostate,
    int,
    std::string
>;

using PropertyValueType = std::variant<
    Color,
    std::string,
    float,
    int,
    bool,
    Vector2,
    Vector3,
    Vector4,
    CSSLength,
    DisplayType,
    PositionType,
    FlexDirection,
    FlexWrap,
    JustifyContent,
    AlignItems,
    AlignSelf,
    AlignContent,
    JustifySelf,
    TextAlign,
    OverflowType,
    GridTrackList,
    GridAutoFlow,
    UIImageFit,
    UIImageSampling,
    UIImageSliceMode,
    UIImageBlendMode,
    UIGeneratedContentValue,
    UIDeferredCSSValue,
    UIFilterListRef,
    UIShaderEffectListRef,
    UIMixBlendMode,
    UITransformListRef,
    PerspectiveType
>;

struct Declaration {
    PropertyType property;
    PropertyValueType value;

    Declaration() = default;

    Declaration(PropertyType property_type, PropertyValueType property_value)
        : property(property_type),
          value(std::move(property_value)) {}
};

// One compound selector in a descendant selector chain. The chain stored on a
// rule contains only its ancestors; the final target keeps using StyleRule's
// legacy selector/id/classes fields for source compatibility.
// Structural pseudo-class `:nth-child(an+b)` / `:nth-last-child(an+b)`.
// :first-child, :last-child, :only-child, odd and even are normalised into
// this form by the parser.
struct UIStructuralPseudo {
    bool from_end = false;
    int a = 0;
    int b = 0;
};

// True when 1-based position `index` (of `count` siblings) matches all entries.
bool StructuralPseudosMatch(
    const std::vector<UIStructuralPseudo>& pseudos,
    std::size_t index,
    std::size_t count
);

struct StyleSelector {
    SelectorType selector = SelectorType::Any;
    CSS_Id id = std::monostate{};
    std::vector<std::string> classes;
    std::vector<std::string> pseudo_states;
    // Required structural pseudo-classes (rules) ...
    std::vector<UIStructuralPseudo> structural;
    // ... and the element's own position (snapshots). 1-based; 0 = unknown.
    std::size_t sibling_index = 0;
    std::size_t sibling_count = 0;
};

// Persistent nearest-first chain of preceding siblings. Sharing the chain avoids
// copying every earlier sibling into every child when + or ~ selectors exist.
struct StyleSelectorSiblingNode;
using StyleSelectorSiblingChain =
    std::shared_ptr<const StyleSelectorSiblingNode>;

struct StyleSelectorSiblingNode {
    StyleSelector selector;
    StyleSelectorSiblingChain previous;
    std::size_t cumulative_hash = 0;
    std::size_t count = 0;
};

enum class SelectorCombinator {
    Descendant,
    Child,
    AdjacentSibling,
    GeneralSibling
};

struct UIMediaQuery {
    std::optional<float> min_width;
    std::optional<float> max_width;
    std::optional<float> min_height;
    std::optional<float> max_height;
    // Empty, "landscape", or "portrait".
    std::string orientation;
};

struct StyleRule {
    SelectorType selector = SelectorType::Any;
    CSS_Id id = std::monostate{};
    std::vector<std::string> classes;
    // `state` is retained for programmatically-created legacy rules. Parsed
    // selectors use pseudo_states and may contain any number of names.
    SelectorState state = SelectorState::None;
    std::vector<std::string> pseudo_states;
    std::vector<UIStructuralPseudo> structural;
    // Ordered from the left-most selector to the compound immediately before
    // the target. Combinators describe each relationship to the next compound.
    std::vector<StyleSelector> ancestors;
    std::vector<SelectorCombinator> combinators;
    // Empty means unconditional. Multiple queries use comma/OR semantics.
    std::vector<UIMediaQuery> media_queries;
    std::vector<Declaration> declarations;
    std::vector<UICustomPropertyDeclaration> custom_properties;
    std::vector<UIComponentStyleDeclaration> component_style_properties;
    std::vector<UIShaderUniformBinding> shader_uniforms;
    UIGeneratedPseudoKind pseudo_element = UIGeneratedPseudoKind::None;
    // Precomputed once by TouchStyleSheet(); avoids recomputing specificity on
    // every style resolution.
    int specificity = 0;
    bool transitions_declared = false;
    std::vector<UITransitionSpec> transitions;
    bool animations_declared = false;
    std::vector<UIAnimationSpec> animations;
};

struct UIKeyframeStep {
    float offset = 0.0f;
    std::vector<Declaration> declarations;
    // Descriptor-driven custom component style values share the same timeline
    // as ordinary RCSS declarations.
    std::vector<UIComponentStyleDeclaration> component_style_properties;
};

struct UIKeyframes {
    std::string name;
    std::vector<UIKeyframeStep> steps;
};

std::size_t NextStyleSheetRevision();

void ClearResolvedStyleCache();

struct StyleSheet;
struct ComputedStyle;
bool UIPropertyAffectsLayout(PropertyType property);
bool IsUILayoutProperty(PropertyType property);
bool UIAnimationTouchesLayout(
    const StyleSheet& style_sheet,
    const std::string& animation_name
);

struct StyleSheet {
    std::vector<StyleRule> rules;
    std::unordered_map<std::string,UIKeyframes> keyframes;
    std::shared_ptr<const UIStyleVariableMap> root_custom_properties;
    // Precomputed by TouchStyleSheet(). Most stylesheets do not use + or ~,
    // so layout can skip all sibling-context bookkeeping.
    bool uses_sibling_combinators = false;
    // Set when any rule uses :nth-child / :first-child / :last-child etc.
    bool uses_structural_selectors = false;
    // These flags let the retained frame skip a complete tree traversal when
    // pseudo states and animations cannot alter layout.
    bool uses_layout_pseudo_states = false;
    bool uses_layout_animations = false;
    // Internal pseudo-element use is reduced to a bit mask so draw paths can
    // bypass selector resolution for parts that the stylesheet never styles.
    std::uint32_t used_internal_parts = 0;
    // Coarse target-pseudo index. It prevents labels, images, inputs, and
    // containers with no matching :state rule from entering the pseudo-style
    // resolver on every draw.
    std::uint64_t target_pseudo_selector_mask = 0;
    bool uses_unqualified_any_target_pseudo = false;
    std::unordered_set<std::string> target_pseudo_classes;
    std::unordered_set<std::size_t> target_pseudo_ids;
    bool uses_before_pseudo = false;
    bool uses_after_pseudo = false;
    std::uint64_t before_pseudo_selector_mask = 0;
    std::uint64_t after_pseudo_selector_mask = 0;
    bool uses_unqualified_before_pseudo = false;
    bool uses_unqualified_after_pseudo = false;
    std::unordered_set<std::string> before_pseudo_classes;
    std::unordered_set<std::string> after_pseudo_classes;
    std::unordered_set<std::size_t> before_pseudo_ids;
    std::unordered_set<std::size_t> after_pseudo_ids;
    bool uses_compositing = false;
    bool uses_backdrop_filters = false;
    // Coarse owner-effect index. Unrelated elements skip compositing style
    // resolution even when some other rule in the stylesheet uses effects.
    std::uint64_t compositing_selector_mask = 0;
    bool uses_unqualified_compositing = false;
    std::unordered_set<std::string> compositing_classes;
    std::unordered_set<std::size_t> compositing_ids;
    // Rules are indexed by target selector. Normal elements scan their type
    // plus wildcard rules instead of every rule in the stylesheet.
    std::array<std::vector<std::size_t>,32> rules_by_selector;
    std::vector<std::size_t> wildcard_rules;
    std::size_t revision = NextStyleSheetRevision();
};

inline const StyleSheet* CSS_UI_ACTIVE_STYLE_SHEET = nullptr;
inline bool CSS_UI_LAYOUT_CHANGE_REQUESTED = false;
inline bool CSS_UI_SCROLL_INPUT_CONSUMED = false;
// Prevent popup interactions from clicking controls drawn underneath.
inline bool CSS_UI_POINTER_INPUT_CONSUMED = false;
inline MouseCursor CSS_UI_REQUESTED_CURSOR = MOUSE_CURSOR_DEFAULT;
inline float CSS_UI_VIEWPORT_WIDTH = 0.0f;
// Temporary diagnostic switch — see the TraceLog call at the end of
// DrawUICompositedLayer in ui_effects.cpp. Set to true to get console
// output showing which compositing branch ran and the actual computed
// transform matrix for each composited element. Safe to delete once the
// perspective/transform disappearing bug is resolved.
inline bool CSS_UI_DEBUG_COMPOSITING = false;
inline float CSS_UI_VIEWPORT_HEIGHT = 0.0f;
inline float CSS_UI_ROOT_FONT_SIZE = 16.0f; // browser default (1rem = 16px)
// Non-zero only while an affected subtree is rendered into a local effect target.
// Scissor rectangles are framebuffer-space and therefore subtract this origin.
inline Vector2 CSS_UI_DRAW_TARGET_ORIGIN = {0.0f,0.0f};
// Retained-mode fast path. When false, layout changes must call
// MarkLayoutDirty()/UI::InvalidateLayout() instead of relying on a full-tree
// mutation hash every frame.
inline bool CSS_UI_DETECT_LAYOUT_MUTATIONS = true;

// -----------------------------------------------------------------
// UI traversal coordinate contexts
// -----------------------------------------------------------------
// Normal RUI uses identity screen coordinates. Viewport pushes a logical
// pointer context and a uniform 2D transform so existing controls can keep
// their retained logical rectangles while rendering/hit-testing through a
// transformed coordinate space. Stacks intentionally support nesting.
struct UIInputContext {
    Vector2 mouse_position{};
    Vector2 mouse_delta{};
};

struct UITransformContext {
    Vector2 offset{};
    float scale = 1.0f;
};

Vector2 GetUIMousePosition();
Vector2 GetUIMouseDelta();
void PushUIInputContext(const UIInputContext& context);
void PopUIInputContext();

UITransformContext GetCurrentUITransform();
void PushUITransform(Vector2 translation, float scale);
void PushUITransformAbsolute(const UITransformContext& transform);
void PopUITransform();

Vector2 UITransformPointToScreen(Vector2 point);
Vector2 UIScreenPointToLogical(Vector2 point);
Rectangle UITransformRectangleToScreen(Rectangle rectangle);
Rectangle UIScreenRectangleToLogical(Rectangle rectangle);
Rectangle CurrentUIVisibleLogicalScreenRect();
void BeginUIScissorMode(Rectangle logical_rectangle);

void TouchStyleSheet(StyleSheet& style_sheet);

std::uint32_t UIInternalPartMask(std::string_view part_class);
bool StyleSheetUsesPart(
    const StyleSheet& style_sheet,
    std::string_view part_class
);
bool StyleMayUseTargetPseudo(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& style
);

StyleSheet MakeDefaultUIStyleSheet();

template <typename Enum, std::size_t N>
inline const char* EnumToString(Enum value, const std::array<const char*, N>& names) {
    std::size_t index = static_cast<std::size_t>(value);

    if(index >= names.size()) {
        return "Unknown";
    }

    return names[index];
}

template <std::size_t N>
inline const char* IndexToString(std::size_t index, const std::array<const char*, N>& names) {
    if(index >= names.size()) {
        return "Unknown";
    }

    return names[index];
}

constexpr std::array<const char*, 29> value_type_names = {
    "Color", "string", "float", "int", "bool",
    "Vector2", "Vector3", "Vector4", "CSSLength",
    "DisplayType", "PositionType", "FlexDirection", "JustifyContent",
    "AlignItems", "AlignSelf", "TextAlign", "OverflowType",
    "GridTrackList", "GridAutoFlow", "UIImageFit", "UIImageSampling",
    "UIImageSliceMode", "UIImageBlendMode", "GeneratedContentValue",
    "DeferredCSSValue", "FilterList", "ShaderEffectList", "MixBlendMode", "TransformList"
};

//--------------------------------------------------------------------
// Input types
//--------------------------------------------------------------------

enum class InputType {
    Slider,
    Drag,
    Instant,
    Trigger,
    Checkbox,
    ColorPicker
};

enum class InputLabelLayout {
    Front, // Label follows the value/control.
    Back   // Label precedes the value/control.
};

enum class UISliderOrientation {
    Horizontal,
    Vertical
};

// Synthetic classes used internally to represent input pseudo-elements.
inline constexpr const char* CSS_INPUT_LABEL_PART ="__css-input-label";
inline constexpr const char* CSS_INPUT_VALUE_BOX_PART ="__css-input-value-box";
inline constexpr const char* CSS_INPUT_DECREMENT_BUTTON_PART ="__css-input-decrement-button";
inline constexpr const char* CSS_INPUT_INCREMENT_BUTTON_PART ="__css-input-increment-button";
inline constexpr const char* CSS_INPUT_SLIDER_TRACK_PART ="__css-input-slider-track";
inline constexpr const char* CSS_INPUT_SLIDER_FILL_PART ="__css-input-slider-fill";
inline constexpr const char* CSS_INPUT_SLIDER_HANDLE_PART ="__css-input-slider-handle";
// Vector and color component captions such as X/Y/Z/W and R/G/B/A.
inline constexpr const char* CSS_INPUT_COMPONENT_LABEL_PART ="__css-input-component-label";
inline constexpr const char* CSS_INPUT_COMPONENT_0_LABEL_PART ="__css-input-component-0-label";
inline constexpr const char* CSS_INPUT_COMPONENT_1_LABEL_PART ="__css-input-component-1-label";
inline constexpr const char* CSS_INPUT_COMPONENT_2_LABEL_PART ="__css-input-component-2-label";
inline constexpr const char* CSS_INPUT_COMPONENT_3_LABEL_PART ="__css-input-component-3-label";
inline constexpr const char* CSS_UI_HEADER_PART ="__css-ui-header";
inline constexpr const char* CSS_UI_POPUP_PART ="__css-ui-popup";
inline constexpr const char* CSS_UI_ITEM_PART ="__css-ui-item";
inline constexpr const char* CSS_UI_SCROLLBAR_PART ="__css-ui-scrollbar";
inline constexpr const char* CSS_UI_SCROLLBAR_THUMB_PART ="__css-ui-scrollbar-thumb";
inline constexpr const char* CSS_UI_RANGE_TRACK_PART ="__css-ui-range-track";
inline constexpr const char* CSS_UI_RANGE_FILL_PART ="__css-ui-range-fill";
inline constexpr const char* CSS_UI_RANGE_HANDLE_PART ="__css-ui-range-handle";
inline constexpr const char* CSS_UI_TAB_PART ="__css-ui-tab";
inline constexpr const char* CSS_UI_TAB_CONTENT_PART ="__css-ui-tab-content";
inline constexpr const char* CSS_UI_SPLITTER_HANDLE_PART ="__css-ui-splitter-handle";

//--------------------------------------------------------------------
// Debug names
//--------------------------------------------------------------------


constexpr std::array<const char*, 131> property_names = {
    "background-color",
    "border-color",
    "border-width",
    "border-top-width",
    "border-right-width",
    "border-bottom-width",
    "border-left-width",
    "border-style",
    "border",
    "border-radius",
    "width",
    "height",
    "min-width",
    "max-width",
    "min-height",
    "max-height",
    "padding",
    "padding-left",
    "padding-right",
    "padding-top",
    "padding-bottom",
    "margin",
    "margin-left",
    "margin-right",
    "margin-top",
    "margin-bottom",
    "color",
    "font-family",
    "font-size",
    "display",
    "gap",
    "row-gap",
    "column-gap",
    "opacity",
    "position",
    "left",
    "right",
    "top",
    "bottom",
    "flex-direction",
    "justify-content",
    "align-items",
    "justify-items",
    "align-self",
    "flex-grow",
    "flex-shrink",
    "flex-basis",
    "text-align",
    "overflow",
    "grid-template-columns",
    "grid-template-rows",
    "grid-auto-columns",
    "grid-auto-rows",
    "grid-auto-flow",
    "grid-column",
    "grid-row",
    "grid-column-span",
    "grid-row-span",
    "step-buttons",
    "step",
    "value-box",
    "drag-speed",
    "slider-show-value",
    "slider-value-width",
    "slider-click-to-set",
    "runtime-movable",
    "runtime-resizable",
    "runtime-constrain",
    "runtime-lock-x",
    "runtime-lock-y",
    "runtime-snap",
    "resize-to-content",
    "scroll-speed",
    "scrollbar-size",
    "scale",
    "header-height",
    "item-height",
    "popup-width",
    "popup-max-height",
    "tooltip-delay",
    "indent",
    "resizable",
    "closable",
    "collapsible",
    "object-fit",
    "image-rendering",
    "image-tint",
    "image-flip-x",
    "image-flip-y",
    "splitter-size",
    "tab-height",
    "tab-closable",
    "object-position-x",
    "object-position-y",
    "image-rotation",
    "image-pivot-x",
    "image-pivot-y",
    "image-frame",
    "image-frame-width",
    "image-frame-height",
    "image-frame-columns",
    "image-slice-left",
    "image-slice-right",
    "image-slice-top",
    "image-slice-bottom",
    "image-slice-scale",
    "image-slice-mode",
    "image-tile-scale",
    "image-tile-offset-x",
    "image-tile-offset-y",
    "image-blend-mode",
    "image-pixel-snap",
    "content",
    "filter",
    "backdrop-filter",
    "effect",
    "mix-blend-mode",
    "transform",
    "perspective",
    "background",
    "font",
    "inset",
    "flex",
    "place-items",
    "flex-wrap",
    "flex-flow",
    "align-content",
    "justify-self",
    "transform-origin-x",
    "transform-origin-y",
    "transform-origin"
};

//--------------------------------------------------------------------
// Shared style data
//--------------------------------------------------------------------

struct EdgeValues {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    EdgeValues() = default;

    explicit EdgeValues(float value)
        : left(value),
          right(value),
          top(value),
          bottom(value) {}
};

struct EdgeLengthValues {
    std::optional<CSSLength> left;
    std::optional<CSSLength> right;
    std::optional<CSSLength> top;
    std::optional<CSSLength> bottom;
};

// Active font registry used by layout and drawing.
// Call SetUIFontRegistry(ui.fonts) after loading fonts; DrawElements also does this automatically.
inline const std::unordered_map<std::string,Font>* CSS_UI_ACTIVE_FONTS = nullptr;

void SetUIFontRegistry(const std::unordered_map<std::string,Font>& fonts);

Font ResolveUIFont(const std::string& name);

// Draws text on whole-pixel coordinates. The glyph atlas is sampled with a
// bilinear filter, so a fractional position (for example the vertical-centering
// half pixel) smears small text. Every text draw goes through this.
inline void DrawUIText(
    Font font,
    const char* text,
    Vector2 position,
    float font_size,
    float spacing,
    Color tint
) {
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    DrawTextEx(font,text,position,font_size,spacing,tint);
}

void ClearTextMeasureCache();

bool AddUIFont(
    std::unordered_map<std::string,Font>& fonts,
    const std::string& name,
    const std::string& path,
    int base_size = 32,
    // Codepoints to rasterise. Empty = default set (ASCII, Latin-1, Latin
    // Extended-A, common punctuation, arrows, symbols).
    const std::vector<int>& codepoints = {}
);

std::vector<int> DefaultUICodepoints();

// Vertical extent of a font's glyph ink relative to the text origin (the y
// passed to DrawTextEx), scaled to `font_size`. `top` is the top of tall
// letters/ascenders, `bottom` the bottom of descenders. Carets and selection
// rectangles use this instead of assuming the box spans [0,font_size].
struct UIFontVerticalExtent {
    float top = 0.0f;
    float bottom = 0.0f;
    float Height() const { return bottom-top; }
};

UIFontVerticalExtent MeasureFontVerticalExtent(Font font,float font_size);

void UnloadUIFonts(std::unordered_map<std::string,Font>& fonts);

struct ComputedStyle {
    Color background_color = BLANK;
    Color border_color = BLANK;
    Color text_color = Color{0,0,0,255};
    Matrix transform_matrix=MatrixIdentity();
    // Retained for compatibility with existing widget metrics. It mirrors
    // the maximum side width while border_widths stores CSS per-side values.
    float border_width = 0.0f;
    EdgeValues border_widths;
    UIBorderStyle border_style = UIBorderStyle::Solid;
    // RCSS compatibility: unitless values in [0,1] retain raylib-style
    // normalized roundness. Pixel values are stored as negative numbers by
    // the parser so e.g. 1px is distinguishable from legacy roundness 1.0.
    // Values > 1 from the C++ fluent API are also treated as pixel radii.
    float border_radius = 0.0f;
    // Unitless local scale from CSS. The applied values are tracked so a
    // repeated layout pass can restore unscaled metrics before resolving.
    float scale = 1.0f;
    float applied_scale = 1.0f;
    float applied_inherited_scale = 1.0f;
    bool applied_relative_reference_includes_local_scale = false;
    float width = 200.0f;
    float height = 100.0f;
    float min_width = 0.0f;
    float max_width = std::numeric_limits<float>::max();
    float min_height = 0.0f;
    float max_height = std::numeric_limits<float>::max();

    float font_size = 16.0f;
    float gap = 0.0f;
    float row_gap = 0.0f;
    float column_gap = 0.0f;
    float opacity = 1.0f;
    float scroll_speed = 36.0f;
    float scrollbar_size = 15.0f;
    float header_height = 30.0f;
    float item_height = 28.0f;
    float popup_width = 220.0f;
    float popup_max_height = 280.0f;
    float tooltip_delay = 0.35f;
    float indent = 18.0f;
    float splitter_size = 6.0f;
    float tab_height = 32.0f;
    Color image_tint = WHITE;
    UIImageFit image_fit = UIImageFit::Stretch; // object-fit: fill
    UIImageSampling image_sampling = UIImageSampling::Bilinear;
    bool image_flip_x = false;
    bool image_flip_y = false;
    Vector2 image_position = {0.5f,0.5f};
    float image_rotation = 0.0f;
    Vector2 image_pivot = {0.5f,0.5f};
    int image_frame = 0;
    Vector2 image_frame_size = {};
    int image_frame_columns = 0;
    EdgeValues image_slice;
    float image_slice_scale = 1.0f;
    UIImageSliceMode image_slice_mode = UIImageSliceMode::Stretch;
    float image_tile_scale = 1.0f;
    Vector2 image_tile_offset = {};
    UIImageBlendMode image_blend_mode = UIImageBlendMode::Alpha;
    bool image_pixel_snap = false;

    // Generated pseudo-element content. The explicit flag distinguishes
    // content: "" from no content declaration.
    std::string generated_content;
    bool generated_content_defined = false;

    // Effect data is pointer-backed so the ordinary fast path adds only cheap
    // null checks and never owns per-element vectors.
    UIFilterListRef filters;
    UIFilterListRef backdrop_filters;
    UIShaderEffectListRef shader_effects;
    UIShaderUniformListRef shader_uniforms;
    UIMixBlendMode mix_blend_mode = UIMixBlendMode::Normal;
    UITransformListRef transform_operations;
    // Origin for scale/rotate/skew/3D transforms, as a length/percentage of the
    // element's own box. Unset (nullopt), same convention as left_length/
    // top_length/etc., means "not specified" — resolved to the 50%/50% center
    // default at the point of use in EvaluateTransformMatrix.
    std::optional<CSSLength> transform_origin_x_length;
    std::optional<CSSLength> transform_origin_y_length;

    // Custom properties inherit. Maps are shared and only materialized for
    // stylesheets/elements that actually use --variables.
    std::shared_ptr<const UIStyleVariableMap> inherited_custom_properties;
    // font-family inherits like in CSS: seeded from the parent's computed font
    // and overridden only when a matching rule sets font-family.
    std::string inherited_font;
    // color inherits too. `*_inherited` records that the current value came
    // from inheritance (so it follows the parent) rather than from a rule or
    // the C++ fluent API (which stay in force).
    std::optional<Color> inherited_text_color;
    // Set by the C++ fluent API so an explicit colour is not replaced by the
    // inherited one.
    bool text_color_explicit = false;
    // 1-based position among siblings and sibling count. Only filled in when
    // the stylesheet uses structural selectors (0 otherwise).
    std::size_t sibling_index = 0;
    std::size_t sibling_count = 0;
    std::shared_ptr<const UIStyleVariableMap> custom_properties;
    // Non-inherited descriptor-driven RCSS values such as Graph.grid-color.
    // Stored as text so the component registry remains the authoritative type
    // converter/validator and the animation engine can interpolate them.
    std::shared_ptr<const UIStyleVariableMap> component_style_properties;

    std::optional<CSSLength> border_width_length;
    EdgeLengthValues border_width_lengths;
    std::optional<CSSLength> width_length;
    std::optional<CSSLength> height_length;
    std::optional<CSSLength> min_width_length;
    std::optional<CSSLength> max_width_length;
    std::optional<CSSLength> min_height_length;
    std::optional<CSSLength> max_height_length;
    std::optional<CSSLength> font_size_length;
    std::optional<CSSLength> gap_length;
    std::optional<CSSLength> row_gap_length;
    std::optional<CSSLength> column_gap_length;
    std::optional<CSSLength> scrollbar_size_length;
    std::optional<CSSLength> header_height_length;
    std::optional<CSSLength> item_height_length;
    std::optional<CSSLength> popup_width_length;
    std::optional<CSSLength> popup_max_height_length;
    std::optional<CSSLength> indent_length;
    std::optional<CSSLength> splitter_size_length;
    std::optional<CSSLength> tab_height_length;
    std::optional<CSSLength> perspective;
    PositionType position = PositionType::Static;
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    std::optional<CSSLength> left_length;
    std::optional<CSSLength> right_length;
    std::optional<CSSLength> top_length;
    std::optional<CSSLength> bottom_length;

    std::string font="default";    
    FlexDirection flex_direction = FlexDirection::Row;
    FlexWrap flex_wrap = FlexWrap::NoWrap;
    JustifyContent justify_content = JustifyContent::FlexStart;
    AlignItems align_items = AlignItems::FlexStart;
    AlignItems justify_items = AlignItems::FlexStart;
    AlignSelf align_self = AlignSelf::Auto;
    // nullopt preserves pre-0.7.1 grid track placement. Flex wrapping uses
    // CSS's default stretch behavior when align-content is omitted.
    std::optional<AlignContent> align_content;
    JustifySelf justify_self = JustifySelf::Auto;
    float flex_grow = 0.0f;
    float flex_shrink = 1.0f;
    float flex_basis = -1.0f;
    std::optional<CSSLength> flex_basis_length;
    TextAlign text_align = TextAlign::Left;
    OverflowType overflow = OverflowType::Visible;
    int grid_template_columns = 1;
    int grid_template_rows = 0;
    GridTrackList grid_column_tracks = {GridTrackSize{1.0f,GridTrackUnit::Fraction}};
    GridTrackList grid_row_tracks;
    GridTrackSize grid_auto_column = {1.0f,GridTrackUnit::Fraction};
    GridTrackSize grid_auto_row = {1.0f,GridTrackUnit::Fraction};
    GridAutoFlow grid_auto_flow = GridAutoFlow::Row;
    int grid_column = 0;
    int grid_row = 0;
    int grid_column_span = 1;
    int grid_row_span = 1;
    // Optional so C++ settings remain unchanged when CSS omits them.
    std::optional<bool> input_step_buttons;
    std::optional<float> input_step;
    std::optional<bool> input_value_box;
    std::optional<float> input_drag_speed;
    std::optional<bool> input_slider_show_value;
    std::optional<float> input_slider_value_width;
    std::optional<bool> input_slider_click_to_set;
    // Runtime editor constraints. Omitted values use the runtime layer defaults.
    std::optional<bool> runtime_movable;
    std::optional<bool> runtime_resizable;
    std::optional<bool> runtime_constrain;
    std::optional<bool> runtime_lock_x;
    std::optional<bool> runtime_lock_y;
    std::optional<float> runtime_snap;
    // Child-bearing widgets can opt into content-driven sizing.
    std::optional<bool> resize_to_content;
    std::optional<bool> resizable;
    std::optional<bool> closable;
    std::optional<bool> collapsible;
    std::optional<bool> tab_closable;
    std::vector<UITransitionSpec> transitions;
    std::vector<UIAnimationSpec> animations;
    CSS_Id id = std::monostate{};
    std::vector<std::string> classes;
    // Runtime selector identity and selector context used by descendant,
    // child, adjacent-sibling, and general-sibling combinators.
    SelectorType selector = SelectorType::Any;
    std::vector<StyleSelector> selector_ancestors;
    std::vector<StyleSelectorSiblingChain>
        selector_ancestor_previous_siblings;
    StyleSelectorSiblingChain selector_previous_siblings;
    DisplayType display = DisplayType::Inline;
    EdgeValues padding;
    EdgeValues margin;
    EdgeLengthValues padding_lengths;
    EdgeLengthValues margin_lengths;
};

//--------------------------------------------------------------------
// Runtime caches
//--------------------------------------------------------------------

void HashCombine(std::size_t& seed, std::size_t value);

std::size_t HashFloat(float value);

std::size_t HashColor(Color color);

std::size_t HashCSSLength(const CSSLength& length);
//--------------------------------------------------------------------
// CSS ID utilities
//--------------------------------------------------------------------

bool HasCSSId(const CSS_Id& id);

bool HasCSSClass(const std::vector<std::string>& classes, const std::string& class_name);

void AddCSSClass(std::vector<std::string>& classes, const std::string& class_name);

bool RemoveCSSClass(std::vector<std::string>& classes, const std::string& class_name);

bool ToggleCSSClass(std::vector<std::string>& classes, const std::string& class_name);

void ClearCSSClasses(std::vector<std::string>& classes);

Color WithOpacity(Color color, float opacity);

Color MixColors(Color first, Color second, float amount);

Color LightenColor(Color color, float amount);

Color DarkenColor(Color color, float amount);

Color ApplyStyleOpacity(Color color, float opacity);


template <typename T, typename HashValue>
inline void HashOptional(
    std::size_t& seed,
    const std::optional<T>& value,
    HashValue hash_value
) {
    HashCombine(seed,value.has_value() ? 1u : 0u);
    if(value.has_value()) {
        HashCombine(seed,hash_value(*value));
    }
}

std::size_t HashCSSId(const CSS_Id& id);

std::size_t HashStyleSelector(const StyleSelector& selector);

StyleSelectorSiblingChain AppendStyleSelectorSibling(
    StyleSelectorSiblingChain previous,
    StyleSelector selector
);

std::size_t HashComputedStyle(const ComputedStyle& style);

struct ResolvedStyleCacheKey {
    const StyleSheet* style_sheet = nullptr;
    std::size_t revision = 0;
    std::size_t base_hash = 0;
    std::size_t pseudo_hash = 0;
    SelectorType selector = SelectorType::Any;
    SelectorState state = SelectorState::None;
    bool pseudo_states = false;

    bool operator==(const ResolvedStyleCacheKey& other) const {
        return
            style_sheet == other.style_sheet &&
            revision == other.revision &&
            base_hash == other.base_hash &&
            pseudo_hash == other.pseudo_hash &&
            selector == other.selector &&
            state == other.state &&
            pseudo_states == other.pseudo_states;
    }
};

struct ResolvedStyleCacheKeyHash {
    std::size_t operator()(const ResolvedStyleCacheKey& key) const {
        std::size_t seed =
            std::hash<const StyleSheet*>{}(key.style_sheet);

        HashCombine(seed,key.revision);
        HashCombine(seed,key.base_hash);
        HashCombine(seed,key.pseudo_hash);
        HashCombine(seed,static_cast<std::size_t>(key.selector));
        HashCombine(seed,static_cast<std::size_t>(key.state));
        HashCombine(seed,std::hash<bool>{}(key.pseudo_states));
        return seed;
    }
};

inline std::unordered_map<
    ResolvedStyleCacheKey,
    ComputedStyle,
    ResolvedStyleCacheKeyHash
> CSS_UI_RESOLVED_STYLE_CACHE;

void ClearResolvedStyleCache();

std::size_t HashTextMeasureKey(
    Font font,
    std::string_view text,
    float font_size,
    float spacing
);

struct TextMeasureCacheEntry {
    unsigned int texture_id = 0;
    int base_size = 0;
    float font_size = 0.0f;
    float spacing = 0.0f;
    std::string text;
    Vector2 measured = {};
};

inline std::unordered_multimap<
    std::size_t,
    TextMeasureCacheEntry
> CSS_UI_TEXT_MEASURE_CACHE;

void ClearTextMeasureCache();

Vector2 MeasureTextCached(
    Font font,
    const char* text,
    float font_size,
    float spacing
);

//--------------------------------------------------------------------
// Commands, drag/drop, and element base
//--------------------------------------------------------------------

class UIUndoStack {
public:
    struct Command {
        std::string label;
        std::string merge_key;
        std::function<void()> undo;
        std::function<void()> redo;
        std::function<bool(Command&)> merge;
    };

    explicit UIUndoStack(std::size_t capacity = 256);
    void Push(Command command, bool execute_redo = false);
    bool Undo();
    bool Redo();
    void Clear();
    bool CanUndo() const;
    bool CanRedo() const;
    const std::string& UndoLabel() const;
    const std::string& RedoLabel() const;
    std::size_t UndoCount() const;
    std::size_t RedoCount() const;
    void BeginTransaction(std::string label);
    void EndTransaction();
    void CancelTransaction();

    template <typename T>
    void PushValue(
        T& target,
        T before,
        T after,
        std::string label,
        std::string merge_key = {}
    ) {
        if(before == after) return;
        T* pointer = &target;
        Command command;
        command.label = std::move(label);
        command.merge_key = std::move(merge_key);
        command.undo = [pointer,before]() { *pointer = before; };
        command.redo = [pointer,after]() { *pointer = after; };
        command.merge = [pointer,after](Command& previous) {
            if(!previous.redo) return false;
            previous.redo = [pointer,after]() { *pointer = after; };
            return true;
        };
        Push(std::move(command),false);
    }

private:
    std::size_t capacity_ = 256;
    std::vector<Command> undo_;
    std::vector<Command> redo_;
    bool transaction_open_ = false;
    std::string transaction_label_;
    std::vector<Command> transaction_commands_;
    mutable std::string empty_label_;
};

struct UIDragPayload {
    std::string type;
    std::string source_key;
    std::string text;
    std::any value;

    template <typename T>
    const T* Get() const { return std::any_cast<T>(&value); }
    template <typename T>
    T* Get() { return std::any_cast<T>(&value); }
};

struct UIDragSourceBinding {
    bool enabled = true;
    std::function<void(bool&)> update_enabled;
    std::function<UIDragPayload()> make_payload;
    std::function<void()> on_begin;
    std::function<void(bool)> on_end;
    std::weak_ptr<void> lifetime;
    bool lifetime_tracked = false;
    float threshold = 4.0f;
    int mouse_button = MOUSE_BUTTON_LEFT;
    bool pressed_inside = false;
    Vector2 press_origin = {};
};

struct UIDropTargetBinding {
    bool enabled = true;
    std::function<void(bool&)> update_enabled;
    std::function<bool(const UIDragPayload&)> accepts;
    std::function<void(const UIDragPayload&)> on_preview;
    std::function<void(const UIDragPayload&)> on_drop;
    std::weak_ptr<void> lifetime;
    bool lifetime_tracked = false;
};

struct UIDragDropPreview {
    Rectangle rectangle = {};
    std::string target_key;
    bool accepted = false;
};

class UIDragDropContext {
public:
    void BeginFrame();
    void EndFrame();
    bool BeginDrag(
        std::string source_key,
        UIDragPayload payload,
        Vector2 pointer,
        std::function<void(bool)> on_end = {}
    );
    void ConsiderTarget(
        std::string target_key,
        Rectangle rectangle,
        UIDropTargetBinding& target
    );
    void Cancel();
    bool Dragging() const { return dragging_; }
    const UIDragPayload* Payload() const { return dragging_ ? &payload_ : nullptr; }
    const UIDragDropPreview& Preview() const { return preview_; }
    Vector2 Pointer() const { return pointer_; }

private:
    bool dragging_ = false;
    bool released_this_frame_ = false;
    UIDragPayload payload_;
    std::string source_key_;
    Vector2 pointer_ = {};
    UIDragDropPreview preview_;
    std::optional<UIDropTargetBinding> preview_target_;
    std::function<void(bool)> on_end_;
};

UIDragDropContext& GetUIDragDropContext();

struct UIElementAnimationState;
struct UILayoutAnimationState;

struct UIElementBase;
struct UIGeneratedPseudoElementState;
void UpdateElementDragDrop(UIElementBase& element);

enum class UIDirtyFlags : std::uint32_t {
    None = 0,
    Style = 1u << 0,
    Layout = 1u << 1,
    Paint = 1u << 2,
    Composite = 1u << 3,
    EffectParameters = 1u << 4,
    PseudoElements = 1u << 5,
    All = 0xffffffffu
};
inline UIDirtyFlags operator|(UIDirtyFlags a, UIDirtyFlags b) {
    return static_cast<UIDirtyFlags>(static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b));
}
inline UIDirtyFlags& operator|=(UIDirtyFlags& a, UIDirtyFlags b) { a=a|b; return a; }
inline bool HasUIDirtyFlag(UIDirtyFlags value, UIDirtyFlags flag) {
    return (static_cast<std::uint32_t>(value)&static_cast<std::uint32_t>(flag))!=0;
}

// Central property invalidation classification. New RCSS properties should be
// added here rather than scattering layout/paint/composite decisions.
UIDirtyFlags UIPropertyDirtyFlags(PropertyType property);

struct UIElementBase {
    Rectangle transform = {
        0.0f,
        0.0f,
        200.0f,
        100.0f
    };

    // CSS/flex/grid output before the persisted runtime override is applied.
    Rectangle layout_transform = transform;
    Vector2 runtime_offset = {0.0f,0.0f};
    Vector2 runtime_size = {0.0f,0.0f};
    bool runtime_size_override = false;
    std::string key;
    std::string tooltip;
    double tooltip_hover_started = -1.0;
    bool inherited_disabled = false;
    UIPseudoStateSet pseudo_states;
    std::shared_ptr<UIDragSourceBinding> drag_source;
    std::shared_ptr<UIDropTargetBinding> drop_target;
    // Opaque per-widget payload. Markup `userData="{...}"` stores a UIXValue
    // here; read it back with rui::Context::FocusedUserData().
    std::any user_data;
    // Focus transition callbacks wired from markup onFocus/onBlur. They are
    // fired by rui::Context::Update when the focused widget changes.
    std::function<void()> on_focus;
    std::function<void()> on_blur;
    // Wired from markup onKey. Polls this widget's key bindings and runs the
    // matching handler; returns true if one fired. rui::Context::Update calls
    // it once per frame, only for the focused widget.
    std::function<bool()> on_key;
    std::size_t last_layout_pseudo_hash = 0;
    // Incremented whenever layout resolves a new base style. Draw-time
    // pseudo-style caching uses this instead of hashing the full style object.
    std::size_t style_generation = 1;
    std::size_t cached_interactive_style_generation = 0;
    std::size_t cached_interactive_sheet_revision = 0;
    std::size_t cached_interactive_pseudo_hash = 0;
    std::size_t cached_interactive_overlay_hash = 0;
    // Monotonic identity for the cached target style. Animation channels use
    // it instead of hashing transition/animation vectors every frame.
    std::size_t cached_interactive_style_revision = 1;
    SelectorType cached_interactive_selector = SelectorType::Any;
    std::shared_ptr<ComputedStyle> cached_interactive_style;
    // Shared so moving widgets in vectors does not invalidate active tweens.
    std::shared_ptr<UIElementAnimationState> animation_state;
    std::shared_ptr<UILayoutAnimationState> layout_animation_state;
    // Lazily created only when the stylesheet actually generates ::before/after
    // or this element uses an effect.
    std::shared_ptr<UIGeneratedPseudoElementState> generated_pseudo_state;
    UIDirtyFlags dirty_flags = UIDirtyFlags::All;

    UIElementBase() = default;

    explicit UIElementBase(Rectangle initial_transform)
        : transform(initial_transform),
          layout_transform(initial_transform) {}
};

struct UIGeneratedPseudoElement {
    UIElementBase element;
    ComputedStyle style;
    Rectangle paint_rect = {};
    bool active = false;
    std::size_t owner_style_generation = 0;
    std::size_t sheet_revision = 0;
    std::size_t pseudo_hash = 0;
};

struct UIGeneratedPseudoElementState {
    std::shared_ptr<UIGeneratedPseudoElement> before;
    std::shared_ptr<UIGeneratedPseudoElement> after;
};

template <typename Derived>
struct UIStyleFluent {
    Derived& Id(std::string value) {
        Self().style.id = std::move(value);
        return Self();
    }

    Derived& Key(std::string value) {
        Self().rect.key = std::move(value);
        return Self();
    }

    Derived& Class(std::string value) {
        auto& classes = Self().style.classes;

        if(std::find(classes.begin(),classes.end(),value) == classes.end()) {
            classes.push_back(std::move(value));
        }

        return Self();
    }

    Derived& Classes(std::initializer_list<std::string> values) {
        for(const std::string& value : values) {
            Class(value);
        }

        return Self();
    }

    Derived& RemoveClass(const std::string& value) {
        auto& classes = Self().style.classes;
        classes.erase(
            std::remove(classes.begin(),classes.end(),value),
            classes.end()
        );
        return Self();
    }

    Derived& ClearClasses() {
        Self().style.classes.clear();
        return Self();
    }

    bool HasClass(const std::string& value) const {
        const auto& classes = Self().style.classes;
        return std::find(classes.begin(),classes.end(),value) != classes.end();
    }

    Derived& PseudoState(std::string name, bool active = true) {
        Self().rect.pseudo_states.Set(name,active);
        return Self();
    }

    Derived& DefinePseudoState(
        std::string name,
        bool initial_value,
        std::function<void(bool&)> update
    ) {
        Self().rect.pseudo_states.Define(
            std::move(name),initial_value,std::move(update)
        );
        return Self();
    }

    Derived& DefinePseudoStateTracked(
        std::string name,
        bool initial_value,
        std::weak_ptr<void> lifetime,
        std::function<void(bool&)> update
    ) {
        Self().rect.pseudo_states.DefineTracked(
            std::move(name),initial_value,std::move(lifetime),std::move(update)
        );
        return Self();
    }

    Derived& DragSource(UIDragSourceBinding binding) {
        Self().rect.drag_source = std::make_shared<UIDragSourceBinding>(std::move(binding));
        return Self();
    }

    Derived& DropTarget(UIDropTargetBinding binding) {
        Self().rect.drop_target = std::make_shared<UIDropTargetBinding>(std::move(binding));
        return Self();
    }

    Derived& TogglePseudoState(const std::string& name) {
        Self().rect.pseudo_states.Toggle(name);
        return Self();
    }

    Derived& RemovePseudoState(const std::string& name) {
        Self().rect.pseudo_states.Remove(name);
        return Self();
    }

    bool HasPseudoState(const std::string& name) const {
        return Self().rect.pseudo_states.Active(name);
    }

    Derived& Width(float value) {
        Self().style.width = value;
        Self().style.width_length = CSSLength{value,LengthUnit::Pixels};
        return Self();
    }

    Derived& Height(float value) {
        Self().style.height = value;
        Self().style.height_length = CSSLength{value,LengthUnit::Pixels};
        return Self();
    }

    Derived& WidthPercent(float value) {
        Self().style.width_length = CSSLength{value,LengthUnit::Percent};
        return Self();
    }

    Derived& HeightPercent(float value) {
        Self().style.height_length = CSSLength{value,LengthUnit::Percent};
        return Self();
    }

    Derived& WidthViewport(float value) {
        Self().style.width_length = CSSLength{value,LengthUnit::ViewWidth};
        return Self();
    }

    Derived& HeightViewport(float value) {
        Self().style.height_length = CSSLength{value,LengthUnit::ViewHeight};
        return Self();
    }

    Derived& Size(float width, float height) {
        Width(width);
        Height(height);
        return Self();
    }

    Derived& MinSize(float width, float height) {
        Self().style.min_width = width;
        Self().style.min_height = height;
        Self().style.min_width_length = CSSLength{width,LengthUnit::Pixels};
        Self().style.min_height_length = CSSLength{height,LengthUnit::Pixels};
        return Self();
    }

    Derived& MaxSize(float width, float height) {
        Self().style.max_width = width;
        Self().style.max_height = height;
        Self().style.max_width_length = CSSLength{width,LengthUnit::Pixels};
        Self().style.max_height_length = CSSLength{height,LengthUnit::Pixels};
        return Self();
    }

    Derived& AutoWidth(bool enabled = true) {
        if(enabled) {
            Self().style.width_length = CSSLength{0.0f,LengthUnit::Auto};
        }
        else if(
            Self().style.width_length.has_value() &&
            Self().style.width_length->unit == LengthUnit::Auto
        ) {
            Self().style.width_length.reset();
        }

        return Self();
    }

    Derived& AutoHeight(bool enabled = true) {
        if(enabled) {
            Self().style.height_length = CSSLength{0.0f,LengthUnit::Auto};
        }
        else if(
            Self().style.height_length.has_value() &&
            Self().style.height_length->unit == LengthUnit::Auto
        ) {
            Self().style.height_length.reset();
        }

        return Self();
    }

    Derived& AutoSize(bool enabled = true) {
        Self().style.resize_to_content = enabled;
        AutoWidth(enabled);
        AutoHeight(enabled);
        return Self();
    }

    Derived& ResizeToContent(bool enabled = true) {
        Self().style.resize_to_content = enabled;
        return Self();
    }

    Derived& Padding(float value) {
        Self().style.padding = EdgeValues(value);
        const CSSLength length{value,LengthUnit::Pixels};
        Self().style.padding_lengths = {length,length,length,length};
        return Self();
    }

    Derived& Margin(float value) {
        Self().style.margin = EdgeValues(value);
        const CSSLength length{value,LengthUnit::Pixels};
        Self().style.margin_lengths = {length,length,length,length};
        return Self();
    }

    Derived& Gap(float value) {
        Self().style.gap = value;
        Self().style.row_gap = value;
        Self().style.column_gap = value;
        const CSSLength length{value,LengthUnit::Pixels};
        Self().style.gap_length = length;
        Self().style.row_gap_length = length;
        Self().style.column_gap_length = length;
        return Self();
    }

    Derived& Flex(FlexDirection direction = FlexDirection::Column) {
        Self().style.display = DisplayType::Flex;
        Self().style.flex_direction = direction;
        return Self();
    }

    Derived& Wrap(FlexWrap value = FlexWrap::Wrap) {
        Self().style.flex_wrap = value;
        return Self();
    }

    Derived& AlignContentMode(AlignContent value) {
        Self().style.align_content = value;
        return Self();
    }

    Derived& JustifySelfMode(JustifySelf value) {
        Self().style.justify_self = value;
        return Self();
    }

    Derived& Display(DisplayType value) {
        Self().style.display = value;
        return Self();
    }

    Derived& Overflow(OverflowType value) {
        Self().style.overflow = value;
        return Self();
    }

    Derived& Scrollable(bool enabled = true) {
        Self().style.overflow =
            enabled ? OverflowType::Auto : OverflowType::Visible;
        return Self();
    }

    Derived& Grow(float value = 1.0f) {
        Self().style.flex_grow = value;
        return Self();
    }

    Derived& Align(AlignSelf value) {
        Self().style.align_self = value;
        return Self();
    }

    Derived& Absolute(float left, float top) {
        Self().style.position = PositionType::Absolute;
        Self().style.left = left;
        Self().style.top = top;
        Self().style.left_length = CSSLength{left,LengthUnit::Pixels};
        Self().style.top_length = CSSLength{top,LengthUnit::Pixels};
        return Self();
    }

    Derived& Background(Color value) {
        Self().style.background_color = value;
        return Self();
    }

    Derived& Foreground(Color value) {
        Self().style.text_color = value;
        Self().style.text_color_explicit = true;
        return Self();
    }

    Derived& Border(
        Color color,
        float width = 1.0f,
        float radius = 0.0f
    ) {
        Self().style.border_color = color;
        Self().style.border_width = width;
        Self().style.border_widths = EdgeValues(width);
        Self().style.border_style = UIBorderStyle::Solid;
        Self().style.border_width_length =
            CSSLength{width,LengthUnit::Pixels};
        Self().style.border_width_lengths.left = Self().style.border_width_length;
        Self().style.border_width_lengths.right = Self().style.border_width_length;
        Self().style.border_width_lengths.top = Self().style.border_width_length;
        Self().style.border_width_lengths.bottom = Self().style.border_width_length;
        Self().style.border_radius = radius;
        return Self();
    }

    Derived& Visible(bool visible = true) {
        Self().style.display =
            visible ? DisplayType::Block : DisplayType::None;
        return Self();
    }

private:
    Derived& Self() {
        return static_cast<Derived&>(*this);
    }

    const Derived& Self() const {
        return static_cast<const Derived&>(*this);
    }
};

struct UISelectable : UIStyleFluent<UISelectable> {
    UIElementBase rect;
    ComputedStyle style;
    std::string text = "Selectable";
    bool selected = false;
    bool toggle_on_click = true;
    bool disabled = false;
    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool clicked = false;
    bool pressed_inside = false;
    std::function<void(UISelectable&)> on_click;

    UISelectable& Text(std::string value) { text = std::move(value); return *this; }
    UISelectable& Id(std::string value) { style.id = std::move(value); return *this; }
    UISelectable& Selected(bool value = true) { selected = value; return *this; }
    UISelectable& ToggleOnClick(bool value = true) { toggle_on_click = value; return *this; }
    UISelectable& Disabled(bool value = true) { disabled = value; return *this; }
    UISelectable& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UISelectable& OnClick(std::function<void(UISelectable&)> callback) {
        on_click = std::move(callback);
        return *this;
    }
};

struct UIComboBox : UIStyleFluent<UIComboBox> {
    UIElementBase rect;
    ComputedStyle style;
    std::string placeholder = "Select...";
    std::vector<std::string> items;
    // Optional per-item CSS classes (parallel to `items`, may be shorter).
    // Style them with `.my-class::item { ... }`. Every item also receives
    // `option-<index>` (0-based) so items can be targeted by position.
    std::vector<std::vector<std::string>> item_classes;
    int selected_index = -1;
    int hovered_index = -1;
    bool open = false;
    bool disabled = false;
    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool changed = false;
    bool pressed_inside = false;
    float scroll_offset = 0.0f;
    std::function<void(UIComboBox&)> on_change;

    UIComboBox& Placeholder(std::string value) { placeholder = std::move(value); return *this; }
    UIComboBox& Items(std::vector<std::string> value) { items = std::move(value); return *this; }
    UIComboBox& AddItem(std::string value) { items.push_back(std::move(value)); return *this; }
    UIComboBox& AddItem(std::string value,std::vector<std::string> classes) {
        item_classes.resize(items.size());
        items.push_back(std::move(value));
        item_classes.push_back(std::move(classes));
        return *this;
    }
    UIComboBox& SelectedIndex(int value) { selected_index = value; return *this; }
    UIComboBox& Open(bool value = true) { open = value; return *this; }
    UIComboBox& Disabled(bool value = true) { disabled = value; return *this; }
    UIComboBox& Id(std::string value) { style.id = std::move(value); return *this; }
    UIComboBox& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIComboBox& OnChange(std::function<void(UIComboBox&)> callback) {
        on_change = std::move(callback);
        return *this;
    }
};

template <typename T>
struct UIRangeInput : UIStyleFluent<UIRangeInput<T>> {
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T,bool>);
    using ValueType = T;

    UIElementBase rect;
    ComputedStyle style;
    std::string text;
    T lower{};
    T upper{};
    T min{};
    T max{};
    bool disabled = false;
    bool hovered = false;
    bool focused = false;
    bool changed = false;
    int active_handle = 0;
    std::function<void(UIRangeInput<T>&)> on_change;

    UIRangeInput<T>& Text(std::string value) { text = std::move(value); return *this; }
    UIRangeInput<T>& Id(std::string value) { style.id = std::move(value); return *this; }
    UIRangeInput<T>& Values(T lower_value, T upper_value) {
        lower = lower_value;
        upper = upper_value;
        return *this;
    }
    UIRangeInput<T>& Range(T minimum, T maximum) { min = minimum; max = maximum; return *this; }
    UIRangeInput<T>& Disabled(bool value = true) { disabled = value; return *this; }
    UIRangeInput<T>& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIRangeInput<T>& OnChange(std::function<void(UIRangeInput<T>&)> callback) {
        on_change = std::move(callback);
        return *this;
    }
};

struct UICanvas;

using UICanvasDrawCallback =
    std::function<void(const UICanvas&,Rectangle)>;
using UICanvasUpdateCallback =
    std::function<void(UICanvas&)>;

struct UICanvas : UIStyleFluent<UICanvas> {
    UIElementBase rect;
    ComputedStyle style;
    bool focusable = true;
    bool disabled = false;
    bool hovered = false;
    bool focused = false;
    bool pressed = false;
    bool released = false;
    bool clicked = false;
    bool dragging = false;
    Vector2 local_mouse = {};
    Vector2 local_mouse_delta = {};
    Vector2 drag_delta = {};
    float wheel = 0.0f;
    Vector2 wheel_delta = {};
    std::array<bool,7> mouse_down{};
    std::array<bool,7> mouse_pressed{};
    std::array<bool,7> mouse_released{};
    std::array<bool,7> mouse_double_clicked{};
    std::array<bool,7> mouse_pressed_inside{};
    std::array<double,7> last_click_time{};
    std::array<Vector2,7> last_click_position{};
    float double_click_seconds = 0.30f;
    float double_click_distance = 5.0f;
    int key_pressed = 0;
    int character_pressed = 0;
    UICanvasUpdateCallback on_update;
    UICanvasDrawCallback on_draw;

    UICanvas& Id(std::string value) { style.id = std::move(value); return *this; }
    UICanvas& Focusable(bool value = true) { focusable = value; return *this; }
    UICanvas& Disabled(bool value = true) { disabled = value; return *this; }
    UICanvas& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UICanvas& OnUpdate(UICanvasUpdateCallback callback) { on_update = std::move(callback); return *this; }
    UICanvas& OnDraw(UICanvasDrawCallback callback) { on_draw = std::move(callback); return *this; }
};

struct UIImage : UIStyleFluent<UIImage> {
    UIElementBase rect;
    ComputedStyle style;
    Texture2D texture{};
    Rectangle source{};
    bool use_source = false;
    Color tint = WHITE;
    UIImageFit fit = UIImageFit::Contain;
    UIImageSampling sampling = UIImageSampling::Nearest;
    bool flip_x = false;
    bool flip_y = false;
    Vector2 position = {0.5f,0.5f};
    float rotation = 0.0f;
    Vector2 pivot = {0.5f,0.5f};
    int frame = 0;
    Vector2 frame_size = {};
    int frame_columns = 0;
    EdgeValues slice;
    float slice_scale = 1.0f;
    UIImageSliceMode slice_mode = UIImageSliceMode::Stretch;
    float tile_scale = 1.0f;
    Vector2 tile_offset = {};
    UIImageBlendMode blend_mode = UIImageBlendMode::Alpha;
    bool pixel_snap = false;
    float zoom = 1.0f;
    Vector2 pan{};
    bool interactive = false;
    bool disabled = false;
    bool hovered = false;
    bool focused = false;
    bool clicked = false;
    bool pressed = false;
    bool released = false;
    unsigned int sampling_texture_id = 0;
    UIImageSampling applied_sampling = UIImageSampling::Nearest;
    std::function<void(UIImage&)> on_click;

    UIImage& Id(std::string value) { style.id = std::move(value); return *this; }
    UIImage& Texture(Texture2D value) { texture = value; return *this; }
    UIImage& Source(Rectangle value) { source = value; use_source = true; return *this; }
    UIImage& ClearSource() { source = {}; use_source = false; return *this; }
    UIImage& Tint(Color value) { tint = value; return *this; }
    UIImage& Fit(UIImageFit value) { fit = value; return *this; }
    UIImage& Sampling(UIImageSampling value) { sampling = value; return *this; }
    UIImage& Flip(bool x, bool y) { flip_x = x; flip_y = y; return *this; }
    UIImage& Position(Vector2 value) { position = value; return *this; }
    UIImage& Rotation(float degrees) { rotation = degrees; return *this; }
    UIImage& Pivot(Vector2 value) { pivot = value; return *this; }
    UIImage& Frame(int value, Vector2 size, int columns = 0) {
        frame = std::max(0,value);
        frame_size = size;
        frame_columns = std::max(0,columns);
        return *this;
    }
    UIImage& NineSlice(float all, UIImageSliceMode mode = UIImageSliceMode::Stretch) {
        slice = EdgeValues(all);
        slice_mode = mode;
        return *this;
    }
    UIImage& NineSlice(float left, float top, float right, float bottom,
                       UIImageSliceMode mode = UIImageSliceMode::Stretch) {
        slice.left = left;
        slice.right = right;
        slice.top = top;
        slice.bottom = bottom;
        slice_mode = mode;
        return *this;
    }
    UIImage& SliceScale(float value) { slice_scale = value; return *this; }
    UIImage& Tile(float scale, Vector2 offset = {}) {
        tile_scale = scale;
        tile_offset = offset;
        return *this;
    }
    UIImage& Blend(UIImageBlendMode value) { blend_mode = value; return *this; }
    UIImage& PixelSnap(bool value = true) { pixel_snap = value; return *this; }
    UIImage& Zoom(float value) { zoom = value; return *this; }
    UIImage& Pan(Vector2 value) { pan = value; return *this; }
    UIImage& Interactive(bool value = true) { interactive = value; return *this; }
    UIImage& Disabled(bool value = true) { disabled = value; return *this; }
    UIImage& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIImage& OnClick(std::function<void(UIImage&)> callback) { on_click = std::move(callback); return *this; }
};

struct UISplitter : UIStyleFluent<UISplitter> {
    UIElementBase rect;
    ComputedStyle style;
    UISplitterOrientation orientation = UISplitterOrientation::Auto;
    float minimum_before = 80.0f;
    float minimum_after = 80.0f;
    bool disabled = false;
    bool hovered = false;
    bool dragging = false;
    Vector2 drag_origin{};
    Vector2 before_start_size{};
    Vector2 after_start_size{};
    float delta = 0.0f;
    bool changed = false;
    std::function<void(UISplitter&)> on_change;

    UISplitter& Id(std::string value) { style.id = std::move(value); return *this; }
    UISplitter& Orientation(UISplitterOrientation value) { orientation = value; return *this; }
    UISplitter& MinimumSizes(float before, float after) {
        minimum_before = before;
        minimum_after = after;
        return *this;
    }
    UISplitter& Disabled(bool value = true) { disabled = value; return *this; }
    UISplitter& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UISplitter& OnChange(std::function<void(UISplitter&)> callback) {
        on_change = std::move(callback);
        return *this;
    }
};

//--------------------------------------------------------------------
// Button
//--------------------------------------------------------------------

struct UIButton : UIStyleFluent<UIButton> {
    UIElementBase rect;
    ComputedStyle style;

    std::string text = "";
    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool disabled = false;

    bool pressed_inside = false;

    bool clicked = false;
    bool pressed = false;
    bool released = false;
    bool hover_entered = false;
    bool hover_exited = false;
    std::function<void(UIButton&)> on_click;

    UIButton& Text(std::string value) { text = std::move(value); return *this; }
    UIButton& Id(std::string value) { style.id = std::move(value); return *this; }
    UIButton& Disabled(bool value = true) { disabled = value; return *this; }
    UIButton& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIButton& OnClick(std::function<void(UIButton&)> callback) {
        on_click = std::move(callback);
        return *this;
    }
};

//--------------------------------------------------------------------
// Label
//--------------------------------------------------------------------

struct UILabel : UIStyleFluent<UILabel> {
    ComputedStyle style;

    std::string text = "";

    Vector2 text_size = {0.0f,0.0f};

    UIElementBase rect = UIElementBase(
        Rectangle{
            0.0f,
            0.0f,
            text_size.x,
            text_size.y
        }
    );

    UILabel& Text(std::string value) { text = std::move(value); return *this; }
    UILabel& Id(std::string value) { style.id = std::move(value); return *this; }
    UILabel& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
};

//--------------------------------------------------------------------
// Input
//--------------------------------------------------------------------

template <typename T>
struct UIInput : UIStyleFluent<UIInput<T>> {
    using ValueType = T;

    UIElementBase rect;
    ComputedStyle style;

    // Trigger caption, checkbox caption, or optional scalar field label.
    std::string text = "";
    std::string placeholder;
    bool show_label = true;
    InputLabelLayout label_layout = InputLabelLayout::Back;

    InputType input_type = InputType::Instant;
    InputStepButtonLayout step_button_layout =InputStepButtonLayout::Front;
    T value{};
    T min{};
    T max{};

    // Optional direct C++ two-way binding. Bind() pulls the pointed value when
    // no explicit Value() was supplied. An explicit Value() wins initialisation
    // regardless of whether it appears before or after Bind().
    T* bound_value = nullptr;
    bool pending_explicit_value = false;
    T step = []() {
        if constexpr(std::is_floating_point_v<T>) {
            // Float sliders commonly use normalized or fractional ranges.
            // A default step of 1 collapsed a 0..1 slider to only 0 or 1.
            return T{0.01};
        }
        else if constexpr(std::is_integral_v<T> && !std::is_same_v<T,bool>) {
            return T{1};
        }
        else {
            return T{};
        }
    }();

    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool dragging = false;
    bool changed = false;
    bool disabled = false;

    // Button-like state used by InputType::Trigger.
    bool clicked = false;
    bool triggered = false;
    bool pressed = false;
    bool released = false;
    bool pressed_inside = false;

    // Optional numeric stepper buttons.
    bool show_step_buttons = false;
    bool decrement_hovered = false;
    bool increment_hovered = false;

    // Gives editable text/value fields a distinct inner box.
    bool show_value_box = true;

    // Slider and drag-number behavior.
    float drag_speed = 0.01f;
    T drag_start_value{};
    float drag_accumulator = 0.0f;
    UISliderOrientation slider_orientation = UISliderOrientation::Horizontal;
    bool slider_show_value = true;
    float slider_value_width = 72.0f;
    bool slider_click_to_set = true;
    bool slider_handle_hovered = false;

    // Color-picker popup state.
    bool picker_open = false;
    bool picker_initialized = false;
    float picker_hue = 0.0f;
    float picker_saturation = 0.0f;
    float picker_brightness = 0.0f;
    int picker_drag_region = 0;
    std::shared_ptr<UIInput<std::string>> hex_input;
    bool hex_input_was_focused = false;
    bool hex_input_initialized = false;
    // Text-entry state. Drag numeric inputs enter this same editing path on
    // a double click while retaining normal single-click drag behavior.
    bool submitted = false;
    std::size_t max_characters = 64;
    int caret_frame_counter = 0;
    std::string edit_buffer;
    bool drag_text_editing = false;
    // Drag-mode text editing starts with the complete numeric token selected.
    // The first typed character replaces the old value, matching native numeric
    // field behavior without introducing a full caret/selection subsystem.
    bool drag_text_select_all = false;
    // Shared caret/selection state for every editable input mode.  The same
    // state is used by instant text/numeric inputs and by drag inputs while
    // they are in double-click text-edit mode.
    std::size_t text_caret = 0;
    std::size_t text_anchor = 0;
    std::size_t text_selection_start = 0;
    std::size_t text_selection_end = 0;
    bool text_mouse_selecting = false;
    double last_drag_click_time = -1.0;
    Vector2 last_drag_click_position = {0.0f,0.0f};
    int last_drag_click_component = -1;

    // Captions displayed beside vector/color components. Vector inputs default
    // to X/Y/Z/W while color inputs default to R/G/B/A.
    std::array<std::string,4> component_labels = []() {
        if constexpr(std::is_same_v<T,Color>) {
            return std::array<std::string,4>{"R","G","B","A"};
        }
        else {
            return std::array<std::string,4>{"X","Y","Z","W"};
        }
    }();
    bool show_component_labels = true;
    InputLabelLayout component_label_layout = InputLabelLayout::Back;

    std::array<std::string,4> component_buffers;
    int active_component = -1;
    std::function<void(UIInput<T>&)> on_change;
    std::function<void(UIInput<T>&)> on_submit;

    // Fluent configuration.
    UIInput<T>& Text(std::string value) { text = std::move(value); return *this; }
    UIInput<T>& Label(std::string value) { text = std::move(value); return *this; }
    UIInput<T>& ShowLabel(bool show = true) { show_label = show; return *this; }
    UIInput<T>& LabelLayout(InputLabelLayout value) {
        label_layout = value;
        component_label_layout = value;
        return *this;
    }
    UIInput<T>& Placeholder(std::string value) { placeholder = std::move(value); return *this; }
    UIInput<T>& Id(std::string value) { style.id = std::move(value); return *this; }
    UIInput<T>& Type(InputType value) { input_type = value; return *this; }
    UIInput<T>& Value(T value_) {
        value = std::move(value_);
        if(bound_value != nullptr) {
            *bound_value = value;
            pending_explicit_value = false;
        }
        else {
            pending_explicit_value = true;
        }
        return *this;
    }
    UIInput<T>& Bind(T* value_pointer) {
        bound_value = value_pointer;
        if(bound_value != nullptr) {
            if(pending_explicit_value) {
                *bound_value = value;
            }
            else {
                value = *bound_value;
            }
        }
        pending_explicit_value = false;
        return *this;
    }
    UIInput<T>& Bind(T& value_reference) { return Bind(&value_reference); }
    UIInput<T>& Unbind() { bound_value = nullptr; return *this; }
    bool IsBound() const { return bound_value != nullptr; }
    UIInput<T>& Range(T minimum, T maximum) { min = minimum; max = maximum; return *this; }
    UIInput<T>& Step(T value) { step = value; return *this; }
    UIInput<T>& StepButtons(
        bool show = true,
        InputStepButtonLayout layout = InputStepButtonLayout::Front
    ) {
        show_step_buttons = show;
        step_button_layout = layout;
        return *this;
    }
    UIInput<T>& ValueBox(bool show = true) { show_value_box = show; return *this; }
    UIInput<T>& DragSpeed(float value) { drag_speed = value; return *this; }
    UIInput<T>& SliderValue(bool show = true, float width = 72.0f) {
        slider_show_value = show;
        slider_value_width = width;
        return *this;
    }
    UIInput<T>& SliderOrientation(UISliderOrientation value) {
        slider_orientation = value;
        return *this;
    }
    UIInput<T>& SliderClickToSet(bool value = true) { slider_click_to_set = value; return *this; }
    UIInput<T>& MaxCharacters(std::size_t value) { max_characters = value; return *this; }
    UIInput<T>& ComponentLabels(std::array<std::string,4> labels) {
        component_labels = std::move(labels);
        return *this;
    }
    UIInput<T>& ComponentLabel(std::size_t index, std::string value) {
        if(index < component_labels.size()) component_labels[index] = std::move(value);
        return *this;
    }
    UIInput<T>& XLabel(std::string value) { return ComponentLabel(0,std::move(value)); }
    UIInput<T>& YLabel(std::string value) { return ComponentLabel(1,std::move(value)); }
    UIInput<T>& ZLabel(std::string value) { return ComponentLabel(2,std::move(value)); }
    UIInput<T>& WLabel(std::string value) { return ComponentLabel(3,std::move(value)); }
    UIInput<T>& RLabel(std::string value) { return ComponentLabel(0,std::move(value)); }
    UIInput<T>& GLabel(std::string value) { return ComponentLabel(1,std::move(value)); }
    UIInput<T>& BLabel(std::string value) { return ComponentLabel(2,std::move(value)); }
    UIInput<T>& ALabel(std::string value) { return ComponentLabel(3,std::move(value)); }
    UIInput<T>& ShowComponentLabels(bool show = true) {
        show_component_labels = show;
        return *this;
    }
    UIInput<T>& ComponentLabelLayout(InputLabelLayout value) {
        component_label_layout = value;
        return *this;
    }
    UIInput<T>& Disabled(bool value = true) { disabled = value; return *this; }
    UIInput<T>& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIInput<T>& OnChange(std::function<void(UIInput<T>&)> callback) {
        on_change = std::move(callback);
        return *this;
    }
    UIInput<T>& OnSubmit(std::function<void(UIInput<T>&)> callback) {
        on_submit = std::move(callback);
        return *this;
    }
    UIInput() = default;

    UIInput(InputType type, T initial_value)
        : input_type(type),
          value(initial_value),
          pending_explicit_value(true) {}

    UIInput(
        InputType type,
        T initial_value,
        T minimum,
        T maximum
    )
        : input_type(type),
          value(initial_value),
          min(minimum),
          max(maximum),
          pending_explicit_value(true) {}
};

//--------------------------------------------------------------------
// Container
//--------------------------------------------------------------------

using LayoutDirection = FlexDirection;

struct UIContainer;
struct UICollapsible;
struct UITreeNode;
struct UIMenu;
struct UIPopup;
struct UIWindow;
struct UITabBar;
class UIViewport;
class UIViewportItem;
struct UIReconcileOptions;
struct UIComponentDescriptor;
class UIComponentRegistry;

class IUICustomElement {
public:
    using GenericEventHandler = std::function<void(
        IUICustomElement&,
        const std::string&,
        const std::string&
    )>;

    UIElementBase rect;
    ComputedStyle style;
    // Invalidates callbacks retained by global interaction systems when this
    // custom element is destroyed or replaced during reconciliation.
    UILifetimeToken callback_lifetime;
    virtual ~IUICustomElement() = default;

    virtual SelectorType Selector() const = 0;
    virtual Vector2 Measure(
        const StyleSheet& style_sheet,
        float available_width,
        float available_height
    ) = 0;
    virtual void Layout(const StyleSheet& style_sheet, Rectangle bounds) = 0;
    virtual void Update(bool inherited_disabled) = 0;
    virtual void Draw(const StyleSheet& style_sheet) = 0;
    virtual bool Focusable() const { return false; }
    virtual bool* FocusFlag() { return nullptr; }
    // Custom 0.8 components can expose a retained child container without
    // implementing their own RXML child plumbing. Leaf components keep null.
    virtual UIContainer* ChildContainer() { return nullptr; }
    virtual const UIContainer* ChildContainer() const { return nullptr; }

    void SetGenericEventHandler(std::string event_name, GenericEventHandler handler) {
        generic_event_handlers_[NormalizeUIPseudoStateName(event_name)] = std::move(handler);
    }
    bool EmitGenericEvent(std::string event_name,std::string value = {}) {
        const std::string normalized = NormalizeUIPseudoStateName(event_name);
        auto found = generic_event_handlers_.find(normalized);
        if(found == generic_event_handlers_.end() || !found->second) return false;
        found->second(*this,normalized,value);
        return true;
    }

    virtual UIElementBase* FindNestedElement(const std::string&) { return nullptr; }
    virtual const UIElementBase* FindNestedElement(const std::string&) const { return nullptr; }
    virtual void ReconcileFrom(IUICustomElement&, const UIReconcileOptions&) {}
    virtual std::string TypeName() const = 0;

private:
    std::unordered_map<std::string,GenericEventHandler> generic_event_handlers_;
};

// RUI 0.9 recommended custom-component authoring layer.
//
// UIComponent supplies the mechanics shared by normal retained components.
// Application components generally implement DrawContent() and, only when
// useful, MeasureContent()/UpdateContent() or the semantic interaction hooks.
// IUICustomElement remains available for framework-level controls that need to
// override the complete Measure/Layout/Update/Draw lifecycle.
class UIComponent : public IUICustomElement {
public:
    bool disabled = false;

    UIComponent();
    ~UIComponent() override = default;

    SelectorType Selector() const final { return selector_; }
    std::string TypeName() const final { return component_name_; }

    Vector2 Measure(
        const StyleSheet& style_sheet,
        float available_width,
        float available_height
    ) override;
    void Layout(const StyleSheet& style_sheet,Rectangle bounds) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet& style_sheet) override;

    bool Focusable() const override { return focusable_; }
    bool* FocusFlag() override { return focusable_ ? &focused_ : nullptr; }

    UIContainer* ChildContainer() override;
    const UIContainer* ChildContainer() const override;

    const UIComponentDescriptor* Descriptor() const { return descriptor_; }
    bool Hovered() const { return hovered_; }
    bool Active() const { return active_; }
    bool Focused() const { return focused_; }
    bool Dragging() const { return dragging_; }
    bool HasKeyboardInput() const { return keyboard_input_; }

    // Called by UIComponentRegistry::Create(). Application code normally does
    // not need to call this. The descriptor pointer is stable after registry
    // freeze and avoids frame-time string lookups.
    void AssociateDescriptor(
        const UIComponentDescriptor* descriptor,
        const UIComponentRegistry* registry,
        std::string component_name,
        SelectorType selector,
        std::uint32_t capability_bits,
        bool accepts_visual_children
    );

protected:
    virtual Vector2 MeasureContent(float available_width,float available_height) const;
    virtual void UpdateContent(bool disabled);
    virtual void DrawContent(const ComputedStyle& style,Rectangle content_bounds);

    virtual void OnClick() {}
    virtual void OnDragStart(Vector2) {}
    virtual void OnDrag(Vector2,Vector2) {}
    virtual void OnDragEnd(Vector2) {}

    Rectangle ContentBounds(const ComputedStyle& resolved_style) const;

private:
    // Applies descriptor-exposed RCSS properties only for the component draw
    // pass, then restores semantic/RXML-bound property state afterwards.
    // Definitions live in rui.cpp where component metadata is complete.
    void BeginRegisteredStyleOverrides(const ComputedStyle& resolved_style);
    void EndRegisteredStyleOverrides();

    const UIComponentDescriptor* descriptor_ = nullptr;
    const UIComponentRegistry* registry_ = nullptr;
    std::string component_name_ = "UIComponent";
    SelectorType selector_ = SelectorType::Any;
    std::uint32_t capability_bits_ = 0;
    bool focusable_ = false;
    bool clickable_ = false;
    bool draggable_ = false;
    bool keyboard_input_ = false;
    bool accepts_visual_children_ = false;
    bool hovered_ = false;
    bool active_ = false;
    bool focused_ = false;
    bool dragging_ = false;
    Vector2 drag_origin_ = {0.0f,0.0f};
    Vector2 last_drag_mouse_ = {0.0f,0.0f};
    std::shared_ptr<UIContainer> children_;
    std::vector<std::pair<std::string,std::string>> registered_style_backups_;
};

using UIElement = std::variant<
    UIButton,
    UILabel,
    UIInput<float>,
    UIInput<int>,
    UIInput<bool>,
    UIInput<Vector2>,
    UIInput<Vector3>,
    UIInput<Vector4>,
    UIInput<Color>,
    UIInput<std::string>,
    UISelectable,
    UIComboBox,
    UIRangeInput<float>,
    UIRangeInput<int>,
    UICanvas,
    UIImage,
    UISplitter,
    std::shared_ptr<IUICustomElement>,
    std::shared_ptr<UIContainer>,
    std::shared_ptr<UICollapsible>,
    std::shared_ptr<UITreeNode>,
    std::shared_ptr<UIMenu>,
    std::shared_ptr<UIPopup>,
    std::shared_ptr<UIWindow>,
    std::shared_ptr<UITabBar>
>;

struct UIContainer : UIStyleFluent<UIContainer> {
    UIElementBase rect;
    ComputedStyle style;

    LayoutDirection direction = LayoutDirection::Column;

    // deque keeps references returned by Add()/EmplaceChild()/Row()/Column()/
    // widget factories stable when later siblings are appended. This is part of
    // the public retained-tree construction contract.
    std::deque<UIElement> elements;

    bool disabled = false;
    Vector2 scroll = {0.0f,0.0f};
    Vector2 content_size = {0.0f,0.0f};
    Vector2 maximum_scroll = {0.0f,0.0f};
    bool show_scrollbars = true;
    bool dragging_horizontal_scrollbar = false;
    bool dragging_vertical_scrollbar = false;
    float scrollbar_drag_anchor = 0.0f;
    float scrollbar_scroll_start = 0.0f;
    bool follow_tail=false;
    float last_maximum_scroll_y=0.0f;
    // Effective scale of the containing widget. Layout writes this before
    // recursing so a container scale applies to its complete subtree.
    float inherited_scale = 1.0f;
    // Layout is skipped while the tree, stylesheet, and containing size are unchanged.
    bool layout_dirty = true;
    float last_containing_width = -1.0f;
    float last_containing_height = -1.0f;
    std::size_t last_layout_revision = 0;
    std::size_t last_layout_signature = 0;

    template <typename Widget,
              typename = std::enable_if_t<std::is_constructible_v<UIElement,Widget>>>
    Widget& Add(Widget widget) {
        elements.emplace_back(std::move(widget));
        layout_dirty = true;
        return std::get<Widget>(elements.back());
    }
    template <typename Widget>
    Widget& AddChild(Widget widget) {
        return Add(std::move(widget));
    }

    template <typename Custom, typename... Arguments>
    Custom& AddCustom(Arguments&&... arguments) {
        static_assert(std::is_base_of_v<IUICustomElement,Custom>);
        auto custom = std::make_shared<Custom>(
            std::forward<Arguments>(arguments)...
        );
        Custom& result = *custom;
        std::string component_class = "__rui-component-";
        for(unsigned char character : result.TypeName()) {
            if(std::isspace(character) || character == '-' || character == '_') continue;
            component_class.push_back(static_cast<char>(std::tolower(character)));
        }
        AddCSSClass(result.style.classes,std::move(component_class));
        elements.emplace_back(
            std::static_pointer_cast<IUICustomElement>(std::move(custom))
        );
        layout_dirty = true;
        return result;
    }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return Add(
            Widget(
                std::forward<Arguments>(arguments)...
            )
        );
    }

    template <typename... Widgets>
    UIContainer& AddMany(Widgets&&... widgets) {
        (
            Add(
                std::forward<Widgets>(widgets)
            ),
            ...
        );
        return *this;
    }

    UIContainer& Row(
        std::string id = {},
        bool auto_height = true
    ) {
        UIContainer row;
        row.direction = LayoutDirection::Row;
        row.style.display = DisplayType::Flex;
        row.style.flex_direction = FlexDirection::Row;
        row.WidthPercent(100.0f);
        if(auto_height) row.AutoHeight();
        if(!id.empty()) row.Id(std::move(id));
        return Add(std::move(row));
    }

    UIContainer& Column(
        std::string id = {},
        bool auto_height = true
    ) {
        UIContainer column;
        column.direction = LayoutDirection::Column;
        column.style.display = DisplayType::Flex;
        column.style.flex_direction = FlexDirection::Column;
        column.WidthPercent(100.0f);
        if(auto_height) column.AutoHeight();
        if(!id.empty()) column.Id(std::move(id));
        return Add(std::move(column));
    }

    UILabel& Label(
        std::string text,
        std::string id = {}
    ) {
        UILabel label;
        label.text = std::move(text);
        if(!id.empty()) label.style.id = std::move(id);
        return Add(std::move(label));
    }

    UIButton& Button(
        std::string text,
        std::string id = {},
        std::function<void(UIButton&)> callback = {}
    ) {
        UIButton button;
        button.text = std::move(text);
        if(!id.empty()) button.style.id = std::move(id);
        button.on_click = std::move(callback);
        return Add(std::move(button));
    }

    // Programmatic input factories mirror Label()/Button()/Row()/Column().
    // They return the retained widget itself, so the complete UIInput fluent
    // surface can be chained immediately. Pointer overloads bind directly to
    // application state using UIInput<T>::Bind().
    template <typename T>
    UIInput<T>& Input(std::string id = {}) {
        UIInput<T> input;
        if(!id.empty()) input.style.id = std::move(id);
        return Add(std::move(input));
    }

    template <typename T>
    UIInput<T>& Input(T* binding, std::string id = {}) {
        UIInput<T>& input = Input<T>(std::move(id));
        input.Bind(binding);
        return input;
    }

    UIInput<float>& FloatInput(std::string id = {}) {
        return Input<float>(std::move(id));
    }
    UIInput<float>& FloatInput(float* binding, std::string id = {}) {
        return Input<float>(binding,std::move(id));
    }

    UIInput<int>& IntInput(std::string id = {}) {
        return Input<int>(std::move(id));
    }
    UIInput<int>& IntInput(int* binding, std::string id = {}) {
        return Input<int>(binding,std::move(id));
    }

    UIInput<bool>& BoolInput(std::string id = {}) {
        return Input<bool>(std::move(id));
    }
    UIInput<bool>& BoolInput(bool* binding, std::string id = {}) {
        return Input<bool>(binding,std::move(id));
    }

    UIInput<std::string>& StringInput(std::string id = {}) {
        return Input<std::string>(std::move(id));
    }
    UIInput<std::string>& StringInput(std::string* binding, std::string id = {}) {
        return Input<std::string>(binding,std::move(id));
    }

    UIInput<Vector2>& Vector2Input(std::string id = {}) {
        return Input<Vector2>(std::move(id));
    }
    UIInput<Vector2>& Vector2Input(Vector2* binding, std::string id = {}) {
        return Input<Vector2>(binding,std::move(id));
    }

    UIInput<Vector3>& Vector3Input(std::string id = {}) {
        return Input<Vector3>(std::move(id));
    }
    UIInput<Vector3>& Vector3Input(Vector3* binding, std::string id = {}) {
        return Input<Vector3>(binding,std::move(id));
    }

    UIInput<Vector4>& Vector4Input(std::string id = {}) {
        return Input<Vector4>(std::move(id));
    }
    UIInput<Vector4>& Vector4Input(Vector4* binding, std::string id = {}) {
        return Input<Vector4>(binding,std::move(id));
    }

    UIInput<Color>& ColorInput(std::string id = {}) {
        return Input<Color>(std::move(id));
    }
    UIInput<Color>& ColorInput(Color* binding, std::string id = {}) {
        return Input<Color>(binding,std::move(id));
    }

    template <typename T>
    UIRangeInput<T>& RangeInput(std::string id = {}) {
        UIRangeInput<T> input;
        if(!id.empty()) input.style.id = std::move(id);
        return Add(std::move(input));
    }

    UIRangeInput<float>& FloatRangeInput(std::string id = {}) {
        return RangeInput<float>(std::move(id));
    }

    UIRangeInput<int>& IntRangeInput(std::string id = {}) {
        return RangeInput<int>(std::move(id));
    }

    UIComboBox& ComboBox(std::string id = {}) {
        UIComboBox combo_box;
        if(!id.empty()) combo_box.style.id = std::move(id);
        return Add(std::move(combo_box));
    }

    UISelectable& Selectable(std::string text = "Selectable", std::string id = {}) {
        UISelectable selectable;
        selectable.text = std::move(text);
        if(!id.empty()) selectable.style.id = std::move(id);
        return Add(std::move(selectable));
    }

    template <typename T>
    UIInput<T>& LabeledInput(
        std::string label_text,
        UIInput<T> input,
        std::string row_id = {},
        std::string label_id = {}
    ) {
        UIContainer& row = Row(std::move(row_id));
        row.Label(std::move(label_text),std::move(label_id));
        return row.Add(std::move(input));
    }

    UIContainer& Add(UIContainer child);
    UICollapsible& Add(UICollapsible child);
    UITreeNode& Add(UITreeNode child);
    UIMenu& Add(UIMenu child);
    UIPopup& Add(UIPopup child);
    UIWindow& Add(UIWindow child);
    UITabBar& Add(UITabBar child);

    operator std::shared_ptr<UIContainer>() && {
        return std::make_shared<UIContainer>(std::move(*this));
    }
};

//--------------------------------------------------------------------
// Advanced custom elements
//--------------------------------------------------------------------

enum class UITextAnimationMode {
    None,
    Typewriter,
    FadeCharacters,
    Wave
};

struct UITextAnimation {
    UITextAnimationMode mode = UITextAnimationMode::None;
    float duration = 0.8f;
    float delay = 0.0f;
    float stagger = 0.025f;
    float wave_amplitude = 3.0f;
    float wave_frequency = 8.0f;
    bool loop = false;
    bool playing = false;
    double started = 0.0;
};

struct UITextWrappedLine {
    std::string text;
    std::size_t first_character = 0;
    std::size_t character_count = 0;
    float width = 0.0f;
};

class UITextBox final : public IUICustomElement, public UIStyleFluent<UITextBox> {
public:
    std::string text;
    std::string placeholder;
    bool editable = false;
    bool multiline = true;
    bool wrap = true;
    bool dynamic_height = false;
    bool disabled = false;
    bool hovered = false;
    bool focused = false;
    bool changed = false;
    bool submitted = false;
    std::size_t max_characters = 65536;
    std::size_t caret = 0;
    std::size_t selection_anchor = 0;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
    bool mouse_selecting = false;
    float scroll_y = 0.0f;
    float line_spacing = 2.0f;
    UITextAnimation text_animation;
    std::vector<UITextWrappedLine> wrapped_lines;
    std::function<void(UITextBox&)> on_change;
    std::function<void(UITextBox&)> on_submit;

    SelectorType Selector() const override { return SelectorType::TextBox; }
    std::string TypeName() const override { return "TextBox"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    bool Focusable() const override { return editable && !disabled; }
    bool* FocusFlag() override { return &focused; }
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    UITextBox& Text(std::string value, bool restart_animation = false);
    UITextBox& Placeholder(std::string value);
    UITextBox& Editable(bool value = true);
    UITextBox& Multiline(bool value = true);
    UITextBox& Wrap(bool value = true);
    UITextBox& DynamicHeight(bool value = true);
    UITextBox& MaxCharacters(std::size_t value);
    UITextBox& Animate(
        UITextAnimationMode mode,
        float duration = 0.8f,
        bool loop = false
    );
    UITextBox& PlayTextAnimation(bool restart = true);
    UITextBox& StopTextAnimation();
    UITextBox& OnChange(std::function<void(UITextBox&)> callback);
    UITextBox& OnSubmit(std::function<void(UITextBox&)> callback);
    void RebuildWrap(float available_width);
    std::size_t VisibleCharacterCount(double now) const;
};

struct UITableColumn {
    std::string id;
    std::string title;
    float width = 140.0f;
    float min_width = 48.0f;
    float max_width = 800.0f;
    bool sortable = true;
    bool resizable = true;
    TextAlign alignment = TextAlign::Left;
};

struct UITableRow {
    std::string key;
    std::vector<std::string> cells;
    bool disabled = false;
};

struct UIVisibleRange {
    std::size_t first = 0;
    std::size_t last = 0;
    bool empty = true;
};

UIVisibleRange ComputeUIVisibleRange(
    std::size_t item_count,
    float item_extent,
    float scroll_offset,
    float viewport_extent,
    std::size_t overscan = 1
);

class UITable final : public IUICustomElement, public UIStyleFluent<UITable> {
public:
    std::vector<UITableColumn> columns;
    std::vector<UITableRow> rows;
    float header_height = 30.0f;
    float row_height = 26.0f;
    float scroll_y = 0.0f;
    std::size_t overscan = 2;
    int sort_column = -1;
    bool sort_ascending = true;
    int selected_row = -1;
    int hovered_row = -1;
    int resizing_column = -1;
    float resize_anchor_x = 0.0f;
    float resize_start_width = 0.0f;
    bool multi_select = false;
    std::set<std::string> selected_keys;
    std::function<void(UITable&,std::size_t)> on_select;
    std::function<void(UITable&,int,bool)> on_sort;

    SelectorType Selector() const override { return SelectorType::Table; }
    std::string TypeName() const override { return "Table"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    bool Focusable() const override { return true; }
    bool* FocusFlag() override { return &focused_; }
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    UITable& AddColumn(UITableColumn column);
    UITable& AddRow(UITableRow row);
    UITable& ClearRows();
    UITable& SortBy(int column, bool ascending);
    UIVisibleRange VisibleRows() const;
    float ContentWidth() const;
    float ContentHeight() const;

private:
    bool focused_ = false;
};

class UIVirtualList final : public IUICustomElement, public UIStyleFluent<UIVirtualList> {
public:
    using BuildVisibleItems = std::function<void(
        UIVirtualList&,
        UIContainer&,
        std::size_t,
        std::size_t
    )>;

    // Legacy string-list storage. This remains the source when no virtual
    // template builder is installed.
    std::vector<std::string> items;

    // Templated mode stores only the number of source records here. The RXML
    // runtime keeps the actual UIX bindings in the builder callback and creates
    // controls only for VisibleItems().
    std::size_t virtual_item_count = 0;
    UIContainer content;
    BuildVisibleItems build_visible_items;

    float item_height = 28.0f;
    float scroll_y = 0.0f;
    std::size_t overscan = 2;
    int selected_index = -1;
    int hovered_index = -1;
    bool disabled = false;
    std::function<void(std::size_t,Rectangle,const std::string&,bool)> draw_item;
    std::function<void(UIVirtualList&,std::size_t)> on_select;

    SelectorType Selector() const override { return SelectorType::VirtualList; }
    std::string TypeName() const override { return "VirtualList"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    bool Focusable() const override { return !disabled; }
    bool* FocusFlag() override { return &focused_; }
    UIElementBase* FindNestedElement(const std::string&) override;
    const UIElementBase* FindNestedElement(const std::string&) const override;
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    bool UsesTemplate() const { return static_cast<bool>(build_visible_items); }
    std::size_t ItemCount() const {
        return UsesTemplate() ? virtual_item_count : items.size();
    }
    UIVisibleRange VisibleItems() const;
    UIVirtualList& Items(std::vector<std::string> values);
    UIVirtualList& VirtualItems(std::size_t count, BuildVisibleItems builder);
    UIVirtualList& ItemHeight(float value);
    UIVirtualList& Overscan(std::size_t value);
    UIVirtualList& OnSelect(std::function<void(UIVirtualList&,std::size_t)> callback);

private:
    void RebuildVisibleContent(std::size_t first, std::size_t last);

    bool focused_ = false;
    bool built_range_valid_ = false;
    std::size_t built_first_ = 0;
    std::size_t built_last_ = 0;
};

enum class UIDockAxis { Horizontal, Vertical };
enum class UIDockDropZone { None, Center, Left, Right, Top, Bottom };
enum class UIDockNodeType { Leaf, Split };

struct UIDockNode {
    std::uint64_t id = 0;
    UIDockNodeType type = UIDockNodeType::Leaf;
    UIDockAxis axis = UIDockAxis::Horizontal;
    float ratio = 0.5f;
    Rectangle rectangle = {};
    std::shared_ptr<UIDockNode> first;
    std::shared_ptr<UIDockNode> second;
    std::vector<std::string> tabs;
    int active_tab = 0;
};

struct UIDockPanel {
    std::string id;
    std::string title;
    bool open = true;
    bool closable = true;
    bool floating = false;
    Rectangle floating_rectangle = {80.0f,80.0f,420.0f,320.0f};
    bool dragging = false;
    bool resizing = false;
    Vector2 drag_anchor = {};
    Vector2 resize_anchor = {};
    Rectangle resize_start = {};
    UIContainer content;
};

struct UIViewportCamera {
    Vector2 pan = {0.0f,0.0f};
    float zoom = 1.0f;
    float min_zoom = 0.1f;
    float max_zoom = 4.0f;
};

struct UIViewportDrawEvent {
    Rectangle screen_bounds{};
    Rectangle visible_world_bounds{};
    Vector2 local_mouse{};
    Vector2 world_mouse{};
    Vector2 pan{};
    float zoom = 1.0f;
};

using UIViewportUpdateCallback = std::function<void(UIViewport&)>;
using UIViewportDrawCallback = std::function<void(const UIViewport&,const UIViewportDrawEvent&)>;
using UIViewportViewChangedCallback = std::function<void(UIViewport&,const UIViewportCamera&)>;

class UIViewportItem final : public IUICustomElement, public UIStyleFluent<UIViewportItem> {
public:
    UIContainer content;
    Vector2 position{};
    int z_index = 0;
    bool z_order_changed = false;
    bool visible = true;
    bool interactive = true;
    bool disabled = false;
    bool laid_out = false;
    Vector2 layout_origin{};
    // RXML installs this for live world-position bindings without rebuilding.
    std::function<void(UIViewportItem&)> sync_bindings;

    SelectorType Selector() const override { return SelectorType::ViewportItem; }
    std::string TypeName() const override { return "ViewportItem"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    bool Focusable() const override { return false; }
    UIElementBase* FindNestedElement(const std::string&) override;
    const UIElementBase* FindNestedElement(const std::string&) const override;
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    void SetPosition(Vector2 value);
    void SetZIndex(int value) {
        if(z_index==value) return;
        z_index=value;
        z_order_changed=true;
    }
    UIViewportItem& Position(Vector2 value) { SetPosition(value); return *this; }
    UIViewportItem& ZIndex(int value) { SetZIndex(value); return *this; }
    UIViewportItem& Visible(bool value=true) { visible=value; return *this; }
    UIViewportItem& Interactive(bool value=true) { interactive=value; return *this; }
    UIViewportItem& Disabled(bool value=true) { disabled=value; return *this; }
    template <typename Widget>
    UIViewportItem& Add(Widget widget) { content.Add(std::move(widget)); return *this; }
};

class UIViewport final : public IUICustomElement, public UIStyleFluent<UIViewport> {
public:
    UIContainer content;
    UIViewportCamera camera;
    // Transform of the UI tree containing this Viewport. Cached during update
    // and draw so coordinate conversion remains well-defined for nesting.
    UITransformContext parent_transform{};

    bool clip = true;
    bool focusable = true;
    bool disabled = false;
    bool hovered = false;
    bool focused = false;
    bool panning = false;
    bool pan_enabled = true;
    bool wheel_zoom = true;
    int pan_button = MOUSE_BUTTON_MIDDLE;
    float zoom_speed = 0.12f;

    Vector2 local_mouse{};
    Vector2 local_mouse_delta{};
    Vector2 world_mouse{};
    Vector2 world_mouse_delta{};
    float wheel = 0.0f;

    UIViewportUpdateCallback on_update;
    UIViewportDrawCallback on_background_draw;
    UIViewportDrawCallback on_foreground_draw;
    UIViewportViewChangedCallback on_view_changed;
    // RXML installs this to keep pan/zoom bindings live in both directions
    // without forcing reconciliation or a markup rebuild.
    std::function<void(UIViewport&)> sync_bindings;

    SelectorType Selector() const override { return SelectorType::Viewport; }
    std::string TypeName() const override { return "Viewport"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    bool Focusable() const override { return focusable; }
    bool* FocusFlag() override { return &focused; }
    UIElementBase* FindNestedElement(const std::string&) override;
    const UIElementBase* FindNestedElement(const std::string&) const override;
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    Vector2 WorldToLocal(Vector2 world) const;
    Vector2 LocalToWorld(Vector2 local) const;
    Vector2 WorldToScreen(Vector2 world) const;
    Vector2 ScreenToWorld(Vector2 screen) const;
    Rectangle WorldToScreen(Rectangle world) const;
    Rectangle ScreenToWorld(Rectangle screen) const;
    Rectangle VisibleWorldRect() const;

    void SetPan(Vector2 pan);
    void PanBy(Vector2 delta);
    void SetZoom(float zoom);
    void ZoomAt(Vector2 parent_position,float zoom);
    void CenterOn(Vector2 world_position);
    void Frame(Rectangle world_bounds,float padding=32.0f);
    void FrameAll(float padding=32.0f);
    void ResetView();

    UIViewport& Pan(Vector2 value) { SetPan(value); return *this; }
    UIViewport& Zoom(float value) { SetZoom(value); return *this; }
    UIViewport& ZoomRange(float minimum,float maximum) {
        camera.min_zoom=std::max(0.0001f,minimum);
        camera.max_zoom=std::max(camera.min_zoom,maximum);
        SetZoom(camera.zoom);
        return *this;
    }
    UIViewport& PanEnabled(bool value=true) { pan_enabled=value; return *this; }
    UIViewport& PanButton(int value) { pan_button=value; return *this; }
    UIViewport& WheelZoom(bool value=true) { wheel_zoom=value; return *this; }
    UIViewport& ZoomSpeed(float value) { zoom_speed=std::max(0.001f,value); return *this; }
    UIViewport& Clip(bool value=true) { clip=value; return *this; }
    UIViewport& FocusableValue(bool value=true) { focusable=value; return *this; }
    UIViewport& Disabled(bool value=true) { disabled=value; return *this; }
    UIViewport& OnUpdate(UIViewportUpdateCallback callback) { on_update=std::move(callback); return *this; }
    UIViewport& OnBackgroundDraw(UIViewportDrawCallback callback) { on_background_draw=std::move(callback); return *this; }
    UIViewport& OnForegroundDraw(UIViewportDrawCallback callback) { on_foreground_draw=std::move(callback); return *this; }
    UIViewport& OnViewChanged(UIViewportViewChangedCallback callback) { on_view_changed=std::move(callback); return *this; }
    UIViewportItem& AddItem(std::string id={},Vector2 position={});
    UIViewportItem* FindItem(const std::string& id);
    const UIViewportItem* FindItem(const std::string& id) const;
    bool RemoveItem(const std::string& id);
    bool BringToFront(const std::string& id);
    void SortItemsByZ();
};

class UIDockSpace final : public IUICustomElement, public UIStyleFluent<UIDockSpace> {
public:
    std::vector<UIDockPanel> panels;
    std::shared_ptr<UIDockNode> root;
    float tab_height = 28.0f;
    float splitter_size = 6.0f;
    float minimum_pane_size = 80.0f;
    UIDockDropZone preview_zone = UIDockDropZone::None;
    std::uint64_t preview_node = 0;
    std::string dragging_panel;
    std::function<void(UIDockSpace&)> on_layout_changed;

    // Optional declarative persistence used by <DockLayout> and DockSpace
    // persistence attributes in RXML.
    std::string layout_file;
    std::string layout_id = "default";
    bool persist_layout = false;
    bool restore_layout_on_load = true;
    bool save_layout_on_change = true;
    bool save_layout_on_exit = true;

    UIDockSpace();
    ~UIDockSpace() override;
    SelectorType Selector() const override { return SelectorType::DockSpace; }
    std::string TypeName() const override { return "DockSpace"; }
    Vector2 Measure(const StyleSheet&,float,float) override;
    void Layout(const StyleSheet&,Rectangle) override;
    void Update(bool inherited_disabled) override;
    void Draw(const StyleSheet&) override;
    UIElementBase* FindNestedElement(const std::string&) override;
    const UIElementBase* FindNestedElement(const std::string&) const override;
    void ReconcileFrom(IUICustomElement&,const UIReconcileOptions&) override;

    UIDockPanel& AddPanel(UIDockPanel panel, std::uint64_t leaf = 0);
    bool RemovePanel(const std::string& panel_id);
    bool DockPanel(
        const std::string& panel_id,
        std::uint64_t target_node,
        UIDockDropZone zone
    );
    bool UndockPanel(const std::string& panel_id, Rectangle floating_rectangle);
    bool SelectPanel(const std::string& panel_id);
    UIDockPanel* FindPanel(const std::string& panel_id);
    const UIDockPanel* FindPanel(const std::string& panel_id) const;
    UIDockNode* FindNode(std::uint64_t id);
    const UIDockNode* FindNode(std::uint64_t id) const;
    std::string SerializeLayout() const;
    bool DeserializeLayout(const std::string& text);
    bool SaveLayoutFile() const;
    bool LoadLayoutFile();
    void NotifyLayoutChanged();
    void NormalizeLayout();

private:
    std::uint64_t next_node_id_ = 1;
    std::string pressed_panel_;
    Vector2 press_origin_ = {};
    std::uint64_t resizing_node_ = 0;
    float resize_start_ratio_ = 0.5f;
    Vector2 resize_origin_ = {};
    bool layout_dirty_ = false;
};

struct UITab {
    std::string id;
    std::string title = "Tab";
    bool disabled = false;
    bool closable = false;
    bool close_requested = false;
    UIContainer content;

    UITab& AutoSize(bool enabled = true) {
        content.AutoSize(enabled);
        return *this;
    }

    UITab& ResizeToContent(bool enabled = true) {
        content.ResizeToContent(enabled);
        return *this;
    }

    template <typename Widget>
    UITab& Add(Widget widget) {
        content.Add(std::move(widget));
        return *this;
    }

    template <typename Widget>
    Widget& AddChild(Widget widget) {
        return content.Add(std::move(widget));
    }

    template <typename... Widgets>
    UITab& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }
};

struct UITabBar : UIStyleFluent<UITabBar> {
    UIElementBase rect;
    ComputedStyle style;
    std::vector<UITab> tabs;
    int selected_index = 0;
    int hovered_index = -1;
    bool disabled = false;
    bool changed = false;
    std::function<void(UITabBar&)> on_change;
    std::function<void(UITabBar&,int)> on_close;

    UITab& AddTab(std::string title, std::string id = {}) {
        UITab tab;
        tab.id = std::move(id);
        tab.title = std::move(title);
        tabs.push_back(std::move(tab));
        return tabs.back();
    }

    template <typename Builder>
    UITabBar& Tab(
        std::string title,
        Builder&& builder,
        std::string id = {}
    ) {
        UITab& tab = AddTab(
            std::move(title),
            std::move(id)
        );
        builder(tab.content);
        return *this;
    }

    UITabBar& Id(std::string value) { style.id = std::move(value); return *this; }
    UITabBar& SelectedIndex(int value) { selected_index = value; return *this; }
    UITabBar& Disabled(bool value = true) { disabled = value; return *this; }
    UITabBar& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UITabBar& OnChange(std::function<void(UITabBar&)> callback) {
        on_change = std::move(callback);
        return *this;
    }
    UITabBar& OnClose(std::function<void(UITabBar&,int)> callback) {
        on_close = std::move(callback);
        return *this;
    }

    UITab* SelectedTab() {
        if(selected_index < 0 || selected_index >= static_cast<int>(tabs.size())) return nullptr;
        return &tabs[static_cast<std::size_t>(selected_index)];
    }

    const UITab* SelectedTab() const {
        if(selected_index < 0 || selected_index >= static_cast<int>(tabs.size())) return nullptr;
        return &tabs[static_cast<std::size_t>(selected_index)];
    }

    operator std::shared_ptr<UITabBar>() && {
        return std::make_shared<UITabBar>(std::move(*this));
    }
};

struct UICollapsible : UIStyleFluent<UICollapsible> {
    UIElementBase rect;
    ComputedStyle style;
    std::string text = "Section";
    bool open = true;
    bool disabled = false;
    bool hovered = false;
    bool clicked = false;
    std::function<void(UICollapsible&)> on_click;
    UIContainer content;

    UICollapsible& Text(std::string value) { text = std::move(value); return *this; }
    UICollapsible& Open(bool value = true) { open = value; return *this; }
    UICollapsible& Disabled(bool value = true) { disabled = value; return *this; }
    UICollapsible& Id(std::string value) { style.id = std::move(value); return *this; }
    UICollapsible& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UICollapsible& OnClick(std::function<void(UICollapsible&)> callback) {
        on_click = std::move(callback);
        return *this;
    }
    template <typename Widget>
    UICollapsible& Add(Widget widget) {
        content.Add(std::move(widget));
        return *this;
    }

    template <typename Widget>
    Widget& AddChild(Widget widget) {
        return content.Add(std::move(widget));
    }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return content.EmplaceChild<Widget>(
            std::forward<Arguments>(arguments)...
        );
    }

    template <typename... Widgets>
    UICollapsible& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }

    operator std::shared_ptr<UICollapsible>() && {
        return std::make_shared<UICollapsible>(std::move(*this));
    }
};

struct UITreeNode : UIStyleFluent<UITreeNode> {
    UIElementBase rect;
    ComputedStyle style;
    std::string text = "Node";
    bool open = true;
    bool selected = false;
    bool disabled = false;
    bool hovered = false;
    bool clicked = false;
    std::function<void(UITreeNode&)> on_click;
    UIContainer content;

    UITreeNode& Text(std::string value) { text = std::move(value); return *this; }
    UITreeNode& Open(bool value = true) { open = value; return *this; }
    UITreeNode& Selected(bool value = true) { selected = value; return *this; }
    UITreeNode& Disabled(bool value = true) { disabled = value; return *this; }
    UITreeNode& Id(std::string value) { style.id = std::move(value); return *this; }
    UITreeNode& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UITreeNode& OnClick(std::function<void(UITreeNode&)> callback) { on_click = std::move(callback); return *this; }
    template <typename Widget>
    UITreeNode& Add(Widget widget) { content.Add(std::move(widget)); return *this; }

    template <typename Widget>
    Widget& AddChild(Widget widget) { return content.Add(std::move(widget)); }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return content.EmplaceChild<Widget>(
            std::forward<Arguments>(arguments)...
        );
    }

    template <typename... Widgets>
    UITreeNode& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }

    operator std::shared_ptr<UITreeNode>() && {
        return std::make_shared<UITreeNode>(std::move(*this));
    }
};

struct UIMenu : UIStyleFluent<UIMenu> {
    UIElementBase rect;
    ComputedStyle style;
    std::string text = "Menu";
    bool open = false;
    bool disabled = false;
    bool hovered = false;
    bool clicked = false;
    std::function<void(UIMenu&)> on_click;
    UIContainer content;

    UIMenu& Text(std::string value) { text = std::move(value); return *this; }
    UIMenu& Open(bool value = true) { open = value; return *this; }
    UIMenu& Disabled(bool value = true) { disabled = value; return *this; }
    UIMenu& Id(std::string value) { style.id = std::move(value); return *this; }
    UIMenu& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIMenu& OnClick(std::function<void(UIMenu&)> callback) { on_click = std::move(callback); return *this; }
    template <typename Widget>
    UIMenu& Add(Widget widget) { content.Add(std::move(widget)); return *this; }

    template <typename Widget>
    Widget& AddChild(Widget widget) { return content.Add(std::move(widget)); }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return content.EmplaceChild<Widget>(
            std::forward<Arguments>(arguments)...
        );
    }

    template <typename... Widgets>
    UIMenu& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }

    operator std::shared_ptr<UIMenu>() && {
        return std::make_shared<UIMenu>(std::move(*this));
    }
};

struct UIPopup : UIStyleFluent<UIPopup> {
    UIElementBase rect;
    ComputedStyle style;
    std::string title;
    bool open = false;
    bool modal = false;
    bool close_on_escape = true;
    bool close_on_outside_click = true;
    bool opened = false;
    bool closed = false;
    std::function<void(UIPopup&)> on_open;
    std::function<void(UIPopup&)> on_close;
    UIContainer content;

    UIPopup& Title(std::string value) { title = std::move(value); return *this; }
    UIPopup& Open(bool value = true) { open = value; return *this; }
    UIPopup& Modal(bool value = true) { modal = value; return *this; }
    UIPopup& CloseOnEscape(bool value = true) { close_on_escape = value; return *this; }
    UIPopup& CloseOnOutsideClick(bool value = true) { close_on_outside_click = value; return *this; }
    UIPopup& Id(std::string value) { style.id = std::move(value); return *this; }
    UIPopup& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIPopup& OnOpen(std::function<void(UIPopup&)> callback) { on_open = std::move(callback); return *this; }
    UIPopup& OnClose(std::function<void(UIPopup&)> callback) { on_close = std::move(callback); return *this; }
    template <typename Widget>
    UIPopup& Add(Widget widget) { content.Add(std::move(widget)); return *this; }

    template <typename Widget>
    Widget& AddChild(Widget widget) { return content.Add(std::move(widget)); }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return content.EmplaceChild<Widget>(
            std::forward<Arguments>(arguments)...
        );
    }

    template <typename... Widgets>
    UIPopup& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }

    operator std::shared_ptr<UIPopup>() && {
        return std::make_shared<UIPopup>(std::move(*this));
    }
};

struct UIWindow : UIStyleFluent<UIWindow> {
    UIElementBase rect;
    ComputedStyle style;
    std::string title = "Window";
    bool open = true;
    bool collapsed = false;
    bool disabled = false;
    bool close_requested = false;
    bool dragging = false;
    bool resizing = false;
    Vector2 drag_anchor = {};
    Rectangle interaction_start = {};
    std::function<void(UIWindow&)> on_close;
    std::function<void(UIWindow&)> on_collapse;
    UIContainer content;

    UIWindow& Title(std::string value) { title = std::move(value); return *this; }
    UIWindow& Open(bool value = true) { open = value; return *this; }
    UIWindow& Collapsed(bool value = true) { collapsed = value; return *this; }
    UIWindow& Disabled(bool value = true) { disabled = value; return *this; }
    UIWindow& Id(std::string value) { style.id = std::move(value); return *this; }
    UIWindow& Tooltip(std::string value) { rect.tooltip = std::move(value); return *this; }
    UIWindow& OnClose(std::function<void(UIWindow&)> callback) { on_close = std::move(callback); return *this; }
    UIWindow& OnCollapse(std::function<void(UIWindow&)> callback) { on_collapse = std::move(callback); return *this; }
    template <typename Widget>
    UIWindow& Add(Widget widget) { content.Add(std::move(widget)); return *this; }

    template <typename Widget>
    Widget& AddChild(Widget widget) { return content.Add(std::move(widget)); }

    template <typename Widget, typename... Arguments>
    Widget& EmplaceChild(Arguments&&... arguments) {
        return content.EmplaceChild<Widget>(
            std::forward<Arguments>(arguments)...
        );
    }

    template <typename... Widgets>
    UIWindow& AddMany(Widgets&&... widgets) {
        content.AddMany(
            std::forward<Widgets>(widgets)...
        );
        return *this;
    }

    operator std::shared_ptr<UIWindow>() && {
        return std::make_shared<UIWindow>(std::move(*this));
    }
};















void OpenPopupAt(UIPopup& popup, Vector2 position);

void ClosePopup(UIPopup& popup);

void OpenContextMenu(UIPopup& popup);

void OpenModal(UIPopup& popup);

void MarkLayoutDirty(
    UIContainer& container,
    bool recursive = true
);

//--------------------------------------------------------------------
// Root UI
//--------------------------------------------------------------------

struct UI {
    UIContainer root;
    std::unordered_map<std::string,Font> fonts;

    bool LoadFont(
        const std::string& name,
        const std::string& path,
        int base_size = 32,
        const std::vector<int>& codepoints = {}
    ) {
        return AddUIFont(fonts,name,path,base_size,codepoints);
    }

    void ActivateFonts() const {
        SetUIFontRegistry(fonts);
    }

    void InvalidateLayout() {
        MarkLayoutDirty(root,true);
    }

    void UnloadFonts() {
        UnloadUIFonts(fonts);
    }
};

//--------------------------------------------------------------------
// Type utilities
//--------------------------------------------------------------------

template <typename T>
struct IsUIInput : std::false_type {};

template <typename T>
struct IsUIInput<UIInput<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsUIInputV = IsUIInput<T>::value;

template <typename T>
struct IsUIRangeInput : std::false_type {};

template <typename T>
struct IsUIRangeInput<UIRangeInput<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsUIRangeInputV = IsUIRangeInput<T>::value;

template <typename T>
struct IsUICompositePointer : std::false_type {};

template <>
struct IsUICompositePointer<std::shared_ptr<UIContainer>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UICollapsible>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UITreeNode>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UIMenu>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UIPopup>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UIWindow>> : std::true_type {};
template <>
struct IsUICompositePointer<std::shared_ptr<UITabBar>> : std::true_type {};

template <typename T>
inline constexpr bool IsUICompositePointerV =
    IsUICompositePointer<T>::value;

template <typename T>
struct IsUICustomElementPointer : std::false_type {};
template <>
struct IsUICustomElementPointer<std::shared_ptr<IUICustomElement>> : std::true_type {};
template <typename T>
inline constexpr bool IsUICustomElementPointerV =
    IsUICustomElementPointer<T>::value;

std::size_t HashRectangle(Rectangle rectangle);

//--------------------------------------------------------------------
// Runtime layout editing and persistence
//--------------------------------------------------------------------

struct UIRuntimeOverride {
    Vector2 offset = {0.0f,0.0f};
    Vector2 size = {0.0f,0.0f};
    bool has_size = false;
};

enum class UIRuntimeInteraction {
    None,
    Move,
    Resize
};

struct UIRuntimeLayer {
    bool enabled = false;
    bool auto_save = true;
    bool constrain_to_parent = true;
    float snap = 8.0f;
    float minimum_width = 32.0f;
    float minimum_height = 24.0f;
    float resize_handle_size = 12.0f;
    int toggle_key = KEY_F2;
    bool toggle_requires_control = false;
    std::string config_path = "ui_layout.cfg";

    std::unordered_map<std::string,UIRuntimeOverride> overrides;
    std::string selected_key;
    std::string active_key;
    UIRuntimeInteraction interaction = UIRuntimeInteraction::None;
    Vector2 drag_start_mouse = {};
    Vector2 drag_start_offset = {};
    Vector2 drag_start_size = {};
    bool dirty = false;
};

struct UIRuntimeTarget {
    std::string key;
    UIElementBase* rect = nullptr;
    ComputedStyle* style = nullptr;
    UIContainer* container = nullptr;
    UIContainer* parent = nullptr;
    SelectorType selector = SelectorType::Any;
    int depth = 0;
    bool visible = true;
};

std::string CSSRuntimeId(const CSS_Id& id);

const char* RuntimeSelectorName(SelectorType selector);

//--------------------------------------------------------------------
// Keyed rebuilding and reconciliation
//--------------------------------------------------------------------

struct UIReconcileOptions {
    bool preserve_values = false;
    bool preserve_selection = true;
    bool preserve_open_state = true;
    bool preserve_runtime_layout = true;
    bool match_unkeyed_by_index = true;
};

std::string UIElementStableKey(const UIElement& element);

void PreserveUIElementBase(
    UIElementBase& destination,
    const UIElementBase& source,
    const UIReconcileOptions& options
);

void ReconcileUI(
    UIContainer& destination,
    UIContainer rebuilt,
    const UIReconcileOptions& options = {}
);

void PreserveUIElementState(
    UIElement& destination,
    UIElement& source,
    const UIReconcileOptions& options
);

void ReconcileUI(
    UIContainer& destination,
    UIContainer rebuilt,
    const UIReconcileOptions& options
);

template <typename Builder>
inline void RebuildUI(
    UIContainer& destination,
    Builder&& builder,
    const UIReconcileOptions& options = {}
) {
    UIContainer rebuilt;
    builder(rebuilt);
    ReconcileUI(destination,std::move(rebuilt),options);
}

UIElement* FindUIElement(UIContainer& container, const std::string& key);

template <typename Widget>
inline Widget* GetUIWidget(UIElement& element) {
    if constexpr(std::is_same_v<Widget,UIContainer>) {
        auto* value = std::get_if<std::shared_ptr<UIContainer>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UICollapsible>) {
        auto* value = std::get_if<std::shared_ptr<UICollapsible>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UITreeNode>) {
        auto* value = std::get_if<std::shared_ptr<UITreeNode>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UIMenu>) {
        auto* value = std::get_if<std::shared_ptr<UIMenu>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UIPopup>) {
        auto* value = std::get_if<std::shared_ptr<UIPopup>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UIWindow>) {
        auto* value = std::get_if<std::shared_ptr<UIWindow>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_same_v<Widget,UITabBar>) {
        auto* value = std::get_if<std::shared_ptr<UITabBar>>(&element);
        return value != nullptr && *value ? value->get() : nullptr;
    }
    else if constexpr(std::is_base_of_v<IUICustomElement,Widget>) {
        auto* value = std::get_if<std::shared_ptr<IUICustomElement>>(&element);
        return value != nullptr && *value ? dynamic_cast<Widget*>(value->get()) : nullptr;
    }
    else {
        return std::get_if<Widget>(&element);
    }
}

template <typename Widget>
inline Widget* FindUI(UIContainer& container, const std::string& key) {
    UIElement* element = FindUIElement(container,key);
    return element != nullptr ? GetUIWidget<Widget>(*element) : nullptr;
}

template <typename Visitor>
inline void VisitRuntimeElements(
    UIContainer& parent,
    Visitor&& visitor,
    const std::string& parent_path = "root",
    int depth = 1,
    bool parent_visible = true
) {
    for(std::size_t index=0; index<parent.elements.size(); ++index) {
        std::visit(
            [&](auto& widget) {
                using T = std::decay_t<decltype(widget)>;
                UIRuntimeTarget target;
                target.parent = &parent;
                target.depth = depth;
                target.visible = parent_visible;
                bool children_visible = parent_visible;

                if constexpr(std::is_same_v<T,UIButton>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Button;
                }
                else if constexpr(std::is_same_v<T,UILabel>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Label;
                }
                else if constexpr(IsUIInputV<T>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Input;
                }
                else if constexpr(std::is_same_v<T,UISelectable>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Selectable;
                }
                else if constexpr(std::is_same_v<T,UIComboBox>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::ComboBox;
                }
                else if constexpr(IsUIRangeInputV<T>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::RangeInput;
                }
                else if constexpr(std::is_same_v<T,UICanvas>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Canvas;
                }
                else if constexpr(std::is_same_v<T,UIImage>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Image;
                }
                else if constexpr(std::is_same_v<T,UISplitter>) {
                    target.rect = &widget.rect;
                    target.style = &widget.style;
                    target.selector = SelectorType::Splitter;
                }
                else if constexpr(IsUICustomElementPointerV<T>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.selector = widget->Selector();
                    if(auto* viewport = dynamic_cast<UIViewport*>(widget.get())) {
                        target.container = &viewport->content;
                        // Descendants use world coordinates. Keep them available
                        // to lookup/animation traversal, but out of the root F3
                        // screen-space layout editor until that editor gains a
                        // transform-aware target descriptor.
                        children_visible = false;
                    }
                    else if(auto* item = dynamic_cast<UIViewportItem*>(widget.get())) {
                        target.container = &item->content;
                        children_visible = children_visible && item->visible;
                    }
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = widget.get();
                    target.selector = SelectorType::Container;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = &widget->content;
                    target.selector = SelectorType::Collapsible;
                    children_visible = parent_visible && widget->open;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = &widget->content;
                    target.selector = SelectorType::TreeNode;
                    children_visible = parent_visible && widget->open;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = &widget->content;
                    target.selector = SelectorType::Menu;
                    children_visible = parent_visible && widget->open;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = &widget->content;
                    target.selector = SelectorType::Popup;
                    target.visible = parent_visible && widget->open;
                    children_visible = target.visible;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.container = &widget->content;
                    target.selector = SelectorType::Window;
                    target.visible = parent_visible && widget->open;
                    children_visible = target.visible && !widget->collapsed;
                }
                else if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                    if(!widget) return;
                    target.rect = &widget->rect;
                    target.style = &widget->style;
                    target.selector = SelectorType::TabBar;
                    if(UITab* tab = widget->SelectedTab()) {
                        target.container = &tab->content;
                    }
                }

                if(target.rect == nullptr || target.style == nullptr) return;
                target.visible =
                    target.visible &&
                    target.style->display != DisplayType::None;
                children_visible = children_visible && target.visible;

                const std::string path = parent_path + "/" +
                    std::to_string(index) + ":" + RuntimeSelectorName(target.selector);
                target.key = CSSRuntimeId(target.style->id);
                if(target.key.empty() && !target.rect->key.empty()) {
                    target.key = target.rect->key;
                }
                if(target.key.empty()) target.key = path;

                visitor(target);

                if(target.container != nullptr) {
                    VisitRuntimeElements(
                        *target.container,
                        visitor,
                        path,
                        depth+1,
                        children_visible
                    );
                }
                if constexpr(IsUICustomElementPointerV<T>) {
                    if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                        for(UIDockPanel& panel : dock->panels) {
                            VisitRuntimeElements(
                                panel.content,visitor,
                                path+"/panel:"+panel.id,
                                depth+1,
                                children_visible && panel.open
                            );
                        }
                    }
                }
            },
            parent.elements[index]
        );
    }
}

void ApplyRuntimeOverrides(
    UIContainer& root,
    UIRuntimeLayer& layer
);

bool LoadRuntimeLayout(
    UIRuntimeLayer& layer,
    UIContainer& root,
    const std::string& path = {}
);

bool SaveRuntimeLayout(const UIRuntimeLayer& layer);

void ResetRuntimeLayout(
    UIRuntimeLayer& layer,
    UIContainer& root
);

std::size_t ComputeContainerLayoutSignature(
    const UIContainer& container
);

void MarkLayoutDirty(
    UIContainer& container,
    bool recursive
);


// Convert the RCSS/C++ border-radius representation to a physical pixel
// radius or to raylib's normalized DrawRectangleRounded() roundness.
//
//   0..1   -> legacy normalized roundness (backward compatible)
//   < 0    -> RCSS pixel radius encoded by the parser (e.g. 10px -> -10)
//   > 1    -> pixel radius (convenient for the C++ fluent API)
inline float ResolveUIBorderRadiusPixels(Rectangle rectangle, float radius) {
    const float min_side = std::min(rectangle.width,rectangle.height);
    if(min_side <= 0.0f || radius == 0.0f) return 0.0f;

    const float maximum = min_side*0.5f;
    if(radius < 0.0f) return std::clamp(-radius,0.0f,maximum);
    if(radius <= 1.0f) return maximum*std::clamp(radius,0.0f,1.0f);
    return std::clamp(radius,0.0f,maximum);
}

inline float ResolveUIBorderRoundness(Rectangle rectangle, float radius) {
    const float min_side = std::min(rectangle.width,rectangle.height);
    if(min_side <= 0.0f) return 0.0f;
    return std::clamp(
        (ResolveUIBorderRadiusPixels(rectangle,radius)*2.0f)/min_side,
        0.0f,
        1.0f
    );
}

// Effective CSS widths are returned as left/right/top/bottom fields.
// A legacy scalar-only style is treated as a uniform border.
EdgeValues EffectiveBorderWidths(const ComputedStyle& style);
float MaximumBorderWidth(const ComputedStyle& style);
void DrawStyleBorder(
    Rectangle rectangle,
    const ComputedStyle& style,
    Color color,
    int segments = 8
);

float GetTextX(Rectangle content, float text_width, TextAlign alignment);

bool HasAllCSSClasses(
    const std::vector<std::string>& element_classes,
    const std::vector<std::string>& rule_classes
);

bool SelectorMatches(
    const StyleRule& rule,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    const std::vector<StyleSelector>& ancestors = {},
    const std::vector<StyleSelectorSiblingChain>&
        ancestor_previous_siblings = {},
    StyleSelectorSiblingChain previous_siblings = {},
    std::size_t sibling_index = 0,
    std::size_t sibling_count = 0
);

int GetSelectorSpecificity(const StyleRule& rule);

//--------------------------------------------------------------------
// Style resolution
//--------------------------------------------------------------------

template <typename T>
inline std::optional<T> ResolveValue(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    PropertyType property_type
) {
    std::optional<T> value;
    int best_specificity = -1;
    const std::size_t selector_index = static_cast<std::size_t>(selector_type);
    const auto apply_rule = [&](const StyleRule& style_rule) {
        if(!SelectorMatches(style_rule,selector_type,id,classes,selector_state)) return;
        const int specificity = style_rule.specificity;
        for(const auto& declaration : style_rule.declarations) {
            if(declaration.property != property_type) continue;
            if(!std::holds_alternative<T>(declaration.value)) continue;
            if(specificity >= best_specificity) {
                value = std::get<T>(declaration.value);
                best_specificity = specificity;
            }
        }
    };
    if(selector_index < style_sheet.rules_by_selector.size()) {
        for(std::size_t index : style_sheet.rules_by_selector[selector_index]) {
            apply_rule(style_sheet.rules[index]);
        }
    }
    else {
        for(const StyleRule& rule : style_sheet.rules) apply_rule(rule);
    }
    return value;
}

std::optional<CSSLength> ResolveLengthValue(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& classes,
    SelectorState selector_state,
    PropertyType property_type
);

float ResolveCSSLength(
    const CSSLength& length,
    float percent_reference,
    float font_size = 16.0f,
    float root_font_size = 16.0f,
    const UIStyleVariableMap* variables = nullptr
);

float ComputedMetricScale(
    const std::optional<CSSLength>& source,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale
);

void ScaleComputedStyleMetrics(
    ComputedStyle& style,
    float inherited_scale,
    float local_scale,
    bool relative_reference_includes_local_scale,
    bool inverse = false
);

void ResetComputedStyleScale(ComputedStyle& style);

void ApplyComputedStyleScale(
    ComputedStyle& style,
    float inherited_scale,
    bool relative_reference_includes_local_scale = false
);

float EffectiveComputedStyleScale(const ComputedStyle& style);

// Modern RCSS paint/effect helpers. These remain no-ops on ordinary styles.
bool RequiresUICompositing(const ComputedStyle& style);
float UIEffectExpansion(const ComputedStyle& style, const Rectangle& element_bounds);
Rectangle ExpandUIPaintBounds(const Rectangle& bounds, const ComputedStyle& style);
void ClearUIEffectResources();
// Captures and composites only when the style requires it. Returns true when
// the callback was rendered through an offscreen effect layer. On the normal
// fast path it returns false without invoking the callback.
bool DrawUICompositedLayer(
    UIElementBase& owner,
    const ComputedStyle& style,
    Rectangle paint_bounds,
    const std::function<void()>& draw_subtree
);

bool StyleMayUseGeneratedPseudo(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& owner_style,
    UIGeneratedPseudoKind kind
);
bool StyleMayUseCompositing(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& style
);
const ComputedStyle* ResolveGeneratedPseudoStyleRef(
    UIElementBase& owner,
    const ComputedStyle& owner_style,
    SelectorType selector,
    const StyleSheet& style_sheet,
    UIGeneratedPseudoKind kind
);
void DrawGeneratedPseudoElement(
    UIElementBase& owner,
    const ComputedStyle& owner_style,
    SelectorType selector,
    const StyleSheet& style_sheet,
    UIGeneratedPseudoKind kind
);

void ResolveComputedLengths(
    ComputedStyle& style,
    float parent_width,
    float parent_height,
    float parent_font_size = 16.0f,
    float inherited_scale = 1.0f,
    bool relative_reference_includes_local_scale = false
);

void ApplyResolvedDeclaration(
    ComputedStyle& style,
    const Declaration& declaration
);

ComputedStyle ResolveStyleUncached(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style = {}
);

ComputedStyle ResolveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    SelectorState selector_state,
    ComputedStyle style = {}
);

ComputedStyle ResolveStyleWithPseudoStates(
    const StyleSheet& style_sheet,
    SelectorType selector_type,
    const CSS_Id& id,
    const std::vector<std::string>& active_states,
    ComputedStyle style = {}
);

const ComputedStyle& ResolveAnimatedStyleRef(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    const ComputedStyle& target_style,
    const std::string& channel = {},
    std::size_t target_revision = 0
);

ComputedStyle ResolveAnimatedStyle(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    ComputedStyle target_style,
    const std::string& channel = {}
);

// Layout animation is intentionally evaluated in a separate pass. It uses the
// same RCSS transition/keyframe declarations, but only layout-affecting
// properties and marks the owning layout subtree dirty while interpolation is
// active.
ComputedStyle ResolveLayoutAnimatedStyle(
    UIElementBase& element,
    const StyleSheet& style_sheet,
    ComputedStyle target_style
);

struct UILayoutFrameActivity {
    bool pseudo_changed = false;
    bool animation_active = false;
};

UILayoutFrameActivity RefreshUILayoutFrameActivity(
    UIContainer& root,
    bool update_pseudo_states = true,
    bool check_layout_animations = true
);
bool IsUILayoutAnimationActive(const UIElementBase& element);
bool HasActiveUILayoutAnimations(const UIContainer& root);
bool RefreshUIPseudoStatesForLayout(UIContainer& root);

enum class UIAnimationCommand {
    Play, Pause, Resume, Stop, Restart, Reverse
};
void QueueUILayoutAnimationCommand(
    UIElementBase& element,
    const std::string& name,
    UIAnimationCommand command,
    bool restart = true
);

bool PlayUIAnimation(
    UIElementBase& element,
    const std::string& name,
    bool restart = true
);
bool PauseUIAnimation(UIElementBase& element, const std::string& name);
bool ResumeUIAnimation(UIElementBase& element, const std::string& name);
bool StopUIAnimation(UIElementBase& element, const std::string& name);
bool RestartUIAnimation(UIElementBase& element, const std::string& name);
bool ReverseUIAnimation(UIElementBase& element, const std::string& name);
bool IsUIAnimationPlaying(const UIElementBase& element, const std::string& name);

UIElementBase* FindUIElementByKey(UIContainer& root, const std::string& key);
const UIElementBase* FindUIElementByKey(const UIContainer& root, const std::string& key);

//--------------------------------------------------------------------
// Layout
//--------------------------------------------------------------------

bool RuntimeStyleFlag(
    const std::optional<bool>& value,
    bool fallback
);

void ApplyRuntimeSizeToElement(
    UIElementBase& rect,
    const ComputedStyle& style,
    Rectangle parent_bounds
);

void ApplyRuntimePositionToElement(
    UIElementBase& rect,
    const ComputedStyle& style,
    Rectangle parent_bounds
);

std::vector<float> ResolveGridTrackSizes(
    const GridTrackList& source_tracks,
    const GridTrackSize& automatic_track,
    int required_count,
    float available_space,
    float gap
);

float GridTrackOffset(
    const std::vector<float>& tracks,
    int index,
    float gap
);

float GridTrackSpan(
    const std::vector<float>& tracks,
    int start,
    int span,
    float gap
);

void LayoutContainer(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    float containing_width = -1.0f,
    float containing_height = -1.0f,
    bool force_layout = false
);

// Concrete input overloads keep template implementations out of consumer translation units.
void UpdateInput(UIInput<float>& input);
void UpdateInput(UIInput<int>& input);

float* GetVectorComponent(Vector2& value, int index);
float* GetVectorComponent(Vector3& value, int index);
float* GetVectorComponent(Vector4& value, int index);
const float* GetVectorComponent(const Vector2& value, int index);
const float* GetVectorComponent(const Vector3& value, int index);
const float* GetVectorComponent(const Vector4& value, int index);
unsigned char* GetColorComponent(Color& value, int index);
const unsigned char* GetColorComponent(const Color& value, int index);

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline std::string FormatInputValue(const T& value) {
    if constexpr(std::is_same_v<T, int>) {
        return TextFormat("%d", value);
    }
    else if constexpr(std::is_floating_point_v<T>) {
        return TextFormat("%.2f", value);
    }
    else if constexpr(std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    }
    else if constexpr(std::is_same_v<T, std::string>) {
        return value;
    }
    else {
        return "";
    }
}

template <typename T>
inline void ResetInputFrameState(UIInput<T>& input) {
    input.changed = false;
    input.clicked = false;
    input.triggered = false;
    input.pressed = false;
    input.released = false;
    input.submitted = false;
    input.decrement_hovered = false;
    input.increment_hovered = false;
}

template <typename T>
inline bool UpdateTriggerInput(UIInput<T>& input) {
    ResetInputFrameState(input);

    if(input.input_type != InputType::Trigger) {
        return false;
    }

    Rectangle rect = input.rect.transform;
    Vector2 mouse = GetUIMousePosition();

    input.hovered = CheckCollisionPointRec(mouse, rect);

    if(input.hovered) {
        CSS_UI_REQUESTED_CURSOR=MOUSE_CURSOR_POINTING_HAND;
    }

    if(input.hovered &&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input.pressed = true;
        input.pressed_inside = true;
        input.focused = true;
    }

    input.active =input.pressed_inside &&IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.released = input.pressed_inside;
        input.clicked =input.pressed_inside &&input.hovered;
        input.triggered = input.clicked;
        input.changed = input.clicked;
        input.pressed_inside = false;
        input.active = false;
    }
    return true;
}

template <typename T>
inline constexpr bool SupportsInputStepButtons =
    std::is_arithmetic_v<T> &&
    !std::is_same_v<T,bool>;

template <typename T>
inline float GetInputStepButtonWidth(const UIInput<T>& input) {
    if constexpr(!SupportsInputStepButtons<T>) {
        return 0.0f;
    }
    else {
        if(!input.show_step_buttons) {
            return 0.0f;
        }

        Rectangle rect = input.rect.transform;

        return std::min(
            rect.height,
            rect.width * 0.25f
        );
    }
}

template <typename T>
inline Rectangle GetInputDecrementRect(
    const UIInput<T>& input,
    InputStepButtonLayout layout
) {
    Rectangle rect = input.rect.transform;
    const float width = GetInputStepButtonWidth(input);

    switch(layout) {
        case InputStepButtonLayout::Front:
            return {
                rect.x + rect.width - width,
                rect.y,
                width,
                rect.height
            };
        case InputStepButtonLayout::Back:
            return {
                rect.x,
                rect.y,
                width,
                rect.height
            };

        case InputStepButtonLayout::Opposite:
        default:
            return {
                rect.x,
                rect.y,
                width,
                rect.height
            };
    }
}

template <typename T>
inline Rectangle GetInputIncrementRect(
    const UIInput<T>& input,
    InputStepButtonLayout layout
) {
    Rectangle rect = input.rect.transform;
    const float width = GetInputStepButtonWidth(input);

    switch(layout) {
        case InputStepButtonLayout::Front:
            return {
                rect.x + rect.width - width * 2.0f,
                rect.y,
                width,
                rect.height
            };

        case InputStepButtonLayout::Back:
            return {
                rect.x + width,
                rect.y,
                width,
                rect.height
            };

        case InputStepButtonLayout::Opposite:
        default:
            return {
                rect.x + rect.width - width,
                rect.y,
                width,
                rect.height
            };
    }
}

template <typename T>
inline Rectangle GetInputValueRect(const UIInput<T>& input) {
    Rectangle rect = input.rect.transform;

    if constexpr(!SupportsInputStepButtons<T>) {
        return rect;
    }
    else {
        if(!input.show_step_buttons) {
            return rect;
        }

        const float button_width =
            GetInputStepButtonWidth(input);

        switch(input.step_button_layout) {
            case InputStepButtonLayout::Front:
                rect.width -= button_width * 2.0f;
                break;

            case InputStepButtonLayout::Back:
                rect.x += button_width * 2.0f;
                rect.width -= button_width * 2.0f;
                break;

            case InputStepButtonLayout::Opposite:
            default:
                rect.x += button_width;
                rect.width -= button_width * 2.0f;
                break;
        }

        rect.width = std::max(0.0f, rect.width);
        return rect;
    }
}

template <typename T>
inline T ClampInputValue(const UIInput<T>& input, T value) {
    if constexpr(SupportsInputStepButtons<T>) {
        if(input.min < input.max) {
            return std::clamp(value,input.min,input.max);
        }
    }

    return value;
}

template <typename T>
inline T SnapInputValue(const UIInput<T>& input, T value) {
    if constexpr(SupportsInputStepButtons<T>) {
        if(input.step > T{} && input.min <= input.max) {
            const double minimum = static_cast<double>(input.min);
            const double step = static_cast<double>(input.step);
            const double snapped =
                minimum +
                std::round(
                    (static_cast<double>(value) - minimum) / step
                ) * step;

            if constexpr(std::is_integral_v<T>) {
                value = static_cast<T>(std::llround(snapped));
            }
            else {
                value = static_cast<T>(snapped);
            }
        }
    }

    return ClampInputValue(input,value);
}

template <typename T>
inline Rectangle GetSliderInteractionRect(const UIInput<T>& input) {
    Rectangle value_rect = GetInputValueRect(input);
    Rectangle content = {
        value_rect.x + input.style.padding.left,
        value_rect.y + input.style.padding.top,
        std::max(
            0.0f,
            value_rect.width -
                input.style.padding.left -
                input.style.padding.right
        ),
        std::max(
            0.0f,
            value_rect.height -
                input.style.padding.top -
                input.style.padding.bottom
        )
    };

    if(input.slider_show_value) {
        if(input.slider_orientation == UISliderOrientation::Vertical) {
            const float reserved = std::min(
                content.height,
                std::max(0.0f,input.style.font_size + 4.0f)
            );

            content.height = std::max(
                0.0f,
                content.height - reserved - 8.0f
            );
        }
        else {
            const float reserved = std::min(
                content.width,
                std::max(0.0f,input.slider_value_width)
            );

            content.width = std::max(
                0.0f,
                content.width - reserved - 8.0f
            );
        }
    }

    return content;
}

template <typename T>
inline float GetSliderNormalizedFromPoint(
    const UIInput<T>& input,
    Rectangle slider_rect,
    Vector2 point
) {
    if(input.slider_orientation == UISliderOrientation::Vertical) {
        if(slider_rect.height <= 0.0f) return 0.0f;

        return 1.0f - std::clamp(
            (point.y-slider_rect.y) / slider_rect.height,
            0.0f,
            1.0f
        );
    }

    if(slider_rect.width <= 0.0f) return 0.0f;

    return std::clamp(
        (point.x-slider_rect.x) / slider_rect.width,
        0.0f,
        1.0f
    );
}

template <typename T>
inline float GetInputNormalizedValue(const UIInput<T>& input) {
    const double range =
        static_cast<double>(input.max) -
        static_cast<double>(input.min);

    if(std::abs(range) <= 0.0000001) {
        return 0.0f;
    }

    return std::clamp(
        static_cast<float>(
            (
                static_cast<double>(input.value) -
                static_cast<double>(input.min)
            ) / range
        ),
        0.0f,
        1.0f
    );
}

template <typename T>
inline void SetInputFromNormalized(
    UIInput<T>& input,
    float normalized
) {
    normalized = std::clamp(normalized,0.0f,1.0f);

    const double calculated =
        static_cast<double>(input.min) +
        static_cast<double>(normalized) *
        (
            static_cast<double>(input.max) -
            static_cast<double>(input.min)
        );

    T next{};

    if constexpr(std::is_integral_v<T>) {
        next = static_cast<T>(std::llround(calculated));
    }
    else {
        next = static_cast<T>(calculated);
    }

    next = SnapInputValue(input,next);

    if(next != input.value) {
        input.value = next;
        input.changed = true;
    }
}

template <typename T>
inline void StepInputValue(UIInput<T>& input, int direction) {
    if constexpr(SupportsInputStepButtons<T>) {
        T old_value = input.value;
        input.value = SnapInputValue(
            input,
            static_cast<T>(
                input.value +
                input.step * static_cast<T>(direction)
            )
        );
        input.changed = input.value != old_value;

        if(input.focused) {
            input.edit_buffer = FormatInputValue(input.value);
            input.caret_frame_counter = 0;
        }
    }
}

template <typename T>
inline bool UpdateInputStepButtons(UIInput<T>& input) {
    if constexpr(!SupportsInputStepButtons<T>) {
        return false;
    }
    else {
        if(!input.show_step_buttons) {
            return false;
        }

        Vector2 mouse = GetUIMousePosition();
        Rectangle increment = GetInputIncrementRect(input,input.step_button_layout);
        Rectangle decrement = GetInputDecrementRect(input,input.step_button_layout);
        input.decrement_hovered =
            CheckCollisionPointRec(mouse,decrement);

        input.increment_hovered =
            CheckCollisionPointRec(mouse,increment);

        if(input.decrement_hovered || input.increment_hovered) {
            CSS_UI_REQUESTED_CURSOR=MOUSE_CURSOR_POINTING_HAND;
        }

        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(input.decrement_hovered) {
                StepInputValue(input,-1);
                input.focused = false;
                input.dragging = false;
                return true;
            }

            if(input.increment_hovered) {
                StepInputValue(input,1);
                input.focused = false;
                input.dragging = false;
                return true;
            }
        }

        return input.decrement_hovered || input.increment_hovered;
    }
}

float* GetVectorComponent(Vector2& value, int index);

float* GetVectorComponent(Vector3& value, int index);

float* GetVectorComponent(Vector4& value, int index);

const float* GetVectorComponent(const Vector2& value, int index);

const float* GetVectorComponent(const Vector3& value, int index);

const float* GetVectorComponent(const Vector4& value, int index);

unsigned char* GetColorComponent(Color& value, int index);

const unsigned char* GetColorComponent(
    const Color& value,
    int index
);

template <typename T>
inline constexpr int VectorComponentCount = 0;

template <>
inline constexpr int VectorComponentCount<Vector2> = 2;

template <>
inline constexpr int VectorComponentCount<Vector3> = 3;

template <>
inline constexpr int VectorComponentCount<Vector4> = 4;

template <typename T>
inline bool RegisterDragInputClick(
    UIInput<T>& input,
    Vector2 position,
    int component = -1
) {
    constexpr double kDoubleClickSeconds = 0.30;
    constexpr float kDoubleClickDistance = 5.0f;

    const double now = GetTime();
    const float dx = position.x-input.last_drag_click_position.x;
    const float dy = position.y-input.last_drag_click_position.y;
    const bool same_component = component == input.last_drag_click_component;
    const bool close_enough =
        dx*dx+dy*dy <= kDoubleClickDistance*kDoubleClickDistance;
    const bool double_click =
        input.last_drag_click_time >= 0.0 &&
        now-input.last_drag_click_time <= kDoubleClickSeconds &&
        same_component &&
        close_enough;

    // Consume the completed pair so a triple click does not immediately
    // start another edit session.
    input.last_drag_click_time = double_click ? -1.0 : now;
    input.last_drag_click_position = position;
    input.last_drag_click_component = component;
    return double_click;
}

template <typename T>
inline bool UIInputHasTextSelection(const UIInput<T>& input) {
    return input.text_selection_end > input.text_selection_start;
}

template <typename T>
inline void UIInputNormalizeTextSelection(UIInput<T>& input, std::size_t size) {
    input.text_caret = std::min(input.text_caret,size);
    input.text_anchor = std::min(input.text_anchor,size);
    input.text_selection_start = std::min(input.text_anchor,input.text_caret);
    input.text_selection_end = std::max(input.text_anchor,input.text_caret);
    input.drag_text_select_all =
        input.text_selection_start == 0 &&
        input.text_selection_end == size &&
        size > 0;
}

template <typename T>
inline void UIInputCollapseTextSelection(UIInput<T>& input, std::size_t caret) {
    input.text_caret = caret;
    input.text_anchor = caret;
    input.text_selection_start = caret;
    input.text_selection_end = caret;
    input.drag_text_select_all = false;
}

template <typename T>
inline void UIInputSelectAllText(UIInput<T>& input, const std::string& buffer) {
    input.text_anchor = 0;
    input.text_caret = buffer.size();
    input.text_selection_start = 0;
    input.text_selection_end = buffer.size();
    input.drag_text_select_all = !buffer.empty();
    input.caret_frame_counter = 0;
}

template <typename T>
inline bool UIInputDeleteTextSelection(UIInput<T>& input, std::string& buffer) {
    UIInputNormalizeTextSelection(input,buffer.size());
    if(!UIInputHasTextSelection(input)) return false;
    const std::size_t start = input.text_selection_start;
    buffer.erase(start,input.text_selection_end-start);
    UIInputCollapseTextSelection(input,start);
    input.caret_frame_counter = 0;
    return true;
}
template <typename T>
inline bool UIInputCopyTextSelection(UIInput<T>& input, std::string& buffer) {
    UIInputNormalizeTextSelection(input, buffer.size());
    if (!UIInputHasTextSelection(input)) return false;

    const std::size_t start = input.text_selection_start;
    const std::size_t length = input.text_selection_end - start;

    SetClipboardText(buffer.substr(start, length).c_str());

    return true;
}

inline float UIInputMeasureTextPrefix(
    Font font,
    const std::string& text,
    std::size_t count,
    float font_size
) {
    count = std::min(count,text.size());
    if(count == 0) return 0.0f;
    return MeasureTextEx(
        font,
        text.substr(0,count).c_str(),
        font_size,
        1.0f
    ).x;
}

template <typename T>
inline void DrawUIInputTextSelectionAndCaret(
    UIInput<T>& input,
    Font font,
    const std::string& text,
    Vector2 position,
    float font_size,
    Color text_color,
    bool editing
) {
    if(!editing) return;
    UIInputNormalizeTextSelection(input,text.size());
    if(UIInputHasTextSelection(input)) {
        const float start_x = UIInputMeasureTextPrefix(
            font,text,input.text_selection_start,font_size
        );
        const float end_x = UIInputMeasureTextPrefix(
            font,text,input.text_selection_end,font_size
        );
        const UIFontVerticalExtent extent = MeasureFontVerticalExtent(font,font_size);
        DrawRectangleRec(
            Rectangle{
                position.x+start_x-1.0f,
                position.y+extent.top-1.0f,
                std::max(2.0f,end_x-start_x+2.0f),
                extent.Height()+2.0f
            },
            Color{80,140,255,110}
        );
    }

    constexpr double CARET_BLINK_INTERVAL = 0.53; // seconds per half-cycle

    if(std::fmod(GetTime(), CARET_BLINK_INTERVAL*2.0) < CARET_BLINK_INTERVAL) {
        const float caret_x = UIInputMeasureTextPrefix(
            font,text,input.text_caret,font_size
        );
        const UIFontVerticalExtent extent = MeasureFontVerticalExtent(font,font_size);
        DrawLineEx(
            {position.x+caret_x,position.y+extent.top-1.0f},
            {position.x+caret_x,position.y+extent.bottom+1.0f},
            1.5f,
            text_color
        );
    }
}

inline std::size_t UIInputTextIndexFromMouse(
    Font font,
    const std::string& text,
    float font_size,
    float text_x,
    float mouse_x
) {
    if(text.empty() || mouse_x <= text_x) return 0;
    float previous = 0.0f;
    for(std::size_t index=1; index<=text.size(); ++index) {
        const float width = UIInputMeasureTextPrefix(font,text,index,font_size);
        const float midpoint = (previous+width)*0.5f;
        if(mouse_x-text_x < midpoint) return index-1;
        previous = width;
    }
    return text.size();
}

template <typename T>
inline void UIInputSetCaretFromMouse(
    UIInput<T>& input,
    const std::string& buffer,
    Rectangle text_rect,
    const ComputedStyle& style,
    float mouse_x,
    bool extend_selection
) {
    Font font = ResolveUIFont(style.font);
    const float text_width = MeasureTextEx(
        font,buffer.c_str(),style.font_size,1.0f
    ).x;
    float text_x = text_rect.x;
    if(style.text_align == TextAlign::Center) {
        text_x += std::max(0.0f,(text_rect.width-text_width)*0.5f);
    }
    else if(style.text_align == TextAlign::Right) {
        text_x += std::max(0.0f,text_rect.width-text_width);
    }
    const std::size_t next = UIInputTextIndexFromMouse(
        font,buffer,style.font_size,text_x,mouse_x
    );
    if(!extend_selection) input.text_anchor = next;
    input.text_caret = next;
    UIInputNormalizeTextSelection(input,buffer.size());
    input.caret_frame_counter = 0;
}

template <typename T>
inline void UIInputHandleTextNavigation(UIInput<T>& input, std::string& buffer) {
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if(control && IsKeyPressed(KEY_A)) {
        UIInputSelectAllText(input,buffer);
        return;
    }

    auto move_caret = [&](std::size_t next) {
        next = std::min(next,buffer.size());
        if(!shift) input.text_anchor = next;
        input.text_caret = next;
        UIInputNormalizeTextSelection(input,buffer.size());
        input.caret_frame_counter = 0;
    };

    if(IsKeyPressed(KEY_HOME)) move_caret(0);
    else if(IsKeyPressed(KEY_END)) move_caret(buffer.size());
    else if(IsKeyPressed(KEY_LEFT)) {
        if(!shift && UIInputHasTextSelection(input)) move_caret(input.text_selection_start);
        else move_caret(input.text_caret > 0 ? input.text_caret-1 : 0);
    }
    else if(IsKeyPressed(KEY_RIGHT)) {
        if(!shift && UIInputHasTextSelection(input)) move_caret(input.text_selection_end);
        else move_caret(std::min(buffer.size(),input.text_caret+1));
    }
}

template <typename T>
inline void UIInputInsertTextCharacter(
    UIInput<T>& input,
    std::string& buffer,
    char character
) {
    UIInputDeleteTextSelection(input,buffer);
    if(buffer.size() >= input.max_characters) return;
    const std::size_t caret = std::min(input.text_caret,buffer.size());
    buffer.insert(buffer.begin()+static_cast<std::ptrdiff_t>(caret),character);
    UIInputCollapseTextSelection(input,caret+1);
    input.caret_frame_counter = 0;
}

template <typename T>
inline void UIInputBackspaceText(UIInput<T>& input, std::string& buffer) {
    if(UIInputDeleteTextSelection(input,buffer)) return;
    if(input.text_caret == 0 || buffer.empty()) return;
    const std::size_t erase_at = std::min(input.text_caret,buffer.size())-1;
    buffer.erase(erase_at,1);
    UIInputCollapseTextSelection(input,erase_at);
    input.caret_frame_counter = 0;
}

template <typename T>
inline void UIInputDeleteText(UIInput<T>& input, std::string& buffer) {
    if(UIInputDeleteTextSelection(input,buffer)) return;
    if(input.text_caret >= buffer.size()) return;
    buffer.erase(input.text_caret,1);
    UIInputCollapseTextSelection(input,input.text_caret);
    input.caret_frame_counter = 0;
}
template <typename T>
inline bool UIInputPasteText(UIInput<T>& input, std::string& buffer) {
    const char* clipboard = GetClipboardText();
    if (clipboard == nullptr || clipboard[0] == '\0') return false;

    std::string text = clipboard;

    UIInputNormalizeTextSelection(input, buffer.size());

    std::size_t insertion_index = input.text_caret;
    if (UIInputHasTextSelection(input)) {
        const std::size_t start = input.text_selection_start;
        buffer.erase(start, input.text_selection_end - start);
        UIInputCollapseTextSelection(input, start);
        insertion_index = start;
    }

    const std::size_t available =
        input.max_characters > buffer.size()
            ? input.max_characters - buffer.size()
            : 0;
    if (text.size() > available) {
        text.resize(available);
    }

    buffer.insert(insertion_index, text);
    input.text_caret = insertion_index + text.size();
    UIInputCollapseTextSelection(input, input.text_caret);
    input.caret_frame_counter = 0;

    return !text.empty();
}
template <typename T>
inline bool CommitVectorInputTextEdit(UIInput<T>& input) {
    if(input.active_component < 0) return false;

    std::string& buffer = input.component_buffers[input.active_component];
    if(buffer.empty() || buffer == "-" || buffer == ".") return false;

    float* value_component = GetVectorComponent(input.value,input.active_component);
    const float* minimum_component = GetVectorComponent(input.min,input.active_component);
    const float* maximum_component = GetVectorComponent(input.max,input.active_component);
    const float* step_component = GetVectorComponent(input.step,input.active_component);
    if(
        value_component == nullptr ||
        minimum_component == nullptr ||
        maximum_component == nullptr
    ) return false;

    const float old_value = *value_component;
    try {
        float next_value = std::stof(buffer);
        if(step_component != nullptr && *step_component > 0.0f) {
            next_value =
                *minimum_component +
                std::round(
                    (next_value-*minimum_component) /
                    *step_component
                ) * *step_component;
        }
        if(*minimum_component < *maximum_component) {
            next_value = std::clamp(
                next_value,*minimum_component,*maximum_component
            );
        }
        *value_component = next_value;
    }
    catch(...) {
        return false;
    }

    const bool changed = *value_component != old_value;
    input.changed = input.changed || changed;
    return changed;
}

template <typename T>
inline void UpdateVectorInput(UIInput<T>& input) {
    if(UpdateTriggerInput(input)) {
        return;
    }

    ResetInputFrameState(input);

    Rectangle rect = input.rect.transform;
    Vector2 mouse = GetUIMousePosition();

    input.hovered = CheckCollisionPointRec(mouse,rect);

    constexpr int component_count =
        VectorComponentCount<T>;

    Rectangle content = {
        rect.x + input.style.padding.left,
        rect.y + input.style.padding.top,
        std::max(
            0.0f,
            rect.width -
                input.style.padding.left -
                input.style.padding.right
        ),
        std::max(
            0.0f,
            rect.height -
                input.style.padding.top -
                input.style.padding.bottom
        )
    };

    const float component_width =
        component_count > 0
            ? content.width /
                static_cast<float>(component_count)
            : 0.0f;

    int hovered_component = -1;

    if(
        input.hovered &&
        component_width > 0.0f
    ) {
        hovered_component = std::clamp(
            static_cast<int>(
                (mouse.x - content.x) /
                component_width
            ),
            0,
            component_count - 1
        );
    }

    if(
        input.input_type == InputType::Instant &&
        hovered_component >= 0
    ) {
        CSS_UI_REQUESTED_CURSOR =
            MOUSE_CURSOR_IBEAM;
    }
    else if(
        input.input_type == InputType::Drag &&
        hovered_component >= 0
    ) {
        CSS_UI_REQUESTED_CURSOR =
            input.drag_text_editing &&
            input.active_component == hovered_component
                ? MOUSE_CURSOR_IBEAM
                : MOUSE_CURSOR_RESIZE_EW;
    }
    else if(
        input.input_type == InputType::Slider &&
        hovered_component >= 0
    ) {
        CSS_UI_REQUESTED_CURSOR =
            MOUSE_CURSOR_POINTING_HAND;
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const int previous_component = input.active_component;
        const bool was_drag_text_editing = input.drag_text_editing;
        const bool drag_double_click =
            input.input_type == InputType::Drag &&
            hovered_component >= 0 &&
            RegisterDragInputClick(input,mouse,hovered_component);

        if(
            was_drag_text_editing &&
            (hovered_component < 0 || hovered_component != previous_component)
        ) {
            CommitVectorInputTextEdit(input);
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
        }

        input.focused = input.hovered;
        input.dragging = false;

        if(hovered_component >= 0) {
            input.active_component = hovered_component;

            float* component = GetVectorComponent(
                input.value,input.active_component
            );
            if(
                component != nullptr &&
                !(
                    was_drag_text_editing &&
                    hovered_component == previous_component
                )
            ) {
                input.component_buffers[input.active_component] =
                    TextFormat("%.2f",*component);
            }

            if(drag_double_click) {
                input.drag_text_editing = true;
                input.focused = true;
                input.dragging = false;
                UIInputSelectAllText(
                    input,
                    input.component_buffers[input.active_component]
                );
                input.caret_frame_counter = 0;
            }
            else if(
                input.input_type == InputType::Slider ||
                (
                    input.input_type == InputType::Drag &&
                    !input.drag_text_editing
                )
            ) {
                input.dragging = true;
                input.drag_start_value = input.value;
                input.drag_accumulator = 0.0f;
            }

            if(
                !drag_double_click &&
                (
                    input.input_type == InputType::Instant ||
                    input.drag_text_editing
                )
            ) {
                Rectangle component_rect = {
                    content.x + component_width*static_cast<float>(input.active_component),
                    content.y,
                    component_width,
                    content.height
                };
                const bool extend =
                    IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                UIInputSetCaretFromMouse(
                    input,
                    input.component_buffers[input.active_component],
                    component_rect,
                    input.style,
                    mouse.x,
                    extend
                );
                input.text_mouse_selecting = true;
            }
        }
        else {
            input.active_component = -1;
            if(input.input_type == InputType::Drag) {
                input.drag_text_editing = false;
                input.drag_text_select_all = false;
                input.last_drag_click_time = -1.0;
            }
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.dragging = false;
        input.text_mouse_selecting = false;
    }

    if(
        input.text_mouse_selecting &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        input.focused &&
        input.active_component >= 0 &&
        (input.input_type == InputType::Instant || input.drag_text_editing)
    ) {
        Rectangle component_rect = {
            content.x + component_width*static_cast<float>(input.active_component),
            content.y,
            component_width,
            content.height
        };
        UIInputSetCaretFromMouse(
            input,
            input.component_buffers[input.active_component],
            component_rect,
            input.style,
            mouse.x,
            true
        );
    }

    input.active = input.dragging;

    /*
     * Slider interaction
     */
    if(
        input.input_type == InputType::Slider &&
        input.dragging &&
        input.active_component >= 0
    ) {
        const float component_x =
            content.x +
            component_width *
                static_cast<float>(
                    input.active_component
                );

        float normalized =
            component_width > 0.0f
                ? (
                    mouse.x -
                    component_x
                ) / component_width
                : 0.0f;

        normalized = std::clamp(
            normalized,
            0.0f,
            1.0f
        );
        float* value_component =
            GetVectorComponent(
                input.value,
                input.active_component
            );

        const float* minimum_component =
            GetVectorComponent(
                input.min,
                input.active_component
            );

        const float* maximum_component =
            GetVectorComponent(
                input.max,
                input.active_component
            );

        const float* step_component =
            GetVectorComponent(
                input.step,
                input.active_component
            );

        if(
            value_component != nullptr &&
            minimum_component != nullptr &&
            maximum_component != nullptr
        ) {
            const float old_value =
                *value_component;

            float next_value =
                *minimum_component +
                normalized *
                (
                    *maximum_component -
                    *minimum_component
                );

            if(
                step_component != nullptr &&
                *step_component > 0.0f
            ) {
                next_value =
                    *minimum_component +
                    std::round(
                        (
                            next_value -
                            *minimum_component
                        ) /
                        *step_component
                    ) *
                    *step_component;
            }

            next_value = std::clamp(
                next_value,
                *minimum_component,
                *maximum_component
            );

            *value_component = next_value;

            input.changed =
                *value_component != old_value;
        }
    }

    /*
     * Drag interaction
     */
    if(
        input.input_type == InputType::Drag &&
        !input.drag_text_editing &&
        input.dragging &&
        input.active_component >= 0
    ) {
        float modifier = 1.0f;

        if(
            IsKeyDown(KEY_LEFT_SHIFT) ||
            IsKeyDown(KEY_RIGHT_SHIFT)
        ) {
            modifier = 0.1f;
        }
        else if(
            IsKeyDown(KEY_LEFT_CONTROL) ||
            IsKeyDown(KEY_RIGHT_CONTROL)
        ) {
            modifier = 10.0f;
        }

        float* value_component =
            GetVectorComponent(
                input.value,
                input.active_component
            );

        const float* start_component =
            GetVectorComponent(
                input.drag_start_value,
                input.active_component
            );

        const float* minimum_component =
            GetVectorComponent(
                input.min,
                input.active_component
            );

        const float* maximum_component =
            GetVectorComponent(
                input.max,
                input.active_component
            );

        const float* step_component =
            GetVectorComponent(
                input.step,
                input.active_component
            );

        // Accumulate horizontal mouse movement using the configured drag speed.
        input.drag_accumulator +=
            GetUIMouseDelta().x *
            input.drag_speed *
            modifier;

        if(
            value_component != nullptr &&
            start_component != nullptr &&
            minimum_component != nullptr &&
            maximum_component != nullptr
        ) {
            const float old_value =
                *value_component;

            float next_value =
                *start_component +
                input.drag_accumulator;

            if(
                step_component != nullptr &&
                *step_component > 0.0f
            ) {
                next_value =
                    *minimum_component +
                    std::round(
                        (
                            next_value -
                            *minimum_component
                        ) /
                        *step_component
                    ) *
                    *step_component;
            }

            if(
                *minimum_component < 
                *maximum_component
            ) {
                next_value = std::clamp(
                    next_value,
                    *minimum_component,
                    *maximum_component
                );
            }

            *value_component = next_value;

            input.changed =
                *value_component != old_value;
        }
    }

    /*
     * Mouse-wheel stepping
     */
    if(
        input.hovered &&
        !input.dragging &&
        !input.drag_text_editing &&
        input.active_component >= 0 &&
        (
            input.input_type ==
                InputType::Slider ||
            input.input_type ==
                InputType::Drag
        )
    ) {
        const float wheel =
            GetMouseWheelMove();

        if(wheel != 0.0f) {
            float* value_component =
                GetVectorComponent(
                    input.value,
                    input.active_component
                );

            const float* step_component =
                GetVectorComponent(
                    input.step,
                    input.active_component
                );

            const float* minimum_component =
                GetVectorComponent(
                    input.min,
                    input.active_component
                );

            const float* maximum_component =
                GetVectorComponent(
                    input.max,
                    input.active_component
                );

            if(
                value_component != nullptr &&
                step_component != nullptr &&
                minimum_component != nullptr &&
                maximum_component != nullptr
            ) {
                const float old_value =
                    *value_component;

                const float direction =
                    wheel > 0.0f
                        ? 1.0f
                        : -1.0f;

                float next_value =
                    old_value +
                    *step_component *
                    direction;

                if(
                    *minimum_component <
                    *maximum_component
                ) {
                    next_value = std::clamp(
                        next_value,
                        *minimum_component,
                        *maximum_component
                    );
                }

                *value_component = next_value;

                input.changed =
                    *value_component != old_value;
            }
        }
    }

    /*
     * Keyboard stepping
     */
    if(
        input.focused &&
        !input.drag_text_editing &&
        input.active_component >= 0 &&
        (
            input.input_type ==
                InputType::Slider ||
            input.input_type ==
                InputType::Drag
        )
    ) {
        int direction = 0;

        if(
            IsKeyPressed(KEY_LEFT) ||
            IsKeyPressed(KEY_DOWN)
        ) {
            direction = -1;
        }
        else if(
            IsKeyPressed(KEY_RIGHT) ||
            IsKeyPressed(KEY_UP)
        ) {
            direction = 1;
        }

        if(direction != 0) {
            float* value_component =
                GetVectorComponent(
                    input.value,
                    input.active_component
                );

            const float* step_component =
                GetVectorComponent(
                    input.step,
                    input.active_component
                );

            const float* minimum_component =
                GetVectorComponent(
                    input.min,
                    input.active_component
                );

            const float* maximum_component =
                GetVectorComponent(
                    input.max,
                    input.active_component
                );

            if(
                value_component != nullptr &&
                step_component != nullptr &&
                minimum_component != nullptr &&
                maximum_component != nullptr
            ) {
                const float old_value =
                    *value_component;

                float next_value =
                    old_value +
                    *step_component *
                    static_cast<float>(
                        direction
                    );

                if(
                    *minimum_component <
                    *maximum_component
                ) {
                    next_value = std::clamp(
                        next_value,
                        *minimum_component,
                        *maximum_component
                    );
                }

                *value_component = next_value;

                input.changed =
                    *value_component != old_value;
            }
        }
    }

    /* Keep non-text vector inputs synchronized with their live value. */
    if(
        input.changed &&
        (
            input.input_type == InputType::Slider ||
            (
                input.input_type == InputType::Drag &&
                !input.drag_text_editing
            )
        ) &&
        input.active_component >= 0
    ) {
        const float* component =
            GetVectorComponent(
                input.value,
                input.active_component
            );

        if(component != nullptr) {
            input.component_buffers[
                input.active_component
            ] = TextFormat(
                "%.2f",
                *component
            );
        }
    }

    /*
     * Text editing. Instant inputs enter on a normal click; drag inputs enter
     * only after a double click on the same component.
     */
    if(
        (
            input.input_type == InputType::Instant ||
            (
                input.input_type == InputType::Drag &&
                input.drag_text_editing
            )
        ) &&
        input.focused &&
        input.active_component >= 0
    ) {
        std::string& buffer =
            input.component_buffers[
                input.active_component
            ];

        UIInputHandleTextNavigation(input,buffer);

        int character = GetCharPressed();

        while(character > 0) {
            const bool digit =
                character >= '0' &&
                character <= '9';

            std::string preview = buffer;
            if(UIInputHasTextSelection(input)) {
                preview.erase(
                    input.text_selection_start,
                    input.text_selection_end-input.text_selection_start
                );
            }

            const std::size_t insertion_index = UIInputHasTextSelection(input)
                ? input.text_selection_start
                : input.text_caret;

            const bool minus =
                character == '-' &&
                preview.empty() &&
                insertion_index == 0;

            const bool decimal =
                character == '.' &&
                preview.find('.') == std::string::npos;

            if(digit || minus || decimal) {
                UIInputInsertTextCharacter(
                    input,buffer,static_cast<char>(character)
                );
            }

            character = GetCharPressed();
        }

        if(IsKeyPressed(KEY_BACKSPACE)) {
            UIInputBackspaceText(input,buffer);
        }

        if(IsKeyPressed(KEY_DELETE)) {
            UIInputDeleteText(input,buffer);
        }

        if(IsKeyPressed(KEY_ENTER)) {
            CommitVectorInputTextEdit(input);
            input.submitted = true;
            input.focused = false;
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
            input.text_mouse_selecting = false;
            UIInputCollapseTextSelection(input,0);
            input.last_drag_click_time = -1.0;
            input.active_component = -1;
            buffer.clear();
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            input.focused = false;
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
            input.text_mouse_selecting = false;
            UIInputCollapseTextSelection(input,0);
            input.last_drag_click_time = -1.0;
            input.active_component = -1;
            buffer.clear();
        }
    }
}
#endif // CSS_UI_INTERNAL_TEMPLATES

void UpdateInput(UIInput<Vector2>& input);

void UpdateInput(UIInput<Vector3>& input);

void UpdateInput(UIInput<Vector4>& input);

void UpdateInput(UIInput<bool>& input);

Vector3 ColorToHSV01(Color color);

Color ColorFromHSV01(
    float hue,
    float saturation,
    float brightness,
    unsigned char alpha = 255
);

struct UIColorPickerGeometry {
    Rectangle popup;
    Rectangle saturation_value;
    Rectangle hue;
    Rectangle alpha;
    Rectangle preview;
    Rectangle close_button;
};

UIColorPickerGeometry GetColorPickerGeometry(
    const UIInput<Color>& input
);

void UpdateInput(UIInput<Color>& input);

//--------------------------------------------------------------------
// Input updating
//--------------------------------------------------------------------

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline bool CommitScalarInputTextEdit(UIInput<T>& input) {
    if(
        input.edit_buffer.empty() ||
        input.edit_buffer == "-" ||
        input.edit_buffer == "."
    ) return false;

    const T old_value = input.value;
    try {
        if constexpr(std::is_same_v<T,int>) {
            input.value = std::stoi(input.edit_buffer);
        }
        else if constexpr(std::is_floating_point_v<T>) {
            input.value = static_cast<T>(std::stod(input.edit_buffer));
        }
        input.value = SnapInputValue(input,input.value);
    }
    catch(...) {
        return false;
    }

    const bool changed = input.value != old_value;
    input.changed = input.changed || changed;
    return changed;
}

template <typename T>
inline void UpdateInput(UIInput<T>& input) {
    if(UpdateTriggerInput(input)) {
        return;
    }

    ResetInputFrameState(input);

    Rectangle rect = input.rect.transform;
    Rectangle value_rect = GetInputValueRect(input);
    Rectangle slider_rect = GetSliderInteractionRect(input);
    Vector2 mouse = GetUIMousePosition();

    input.hovered = CheckCollisionPointRec(mouse,rect);

    const bool supports_steps =input.input_type == InputType::Instant ||input.input_type == InputType::Slider ||input.input_type == InputType::Drag;

    bool step_area_hovered =supports_steps? UpdateInputStepButtons(input): false;

    bool value_hovered =!step_area_hovered &&CheckCollisionPointRec(mouse,value_rect);

    bool slider_hovered =input.input_type == InputType::Slider && CheckCollisionPointRec(mouse,slider_rect);
    input.slider_handle_hovered = false;
    const bool vertical_slider =
        input.slider_orientation == UISliderOrientation::Vertical;
    const float slider_extent =
        vertical_slider ? slider_rect.height : slider_rect.width;
    if(input.input_type == InputType::Slider) {
        const float normalized =GetInputNormalizedValue(input);
        Rectangle handle_hitbox{};

        if(vertical_slider) {
            const float handle_margin = std::min(10.0f, slider_rect.height * 0.5f);
            const float handle_y = std::clamp(
                slider_rect.y + slider_rect.height * (1.0f-normalized),
                slider_rect.y + handle_margin,
                slider_rect.y + slider_rect.height - handle_margin
            );
            handle_hitbox = {
                slider_rect.x,
                handle_y - 14.0f,
                slider_rect.width,
                28.0f
            };
        }
        else {
            const float handle_margin = std::min(10.0f, slider_rect.width * 0.5f);
            const float handle_x = std::clamp(
                slider_rect.x + slider_rect.width * normalized,
                slider_rect.x + handle_margin,
                slider_rect.x + slider_rect.width - handle_margin
            );
            handle_hitbox = {
                handle_x - 14.0f,
                slider_rect.y,
                28.0f,
                slider_rect.height
            };
        }

        input.slider_handle_hovered =CheckCollisionPointRec(mouse,handle_hitbox);
    }
    if(input.input_type == InputType::Instant && value_hovered) {
        CSS_UI_REQUESTED_CURSOR=MOUSE_CURSOR_IBEAM;
    }
    else if(input.input_type == InputType::Slider &&(slider_hovered ||input.slider_handle_hovered)) {
        CSS_UI_REQUESTED_CURSOR=MOUSE_CURSOR_POINTING_HAND;
    }
    else if(input.input_type == InputType::Drag &&value_hovered) {
        CSS_UI_REQUESTED_CURSOR =
            input.drag_text_editing
                ? MOUSE_CURSOR_IBEAM
                : MOUSE_CURSOR_RESIZE_EW;
    }
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const bool was_focused = input.focused;
        const bool was_drag_text_editing = input.drag_text_editing;
        const bool drag_double_click =
            input.input_type == InputType::Drag &&
            value_hovered &&
            RegisterDragInputClick(input,mouse);

        if(!step_area_hovered) {
            input.focused =
                value_hovered &&
                (
                    input.input_type == InputType::Instant ||
                    input.input_type == InputType::Drag ||
                    input.input_type == InputType::Slider
                );
        }

        if(
            was_focused &&
            !input.focused &&
            (
                input.input_type == InputType::Instant ||
                was_drag_text_editing
            )
        ) {
            CommitScalarInputTextEdit(input);
            input.edit_buffer.clear();
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
            input.last_drag_click_time = -1.0;
        }

        if(
            input.input_type == InputType::Slider &&
            (slider_hovered || input.slider_handle_hovered)
        ) {
            if(input.slider_click_to_set || input.slider_handle_hovered) {
                input.dragging = true;
                if(input.slider_click_to_set && slider_extent > 0.0f) {
                    SetInputFromNormalized(
                        input,
                        GetSliderNormalizedFromPoint(input,slider_rect,mouse)
                    );
                }
            }
        }

        if(input.input_type == InputType::Drag && value_hovered) {
            if(drag_double_click) {
                input.drag_text_editing = true;
                input.dragging = false;
                input.focused = true;
                input.edit_buffer = FormatInputValue(input.value);
                UIInputSelectAllText(input,input.edit_buffer);
                input.caret_frame_counter = 0;
            }
            else if(!input.drag_text_editing) {
                input.dragging = true;
                input.drag_start_value = input.value;
                input.drag_accumulator = 0.0f;
            }
        }

        if(
            value_hovered &&
            (input.input_type == InputType::Instant || input.drag_text_editing)
        ) {
            if(!was_focused || input.edit_buffer.empty()) {
                input.edit_buffer = FormatInputValue(input.value);
            }
            if(!drag_double_click) {
                Rectangle text_rect = value_rect;
                text_rect.x += input.style.padding.left;
                text_rect.y += input.style.padding.top;
                text_rect.width = std::max(
                    0.0f,
                    text_rect.width-input.style.padding.left-input.style.padding.right
                );
                text_rect.height = std::max(
                    0.0f,
                    text_rect.height-input.style.padding.top-input.style.padding.bottom
                );
                const bool extend =
                    IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                UIInputSetCaretFromMouse(
                    input,input.edit_buffer,text_rect,input.style,mouse.x,extend
                );
                input.text_mouse_selecting = true;
            }
            input.caret_frame_counter = 0;
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.dragging = false;
        input.text_mouse_selecting = false;
    }

    if(
        input.text_mouse_selecting &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        input.focused &&
        (input.input_type == InputType::Instant || input.drag_text_editing)
    ) {
        Rectangle text_rect = value_rect;
        text_rect.x += input.style.padding.left;
        text_rect.y += input.style.padding.top;
        text_rect.width = std::max(
            0.0f,
            text_rect.width-input.style.padding.left-input.style.padding.right
        );
        text_rect.height = std::max(
            0.0f,
            text_rect.height-input.style.padding.top-input.style.padding.bottom
        );
        UIInputSetCaretFromMouse(
            input,input.edit_buffer,text_rect,input.style,mouse.x,true
        );
    }

    input.active = input.dragging;

    if(
        input.input_type == InputType::Slider &&
        input.dragging &&
        slider_extent > 0.0f
    ) {
        SetInputFromNormalized(
            input,
            GetSliderNormalizedFromPoint(
                input,slider_rect,mouse
            )
        );
    }

    if(
        input.input_type == InputType::Drag &&
        !input.drag_text_editing &&
        input.dragging
    ) {
        float modifier = 1.0f;

        if(
            IsKeyDown(KEY_LEFT_SHIFT) ||
            IsKeyDown(KEY_RIGHT_SHIFT)
        ) {
            modifier = 0.1f;
        }
        else if(
            IsKeyDown(KEY_LEFT_CONTROL) ||
            IsKeyDown(KEY_RIGHT_CONTROL)
        ) {
            modifier = 10.0f;
        }

        input.drag_accumulator +=
            GetUIMouseDelta().x *
            input.drag_speed *
            modifier;

        const double requested =
            static_cast<double>(input.drag_start_value) +
            static_cast<double>(input.drag_accumulator);

        T next{};

        if constexpr(std::is_integral_v<T>) {
            next = static_cast<T>(
                std::llround(requested)
            );
        }
        else {
            next = static_cast<T>(requested);
        }

        next = SnapInputValue(input,next);

        if(next != input.value) {
            input.value = next;
            input.changed = true;
        }
    }

    if(
        input.hovered &&
        !input.dragging &&
        !input.drag_text_editing &&
        (
            input.input_type == InputType::Slider ||
            input.input_type == InputType::Drag
        )
    ) {
        float wheel = GetMouseWheelMove();

        if(wheel != 0.0f) {
            StepInputValue(
                input,
                wheel > 0.0f ? 1 : -1
            );
        }
    }

    if(
        input.focused &&
        !input.drag_text_editing &&
        (
            input.input_type == InputType::Slider ||
            input.input_type == InputType::Drag
        )
    ) {
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_DOWN)) {
            StepInputValue(input,-1);
        }

        if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP)) {
            StepInputValue(input,1);
        }
    }

    /* Keep non-text numeric inputs synchronized with their live value. */
    if(
        input.changed &&
        (
            input.input_type == InputType::Slider ||
            (
                input.input_type == InputType::Drag &&
                !input.drag_text_editing
            )
        )
    ) {
        input.edit_buffer =
            FormatInputValue(input.value);
        input.caret_frame_counter = 0;
    }

    if(
        (
            input.input_type == InputType::Instant ||
            (
                input.input_type == InputType::Drag &&
                input.drag_text_editing
            )
        ) &&
        input.focused
    ) {
        input.caret_frame_counter++;
        UIInputHandleTextNavigation(input,input.edit_buffer);

        int character = GetCharPressed();

        while(character > 0) {
            bool digit =
                character >= '0' &&
                character <= '9';

            std::string preview = input.edit_buffer;
            if(UIInputHasTextSelection(input)) {
                preview.erase(
                    input.text_selection_start,
                    input.text_selection_end-input.text_selection_start
                );
            }

            const std::size_t insertion_index = UIInputHasTextSelection(input)
                ? input.text_selection_start
                : input.text_caret;
            bool minus = character == '-' && preview.empty() && insertion_index == 0;

            bool decimal =
                character == '.' &&
                preview.find('.') == std::string::npos;

            if(
                UIInputHasTextSelection(input) ||
                input.edit_buffer.size() < input.max_characters
            ) {
                if(digit || minus) {
                    UIInputInsertTextCharacter(
                        input,input.edit_buffer,static_cast<char>(character)
                    );
                }
                else if constexpr(
                    std::is_floating_point_v<T>
                ) {
                    if(decimal) {
                        UIInputInsertTextCharacter(input,input.edit_buffer,'.');
                    }
                }
            }

            character = GetCharPressed();
        }

        if(IsKeyPressed(KEY_BACKSPACE)) {
            UIInputBackspaceText(input,input.edit_buffer);
        }

        if(IsKeyPressed(KEY_DELETE)) {
            UIInputDeleteText(input,input.edit_buffer);
        }

        if(IsKeyPressed(KEY_ENTER)) {
            CommitScalarInputTextEdit(input);
            input.submitted = true;
            input.focused = false;
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
            input.text_mouse_selecting = false;
            UIInputCollapseTextSelection(input,0);
            input.last_drag_click_time = -1.0;
            input.edit_buffer.clear();
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            input.focused = false;
            input.drag_text_editing = false;
            input.drag_text_select_all = false;
            input.text_mouse_selecting = false;
            UIInputCollapseTextSelection(input,0);
            input.last_drag_click_time = -1.0;
            input.edit_buffer.clear();
        }
    }
}

#endif // CSS_UI_INTERNAL_TEMPLATES

void UpdateInput(UIInput<std::string>& input);

//--------------------------------------------------------------------
// Button updating
//--------------------------------------------------------------------

void UpdateSelectable(UISelectable& selectable);

Rectangle ComboPopupRectangle(const UIComboBox& combo);

void UpdateComboBox(UIComboBox& combo);

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline void UpdateRangeInput(UIRangeInput<T>& input) {
    input.changed = false;
    if(input.disabled || input.max <= input.min) {
        input.hovered = false;
        input.active_handle = 0;
        return;
    }

    const Rectangle rect = input.rect.transform;
    const float track_x = rect.x+input.style.padding.left;
    const float track_width = std::max(
        1.0f,
        rect.width-input.style.padding.left-input.style.padding.right
    );
    const Vector2 mouse = GetUIMousePosition();
    input.hovered = CheckCollisionPointRec(mouse,rect);

    auto value_to_x = [&](T value) {
        const float t = std::clamp(
            (static_cast<float>(value)-static_cast<float>(input.min)) /
                (static_cast<float>(input.max)-static_cast<float>(input.min)),
            0.0f,
            1.0f
        );
        return track_x + t*track_width;
    };
    auto mouse_value = [&]() {
        float t = std::clamp((mouse.x-track_x)/track_width,0.0f,1.0f);
        float value = Lerp(
            static_cast<float>(input.min),
            static_cast<float>(input.max),
            t
        );
        if constexpr(std::is_integral_v<T>) return static_cast<T>(std::round(value));
        else return static_cast<T>(value);
    };

    if(input.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input.focused = true;
        input.active_handle =
            std::fabs(mouse.x-value_to_x(input.lower)) <=
            std::fabs(mouse.x-value_to_x(input.upper)) ? 1 : 2;
    }

    if(input.active_handle != 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        T value = mouse_value();
        if(input.active_handle == 1) {
            T next = std::min(value,input.upper);
            input.changed = next != input.lower;
            input.lower = next;
        }
        else {
            T next = std::max(value,input.lower);
            input.changed = next != input.upper;
            input.upper = next;
        }
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) input.active_handle = 0;
}

#endif // CSS_UI_INTERNAL_TEMPLATES
// Superseded — perspective is now folded directly into style.transform_matrix
// by EvaluateTransformMatrix, as the correct CSS-spec matrix in the same
// local coordinate space as the rest of the transform, rather than as a
// separate RL_PROJECTION camera/frustum switch. That switch-based approach
// is what caused perspective distance to have no visible effect: composing
// a frustum matrix onto raylib's ambient 2D orthographic projection diluted
// the depth term almost to nothing, since the two matrices have wildly
// different numeric scale. Left defined (unused) rather than deleted, in
// case anything still references it.
inline bool PushUIPerspective(const ComputedStyle& style, const Rectangle& bounds) {
    if (!style.perspective) return false;
    float d = ResolveCSSLength(*style.perspective, std::min(bounds.width, bounds.height),
                               style.font_size, CSS_UI_ROOT_FONT_SIZE,
                               style.custom_properties.get());
    if (d <= 0.0f) return false;

    // Switch to projection matrix
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();

    // Create a perspective matrix that matches CSS perspective.
    // The vanishing point is at the center of the element.
    float aspect = bounds.width / bounds.height;
    float fov = 2.0f * atanf((bounds.height / 2.0f) / d);
    float near = 0.1f;
    float far = 1000.0f;
    Matrix proj = MatrixPerspective(fov, aspect, near, far);

    // Apply the perspective matrix relative to the element's center.
    // CSS perspective origin defaults to center, so we translate the view
    // so that the element's center is at the origin of the projection.
    Vector2 center = { bounds.x + bounds.width/2.0f, bounds.y + bounds.height/2.0f };
    rlTranslatef(-center.x, -center.y, 0.0f);
    rlMultMatrixf(MatrixToFloat(proj));
    rlTranslatef(center.x, center.y, 0.0f);

    // Switch back to model-view for drawing children
    rlMatrixMode(RL_MODELVIEW);
    return true;
}
inline void PopUIPerspective() {
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
}
void UpdateRangeInput(UIRangeInput<float>& input);
void UpdateRangeInput(UIRangeInput<int>& input);

void UpdateCanvas(UICanvas& canvas);

void UpdateImage(UIImage& image);

struct UIElementLayoutRef {
    UIElementBase* rect = nullptr;
    ComputedStyle* style = nullptr;
};

UIElementLayoutRef GetUIElementLayoutRef(UIElement& element);

void UpdateSplitter(
    UIContainer& parent,
    std::size_t index,
    UISplitter& splitter
);

void UpdateButton(UIButton& button);

//--------------------------------------------------------------------
// Label drawing
//--------------------------------------------------------------------

void DrawLabel(
    UILabel& label,
    const StyleSheet& style_sheet
);

//--------------------------------------------------------------------
// Button drawing
//--------------------------------------------------------------------

void DrawButton(
    UIButton& button,
    const StyleSheet& style_sheet
);

const ComputedStyle& ResolveInteractiveStyleRef(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& base,
    bool focused,
    bool hovered,
    bool active,
    bool disabled,
    UIElementBase& element,
    std::size_t overlay_hash = 0
);

ComputedStyle ResolveInteractiveStyle(
    const StyleSheet& style_sheet,
    SelectorType selector,
    const ComputedStyle& base,
    bool focused,
    bool hovered,
    bool active,
    bool disabled,
    UIElementBase* element = nullptr,
    const std::string& animation_channel = {},
    std::size_t overlay_hash = 0
);

void DrawStyledBox(
    Rectangle rectangle,
    const ComputedStyle& style
);

void DrawStyledTextContent(
    Rectangle rectangle,
    const std::string& text,
    const ComputedStyle& style,
    TextAlign alignment
);

void DrawStyledTextBox(
    Rectangle rectangle,
    const std::string& text,
    const ComputedStyle& style,
    TextAlign alignment
);

void DrawSelectable(
    UISelectable& selectable,
    const StyleSheet& style_sheet
);

void DrawComboBoxBase(
    UIComboBox& combo,
    const StyleSheet& style_sheet
);

void DrawComboBoxPopup(
    UIComboBox& combo,
    const StyleSheet& style_sheet
);

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline void DrawRangeInput(
    UIRangeInput<T>& input,
    const StyleSheet& style_sheet
) {
    const ComputedStyle& style = ResolveInteractiveStyleRef(
        style_sheet,
        SelectorType::RangeInput,
        input.style,
        input.focused,
        input.hovered,
        input.active_handle != 0,
        input.disabled || input.rect.inherited_disabled,
        input.rect
    );
    struct RangePseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~RangePseudoAfterScope() {
            DrawGeneratedPseudoElement(
                rect,style,SelectorType::RangeInput,sheet,UIGeneratedPseudoKind::After
            );
        }
    } range_pseudo_after{input.rect,style,style_sheet};
    const float visual_scale = EffectiveComputedStyleScale(style);
    DrawStyledBox(input.rect.transform,style);
    DrawGeneratedPseudoElement(
        input.rect,style,SelectorType::RangeInput,style_sheet,UIGeneratedPseudoKind::Before
    );
    Rectangle track = {
        input.rect.transform.x+style.padding.left,
        input.rect.transform.y+input.rect.transform.height*0.5f-2.0f*visual_scale,
        std::max(1.0f,input.rect.transform.width-style.padding.left-style.padding.right),
        4.0f*visual_scale
    };
    auto value_to_x = [&](T value) {
        if(input.max <= input.min) return track.x;
        const float t = std::clamp(
            (static_cast<float>(value)-static_cast<float>(input.min)) /
                (static_cast<float>(input.max)-static_cast<float>(input.min)),
            0.0f,
            1.0f
        );
        return track.x+t*track.width;
    };
    auto resolve_part = [&](ComputedStyle part_style, const char* part_class) {
        if(!StyleSheetUsesPart(style_sheet,part_class)) return part_style;
        AddCSSClass(part_style.classes,part_class);
        part_style = ResolveStyleWithPseudoStates(
            style_sheet,
            SelectorType::RangeInput,
            style.id,
            input.rect.pseudo_states.ActiveNamesView(),
            part_style
        );
        return ResolveAnimatedStyle(
            input.rect,style_sheet,std::move(part_style),part_class
        );
    };
    ComputedStyle track_style = resolve_part(style,CSS_UI_RANGE_TRACK_PART);
    DrawRectangleRounded(track,1.0f,4,track_style.border_color.a ? track_style.border_color : Color{80,84,96,255});
    const float lower_x = value_to_x(input.lower);
    const float upper_x = value_to_x(input.upper);
    ComputedStyle fill_style = resolve_part(style,CSS_UI_RANGE_FILL_PART);
    DrawRectangleRounded(
        {lower_x,track.y,std::max(0.0f,upper_x-lower_x),track.height},
        1.0f,
        4,
        fill_style.background_color.a ? fill_style.background_color : style.text_color
    );
    ComputedStyle handle_style = resolve_part(style,CSS_UI_RANGE_HANDLE_PART);
    Color handle_color = handle_style.background_color.a
        ? handle_style.background_color
        : style.text_color;
    DrawCircleV({lower_x,track.y+track.height*0.5f},7.0f*visual_scale,handle_color);
    DrawCircleV({upper_x,track.y+track.height*0.5f},7.0f*visual_scale,handle_color);
}

#endif // CSS_UI_INTERNAL_TEMPLATES

void DrawRangeInput(
    UIRangeInput<float>& input,
    const StyleSheet& style_sheet
);

void DrawRangeInput(
    UIRangeInput<int>& input,
    const StyleSheet& style_sheet
);

void DrawCanvas(
    UICanvas& canvas,
    const StyleSheet& style_sheet
);

Color MultiplyUIColor(Color first, Color second);

void DrawImage(
    UIImage& image,
    const StyleSheet& style_sheet
);

void DrawSplitter(
    UISplitter& splitter,
    const StyleSheet& style_sheet
);

void DrawHorizontalDragArrows(
    Rectangle rect,
    Color color,
    float visual_scale = 1.0f
);

//--------------------------------------------------------------------
// Input drawing
//--------------------------------------------------------------------

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline const ComputedStyle& ResolveInputDrawStyle(
    UIInput<T>& input,
    const StyleSheet& style_sheet
) {
    return ResolveInteractiveStyleRef(
        style_sheet,SelectorType::Input,input.style,
        input.focused,input.hovered,input.active,
        input.disabled || input.rect.inherited_disabled,input.rect
    );
}

template <typename T>
inline ComputedStyle ResolveInputPartDrawStyle(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    const char* part_class,
    bool hovered,
    bool active,
    bool focused,
    ComputedStyle draw_style
) {
    if(!StyleSheetUsesPart(style_sheet,part_class)) {
        ResolveComputedLengths(
            draw_style,
            input.rect.transform.width,
            input.rect.transform.height,
            input.style.font_size,
            input.style.applied_inherited_scale,
            true
        );
        return draw_style;
    }

    std::vector<std::string> original_classes = draw_style.classes;
    AddCSSClass(draw_style.classes,part_class);

    std::vector<std::string> active_states = input.rect.pseudo_states.ActiveNamesView();
    auto set_state = [&](const char* name, bool enabled) {
        const std::string normalized = NormalizeUIPseudoStateName(name);
        auto found = std::find(active_states.begin(),active_states.end(),normalized);
        if(enabled && found == active_states.end()) active_states.push_back(normalized);
        else if(!enabled && found != active_states.end()) active_states.erase(found);
    };
    set_state("focus",focused);
    set_state("hover",hovered);
    set_state("active",active);
    set_state("disabled",input.disabled || input.rect.inherited_disabled);

    draw_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Input,
        input.style.id,
        active_states,
        draw_style
    );
    draw_style = ResolveAnimatedStyle(
        input.rect,
        style_sheet,
        std::move(draw_style),
        part_class
    );

    ResolveComputedLengths(
        draw_style,
        input.rect.transform.width,
        input.rect.transform.height,
        input.style.font_size,
        input.style.applied_inherited_scale,
        true
    );

    draw_style.classes = std::move(original_classes);
    return draw_style;
}

template <typename T>
inline ComputedStyle ResolveInputLabelDrawStyle(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    const ComputedStyle& draw_style
) {
    ComputedStyle label_style = draw_style;

    // Scalar labels inherit typography only. Their box is transparent unless
    // input::label (or a class-specific ::label selector) styles it.
    label_style.background_color = BLANK;
    label_style.border_color = BLANK;
    label_style.border_width = 0.0f;
    label_style.border_widths = EdgeValues{};
    label_style.border_width_length.reset();
    label_style.border_width_lengths = EdgeLengthValues{};
    label_style.border_radius = 0.0f;
    label_style.padding = {};
    label_style.margin = {};
    label_style.padding_lengths = {};
    label_style.margin_lengths = {};
    label_style.width_length.reset();
    label_style.height_length.reset();
    label_style.min_width_length.reset();
    label_style.max_width_length.reset();
    label_style.min_height_length.reset();
    label_style.max_height_length.reset();

    const float default_gap =
        4.0f * EffectiveComputedStyleScale(draw_style);
    if(input.label_layout == InputLabelLayout::Back) {
        label_style.margin.right = default_gap;
    }
    else {
        label_style.margin.left = default_gap;
    }

    if(!StyleSheetUsesPart(style_sheet,CSS_INPUT_LABEL_PART)) {
        ResolveComputedLengths(
            label_style,
            input.rect.transform.width,
            input.rect.transform.height,
            input.style.font_size,
            input.style.applied_inherited_scale,
            true
        );
        return label_style;
    }

    std::vector<std::string> original_classes = label_style.classes;
    AddCSSClass(label_style.classes,CSS_INPUT_LABEL_PART);

    std::vector<std::string> active_states =
        input.rect.pseudo_states.ActiveNamesView();
    auto set_state = [&](const char* name, bool enabled) {
        const std::string normalized = NormalizeUIPseudoStateName(name);
        auto found = std::find(active_states.begin(),active_states.end(),normalized);
        if(enabled && found == active_states.end()) active_states.push_back(normalized);
        else if(!enabled && found != active_states.end()) active_states.erase(found);
    };
    set_state("focus",input.focused);
    set_state("hover",input.hovered);
    set_state("active",input.active);
    set_state("disabled",input.disabled || input.rect.inherited_disabled);

    label_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Input,
        input.style.id,
        active_states,
        label_style
    );
    label_style = ResolveAnimatedStyle(
        input.rect,
        style_sheet,
        std::move(label_style),
        CSS_INPUT_LABEL_PART
    );

    ResolveComputedLengths(
        label_style,
        input.rect.transform.width,
        input.rect.transform.height,
        input.style.font_size,
        input.style.applied_inherited_scale,
        true
    );

    label_style.classes = std::move(original_classes);
    return label_style;
}

inline const char* InputComponentLabelPartClass(int index) {
    switch(index) {
        case 0: return CSS_INPUT_COMPONENT_0_LABEL_PART;
        case 1: return CSS_INPUT_COMPONENT_1_LABEL_PART;
        case 2: return CSS_INPUT_COMPONENT_2_LABEL_PART;
        case 3: return CSS_INPUT_COMPONENT_3_LABEL_PART;
        default: return CSS_INPUT_COMPONENT_LABEL_PART;
    }
}

template <typename T>
inline ComputedStyle ResolveInputComponentLabelDrawStyle(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    int component_index,
    bool hovered,
    bool active,
    const ComputedStyle& draw_style
) {
        ComputedStyle label_style = draw_style;

    // Default component-label appearance.
    label_style.background_color = BLANK;
    label_style.border_color = BLANK;
    label_style.border_width = 0.0f;
    label_style.border_widths = EdgeValues{};
    label_style.border_width_length.reset();
    label_style.border_width_lengths = EdgeLengthValues{};
    label_style.border_radius = 0.0f;

    label_style.padding = {};
    label_style.margin = {};

    label_style.padding_lengths = {};
    label_style.margin_lengths = {};

    label_style.width_length.reset();
    label_style.height_length.reset();
    label_style.min_width_length.reset();
    label_style.max_width_length.reset();
    label_style.min_height_length.reset();
    label_style.max_height_length.reset();

    const float default_gap =
        4.0f * EffectiveComputedStyleScale(draw_style);
    if(input.component_label_layout == InputLabelLayout::Front) {
        label_style.margin.left = default_gap;
    }
    else {
        label_style.margin.right = default_gap;
    }

    const char* component_part =
        InputComponentLabelPartClass(component_index);

    if(
        !StyleSheetUsesPart(style_sheet,CSS_INPUT_COMPONENT_LABEL_PART) &&
        !StyleSheetUsesPart(style_sheet,component_part)
    ) {
        ResolveComputedLengths(
            label_style,
            input.rect.transform.width,
            input.rect.transform.height,
            input.style.font_size,
            input.style.applied_inherited_scale,
            true
        );
        return label_style;
    }

    std::vector<std::string> original_classes = label_style.classes;
    AddCSSClass(label_style.classes,CSS_INPUT_COMPONENT_LABEL_PART);
    AddCSSClass(label_style.classes,component_part);

    std::vector<std::string> active_states =
        input.rect.pseudo_states.ActiveNamesView();
    auto set_state = [&](const char* name, bool enabled) {
        const std::string normalized = NormalizeUIPseudoStateName(name);
        auto found = std::find(active_states.begin(),active_states.end(),normalized);
        if(enabled && found == active_states.end()) active_states.push_back(normalized);
        else if(!enabled && found != active_states.end()) active_states.erase(found);
    };
    set_state("focus",input.focused);
    set_state("hover",hovered);
    set_state("active",active);
    set_state("disabled",input.disabled || input.rect.inherited_disabled);

    label_style = ResolveStyleWithPseudoStates(
        style_sheet,
        SelectorType::Input,
        input.style.id,
        active_states,
        label_style
    );
    label_style = ResolveAnimatedStyle(
        input.rect,
        style_sheet,
        std::move(label_style),
        component_part
    );

    ResolveComputedLengths(
        label_style,
        input.rect.transform.width,
        input.rect.transform.height,
        input.style.font_size,
        input.style.applied_inherited_scale,
        true
    );

    label_style.classes = std::move(original_classes);
    return label_style;
}

template <typename T>
inline Vector2 DrawScalarInputLabelValue(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    Rectangle content_rect,
    const std::string& value_text,
    const ComputedStyle& draw_style
) {
    Font value_font = ResolveUIFont(draw_style.font);
    Vector2 value_size = MeasureTextCached(
        value_font,
        value_text.c_str(),
        draw_style.font_size,
        1.0f
    );

    auto draw_value = [&](Vector2 position) {
        const bool editing =
            input.focused &&
            (
                input.input_type == InputType::Instant ||
                (input.input_type == InputType::Drag && input.drag_text_editing)
            );
        DrawUIInputTextSelectionAndCaret(
            input,
            value_font,
            value_text,
            position,
            draw_style.font_size,
            ApplyStyleOpacity(draw_style.text_color,draw_style.opacity),
            editing
        );
        DrawUIText(
            value_font,
            value_text.c_str(),
            position,
            draw_style.font_size,
            1.0f,
            ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
        );
    };

    auto centered_value_position = [&]() {
        return Vector2{
            std::round(GetTextX(content_rect,value_size.x,draw_style.text_align)),
            std::round(
                content_rect.y+
                (content_rect.height-value_size.y)*0.5f
            )
        };
    };

    const bool has_label =
        input.show_label &&
        !input.text.empty() &&
        (
            std::is_same_v<T,float> ||
            std::is_same_v<T,int>
        );

    if(!has_label) {
        Vector2 value_position = centered_value_position();
        draw_value(value_position);
        return value_position;
    }

    ComputedStyle label_style = ResolveInputLabelDrawStyle(
        input,
        style_sheet,
        draw_style
    );

    if(label_style.display == DisplayType::None) {
        Vector2 value_position = centered_value_position();
        draw_value(value_position);
        return value_position;
    }

    Font label_font = ResolveUIFont(label_style.font);
    Vector2 label_text_size = MeasureTextCached(
        label_font,
        input.text.c_str(),
        label_style.font_size,
        1.0f
    );

    const float label_width =
        label_text_size.x+
        label_style.padding.left+
        label_style.padding.right;
    const float label_height = std::min(
        content_rect.height,
        label_text_size.y+
        label_style.padding.top+
        label_style.padding.bottom
    );
    const bool label_before =
        input.label_layout == InputLabelLayout::Back;
    const float label_gap = std::max(
        0.0f,
        label_before
            ? label_style.margin.right
            : label_style.margin.left
    );
    const float group_width =
        label_width+label_gap+value_size.x;
    const float group_x = GetTextX(
        content_rect,
        group_width,
        draw_style.text_align
    );

    const float label_x = label_before
        ? group_x
        : group_x+value_size.x+label_gap;
    Vector2 value_position = {
        std::round(
            label_before
                ? group_x+label_width+label_gap
                : group_x
        ),
        std::round(
            content_rect.y+
            (content_rect.height-value_size.y)*0.5f
        )
    };

    Rectangle label_rect = {
        label_x,
        content_rect.y+(content_rect.height-label_height)*0.5f,
        std::min(label_width,content_rect.width),
        label_height
    };

    if(label_style.background_color.a > 0) {
        DrawRectangleRounded(
            label_rect,
            ResolveUIBorderRoundness(label_rect,label_style.border_radius),
            6,
            ApplyStyleOpacity(label_style.background_color,label_style.opacity)
        );
    }

    DrawStyleBorder(
        label_rect,label_style,
        ApplyStyleOpacity(label_style.border_color,label_style.opacity),6
    );

    Rectangle label_content = {
        label_rect.x+label_style.padding.left,
        label_rect.y+label_style.padding.top,
        std::max(
            0.0f,
            label_rect.width-
            label_style.padding.left-
            label_style.padding.right
        ),
        std::max(
            0.0f,
            label_rect.height-
            label_style.padding.top-
            label_style.padding.bottom
        )
    };

    DrawUIText(
        label_font,
        input.text.c_str(),
        {
            std::round(
                GetTextX(
                    label_content,
                    label_text_size.x,
                    label_style.text_align
                )
            ),
            std::round(
                label_content.y+
                (label_content.height-label_text_size.y)*0.5f
            )
        },
        label_style.font_size,
        1.0f,
        ApplyStyleOpacity(label_style.text_color,label_style.opacity)
    );

    draw_value(value_position);

    return value_position;
}

template <typename T>
inline void DrawInputComponentText(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    int component_index,
    Rectangle component_rect,
    const std::string& value_text,
    const ComputedStyle& draw_style,
    bool component_hovered,
    bool component_active
) {
    Font value_font = ResolveUIFont(draw_style.font);
    Vector2 value_size = MeasureTextCached(
        value_font,
        value_text.c_str(),
        draw_style.font_size,
        1.0f
    );

    auto draw_value = [&](Vector2 position) {
        const bool editing =
            input.focused &&
            input.active_component == component_index &&
            (
                input.input_type == InputType::Instant ||
                (input.input_type == InputType::Drag && input.drag_text_editing)
            );
        DrawUIInputTextSelectionAndCaret(
            input,
            value_font,
            value_text,
            position,
            draw_style.font_size,
            ApplyStyleOpacity(draw_style.text_color,draw_style.opacity),
            editing
        );
        DrawUIText(
            value_font,
            value_text.c_str(),
            position,
            draw_style.font_size,
            1.0f,
            ApplyStyleOpacity(draw_style.text_color,draw_style.opacity)
        );
    };

    const bool has_label =
        input.show_component_labels &&
        component_index >= 0 &&
        component_index < static_cast<int>(input.component_labels.size()) &&
        !input.component_labels[static_cast<std::size_t>(component_index)].empty();

    if(!has_label) {
        draw_value({
            std::round(component_rect.x + (component_rect.width-value_size.x)*0.5f),
            std::round(component_rect.y + (component_rect.height-value_size.y)*0.5f)
        });
        return;
    }

    const std::string& label_text =
        input.component_labels[static_cast<std::size_t>(component_index)];

    ComputedStyle label_style = ResolveInputComponentLabelDrawStyle(
        input,
        style_sheet,
        component_index,
        component_hovered,
        component_active,
        draw_style
    );

    if(label_style.display == DisplayType::None) {
        draw_value({
            std::round(component_rect.x + (component_rect.width-value_size.x)*0.5f),
            std::round(component_rect.y + (component_rect.height-value_size.y)*0.5f)
        });
        return;
    }

    Font label_font = ResolveUIFont(label_style.font);
    Vector2 label_text_size = MeasureTextCached(
        label_font,
        label_text.c_str(),
        label_style.font_size,
        1.0f
    );

    const float label_width =
        label_text_size.x +
        label_style.padding.left +
        label_style.padding.right;
    const float label_height = std::min(
        component_rect.height,
        label_text_size.y +
        label_style.padding.top +
        label_style.padding.bottom
    );
    const bool label_before =
        input.component_label_layout == InputLabelLayout::Back;
    const float label_gap = std::max(
        0.0f,
        label_before
            ? label_style.margin.right
            : label_style.margin.left
    );
    const float group_width = label_width + label_gap + value_size.x;
    const float group_x =
        component_rect.x +
        std::max(0.0f,(component_rect.width-group_width)*0.5f);

    const float label_x = label_before
        ? group_x
        : group_x+value_size.x+label_gap;
    const float value_x = label_before
        ? group_x+label_width+label_gap
        : group_x;

    Rectangle label_rect = {
        label_x,
        component_rect.y + (component_rect.height-label_height)*0.5f,
        std::min(label_width,component_rect.width),
        label_height
    };

    if(label_style.background_color.a > 0) {
        DrawRectangleRounded(
            label_rect,
            ResolveUIBorderRoundness(label_rect,label_style.border_radius),
            6,
            ApplyStyleOpacity(label_style.background_color,label_style.opacity)
        );
    }

    DrawStyleBorder(
        label_rect,label_style,
        ApplyStyleOpacity(label_style.border_color,label_style.opacity),6
    );

    Rectangle label_content = {
        label_rect.x + label_style.padding.left,
        label_rect.y + label_style.padding.top,
        std::max(0.0f,label_rect.width-label_style.padding.left-label_style.padding.right),
        std::max(0.0f,label_rect.height-label_style.padding.top-label_style.padding.bottom)
    };

    DrawUIText(
        label_font,
        label_text.c_str(),
        {
            std::round(GetTextX(label_content,label_text_size.x,label_style.text_align)),
            std::round(label_content.y+(label_content.height-label_text_size.y)*0.5f)
        },
        label_style.font_size,
        1.0f,
        ApplyStyleOpacity(label_style.text_color,label_style.opacity)
    );

    draw_value({
        std::round(value_x),
        std::round(component_rect.y+(component_rect.height-value_size.y)*0.5f)
    });
}

template <typename T>
inline void DrawTriggerInput(
    UIInput<T>& input,
    const StyleSheet& style_sheet
) {
    const ComputedStyle& draw_style =
        ResolveInputDrawStyle(input,style_sheet);
    struct InputPseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~InputPseudoAfterScope() {
            DrawGeneratedPseudoElement(
                rect,style,SelectorType::Input,sheet,UIGeneratedPseudoKind::After
            );
        }
    } input_pseudo_after{input.rect,draw_style,style_sheet};

    Rectangle rect = input.rect.transform;

    DrawRectangleRounded(
        rect,
        ResolveUIBorderRoundness(rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawStyleBorder(
        rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        input.rect,draw_style,SelectorType::Input,style_sheet,UIGeneratedPseudoKind::Before
    );

    Rectangle content = {
        rect.x + draw_style.padding.left,
        rect.y + draw_style.padding.top,
        std::max(
            0.0f,
            rect.width -
                draw_style.padding.left -
                draw_style.padding.right
        ),
        std::max(
            0.0f,
            rect.height -
                draw_style.padding.top -
                draw_style.padding.bottom
        )
    };

    Font font = ResolveUIFont(draw_style.font);
    Vector2 text_size = MeasureTextCached(
        font,
        input.text.c_str(),
        draw_style.font_size,
        1.0f
    );

    Vector2 text_position = {
        GetTextX(
            content,
            text_size.x,
            draw_style.text_align
        ),
        content.y +
            (content.height-text_size.y)*0.5f
    };

    DrawUIText(
        font,
        input.text.c_str(),
        text_position,
        draw_style.font_size,
        1.0f,
        ApplyStyleOpacity(
            draw_style.text_color,
            draw_style.opacity
        )
    );
}

template <typename T>
inline void DrawInputStepButtons(
    UIInput<T>& input,
    const StyleSheet& style_sheet,
    const ComputedStyle& draw_style,
    InputStepButtonLayout layout
) {
    if constexpr(!SupportsInputStepButtons<T>) {
        return;
    }
    else {
        if(!input.show_step_buttons) {
            return;
        }

        Rectangle decrement =
            GetInputDecrementRect(input, layout);

        Rectangle increment =
            GetInputIncrementRect(input, layout);

        bool decrement_active =
            input.decrement_hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        bool increment_active =
            input.increment_hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        ComputedStyle decrement_style =
            ResolveInputPartDrawStyle(
                input,
                style_sheet,
                CSS_INPUT_DECREMENT_BUTTON_PART,
                input.decrement_hovered,
                decrement_active,
                false,
                draw_style
            );

        ComputedStyle increment_style =
            ResolveInputPartDrawStyle(
                input,
                style_sheet,
                CSS_INPUT_INCREMENT_BUTTON_PART,
                input.increment_hovered,
                increment_active,
                false,
                draw_style
            );

        auto draw_step_button = [](
            Rectangle rect,
            const char* caption,
            const ComputedStyle& style
        ) {
            DrawRectangleRounded(
                rect,
                ResolveUIBorderRoundness(rect,style.border_radius),
                6,
                ApplyStyleOpacity(
                    style.background_color,
                    style.opacity
                )
            );

            ComputedStyle border_style = style;
            if(MaximumBorderWidth(border_style) < 1.0f) {
                border_style.border_width = 1.0f;
                border_style.border_widths = EdgeValues(1.0f);
                border_style.border_width_lengths = EdgeLengthValues{};
            }
            DrawStyleBorder(
                rect,border_style,
                ApplyStyleOpacity(style.border_color,style.opacity),6
            );

            Font font = ResolveUIFont(style.font);

            Vector2 text_size = MeasureTextCached(
                font,
                caption,
                style.font_size,
                1.0f
            );

            DrawUIText(
                font,
                caption,
                {
                    rect.x + (rect.width - text_size.x) * 0.5f,
                    rect.y + (rect.height - text_size.y) * 0.5f
                },
                style.font_size,
                1.0f,
                ApplyStyleOpacity(
                    style.text_color,
                    style.opacity
                )
            );
        };

        draw_step_button(
            decrement,
            "-",
            decrement_style
        );

        draw_step_button(
            increment,
            "+",
            increment_style
        );
    }
}
template <typename T>
inline Rectangle GetInputTextAreaRect(
    const UIInput<T>& input,
    const ComputedStyle& draw_style
) {
    Rectangle value_rect = GetInputValueRect(input);

    if(!input.show_value_box) {
        return value_rect;
    }

    float inset = std::max(
        2.0f * EffectiveComputedStyleScale(draw_style),
        draw_style.border_width +
            EffectiveComputedStyleScale(draw_style)
    );
    return Rectangle{
        value_rect.x + inset,
        value_rect.y + inset,
        std::max(0.0f,value_rect.width - inset * 2.0f),
        std::max(0.0f,value_rect.height - inset * 2.0f)
    };
}

void DrawInputTextArea(
    Rectangle text_area,
    const ComputedStyle& draw_style,
    bool focused
);

template <typename T>
inline void DrawInput(
    UIInput<T>& input,
    const StyleSheet& style_sheet
) {
    if(input.input_type == InputType::Trigger) {
        DrawTriggerInput(input,style_sheet);
        return;
    }

    const ComputedStyle& draw_style =
        ResolveInputDrawStyle(input,style_sheet);
    const float visual_scale = EffectiveComputedStyleScale(draw_style);

    Rectangle outer_rect =
        input.rect.transform;

    Rectangle value_rect =
        GetInputValueRect(input);

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(
            draw_style.background_color,
            draw_style.opacity
        )
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        input.rect,draw_style,SelectorType::Input,style_sheet,UIGeneratedPseudoKind::Before
    );
    struct ScalarInputPseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~ScalarInputPseudoAfterScope() {
            DrawGeneratedPseudoElement(rect,style,SelectorType::Input,sheet,UIGeneratedPseudoKind::After);
        }
    } scalar_input_pseudo_after{input.rect,draw_style,style_sheet};

    DrawInputStepButtons(input,style_sheet,draw_style,input.step_button_layout);

    Rectangle content_rect = value_rect;

    if(
        (
            input.input_type == InputType::Instant ||
            input.input_type == InputType::Drag
        ) &&
        input.show_value_box
    ) {
        bool value_hovered = CheckCollisionPointRec(
            GetUIMousePosition(),
            value_rect
        );

        bool value_active =
            value_hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        ComputedStyle value_box_style =
            ResolveInputPartDrawStyle(
                input,
                style_sheet,
                CSS_INPUT_VALUE_BOX_PART,
                value_hovered,
                value_active,
                input.focused,
                draw_style
            );

        content_rect = GetInputTextAreaRect(
            input,
            value_box_style
        );

        DrawInputTextArea(
            content_rect,
            value_box_style,
            input.focused
        );
    }

    Rectangle content = {
        content_rect.x + draw_style.padding.left,
        content_rect.y + draw_style.padding.top,
        std::max(
            0.0f,
            content_rect.width -
                draw_style.padding.left -
                draw_style.padding.right
        ),
        std::max(
            0.0f,
            content_rect.height -
                draw_style.padding.top -
                draw_style.padding.bottom
        )
    };

    if(
        input.input_type == InputType::Instant ||
        input.input_type == InputType::Drag
    ) {
        const bool text_editing =
            input.focused &&
            (
                input.input_type == InputType::Instant ||
                (
                    input.input_type == InputType::Drag &&
                    input.drag_text_editing
                )
            );
        std::string value_text = text_editing
            ? input.edit_buffer
            : FormatInputValue(input.value);

        Font font = ResolveUIFont(draw_style.font);
        Vector2 text_size = MeasureTextCached(
            font,
            value_text.c_str(),
            draw_style.font_size,
            1.0f
        );

        Vector2 text_position = DrawScalarInputLabelValue(
            input,
            style_sheet,
            content,
            value_text,
            draw_style
        );

        Color text_color = ApplyStyleOpacity(
            draw_style.text_color,
            draw_style.opacity
        );

        if(
            input.input_type == InputType::Drag &&
            !input.drag_text_editing
        ) {
            DrawHorizontalDragArrows(
                content,
                ApplyStyleOpacity(
                    draw_style.text_color,
                    draw_style.opacity*0.65f
                ),
                visual_scale
            );
        }

        return;
    }

    if(input.input_type != InputType::Slider) {
        return;
    }

    Rectangle slider_rect =
        GetSliderInteractionRect(input);

    ComputedStyle track_fallback = draw_style;
    track_fallback.background_color =
        DarkenColor(
            draw_style.background_color,
            0.35f
        );
    track_fallback.border_color =
        draw_style.border_color;
    track_fallback.border_width = 0.0f;
    track_fallback.border_widths = EdgeValues{};
    track_fallback.border_width_length.reset();
    track_fallback.border_width_lengths = EdgeLengthValues{};
    track_fallback.width = 6.0f;
    track_fallback.width_length =
        CSSLength{6.0f,LengthUnit::Pixels};
    track_fallback.height = 6.0f;
    track_fallback.height_length =
        CSSLength{6.0f,LengthUnit::Pixels};
    track_fallback.border_radius = 1.0f;

    ComputedStyle track_style =
        ResolveInputPartDrawStyle(
            input,
            style_sheet,
            CSS_INPUT_SLIDER_TRACK_PART,
            input.hovered,
            input.active,
            input.focused,
            track_fallback
        );

    ComputedStyle fill_fallback = track_fallback;
    fill_fallback.background_color =
        draw_style.text_color;

    ComputedStyle fill_style =
        ResolveInputPartDrawStyle(
            input,
            style_sheet,
            CSS_INPUT_SLIDER_FILL_PART,
            input.hovered,
            input.active,
            input.focused,
            fill_fallback
        );

    ComputedStyle handle_fallback = draw_style;
    handle_fallback.background_color =
        draw_style.text_color;
    handle_fallback.border_color =
        draw_style.background_color;
    handle_fallback.border_width = 2.0f;
    handle_fallback.border_widths = EdgeValues(2.0f);
    handle_fallback.border_width_length = CSSLength{2.0f,LengthUnit::Pixels};
    handle_fallback.border_width_lengths.left = handle_fallback.border_width_length;
    handle_fallback.border_width_lengths.right = handle_fallback.border_width_length;
    handle_fallback.border_width_lengths.top = handle_fallback.border_width_length;
    handle_fallback.border_width_lengths.bottom = handle_fallback.border_width_length;
    handle_fallback.width = 20.0f;
    handle_fallback.height = 20.0f;
    handle_fallback.width_length =
        CSSLength{20.0f,LengthUnit::Pixels};
    handle_fallback.height_length =
        CSSLength{20.0f,LengthUnit::Pixels};
    handle_fallback.border_radius = 1.0f;

    ComputedStyle handle_style =
        ResolveInputPartDrawStyle(
            input,
            style_sheet,
            CSS_INPUT_SLIDER_HANDLE_PART,
            input.slider_handle_hovered,
            input.active,
            input.focused,
            handle_fallback
        );

    const float normalized =
        GetInputNormalizedValue(input);

    const bool vertical_slider =
        input.slider_orientation == UISliderOrientation::Vertical;

    Rectangle track{};
    if(vertical_slider) {
        const float track_width =
            std::max(1.0f,track_style.width);
        track = {
            slider_rect.x+
                (slider_rect.width-track_width)*0.5f,
            slider_rect.y,
            track_width,
            slider_rect.height
        };
    }
    else {
        track = {
            slider_rect.x,
            slider_rect.y+
                (
                    slider_rect.height-
                    track_style.height
                )*0.5f,
            slider_rect.width,
            std::max(1.0f,track_style.height)
        };
    }

    DrawRectangleRounded(
        track,
        ResolveUIBorderRoundness(track,track_style.border_radius),
        8,
        ApplyStyleOpacity(
            track_style.background_color,
            track_style.opacity
        )
    );

    DrawStyleBorder(
        track,track_style,
        ApplyStyleOpacity(track_style.border_color,track_style.opacity),8
    );

    Rectangle filled_track{};
    Rectangle handle{};

    if(vertical_slider) {
        const float handle_half_height =
            std::min(handle_style.height * 0.5f, track.height * 0.5f);
        const float handle_center_y = std::clamp(
            track.y + track.height * (1.0f-normalized),
            track.y + handle_half_height,
            track.y + track.height - handle_half_height
        );
        const float fill_width =
            std::max(1.0f,fill_style.width);

        filled_track = {
            track.x+(track.width-fill_width)*0.5f,
            handle_center_y,
            fill_width,
            std::max(0.0f,track.y+track.height-handle_center_y)
        };

        handle = {
            track.x+
                track.width*0.5f-
                handle_style.width*0.5f,
            handle_center_y-
                handle_style.height*0.5f,
            std::max(2.0f,handle_style.width),
            std::max(2.0f,handle_style.height)
        };
    }
    else {
        const float handle_half_width =
            std::min(handle_style.width * 0.5f, track.width * 0.5f);
        const float handle_center_x = std::clamp(
            track.x + track.width * normalized,
            track.x + handle_half_width,
            track.x + track.width - handle_half_width
        );

        filled_track = {
            track.x,
            track.y,
            std::max(0.0f, handle_center_x - track.x),
            std::max(1.0f,fill_style.height)
        };

        filled_track.y =
            track.y+
            (
                track.height-
                filled_track.height
            )*0.5f;

        handle = {
            handle_center_x-
                handle_style.width*0.5f,
            track.y+
                track.height*0.5f-
                handle_style.height*0.5f,
            std::max(2.0f,handle_style.width),
            std::max(2.0f,handle_style.height)
        };
    }

    const bool has_fill = vertical_slider
        ? filled_track.height > 0.0f
        : filled_track.width > 0.0f;

    if(has_fill) {
        DrawRectangleRounded(
            filled_track,
            ResolveUIBorderRoundness(filled_track,fill_style.border_radius),
            8,
            ApplyStyleOpacity(
                fill_style.background_color,
                fill_style.opacity
            )
        );
    }

    DrawRectangleRounded(
        handle,
        ResolveUIBorderRoundness(handle,handle_style.border_radius),
        8,
        ApplyStyleOpacity(
            handle_style.background_color,
            handle_style.opacity
        )
    );

    DrawStyleBorder(
        handle,handle_style,
        ApplyStyleOpacity(handle_style.border_color,handle_style.opacity),8
    );

    if(input.slider_show_value) {
        Rectangle value_display{};

        if(vertical_slider) {
            const float value_y =
                slider_rect.y+slider_rect.height+8.0f;
            value_display = {
                content.x,
                value_y,
                content.width,
                std::max(
                    0.0f,
                    content.y+content.height-value_y
                )
            };
        }
        else {
            value_display = {
                slider_rect.x+
                    slider_rect.width+
                    8.0f,
                content.y,
                std::max(
                    0.0f,
                    content.x+
                        content.width-
                        (
                            slider_rect.x+
                            slider_rect.width+
                            8.0f
                        )
                ),
                content.height
            };
        }

        const std::string value_text =
            FormatInputValue(input.value);

        DrawScalarInputLabelValue(
            input,
            style_sheet,
            value_display,
            value_text,
            draw_style
        );
    }
}

#endif // CSS_UI_INTERNAL_TEMPLATES

void DrawInput(
    UIInput<float>& input,
    const StyleSheet& style_sheet
);

void DrawInput(
    UIInput<int>& input,
    const StyleSheet& style_sheet
);

void DrawInput(
    UIInput<std::string>& input,
    const StyleSheet& style_sheet
);

#ifdef CSS_UI_INTERNAL_TEMPLATES
template <typename T>
inline void DrawVectorInput(
    UIInput<T>& input,
    const StyleSheet& style_sheet
) {
    if(input.input_type == InputType::Trigger) {
        DrawTriggerInput(input,style_sheet);
        return;
    }

    const ComputedStyle& draw_style = ResolveInputDrawStyle(input,style_sheet);

    Rectangle outer_rect = input.rect.transform;

    Rectangle content = {
        outer_rect.x + draw_style.padding.left,
        outer_rect.y + draw_style.padding.top,
        std::max(
            0.0f,
            outer_rect.width -
                draw_style.padding.left -
                draw_style.padding.right
        ),
        std::max(
            0.0f,
            outer_rect.height -
                draw_style.padding.top -
                draw_style.padding.bottom
        )
    };

    DrawRectangleRounded(
        outer_rect,
        ResolveUIBorderRoundness(outer_rect,draw_style.border_radius),
        8,
        ApplyStyleOpacity(draw_style.background_color,draw_style.opacity)
    );

    DrawStyleBorder(
        outer_rect,draw_style,
        ApplyStyleOpacity(draw_style.border_color,draw_style.opacity),8
    );
    DrawGeneratedPseudoElement(
        input.rect,draw_style,SelectorType::Input,style_sheet,UIGeneratedPseudoKind::Before
    );
    struct VectorInputPseudoAfterScope {
        UIElementBase& rect; const ComputedStyle& style; const StyleSheet& sheet;
        ~VectorInputPseudoAfterScope() {
            DrawGeneratedPseudoElement(rect,style,SelectorType::Input,sheet,UIGeneratedPseudoKind::After);
        }
    } vector_input_pseudo_after{input.rect,draw_style,style_sheet};

    constexpr int component_count =
        VectorComponentCount<T>;

    float component_width =
        component_count > 0
            ? content.width /
                static_cast<float>(component_count)
            : 0.0f;

    for(int index = 0; index < component_count; index++) {
        Rectangle component_rect = {
            content.x +
                component_width *
                static_cast<float>(index),
            content.y,
            component_width,
            content.height
        };

        if(index > 0) {
            DrawLineV(
                Vector2{
                    component_rect.x,
                    component_rect.y
                },
                Vector2{
                    component_rect.x,
                    component_rect.y +
                        component_rect.height
                },
                draw_style.border_color
            );
        }

        const float* component =
            GetVectorComponent(input.value, index);

        std::string value_text;

        if(
            input.focused &&
            input.active_component == index &&
            !input.component_buffers[index].empty()
        ) {
            value_text =
                input.component_buffers[index];
        }
        else if(component != nullptr) {
            value_text = TextFormat("%.2f",*component);
        }

        const bool component_hovered =
            input.hovered &&
            CheckCollisionPointRec(GetUIMousePosition(),component_rect);
        const bool component_active =
            input.active_component == index &&
            (input.active || input.dragging || input.focused);

        DrawInputComponentText(
            input,
            style_sheet,
            index,
            component_rect,
            value_text,
            draw_style,
            component_hovered,
            component_active
        );

        if(
            input.input_type == InputType::Drag &&
            !(
                input.drag_text_editing &&
                input.active_component == index
            )
        ) {
            DrawHorizontalDragArrows(
                component_rect,
                ApplyStyleOpacity(
                    draw_style.text_color,
                    draw_style.opacity*0.55f
                ),
                EffectiveComputedStyleScale(draw_style)
            );
        }
    }
}

#endif // CSS_UI_INTERNAL_TEMPLATES

void DrawInput(
    UIInput<Vector2>& input,
    const StyleSheet& style_sheet
);

void DrawInput(
    UIInput<Vector3>& input,
    const StyleSheet& style_sheet
);

void DrawInput(
    UIInput<Vector4>& input,
    const StyleSheet& style_sheet
);

void DrawInput(
    UIInput<bool>& input,
    const StyleSheet& style_sheet
);

void DrawCheckerboard(
    Rectangle rect,
    float cell_size = 8.0f
);

std::string FormatColorHex(Color color);

void DrawInput(
    UIInput<Color>& input,
    const StyleSheet& style_sheet
);

void DrawColorPickerPopup(
    UIInput<Color>& input,
    const StyleSheet& style_sheet
);

//--------------------------------------------------------------------
// Recursive updating
//--------------------------------------------------------------------

std::optional<UIRuntimeTarget> FindRuntimeTarget(
    UIContainer& root,
    const std::string& key
);

Rectangle RuntimeParentBounds(const UIRuntimeTarget& target);

bool RuntimeCanMove(const UIRuntimeTarget& target);

bool RuntimeCanResize(const UIRuntimeTarget& target);

bool UpdateRuntimeLayout(
    UIContainer& root,
    UIRuntimeLayer& layer
);

void DrawRuntimeLayoutOverlay(
    UIContainer& root,
    const UIRuntimeLayer& layer
);

void UpdateElements(
    UIContainer& ui,
    bool reset_cursor = true,
    bool inherited_disabled = false
);

// Unified retained-frame update. Layout runs only when dirty, when pseudo-state
// selection changed, or while a layout tween is active.
void UpdateUIFrame(
    UIContainer& root,
    const StyleSheet& style_sheet,
    float width = -1.0f,
    float height = -1.0f,
    bool force_layout = false,
    bool detect_layout_mutations = false
);

struct UIFocusEntry {
    bool* focused = nullptr;
    UIElementBase* rect = nullptr;
};

void CollectFocusableElements(
    UIContainer& container,
    std::vector<UIFocusEntry>& focus,
    bool inherited_disabled = false
);

void AdvanceKeyboardFocus(UIContainer& root);

void UpdatePointerFocus(UIContainer& root);

Rectangle CompositeHeaderRectangle(
    const UIElementBase& rect,
    const ComputedStyle& style
);

bool UpdateCompositeHeader(
    Rectangle header,
    bool disabled,
    bool& hovered,
    bool& clicked
);

Rectangle TabHeaderRectangle(const UITabBar& tabs);

Rectangle TabItemRectangle(
    const UITabBar& tabs,
    std::size_t index
);
void EvaluateTransformMatrix(
    ComputedStyle& style,
    const Rectangle& bounds,
    float parent_width,
    float parent_height,
    float font_size,
    float root_font_size,
    float viewport_width,
    float viewport_height
);
void UpdateTabBar(UITabBar& tabs, bool inherited_disabled);

void TranslateContainer(UIContainer& container, Vector2 delta);

UIPopup* FindOpenModal(UIContainer& container);

void UpdatePopup(UIPopup& popup, bool disabled);

void UpdateWindow(UIWindow& window, bool disabled);

void UpdateContainerScroll(UIContainer& ui, bool disabled);

void UpdateElements(
    UIContainer& ui,
    bool reset_cursor,
    bool inherited_disabled
);

//--------------------------------------------------------------------
// Recursive drawing
//--------------------------------------------------------------------

Rectangle IntersectRectangles(Rectangle first, Rectangle second);

bool RectanglesOverlap(Rectangle first, Rectangle second);

bool SameScissorRectangle(Rectangle first, Rectangle second);

struct UIDrawContext {
    bool scissor_active = false;
    Rectangle scissor = {};
    UIInput<Color>* active_color_picker = nullptr;
    std::vector<UIComboBox*> open_combos;
    std::vector<UIMenu*> open_menus;
    std::vector<UIPopup*> open_popups;
    std::vector<UIWindow*> open_windows;
    UIElementBase* tooltip_owner = nullptr;
    ComputedStyle tooltip_source_style;
};

void ConsiderTooltip(
    UIDrawContext& context,
    UIElementBase& rect,
    const ComputedStyle& style
);

void DrawCompositeHeader(
    UIElementBase& rect,
    ComputedStyle style,
    SelectorType selector,
    const CSS_Id& id,
    const std::string& text,
    const StyleSheet& style_sheet,
    bool hovered,
    bool active,
    bool disabled,
    bool expanded,
    bool show_arrow = true
);

void DrawTabBar(
    UITabBar& tabs,
    const StyleSheet& style_sheet
);

void DrawContainerScrollbars(
    UIContainer& ui,
    const StyleSheet& style_sheet
);

void SetUIDrawScissor(
    UIDrawContext& context,
    bool enabled,
    Rectangle rectangle = {}
);

void DrawContainerBox(UIContainer& ui, const StyleSheet& style_sheet);

void DrawElementsClippedInternal(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    Rectangle parent_clip,
    bool parent_clip_active,
    UIDrawContext& context,
    bool skip_current_composite=false
);

void DrawQueuedOverlays(
    UIDrawContext& context,
    const StyleSheet& style_sheet,
    Rectangle screen_clip
);

void DrawTooltip(
    UIDrawContext& context,
    const StyleSheet& style_sheet
);

void DrawElementsClipped(
    UIContainer& ui,
    const std::unordered_map<std::string,Font>& fonts,
    const StyleSheet& style_sheet,
    Rectangle clip
);

// Nested draw overload that preserves the active font registry.
void DrawElementsClipped(
    UIContainer& ui,
    const StyleSheet& style_sheet,
    Rectangle clip
);

// Retained for compatibility. DrawElements no longer performs this extra traversal.
void DrawInputPopups(
    UIContainer& ui,
    const StyleSheet& style_sheet
);

void DrawElements(
    UIContainer& ui,
    const std::unordered_map<std::string,Font>& fonts,
    const StyleSheet& style_sheet
);

// ---------------------------------------------------------------------------
// UIComponent generic lifecycle (RUI 0.9)
// ---------------------------------------------------------------------------

inline UIComponent::UIComponent() = default;

inline void UIComponent::AssociateDescriptor(
    const UIComponentDescriptor* descriptor,
    const UIComponentRegistry* registry,
    std::string component_name,
    SelectorType selector,
    std::uint32_t capability_bits,
    bool accepts_visual_children
) {
    descriptor_ = descriptor;
    registry_ = registry;
    component_name_ = std::move(component_name);
    selector_ = selector;
    capability_bits_ = capability_bits;
    // UIComponentCapability bit positions are intentionally stable public API.
    focusable_ = (capability_bits_ & (1u << 0)) != 0;
    clickable_ = (capability_bits_ & (1u << 1)) != 0;
    draggable_ = (capability_bits_ & (1u << 2)) != 0;
    keyboard_input_ = (capability_bits_ & (1u << 5)) != 0;
    accepts_visual_children_ = accepts_visual_children;
    if(accepts_visual_children_ && !children_) {
        children_ = std::make_shared<UIContainer>();
        children_->direction = LayoutDirection::Column;
    }
}

inline UIContainer* UIComponent::ChildContainer() {
    return accepts_visual_children_ ? children_.get() : nullptr;
}

inline const UIContainer* UIComponent::ChildContainer() const {
    return accepts_visual_children_ ? children_.get() : nullptr;
}

inline Vector2 UIComponent::MeasureContent(float,float) const {
    return {0.0f,0.0f};
}

inline void UIComponent::UpdateContent(bool) {}
inline void UIComponent::DrawContent(const ComputedStyle&,Rectangle) {}

inline Rectangle UIComponent::ContentBounds(const ComputedStyle& resolved_style) const {
    Rectangle content = rect.transform;
    content.x += resolved_style.padding.left;
    content.y += resolved_style.padding.top;
    content.width = std::max(
        0.0f,
        content.width-resolved_style.padding.left-resolved_style.padding.right
    );
    content.height = std::max(
        0.0f,
        content.height-resolved_style.padding.top-resolved_style.padding.bottom
    );
    return content;
}

inline Vector2 UIComponent::Measure(
    const StyleSheet&,
    float available_width,
    float available_height
) {
    const float content_width = std::max(
        0.0f,available_width-style.padding.left-style.padding.right
    );
    const float content_height = std::max(
        0.0f,available_height-style.padding.top-style.padding.bottom
    );
    const Vector2 intrinsic = MeasureContent(content_width,content_height);
    return {
        std::max(0.0f,intrinsic.x)+style.padding.left+style.padding.right,
        std::max(0.0f,intrinsic.y)+style.padding.top+style.padding.bottom
    };
}

inline void UIComponent::Layout(const StyleSheet& style_sheet,Rectangle bounds) {
    rect.transform = bounds;
    if(!children_) return;
    const Rectangle content = ContentBounds(style);
    children_->rect.transform = content;
    children_->rect.transform.width = content.width;
    children_->rect.transform.height = content.height;
    children_->inherited_scale = EffectiveComputedStyleScale(style);
    LayoutContainer(
        *children_,style_sheet,content.width,content.height,true
    );
}

inline void UIComponent::Update(bool inherited_disabled) {
    const bool effective_disabled = disabled || inherited_disabled;
    rect.inherited_disabled = inherited_disabled;
    const Vector2 mouse = GetUIMousePosition();
    hovered_ = !effective_disabled && CheckCollisionPointRec(mouse,rect.transform);

    if(effective_disabled) {
        active_ = false;
        dragging_ = false;
    }
    else {
        if((clickable_ || draggable_) && hovered_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            active_ = true;
            drag_origin_ = mouse;
            last_drag_mouse_ = mouse;
        }

        if(draggable_ && active_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Vector2 total = {mouse.x-drag_origin_.x,mouse.y-drag_origin_.y};
            if(!dragging_ && (std::abs(total.x) >= 3.0f || std::abs(total.y) >= 3.0f)) {
                dragging_ = true;
                OnDragStart(drag_origin_);
                EmitGenericEvent("dragstart");
            }
            if(dragging_) {
                const Vector2 delta = {mouse.x-last_drag_mouse_.x,mouse.y-last_drag_mouse_.y};
                OnDrag(delta,total);
                EmitGenericEvent("drag");
                last_drag_mouse_ = mouse;
            }
        }

        if(active_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if(dragging_) {
                OnDragEnd(mouse);
                EmitGenericEvent("dragend");
            }
            else if(clickable_ && hovered_) {
                OnClick();
                EmitGenericEvent("click");
            }
            active_ = false;
            dragging_ = false;
        }
    }

    rect.pseudo_states.SetKnown("hover",hovered_);
    rect.pseudo_states.SetKnown("active",active_);
    rect.pseudo_states.SetKnown("focus",focused_);
    rect.pseudo_states.SetKnown("disabled",effective_disabled);
    rect.pseudo_states.Update();

    UpdateContent(effective_disabled);
    if(children_) UpdateElements(*children_,false,effective_disabled);
}

inline void UIComponent::Draw(const StyleSheet& style_sheet) {
    if(style.display == DisplayType::None) return;
    const bool effective_disabled = disabled || rect.inherited_disabled;
    const ComputedStyle& draw_style = ResolveInteractiveStyleRef(
        style_sheet,selector_,style,focused_,hovered_,active_,
        effective_disabled,rect
    );

    const Color background = ApplyStyleOpacity(
        draw_style.background_color,draw_style.opacity
    );
    if(background.a > 0) {
        DrawRectangleRounded(
            rect.transform,
            ResolveUIBorderRoundness(rect.transform,draw_style.border_radius),
            8,
            background
        );
    }
    const Color border = ApplyStyleOpacity(
        draw_style.border_color,draw_style.opacity
    );
    DrawStyleBorder(rect.transform,draw_style,border,8);
    DrawGeneratedPseudoElement(
        rect,draw_style,selector_,style_sheet,UIGeneratedPseudoKind::Before
    );

    const Rectangle content = ContentBounds(draw_style);
    BeginRegisteredStyleOverrides(draw_style);
    DrawContent(draw_style,content);
    EndRegisteredStyleOverrides();

    if(children_) {
        Rectangle clip = rect.transform;
        if(draw_style.overflow == OverflowType::Visible) {
            clip = {
                0.0f,0.0f,
                static_cast<float>(GetScreenWidth()),
                static_cast<float>(GetScreenHeight())
            };
        }
        DrawElementsClipped(*children_,style_sheet,clip);
    }

    DrawGeneratedPseudoElement(
        rect,draw_style,selector_,style_sheet,UIGeneratedPseudoKind::After
    );
}