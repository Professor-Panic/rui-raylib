# RUI Developer Guide

**Source reviewed:** RUI 0.9.1 (`VersionString = "0.9.1"`)  
**Purpose:** Setup, RXML markup, RCSS styling, C++ value/function binding, and built-in component reference.

> This guide describes the code in the supplied archive. The archive contains headers and implementation files, but no `CMakeLists.txt`, project template, README, tests, or runnable sample application. The setup below shows one way to integrate the source into a raylib C++ project.

## 1. What RUI is

RUI is a retained-mode UI library built on raylib. It provides:

- A C++ UI tree and drawing/update loop.
- RXML (an XML-like declarative markup language) for describing UI hierarchy.
- RCSS (a CSS-like stylesheet language) for layout, colors, typography, interaction states, transitions, animation, and effects.
- A runtime registry for binding C++ values/objects/lists and callable functions into markup.
- Built-in form controls, containers, windows, menus, docking, tables, viewports, and other widgets.
- A component registry for metadata, component/property/event descriptions, custom component registration, and generated component references/schemas.

The main high-level entry point is `rui::Context` in `include/rui.h`. It owns a UI tree, the RXML runtime, and parsed stylesheet state.

## 2. Project setup

### 2.1 Files and dependencies

The supplied archive has this layout:

```text
include/
  rui.h
  ui_markup.h
  css_emulator.h
  css_parser.h
  component_registry.h
  tweeny-4.0.0.h
src/rui/
  rui.cpp
  rui_advanced.cpp
  ui_markup.cpp
  css_parser.cpp
  css_emulator_*.cpp
  ui_*.cpp
```

RUI includes raylib headers (`raylib.h`, `raymath.h`, and `rlgl.h`). Use **C++17 or newer** and make sure raylib is installed and linkable. The archive does not state the correct platform linker flags for your environment; follow the raylib build instructions for Windows, Linux, or macOS.

Add `include/` as an include directory, compile the `.cpp` files under `src/rui/`, and link raylib. Do not compile only `rui.cpp`: the implementation is split across multiple source files.

### 2.2 Minimal C++ application

```cpp
#include "rui.h"
#include "raylib.h"

int main() {
    InitWindow(960, 640, "RUI example");
    SetTargetFPS(60);

    rui::Context app;

    // Load RCSS and RXML from files relative to the process working directory.
    if (!app.LoadStyleFile("ui.rcss")) {
        TraceLog(LOG_WARNING, "Could not load ui.rcss");
    }
    if (!app.LoadMarkupFile("ui.rxml")) {
        TraceLog(LOG_WARNING, "Could not load ui.rxml");
    }

    while (!WindowShouldClose()) {
        app.Update({
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())
        });

        BeginDrawing();
        ClearBackground(RAYWHITE);
        app.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

`Context::Update()` handles the UI update/layout/input and runtime synchronization. `Context::Draw()` renders the UI. Call both every frame. Pass viewport width and height through `FrameOptions` as shown when you want an explicit available size.

Other useful `rui::Context` methods:

| Method | Purpose |
|---|---|
| `LoadMarkupFile(path)` | Load an RXML/RML file, resolving markup `<Import src="..." />` directives. |
| `LoadMarkupString(source, source_name)` | Load markup from a string. `source_name` is useful for resolving imports. |
| `LoadStyleFile(path)` | Load an RCSS file; file loading expands supported CSS imports. |
| `LoadStyleString(source)` | Parse an RCSS string. |
| `AppendStyleFile(path)` / `PrependStyleFile(path)` | Add stylesheet files; appended content has higher cascade priority than existing file source. |
| `RequestRebuild()` / `Rebuild()` | Request or perform a markup tree rebuild. |
| `Root()` | Access the root `UIContainer`. |
| `Undo()` / `Redo()` / `UndoStack()` | Access runtime undo/redo support. |
| `HasKeyboardFocus()`, `HasPointerCapture()`, `WantsKeyboard()`, `WantsMouse()` | Query UI input/focus state after `Update()`. |

### 2.3 Suggested project organization

```text
MyApp/
  CMakeLists.txt          # your build configuration
  src/main.cpp
  ui/main.rxml
  ui/theme.rcss
  include/                # RUI headers or a path to the RUI include directory
  third_party/RUI/        # optional location for the supplied source
```

If markup and styles refer to relative files, confirm the application's **working directory**. A relative path is resolved from the process working directory unless a loader explicitly resolves imports relative to its source file.

## 3. RXML fundamentals

RXML is an XML-like declarative language; it is not HTML and it does not execute arbitrary JavaScript. Tags and attributes are parsed by RUI's own runtime. Names are normalized for matching, so built-in tags are generally used in lowercase (`<button>`) or with their registered names (`<Button>`).

### 3.1 Small complete interface

Save as `ui/main.rxml`:

```xml
<UI direction="column" class="app">
  <Container class="panel" direction="column">
    <Label class="heading">RUI counter</Label>
    <Label class="counter-value" text="Count: {count}" />
    <Row class="actions">
      <Button onClick="{increment}">Add one</Button>
      <Button onClick="{reset}">Reset</Button>
    </Row>
    <Input valueType="int" value="{count}" min="0" max="100"
           onChange="{countChanged}" />
  </Container>
</UI>
```

The `count`, `increment`, `reset`, and `countChanged` names must be registered from C++ (example in the next section). `text="Count: {count}"` interpolates the current value into a string. `value="{count}"` on an input keeps a live binding to the value and lets the control write changes back when the binding is writable.

### 3.2 Root tags, children, and common attributes

- `<UI>` is the top-level/root styling and layout wrapper. It supports `id`, `class`, and `direction="row|column"`.
- `<Container>`, `<Row>`, and `<Column>` create container elements. Container content may include other components.
- A component's child text can supply its text, e.g. `<Button>Save</Button>` or `<Label>Hello</Label>`. A `text="..."` attribute can be used too.
- Common attributes include `id`, `class`, `key`, `tooltip`, `states`, and `disabled`, plus component-specific attributes. `id` is useful for CSS `#id` selectors; `class` accepts one or more space-separated CSS class names.
- Use a stable `key` for repeated/dynamic content where appropriate. A markup rebuild may replace UI elements, so do not keep raw element pointers across rebuilds unless you control their lifetime.

### 3.3 Expressions and interpolation

Wrap a whole expression in braces to evaluate it as a typed value or binding:

```xml
<Label text="{username}" />
<Input value="{settings.volume}" min="0" max="1" />
<Button onClick="{increment}">Increment</Button>
```

Include an expression inside ordinary text/attributes to interpolate it into a string:

```xml
<Label text="Hello, {username}!" />
```

RUI's expression parser supports values such as strings, numbers, booleans and `null`/`none`; property access (`settings.volume`); array indexing; function calls; arithmetic (`+ - * / %`); comparisons; logical `&&`/`||`; unary `!`/minus; and a conditional expression (`condition ? left : right`). The source also includes conversion/math helpers such as `str()`, `int()`, `float()`/`number()`, `bool()`, `len()`, `min()`, `max()`, `clamp()`, `lerp()`, `remap()`, `smoothstep()`, `pingpong()`, `vec2()`, `vec3()`, `vec4()`, `rgb()`/`rgba()`, and `cpp()` for invoking a registered native function. Keep expressions small and test them with your version of RUI.

### 3.4 Conditional and repeated markup

Use an `if` attribute to include an element only when the expression is truthy:

```xml
<Label if="{count > 0}" text="The count is positive." />
```

Or use an explicit conditional block:

```xml
<If test="{isLoggedIn}">
  <Label>Welcome back.</Label>
  <Else>
    <Label>Please sign in.</Label>
  </Else>
</If>
```

Repeat children with `For` and an array expression. The `as` name defaults to `item`; `index` defaults to `index`:

```xml
<For each="{people}" as="person" index="i">
  <Label key="{person.id}" text="{i + 1}. {person.name}" />
</For>
```

A `for` attribute is also supported in the form `for="{person in people}"`.

### 3.5 RXML functions and reusable components

A markup function is declared once and can be called from an event handler or another markup function:

```xml
<Function name="clearCount">
  <Set target="count" value="0" />
</Function>

<Function name="add">
  <Set target="count" value="{count + 1}" />
</Function>

<Button onClick="add">Add</Button>
<Button onClick="clearCount">Clear</Button>
```

Actions supported inside `<Function>` include `Set`, `Toggle`, `Call`/`Emit`, `If`/`Else`, pseudo-state commands (`SetState`, `ToggleState`, `RemoveState`), animation commands (`PlayAnimation`, `PauseAnimation`, `ResumeAnimation`, `StopAnimation`, `RestartAnimation`, `ReverseAnimation`), `Undo`, `Redo`, and `CancelDrag`. `Set` supports `target` (or `path`) and `value` attributes. `Call`/`Emit` uses `name` and optional `args`, or child `<Arg value="..." />` entries.

A reusable RXML component is declared with `<Component name="...">`. The custom tag's attributes become component-scope values and are also available on the `props` object. `<Slot>` renders the children supplied at the call site:

```xml
<Component name="Panel">
  <Container class="panel">
    <Label class="panel-title" text="{title}" />
    <Slot />
  </Container>
</Component>

<Panel title="Settings">
  <Label>Choose a preference.</Label>
</Panel>
```

For file organization, `<Import src="..." />` can import another markup file. Imports are resolved relative to the importing markup file; circular imports are diagnosed. `<Style>` blocks embedded in markup are collected as stylesheet source. External RCSS files can also be loaded with `Context::LoadStyleFile()`.

## 4. Bind C++ values and functions

Bindings are exposed through `app.runtime` (a `UIXRuntime`) or its `Registry()`. RUI evaluates expressions by looking up registered names. A `UIXBindingHandle` can be stored if you need to unbind a particular registration later; an ordinary handle is not an auto-unbinding RAII object. `BindScoped()` returns a scope-bound registration that unregisters when it is destroyed or reset.

### 4.1 Bind a primitive value

```cpp
rui::Context app;
int count = 0;
std::string username = "Developer";

app.runtime.Bind("count", count);
app.runtime.Bind("username", username);
```

In RXML:

```xml
<Label text="Hello {username}" />
<Label text="Count: {count}" />
<Input valueType="int" value="{count}" min="0" max="100" />
```

`Bind(name, value)` creates a reference binding for a C++ lvalue. Keep the referenced variable alive while the binding may be used. RUI can read and write the referenced value, as long as conversion to its C++ type is supported. `Constant(name, value)` (or `Value(name, UIXValue)`) registers a value that is not a writable reference to a C++ variable.

### 4.2 Bind native functions

```cpp
app.runtime.Function(
    "increment",
    [&count](const UIXArguments&, UIXEvent&) -> UIXValue {
        ++count;
        return UIXValue(count);
    }
);

app.runtime.Function(
    "reset",
    [&count](const UIXArguments&, UIXEvent&) -> UIXValue {
        count = 0;
        return UIXValue{};
    }
);

app.runtime.Function(
    "countChanged",
    [&count](const UIXArguments&, UIXEvent& event) -> UIXValue {
        // A value-change event carries the current control value in event.value.
        // The bound input also writes through its value binding.
        (void)count;
        (void)event;
        return UIXValue{};
    }
);
```

Attach a native C++ function using braces:

```xml
<Button onClick="{increment}">Add one</Button>
```

For native C++ handlers, use `onClick="{handlerName}"` or `onChange="{handlerName}"`. A **bare** handler such as `onClick="increment"` is reserved for an RXML `<Function name="increment">` declared in markup. A whole brace expression that names a function calls it; a brace expression containing statements can execute those statements directly, e.g. `onClick="{count = count + 1}"`.

For normal widget handlers, RUI invokes `UIXNativeFunction`, whose signature is `UIXValue(const UIXArguments&, UIXEvent&)`. The `UIXEvent` includes `type`, `id`, `value`, `previous_value`, `index`, `bounds`, and `native_widget`. Markup expressions can access the event through `$event`, for example `onClick="{cpp('logClick', $event.id)}"` if you register a compatible native function and prefer to pass explicit values.

### 4.3 Bind an object

Expose selected fields using `BindObject`. This keeps the public markup surface intentional rather than exposing every C++ member automatically.

```cpp
struct Settings {
    std::string theme = "dark";
    int volume = 70;
};

Settings settings;
auto settingsBinding = app.runtime.BindObject("settings", settings,
    [](auto& object) {
        object.Field("theme", &Settings::theme)
              .Field("volume", &Settings::volume);
    });
```

Markup can access the fields with dot notation:

```xml
<Label text="Theme: {settings.theme}" />
<Input valueType="int" value="{settings.volume}" min="0" max="100" />
```

The builder also supports property getter/setter definitions (`Property`), nested objects (`Object`), constant fields (`Value`), and tracked fields (`TrackedField`).

### 4.4 Bind arrays/lists

Use `BindList` to expose a C++ `std::vector` as an RXML array. For structured items, specify the fields the markup can read:

```cpp
struct Person { int id; std::string name; };
std::vector<Person> people = {{1, "Ari"}, {2, "Mina"}};

auto peopleBinding = app.runtime.BindList("people", people,
    [](auto& list) {
        list.Field("id", &Person::id)
            .Field("name", &Person::name);
    });
```

```xml
<For each="{people}" as="person" index="i">
  <Label key="{person.id}" text="{person.name}" />
</For>
```

`BindList` reads the referenced vector on demand. Keep it and the item storage alive while markup can evaluate it; avoid moving/reallocating model objects if external bindings directly reference their fields and those references may remain in use.

### 4.5 Binding lifetime and unbinding

- `Bind(name, value)` / `Function(name, callback)` return a `UIXBindingHandle`. Save it when you need to call `Unbind(handle)` or `UnbindFunction(handle)`.
- `BindScoped(name, value)` returns `UIXScopedBinding`; it unregisters on destruction/reset.
- `BindGroup()` groups registrations so they can be removed together.
- `BindTracked(name, value, lifetime)` and `BindObjectTracked` allow a `UIXBindingLifetime` to invalidate registrations associated with an owner. The source warns when a tracked owner expires and a binding has not been explicitly unbound.
- Do not register a reference to a temporary or a local variable that goes out of scope while the UI is active.

## 5. RCSS styling

RCSS resembles CSS but it is implemented by RUI's parser and renderer. It is not a browser stylesheet; only RUI-supported selectors and properties apply. Load it with `LoadStyleFile()` / `LoadStyleString()`, or embed rules in an RXML `<Style>` block.

### 5.1 Example theme

Save as `ui.rcss`:

```css
:root {
  --surface: #20242b;
  --panel: #2b313b;
  --accent: #6da9ff;
  --text: #edf2f8;
  --muted: #aab5c4;
}

.app {
  width: 100%;
  height: 100%;
  padding: 24px;
  background-color: var(--surface);
  color: var(--text);
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.panel {
  padding: 18px;
  background: var(--panel);
  border: 1px solid #424b59;
  border-radius: 12px;
  gap: 10px;
}

.heading {
  font-size: 22px;
  color: var(--text);
}

.counter-value {
  font-size: 18px;
  color: var(--accent);
}

button {
  padding: 8px 14px;
  background-color: #394454;
  color: var(--text);
  border-radius: 6px;
  transition: background-color 120ms ease;
}

button:hover {
  background-color: var(--accent);
}

button:disabled {
  opacity: 0.45;
}
```

### 5.2 Selectors and states

Common selector forms:

```css
button { ... }          /* component/type selector */
.panel { ... }          /* class selector */
#saveButton { ... }     /* id selector */
.panel button { ... }   /* descendant selector */
button:hover { ... }    /* interaction state */
```

The built-in states include `:hover`, `:active`, `:focus`, and `:disabled`. RUI also supports open-ended named pseudo-states managed from C++ or RXML actions. Use the state name supported by your widget/runtime (for example `:open`) and test it against the component in question.

### 5.3 Common RCSS property families

The parser supports a broad subset of layout and visual declarations, including:

| Area | Representative properties |
|---|---|
| Sizing | `width`, `height`, `min-width`, `max-width`, `min-height`, `max-height` |
| Box model | `padding`, `margin`, individual side values, `border`, `border-width`, `border-color`, `border-radius`, `border-style` |
| Color and text | `background`, `background-color`, `color`, `font-size`, `font-name`, `text-align`, `content` |
| Flex layout | `display`, `flex-direction`, `flex`, `flex-grow`, `flex-shrink`, `flex-basis`, `flex-wrap`, `gap`, `row-gap`, `column-gap`, `align-items`, `align-self`, `align-content`, `justify-content`, `justify-self`, `place-items` |
| Grid layout | `grid-template-columns`, `grid-template-rows`, `grid-auto-columns`, `grid-auto-rows`, `grid-auto-flow`, `grid-column`, `grid-row`, row/column span values |
| Placement/overflow | `position`, `inset`, `left`, `right`, `top`, `bottom`, `overflow`, `opacity`, `scale`, `transform`, `transform-origin`, `perspective` |
| Widget options | `resizable`, `closable`, `collapsible`, `header-height`, `item-height`, `popup-width`, `popup-max-height`, `tooltip-delay`, scroll and splitter settings |
| Image | `object-fit`, `image-rendering`, `image-tint`, `image-flip-x/y`, object position, frame/slice/tile options, blend mode, rotation, pixel snap |
| Effects | `filter`, `backdrop-filter`, `effect`, `mix-blend-mode` and descriptor-defined custom component style properties |

Values/units are interpreted by RUI. Length parsing includes pixel/numeric values and percentage/flexible grid tracks; `font-size` has special handling for `em`. Check the parser/runtime before relying on web-only features such as arbitrary DOM selectors or unsupported CSS units.

### 5.4 Variables, imports, transitions, and animations

CSS custom properties can be declared under `:root` and referenced with `var(--name)`:

```css
:root { --accent: #6da9ff; }
button { background-color: var(--accent); }
```

The RCSS parser includes `@keyframes`, `@media` handling, and transition declarations. A simple transition example is:

```css
button {
  background-color: #303846;
  transition: background-color 160ms ease;
}
button:hover { background-color: #6da9ff; }
```

RUI's animation system also exposes imperative controls by key through `Context`/`UIXRuntime` (play, pause, resume, stop, restart, reverse, query playing state). `Context::LoadStyleFile()` expands supported CSS file imports; confirm the exact `@media` conditions and effect syntax you need in this parser rather than assuming full browser-CSS parity.

## 6. Built-in component reference

The default registry in `component_registry.h` registers the following components. Tag matching is case-insensitive/normalized. Several aliases are listed; component-specific markup support can be richer than the metadata's small common property list.

| Component | Common tag names / aliases | Purpose and useful notes |
|---|---|---|
| `Container` | `container`, `row`, `column` | Parent layout container; children may be UI elements. `direction="row"` or `direction="column"`; container also supports scrollbars/disabled settings. |
| `Button` | `button` | Clickable button; text comes from inner text or `text`; `disabled`; `onClick`. |
| `Label` | `label`, `text` | Static/read-only text label; text may contain expressions. |
| `FloatInput` | `input` (default/fallback), `floatinput` | Floating-point input. `value`, `min`, `max`, `step`, `disabled`, `onChange`, `onSubmit`; also supports different input presentation modes. |
| `IntInput` | `intinput` or `input valueType="int"` | Integer input. `value`, `min`, `max`, `step`, `disabled`, change/submit events. |
| `BoolInput` | `boolinput` or `input valueType="bool"` | Boolean input. `value`, `disabled`, `onChange`. |
| `StringInput` | `stringinput`, `textinput`, or `input valueType="string"` | Editable string input; `value`, `placeholder`, `disabled`, change/submit events. |
| `Vector2Input` | `vector2input` or `input valueType="vector2"` | Two-component vector input. |
| `Vector3Input` | `vector3input` or `input valueType="vector3"` | Three-component vector input. |
| `Vector4Input` | `vector4input` or `input valueType="vector4"` | Four-component vector input. |
| `ColorInput` | `colorinput` or `input valueType="color"` | RGBA/color input. |
| `Selectable` | `selectable` | Selectable/toggle-like control; supports `selected`, `toggleOnClick`, and `onClick`. |
| `ComboBox` | `combobox`, `combo`, `select` | Dropdown. Use `items="{array}"` or child `<Option>` elements; `selectedIndex`, `placeholder`, `onChange`. |
| `RangeInput` | `rangeinput`, `floatrange`, `intrange` | Two-handle lower/upper range control; `lower`, `upper`, `min`, `max`, `valueType`, `onChange`. |
| `Canvas` | `canvas` | Custom/immediate drawing surface for app-provided drawing behavior. |
| `Image` | `image`, `uiimage` | Image/texture element; supports fit/sampling/slice/blend and CSS image properties. |
| `Splitter` | `splitter` | Resizable panes; supports orientation and pane sizing options. |
| `Collapsible` | `collapsible` | Container whose child content can be collapsed. |
| `TreeNode` | `treenode`, `tree` | Hierarchical/tree node container. |
| `TabBar` | `tabbar`, `tabs` | Tabbed content; configuration-oriented child structure. |
| `Menu` | `menu` | Menu control/container. |
| `Popup` | `popup`, `modal` | Popup/modal surface. `modal` is an alias that sets modal behavior by default. |
| `Window` | `window` | Floating window; supports runtime movement/resizing configuration. |
| `TextBox` | `textbox`, `richtext`, `wrappedtext` | Wrapped/editable text box; `editable` defaults to true for `textbox`. |
| `Table` | `table`, `datagrid` | Virtualized table/data-grid; uses configuration child elements such as columns/rows. |
| `VirtualList` | `virtuallist`, `virtual-list` | Virtualized list surface. |
| `DockSpace` | `dockspace`, `dock-space`, `docking` | Dockable workspace with layout/persistence support. |
| `Viewport` | `viewport` | Transformed/zoomable/pannable viewport; supports `pan`, `zoom`, `panEnabled`, `wheelZoom`, min/max zoom, and view/update callbacks. |
| `ViewportItem` | `viewportitem` | World-space child displayed in a viewport. |

### 6.1 Input modes and options

`<Input>` chooses its type from `valueType` (or, when omitted, often infers it from the bound value). Explicit values recognized include `float`, `int`, `bool`, `string`, `vector2`, `vector3`, `vector4`, and `color`. Use the specialized tag when you prefer an explicit type.

For numeric/vector/color inputs, RUI's markup code also recognizes options such as `type="slider|drag|trigger|checkbox|color"`, `label`, `placeholder`, `showLabel`, `labelLayout`, `labels`/`componentLabels`, `showComponentLabels`, `min`, `max`, `step`, `stepButtons`, `valueBox`, `dragSpeed`, `sliderShowValue`, `sliderValueWidth`, `sliderClickToSet`, `sliderOrientation`, and `stepButtonLayout`. Exact relevance depends on input type.

### 6.2 Important component-specific child structures

Some widgets consume specific child tags rather than treating every child as a normal nested UI element:

- Combo boxes accept `<Option>...</Option>` for options.
- Tables use child configuration elements such as `<Column>` and `<Row>`.
- Tab bars, tables, virtual lists, and dock spaces have configuration-oriented structures.
- `Viewport`, `ViewportItem`, `Window`, `Popup`, `Menu`, tree/collapsible components, and containers can own nested content where supported.

Refer to the component-specific branch in `src/rui/ui_markup.cpp` for an attribute's exact default and behavior if it is not covered above.

## 7. Events and state

### 7.1 Event handlers

Common handler attributes include:

- `Button`: `onClick`
- inputs: `onChange`, often `onSubmit`
- selectable: `onClick`
- combo/range input: `onChange`
- advanced components may expose specialized handlers, e.g. drag/drop, viewport update/view changed, docking layout changes, or custom drawing callbacks.

The event object contains `type`, `id`, `value`, `previousValue` (C++ field `previous_value`), `index`, and `bounds`. For example, RXML may use `$event.value` or `$event.id`. For native C++, read the `UIXEvent&` parameter.

### 7.2 Pseudo-states and animations

Use `Context::runtime.Control(key)` or runtime state/animation helpers for a keyed element. The source exposes methods including `SetPseudoState`, `TogglePseudoState`, `RemovePseudoState`, `HasPseudoState`, `PlayAnimation`, `PauseAnimation`, `ResumeAnimation`, `StopAnimation`, `RestartAnimation`, `ReverseAnimation`, and `IsAnimationPlaying`. RXML function actions have corresponding state/animation commands.

RXML common attributes include `states`; individual custom state names can be toggled by RXML actions or C++ pseudo-state APIs and then targeted in RCSS. A component's generated key/id must correspond to the target used for runtime operations.

## 8. Custom components and metadata

RUI 0.9 includes a component registry and descriptor/builder helpers in `component_registry.h` and `rui.h`. You can register application-specific components with the runtime's component registry before its first rebuild, then describe properties, events, aliases, children policy, and documentation. Useful aliases/helpers exposed by `rui.h` include:

- `rui::DefaultComponentRegistry()`
- `rui::DescribeComponent<Custom>("TagName")`
- `rui::Component<Custom>("TagName")`
- `rui::Property<Widget>(...)`
- `rui::GenerateSchema(registry)`
- `rui::GenerateProjectSchema(registry)`
- `rui::GenerateComponentReference(registry)`

The default component registry stays mutable until the first rebuild (per the source comment); plan registrations before loading/rebuilding markup. Schema/reference generation is a practical way to inspect registered metadata at runtime rather than maintaining a second manual list.

## 9. Diagnostics and troubleshooting

- **Markup doesn't load:** check the boolean result from `LoadMarkupFile()` / `LoadMarkupString()` and inspect `app.runtime.Diagnostics()`. Typical issues include mismatched closing tags, missing imports, unknown components, and expression errors.
- **Styles don't apply:** verify that `LoadStyleFile()` succeeded; confirm the selector matches the element's tag/class/id; confirm the property is supported by RUI; check cascade order if using append/prepend style files.
- **A bound label is blank or stale:** ensure the name was registered with exactly the expected path; for object data, register fields with `BindObject`; make sure the C++ object remains alive.
- **An input doesn't write back:** use a whole expression such as `value="{count}"`, not interpolated text such as `value="Count {count}"`; the binding must be writable and compatible with the C++ type.
- **A native event handler is reported unknown:** write it in braces, e.g. `onClick="{save}"`. Bare names refer to RXML `<Function>` functions.
- **Markup changes aren't visible:** call `RequestRebuild()` or `Rebuild()` when changing the document tree. For ordinary value synchronization, bindings update through the runtime without needing a full markup rebuild.
- **A callback references invalid memory:** use a suitable registration lifetime, `BindScoped()`, a `UIXBindingGroup`, tracked binding lifetime, or explicitly unbind callbacks before their owner is destroyed.
- **The wrong file is loaded:** check the working directory for your root markup/style paths, and use source-relative imports inside RXML/RCSS files.

## 10. Quick reference

### RXML patterns

```xml
<Label text="{model.title}" />             <!-- whole expression -->
<Label text="Hello {username}" />           <!-- string interpolation -->
<Input value="{count}" valueType="int" />  <!-- two-way binding -->
<Button onClick="{save}">Save</Button>      <!-- native C++ callback -->
<Button onClick="reset">Reset</Button>      <!-- RXML Function named reset -->
<Label if="{ready}" text="Ready" />
<For each="{items}" as="item"><Label text="{item.name}" /></For>
```

### RCSS patterns

```css
:root { --accent: #6da9ff; }
.panel { padding: 16px; gap: 8px; }
#saveButton { background-color: var(--accent); }
button:hover { opacity: 0.9; }
button:disabled { opacity: 0.4; }
```

### Main APIs

```cpp
rui::Context app;
app.runtime.Bind("count", count);
app.runtime.BindObject("model", model, [](auto& b) { b.Field("title", &Model::title); });
app.runtime.BindList("items", items, [](auto& b) { b.Field("name", &Item::name); });
app.runtime.Function("save", [](const UIXArguments&, UIXEvent&) -> UIXValue { return {}; });
app.LoadStyleFile("ui.rcss");
app.LoadMarkupFile("ui.rxml");
app.Update({float(GetScreenWidth()), float(GetScreenHeight())});
app.Draw();
```

---

**Implementation note:** this guide is intended as a practical developer reference, not a claim of full web-standard CSS/XML compatibility. When you need the exact behavior/default for a specialized control, verify it in the supplied source, especially `src/rui/ui_markup.cpp`, `include/component_registry.h`, and `src/rui/css_parser.cpp`.
