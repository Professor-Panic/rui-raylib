#include "css_emulator.h"

void ApplyRuntimeOverrides(
    UIContainer& root,
    UIRuntimeLayer& layer
) {
    VisitRuntimeElements(root,[&](UIRuntimeTarget& target) {
        target.rect->runtime_offset = {0.0f,0.0f};
        target.rect->runtime_size = {0.0f,0.0f};
        target.rect->runtime_size_override = false;

        auto found = layer.overrides.find(target.key);
        if(found == layer.overrides.end()) return;
        target.rect->runtime_offset = found->second.offset;
        target.rect->runtime_size = found->second.size;
        target.rect->runtime_size_override = found->second.has_size;
    });
    MarkLayoutDirty(root,true);
}

bool LoadRuntimeLayout(
    UIRuntimeLayer& layer,
    UIContainer& root,
    const std::string& path ) {
    if(!path.empty()) layer.config_path = path;
    std::ifstream file(layer.config_path);
    if(!file.is_open()) return false;

    std::string header;
    std::getline(file,header);
    if(header != "css-ui-runtime-v1") {
        TraceLog(LOG_WARNING,"Unsupported runtime layout cfg: %s",layer.config_path.c_str());
        return false;
    }

    std::unordered_map<std::string,UIRuntimeOverride> loaded;
    std::string key;
    UIRuntimeOverride value;
    int has_size = 0;
    while(file >> std::quoted(key)
        >> value.offset.x >> value.offset.y
        >> value.size.x >> value.size.y >> has_size
    ) {
        value.has_size = has_size != 0;
        loaded[key] = value;
    }

    layer.overrides = std::move(loaded);
    layer.dirty = false;
    ApplyRuntimeOverrides(root,layer);
    return true;
}

bool SaveRuntimeLayout(const UIRuntimeLayer& layer) {
    if(layer.config_path.empty()) return false;
    std::ofstream file(layer.config_path,std::ios::trunc);
    if(!file.is_open()) {
        TraceLog(LOG_WARNING,"Could not save runtime layout cfg: %s",layer.config_path.c_str());
        return false;
    }

    file << "css-ui-runtime-v1\n" << std::setprecision(9);
    for(const auto& [key,value] : layer.overrides) {
        file << std::quoted(key) << ' '
             << value.offset.x << ' ' << value.offset.y << ' '
             << value.size.x << ' ' << value.size.y << ' '
             << (value.has_size ? 1 : 0) << '\n';
    }
    return static_cast<bool>(file);
}

void ResetRuntimeLayout(
    UIRuntimeLayer& layer,
    UIContainer& root
) {
    layer.overrides.clear();
    layer.selected_key.clear();
    layer.active_key.clear();
    layer.interaction = UIRuntimeInteraction::None;
    layer.dirty = true;
    ApplyRuntimeOverrides(root,layer);
    if(layer.auto_save) SaveRuntimeLayout(layer);
}

std::size_t ComputeContainerLayoutSignature(
    const UIContainer& container
) {
    std::size_t seed = HashRectangle(container.rect.transform);
    HashCombine(seed,HashComputedStyle(container.style));
    HashCombine(seed,static_cast<std::size_t>(container.direction));
    HashCombine(seed,HashFloat(container.scroll.x));
    HashCombine(seed,HashFloat(container.scroll.y));
    HashCombine(seed,std::hash<bool>{}(container.disabled));
    HashCombine(seed,container.elements.size());

    for(const UIElement& element : container.elements) {
        HashCombine(seed,element.index());

        std::visit(
            [&](const auto& widget) {
                using T = std::decay_t<decltype(widget)>;

                if constexpr(IsUICustomElementPointerV<T>) {
                    if(!widget) return;
                    HashCombine(seed,HashComputedStyle(widget->style));
                    HashCombine(seed,std::hash<std::string>{}(widget->TypeName()));
                    if(const auto* text = dynamic_cast<const UITextBox*>(widget.get())) {
                        HashCombine(seed,std::hash<std::string>{}(text->text));
                        HashCombine(seed,std::hash<bool>{}(text->wrap));
                    }
                    else if(const auto* table = dynamic_cast<const UITable*>(widget.get())) {
                        HashCombine(seed,table->columns.size());
                        HashCombine(seed,table->rows.size());
                        HashCombine(seed,HashFloat(table->scroll_y));
                    }
                    else if(const auto* list = dynamic_cast<const UIVirtualList*>(widget.get())) {
                        HashCombine(seed,list->items.size());
                        HashCombine(seed,HashFloat(list->scroll_y));
                    }
                    else if(const auto* dock = dynamic_cast<const UIDockSpace*>(widget.get())) {
                        HashCombine(seed,std::hash<std::string>{}(dock->SerializeLayout()));
                        for(const UIDockPanel& panel : dock->panels) {
                            HashCombine(seed,ComputeContainerLayoutSignature(panel.content));
                        }
                    }
                }
                else if constexpr(IsUICompositePointerV<T>) {
                    if(!widget) return;
                    HashCombine(seed,HashComputedStyle(widget->style));

                    if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                        HashCombine(seed,std::hash<int>{}(widget->selected_index));
                        HashCombine(seed,widget->tabs.size());
                        for(const UITab& tab : widget->tabs) {
                            HashCombine(seed,std::hash<std::string>{}(tab.id));
                            HashCombine(seed,std::hash<std::string>{}(tab.title));
                            HashCombine(seed,ComputeContainerLayoutSignature(tab.content));
                        }
                    }
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                        HashCombine(seed,ComputeContainerLayoutSignature(*widget));
                    }
                    else {
                        if constexpr(std::is_same_v<T,std::shared_ptr<UICollapsible>>) {
                            HashCombine(seed,std::hash<std::string>{}(widget->text));
                            HashCombine(seed,std::hash<bool>{}(widget->open));
                        }
                        else if constexpr(std::is_same_v<T,std::shared_ptr<UITreeNode>>) {
                            HashCombine(seed,std::hash<std::string>{}(widget->text));
                            HashCombine(seed,std::hash<bool>{}(widget->open));
                            HashCombine(seed,std::hash<bool>{}(widget->selected));
                        }
                        else if constexpr(std::is_same_v<T,std::shared_ptr<UIMenu>>) {
                            HashCombine(seed,std::hash<std::string>{}(widget->text));
                            HashCombine(seed,std::hash<bool>{}(widget->open));
                        }
                        else if constexpr(std::is_same_v<T,std::shared_ptr<UIPopup>>) {
                            HashCombine(seed,std::hash<std::string>{}(widget->title));
                            HashCombine(seed,std::hash<bool>{}(widget->open));
                            HashCombine(seed,std::hash<bool>{}(widget->modal));
                        }
                        else if constexpr(std::is_same_v<T,std::shared_ptr<UIWindow>>) {
                            HashCombine(seed,std::hash<std::string>{}(widget->title));
                            HashCombine(seed,std::hash<bool>{}(widget->open));
                            HashCombine(seed,std::hash<bool>{}(widget->collapsed));
                        }
                        HashCombine(seed,ComputeContainerLayoutSignature(widget->content));
                    }
                }
                else {
                    HashCombine(seed,HashComputedStyle(widget.style));

                    if constexpr(std::is_same_v<T,UILabel>) {
                        HashCombine(
                            seed,
                            std::hash<std::string>{}(widget.text)
                        );
                    }
                    else if constexpr(
                        std::is_same_v<T,UIButton> ||
                        std::is_same_v<T,UISelectable>
                    ) {
                        HashCombine(seed,std::hash<std::string>{}(widget.text));
                    }
                    else if constexpr(std::is_same_v<T,UIComboBox>) {
                        HashCombine(seed,widget.items.size());
                        HashCombine(seed,std::hash<int>{}(widget.selected_index));
                        HashCombine(seed,std::hash<bool>{}(widget.open));
                    }
                }
            },
            element
        );
    }

    return seed;
}

void MarkLayoutDirty(
    UIContainer& container,
    bool recursive
) {
    container.layout_dirty = true;

    if(!recursive) {
        return;
    }

    for(UIElement& element : container.elements) {
        std::visit(
            [&](auto& widget) {
                using T = std::decay_t<decltype(widget)>;
                if constexpr(IsUICustomElementPointerV<T>) {
                    if(!widget) return;
                    if(auto* dock = dynamic_cast<UIDockSpace*>(widget.get())) {
                        for(UIDockPanel& panel : dock->panels) MarkLayoutDirty(panel.content,true);
                    }
                }
                else if constexpr(IsUICompositePointerV<T>) {
                    if(!widget) return;

                    if constexpr(std::is_same_v<T,std::shared_ptr<UITabBar>>) {
                        for(UITab& tab : widget->tabs) MarkLayoutDirty(tab.content,true);
                    }
                    else if constexpr(std::is_same_v<T,std::shared_ptr<UIContainer>>) {
                        MarkLayoutDirty(*widget,true);
                    }
                    else {
                        MarkLayoutDirty(widget->content,true);
                    }
                }
            },
            element
        );
    }
}

