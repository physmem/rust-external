#pragma once

#include "../../base/animation/animation.hpp"
#include "../events/events.hpp"
#include "../hash/fnv.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <yoga/Yoga.h>

namespace gui
{
	struct ConstName
	{
		const char* str{};
		fnv1a_t hash{};

		consteval ConstName(const char* s) : str(s), hash(fnv::hash_const(s)) {}
	};

	class Object
	{
	public:
		Object()
		{
			yoga_node = YGNodeNew();
			YGNodeSetContext(yoga_node, this);
		}

		Object(std::string_view name) : m_name(name), id(fnv::hash(name))
		{
			yoga_node = YGNodeNew();
			YGNodeSetContext(yoga_node, this);
		}

		Object(ConstName name) : m_name(name.str), id(name.hash)
		{
			yoga_node = YGNodeNew();
			YGNodeSetContext(yoga_node, this);
		}

		virtual ~Object()
		{
			if (yoga_node) {
				YGNodeFree(yoga_node);
			}
		}

		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		Object* parent{};
		std::vector<std::shared_ptr<Object>> children{};

		bool should_display{ true };
		bool should_save{ true };
		Animation display_anim{ 1.0f };

		inline static Object* g_active_object{ nullptr };
		inline static Object* g_focused_object{ nullptr };

		template <typename T, typename... Args>
		T* add_object(Args&&... args)
		{
			auto child = std::make_shared<T>(std::forward<Args>(args)...);
			child->parent = this;

			this->attach_child(child);

			T* raw = child.get();
			children.push_back(std::move(child));
			return raw;
		}

		virtual void attach_child(std::shared_ptr<Object> child)
		{
			YGNodeInsertChild(yoga_node, child->yoga_node, static_cast<uint32_t>(children.size()));
		}

		void remove_child(Object* child)
		{
			auto it = std::find_if(children.begin(), children.end(), [child](const std::shared_ptr<Object>& ptr) {
				return ptr.get() == child;
				});

			if (it != children.end()) {
				YGNodeRemoveChild(yoga_node, (*it)->yoga_node);
				children.erase(it);
			}
		}

		template <typename T>
		T* find_child(fnv1a_t target_id)
		{
			for (auto& child : children)
			{
				if (child->id == target_id)
				{
					if constexpr (std::is_same_v<T, Object>)
						return child.get();
					else
						return dynamic_cast<T*>(child.get());
				}

				if (T* found = child->find_child<T>(target_id))
					return found;
			}
			return nullptr;
		}

		float get_x() const { return YGNodeLayoutGetLeft(yoga_node); }
		float get_y() const { return YGNodeLayoutGetTop(yoga_node); }
		float get_width() const { return YGNodeLayoutGetWidth(yoga_node); }
		float get_height() const { return YGNodeLayoutGetHeight(yoga_node); }

		virtual void update_layout()
		{
			display_anim.update(should_display ? 1.0f : 0.0f);

			if (m_base_height < 0.0f) {
				YGValue h = YGNodeStyleGetHeight(yoga_node);
				if (h.unit == YGUnitPoint) m_base_height = h.value;
			}

			if (m_base_margin_bottom < 0.0f) {
				YGValue m = YGNodeStyleGetMargin(yoga_node, YGEdgeBottom);
				if (m.unit == YGUnitPoint) m_base_margin_bottom = m.value;
			}

			if (m_base_height > 0.0f) {
				YGNodeStyleSetHeight(yoga_node, m_base_height * display_anim.value);
			}

			if (m_base_margin_bottom > 0.0f) {
				YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, m_base_margin_bottom * display_anim.value);
			}

			for (auto& child : children) {
				child->update_layout();
			}
		}

		virtual glm::vec2 get_scroll_offset() const { return { 0.0f, 0.0f }; }

		virtual glm::vec2 get_absolute_position()
		{
			if (parent)
			{
				glm::vec2 pos = parent->get_child_origin() + glm::vec2(YGNodeLayoutGetLeft(yoga_node), YGNodeLayoutGetTop(yoga_node));

				if (m_base_height > 0.0f)
				{
					float total_height = m_base_height + (m_base_margin_bottom > 0.0f ? m_base_margin_bottom : 0.0f);
					pos.y -= total_height * (1.0f - display_anim.value);
				}

				return pos;
			}

			return { 0.0f, 0.0f };
		}

		virtual glm::vec2 get_child_origin() { return get_absolute_position(); }

		virtual float get_preferred_width() { return 180.0f; }

		bool is_descendant_of(Object* other)
		{
			if (this == other) return true;
			if (parent) return parent->is_descendant_of(other);
			return false;
		}

		virtual void for_each_logical_child(std::function<void(Object*)> callback)
		{
			for (auto& child : children) {
				callback(child.get());
			}
		}

		virtual void render()
		{
			for (auto& child : children) {
				child->render();
			}
		}

		virtual void render_overlay() {}
		virtual void on_event(Event& e) {}
		virtual void reset_to_default() {}

		virtual void dispatch_event(Event& e)
		{
			if (e.get_type() == EventType::MouseButton) {
				auto& me = static_cast<MouseButtonEvent&>(e);
				if (me.pressed && on_click) {
					on_click(MouseState{ me.position, me.button, me.pressed });
					e.handled = true;
				}
			}
			if (e.get_type() == EventType::MouseEnter) {
				if (on_mouse_enter) on_mouse_enter(MouseState{});
			}
			if (e.get_type() == EventType::MouseLeave) {
				if (on_mouse_leave) on_mouse_leave(MouseState{});
			}

			if (!e.handled) {
				on_event(e);
			}

			for (auto it = children.rbegin(); it != children.rend(); ++it) {
				if (e.handled) break;
				(*it)->dispatch_event(e);
			}
		}

		std::function<void(const MouseState&)> on_click{};
		std::function<void(const MouseState&)> on_mouse_enter{};
		std::function<void(const MouseState&)> on_mouse_leave{};

		std::string m_name{};

		fnv1a_t id{};
		YGNodeRef yoga_node{};

		float m_base_height{ -1.0f };
		float m_base_margin_bottom{ -1.0f };
	};
}
