#include "css_emulator.h"

#include <cstdlib>
#include <filesystem>

namespace {

constexpr float kEpsilon = 0.0001f;

bool WeakCallbackExpired(bool tracked, const std::weak_ptr<void>& lifetime) {
    return tracked && lifetime.expired();
}

std::string ElementRuntimeKey(const UIElementBase& element) {
    return element.key.empty() ? std::string("__element") : element.key;
}

Rectangle InsetRectangle(Rectangle rectangle, const EdgeValues& padding) {
    return {
        rectangle.x + padding.left,
        rectangle.y + padding.top,
        std::max(0.0f,rectangle.width-padding.left-padding.right),
        std::max(0.0f,rectangle.height-padding.top-padding.bottom)
    };
}

void DrawBasicElementBox(Rectangle rectangle, const ComputedStyle& style) {
    DrawRectangleRounded(
        rectangle,
        ResolveUIBorderRoundness(rectangle,style.border_radius),
        8,
        ApplyStyleOpacity(style.background_color,style.opacity)
    );
    DrawStyleBorder(
        rectangle,style,
        ApplyStyleOpacity(style.border_color,style.opacity),8
    );
}

float TextLineHeight(const ComputedStyle& style, float line_spacing) {
    return std::max(1.0f,style.font_size+line_spacing);
}

bool IsWhitespaceByte(char value) {
    return value == ' ' || value == '\t' || value == '\r';
}

std::vector<std::string> SplitSerialized(const std::string& value, char separator) {
    std::vector<std::string> result;
    std::string current;
    bool escape = false;
    for(char c : value) {
        if(escape) {
            current.push_back(c);
            escape = false;
        }
        else if(c == '\\') escape = true;
        else if(c == separator) {
            result.push_back(current);
            current.clear();
        }
        else current.push_back(c);
    }
    result.push_back(current);
    return result;
}

std::string EscapeSerialized(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for(char c : value) {
        if(c == '\\' || c == '|' || c == ',' || c == '(' || c == ')') result.push_back('\\');
        result.push_back(c);
    }
    return result;
}

void VisitDockNodes(
    const std::shared_ptr<UIDockNode>& node,
    const std::function<void(UIDockNode&)>& visitor
) {
    if(!node) return;
    visitor(*node);
    VisitDockNodes(node->first,visitor);
    VisitDockNodes(node->second,visitor);
}

void VisitDockNodesConst(
    const std::shared_ptr<UIDockNode>& node,
    const std::function<void(const UIDockNode&)>& visitor
) {
    if(!node) return;
    visitor(*node);
    VisitDockNodesConst(node->first,visitor);
    VisitDockNodesConst(node->second,visitor);
}

UIDockNode* FirstDockLeaf(const std::shared_ptr<UIDockNode>& node) {
    if(!node) return nullptr;
    if(node->type == UIDockNodeType::Leaf) return node.get();
    if(UIDockNode* first = FirstDockLeaf(node->first)) return first;
    return FirstDockLeaf(node->second);
}

bool RemoveDockTab(const std::shared_ptr<UIDockNode>& node,const std::string& panel_id) {
    if(!node) return false;
    bool removed = false;
    if(node->type == UIDockNodeType::Leaf) {
        const auto before = node->tabs.size();
        node->tabs.erase(
            std::remove(node->tabs.begin(),node->tabs.end(),panel_id),
            node->tabs.end()
        );
        removed = before != node->tabs.size();
        if(node->tabs.empty()) node->active_tab = 0;
        else node->active_tab = std::clamp(node->active_tab,0,static_cast<int>(node->tabs.size())-1);
    }
    return RemoveDockTab(node->first,panel_id) || RemoveDockTab(node->second,panel_id) || removed;
}

std::shared_ptr<UIDockNode> CollapseDockNode(const std::shared_ptr<UIDockNode>& node) {
    if(!node) return {};
    if(node->type == UIDockNodeType::Leaf) return node;
    node->first = CollapseDockNode(node->first);
    node->second = CollapseDockNode(node->second);
    const bool first_empty = !node->first ||
        (node->first->type == UIDockNodeType::Leaf && node->first->tabs.empty());
    const bool second_empty = !node->second ||
        (node->second->type == UIDockNodeType::Leaf && node->second->tabs.empty());
    if(first_empty && !second_empty) return node->second;
    if(second_empty && !first_empty) return node->first;
    if(first_empty && second_empty) {
        auto leaf = std::make_shared<UIDockNode>();
        leaf->id = node->id;
        return leaf;
    }
    return node;
}

UIDockDropZone DockZoneForPoint(Rectangle rectangle, Vector2 point) {
    if(!CheckCollisionPointRec(point,rectangle)) return UIDockDropZone::None;
    const float edge_x = std::min(rectangle.width*0.25f,72.0f);
    const float edge_y = std::min(rectangle.height*0.25f,72.0f);
    if(point.x < rectangle.x+edge_x) return UIDockDropZone::Left;
    if(point.x > rectangle.x+rectangle.width-edge_x) return UIDockDropZone::Right;
    if(point.y < rectangle.y+edge_y) return UIDockDropZone::Top;
    if(point.y > rectangle.y+rectangle.height-edge_y) return UIDockDropZone::Bottom;
    return UIDockDropZone::Center;
}

Rectangle DockPreviewRectangle(Rectangle rectangle, UIDockDropZone zone) {
    switch(zone) {
        case UIDockDropZone::Left: return {rectangle.x,rectangle.y,rectangle.width*0.5f,rectangle.height};
        case UIDockDropZone::Right: return {rectangle.x+rectangle.width*0.5f,rectangle.y,rectangle.width*0.5f,rectangle.height};
        case UIDockDropZone::Top: return {rectangle.x,rectangle.y,rectangle.width,rectangle.height*0.5f};
        case UIDockDropZone::Bottom: return {rectangle.x,rectangle.y+rectangle.height*0.5f,rectangle.width,rectangle.height*0.5f};
        case UIDockDropZone::Center: return rectangle;
        default: return {};
    }
}

void LayoutDockNode(
    UIDockSpace& dock,
    const std::shared_ptr<UIDockNode>& node,
    Rectangle rectangle,
    const StyleSheet& style_sheet
) {
    if(!node) return;
    node->rectangle = rectangle;
    if(node->type == UIDockNodeType::Split && node->first && node->second) {
        const float ratio = std::clamp(node->ratio,0.05f,0.95f);
        if(node->axis == UIDockAxis::Horizontal) {
            const float available = std::max(0.0f,rectangle.width-dock.splitter_size);
            float first_width = available*ratio;
            first_width = std::clamp(
                first_width,
                std::min(dock.minimum_pane_size,available),
                std::max(0.0f,available-std::min(dock.minimum_pane_size,available))
            );
            LayoutDockNode(dock,node->first,{rectangle.x,rectangle.y,first_width,rectangle.height},style_sheet);
            LayoutDockNode(
                dock,node->second,
                {rectangle.x+first_width+dock.splitter_size,rectangle.y,
                 std::max(0.0f,available-first_width),rectangle.height},style_sheet
            );
        }
        else {
            const float available = std::max(0.0f,rectangle.height-dock.splitter_size);
            float first_height = available*ratio;
            first_height = std::clamp(
                first_height,
                std::min(dock.minimum_pane_size,available),
                std::max(0.0f,available-std::min(dock.minimum_pane_size,available))
            );
            LayoutDockNode(dock,node->first,{rectangle.x,rectangle.y,rectangle.width,first_height},style_sheet);
            LayoutDockNode(
                dock,node->second,
                {rectangle.x,rectangle.y+first_height+dock.splitter_size,
                 rectangle.width,std::max(0.0f,available-first_height)},style_sheet
            );
        }
        return;
    }

    if(node->tabs.empty()) return;
    node->active_tab = std::clamp(node->active_tab,0,static_cast<int>(node->tabs.size())-1);
    UIDockPanel* panel = dock.FindPanel(node->tabs[static_cast<std::size_t>(node->active_tab)]);
    if(panel == nullptr || !panel->open || panel->floating) return;
    Rectangle body = {
        rectangle.x,
        rectangle.y+dock.tab_height,
        rectangle.width,
        std::max(0.0f,rectangle.height-dock.tab_height)
    };
    panel->content.rect.transform = body;
    panel->content.style.width = body.width;
    panel->content.style.height = body.height;
    panel->content.style.width_length = CSSLength{body.width,LengthUnit::Pixels};
    panel->content.style.height_length = CSSLength{body.height,LengthUnit::Pixels};
    MarkLayoutDirty(panel->content,false);
    LayoutContainer(panel->content,style_sheet,body.width,body.height,true);
}

Rectangle DockSplitterRectangle(const UIDockSpace& dock,const UIDockNode& node) {
    if(node.type != UIDockNodeType::Split || !node.first || !node.second) return {};
    if(node.axis == UIDockAxis::Horizontal) {
        return {
            node.first->rectangle.x+node.first->rectangle.width,
            node.rectangle.y,
            dock.splitter_size,
            node.rectangle.height
        };
    }
    return {
        node.rectangle.x,
        node.first->rectangle.y+node.first->rectangle.height,
        node.rectangle.width,
        dock.splitter_size
    };
}

void DrawDockNode(UIDockSpace& dock,const std::shared_ptr<UIDockNode>& node,const StyleSheet& sheet) {
    if(!node) return;
    if(node->type == UIDockNodeType::Split) {
        DrawDockNode(dock,node->first,sheet);
        DrawDockNode(dock,node->second,sheet);
        Rectangle splitter = DockSplitterRectangle(dock,*node);
        DrawRectangleRec(splitter,ApplyStyleOpacity(dock.style.border_color,dock.style.opacity));
        return;
    }

    DrawRectangleRec(
        {node->rectangle.x,node->rectangle.y,node->rectangle.width,dock.tab_height},
        DarkenColor(dock.style.background_color,0.08f)
    );
    float x = node->rectangle.x;
    Font font = ResolveUIFont(dock.style.font);
    for(std::size_t index=0; index<node->tabs.size(); ++index) {
        UIDockPanel* panel = dock.FindPanel(node->tabs[index]);
        if(panel == nullptr || !panel->open || panel->floating) continue;
        const float text_width = MeasureTextCached(
            font,panel->title.c_str(),dock.style.font_size,1.0f
        ).x;
        const float width = std::max(70.0f,text_width+(panel->closable ? 42.0f : 24.0f));
        Rectangle tab = {x,node->rectangle.y,width,dock.tab_height};
        const bool active = static_cast<int>(index) == node->active_tab;
        DrawRectangleRec(
            tab,
            active ? dock.style.background_color : DarkenColor(dock.style.background_color,0.14f)
        );
        DrawUIText(
            font,panel->title.c_str(),
            {tab.x+8.0f,tab.y+(tab.height-dock.style.font_size)*0.5f},
            dock.style.font_size,1.0f,dock.style.text_color
        );
        if(panel->closable) {
            const Rectangle close = {tab.x+tab.width-20.0f,tab.y+4.0f,16.0f,tab.height-8.0f};
            DrawUIText(font,"x",{close.x+3.0f,close.y},dock.style.font_size,1.0f,dock.style.text_color);
        }
        x += width;
    }
    if(node->tabs.empty()) return;
    node->active_tab = std::clamp(node->active_tab,0,static_cast<int>(node->tabs.size())-1);
    UIDockPanel* panel = dock.FindPanel(node->tabs[static_cast<std::size_t>(node->active_tab)]);
    if(panel && panel->open && !panel->floating) {
        DrawElementsClipped(
            panel->content,
            sheet,
            {node->rectangle.x,node->rectangle.y+dock.tab_height,
             node->rectangle.width,std::max(0.0f,node->rectangle.height-dock.tab_height)}
        );
    }
}

void SerializeDockNode(const std::shared_ptr<UIDockNode>& node,std::ostringstream& output) {
    if(!node) { output << "N"; return; }
    if(node->type == UIDockNodeType::Leaf) {
        output << "L(" << node->id << ',' << node->active_tab << ',';
        for(std::size_t index=0; index<node->tabs.size(); ++index) {
            if(index) output << '|';
            output << EscapeSerialized(node->tabs[index]);
        }
        output << ')';
        return;
    }
    output << "S(" << node->id << ','
           << (node->axis == UIDockAxis::Horizontal ? 'H' : 'V') << ','
           << node->ratio << ',';
    SerializeDockNode(node->first,output);
    output << ',';
    SerializeDockNode(node->second,output);
    output << ')';
}

class DockLayoutParser {
public:
    DockLayoutParser(const std::string& input,std::uint64_t& next_id)
        : input_(input),next_id_(next_id) {}

    std::shared_ptr<UIDockNode> Parse() {
        Skip();
        auto node = Node();
        Skip();
        return position_ == input_.size() ? node : nullptr;
    }

private:
    std::shared_ptr<UIDockNode> Node() {
        Skip();
        if(position_ >= input_.size()) return {};
        if(input_[position_] == 'N') { ++position_; return {}; }
        const char kind = input_[position_++];
        if(kind != 'L' && kind != 'S') return {};
        if(!Consume('(')) return {};
        auto node = std::make_shared<UIDockNode>();
        node->type = kind == 'L' ? UIDockNodeType::Leaf : UIDockNodeType::Split;
        node->id = static_cast<std::uint64_t>(Number());
        next_id_ = std::max(next_id_,node->id+1);
        if(!Consume(',')) return {};
        if(kind == 'L') {
            node->active_tab = static_cast<int>(Number());
            if(!Consume(',')) return {};
            std::string tabs = Until(')');
            for(const std::string& tab : SplitSerialized(tabs,'|')) {
                if(!tab.empty()) node->tabs.push_back(tab);
            }
            if(!Consume(')')) return {};
            return node;
        }
        Skip();
        if(position_ >= input_.size()) return {};
        node->axis = input_[position_++] == 'V' ? UIDockAxis::Vertical : UIDockAxis::Horizontal;
        if(!Consume(',')) return {};
        node->ratio = static_cast<float>(Number());
        if(!Consume(',')) return {};
        node->first = Node();
        if(!Consume(',')) return {};
        node->second = Node();
        if(!Consume(')')) return {};
        return node;
    }

    double Number() {
        Skip();
        char* end = nullptr;
        const char* begin = input_.c_str()+position_;
        double value = std::strtod(begin,&end);
        if(end == begin) return 0.0;
        position_ = static_cast<std::size_t>(end-input_.c_str());
        return value;
    }

    std::string Until(char terminal) {
        std::string result;
        bool escape = false;
        while(position_ < input_.size() && (escape || input_[position_] != terminal)) {
            char c = input_[position_++];
            if(escape) { result.push_back(c); escape = false; }
            else if(c == '\\') escape = true;
            else result.push_back(c);
        }
        return result;
    }

    bool Consume(char value) {
        Skip();
        if(position_ >= input_.size() || input_[position_] != value) return false;
        ++position_;
        return true;
    }

    void Skip() {
        while(position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
    }

    const std::string& input_;
    std::size_t position_ = 0;
    std::uint64_t& next_id_;
};

} // namespace

// -----------------------------------------------------------------------------
// Undo/redo
// -----------------------------------------------------------------------------

UIUndoStack::UIUndoStack(std::size_t capacity)
    : capacity_(std::max<std::size_t>(1,capacity)) {}

void UIUndoStack::Push(Command command,bool execute_redo) {
    if(!command.undo || !command.redo) return;
    if(execute_redo) command.redo();
    if(transaction_open_) {
        transaction_commands_.push_back(std::move(command));
        return;
    }
    if(!undo_.empty() && !command.merge_key.empty() &&
       undo_.back().merge_key == command.merge_key) {
        // Keep the original undo action and replace the redo action with the
        // newest value. This collapses continuous slider/text edits into one
        // command without losing the transaction's starting point.
        undo_.back().redo = std::move(command.redo);
        undo_.back().label = std::move(command.label);
        redo_.clear();
        return;
    }
    undo_.push_back(std::move(command));
    if(undo_.size() > capacity_) undo_.erase(undo_.begin());
    redo_.clear();
}

bool UIUndoStack::Undo() {
    if(undo_.empty() || transaction_open_) return false;
    Command command = std::move(undo_.back());
    undo_.pop_back();
    command.undo();
    redo_.push_back(std::move(command));
    return true;
}

bool UIUndoStack::Redo() {
    if(redo_.empty() || transaction_open_) return false;
    Command command = std::move(redo_.back());
    redo_.pop_back();
    command.redo();
    undo_.push_back(std::move(command));
    return true;
}

void UIUndoStack::Clear() { undo_.clear(); redo_.clear(); transaction_commands_.clear(); transaction_open_ = false; }
bool UIUndoStack::CanUndo() const { return !undo_.empty() && !transaction_open_; }
bool UIUndoStack::CanRedo() const { return !redo_.empty() && !transaction_open_; }
const std::string& UIUndoStack::UndoLabel() const { return undo_.empty() ? empty_label_ : undo_.back().label; }
const std::string& UIUndoStack::RedoLabel() const { return redo_.empty() ? empty_label_ : redo_.back().label; }
std::size_t UIUndoStack::UndoCount() const { return undo_.size(); }
std::size_t UIUndoStack::RedoCount() const { return redo_.size(); }

void UIUndoStack::BeginTransaction(std::string label) {
    if(transaction_open_) EndTransaction();
    transaction_open_ = true;
    transaction_label_ = std::move(label);
    transaction_commands_.clear();
}

void UIUndoStack::EndTransaction() {
    if(!transaction_open_) return;
    transaction_open_ = false;
    if(transaction_commands_.empty()) return;
    std::vector<Command> commands = std::move(transaction_commands_);
    Command grouped;
    grouped.label = std::move(transaction_label_);
    grouped.undo = [commands]() mutable {
        for(auto iterator=commands.rbegin(); iterator!=commands.rend(); ++iterator) iterator->undo();
    };
    grouped.redo = [commands]() mutable {
        for(Command& command : commands) command.redo();
    };
    Push(std::move(grouped),false);
}

void UIUndoStack::CancelTransaction() {
    if(!transaction_open_) return;
    for(auto iterator=transaction_commands_.rbegin(); iterator!=transaction_commands_.rend(); ++iterator) {
        iterator->undo();
    }
    transaction_commands_.clear();
    transaction_open_ = false;
    transaction_label_.clear();
}

// -----------------------------------------------------------------------------
// General drag/drop
// -----------------------------------------------------------------------------

UIDragDropContext& GetUIDragDropContext() {
    static UIDragDropContext context;
    return context;
}

void UIDragDropContext::BeginFrame() {
    // Drag/drop spans coordinate spaces (for example an asset browser into a
    // Viewport), so the shared drag pointer and preview rectangles are always
    // stored in physical screen coordinates.
    pointer_ = ::GetMousePosition();
    preview_ = {};
    preview_target_.reset();
    released_this_frame_ = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool UIDragDropContext::BeginDrag(
    std::string source_key,
    UIDragPayload payload,
    Vector2 pointer,
    std::function<void(bool)> on_end
) {
    if(dragging_) return false;
    dragging_ = true;
    source_key_ = std::move(source_key);
    payload.source_key = source_key_;
    payload_ = std::move(payload);
    pointer_ = pointer;
    on_end_ = std::move(on_end);
    return true;
}

void UIDragDropContext::ConsiderTarget(
    std::string target_key,
    Rectangle rectangle,
    UIDropTargetBinding& target
) {
    if(!dragging_ || !CheckCollisionPointRec(pointer_,rectangle)) return;
    if(WeakCallbackExpired(target.lifetime_tracked,target.lifetime)) return;
    bool enabled = target.enabled;
    if(target.update_enabled) target.update_enabled(enabled);
    if(!enabled) return;
    const bool accepted = !target.accepts || target.accepts(payload_);
    preview_ = {rectangle,std::move(target_key),accepted};
    if(accepted) preview_target_ = target;
    else preview_target_.reset();
    if(accepted && target.on_preview) target.on_preview(payload_);
}

void UIDragDropContext::EndFrame() {
    if(!dragging_ || !released_this_frame_) return;
    bool accepted = preview_target_.has_value() && preview_.accepted;
    if(accepted && WeakCallbackExpired(
        preview_target_->lifetime_tracked,preview_target_->lifetime
    )) accepted = false;
    if(accepted && preview_target_->on_drop) preview_target_->on_drop(payload_);
    if(on_end_) on_end_(accepted);
    dragging_ = false;
    payload_ = {};
    source_key_.clear();
    preview_ = {};
    preview_target_.reset();
    on_end_ = {};
}

void UIDragDropContext::Cancel() {
    if(dragging_ && on_end_) on_end_(false);
    dragging_ = false;
    payload_ = {};
    source_key_.clear();
    preview_ = {};
    preview_target_.reset();
    on_end_ = {};
}

void UpdateElementDragDrop(UIElementBase& element) {
    if(!element.drag_source && !element.drop_target) return;

    UIDragDropContext& context = GetUIDragDropContext();
    if(element.drag_source) {
        UIDragSourceBinding& source = *element.drag_source;
        if(WeakCallbackExpired(source.lifetime_tracked,source.lifetime)) {
            element.drag_source.reset();
        }
        else {
            bool enabled = source.enabled;
            if(source.update_enabled) source.update_enabled(enabled);
            const Vector2 mouse = GetUIMousePosition();
            const Vector2 screen_mouse = UITransformPointToScreen(mouse);
            const bool hovered = CheckCollisionPointRec(mouse,element.transform);
            if(!context.Dragging() && enabled && hovered && IsMouseButtonPressed(source.mouse_button)) {
                source.pressed_inside = true;
                source.press_origin = screen_mouse;
            }
            if(source.pressed_inside && IsMouseButtonDown(source.mouse_button)) {
                const float dx = screen_mouse.x-source.press_origin.x;
                const float dy = screen_mouse.y-source.press_origin.y;
                if(dx*dx+dy*dy >= source.threshold*source.threshold && source.make_payload) {
                    UIDragPayload payload = source.make_payload();
                    const auto on_end_callback = source.on_end;
                    const std::weak_ptr<void> source_lifetime = source.lifetime;
                    const bool source_lifetime_tracked = source.lifetime_tracked;
                    auto on_end = [on_end_callback,source_lifetime,source_lifetime_tracked](bool accepted) {
                        if(WeakCallbackExpired(source_lifetime_tracked,source_lifetime)) return;
                        if(on_end_callback) on_end_callback(accepted);
                    };
                    if(context.BeginDrag(ElementRuntimeKey(element),std::move(payload),screen_mouse,std::move(on_end))) {
                        source.pressed_inside = false;
                        if(source.on_begin) source.on_begin();
                        element.pseudo_states.SetKnown("dragging",true);
                    }
                }
            }
            if(IsMouseButtonReleased(source.mouse_button)) source.pressed_inside = false;
        }
    }
    if(element.drop_target) {
        UIDropTargetBinding& target = *element.drop_target;
        if(WeakCallbackExpired(target.lifetime_tracked,target.lifetime)) {
            element.drop_target.reset();
        }
        else {
            context.ConsiderTarget(
                ElementRuntimeKey(element),
                UITransformRectangleToScreen(element.transform),
                target
            );
            element.pseudo_states.Set(
                "drag-over",
                context.Preview().target_key == ElementRuntimeKey(element) &&
                context.Preview().accepted
            );
        }
    }
    if(element.drag_source && !context.Dragging()) {
        element.pseudo_states.SetKnown("dragging",false);
    }
}

// -----------------------------------------------------------------------------
// Shared virtualization
// -----------------------------------------------------------------------------

UIVisibleRange ComputeUIVisibleRange(
    std::size_t item_count,
    float item_extent,
    float scroll_offset,
    float viewport_extent,
    std::size_t overscan
) {
    UIVisibleRange result;
    if(item_count == 0 || item_extent <= 0.0f || viewport_extent <= 0.0f) return result;
    const std::size_t first_visible = static_cast<std::size_t>(
        std::max(0.0f,std::floor(std::max(0.0f,scroll_offset)/item_extent))
    );
    const std::size_t visible_count = static_cast<std::size_t>(
        std::ceil(viewport_extent/item_extent)
    )+1;
    result.first = first_visible > overscan ? first_visible-overscan : 0;
    result.last = std::min(item_count,first_visible+visible_count+overscan);
    result.empty = result.first >= result.last;
    return result;
}

// -----------------------------------------------------------------------------
// Text box
// -----------------------------------------------------------------------------

UITextBox& UITextBox::Text(std::string value,bool restart_animation) {
    if(text == value) return *this;
    text = std::move(value);
    caret = std::min(caret,text.size());
    wrapped_lines.clear();
    if(restart_animation) PlayTextAnimation(true);
    return *this;
}
UITextBox& UITextBox::Placeholder(std::string value) { placeholder = std::move(value); return *this; }
UITextBox& UITextBox::Editable(bool value) { editable = value; return *this; }
UITextBox& UITextBox::Multiline(bool value) { multiline = value; return *this; }
UITextBox& UITextBox::Wrap(bool value) { wrap = value; wrapped_lines.clear(); return *this; }
UITextBox& UITextBox::DynamicHeight(bool value) { dynamic_height = value; return *this; }
UITextBox& UITextBox::MaxCharacters(std::size_t value) { max_characters = value; return *this; }
UITextBox& UITextBox::Animate(UITextAnimationMode mode,float duration,bool loop) {
    text_animation.mode = mode;
    text_animation.duration = std::max(0.001f,duration);
    text_animation.loop = loop;
    return PlayTextAnimation(true);
}
UITextBox& UITextBox::PlayTextAnimation(bool restart) {
    if(restart || !text_animation.playing) text_animation.started = GetTime();
    text_animation.playing = true;
    return *this;
}
UITextBox& UITextBox::StopTextAnimation() { text_animation.playing = false; return *this; }
UITextBox& UITextBox::OnChange(std::function<void(UITextBox&)> callback) { on_change = std::move(callback); return *this; }
UITextBox& UITextBox::OnSubmit(std::function<void(UITextBox&)> callback) { on_submit = std::move(callback); return *this; }

void UITextBox::RebuildWrap(float available_width) {
    wrapped_lines.clear();
    const std::string& source = text.empty() ? placeholder : text;
    Font font = ResolveUIFont(style.font);
    const float width = std::max(1.0f,available_width);
    std::size_t global = 0;
    std::size_t line_start = 0;
    std::string line;
    std::string word;
    std::size_t word_start = 0;

    auto push_line = [&](std::string value,std::size_t first) {
        UITextWrappedLine wrapped;
        wrapped.text = std::move(value);
        wrapped.first_character = first;
        wrapped.character_count = wrapped.text.size();
        wrapped.width = MeasureTextCached(font,wrapped.text.c_str(),style.font_size,1.0f).x;
        wrapped_lines.push_back(std::move(wrapped));
    };

    auto flush_word = [&]() {
        if(word.empty()) return;
        std::string candidate = line + word;
        const float candidate_width = MeasureTextCached(font,candidate.c_str(),style.font_size,1.0f).x;
        if(wrap && !line.empty() && candidate_width > width) {
            while(!line.empty() && IsWhitespaceByte(line.back())) line.pop_back();
            push_line(line,line_start);
            line = word;
            line_start = word_start;
        }
        else if(wrap && line.empty() && candidate_width > width) {
            std::string fragment;
            std::size_t fragment_start = word_start;
            for(char c : word) {
                std::string next = fragment+c;
                if(!fragment.empty() && MeasureTextCached(font,next.c_str(),style.font_size,1.0f).x > width) {
                    push_line(fragment,fragment_start);
                    fragment_start += fragment.size();
                    fragment.assign(1,c);
                }
                else fragment = std::move(next);
            }
            line = std::move(fragment);
            line_start = fragment_start;
        }
        else line = std::move(candidate);
        word.clear();
    };

    for(char c : source) {
        if(c == '\n') {
            flush_word();
            push_line(line,line_start);
            line.clear();
            ++global;
            line_start = global;
            continue;
        }
        if(IsWhitespaceByte(c)) {
            flush_word();
            if(line.empty()) line_start = global;
            line.push_back(c);
        }
        else {
            if(word.empty()) word_start = global;
            word.push_back(c);
        }
        ++global;
    }
    flush_word();
    if(!line.empty() || wrapped_lines.empty()) push_line(line,line_start);
}

Vector2 UITextBox::Measure(const StyleSheet&,float available_width,float) {
    const float content_width = std::max(1.0f,
        (style.width_length && style.width_length->unit != LengthUnit::Auto ? style.width : available_width)-
        style.padding.left-style.padding.right
    );
    RebuildWrap(content_width);
    float width = 0.0f;
    for(const UITextWrappedLine& line : wrapped_lines) width = std::max(width,line.width);
    const float height = TextLineHeight(style,line_spacing)*static_cast<float>(wrapped_lines.size());
    return {
        width+style.padding.left+style.padding.right,
        height+style.padding.top+style.padding.bottom
    };
}

void UITextBox::Layout(const StyleSheet&,Rectangle bounds) {
    rect.transform = bounds;
    RebuildWrap(std::max(1.0f,bounds.width-style.padding.left-style.padding.right));
    if(dynamic_height || (style.height_length && style.height_length->unit == LengthUnit::Auto)) {
        rect.transform.height = TextLineHeight(style,line_spacing)*static_cast<float>(wrapped_lines.size())+
            style.padding.top+style.padding.bottom;
    }
}

std::size_t UITextBox::VisibleCharacterCount(double now) const {
    if(!text_animation.playing || text_animation.mode == UITextAnimationMode::None ||
       text_animation.mode == UITextAnimationMode::Wave) return text.size();
    double elapsed = now-text_animation.started-text_animation.delay;
    if(elapsed <= 0.0) return 0;
    const double duration = std::max(0.001,static_cast<double>(text_animation.duration));
    if(text_animation.loop) elapsed = std::fmod(elapsed,duration);
    const float progress = std::clamp(static_cast<float>(elapsed/duration),0.0f,1.0f);
    return std::min(text.size(),static_cast<std::size_t>(std::ceil(progress*static_cast<float>(text.size()))));
}

namespace {

void NormalizeTextBoxSelection(UITextBox& box) {
    box.caret = std::min(box.caret,box.text.size());
    box.selection_anchor = std::min(box.selection_anchor,box.text.size());
    box.selection_start = std::min(box.selection_anchor,box.caret);
    box.selection_end = std::max(box.selection_anchor,box.caret);
}

void CollapseTextBoxSelection(UITextBox& box,std::size_t caret) {
    box.caret = std::min(caret,box.text.size());
    box.selection_anchor = box.caret;
    box.selection_start = box.caret;
    box.selection_end = box.caret;
}

bool DeleteTextBoxSelection(UITextBox& box) {
    NormalizeTextBoxSelection(box);
    if(box.selection_end <= box.selection_start) return false;
    const std::size_t start = box.selection_start;
    box.text.erase(start,box.selection_end-start);
    CollapseTextBoxSelection(box,start);
    return true;
}

std::size_t TextBoxCaretFromPoint(const UITextBox& box,Vector2 mouse) {
    if(box.wrapped_lines.empty()) return 0;
    const Rectangle content = InsetRectangle(box.rect.transform,box.style.padding);
    const float line_height = TextLineHeight(box.style,box.line_spacing);
    int line_index = static_cast<int>(std::floor(
        (mouse.y-content.y+box.scroll_y)/std::max(line_height,1.0f)
    ));
    line_index = std::clamp(line_index,0,static_cast<int>(box.wrapped_lines.size())-1);
    const UITextWrappedLine& line = box.wrapped_lines[static_cast<std::size_t>(line_index)];
    Font font = ResolveUIFont(box.style.font);
    const float local_x = mouse.x-content.x;
    if(local_x <= 0.0f) return line.first_character;
    float previous = 0.0f;
    for(std::size_t index=1; index<=line.text.size(); ++index) {
        const float width = MeasureTextCached(
            font,line.text.substr(0,index).c_str(),box.style.font_size,1.0f
        ).x;
        if(local_x < (previous+width)*0.5f) return line.first_character+index-1;
        previous = width;
    }
    return std::min(line.first_character+line.character_count,box.text.size());
}

} // namespace

void UITextBox::Update(bool inherited_disabled) {
    changed = false;
    submitted = false;
    const bool blocked = disabled || inherited_disabled;
    const Vector2 mouse = GetUIMousePosition();
    hovered = !blocked && CheckCollisionPointRec(mouse,rect.transform);

    if(editable && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        focused = true;
        const std::size_t next = TextBoxCaretFromPoint(*this,mouse);
        const bool extend = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if(!extend) selection_anchor = next;
        caret = next;
        NormalizeTextBoxSelection(*this);
        mouse_selecting = true;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) mouse_selecting = false;
    if(mouse_selecting && focused && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        caret = TextBoxCaretFromPoint(*this,mouse);
        NormalizeTextBoxSelection(*this);
    }

    if(!editable || blocked || !focused) return;

    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if(control && IsKeyPressed(KEY_A)) {
        selection_anchor = 0;
        caret = text.size();
        NormalizeTextBoxSelection(*this);
    }

    auto moveCaret = [&](std::size_t next) {
        next = std::min(next,text.size());
        if(!shift) selection_anchor = next;
        caret = next;
        NormalizeTextBoxSelection(*this);
    };

    if(IsKeyPressed(KEY_HOME)) moveCaret(0);
    else if(IsKeyPressed(KEY_END)) moveCaret(text.size());
    else if(UIKeyPressedOrRepeat(KEY_LEFT)) {
        if(!shift && selection_end > selection_start) moveCaret(selection_start);
        else moveCaret(caret > 0 ? caret-1 : 0);
    }
    else if(UIKeyPressedOrRepeat(KEY_RIGHT)) {
        if(!shift && selection_end > selection_start) moveCaret(selection_end);
        else moveCaret(std::min(text.size(),caret+1));
    }

    int character = GetCharPressed();
    while(character > 0) {
        if(character >= 32 && (selection_end > selection_start || text.size() < max_characters)) {
            DeleteTextBoxSelection(*this);
            if(text.size() < max_characters) {
                text.insert(text.begin()+static_cast<std::ptrdiff_t>(caret),static_cast<char>(character));
                CollapseTextBoxSelection(*this,caret+1);
                changed = true;
            }
        }
        character = GetCharPressed();
    }

    if(UIKeyPressedOrRepeat(KEY_BACKSPACE)) {
        if(DeleteTextBoxSelection(*this)) changed = true;
        else if(caret > 0) {
            text.erase(caret-1,1);
            CollapseTextBoxSelection(*this,caret-1);
            changed = true;
        }
    }
    if(UIKeyPressedOrRepeat(KEY_DELETE)) {
        if(DeleteTextBoxSelection(*this)) changed = true;
        else if(caret < text.size()) {
            text.erase(caret,1);
            CollapseTextBoxSelection(*this,caret);
            changed = true;
        }
    }
    if(IsKeyPressed(KEY_ENTER)) {
        if(multiline && !control && (selection_end > selection_start || text.size() < max_characters)) {
            DeleteTextBoxSelection(*this);
            if(text.size() < max_characters) {
                text.insert(text.begin()+static_cast<std::ptrdiff_t>(caret),'\n');
                CollapseTextBoxSelection(*this,caret+1);
                changed = true;
            }
        }
        else submitted = true;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        focused = false;
        mouse_selecting = false;
    }
    if(changed) {
        wrapped_lines.clear();
        if(on_change) on_change(*this);
    }
    if(submitted && on_submit) on_submit(*this);
}

void UITextBox::Draw(const StyleSheet& style_sheet) {
    rect.pseudo_states.SetKnown("focus",focused);
    rect.pseudo_states.SetKnown("hover",hovered);
    rect.pseudo_states.SetKnown("disabled",disabled || rect.inherited_disabled);
    rect.pseudo_states.Update();
    ComputedStyle draw_style = ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::TextBox,style.id,rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style = ResolveAnimatedStyle(rect,style_sheet,std::move(draw_style),"textbox");
    DrawBasicElementBox(rect.transform,draw_style);
    Rectangle content = InsetRectangle(rect.transform,draw_style.padding);
    const std::string& source = text.empty() ? placeholder : text;
    const Color base_color = text.empty()
        ? ApplyStyleOpacity(draw_style.text_color,draw_style.opacity*0.55f)
        : ApplyStyleOpacity(draw_style.text_color,draw_style.opacity);
    const double now = GetTime();
    const std::size_t visible = VisibleCharacterCount(now);
    Font font = ResolveUIFont(draw_style.font);
    NormalizeTextBoxSelection(*this);
    float y = content.y-scroll_y;
    const float line_height = TextLineHeight(draw_style,line_spacing);
    for(const UITextWrappedLine& line : wrapped_lines) {
        if(y+line_height >= content.y && y <= content.y+content.height) {
            if(editable && focused && selection_end > selection_start) {
                const std::size_t line_start = line.first_character;
                const std::size_t line_end = line.first_character+line.character_count;
                const std::size_t selected_start = std::max(selection_start,line_start);
                const std::size_t selected_end = std::min(selection_end,line_end);
                if(selected_end > selected_start) {
                    const std::size_t local_start = selected_start-line_start;
                    const std::size_t local_end = selected_end-line_start;
                    const float selection_x = MeasureTextCached(
                        font,line.text.substr(0,local_start).c_str(),draw_style.font_size,1.0f
                    ).x;
                    const float selection_end_x = MeasureTextCached(
                        font,line.text.substr(0,local_end).c_str(),draw_style.font_size,1.0f
                    ).x;
                    const UIFontVerticalExtent extent = MeasureFontVerticalExtent(font,draw_style.font_size);
                    DrawRectangleRec(
                        {content.x+selection_x,y+extent.top-1.0f,std::max(2.0f,selection_end_x-selection_x),extent.Height()+2.0f},
                        Color{80,140,255,110}
                    );
                }
            }
            std::size_t count = line.character_count;
            if(line.first_character >= visible) count = 0;
            else count = std::min(count,visible-line.first_character);
            std::string shown = line.text.substr(0,count);
            if(text_animation.mode == UITextAnimationMode::Wave && text_animation.playing) {
                float x = content.x;
                for(std::size_t index=0; index<shown.size(); ++index) {
                    std::string glyph(1,shown[index]);
                    const float phase = static_cast<float>(now-text_animation.started)*text_animation.wave_frequency+
                        static_cast<float>(line.first_character+index)*0.45f;
                    const float offset = std::sin(phase)*text_animation.wave_amplitude;
                    DrawUIText(font,glyph.c_str(),{x,y+offset},draw_style.font_size,1.0f,base_color);
                    x += MeasureTextCached(font,glyph.c_str(),draw_style.font_size,1.0f).x;
                }
            }
            else {
                Color color = base_color;
                if(text_animation.mode == UITextAnimationMode::FadeCharacters && text_animation.playing && count > 0) {
                    const double elapsed = now-text_animation.started-text_animation.delay;
                    const float newest_age = static_cast<float>(elapsed)-
                        static_cast<float>(line.first_character+count-1)*text_animation.stagger;
                    const float alpha = std::clamp(newest_age/std::max(0.001f,text_animation.stagger*4.0f),0.0f,1.0f);
                    color = ApplyStyleOpacity(color,alpha);
                }
                DrawUIText(font,shown.c_str(),{content.x,y},draw_style.font_size,1.0f,color);
            }
        }
        y += line_height;
    }
    if(editable && focused && (static_cast<int>(now*2.0)&1)==0) {
        const std::string prefix = source.substr(0,std::min(caret,source.size()));
        const std::size_t last_newline = prefix.find_last_of('\n');
        const std::string current = last_newline == std::string::npos ? prefix : prefix.substr(last_newline+1);
        const float caret_x = content.x+MeasureTextCached(font,current.c_str(),draw_style.font_size,1.0f).x;
        std::size_t line_index = 0;
        for(std::size_t index=0; index<wrapped_lines.size(); ++index) {
            const auto& line = wrapped_lines[index];
            if(caret >= line.first_character && caret <= line.first_character+line.character_count) {
                line_index = index;
                break;
            }
        }
        const float caret_y = content.y+static_cast<float>(line_index)*line_height-scroll_y;
        const UIFontVerticalExtent extent = MeasureFontVerticalExtent(font,draw_style.font_size);
        DrawLineEx(
            {caret_x,caret_y+extent.top-1.0f},
            {caret_x,caret_y+extent.bottom+1.0f},
            1.0f,base_color
        );
    }
}

void UITextBox::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old = dynamic_cast<UITextBox*>(&previous);
    if(!old) return;
    focused = old->focused;
    caret = old->caret;
    selection_anchor = old->selection_anchor;
    selection_start = old->selection_start;
    selection_end = old->selection_end;
    mouse_selecting = old->mouse_selecting;
    scroll_y = old->scroll_y;
    const bool incoming_text_changed = text != old->text;
    if(options.preserve_values) {
        text = old->text;
        text_animation = old->text_animation;
    }
    else if(!incoming_text_changed) {
        text_animation = old->text_animation;
    }
    else if(text_animation.mode != UITextAnimationMode::None) {
        PlayTextAnimation(true);
    }
    caret = std::min(caret,text.size());
    selection_anchor = std::min(selection_anchor,text.size());
    selection_start = std::min(selection_start,text.size());
    selection_end = std::min(selection_end,text.size());
}

// -----------------------------------------------------------------------------
// Table
// -----------------------------------------------------------------------------

UITable& UITable::AddColumn(UITableColumn column) { columns.push_back(std::move(column)); return *this; }
UITable& UITable::AddRow(UITableRow row) { rows.push_back(std::move(row)); return *this; }
UITable& UITable::ClearRows() { rows.clear(); selected_row = -1; selected_keys.clear(); return *this; }

float UITable::ContentWidth() const {
    float total = 0.0f;
    for(const UITableColumn& column : columns) total += column.width;
    return total;
}
float UITable::ContentHeight() const { return header_height+row_height*static_cast<float>(rows.size()); }
UIVisibleRange UITable::VisibleRows() const {
    return ComputeUIVisibleRange(
        rows.size(),row_height,scroll_y,
        std::max(0.0f,rect.transform.height-header_height),overscan
    );
}

UITable& UITable::SortBy(int column,bool ascending) {
    if(column < 0 || column >= static_cast<int>(columns.size()) || !columns[static_cast<std::size_t>(column)].sortable) return *this;
    sort_column = column;
    sort_ascending = ascending;
    std::stable_sort(rows.begin(),rows.end(),[&](const UITableRow& a,const UITableRow& b) {
        const std::string left = column < static_cast<int>(a.cells.size()) ? a.cells[static_cast<std::size_t>(column)] : std::string{};
        const std::string right = column < static_cast<int>(b.cells.size()) ? b.cells[static_cast<std::size_t>(column)] : std::string{};
        return ascending ? left < right : left > right;
    });
    if(on_sort) on_sort(*this,column,ascending);
    return *this;
}

Vector2 UITable::Measure(const StyleSheet&,float available_width,float available_height) {
    return {
        std::min(std::max(100.0f,ContentWidth()),available_width),
        std::min(std::max(header_height+row_height,ContentHeight()),available_height)
    };
}
void UITable::Layout(const StyleSheet&,Rectangle bounds) { rect.transform = bounds; }

void UITable::Update(bool inherited_disabled) {
    const bool disabled = inherited_disabled;
    const Vector2 mouse = GetUIMousePosition();
    const bool hovered = !disabled && CheckCollisionPointRec(mouse,rect.transform);
    hovered_row = -1;
    if(hovered) {
        const float wheel = GetMouseWheelMove();
        if(std::abs(wheel) > kEpsilon) {
            const float maximum = std::max(0.0f,ContentHeight()-rect.transform.height);
            scroll_y = std::clamp(scroll_y-wheel*row_height*3.0f,0.0f,maximum);
        }
    }

    if(resizing_column >= 0) {
        UITableColumn& column = columns[static_cast<std::size_t>(resizing_column)];
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            column.width = std::clamp(
                resize_start_width+(mouse.x-resize_anchor_x),column.min_width,column.max_width
            );
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
        }
        else resizing_column = -1;
        return;
    }

    float x = rect.transform.x;
    for(std::size_t index=0; index<columns.size(); ++index) {
        UITableColumn& column = columns[index];
        Rectangle header = {x,rect.transform.y,column.width,header_height};
        Rectangle handle = {x+column.width-3.0f,header.y,6.0f,header.height};
        if(hovered && column.resizable && CheckCollisionPointRec(mouse,handle) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            resizing_column = static_cast<int>(index);
            resize_anchor_x = mouse.x;
            resize_start_width = column.width;
            return;
        }
        if(hovered && column.sortable && CheckCollisionPointRec(mouse,header) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            SortBy(static_cast<int>(index),sort_column == static_cast<int>(index) ? !sort_ascending : true);
            return;
        }
        x += column.width;
    }

    const float body_y = rect.transform.y+header_height;
    if(hovered && mouse.y >= body_y) {
        const int row = static_cast<int>(std::floor((mouse.y-body_y+scroll_y)/row_height));
        if(row >= 0 && row < static_cast<int>(rows.size())) {
            hovered_row = row;
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !rows[static_cast<std::size_t>(row)].disabled) {
                focused_ = true;
                selected_row = row;
                const std::string key = rows[static_cast<std::size_t>(row)].key.empty()
                    ? std::to_string(row) : rows[static_cast<std::size_t>(row)].key;
                if(multi_select && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
                    if(selected_keys.count(key)) selected_keys.erase(key);
                    else selected_keys.insert(key);
                }
                else {
                    selected_keys.clear();
                    selected_keys.insert(key);
                }
                if(on_select) on_select(*this,static_cast<std::size_t>(row));
            }
        }
    }
}

void UITable::Draw(const StyleSheet& style_sheet) {
    ComputedStyle draw_style = ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::Table,style.id,rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style = ResolveAnimatedStyle(rect,style_sheet,std::move(draw_style),"table");
    DrawBasicElementBox(rect.transform,draw_style);
    Font font = ResolveUIFont(draw_style.font);
    float x = rect.transform.x;
    for(std::size_t column_index=0; column_index<columns.size(); ++column_index) {
        const UITableColumn& column = columns[column_index];
        Rectangle header = {x,rect.transform.y,column.width,header_height};
        DrawRectangleRec(header,DarkenColor(draw_style.background_color,0.10f));
        DrawUIText(
            font,column.title.c_str(),
            {header.x+6.0f,header.y+(header.height-draw_style.font_size)*0.5f},
            draw_style.font_size,1.0f,draw_style.text_color
        );
        if(sort_column == static_cast<int>(column_index)) {
            const char* arrow = sort_ascending ? "^" : "v";
            DrawUIText(font,arrow,{header.x+header.width-14.0f,header.y+5.0f},draw_style.font_size,1.0f,draw_style.text_color);
        }
        DrawLineEx({header.x+header.width,header.y},{header.x+header.width,header.y+rect.transform.height},1.0f,draw_style.border_color);
        x += column.width;
    }
    const UIVisibleRange range = VisibleRows();
    if(range.empty) return;
    for(std::size_t row_index=range.first; row_index<range.last; ++row_index) {
        const UITableRow& row = rows[row_index];
        const float y = rect.transform.y+header_height+static_cast<float>(row_index)*row_height-scroll_y;
        if(y+row_height < rect.transform.y+header_height || y > rect.transform.y+rect.transform.height) continue;
        const std::string key = row.key.empty() ? std::to_string(row_index) : row.key;
        const bool selected = selected_keys.count(key) != 0 || selected_row == static_cast<int>(row_index);
        Color background = selected
            ? MixColors(draw_style.background_color,draw_style.text_color,0.18f)
            : (row_index%2 ? DarkenColor(draw_style.background_color,0.035f) : draw_style.background_color);
        if(hovered_row == static_cast<int>(row_index)) background = LightenColor(background,0.06f);
        DrawRectangleRec({rect.transform.x,y,rect.transform.width,row_height},background);
        float cell_x = rect.transform.x;
        for(std::size_t column_index=0; column_index<columns.size(); ++column_index) {
            const UITableColumn& column = columns[column_index];
            const std::string value = column_index < row.cells.size() ? row.cells[column_index] : std::string{};
            Vector2 measured = MeasureTextCached(font,value.c_str(),draw_style.font_size,1.0f);
            float text_x = cell_x+6.0f;
            if(column.alignment == TextAlign::Center) text_x = cell_x+(column.width-measured.x)*0.5f;
            else if(column.alignment == TextAlign::Right) text_x = cell_x+column.width-measured.x-6.0f;
            DrawUIText(font,value.c_str(),{text_x,y+(row_height-draw_style.font_size)*0.5f},draw_style.font_size,1.0f,row.disabled?ApplyStyleOpacity(draw_style.text_color,0.45f):draw_style.text_color);
            cell_x += column.width;
        }
    }
}

void UITable::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old = dynamic_cast<UITable*>(&previous);
    if(!old) return;
    scroll_y = old->scroll_y;
    sort_column = old->sort_column;
    sort_ascending = old->sort_ascending;
    resizing_column = old->resizing_column;
    focused_ = old->focused_;
    if(options.preserve_selection) {
        selected_row = old->selected_row;
        selected_keys = old->selected_keys;
    }
    for(UITableColumn& column : columns) {
        auto found = std::find_if(old->columns.begin(),old->columns.end(),[&](const UITableColumn& candidate) {
            return !column.id.empty() && column.id == candidate.id;
        });
        if(found != old->columns.end()) column.width = found->width;
    }
}

// -----------------------------------------------------------------------------
// Virtual list
// -----------------------------------------------------------------------------

UIVirtualList& UIVirtualList::Items(std::vector<std::string> values) {
    items = std::move(values);
    virtual_item_count = 0;
    build_visible_items = {};
    content = {};
    built_range_valid_ = false;
    return *this;
}

UIVirtualList& UIVirtualList::VirtualItems(
    std::size_t count,
    BuildVisibleItems builder
) {
    virtual_item_count = count;
    build_visible_items = std::move(builder);
    items.clear();
    content = {};
    built_range_valid_ = false;
    return *this;
}

UIVirtualList& UIVirtualList::ItemHeight(float value) {
    item_height = std::max(1.0f,value);
    built_range_valid_ = false;
    return *this;
}

UIVirtualList& UIVirtualList::Overscan(std::size_t value) {
    overscan = value;
    built_range_valid_ = false;
    return *this;
}

UIVirtualList& UIVirtualList::OnSelect(
    std::function<void(UIVirtualList&,std::size_t)> callback
) {
    on_select = std::move(callback);
    return *this;
}

UIVisibleRange UIVirtualList::VisibleItems() const {
    return ComputeUIVisibleRange(
        ItemCount(),
        item_height,
        scroll_y,
        rect.transform.height,
        overscan
    );
}

Vector2 UIVirtualList::Measure(
    const StyleSheet&,
    float available_width,
    float available_height
) {
    return {
        available_width,
        std::min(
            available_height,
            item_height*static_cast<float>(ItemCount())
        )
    };
}

void UIVirtualList::RebuildVisibleContent(
    std::size_t first,
    std::size_t last
) {
    if(!UsesTemplate()) return;

    UIContainer rebuilt;
    rebuilt.direction = FlexDirection::Column;
    rebuilt.style.display = DisplayType::Flex;
    rebuilt.style.flex_direction = FlexDirection::Column;
    rebuilt.style.overflow = OverflowType::Scroll;
    rebuilt.style.gap = 0.0f;
    rebuilt.style.row_gap = 0.0f;
    rebuilt.style.column_gap = 0.0f;
    rebuilt.style.gap_length = CSSLength{0.0f,LengthUnit::Pixels};
    rebuilt.style.row_gap_length = CSSLength{0.0f,LengthUnit::Pixels};
    rebuilt.style.column_gap_length = CSSLength{0.0f,LengthUnit::Pixels};
    rebuilt.show_scrollbars = false;

    auto add_spacer = [&](float height, const char* suffix) {
        if(height <= 0.0f) return;

        UIContainer spacer;
        spacer.direction = FlexDirection::Column;
        spacer.style.display = DisplayType::Block;
        spacer.style.width_length = CSSLength{100.0f,LengthUnit::Percent};
        spacer.style.height = height;
        spacer.style.height_length = CSSLength{height,LengthUnit::Pixels};
        spacer.style.min_height = height;
        spacer.style.max_height = height;
        spacer.style.min_height_length = CSSLength{height,LengthUnit::Pixels};
        spacer.style.max_height_length = CSSLength{height,LengthUnit::Pixels};
        spacer.style.flex_grow = 0.0f;
        spacer.style.flex_shrink = 0.0f;
        spacer.rect.key = std::string("__virtual-list-")+suffix;
        rebuilt.Add(std::move(spacer));
    };

    add_spacer(static_cast<float>(first)*item_height,"before");
    build_visible_items(*this,rebuilt,first,last);

    const std::size_t count = ItemCount();
    add_spacer(static_cast<float>(count-last)*item_height,"after");

    UIReconcileOptions reconcile;
    reconcile.preserve_values = false;
    reconcile.preserve_selection = true;
    reconcile.preserve_open_state = true;
    reconcile.preserve_runtime_layout = true;
    reconcile.match_unkeyed_by_index = true;
    ReconcileUI(content,std::move(rebuilt),reconcile);

    built_first_ = first;
    built_last_ = last;
    built_range_valid_ = true;
}

void UIVirtualList::Layout(
    const StyleSheet& style_sheet,
    Rectangle bounds
) {
    rect.transform = bounds;

    const float maximum = std::max(
        0.0f,
        item_height*static_cast<float>(ItemCount())-rect.transform.height
    );
    scroll_y = std::clamp(scroll_y,0.0f,maximum);

    if(!UsesTemplate()) return;

    const UIVisibleRange range = VisibleItems();
    const std::size_t first = range.empty ? 0 : range.first;
    const std::size_t last = range.empty ? 0 : range.last;

    if(!built_range_valid_ || first != built_first_ || last != built_last_) {
        RebuildVisibleContent(first,last);
    }

    content.rect.transform = bounds;
    content.style.width = bounds.width;
    content.style.height = bounds.height;
    content.style.width_length = CSSLength{bounds.width,LengthUnit::Pixels};
    content.style.height_length = CSSLength{bounds.height,LengthUnit::Pixels};
    content.style.min_width = bounds.width;
    content.style.max_width = bounds.width;
    content.style.min_height = bounds.height;
    content.style.max_height = bounds.height;
    content.style.min_width_length = CSSLength{bounds.width,LengthUnit::Pixels};
    content.style.max_width_length = CSSLength{bounds.width,LengthUnit::Pixels};
    content.style.min_height_length = CSSLength{bounds.height,LengthUnit::Pixels};
    content.style.max_height_length = CSSLength{bounds.height,LengthUnit::Pixels};
    content.scroll.y = scroll_y;
    content.show_scrollbars = false;

    MarkLayoutDirty(content,false);
    LayoutContainer(content,style_sheet,bounds.width,bounds.height,true);
    scroll_y = content.scroll.y;
}

void UIVirtualList::Update(bool inherited_disabled) {
    const bool blocked = disabled || inherited_disabled;
    const Vector2 mouse = GetUIMousePosition();
    const bool hovered = !blocked && CheckCollisionPointRec(mouse,rect.transform);
    hovered_index = -1;

    if(UsesTemplate()) {
        if(
            hovered &&
            !CSS_UI_POINTER_INPUT_CONSUMED &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        ) {
            focused_ = true;
            UpdatePointerFocus(content);
        }

        if(focused_ && IsKeyPressed(KEY_TAB)) {
            AdvanceKeyboardFocus(content);
        }

        const bool pointer_consumed_before = CSS_UI_POINTER_INPUT_CONSUMED;
        const bool continuing_pointer_sequence =
            !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            (
                IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
            );

        // Overscan rows intentionally exist outside the list viewport. Block new
        // pointer hits there, but still let an already-started drag receive its
        // down/release frames when the cursor leaves the viewport.
        if(!hovered && !continuing_pointer_sequence) {
            CSS_UI_POINTER_INPUT_CONSUMED = true;
        }

        const float previous_scroll = content.scroll.y;
        UpdateElements(content,false,blocked);
        scroll_y = content.scroll.y;
        CSS_UI_POINTER_INPUT_CONSUMED = pointer_consumed_before;

        if(std::abs(scroll_y-previous_scroll) > kEpsilon) {
            CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
        }

        if(hovered) {
            const int index = static_cast<int>(std::floor(
                (mouse.y-rect.transform.y+scroll_y)/item_height
            ));
            if(index >= 0 && index < static_cast<int>(ItemCount())) {
                hovered_index = index;
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    selected_index = index;
                    if(on_select) on_select(*this,static_cast<std::size_t>(index));
                }
            }
        }
        return;
    }

    if(!hovered) return;

    const float maximum = std::max(
        0.0f,
        item_height*static_cast<float>(items.size())-rect.transform.height
    );
    const float wheel = GetMouseWheelMove();
    if(std::abs(wheel) > kEpsilon) {
        scroll_y = std::clamp(
            scroll_y-wheel*item_height*3.0f,
            0.0f,
            maximum
        );
    }

    const int index = static_cast<int>(std::floor(
        (mouse.y-rect.transform.y+scroll_y)/item_height
    ));
    if(index >= 0 && index < static_cast<int>(items.size())) {
        hovered_index = index;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_index = index;
            focused_ = true;
            if(on_select) on_select(*this,static_cast<std::size_t>(index));
        }
    }
}

void UIVirtualList::Draw(const StyleSheet& style_sheet) {
    ComputedStyle draw_style = ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::VirtualList,style.id,
        rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style = ResolveAnimatedStyle(
        rect,style_sheet,std::move(draw_style),"virtual-list"
    );
    DrawBasicElementBox(rect.transform,draw_style);

    if(UsesTemplate()) {
        DrawElementsClipped(content,style_sheet,rect.transform);
        return;
    }

    Font font = ResolveUIFont(draw_style.font);
    const UIVisibleRange range = VisibleItems();
    if(range.empty) return;
    for(std::size_t index=range.first; index<range.last; ++index) {
        Rectangle item_rectangle = {
            rect.transform.x,
            rect.transform.y+static_cast<float>(index)*item_height-scroll_y,
            rect.transform.width,
            item_height
        };
        if(item_rectangle.y+item_rectangle.height < rect.transform.y ||
           item_rectangle.y > rect.transform.y+rect.transform.height) continue;
        const bool selected = selected_index == static_cast<int>(index);
        if(draw_item) draw_item(index,item_rectangle,items[index],selected);
        else {
            Color background = selected
                ? MixColors(draw_style.background_color,draw_style.text_color,0.18f)
                : (index%2 ? DarkenColor(draw_style.background_color,0.035f) : draw_style.background_color);
            if(hovered_index == static_cast<int>(index)) background = LightenColor(background,0.06f);
            DrawRectangleRec(item_rectangle,background);
            DrawUIText(
                font,items[index].c_str(),
                {item_rectangle.x+draw_style.padding.left,
                 item_rectangle.y+(item_height-draw_style.font_size)*0.5f},
                draw_style.font_size,1.0f,draw_style.text_color
            );
        }
    }
}

UIElementBase* UIVirtualList::FindNestedElement(const std::string& key) {
    return UsesTemplate() ? FindUIElementByKey(content,key) : nullptr;
}

const UIElementBase* UIVirtualList::FindNestedElement(const std::string& key) const {
    return UsesTemplate() ? FindUIElementByKey(content,key) : nullptr;
}

void UIVirtualList::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old = dynamic_cast<UIVirtualList*>(&previous);
    if(!old) return;
    scroll_y = old->scroll_y;
    focused_ = old->focused_;
    if(options.preserve_selection) selected_index = old->selected_index;
    // The new RXML builder owns fresh scopes/bindings. Rebuild the visible
    // content on the next layout instead of retaining old callback closures.
    built_range_valid_ = false;
}

// -----------------------------------------------------------------------------
// Docking
// -----------------------------------------------------------------------------

UIDockSpace::UIDockSpace() {
    root = std::make_shared<UIDockNode>();
    root->id = next_node_id_++;
    style.display = DisplayType::Block;
    style.overflow = OverflowType::Hidden;
}

UIDockSpace::~UIDockSpace() {
    if(persist_layout && save_layout_on_exit && layout_dirty_) {
        SaveLayoutFile();
    }
}

bool UIDockSpace::SaveLayoutFile() const {
    if(layout_file.empty()) return false;
    const std::filesystem::path path(layout_file);
    std::error_code error;
    if(path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(),error);
        if(error) return false;
    }
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    if(!stream) return false;
    std::string safe_id = layout_id;
    std::replace(safe_id.begin(),safe_id.end(),'\n',' ');
    std::replace(safe_id.begin(),safe_id.end(),'\r',' ');
    stream << "RUI_DOCK_LAYOUT_V1\n"
           << "id=" << safe_id << "\n"
           << "layout=" << SerializeLayout() << "\n";
    return static_cast<bool>(stream);
}

bool UIDockSpace::LoadLayoutFile() {
    if(layout_file.empty()) return false;
    std::ifstream stream(layout_file,std::ios::binary);
    if(!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    std::string source = buffer.str();
    std::string serialized = source;
    if(source.rfind("RUI_DOCK_LAYOUT_V1",0) == 0) {
        std::istringstream lines(source);
        std::string line;
        std::string file_id;
        serialized.clear();
        while(std::getline(lines,line)) {
            if(line.rfind("id=",0) == 0) file_id = line.substr(3);
            else if(line.rfind("layout=",0) == 0) serialized = line.substr(7);
        }
        if(!file_id.empty() && file_id != layout_id) return false;
    }
    const bool loaded = !serialized.empty() && DeserializeLayout(serialized);
    if(loaded) layout_dirty_ = false;
    return loaded;
}

void UIDockSpace::NotifyLayoutChanged() {
    layout_dirty_ = true;
    if(persist_layout && save_layout_on_change) SaveLayoutFile();
    if(on_layout_changed) on_layout_changed(*this);
}

UIDockPanel* UIDockSpace::FindPanel(const std::string& panel_id) {
    auto found = std::find_if(panels.begin(),panels.end(),[&](const UIDockPanel& panel) { return panel.id == panel_id; });
    return found == panels.end() ? nullptr : &*found;
}
const UIDockPanel* UIDockSpace::FindPanel(const std::string& panel_id) const {
    auto found = std::find_if(panels.begin(),panels.end(),[&](const UIDockPanel& panel) { return panel.id == panel_id; });
    return found == panels.end() ? nullptr : &*found;
}
UIDockNode* UIDockSpace::FindNode(std::uint64_t id) {
    UIDockNode* result = nullptr;
    VisitDockNodes(root,[&](UIDockNode& node) { if(node.id == id) result = &node; });
    return result;
}
const UIDockNode* UIDockSpace::FindNode(std::uint64_t id) const {
    const UIDockNode* result = nullptr;
    VisitDockNodesConst(root,[&](const UIDockNode& node) { if(node.id == id) result = &node; });
    return result;
}

UIDockPanel& UIDockSpace::AddPanel(UIDockPanel panel,std::uint64_t leaf) {
    if(panel.id.empty()) panel.id = "panel-"+std::to_string(panels.size()+1);
    if(panel.title.empty()) panel.title = panel.id;
    if(FindPanel(panel.id)) {
        *FindPanel(panel.id) = std::move(panel);
        return *FindPanel(panel.id);
    }
    panels.push_back(std::move(panel));
    UIDockNode* target = FindNode(leaf ? leaf : root->id);
    if(target == nullptr || target->type != UIDockNodeType::Leaf) target = FirstDockLeaf(root);
    if(target == nullptr) {
        root = std::make_shared<UIDockNode>();
        root->id = next_node_id_++;
        target = root.get();
    }
    target->tabs.push_back(panels.back().id);
    target->active_tab = static_cast<int>(target->tabs.size())-1;
    return panels.back();
}

bool UIDockSpace::RemovePanel(const std::string& panel_id) {
    const auto before = panels.size();
    panels.erase(std::remove_if(panels.begin(),panels.end(),[&](const UIDockPanel& panel) { return panel.id == panel_id; }),panels.end());
    RemoveDockTab(root,panel_id);
    NormalizeLayout();
    const bool removed = before != panels.size();
    if(removed) NotifyLayoutChanged();
    return removed;
}

bool UIDockSpace::DockPanel(const std::string& panel_id,std::uint64_t target_node,UIDockDropZone zone) {
    UIDockPanel* panel = FindPanel(panel_id);
    UIDockNode* target = FindNode(target_node);
    if(panel == nullptr || target == nullptr || target->type != UIDockNodeType::Leaf || zone == UIDockDropZone::None) return false;
    RemoveDockTab(root,panel_id);
    panel->floating = false;
    if(zone == UIDockDropZone::Center) {
        target->tabs.push_back(panel_id);
        target->active_tab = static_cast<int>(target->tabs.size())-1;
    }
    else {
        auto old_leaf = std::make_shared<UIDockNode>(*target);
        auto new_leaf = std::make_shared<UIDockNode>();
        new_leaf->id = next_node_id_++;
        new_leaf->tabs.push_back(panel_id);
        target->type = UIDockNodeType::Split;
        target->axis = (zone == UIDockDropZone::Left || zone == UIDockDropZone::Right)
            ? UIDockAxis::Horizontal : UIDockAxis::Vertical;
        target->ratio = 0.5f;
        target->tabs.clear();
        target->active_tab = 0;
        const bool new_first = zone == UIDockDropZone::Left || zone == UIDockDropZone::Top;
        target->first = new_first ? new_leaf : old_leaf;
        target->second = new_first ? old_leaf : new_leaf;
    }
    NormalizeLayout();
    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
    NotifyLayoutChanged();
    return true;
}

bool UIDockSpace::UndockPanel(const std::string& panel_id,Rectangle floating_rectangle) {
    UIDockPanel* panel = FindPanel(panel_id);
    if(panel == nullptr) return false;
    RemoveDockTab(root,panel_id);
    panel->floating = true;
    panel->floating_rectangle = floating_rectangle;
    NormalizeLayout();
    NotifyLayoutChanged();
    return true;
}

bool UIDockSpace::SelectPanel(const std::string& panel_id) {
    bool selected = false;
    VisitDockNodes(root,[&](UIDockNode& node) {
        auto found = std::find(node.tabs.begin(),node.tabs.end(),panel_id);
        if(found != node.tabs.end()) {
            node.active_tab = static_cast<int>(std::distance(node.tabs.begin(),found));
            selected = true;
        }
    });
    if(selected) NotifyLayoutChanged();
    return selected;
}

void UIDockSpace::NormalizeLayout() {
    root = CollapseDockNode(root);
    if(!root) {
        root = std::make_shared<UIDockNode>();
        root->id = next_node_id_++;
    }
    std::set<std::string> seen;
    VisitDockNodes(root,[&](UIDockNode& node) {
        if(node.type != UIDockNodeType::Leaf) return;
        node.tabs.erase(std::remove_if(node.tabs.begin(),node.tabs.end(),[&](const std::string& id) {
            if(seen.count(id) || FindPanel(id) == nullptr) return true;
            seen.insert(id);
            return false;
        }),node.tabs.end());
        node.active_tab = node.tabs.empty() ? 0 : std::clamp(node.active_tab,0,static_cast<int>(node.tabs.size())-1);
    });
    UIDockNode* fallback = FirstDockLeaf(root);
    for(UIDockPanel& panel : panels) {
        if(!panel.floating && !seen.count(panel.id) && fallback) fallback->tabs.push_back(panel.id);
    }
}

Vector2 UIDockSpace::Measure(const StyleSheet&,float available_width,float available_height) {
    return {available_width,available_height};
}
void UIDockSpace::Layout(const StyleSheet& style_sheet,Rectangle bounds) {
    rect.transform = bounds;
    tab_height = style.tab_height > 0.0f ? style.tab_height : tab_height;
    splitter_size = style.splitter_size > 0.0f ? style.splitter_size : splitter_size;
    LayoutDockNode(*this,root,bounds,style_sheet);
    for(UIDockPanel& panel : panels) {
        if(!panel.floating || !panel.open) continue;
        Rectangle body = {
            panel.floating_rectangle.x,
            panel.floating_rectangle.y+tab_height,
            panel.floating_rectangle.width,
            std::max(0.0f,panel.floating_rectangle.height-tab_height)
        };
        panel.content.rect.transform = body;
        MarkLayoutDirty(panel.content,false);
        LayoutContainer(panel.content,style_sheet,body.width,body.height,true);
    }
}

void UIDockSpace::Update(bool inherited_disabled) {
    if(inherited_disabled) return;

    // An overlay can own pointer input while still living inside a dock panel.
    // ComboBox is the important case: the popup capture intentionally prevents
    // click-through, but the panel subtree must keep updating so the owning combo
    // can select an item, dismiss on an outside click, or process Escape.
    //
    // While captured, skip DockSpace's own tabs/splitters/floating-window chrome
    // and update only panel contents. CSS_UI_POINTER_INPUT_CONSUMED remains set,
    // so UpdateElements blocks every underlying control except g_open_combo.
    if(CSS_UI_POINTER_INPUT_CONSUMED) {
        for(UIDockPanel& panel : panels) {
            if(panel.floating && panel.open) {
                UpdateElements(panel.content,false,false);
            }
        }

        VisitDockNodes(root,[&](UIDockNode& node) {
            if(node.type != UIDockNodeType::Leaf || node.tabs.empty()) return;
            const int active = std::clamp(
                node.active_tab,
                0,
                static_cast<int>(node.tabs.size())-1
            );
            UIDockPanel* panel = FindPanel(
                node.tabs[static_cast<std::size_t>(active)]
            );
            if(panel && panel->open && !panel->floating) {
                UpdateElements(panel->content,false,false);
            }
        });
        return;
    }

    const Vector2 mouse = GetUIMousePosition();
    UIDragDropContext& drag = GetUIDragDropContext();

    VisitDockNodes(root,[&](UIDockNode& node) {
        if(node.type == UIDockNodeType::Split) {
            Rectangle splitter = DockSplitterRectangle(*this,node);
            if(CheckCollisionPointRec(mouse,splitter) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                resizing_node_ = node.id;
                resize_start_ratio_ = node.ratio;
                resize_origin_ = mouse;
            }
            if(resizing_node_ == node.id) {
                if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    const float available = node.axis == UIDockAxis::Horizontal
                        ? std::max(1.0f,node.rectangle.width-splitter_size)
                        : std::max(1.0f,node.rectangle.height-splitter_size);
                    const float delta = node.axis == UIDockAxis::Horizontal
                        ? mouse.x-resize_origin_.x : mouse.y-resize_origin_.y;
                    node.ratio = std::clamp(resize_start_ratio_+delta/available,0.05f,0.95f);
                    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                }
                else {
                    resizing_node_ = 0;
                    NotifyLayoutChanged();
                }
            }
            return;
        }

        float x = node.rectangle.x;
        for(std::size_t index=0; index<node.tabs.size(); ++index) {
            UIDockPanel* panel = FindPanel(node.tabs[index]);
            if(panel == nullptr || !panel->open || panel->floating) continue;
            Font font = ResolveUIFont(style.font);
            float width = std::max(70.0f,MeasureTextCached(font,panel->title.c_str(),style.font_size,1.0f).x+(panel->closable ? 42.0f : 24.0f));
            Rectangle tab = {x,node.rectangle.y,width,tab_height};
            if(CheckCollisionPointRec(mouse,tab) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const Rectangle close = {tab.x+tab.width-20.0f,tab.y+4.0f,16.0f,tab.height-8.0f};
                if(panel->closable && CheckCollisionPointRec(mouse,close)) {
                    panel->open = false;
                    RemoveDockTab(root,panel->id);
                    NormalizeLayout();
                    CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
                    NotifyLayoutChanged();
                }
                else {
                    if(node.active_tab != static_cast<int>(index)) {
                        node.active_tab = static_cast<int>(index);
                        NotifyLayoutChanged();
                    }
                    pressed_panel_ = panel->id;
                    press_origin_ = mouse;
                }
            }
            x += width;
        }

        if(!pressed_panel_.empty() && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !drag.Dragging()) {
            const float dx = mouse.x-press_origin_.x;
            const float dy = mouse.y-press_origin_.y;
            if(dx*dx+dy*dy > 16.0f) {
                UIDragPayload payload;
                payload.type = "rui/dock-panel";
                payload.text = pressed_panel_;
                payload.value = pressed_panel_;
                dragging_panel = pressed_panel_;
                const std::weak_ptr<void> lifetime = callback_lifetime.Token();
                drag.BeginDrag("dock:"+pressed_panel_,std::move(payload),mouse,[this,lifetime](bool accepted) {
                    if(lifetime.expired()) return;
                    if(!accepted && !dragging_panel.empty()) {
                        UndockPanel(
                            dragging_panel,
                            {GetUIMousePosition().x-80.0f,GetUIMousePosition().y-14.0f,420.0f,320.0f}
                        );
                    }
                    dragging_panel.clear();
                });
                pressed_panel_.clear();
            }
        }
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) pressed_panel_.clear();

        UIDropTargetBinding target;
        target.accepts = [](const UIDragPayload& payload) { return payload.type == "rui/dock-panel"; };
        const UIDockDropZone zone = DockZoneForPoint(node.rectangle,mouse);
        const std::weak_ptr<void> dock_lifetime = callback_lifetime.Token();
        target.lifetime = dock_lifetime;
        target.lifetime_tracked = true;
        target.on_preview = [this,&node,zone,dock_lifetime](const UIDragPayload&) {
            if(dock_lifetime.expired()) return;
            preview_node = node.id;
            preview_zone = zone;
        };
        target.on_drop = [this,&node,zone,dock_lifetime](const UIDragPayload& payload) {
            if(dock_lifetime.expired()) return;
            const std::string* panel_id = payload.Get<std::string>();
            if(panel_id) DockPanel(*panel_id,node.id,zone);
            preview_node = 0;
            preview_zone = UIDockDropZone::None;
        };
        drag.ConsiderTarget("dock-node:"+std::to_string(node.id),node.rectangle,target);
    });

    for(UIDockPanel& panel : panels) {
        if(!panel.floating || !panel.open) continue;
        Rectangle header = {
            panel.floating_rectangle.x,panel.floating_rectangle.y,
            panel.floating_rectangle.width,tab_height
        };
        const Rectangle close = {
            header.x+header.width-22.0f,header.y+4.0f,18.0f,std::max(1.0f,header.height-8.0f)
        };
        const Rectangle resize = {
            panel.floating_rectangle.x+panel.floating_rectangle.width-12.0f,
            panel.floating_rectangle.y+panel.floating_rectangle.height-12.0f,12.0f,12.0f
        };
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if(panel.closable && CheckCollisionPointRec(mouse,close)) {
                panel.open = false;
                NotifyLayoutChanged();
                continue;
            }
            if(CheckCollisionPointRec(mouse,resize)) {
                panel.resizing = true;
                panel.resize_anchor = mouse;
                panel.resize_start = panel.floating_rectangle;
            }
            else if(CheckCollisionPointRec(mouse,header)) {
                panel.dragging = true;
                panel.drag_anchor = {mouse.x-panel.floating_rectangle.x,mouse.y-panel.floating_rectangle.y};
            }
        }
        if(panel.dragging) {
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                panel.floating_rectangle.x = mouse.x-panel.drag_anchor.x;
                panel.floating_rectangle.y = mouse.y-panel.drag_anchor.y;
                CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
            }
            else {
                panel.dragging = false;
                NotifyLayoutChanged();
            }
        }
        if(panel.resizing) {
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                panel.floating_rectangle.width = std::max(minimum_pane_size,panel.resize_start.width+mouse.x-panel.resize_anchor.x);
                panel.floating_rectangle.height = std::max(minimum_pane_size,panel.resize_start.height+mouse.y-panel.resize_anchor.y);
                CSS_UI_LAYOUT_CHANGE_REQUESTED = true;
            }
            else {
                panel.resizing = false;
                NotifyLayoutChanged();
            }
        }
        UpdateElements(panel.content,false,false);
    }

    VisitDockNodes(root,[&](UIDockNode& node) {
        if(node.type != UIDockNodeType::Leaf || node.tabs.empty()) return;
        UIDockPanel* panel = FindPanel(node.tabs[static_cast<std::size_t>(std::clamp(node.active_tab,0,static_cast<int>(node.tabs.size())-1))]);
        if(panel && panel->open && !panel->floating) UpdateElements(panel->content,false,false);
    });
}

void UIDockSpace::Draw(const StyleSheet& style_sheet) {
    ComputedStyle draw_style = ResolveStyleWithPseudoStates(
        style_sheet,SelectorType::DockSpace,style.id,rect.pseudo_states.ActiveNamesView(),style
    );
    draw_style = ResolveAnimatedStyle(rect,style_sheet,std::move(draw_style),"dockspace");
    DrawBasicElementBox(rect.transform,draw_style);
    DrawDockNode(*this,root,style_sheet);
    for(UIDockPanel& panel : panels) {
        if(!panel.floating || !panel.open) continue;
        DrawStyledTextBox(panel.floating_rectangle,"",draw_style,TextAlign::Left);
        DrawRectangleRec(
            {panel.floating_rectangle.x,panel.floating_rectangle.y,panel.floating_rectangle.width,tab_height},
            DarkenColor(draw_style.background_color,0.08f)
        );
        Font font = ResolveUIFont(draw_style.font);
        DrawUIText(
            font,panel.title.c_str(),
            {panel.floating_rectangle.x+8.0f,panel.floating_rectangle.y+(tab_height-draw_style.font_size)*0.5f},
            draw_style.font_size,1.0f,draw_style.text_color
        );
        if(panel.closable) {
            DrawUIText(
                font,"x",
                {panel.floating_rectangle.x+panel.floating_rectangle.width-18.0f,panel.floating_rectangle.y+4.0f},
                draw_style.font_size,1.0f,draw_style.text_color
            );
        }
        DrawRectangleRec(
            {panel.floating_rectangle.x+panel.floating_rectangle.width-10.0f,
             panel.floating_rectangle.y+panel.floating_rectangle.height-10.0f,8.0f,8.0f},
            draw_style.border_color
        );
        DrawElementsClipped(
            panel.content,style_sheet,
            {panel.floating_rectangle.x,panel.floating_rectangle.y+tab_height,
             panel.floating_rectangle.width,std::max(0.0f,panel.floating_rectangle.height-tab_height)}
        );
    }
    if(preview_node != 0 && preview_zone != UIDockDropZone::None) {
        const UIDockNode* node = FindNode(preview_node);
        if(node) {
            Rectangle preview = DockPreviewRectangle(node->rectangle,preview_zone);
            DrawRectangleRec(preview,Color{100,150,255,72});
            DrawRectangleLinesEx(preview,2.0f,Color{130,180,255,220});
        }
    }
}

UIElementBase* UIDockSpace::FindNestedElement(const std::string& key) {
    for(UIDockPanel& panel : panels) {
        if(panel.id == key || "#"+panel.id == key) return &panel.content.rect;
        if(UIElementBase* found = FindUIElementByKey(panel.content,key)) return found;
    }
    return nullptr;
}
const UIElementBase* UIDockSpace::FindNestedElement(const std::string& key) const {
    for(const UIDockPanel& panel : panels) {
        if(panel.id == key || "#"+panel.id == key) return &panel.content.rect;
        if(const UIElementBase* found = FindUIElementByKey(panel.content,key)) return found;
    }
    return nullptr;
}

void UIDockSpace::ReconcileFrom(IUICustomElement& previous,const UIReconcileOptions& options) {
    auto* old = dynamic_cast<UIDockSpace*>(&previous);
    if(!old) return;
    root = old->root;
    next_node_id_ = old->next_node_id_;

    // Rebuilds can happen immediately after the mouse press that begins a
    // splitter/panel resize. Preserve the complete interaction transaction so
    // reconciliation does not cancel resizing on the following frame.
    pressed_panel_ = old->pressed_panel_;
    press_origin_ = old->press_origin_;
    resizing_node_ = old->resizing_node_;
    resize_start_ratio_ = old->resize_start_ratio_;
    resize_origin_ = old->resize_origin_;
    dragging_panel = old->dragging_panel;
    preview_zone = old->preview_zone;
    preview_node = old->preview_node;

    layout_dirty_ = old->layout_dirty_;
    old->layout_dirty_ = false;
    for(UIDockPanel& panel : panels) {
        UIDockPanel* old_panel = old->FindPanel(panel.id);
        if(!old_panel) continue;
        panel.open = old_panel->open;
        panel.floating = old_panel->floating;
        panel.floating_rectangle = old_panel->floating_rectangle;
        panel.dragging = old_panel->dragging;
        panel.resizing = old_panel->resizing;
        panel.drag_anchor = old_panel->drag_anchor;
        panel.resize_anchor = old_panel->resize_anchor;
        panel.resize_start = old_panel->resize_start;
        ReconcileUI(old_panel->content,std::move(panel.content),options);
        panel.content = std::move(old_panel->content);
    }
    NormalizeLayout();
}

std::string UIDockSpace::SerializeLayout() const {
    std::ostringstream output;
    SerializeDockNode(root,output);
    output << ";F(";
    bool first = true;
    for(const UIDockPanel& panel : panels) {
        if(!panel.floating) continue;
        if(!first) output << '|';
        first = false;
        output << EscapeSerialized(panel.id) << ','
               << panel.floating_rectangle.x << ',' << panel.floating_rectangle.y << ','
               << panel.floating_rectangle.width << ',' << panel.floating_rectangle.height;
    }
    output << ')';
    return output.str();
}

bool UIDockSpace::DeserializeLayout(const std::string& text) {
    const std::size_t floating = text.find(";F(");
    const std::string tree = floating == std::string::npos ? text : text.substr(0,floating);
    DockLayoutParser parser(tree,next_node_id_);
    std::shared_ptr<UIDockNode> parsed = parser.Parse();
    if(!parsed) return false;
    root = std::move(parsed);
    for(UIDockPanel& panel : panels) panel.floating = false;
    if(floating != std::string::npos && text.back() == ')') {
        const std::string floating_text = text.substr(floating+3,text.size()-(floating+4));
        for(const std::string& entry : SplitSerialized(floating_text,'|')) {
            const std::vector<std::string> fields = SplitSerialized(entry,',');
            if(fields.size() != 5) continue;
            UIDockPanel* panel = FindPanel(fields[0]);
            if(!panel) continue;
            panel->floating = true;
            panel->floating_rectangle = {
                std::strtof(fields[1].c_str(),nullptr),std::strtof(fields[2].c_str(),nullptr),
                std::strtof(fields[3].c_str(),nullptr),std::strtof(fields[4].c_str(),nullptr)
            };
            RemoveDockTab(root,panel->id);
        }
    }
    NormalizeLayout();
    return true;
}