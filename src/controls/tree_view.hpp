#ifndef TREEVIEW_HPP
    #define TREEVIEW_HPP

    #include <algorithm>

    #include "widget.hpp"
    #include "scrollable.hpp"
    #include "spacer.hpp"
    #include "../application.hpp"
    #include "cell_renderer.hpp"
    #include "../core/array_list.hpp"

    /// Meant to represent a single row within the TreeView Widget.
    /// Note that a TreeNode can have children and so its not exactly
    /// equivalent to a single row.
    template <typename T> class TreeNode {
        public:
            std::vector<Drawable*> columns;

            /// This is a pointer to backend data.
            /// This can be used to display one value but perform
            /// the sorting process on something completely different.
            T *hidden = nullptr;

            TreeNode<T> *parent = nullptr;
            i32 parent_index = -1;
            std::vector<TreeNode<T>*> children;
            bool is_collapsed = false;
            i32 max_cell_height = 0;
            i32 depth = 0;

            /// Internal data that stores the vertical position and height of the TreeNode.
            BinarySearchData bs_data;

            TreeNode(std::vector<Drawable*> columns, T *hidden) {
                this->columns = columns;
                this->hidden = hidden;
            }

            ~TreeNode() {
                for (TreeNode<T> *child : children) {
                    delete child;
                }
                for (Drawable *drawable : columns) {
                    delete drawable;
                }
                delete hidden;
            }
    };

    enum class Traversal {
        /// Continue going down as normal by traversing all nodes.
        Continue,

        /// Ends the traversal of the current node and its children early and goes to the next one on the same level.
        Next,

        /// Ends the traversal of the entire tree immediately.
        /// Note: When manually descending (NOT using forEachNode) it is your
        /// responsibility to check `early_exit` for Traversal::Break
        /// after calling descend().
        Break
    };

    /// The model for TreeNodes.
    /// Contains utility methods for traversing the model
    /// adding nodes and emptying the model.
    template <typename T> class Tree {
        public:
            std::vector<TreeNode<T>*> roots;

            Tree() {

            }

            ~Tree() {
                for (TreeNode<T> *root : roots) {
                    delete root;
                }
            }

            TreeNode<T>* append(TreeNode<T> *parent, TreeNode<T> *node) {
                if (!parent) {
                    node->parent = nullptr;
                    roots.push_back(node);
                    node->parent_index = roots.size() - 1;

                    _updateDepth(parent, node);
                    _updateBSData();

                    return node;
                } else {
                    parent->children.push_back(node);
                    node->parent_index = parent->children.size() - 1;
                    node->parent = parent;

                    _updateDepth(parent, node);
                    _updateBSData();

                    return node;
                }
            }

            TreeNode<T>* insert(TreeNode<T> *parent, u64 index, TreeNode<T> *node) {
                if (!parent) {
                    node->parent = nullptr;
                    node->depth = 1;
                    roots.insert(roots.begin() + index, node);
                    node->parent_index = index;

                    for (u64 i = index + 1; i < roots.size(); i++) {
                        roots[i]->parent_index = i;
                    }

                    _updateDepth(parent, node);
                    _updateBSData();

                    return node;
                } else {
                    parent->children.insert(parent->children.begin() + index, node);
                    node->parent_index = index;
                    node->parent = parent;

                    for (u64 i = index + 1; i < parent->children.size(); i++) {
                        parent->children[i]->parent_index = i;
                    }

                    _updateDepth(parent, node);
                    _updateBSData();

                    return node;
                }
            }

            void _updateDepth(TreeNode<T> *parent, TreeNode<T> *node) {
                i32 depth = -1;
                if (!parent) {
                    if (node->depth != 1) {
                        node->depth = 1;
                        depth = 1;
                    } else { return; }
                } else {
                    if (node->depth != parent->depth + 1) {
                        node->depth = parent->depth + 1;
                        depth = node->depth;
                    } else { return; }
                }
                forEachNode(node->children, [&](TreeNode<T> *_node) -> Traversal {
                    if (_node->parent_index == 0) {
                        depth++;
                        _node->depth = depth;
                    } else if (_node->parent_index == cast(i32, _node->parent->children.size() - 1)) {
                        _node->depth = depth;
                        depth--;
                    } else {
                        _node->depth = depth;
                    }
                    return Traversal::Continue;
                });
            }

            TreeNode<T>* remove(u64 index) {
                return remove(get(index));
            }

            // TODO we could think about finally bringing in Box<T> here and return that instead
            // and have an overload for insert / append which takes in a box, unboxes it and calls the regular version
            TreeNode<T>* remove(TreeNode<T> *node) {
                if (!node) { return nullptr; }
                if (node->parent) {
                    node->parent->children.erase(node->parent->children.begin() + node->parent_index);
                    for (i32 i = node->parent_index; i < cast(i32, node->parent->children.size()); i++) {
                        TreeNode<T> *n = node->parent->children[i];
                        n->parent_index--;
                    }
                    node->parent = nullptr;
                    node->parent_index = -1;
                } else {
                    roots.erase(roots.begin() + node->parent_index);
                    for (i32 i = node->parent_index; i < cast(i32, roots.size()); i++) {
                        TreeNode<T> *n = roots[i];
                        n->parent_index--;
                    }
                }
                _updateBSData();
                _updateDepth(node->parent, node);
                return node;
            }

            void clear() {
                for (TreeNode<T> *root : roots) {
                    delete root;
                }
                roots.clear();
            }

            TreeNode<T>* descend(Traversal &early_exit, TreeNode<T> *root, std::function<Traversal(TreeNode<T> *node)> fn = nullptr) {
                if (fn) {
                    early_exit = fn(root);
                    if (early_exit == Traversal::Break || early_exit == Traversal::Next) {
                        return root;
                    }
                }
                if (root->children.size()) {
                    TreeNode<T> *last = nullptr;
                    for (TreeNode<T> *child : root->children) {
                        last = descend(early_exit, child, fn);
                        if (early_exit == Traversal::Break) {
                            return last;
                        }
                    }
                    return last;
                } else {
                    return root;
                }
            }

            void forEachNode(std::vector<TreeNode<T>*> &roots, std::function<Traversal(TreeNode<T> *node)> fn) {
                Traversal early_exit = Traversal::Continue;
                for (TreeNode<T> *root : roots) {
                    descend(early_exit, root, fn);
                    if (early_exit == Traversal::Break) {
                        break;
                    }
                }
            }

            TreeNode<T>* get(u64 index) {
                u64 i = 0;
                TreeNode<T> *result = nullptr;
                forEachNode(roots, [&](TreeNode<T> *node) -> Traversal {
                    if (i == index) {
                        result = node;
                        return Traversal::Break;
                    }
                    i++;
                    return Traversal::Continue;
                });
                return result;
            }

            // TODO ideally this could notify the treeview widget that virtual size has changed
            // this isnt always true, often times only position would be adjusted but in the case of remove
            // the height of the tree changes as well, even if only temporarily
            // TODO also it could be more granual, we could pass in a node
            // and based on that we could ascend up to roots keeping the parent index around
            // and only update from there
            u64 _updateBSData() {
                u64 position = 0;
                forEachNode(roots, [&](TreeNode<T> *node) -> Traversal {
                    node->bs_data.position = position;
                    position += node->bs_data.length;
                    return Traversal::Continue;
                });
                return position;
            }

            TreeNode<T>* find(std::function<bool(TreeNode<T>*)> predicate) {
                TreeNode<T> *result = nullptr;
                forEachNode(roots, [&](TreeNode<T> *node) -> Traversal {
                    if (predicate(node)) {
                        result = node;
                        return Traversal::Break;
                    }
                    return Traversal::Continue;
                });
                return result;
            }
    };

    template <typename T> class TreeView;

    enum class Sort {
        None,
        Ascending,
        Descending
    };

    template <typename T> class Column : public Box {
        public:
            std::function<bool(TreeNode<T> *lhs, TreeNode<T> *rhs)> sort_fn = nullptr;

            Column(
                String text, Image *image = nullptr,
                HorizontalAlignment alignment = HorizontalAlignment::Center,
                std::function<bool(TreeNode<T> *lhs, TreeNode<T> *rhs)> sort_function = nullptr) : Box(Align::Horizontal) {
                if (alignment == HorizontalAlignment::Right) {
                    this->append(new Spacer(), Fill::Both);
                }
                Button *b = new Button(text);
                    b->setImage(image);
                if (alignment == HorizontalAlignment::Left || alignment == HorizontalAlignment::Right) {
                    this->append(b, Fill::Vertical);
                } else {
                    this->append(b, Fill::Both);
                }
                this->sort_fn = sort_function;
                if (sort_fn) {
                    if (alignment == HorizontalAlignment::Left) {
                        this->append(new Spacer(), Fill::Both);
                    }
                    IconButton *sort_icon = new IconButton((new Image(Application::get()->icons["up_arrow"]))->setMinSize(Size(12, 12)));
                        sort_icon->hide();
                    this->append(sort_icon, Fill::Vertical);
                }
                this->onMouseDown.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (event.x >= (rect.x + rect.w) - 5) {
                        m_dragging = true;
                        m_custom_width = m_custom_size ? m_custom_width : m_size.w;
                        m_custom_size = true;
                        Application::get()->setMouseCursor(Cursor::SizeWE);
                    }
                });
                this->onMouseClick.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (!m_dragging && sort_fn) {
                        if (m_sort == Sort::None) {
                            children[children.size() - 1]->show();
                            m_sort = Sort::Ascending;
                        } else if (m_sort == Sort::Ascending) {
                            IconButton *sort_icon = (IconButton*)children[children.size() - 1];
                                sort_icon->image()->flipVertically();
                            m_sort = Sort::Descending;
                        } else {
                            IconButton *sort_icon = (IconButton*)children[children.size() - 1];
                                sort_icon->image()->flipVertically();
                            m_sort = Sort::Ascending;
                        }
                        sort(m_sort);
                    } else {
                        m_dragging = false;
                        Application::get()->setMouseCursor(Cursor::Default);
                    }
                });
                this->onMouseMotion.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (m_dragging) {
                        // Ignore any right side movement if the mouse is to the left of the column's right most boundary.
                        if (!((event.x < (rect.x + rect.w)) && (event.xrel > 0))) {
                            setExpand(false);
                            setWidth(rect.w + event.xrel);
                        }
                    } else {
                        if (!isPressed()) {
                            if (event.x >= (rect.x + rect.w) - 5) {
                                Application::get()->setMouseCursor(Cursor::SizeWE);
                            } else {
                                Application::get()->setMouseCursor(Cursor::Default);
                            }
                        }
                    }
                });
                this->onMouseLeft.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (m_dragging) {
                        m_dragging = false;
                    }
                    Application::get()->setMouseCursor(Cursor::Default);
                });
            }

            ~Column() {

            }

            virtual const char* name() override {
                return "Column";
            }

            virtual void draw(DrawingContext &dc, Rect rect, i32 state) override {
                this->rect = rect;
                Color color;
                if (m_dragging) {
                    color = dc.widgetBackground(style());
                } else if (state & STATE_PRESSED && state & STATE_HOVERED) {
                    color = dc.pressedBackground(style());
                } else if (state & STATE_HOVERED) {
                    color = dc.hoveredBackground(style());
                } else {
                    color = dc.widgetBackground(style());
                }
                dc.drawBorder(rect, style(), state);

                dc.fillRect(rect, color);
                layoutChildren(dc, rect);
            }

            virtual bool isLayout() override {
                return false;
            }

            void sort(Sort sort) {
                if (sort_fn) {
                    if (sort == Sort::None) {
                        IconButton *sort_icon = (IconButton*)children[children.size() - 1];
                            sort_icon->hide();
                        if (m_sort == Sort::Descending) {
                            sort_icon->image()->flipVertically();
                        }
                        m_sort = sort;
                    } else {
                        assert(m_model && "Model cannot be null when sorting! Only sort once youve set the model.");
                        if (isSorted() == Sort::Ascending) {
                            std::sort(m_model->roots.rbegin(), m_model->roots.rend(), sort_fn);
                            m_model->forEachNode(m_model->roots, [&](TreeNode<T> *node) {
                                if (!node->children.size()) { return Traversal::Next; }
                                std::sort(node->children.rbegin(), node->children.rend(), sort_fn);
                                // TODO we could think about moving this to calculateVirtualSize
                                for (u64 i = 0; i < node->children.size(); i++) {
                                    node->children[i]->parent_index = i;
                                }
                                return Traversal::Continue;
                            });
                        } else {
                            std::sort(m_model->roots.begin(), m_model->roots.end(), sort_fn);
                            m_model->forEachNode(m_model->roots, [&](TreeNode<T> *node) {
                                if (!node->children.size()) { return Traversal::Next; }
                                std::sort(node->children.begin(), node->children.end(), sort_fn);
                                for (u64 i = 0; i < node->children.size(); i++) {
                                    node->children[i]->parent_index = i;
                                }
                                return Traversal::Continue;
                            });
                        }
                        ((TreeView<T>*)parent)->m_virtual_size_changed = true;
                        ((TreeView<T>*)parent)->sizeHint(*Application::get()->currentWindow()->dc);
                        update();
                    }
                }
            }

            Sort isSorted() {
                return m_sort;
            }

            void setModel(Tree<T> *model) {
                m_model = model;
            }

            virtual Size sizeHint(DrawingContext &dc) override {
                u32 visible = 0;
                u32 horizontal_non_expandable = 0;
                if (m_size_changed) {
                    Size size = Size();
                    for (Widget *child : children) {
                        Size s = child->sizeHint(dc);
                        size.w += s.w;
                        if (s.h > size.h) {
                            size.h = s.h;
                        }
                        visible += child->proportion();
                        if (child->fillPolicy() == Fill::Vertical || child->fillPolicy() == Fill::None) {
                            horizontal_non_expandable++;
                        }
                    }
                    m_widgets_only = size;

                    dc.sizeHintBorder(size, style());

                    m_horizontal_non_expandable = horizontal_non_expandable;
                    m_visible_children = visible;
                    m_size = size;
                    m_size_changed = false;

                    if (m_custom_size) {
                        return Size(m_custom_width, m_size.h);
                    }
                    return size;
                } else {
                    if (m_custom_size) {
                        return Size(m_custom_width, m_size.h);
                    }
                    return m_size;
                }
            }

            i32 width() {
                if (m_custom_size) {
                    return m_custom_width;
                }
                return m_size.w;
            }

            void setWidth(i32 width) {
                if (width < m_min_width) {
                    return;
                }
                m_custom_size = true;
                m_custom_width = width;
                rect.w = width;
                layout(LAYOUT_STYLE);
            }

            void setColumnStyle(Style column, Style button) {
                setStyle(column);
                for (auto child : children) {
                    child->setStyle(button);
                }
            }

            Column* setTooltip(Widget *tooltip) {
                this->tooltip = tooltip;
                return this;
            }

            void setExpand(bool expand) {
                m_expand = expand;
                update();
            }

            bool expand() {
                return m_expand;
            }

        protected:
            Sort m_sort = Sort::None;
            Tree<T> *m_model = nullptr;
            bool m_dragging = false;
            bool m_custom_size = false;
            i32 m_custom_width = 0;
            i32 m_min_width = 16;
            bool m_expand = false;
    };

    /// Describes which of the grid lines get drawn.
    enum class GridLines {
        None,
        Horizontal,
        Vertical,
        Both,
    };

    /// Describes the scrolling mode of the TreeView.
    /// Scroll means that the TreeView uses its own ScrollBars.
    /// Unroll means the TreeView contents are factored into the
    /// physical size of the Widget and stretch the height of it.
    /// Unroll also always leaves the Column heading at the top of the Widget
    /// it does not scroll them with the contents.
    /// in this mode it is the responsibility of the parent Widget to take
    /// care of any scrolling operations.
    /// This mode can be desirable when you want to have multiple TreeViews
    /// within a Scrollable without everyone of the stealing the scroll events.
    enum class Mode {
        Scroll,
        Unroll,
    };

    template <typename T> class TreeView : public Scrollable {
        public:
            struct DropAction {
                enum class Type {
                    Root  = 0b0001,
                    Child = 0b0010,
                    Above = 0b0100,
                    Below = 0b1000,
                };

                TreeNode<T> *node = nullptr;
                Type type = Type::Root;
            };

            EventListener<TreeView<T>*, TreeNode<T>*> onNodeHovered = EventListener<TreeView<T>*, TreeNode<T>*>();
            EventListener<TreeView<T>*, TreeNode<T>*> onNodeActivated = EventListener<TreeView<T>*, TreeNode<T>*>();
            EventListener<TreeView<T>*, TreeNode<T>*> onNodeSelected = EventListener<TreeView<T>*, TreeNode<T>*>();
            EventListener<TreeView<T>*, TreeNode<T>*> onNodeDeselected = EventListener<TreeView<T>*, TreeNode<T>*>();
            EventListener<TreeView<T>*, TreeNode<T>*> onNodeCollapsed = EventListener<TreeView<T>*, TreeNode<T>*>();
            EventListener<TreeView<T>*, TreeNode<T>*> onNodeExpanded = EventListener<TreeView<T>*, TreeNode<T>*>();

            TreeView(Size min_size = Size(100, 100)) : Scrollable(min_size) {
                this->onMouseMotion.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (m_event_node) {
                        if (m_hovered != m_event_node) {
                            m_hovered = m_event_node;
                            onNodeHovered.notify(this, m_event_node);
                        }
                        i32 x = inner_rect.x;
                        if (m_horizontal_scrollbar->isVisible()) {
                            x -= m_horizontal_scrollbar->m_slider->m_value * ((m_virtual_size.w) - inner_rect.w);
                        }
                        if ((m_event_node->children.size() && !m_table) &&
                            (event.x >= x + (m_event_node->depth - 1) * m_indent && event.x <= x + m_event_node->depth * m_indent)) {
                            m_tree_collapser = m_event_node;
                        } else {
                            m_tree_collapser = nullptr;
                        }
                        update();
                    }
                });
                this->onMouseDown.addEventListener([&](Widget *widget, MouseEvent event) {
                    if (m_event_node) {
                        i32 x = inner_rect.x;
                        if (m_horizontal_scrollbar->isVisible()) {
                            x -= m_horizontal_scrollbar->m_slider->m_value * ((m_virtual_size.w) - inner_rect.w);
                        }
                        if (m_event_node->children.size() &&
                            (!m_table && (event.x >= x + (m_event_node->depth - 1) * m_indent && event.x <= x + m_event_node->depth * m_indent))) {
                            if (m_event_node->is_collapsed) {
                                expand(m_event_node);
                            } else {
                                collapse(m_event_node);
                            }
                        } else {
                            if (event.click == MouseEvent::Click::Double) {
                                onNodeActivated.notify(this, m_event_node);
                            } else {
                                if (isCtrlPressed()) {
                                    if (isShiftPressed() and !m_focused.isEmpty()) {
                                        void *begin = nullptr;
                                        void *end = nullptr;
                                        void *focused = m_focused[0];
                                        if (focused == m_event_node) {
                                            forceMultiselect(m_event_node);
                                            return;
                                        }
                                        m_model->forEachNode(m_model->roots, [&](TreeNode<T> *n) -> Traversal {
                                            if (!begin) {
                                                if (n == m_event_node) {
                                                    begin = m_event_node;
                                                    end = focused;
                                                    forceMultiselect(n);
                                                } else if (n == focused) {
                                                    begin = focused;
                                                    end = m_event_node;
                                                    forceMultiselect(n);
                                                }
                                            } else {
                                                forceMultiselect(n);
                                                if (n == end) {
                                                    return Traversal::Break;
                                                }
                                            }
                                            return Traversal::Continue;
                                        });
                                    } else {
                                        multiselect(m_event_node);
                                    }
                                } else {
                                    if (isShiftPressed() and !m_focused.isEmpty()) {
                                        void *begin = nullptr;
                                        void *end = nullptr;
                                        void *focused = m_focused[0];
                                        if (focused == m_event_node) {
                                            forceMultiselect(m_event_node);
                                            return;
                                        }
                                        m_model->forEachNode(m_model->roots, [&](TreeNode<T> *n) -> Traversal {
                                            if (!begin) {
                                                if (n == m_event_node) {
                                                    begin = m_event_node;
                                                    end = focused;
                                                    forceMultiselect(n);
                                                } else if (n == focused) {
                                                    begin = focused;
                                                    end = m_event_node;
                                                    forceMultiselect(n);
                                                }
                                            } else {
                                                forceMultiselect(n);
                                                if (n == end) {
                                                    return Traversal::Break;
                                                }
                                            }
                                            return Traversal::Continue;
                                        });
                                    } else {
                                        select(m_event_node);
                                    }
                                }
                            }
                        }
                    }
                });
                this->onMouseLeft.addEventListener([&](Widget *widget, MouseEvent event) {
                    this->m_hovered = nullptr;
                });
                // TODO this should jump to the next index based node which may not be on the same depth
                // ex:
                // * 1--
                //     |--- 2
                //     |--- 3
                //     |--- 4
                // * 5 --...
                // if we are on node 5 pressing up should go to 4 not 1!
                // but isnt the manual descend doing that?? im confused... maybe it was an issue with the file explorer
                bind(SDLK_UP, Mod::None, [&]() {
                    if (m_cursor) {
                        // TODO make sure to scroll the node into view
                        auto focusNextNode = [&](TreeNode<T> *node) -> TreeNode<T>* {
                            if (node->parent_index == 0 && node->depth != 1) {
                                return node->parent;
                            }
                            if (node->parent_index > 0 && node->depth != 1) {
                                return node->parent->children[node->parent_index - 1];
                            }
                            if (node->depth == 1 && node->parent_index > 0) {
                                Traversal early_exit = Traversal::Continue;
                                return m_model->descend(early_exit, m_model->roots[node->parent_index - 1], [&](TreeNode<T> *node) {
                                    if (node->is_collapsed) { return Traversal::Break; }
                                    else { return Traversal::Continue; }
                                });
                            }
                            return nullptr; // TODO change to maybe wrap around
                        };
                        TreeNode<T> *result = focusNextNode(m_cursor);
                        if (result) {
                            select(result);
                        }
                    } else {
                        assert(m_model && m_model->roots.size() && "Trying to focus node when model doesnt exist or is empty!");
                        m_cursor = m_model->roots[0];
                    }
                    update();
                });
                bind(SDLK_DOWN, Mod::None, [&]() {
                    if (m_cursor) {
                        // TODO make sure to scroll the node into view
                        auto focusNextNode = [&](TreeNode<T> *node) -> TreeNode<T>* {
                            if (node->children.size() && !node->is_collapsed) {
                                return node->children[0];
                            }
                            while (node->parent) {
                                if ((i32)node->parent->children.size() - 1 > node->parent_index) {
                                    return node->parent->children[node->parent_index + 1];
                                }
                                node = node->parent;
                            }
                            if (node->depth == 1 && (i32)m_model->roots.size() - 1 > node->parent_index) {
                                return m_model->roots[node->parent_index + 1];
                            }
                            return nullptr; // TODO change to maybe wrap around
                        };
                        TreeNode<T> *result = focusNextNode(m_cursor);
                        if (result) {
                            select(result);
                        }
                    } else {
                        assert(m_model && m_model->roots.size() && "Trying to focus node when model doesnt exist or is empty!");
                        m_cursor = m_model->roots[0];
                    }
                    update();
                });
                bind(SDLK_LEFT, Mod::None, [&]() {
                    if (m_cursor && m_cursor->children.size()) {
                        collapse(m_cursor);
                    }
                });
                bind(SDLK_RIGHT, Mod::None, [&]() {
                    if (m_cursor && m_cursor->children.size()) {
                        expand(m_cursor);
                    }
                });
                m_column_style = Style();
                m_column_style.border.type = STYLE_BOTTOM|STYLE_RIGHT;
                m_column_style.border.bottom = 1;
                m_column_style.border.right = 1;
                m_column_style.margin.type = STYLE_NONE;

                m_column_button_style = Style();
                m_column_button_style.widget_background_color = COLOR_NONE;
                m_column_button_style.border.type = STYLE_NONE;
                m_column_button_style.margin.type = STYLE_NONE;
            }

            ~TreeView() {
                delete m_model;
                delete m_collapsed;
                delete m_expanded;
            }

            virtual const char* name() override {
                return "TreeView";
            }

            virtual void draw(DrawingContext &dc, Rect rect, i32 state) override {
                assert(m_model && "A TreeView needs a model to work!");
                this->rect = rect;

                dc.margin(rect, style());
                dc.drawBorder(rect, style(), state);
                this->inner_rect = rect;
                dc.fillRect(rect, dc.textBackground(style()));

                Size virtual_size = m_virtual_size;
                if (areColumnHeadersHidden()) {
                    virtual_size.h -= m_children_size.h;
                }
                Rect old_clip = dc.clip();
                Point pos = Point(rect.x, rect.y);
                if (m_mode == Mode::Scroll) {
                    pos = automaticallyAddOrRemoveScrollBars(dc, rect, virtual_size);
                } else {
                    m_vertical_scrollbar->hide();
                    m_horizontal_scrollbar->hide();
                }
                this->inner_rect = rect;
                Rect tv_clip = old_clip;

                i32 child_count = m_expandable_columns;
                if (child_count < 1) { child_count = 1; }
                i32 expandable_length = (rect.w - m_children_size.w) / child_count;
                i32 remainder = (rect.w - m_children_size.w) % child_count;
                if (expandable_length < 0) {
                    expandable_length = 0;
                    remainder = 0;
                }
                i32 local_pos_x = pos.x;
                i32 i = 0;
                m_current_header_width = 0;
                for (Widget *child : children) {
                    i32 child_expandable_length = expandable_length;
                    if (remainder) {
                        child_expandable_length++;
                        remainder--;
                    }
                    Size s = child->sizeHint(dc);
                    if (((Column<T>*)child)->expand()) {
                        s.w += child_expandable_length > 0 ? child_expandable_length : 0;
                        m_column_widths[i] = s.w;
                    }
                    m_current_header_width += s.w;
                    child->rect = Rect(local_pos_x, rect.y, s.w, m_children_size.h); // Always set rect because we need it for event handling.
                    if (!areColumnHeadersHidden()) {
                        dc.setClip(Rect(
                            local_pos_x,
                            rect.y,
                            local_pos_x + s.w > rect.x + rect.w ? (rect.x + rect.w) - local_pos_x : s.w,
                            m_children_size.h
                        ).clipTo(tv_clip).clipTo(rect));
                        child->draw(dc, Rect(local_pos_x, rect.y, s.w, m_children_size.h), child->state());
                    }
                    local_pos_x += s.w;
                    i++;
                }
                i32 column_header = 0;
                if (!areColumnHeadersHidden()) {
                    column_header = m_children_size.h;
                    pos.y += column_header;
                }

                i32 mx, my;
                SDL_GetMouseState(&mx, &my);
                i32 moffset = my - pos.y;
                i32 drop_y = -1;
                i32 drop_h = -1;
                i32 drop_offset = 0;

                Rect drawing_rect = rect;
                if (m_mode == Mode::Unroll) {
                    drawing_rect = Rect(0, 0, Application::get()->currentWindow()->size.w, Application::get()->currentWindow()->size.h);
                    column_header = 0;
                }

                bool collapsed = false;
                i32 collapsed_depth = -1;
                u64 y_scroll_offset = (m_vertical_scrollbar->isVisible() ? m_vertical_scrollbar->m_slider->m_value : 0.0) * ((virtual_size.h) - inner_rect.h);
                if (m_mode == Mode::Unroll && rect.y + m_children_size.h < 0) {
                    y_scroll_offset = (rect.y + m_children_size.h) * -1;
                }
                Option<TreeNode<T>*> result = binarySearch(m_model->roots, y_scroll_offset).value;
                if (result) {
                    pos.y += result.value->bs_data.position;
                    TreeNode<T> *node = result.value;

                    bool finished = false;
                    while (node) {
                        setupDropData(moffset, drop_y, drop_h, drop_offset, node, column_header, y_scroll_offset);
                        if (node->depth <= collapsed_depth) {
                            collapsed = false;
                            collapsed_depth = -1;
                        }
                        if (!collapsed) {
                            drawNode(dc, pos, node, rect, drawing_rect, tv_clip, column_header);
                            if (pos.y > drawing_rect.y + drawing_rect.h) { break; }
                            if (node->is_collapsed && !collapsed) {
                                collapsed = true;
                                collapsed_depth = node->depth;
                            }

                            m_model->forEachNode(node->children, [&](TreeNode<T> *node) -> Traversal {
                                setupDropData(moffset, drop_y, drop_h, drop_offset, node, column_header, y_scroll_offset);
                                if (node->depth <= collapsed_depth) {
                                    collapsed = false;
                                    collapsed_depth = -1;
                                }
                                if (!collapsed) {
                                    drawNode(dc, pos, node, rect, drawing_rect, tv_clip, column_header);
                                    if (pos.y > drawing_rect.y + drawing_rect.h) {
                                        finished = true;
                                        return Traversal::Break;
                                    }
                                }
                                if (node->is_collapsed && !collapsed) {
                                    collapsed = true;
                                    collapsed_depth = node->depth;
                                }
                                return Traversal::Continue;
                            });
                        }
                        if (finished) { break; }
                        node = findNextNode(node);
                    }
                }

                if (m_model->roots.size()) {
                    i32 local_column_header = !areColumnHeadersHidden() ? m_children_size.h : 0;
                    // Clip and draw column grid lines.
                    if (m_grid_lines == GridLines::Vertical || m_grid_lines == GridLines::Both) {
                        dc.setClip(Rect(rect.x, rect.y + local_column_header, rect.w, rect.h - local_column_header).clipTo(tv_clip));
                        for (i32 width : m_column_widths) {
                            dc.fillRect(Rect(pos.x + width - m_grid_line_width, rect.y + local_column_header, m_grid_line_width, virtual_size.h - local_column_header), dc.textDisabled(style()));
                            pos.x += width;
                        }
                    }
                }

                dc.setClip(old_clip);
                if (m_mode == Mode::Scroll) {
                    drawScrollBars(dc, rect, virtual_size);
                }
                if (Application::get()->drag.state == DragEvent::State::Dragging and state & STATE_HOVERED) {
                    if (drop_allow) {
                        Color c = dc.hoveredBackground(style());
                        c.a = 0xaa;
                        Color cc = dc.accentWidgetBackground(style());
                        cc.a = 0x55;
                        dc.fillRect(this->rect, c);
                        if (drop_y != -1) {
                            if (drop_action.type == DropAction::Type::Above and drop_allow & cast(i32, DropAction::Type::Above)) {
                                dc.fillRect(Rect(rect.x, drop_y - drop_offset, rect.w, drop_h), cc);
                                dc.fillRect(Rect(rect.x, drop_y, rect.w, 2), dc.accentWidgetBackground(style()));
                            } else if (drop_action.type == DropAction::Type::Below and drop_allow & cast(i32, DropAction::Type::Below)) {
                                dc.fillRect(Rect(rect.x, drop_y - drop_offset, rect.w, drop_h), cc);
                                dc.fillRect(Rect(rect.x, drop_y, rect.w, 2), dc.accentWidgetBackground(style()));
                            } else if (drop_action.type == DropAction::Type::Child and drop_allow & cast(i32, DropAction::Type::Child)) {
                                dc.fillRect(Rect(rect.x, drop_y - drop_offset, rect.w, drop_h), cc);
                                dc.fillRect(Rect(rect.x, drop_y, rect.w, 2), dc.accentWidgetBackground(style()));
                                dc.fillRect(Rect(rect.x, drop_y + drop_h - 2, rect.w, 2), dc.accentWidgetBackground(style()));
                            }
                        } else {
                            if (drop_allow & cast(i32, DropAction::Type::Root)) {
                                dc.fillRect(this->rect, cc);
                            }
                            drop_action.node = nullptr;
                            drop_action.type = DropAction::Type::Root;
                        }
                    }
                }
                dc.drawKeyboardFocus(this->rect, style(), state);
            }

            void setModel(Tree<T> *model) {
                delete m_model;
                m_model = model;
                for (Widget *widget : children) {
                    ((Column<T>*)widget)->setModel(model);
                    ((Column<T>*)widget)->setColumnStyle(m_column_style, m_column_button_style);
                }
                m_virtual_size_changed = true;
                m_auto_size_columns = true;
                m_hovered = nullptr;
                m_cursor = nullptr;
                m_focused.clear();
                m_event_node = nullptr;
                m_tree_collapser = nullptr;
                if (m_last_sort) {
                    m_last_sort->sort(Sort::None);
                    m_last_sort = nullptr;
                }
                update();
            }

            void sort(u64 column_index, Sort sort_type) {
                ((Column<T>*)children[column_index])->sort(sort_type);
            }

            void clear() {
                if (m_model) {
                    m_model->clear();
                    m_virtual_size_changed = true;
                    m_hovered = nullptr;
                    m_cursor = nullptr;
                    m_focused.clear();
                    m_event_node = nullptr;
                    m_tree_collapser = nullptr;
                    if (m_last_sort) {
                        m_last_sort->sort(Sort::None);
                        m_last_sort = nullptr;
                    }
                    update();
                }
            }

            void setGridLines(GridLines grid_lines) {
                m_grid_lines = grid_lines;
                update();
            }

            GridLines gridLines() {
                return m_grid_lines;
            }

            /// Can return null.
            TreeNode<T>* hovered() {
                return m_hovered;
            }

            /// Can return null.
            ArrayList<TreeNode<T>*> selected() {
                return m_focused;
            }

            u8 indent() {
                return m_indent;
            }

            void setIndent(u8 indent_width) {
                if (indent_width >= 12) {
                    m_indent = indent_width;
                    update();
                }
            }

            void select(TreeNode<T> *node) {
                if (!m_focused.contains(node)) {
                    deselectAll();
                    m_focused.append(node);
                    m_cursor = node;
                    onNodeSelected.notify(this, node);
                } else {
                    m_focused.erase(m_focused.find(node).unwrap());
                    deselectAll();
                    m_focused.append(node);
                    m_cursor = node;
                    onNodeSelected.notify(this, node);
                    // TODO if we deselct then we cant drag all the nodes unless
                    // we do multiselect with control and only drag the last node
                    // i think we need to have something that detects a drag
                }
                update();
            }

            void select(u64 index) {
                // TODO should we do something about null here?
                select(m_model->get(index));
            }

            void multiselect(TreeNode<T> *node) {
                Option<usize> index = m_focused.find(node);
                if (index) {
                    m_focused.erase(index.value);
                    onNodeDeselected.notify(this, node);
                } else {
                    m_focused.append(node);
                    m_cursor = node;
                    onNodeSelected.notify(this, node);
                }
                update();
            }

            void multiselect(u64 index) {
                // TODO should we do something about null here?
                multiselect(m_model->get(index));
            }

            void forceMultiselect(TreeNode<T> *node) {
                Option<usize> index = m_focused.find(node);
                if (!index) {
                    m_focused.append(node);
                    m_cursor = node;
                    onNodeSelected.notify(this, node);
                    update();
                }
            }

            void deselectAll() {
                if (!m_focused.isEmpty()) {
                    notifyOnDeselected();
                    m_focused.clear();
                    update();
                }
            }

            void deselect(TreeNode<T> *node) {
                Option<usize> index = m_focused.find(node);
                if (index) {
                    onNodeDeselected.notify(this, node);
                    m_focused.erase(index.value);
                    update();
                }
            }

            void notifyOnDeselected() {
                for (TreeNode<T> *node : m_focused) {
                    onNodeDeselected.notify(this, node);
                }
                update();
            }

            void collapse(TreeNode<T> *node) {
                if (node && node->children.size()) {
                    node->is_collapsed = true;
                    onNodeCollapsed.notify(this, node);
                    m_virtual_size_changed = true;
                    update();
                    if (m_mode == Mode::Unroll && parent) { parent->m_size_changed = true; }
                }
            }

            void collapseRecursively(TreeNode<T> *node) {
                collapseOrExpandRecursively(node, true);
            }

            void collapseAll() {
                collapseOrExpandAll(true);
            }

            void expand(TreeNode<T> *node) {
                if (node && node->children.size()) {
                    node->is_collapsed = false;
                    onNodeExpanded.notify(this, node);
                    m_virtual_size_changed = true;
                    update();
                    if (m_mode == Mode::Unroll && parent) { parent->m_size_changed = true; }
                }
            }

            void expandRecursively(TreeNode<T> *node) {
                collapseOrExpandRecursively(node, false);
            }

            void expandAll() {
                collapseOrExpandAll(false);
            }

            TreeView* append(Column<T> *column) {
                if (column->parent) {
                    column->parent->remove(column->parent_index);
                }
                column->parent = this;
                column->onMouseEntered.addEventListener([&](Widget *widget, MouseEvent event) {
                    this->m_hovered = nullptr;
                });
                column->onMouseClick.addEventListener([&](Widget *column, MouseEvent event) {
                    Column<T> *col = (Column<T>*)column;
                    if (col->sort_fn) {
                        if (this->m_last_sort && this->m_last_sort != col) {
                            this->m_last_sort->sort(Sort::None);
                        } else {
                            col->sort(col->isSorted());
                        }
                        this->m_last_sort = col;
                    }
                });
                children.push_back(column);
                column->parent_index = children.size() - 1;
                layout(LAYOUT_CHILD);

                return this;
            }

            virtual Size sizeHint(DrawingContext &dc) override {
                if (m_size_changed) { // TODO this will be slow for a large number of columns
                    Scrollable::sizeHint(dc);
                    m_virtual_size.w = 0;
                    m_column_widths.clear();
                    Size size = Size();
                    m_expandable_columns = 0;
                    for (Widget *child : children) {
                        Size s = child->sizeHint(dc);
                        m_column_widths.push_back(s.w);
                        size.w += s.w;
                        if (s.h > size.h) {
                            size.h = s.h;
                        }
                        if (((Column<T>*)child)->expand()) {
                            m_expandable_columns++;
                        }
                    }
                    m_children_size = size;
                    m_virtual_size.w = size.w;
                    m_size_changed = false;
                }
                if (m_virtual_size_changed) {
                    calculateVirtualSize(dc);
                }
                if (m_mode == Mode::Scroll) {
                    Size viewport_and_style = m_viewport;
                        dc.sizeHintMargin(viewport_and_style, style());
                        dc.sizeHintBorder(viewport_and_style, style());
                    return viewport_and_style;
                }
                if (areColumnHeadersHidden()) {
                    Size virtual_size_and_style = Size(m_virtual_size.w, m_virtual_size.h - m_children_size.h);
                        dc.sizeHintMargin(virtual_size_and_style, style());
                        dc.sizeHintBorder(virtual_size_and_style, style());
                    return virtual_size_and_style;
                }
                Size virtual_size_and_style = m_virtual_size;
                    dc.sizeHintMargin(virtual_size_and_style, style());
                    dc.sizeHintBorder(virtual_size_and_style, style());
                return virtual_size_and_style;
            }

            bool isTable() {
                return m_table;
            }

            void setTableMode(bool table) {
                m_table = table;
                update();
            }

            void setMode(Mode mode) {
                m_mode = mode;
                layout(LAYOUT_STYLE);
            }

            Mode mode() {
                return m_mode;
            }

            void showColumnHeaders() {
                m_column_headers_hidden = false;
            }

            void hideColumnHeaders() {
                m_column_headers_hidden = true;
            }

            bool areColumnHeadersHidden() {
                return m_column_headers_hidden;
            }

            Widget* propagateMouseEvent(Window *window, State *state, MouseEvent event) override {
                if (m_vertical_scrollbar->isVisible()) {
                    if ((event.x >= m_vertical_scrollbar->rect.x && event.x <= m_vertical_scrollbar->rect.x + m_vertical_scrollbar->rect.w) &&
                        (event.y >= m_vertical_scrollbar->rect.y && event.y <= m_vertical_scrollbar->rect.y + m_vertical_scrollbar->rect.h)) {
                        return m_vertical_scrollbar->propagateMouseEvent(window, state, event);
                    }
                }
                if (m_horizontal_scrollbar->isVisible()) {
                    if ((event.x >= m_horizontal_scrollbar->rect.x && event.x <= m_horizontal_scrollbar->rect.x + m_horizontal_scrollbar->rect.w) &&
                        (event.y >= m_horizontal_scrollbar->rect.y && event.y <= m_horizontal_scrollbar->rect.y + m_horizontal_scrollbar->rect.h)) {
                        return m_horizontal_scrollbar->propagateMouseEvent(window, state, event);
                    }
                }
                if (m_vertical_scrollbar->isVisible() && m_horizontal_scrollbar->isVisible()) {
                    if ((event.x > m_horizontal_scrollbar->rect.x + m_horizontal_scrollbar->rect.w) &&
                        (event.y > m_vertical_scrollbar->rect.y + m_vertical_scrollbar->rect.h)) {
                        if (state->hovered) {
                            state->hovered->onMouseLeft.notify(this, event);
                        }
                        state->hovered = nullptr;
                        update();
                        return nullptr;
                    }
                }
                if (!areColumnHeadersHidden()) {
                    for (Widget *child : children) {
                        if ((event.x >= child->rect.x && event.x <= child->rect.x + child->rect.w) &&
                            (event.y >= child->rect.y && event.y <= child->rect.y + child->rect.h)) {
                            Widget *last = nullptr;
                            if (child->isLayout()) {
                                last = child->propagateMouseEvent(window, state, event);
                            } else {
                                child->handleMouseEvent(window, state, event);
                                last = child;
                            }
                            return last;
                        }
                    }
                }
                {
                    // Go down the Node tree to find either a Widget to pass the event to
                    // or simply record the node and pass the event to the TreeView itself as per usual.
                    i32 x = inner_rect.x;
                    i32 y = inner_rect.y;
                    if (m_horizontal_scrollbar) {
                        x -= m_horizontal_scrollbar->m_slider->m_value * ((m_virtual_size.w) - inner_rect.w);
                    }
                    if (event.x <= x + m_current_header_width) {
                        Size virtual_size = m_virtual_size;
                        if (areColumnHeadersHidden()) {
                            virtual_size.h -= m_children_size.h;
                        } else {
                            y += m_children_size.h;
                        }
                        u64 y_scroll_offset = (m_vertical_scrollbar->isVisible() ? m_vertical_scrollbar->m_slider->m_value : 0.0) * (virtual_size.h - inner_rect.h);
                        // u64 x_scroll_offset = (m_horizontal_scrollbar->isVisible() ? m_horizontal_scrollbar->m_slider->m_value : 0.0) * (virtual_size.w - inner_rect.w);
                        Option<TreeNode<T>*> result = binarySearch(m_model->roots, (event.y - y) + y_scroll_offset).value;
                        if (result) {
                            TreeNode<T> *node = result.value;
                            for (u64 i = 0; i < children.size(); i++) {
                                Column<T> *col = (Column<T>*)children[i];
                                if ((event.x >= col->rect.x) && (event.x <= (col->rect.x + col->rect.w))) {
                                    if (node->columns[i]->isWidget()) {
                                        auto widget = (Widget*)node->columns[i];
                                        widget->parent = this;
                                        if ((event.x >= widget->rect.x && event.x <= widget->rect.x + widget->rect.w) &&
                                            (event.y >= widget->rect.y && event.y <= widget->rect.y + widget->rect.h)
                                        ) {
                                            Window *win = Application::get()->currentWindow();
                                            widget->handleMouseEvent(win, win->m_state, event);
                                            return widget;
                                        } else {
                                            m_event_node = node;
                                        }
                                    } else {
                                        m_event_node = node;
                                    }
                                }
                            }
                        } else {
                            m_event_node = nullptr;
                            m_tree_collapser = nullptr;
                        }
                    } else {
                        m_event_node = nullptr;
                        m_tree_collapser = nullptr;
                    }
                }

                handleMouseEvent(window, state, event);
                return this;
            }

            Tree<T>* model() {
                return m_model;
            }

            Tree<T> *m_model = nullptr;
            Size m_virtual_size;
            bool m_virtual_size_changed = false;
            Mode m_mode = Mode::Scroll;
            u8 m_indent = 24;
            TreeNode<T> *m_hovered = nullptr;
            TreeNode<T> *m_cursor = nullptr;
            ArrayList<TreeNode<T>*> m_focused;
            TreeNode<T> *m_event_node = nullptr; // node to be handled in mouse events
            TreeNode<T> *m_tree_collapser = nullptr; // the tree collapse/expand icon node if any (for highlighting)
            GridLines m_grid_lines = GridLines::Both;
            i32 m_treeline_size = 2;
            i32 m_grid_line_width = 1;
            Column<T> *m_last_sort = nullptr;
            Size m_children_size = Size();
            i32 m_current_header_width = 0;
            bool m_column_headers_hidden = false;
            std::vector<i32> m_column_widths;
            bool m_auto_size_columns = false;
            bool m_table = false;
            Style m_column_style;
            Style m_column_button_style;
            Image *m_collapsed = (new Image(Application::get()->icons["up_arrow"]))->clockwise90();
            Image *m_expanded = (new Image(Application::get()->icons["up_arrow"]))->flipVertically();
            i32 m_expandable_columns = 0;
            DropAction drop_action;
            i32 drop_allow = 0b1111;

            void collapseOrExpandRecursively(TreeNode<T> *node, bool is_collapsed) {
                if (node) {
                    Traversal _unused = Traversal::Continue;
                    m_model->descend(_unused, node, [&](TreeNode<T> *_node) {
                        if (_node->children.size()) {
                            _node->is_collapsed = is_collapsed;
                        }
                        return Traversal::Continue;
                    });
                    m_virtual_size_changed = true;
                    update();
                    if (m_mode == Mode::Unroll && parent) { parent->m_size_changed = true; }
                }
            }

            void collapseOrExpandAll(bool is_collapsed) {
                Traversal _unused = Traversal::Continue;
                for (TreeNode<T> *root : m_model->roots) {
                    m_model->descend(_unused, root, [&](TreeNode<T> *node) {
                        if (node->children.size()) {
                            node->is_collapsed = is_collapsed;
                        }
                        return Traversal::Continue;
                    });
                }
                m_virtual_size_changed = true;
                update();
                if (m_mode == Mode::Unroll && parent) { parent->m_size_changed = true; }
            }

            void calculateVirtualSize(DrawingContext &dc) {
                m_virtual_size = m_children_size;
                bool collapsed = false;
                i32 collapsed_depth = -1;
                i32 parent_index = 0;
                u64 scroll_offset = 0;

                m_model->forEachNode(
                    m_model->roots,
                    [&](TreeNode<T> *node) {
                        if (node->depth == 1) {
                            node->parent_index = parent_index++;
                        }
                        if (node->depth <= collapsed_depth) {
                            collapsed = false;
                            collapsed_depth = -1;
                        }
                        if (!collapsed) {
                            // Check and set the max height of the node.
                            node->max_cell_height = m_grid_line_width;
                            i32 index = 0;
                            assert(node->columns.size() == children.size() && "The amount of Column<T>s and Drawables should be the same!");
                            for (Drawable *drawable : node->columns) {
                                Size s = drawable->sizeHint(dc);
                                Column<T> *col = (Column<T>*)children[index];
                                if (!m_table && !index) {
                                    s.w += node->depth * indent();
                                }

                                // Automatically set the columns to be wide
                                // enough for their contents.
                                if ((m_auto_size_columns || m_mode == Mode::Unroll) && s.w > col->width()) {
                                    s.w += m_grid_line_width;
                                    col->setWidth(s.w);
                                    // The below is necessary because sizeHint won't run
                                    // again until the next update().
                                    m_children_size.w += s.w - m_column_widths[index];
                                    m_column_widths[index] = s.w;
                                    // We don't need to recalculate here specifically
                                    // because we already update the values manually.
                                    m_size_changed = false;
                                }
                                if (s.h + m_grid_line_width > node->max_cell_height) {
                                    node->max_cell_height = s.h + m_grid_line_width;
                                }
                                index++;
                            }
                            node->bs_data = BinarySearchData{scroll_offset, (u64)node->max_cell_height};
                            scroll_offset += (u64)node->max_cell_height;
                            m_virtual_size.h += node->max_cell_height;
                        } else {
                            node->bs_data = BinarySearchData{scroll_offset, 0};
                        }

                        if (node->is_collapsed && !collapsed) {
                            collapsed = true;
                            collapsed_depth = node->depth;
                        }
                        return Traversal::Continue;
                    }
                );

                m_virtual_size.w = m_children_size.w;
                m_virtual_size_changed = false;
                m_auto_size_columns = false;
            }


            BinarySearchResult<TreeNode<T>*> binarySearch(std::vector<TreeNode<T>*> &roots, u64 target) {
                if (!roots.size()) { return BinarySearchResult<TreeNode<T>*>{ 0, Option<TreeNode<T>*>() }; }
                u64 lower = 0;
                u64 upper = roots.size() - 1;
                u64 mid = 0;
                BinarySearchData point = {0, 0};

                while (lower <= upper) {
                    mid = (lower + upper) / 2;
                    point = roots[mid]->bs_data;
                    if (target < point.position) {
                        upper = mid - 1;
                    } else if (target > (roots.size() - 1 > mid ? roots[mid + 1]->bs_data.position : point.position + point.length)) {
                        lower = mid + 1;
                    } else {
                        break;
                    }
                }

                if (point.position <= target && point.position + point.length >= target) {
                   return BinarySearchResult<TreeNode<T>*>{ mid, Option<TreeNode<T>*>(roots[mid]) };
                } else if (roots[mid]->children.size()) {
                    return binarySearch(roots[mid]->children, target);
                }

                return BinarySearchResult<TreeNode<T>*>{ 0, Option<TreeNode<T>*>() };
            }

            void drawNode(DrawingContext &dc, Point &pos, TreeNode<T> *node, Rect rect, Rect drawing_rect, Rect tv_clip, i32 column_header) {
                i32 cell_start = pos.x;
                for (u64 i = 0; i < node->columns.size(); i++) {
                    i32 col_width = m_column_widths[i];
                    Drawable *drawable = node->columns[i];
                    Size s = drawable->sizeHint(dc);
                    if (cell_start + col_width > drawing_rect.x && cell_start < drawing_rect.x + drawing_rect.w) {
                        Rect cell_clip =
                            Rect(cell_start, pos.y, col_width, node->max_cell_height)
                                .clipTo(
                                    Rect(
                                        rect.x,
                                        rect.y + column_header,
                                        rect.w,
                                        rect.h - column_header
                                    )
                                );
                        // Clip and draw the current cell.
                        dc.setClip(cell_clip.clipTo(tv_clip));
                        i32 cell_x = cell_start;
                        i32 state = STATE_DEFAULT;
                        if (m_focused.contains(node)) { state |= STATE_HARD_FOCUSED; }
                        if (m_hovered == node) { state |= STATE_HOVERED; }
                        if (!m_table && !i) {
                            // Draw the cell background using appropriate state in treeline gutter when drawing treelines.
                            EmptyCell().draw(
                                dc,
                                Rect(cell_x, cell_clip.y, node->depth * m_indent, cell_clip.h),
                                state
                            );
                            cell_x += node->depth * m_indent;
                        }
                        i32 h_grid_line = 0;
                        if (m_grid_lines == GridLines::Horizontal || m_grid_lines == GridLines::Both) {
                            h_grid_line = m_grid_line_width;
                        }
                        i32 v_grid_line = 0;
                        if (m_grid_lines == GridLines::Vertical || m_grid_lines == GridLines::Both) {
                            v_grid_line = m_grid_line_width;
                        }
                        if (drawable->isWidget()) {
                            // Draw the cell background using appropriate state for cells with widgets in them.
                            EmptyCell().draw(
                                dc,
                                Rect(
                                    cell_x, pos.y, col_width > s.w ? col_width - h_grid_line : s.w - h_grid_line, node->max_cell_height - v_grid_line
                                ),
                                state
                            );
                            state = ((Widget*)drawable)->state();
                        }
                        Rect draw_rect = Rect(cell_x, pos.y, col_width > s.w ? col_width - h_grid_line : s.w - h_grid_line, node->max_cell_height - v_grid_line);
                        drawable->draw(
                            dc,
                            draw_rect,
                            state
                        );
                        if (m_cursor == node) {
                            draw_rect.x = pos.x;
                            draw_rect.w = drawing_rect.w - h_grid_line;
                            dc.drawDashedRect(draw_rect, dc.textForeground(style()));
                        }
                    }
                    cell_start += col_width;
                    if (cell_start > drawing_rect.x + drawing_rect.w) {
                        break;
                    }
                }
                if (m_grid_lines == GridLines::Horizontal || m_grid_lines == GridLines::Both) {
                    dc.setClip(Rect(rect.x, rect.y + column_header, rect.w, rect.h - column_header).clipTo(tv_clip));
                    dc.fillRect(Rect(rect.x, pos.y + node->max_cell_height - m_grid_line_width, m_current_header_width, m_grid_line_width), dc.textDisabled(style()));
                }
                if (!m_table) {
                    drawTreeLine(dc, pos, rect, tv_clip, column_header, node);
                }
                pos.y += node->max_cell_height;
            }

            TreeNode<T>* findNextNode(TreeNode<T> *node) {
                while (node->parent) {
                    if ((i32)node->parent->children.size() - 1 > node->parent_index) {
                        return node->parent->children[node->parent_index + 1];
                    }
                    node = node->parent;
                }
                if (node->depth == 1) {
                    if ((i32)m_model->roots.size() - 1 > node->parent_index) {
                        return m_model->roots[node->parent_index + 1];
                    }
                }

                return nullptr;
            }

            void drawTreeLine(DrawingContext &dc, Point pos, Rect rect, Rect tv_clip, i32 column_header, TreeNode<T> *node) {
                dc.setClip(Rect(rect.x, rect.y + column_header, m_column_widths[0], rect.h - column_header).clipTo(tv_clip));
                i32 x = pos.x + (node->depth * m_indent);
                i32 y = pos.y + (node->max_cell_height / 2);

                if (node->parent) {
                    // Higher sibling or no sibling
                    drawTreeLineFromParentToLowestChildRecursivelyAscending(dc, pos, node, column_header);
                }

                if (node->children.size()) {
                    // Draw a little line connecting the parent to its children.
                    // We do this so that the node status icon doesn't get drawn over.
                    if (!node->is_collapsed) {
                        dc.fillRect(
                            Rect(
                                x - (m_indent / 2) - (m_treeline_size / 2),
                                y,
                                m_treeline_size,
                                node->max_cell_height / 2
                            ),
                            dc.borderBackground(style())
                        );
                    }

                    if (node->parent) {
                        drawTreeLineConnector(dc, x, y);

                        // Lower sibling
                        if (node->parent_index < (i32)node->parent->children.size() - 1) {
                            auto sibling = node->parent->children[node->parent_index + 1];
                            u64 distance = sibling->bs_data.position - node->bs_data.position;
                            // Sibling off screen
                            if (pos.y + (i32)distance > rect.y + rect.h) {
                                dc.fillRect(
                                    Rect(
                                        x - (m_indent * 1.5) - (m_treeline_size / 2),
                                        y,
                                        m_treeline_size,
                                        (rect.y + rect.h) - pos.y
                                    ),
                                    dc.borderBackground(style())
                                );
                            }
                        }

                        // Draw regular line to parent
                        drawTreeLineToParent(dc, x, pos.y, node);
                    }

                    Image *img = !node->is_collapsed ? m_expanded : m_collapsed;
                    Color fg = dc.iconForeground(style());
                    if (m_tree_collapser && m_tree_collapser == node) {
                        fg = dc.textSelected(style());
                    }
                    dc.drawTextureAligned(
                        Rect(x - m_indent, y - (node->max_cell_height / 2), m_indent, node->max_cell_height),
                        Size(m_indent / 2, m_indent / 2),
                        img->_texture(),
                        img->coords(),
                        HorizontalAlignment::Center,
                        VerticalAlignment::Center,
                        fg
                    );
                // End of the line.
                } else {
                    if (node->parent) {
                        drawTreeLineConnector(dc, x, y);
                        drawTreeLineToParent(dc, x, pos.y, node);
                        drawTreeLineNoChildrenIndicator(dc, pos.x, y, node);
                    }
                }
            }

            void drawTreeLineConnector(DrawingContext &dc, i32 x, i32 y) {
                dc.fillRect(
                    Rect(
                        x - (m_indent * 1.5) - (m_treeline_size / 2),
                        y,
                        m_indent,
                        m_treeline_size
                    ),
                    dc.borderBackground(style())
                );
            }

            void drawTreeLineToParent(DrawingContext &dc, i32 x, i32 y, TreeNode<T> *node) {
                u64 distance = node->bs_data.position - node->parent->bs_data.position;
                dc.fillRect(
                    Rect(
                        x - (m_indent * 1.5) - (m_treeline_size / 2),
                        y - (distance - node->parent->bs_data.length) - m_grid_line_width,
                        m_treeline_size,
                        (i32)distance - node->parent->bs_data.length + (node->bs_data.length / 2) + m_grid_line_width
                    ),
                    dc.borderBackground(style())
                );
            }

            void drawTreeLineNoChildrenIndicator(DrawingContext &dc, i32 x, i32 y, TreeNode<T> *node) {
                dc.fillRect(
                    Rect(
                        x + ((node->depth - 1) * m_indent) + (m_indent / 3),
                        y - (m_indent / 8) + (m_treeline_size / 2),
                        m_indent / 4,
                        m_indent / 4
                    ),
                    dc.iconForeground(style())
                );
            }

            void drawTreeLineFromParentToLowestChildRecursivelyAscending(DrawingContext &dc, Point pos, TreeNode<T> *node, i32 column_header) {
                TreeNode<T> *sibling = node->parent;
                u64 distance = 0;
                if (node->parent->children.size() > 1 && node->parent_index > 0) {
                    sibling = node->parent->children[node->parent_index - 1];
                    distance = node->bs_data.position - sibling->bs_data.position;
                }

                // Sibling off screen
                // NOTE: The reason for node->bs_data.length here is that
                // pos.y is the top of the viewport not the beginning of the start node.
                // So if pos.y is halfway through the start node then just the distance will not take
                // us all the way to the beginning of the sibling and to keep it safe we use the
                // entire height of the node rather than just the the different between pos.y and node->bs_data.position.
                if (pos.y - (i32)(distance + node->bs_data.length) <= rect.y + column_header) {
                    // When the higher sibling is off screen
                    // recursively go up the tree to root and draw a line
                    // between the parent and its last child.
                    // This is needed when not a single node directly related to the line
                    // is visible on screen but the line spans more than the screen.
                    auto _parent = node->parent;
                    while (_parent->parent) {
                        _parent = _parent->parent;
                        // We know here that the parent will have at least one child because
                        // we are getting here from within the hierarchy.
                        auto _node = _parent->children[_parent->children.size() - 1];
                        dc.fillRect(
                        Rect(
                                pos.x + (_node->depth * m_indent) - (m_indent * 1.5) - (m_treeline_size / 2),
                                pos.y - (node->bs_data.position - _parent->bs_data.position),
                                m_treeline_size,
                                _node->bs_data.position - _parent->bs_data.position
                            ),
                            dc.borderBackground(style())
                        );
                    }
                }
            }

            Widget* handleFocusEvent(FocusEvent event, State *state, FocusPropagationData data) override {
                return Widget::handleFocusEvent(event, state, data);
            }

            i32 isFocusable() override {
                return (i32)FocusType::Focusable;
            }

            bool handleLayoutEvent(LayoutEvent event) override {
                if (event) {
                    m_virtual_size_changed = true;
                    // Since we already know the layout needs to be redone
                    // we return true to avoid having to traverse the entire widget graph to the top.
                    if (m_size_changed) { return true; }
                    m_size_changed = true;
                }
                return false;
            }

            void forEachDrawable(std::function<void(Drawable *drawable)> action) override {
                assert(m_model && "Model needs to be set for forEachDrawable!");
                action(this);
                for (Widget *child : children) {
                    child->forEachDrawable(action);
                }
                m_model->forEachNode(m_model->roots, [&](TreeNode<T> *node) -> Traversal {
                    if (node->is_collapsed) { return Traversal::Break; }
                    for (Drawable *cell : node->columns) {
                        action(cell);
                    }
                    return Traversal::Continue;
                });
            }

            void handleDragEvent(DragEvent event) override {
                onDragDropped.notify(this, event);
            }

            void setupDropData(i32 &moffset, i32 &drop_y, i32 &drop_h, i32 &drop_offset, TreeNode<T> *node, i32 column_header, i32 y_scroll_offset) {
                if (moffset >= cast(i32, node->bs_data.position) and moffset <= cast(i32, node->bs_data.position + node->bs_data.length)) {
                    if (drop_allow == 0b1100 or drop_allow == 0b1101) {
                        if (moffset > cast(i32, node->bs_data.position + (node->bs_data.length / 2))) {
                            drop_y = node->bs_data.position - y_scroll_offset + node->bs_data.length;
                            drop_h = node->bs_data.length / 4;
                            drop_offset = node->bs_data.length / 4;
                            drop_action = DropAction{node, DropAction::Type::Below};
                        } else {
                            drop_y = node->bs_data.position - y_scroll_offset;
                            drop_h = node->bs_data.length / 4;
                            drop_offset = 0;
                            drop_action = DropAction{node, DropAction::Type::Above};
                        }
                    } else if (drop_allow == 0b0011 or drop_allow == 0b0010) {
                        drop_y = node->bs_data.position - y_scroll_offset;
                        drop_h = node->bs_data.length;
                        drop_offset = 0;
                        drop_action = DropAction{node, DropAction::Type::Child};
                    } else {
                        if (moffset > cast(i32, node->bs_data.position + ((node->bs_data.length / 4) * 3))) {
                            drop_y = node->bs_data.position - y_scroll_offset + node->bs_data.length;
                            drop_h = node->bs_data.length / 4;
                            drop_offset = node->bs_data.length / 4;
                            drop_action = DropAction{node, DropAction::Type::Below};
                        } else if (moffset < cast(i32, node->bs_data.position + (node->bs_data.length / 4))) {
                            drop_y = node->bs_data.position - y_scroll_offset;
                            drop_h = node->bs_data.length / 4;
                            drop_offset = 0;
                            drop_action = DropAction{node, DropAction::Type::Above};
                        } else {
                            drop_y = node->bs_data.position - y_scroll_offset + node->bs_data.length / 4;
                            drop_h = node->bs_data.length / 2;
                            drop_offset = 0;
                            drop_action = DropAction{node, DropAction::Type::Child};
                        }
                    }
                    drop_y += column_header;
                    drop_y += this->rect.y;
                }
            }

            bool isCtrlPressed() {
                SDL_Keymod mod = SDL_GetModState();
                if (mod & Mod::Ctrl) {
                    return true;
                }
                return false;
            }

            bool isShiftPressed() {
                SDL_Keymod mod = SDL_GetModState();
                if (mod & Mod::Shift) {
                    return true;
                }
                return false;
            }
    };
#endif
